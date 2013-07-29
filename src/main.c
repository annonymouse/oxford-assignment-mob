/* main loops for setting things up */

#include "node.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uv.h>

struct node_init{
    uv_thread_t thread;
    unsigned id;
    unsigned x;
    unsigned y;
};

static void node_runner(void* ctx){
    struct node_init* node = (struct node_init*)ctx;
    node_run(node->id, node->x, node->y);
    /* should never get here */
    return;
}

static int launch_nodes(struct node_init* node, size_t len){
    /* need to launch threads for each of these guys */
    for (unsigned i = 0; i < len; i++){
        uv_thread_create(&node[i].thread, node_runner, &node[i]);
    }
    return 0;
}

int main(int argc, char * const* argv){
    int use_sleep = 1;
    unsigned nodes = 10;
    unsigned sleep_time = 0;
    struct option longopts[] = {
        {"sleep", no_argument, &use_sleep, 1},
        {"timedwait", no_argument, &use_sleep, 0},
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
    while(1);
    return 0;
}

