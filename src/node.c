#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

/* Code that runs on the node, and is laucnhes it's own 
 * libuv event loop.  This will allow me to simulate random and periodic
 * wakeups 
 */

#define MAX_NEIGH (8)
#define MAX_PIPE_NAME (128)
uv_pipe_t neighbours[MAX_NEIGH];
uv_connect_t neighbour_connections[MAX_NEIGH];
char neighbour_name[MAX_NEIGH][MAX_PIPE_NAME];


static void recv_data(uv_connect_t* req, int status){
    unsigned ni = *(unsigned*) req->data;
    if (-1 == status){
        /* No node there, or disconnection */
        fprintf(stderr, "No contact with node %u %s - %s\n", ni,
                neighbour_name[ni] ,
                uv_err_name(uv_last_error(req->handle->loop)));
    }
    printf("Received data %u, node %s\n", status, neighbour_name[ni]);
}

static void data_sender(uv_timer_t* handle, int status){
    printf("Sending data\n");
}

static void tempsensor(uv_timer_t* handle, int status){
    /* temperature callback fakes a temperature */
    printf("Temperature is 4 from node %u\n", *(unsigned *)handle->data);
}

static void parent_discovery(uv_work_t* handle){
    printf("Starting parent discovery\n");
}

static void parent_discovery_done(uv_work_t* handle, int status){
    /* TODO register the send data here */
}

static void neighbour_discovery(uv_work_t* handle){
    /* find neighbours */
    printf("Starting neighbour discovery\n");

    for (unsigned i = 0; i < MAX_NEIGH; i++){
        uv_pipe_init(handle->loop, &neighbours[i], 0);
        unsigned* data = malloc(sizeof(unsigned)); 
        *data = i;
        neighbour_connections[i].data = data; 
        uv_pipe_connect(&neighbour_connections[i], &neighbours[i], 
                    neighbour_name[i], recv_data);
    }
}

static void neighbour_discovery_done(uv_work_t* handle, int status){
    /* have the list of neihbours and relative routing costs */
    /* we should start parent discovery */

    uv_queue_work(handle->loop, handle, parent_discovery, parent_discovery_done);
}

static void neighbour_namer(int x, int y){
    unsigned s = 0;
    for (int i = x - 1; i < (x + 2); i++){
        for (int j = y - 1; (j < (y + 2)) /*&& (s < MAX_NEIGH)*/; j++){
            printf("loop %d, %d \n", i, j);
            if (j == y && i == x){
                continue;
            }
            snprintf(neighbour_name[s++], 128, "%d.%d.node", i, j);
            printf("%s\n",neighbour_name[s - 1]);
        }
    }
}

int node_run(unsigned id, signed x, signed y){
    uv_loop_t * loop = uv_loop_new();
    printf("Launching node %u at [%u,%u]\n", id, x, y);

    /* register temperature sensor callbacks */
    uv_timer_t sensor_sim;
    sensor_sim.data = &id;
    uv_timer_init(loop, &sensor_sim);
    uv_timer_start(&sensor_sim, &tempsensor, 1000,1000);
   
    /* should discover neighbours - and later parent */
    uv_work_t nd;
    uv_queue_work(loop, &nd, neighbour_discovery, neighbour_discovery_done);
    
    /* periodically send data to parent */

    uv_timer_t send_data;
    send_data.data = &id;
    uv_timer_init(loop, &send_data);
    uv_timer_start(&send_data, data_sender, 2000, 2000);

    /* setup receive loop for your connection.  Currently simulation data by
     * having pipes for the 8 surrounding nodes */
    uv_pipe_t my_pipe;
    uv_pipe_init(loop, &my_pipe, 0);
    char pipe_name[128];
    snprintf(pipe_name, 128, ".%d.%d.node", x, y);
    if (uv_pipe_bind(&my_pipe, pipe_name)){
        fprintf(stderr, "Failed to bind %s, %s\n", pipe_name, 
                uv_err_name(uv_last_error(loop)));
        return -1;
    }

    /* preseed neighbour with names of neighbours */
    neighbour_namer(x, y);

    uv_run(loop, UV_RUN_DEFAULT);
    /* shouldn't get here */
    return 0;
}

int main(int argc, char * const* argv){
    struct option longopts[] = {
        {"id", required_argument, NULL, 'i'},
        {"range", required_argument, NULL, 'r'},
        {NULL, 0, NULL, 0}
    };
    signed x = 0;
    signed y = 0;
    unsigned range = 1;
    unsigned id = 0;
    char ch;
    while (-1 != (ch = getopt_long(argc, argv, "i:x:y:r:", longopts, NULL))){
        switch(ch){
            case 'i':
                id = strtoul(optarg, NULL, 0);
                break;
            case 'x':
                x = strtol(optarg, NULL, 0);
                break;
            case 'y':
                y = strtol(optarg, NULL, 0);
                break;
            case 'r':
                range = strtoul(optarg, NULL, 0);
                break;


            default: break;
        }
    }

    argc -= optind;
    argv += optind;

    return node_run(id, x, y); 
}

