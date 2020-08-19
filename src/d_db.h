#ifndef _d_db_h_
#define _d_db_h_

#include "p_map.h"
#include "g_sign.h"

void d_db_enable();
void d_db_disable();
int d_db_is_enabled();
int d_db_init(char *path);
void d_db_close();
void d_db_commit();
void d_db_auth_set(char *username, char *identity_token);
int d_db_auth_select(char *username);
void d_db_auth_select_none();
int d_db_auth_get(
    char *username,
    char *identity_token, int identity_token_length);
int d_db_auth_get_selected(
    char *username, int username_length,
    char *identity_token, int identity_token_length);
void d_db_save_state(float x, float y, float z, float rx, float ry);
int d_db_load_state(float *x, float *y, float *z, float *rx, float *ry);
void d_db_insert_block(int p, int q, int x, int y, int z, int w);
void d_db_insert_light(int p, int q, int x, int y, int z, int w);
void d_db_insert_sign(
    int p, int q, int x, int y, int z, int face, const char *text);
void d_db_delete_sign(int x, int y, int z, int face);
void d_db_delete_signs(int x, int y, int z);
void d_db_delete_all_signs();
void d_db_load_blocks(Map *map, int p, int q);
void d_db_load_lights(Map *map, int p, int q);
void d_db_load_signs(SignList *list, int p, int q);
int d_db_get_key(int p, int q);
void d_db_set_key(int p, int q, int key);
void d_db_worker_start();
void d_db_worker_stop();
int d_db_worker_run(void *arg);

#endif

