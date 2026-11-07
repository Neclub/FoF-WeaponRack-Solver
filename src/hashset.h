/* Open-addressing map: board (uint32) -> parent, meta, last move. */
#ifndef WEAPON_RACK_HASHSET_H
#define WEAPON_RACK_HASHSET_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t *keys;
    uint32_t *parents;
    uint16_t *metas; /* src | (dst<<2) | (code<<4) */
    int8_t *last_src;
    int8_t *last_dst;
    uint8_t *state; /* 0 empty, 1 occupied */
    size_t cap;
    size_t size;
} BoardMap;

static inline uint32_t board_hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static inline int board_map_init(BoardMap *m, size_t cap) {
    size_t c = 16;
    while (c < cap) c <<= 1;
    m->cap = c;
    m->size = 0;
    m->keys = (uint32_t *)calloc(c, sizeof(uint32_t));
    m->parents = (uint32_t *)calloc(c, sizeof(uint32_t));
    m->metas = (uint16_t *)calloc(c, sizeof(uint16_t));
    m->last_src = (int8_t *)malloc(c);
    m->last_dst = (int8_t *)malloc(c);
    m->state = (uint8_t *)calloc(c, 1);
    if (!m->keys || !m->parents || !m->metas || !m->last_src || !m->last_dst || !m->state) {
        return 0;
    }
    memset(m->last_src, -1, c);
    memset(m->last_dst, -1, c);
    return 1;
}

static inline void board_map_free(BoardMap *m) {
    free(m->keys);
    free(m->parents);
    free(m->metas);
    free(m->last_src);
    free(m->last_dst);
    free(m->state);
    memset(m, 0, sizeof(*m));
}

static inline void board_map_put_raw(
    BoardMap *m,
    uint32_t key,
    uint32_t parent,
    uint16_t meta,
    int8_t last_src,
    int8_t last_dst
) {
    size_t mask = m->cap - 1;
    size_t i = board_hash(key) & mask;
    for (;;) {
        if (!m->state[i]) {
            m->state[i] = 1;
            m->keys[i] = key;
            m->parents[i] = parent;
            m->metas[i] = meta;
            m->last_src[i] = last_src;
            m->last_dst[i] = last_dst;
            m->size++;
            return;
        }
        if (m->keys[i] == key) {
            m->parents[i] = parent;
            m->metas[i] = meta;
            m->last_src[i] = last_src;
            m->last_dst[i] = last_dst;
            return;
        }
        i = (i + 1) & mask;
    }
}

static inline int board_map_grow(BoardMap *m) {
    BoardMap nmap;
    if (!board_map_init(&nmap, m->cap * 2)) return 0;
    for (size_t i = 0; i < m->cap; i++) {
        if (!m->state[i]) continue;
        board_map_put_raw(&nmap, m->keys[i], m->parents[i], m->metas[i],
                          m->last_src[i], m->last_dst[i]);
    }
    board_map_free(m);
    *m = nmap;
    return 1;
}

static inline int board_map_contains(const BoardMap *m, uint32_t key) {
    size_t mask = m->cap - 1;
    size_t i = board_hash(key) & mask;
    for (;;) {
        if (!m->state[i]) return 0;
        if (m->keys[i] == key) return 1;
        i = (i + 1) & mask;
    }
}

/* Returns 1 if newly inserted, 0 if already present, -1 on OOM. */
static inline int board_map_insert(
    BoardMap *m,
    uint32_t key,
    uint32_t parent,
    uint16_t meta,
    int8_t last_src,
    int8_t last_dst
) {
    if (m->size * 2 >= m->cap) {
        if (!board_map_grow(m)) return -1;
    }
    size_t mask = m->cap - 1;
    size_t i = board_hash(key) & mask;
    for (;;) {
        if (!m->state[i]) {
            m->state[i] = 1;
            m->keys[i] = key;
            m->parents[i] = parent;
            m->metas[i] = meta;
            m->last_src[i] = last_src;
            m->last_dst[i] = last_dst;
            m->size++;
            return 1;
        }
        if (m->keys[i] == key) return 0;
        i = (i + 1) & mask;
    }
}

static inline int board_map_get(
    const BoardMap *m,
    uint32_t key,
    uint32_t *parent,
    uint16_t *meta,
    int8_t *last_src,
    int8_t *last_dst
) {
    size_t mask = m->cap - 1;
    size_t i = board_hash(key) & mask;
    for (;;) {
        if (!m->state[i]) return 0;
        if (m->keys[i] == key) {
            if (parent) *parent = m->parents[i];
            if (meta) *meta = m->metas[i];
            if (last_src) *last_src = m->last_src[i];
            if (last_dst) *last_dst = m->last_dst[i];
            return 1;
        }
        i = (i + 1) & mask;
    }
}

#endif
