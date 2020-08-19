#ifndef _p_world_h_
#define _p_world_h_

typedef void (*world_func)(int, int, int, int, void *);

void p_world_create(int p, int q, world_func func, void *arg);

#endif

