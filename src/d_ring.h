#ifndef _d_ring_h_
#define _d_ring_h_

typedef enum {
    BLOCK,
    LIGHT,
    KEY,
    COMMIT,
    EXIT
} RingEntryType;

typedef struct {
    RingEntryType type;
    int p;
    int q;
    int x;
    int y;
    int z;
    int w;
    int key;
} RingEntry;

typedef struct {
    unsigned int capacity;
    unsigned int start;
    unsigned int end;
    RingEntry *data;
} Ring;

void d_ring_alloc(Ring *ring, int capacity);
void d_ring_free(Ring *ring);
int d_ring_empty(Ring *ring);
int d_ring_full(Ring *ring);
int d_ring_size(Ring *ring);
void d_ring_grow(Ring *ring);
void d_ring_put(Ring *ring, RingEntry *entry);
void d_ring_put_block(Ring *ring, int p, int q, int x, int y, int z, int w);
void d_ring_put_light(Ring *ring, int p, int q, int x, int y, int z, int w);
void d_ring_put_key(Ring *ring, int p, int q, int key);
void d_ring_put_commit(Ring *ring);
void d_ring_put_exit(Ring *ring);
int d_ring_get(Ring *ring, RingEntry *entry);

#endif
