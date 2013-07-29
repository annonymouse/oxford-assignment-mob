#include <stdio.h>
#include <uv.h>
/* Code that runs on the node, and is laucnhes it's own 
 * libuv event loop.  This will allow me to simulate random and periodic
 * wakeups 
 */






void tempsensor(uv_timer_t* handle, int status){
    printf("Temperature is 4 from loop %p\n", handle->loop);
}

int node_run(unsigned id, unsigned x, unsigned y){
    uv_loop_t * loop = uv_loop_new();
    printf("Launching node %u at [%u,%u] on loop %p\n", id, x, y, loop);

    /* register temperature sensor callbacks */
    uv_timer_t sensor_sim;
    uv_timer_init(loop, &sensor_sim);
    uv_timer_start(&sensor_sim, &tempsensor, 1000,1000);
    
    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}
