#ifndef _g_sign_h_
#define _g_sign_h_

#define MAX_SIGN_LENGTH 64

typedef struct {
    int x;
    int y;
    int z;
    int face;
    char text[MAX_SIGN_LENGTH];
} Sign;

typedef struct {
    unsigned int capacity;
    unsigned int size;
    Sign *data;
} SignList;

void g_sign_list_alloc(SignList *list, int capacity);
void g_sign_list_free(SignList *list);
void g_sign_list_grow(SignList *list);
void g_sign_list_add(
    SignList *list, int x, int y, int z, int face, const char *text);
int g_sign_list_remove(SignList *list, int x, int y, int z, int face);
int g_sign_list_remove_all(SignList *list, int x, int y, int z);

#endif

