#include <stdlib.h>
#include <string.h>
#include "d_ring.h"

void d_ring_alloc(Ring *ring, int capacity) {
    ring->capacity = capacity;
    ring->start = 0;
    ring->end = 0;
    ring->data = (RingEntry *)calloc(capacity, sizeof(RingEntry));
}

void d_ring_free(Ring *ring) {
    free(ring->data);
}

int d_ring_empty(Ring *ring) {
    return ring->start == ring->end;
}

int d_ring_full(Ring *ring) {
    return ring->start == (ring->end + 1) % ring->capacity;
}

int d_ring_size(Ring *ring) {
    if (ring->end >= ring->start) {
        return ring->end - ring->start;
    }
    else {
        return ring->capacity - (ring->start - ring->end);
    }
}

void d_ring_grow(Ring *ring) {
    Ring new_ring;
    RingEntry entry;
    d_ring_alloc(&new_ring, ring->capacity * 2);
    while (d_ring_get(ring, &entry)) {
        d_ring_put(&new_ring, &entry);
    }
    free(ring->data);
    ring->capacity = new_ring.capacity;
    ring->start = new_ring.start;
    ring->end = new_ring.end;
    ring->data = new_ring.data;
}

void d_ring_put(Ring *ring, RingEntry *entry) {
    if (d_ring_full(ring)) {
        d_ring_grow(ring);
    }
    RingEntry *e = ring->data + ring->end;
    memcpy(e, entry, sizeof(RingEntry));
    ring->end = (ring->end + 1) % ring->capacity;
}

void d_ring_put_block(Ring *ring, int p, int q, int x, int y, int z, int w) {
    RingEntry entry;
    entry.type = BLOCK;
    entry.p = p;
    entry.q = q;
    entry.x = x;
    entry.y = y;
    entry.z = z;
    entry.w = w;
    d_ring_put(ring, &entry);
}

void d_ring_put_light(Ring *ring, int p, int q, int x, int y, int z, int w) {
    RingEntry entry;
    entry.type = LIGHT;
    entry.p = p;
    entry.q = q;
    entry.x = x;
    entry.y = y;
    entry.z = z;
    entry.w = w;
    d_ring_put(ring, &entry);
}

void d_ring_put_key(Ring *ring, int p, int q, int key) {
    RingEntry entry;
    entry.type = KEY;
    entry.p = p;
    entry.q = q;
    entry.key = key;
    d_ring_put(ring, &entry);
}

void d_ring_put_commit(Ring *ring) {
    RingEntry entry;
    entry.type = COMMIT;
    d_ring_put(ring, &entry);
}

void d_ring_put_exit(Ring *ring) {
    RingEntry entry;
    entry.type = EXIT;
    d_ring_put(ring, &entry);
}

int d_ring_get(Ring *ring, RingEntry *entry) {
    if (d_ring_empty(ring)) {
        return 0;
    }
    RingEntry *e = ring->data + ring->start;
    memcpy(entry, e, sizeof(RingEntry));
    ring->start = (ring->start + 1) % ring->capacity;
    return 1;
}
