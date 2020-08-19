#ifndef _p_map_h_
#define _p_map_h_

#define EMPTY_ENTRY(entry) ((entry)->value == 0)

#define MAP_FOR_EACH(map, ex, ey, ez, ew) \
    for (unsigned int i = 0; i <= map->mask; i++) { \
        MapEntry *entry = map->data + i; \
        if (EMPTY_ENTRY(entry)) { \
            continue; \
        } \
        int ex = entry->e.x + map->dx; \
        int ey = entry->e.y + map->dy; \
        int ez = entry->e.z + map->dz; \
        int ew = entry->e.w;

#define END_MAP_FOR_EACH }

typedef union {
    unsigned int value;
    struct {
        unsigned char x;
        unsigned char y;
        unsigned char z;
        char w;
    } e;
} MapEntry;

typedef struct {
    int dx;
    int dy;
    int dz;
    unsigned int mask;
    unsigned int size;
    MapEntry *data;
} Map;

void p_map_alloc(Map *map, int dx, int dy, int dz, int mask);
void p_map_free(Map *map);
void p_map_copy(Map *dst, Map *src);
void p_map_grow(Map *map);
int p_map_set(Map *map, int x, int y, int z, int w);
int p_map_get(Map *map, int x, int y, int z);

#endif

