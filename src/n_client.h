#ifndef _n_client_h_
#define _n_client_h_

#define DEFAULT_PORT 4080

void n_client_enable();
void n_client_disable();
int n_client_is_enabled();
void n_client_connect(char *hostname, int port);
void n_client_start();
void n_client_stop();
void n_client_send(char *data);
char *n_client_recv();
void n_client_version(int version);
void n_client_login(const char *username, const char *identity_token);
void n_client_position(float x, float y, float z, float rx, float ry);
void n_client_chunk(int p, int q, int key);
void n_client_block(int x, int y, int z, int w);
void n_client_light(int x, int y, int z, int w);
void n_client_sign(int x, int y, int z, int face, const char *text);
void n_client_talk(const char *text);

#endif
