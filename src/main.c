/* main loops for setting things up */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uv.h>

struct node_init{
    //uv_thread_t thread;
    uv_process_t proc;
    uv_process_options_t opts;
    uv_stdio_container_t stdio[3];
    unsigned id;
    unsigned x;
    unsigned y;
};

static void proc_died(uv_process_t* handle, int exit_status,int term_signal){
    printf("Node died %d %d\n", exit_status, term_signal);
    uv_close((uv_handle_t*)handle, NULL);
}

#if 0
static void node_runner(void* ctx){
    struct node_init* node = (struct node_init*)ctx;
    node_run(node->id, node->x, node->y);
    /* should never get here */
    return;
}
#endif

static int launch_nodes(struct node_init* node, size_t len){
    /* need to launch threads for each of these guys */
    for (unsigned i = 0; i < len; i++){
        node[i].id = i;
        node[i].opts.exit_cb = proc_died;
        node[i].opts.file = "./node";
        node[i].opts.stdio_count = 3;
        node[i].opts.stdio = node[i].stdio;
        node[i].stdio[0].flags = UV_IGNORE;
        node[i].stdio[1].flags = UV_INHERIT_FD;
        node[i].stdio[1].data.fd = 1;
        node[i].stdio[2].flags = UV_INHERIT_FD;
        node[i].stdio[2].data.fd = 2;
        
        //uv_thread_create(&node[i].thread, node_runner, &node[i]);
        //
        uv_spawn(uv_default_loop(), &node[i].proc, node[i].opts);
    }
    return 0;
}

int main(int argc, char * const* argv){
    unsigned nodes = 10;
    unsigned sleep_time = 0;
    uv_loop_t* loop = uv_default_loop();
    struct option longopts[] = {
        {"nodes", required_argument, NULL, 'n'},
        {"time", required_argument, NULL, 't'},
        {NULL, 0, NULL, 0}
    };
    char ch;
    while (-1 != (ch = getopt_long(argc, argv, "n:", longopts, NULL))){
        switch(ch){
            case 'n':
                nodes = strtoul(optarg, NULL, 0);
                break;
            case 't':
                sleep_time = strtoul(optarg, NULL, 0);
                break;
            default: break;
        }
    }

    argc -= optind;
    argv += optind;

    struct node_init* node_ctx = malloc(nodes * sizeof(struct node_init));
    if (NULL == node_ctx){
        return -1;
    }

    launch_nodes(node_ctx, nodes);

    /* sleep forever */
    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}

