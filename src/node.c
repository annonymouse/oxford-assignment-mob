#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#include <sys/queue.h>
struct write_req_t {
    uv_write_t req;
    uv_buf_t buf;
};

/* Code that runs on the node, and is laucnhes it's own 
 * libuv event loop.  This will allow me to simulate random and periodic
 * wakeups 
 */


#define MAX_NAME (128)

#define MSG_RA (0)
#define MSG_NA (1)
#define MSG_DA (2)
#define MSG_RQ (3)
#define MAX_NEIGH (8)

struct data {
    char src[MAX_NAME];
    int type;
    union {
        struct ra {
            int hops;
            int seq;
        } ra;
        struct da {
            int sum;
            size_t samples;
        } da;
    }u;
}__attribute__((packed));


SLIST_HEAD(nb_list, neighbour) head = SLIST_HEAD_INITIALIZER(head);
struct neighbour {
    /* Implementation details */
    SLIST_ENTRY(neighbour) entry;
    const char* link_name;
    bool created;
    uv_pipe_t pipe;
    uv_pipe_t acc_con;
    uv_connect_t connection;

    /* neighbour data */
    const char* neigh_name;
    
    /* buffering */
    struct data buf;
    size_t len;
};

struct sample_data {
    int sum;
    size_t samples;
} data ;

struct gw_route {
    int hops;
    const char* next_hop;
    int seq;
} route;

const char* name = "node";

int neighbour_ucast_msg(const char*, const struct data*);
int neighbour_bcast_msg(const struct data*);

uv_buf_t alloc_buffer(uv_handle_t*handle, size_t suggested_size){
    return uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

void send_data_msg(int sum, size_t len){
    struct data buf;
    strlcpy(buf.src, name, MAX_NAME);
    buf.type = MSG_DA;
    buf.u.da.sum = sum;
    buf.u.da.samples = len;
    neighbour_ucast_msg(route.next_hop, &buf);
    return;
}

void send_na_msg(){
    /* these are always headed to a partner*/ 
    struct data buf;
    strlcpy(buf.src, name, MAX_NAME);
    buf.type = MSG_NA;
    neighbour_bcast_msg(&buf);
    return;
}

void send_ra_msg(){
    struct data buf;
    strlcpy(buf.src, name, MAX_NAME);
    buf.type = MSG_RA;
    buf.u.ra.hops = route.hops == -1 ? route.hops:route.hops+1;
    buf.u.ra.seq = route.seq;
    neighbour_bcast_msg(&buf);
}

void deal_with_msg(struct neighbour* np, const struct data* buf){
    switch(buf->type){
        case MSG_RA: {
            /* check our routing table*/
            int new_hops = buf->u.ra.hops;
            int new_seq = buf->u.ra.seq;
            if ((new_hops < route.hops && new_seq >= route.seq) // shorter path
                    || (new_hops == -1 && new_seq == route.seq)){ // path says far
                route.hops = new_hops;
                route.seq = new_seq;
                /* broadcast new information */
                send_ra_msg();
            }
            break;
                     }

        case MSG_NA: 
            if (!np->neigh_name){
                char* local_name = (char*)malloc(sizeof(char) * MAX_NAME);
                strlcpy(local_name, buf->src, MAX_NAME);
                np->neigh_name = local_name;
                /* only expecting one connetion */
            }
        case MSG_DA:
            /* Forward this to gw ag, or print it if you are the gw */
            if (route.hops == 0){
                /* leaving off decimal points */
                printf("Got AVG sample data %lu\n", buf->u.da.sum / buf->u.da.samples);
            }
            else {
                /* normal node */
                /* combine with own, it'll be transmitted on interval*/
                data.sum += buf->u.da.sum; 
                data.samples += buf->u.da.samples;
            }

        case MSG_RQ:
        default: return;
    }

}

void recv_data(uv_stream_t *stream, ssize_t nread, uv_buf_t buf) {
    struct neighbour* np = (struct neighbour*)stream->data;
    if (nread == -1) {
        /* stuff */
    }
    else {
        if (nread > 0 && buf.len != 0) {
            char* d = buf.base;
            char* e = (char*)&np->buf;
            while (buf.len > 0){
                /* very slow */
                *d = *e++;
                d++;
                buf.len--;
                if(np->len == sizeof(data)){
                    deal_with_msg(np, &np->buf);
                    np->len = 0;
                    e = (char*)&np->buf;
                }
            }
        }
    }
    if (buf.base) free(buf.base);
}

static void on_connect(uv_connect_t* req, int status){
    struct neighbour* np = (struct neighbour*)req->data;
    if (-1 == status){
        /* No node there, or disconnection */
        fprintf(stderr, "No contact with link %s - %s\n", np->link_name,
                uv_err_name(uv_last_error(req->handle->loop)));
        /* kill this connect req */
    }
    printf("Connected to link %s - %u\n", np->link_name, status);
    /* accept this connection and let work happen with it */
    uv_read_start((uv_stream_t*)&np->pipe, alloc_buffer, recv_data);
    /* send a NA */
    send_na_msg();
}

static void on_listen_connect(uv_stream_t* req, int status){
    printf("Accepting connection\n");
    struct neighbour* np = (struct neighbour*)req->data;
    uv_pipe_init(req->loop, &np->acc_con, 0);
    np->acc_con.data = np;
    if (uv_accept(req, (uv_stream_t*) &np->acc_con) == 0){
        uv_read_start((uv_stream_t*)&np->acc_con, alloc_buffer, recv_data);
    }
    uv_close((uv_handle_t*)req, NULL);
}

static void data_sender(uv_timer_t* handle, int status){
    printf("Sending data\n");
    send_data_msg(data.sum, data.samples);
    data.sum = 0;
    data.samples = 0;
}

static void tempsensor(uv_timer_t* handle, int status){
    /* temperature callback fakes a temperature */
    int r = rand() % 100; /* temperatures between 0 and 99 */
    data.sum+=r;
    data.samples++;
    printf("Temperature is %d count:%lu sum:%d", r, data.samples, data.sum); 
}

int neighbour_bcast_msg(const struct data* data){
    struct neighbour* np;
    SLIST_FOREACH(np, &head, entry){
        /* this should be refactored */
        neighbour_ucast_msg(np->neigh_name, data); 
    }
    return 0;
}

void clean_up(uv_write_t *req, int status) {
    struct write_req_t *wr = (struct write_req_t*) req;
    free(wr->buf.base);
    free(wr);
}

int neighbour_ucast_msg(const char* name, const struct data* data){
    struct neighbour* np;
    SLIST_FOREACH(np, &head, entry){
        if (np->neigh_name && 0 == strncmp(name, np->neigh_name, MAX_NAME)){
            /* send message */
            /* Do we have an accepted connection, or are we connected later */
            uv_pipe_t * pipe;
            if (np->created){
                pipe = &np->acc_con;
            }
            else {
                pipe = &np->pipe;
            }
            /* send the data */
            struct write_req_t *req = 
                (struct write_req_t*) malloc(sizeof(struct write_req_t));
            req->buf = uv_buf_init((char*) malloc(sizeof(struct data)), sizeof(struct data));
            memcpy(req->buf.base, data, sizeof(struct data));
            uv_write((uv_write_t*) req, (uv_stream_t*)pipe, &req->buf, 1, clean_up);
            return 0;
        }
    }
    return -1;
}

static void gateway_bcast(uv_timer_t* req, int status){
    printf("Starting gateway bcast\n");
    /* send data to each neighbour */
    send_ra_msg();
}

int node_run(const char* id, bool gw){
    uv_loop_t * loop = uv_loop_new();
    printf("Launching node %s\n", id);

    /* register temperature sensor callbacks */
    srand(time(0));
    uv_timer_t sensor_sim;
    uv_timer_init(loop, &sensor_sim);
    uv_timer_start(&sensor_sim, &tempsensor, 0,100);
    /* periodically send data to parent */
    uv_timer_t send_data;
    send_data.data = &id;
    uv_timer_init(loop, &send_data);
    uv_timer_start(&send_data, data_sender, 2000, 2000);

    /* setup receive loop for your connection.  Currently simulation data by
     * having pipes for the 8 surrounding nodes */
    struct neighbour* np;
    SLIST_FOREACH(np, &head, entry){
        uv_pipe_init(loop, &np->pipe, 0);
        np->pipe.data = np;
        np->created = true;
        if (uv_pipe_bind(&np->pipe, np->link_name)){
            uv_err_t err = uv_last_error(loop);
            if (UV_EADDRINUSE == err.code){
                /* it already exists, so another node created it, we'll just
                 * connect to it and listen */
                np->connection.data = np;
                uv_pipe_connect(&np->connection, &np->pipe, np->link_name, 
                        on_connect);
                np->created = false;
            }
            else {
                fprintf(stderr, "Failed to bind %s, %s\n", np->link_name, 
                        uv_err_name(uv_last_error(loop)));
                return -1;
            }
        }
        else {
            /* start listening */
            if (uv_listen((uv_stream_t*)& np->pipe, 1, on_listen_connect)){
                fprintf(stderr, "Listen error\n");
                return -1;
            }
        }
    }

    /* if I'm a gw I have the closest route */
    if (gw){
        route.hops = 0;
        route.next_hop = id;
        route.seq = 0;
        /* broadcast route periodically */
        uv_timer_t bcast_route;
        bcast_route.data = &route;
        uv_timer_init(loop, &bcast_route);
        uv_timer_start(&bcast_route, gateway_bcast, 0, 2000);
    } 
    else {
        route.hops = -1;
        route.seq = -1;
        route.next_hop =  NULL;
    }

    uv_run(loop, UV_RUN_DEFAULT);
    /* shouldn't get here */
    return 0;
}

int main(int argc, char * const* argv){
    struct option longopts[] = {
        {"id", required_argument, NULL, 'i'},
        {"gateway", no_argument, NULL, 'g'},
        {NULL, 0, NULL, 0}
    };
    char ch;
    bool gateway = false;
    while (-1 != (ch = getopt_long(argc, argv, "gi:", longopts, NULL))){
        switch(ch){
            case 'i':
                name = optarg;
                break;
            case 'g':
                gateway = true;
                break;

            default: break;
        }
    }

    argc -= optind;
    argv += optind;

    /* physical link connections to make */
    while (argc > 0){
        struct neighbour* n = (struct neighbour*)malloc(
                sizeof(struct neighbour)); 
        n->link_name = argv[0];
        SLIST_INSERT_HEAD(&head, n, entry);
        argc--;
        argv++;
    }

    return node_run(name, gateway); 
}

