#define _CRT_SECURE_NO_WARNINGS
#include "solver.h"
#include "hashset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RACK_MASK 0xFFu
#define SLOT_MASK 0x3u
#define FULL_SW 0x55u
#define FULL_SP 0xAAu
#define FULL_AX 0xFFu

static const char *ABBREV[] = {"", "Sw", "Sp", "Ax"};
static const char *LABELS[] = {"Empty", "Sword", "Spear", "Axe", "Mixed"};

const char *wr_code_abbrev(int code) {
    if (code < 1 || code > 3) return "?";
    return ABBREV[code];
}

const char *wr_label_name(int label) {
    if (label < 0 || label > 4) return "?";
    return LABELS[label];
}

uint32_t wr_pack(const uint8_t slots[WR_RACKS][WR_SLOTS]) {
    uint32_t board = 0;
    for (int r = 0; r < WR_RACKS; r++) {
        uint32_t rack = 0;
        for (int s = 0; s < WR_SLOTS; s++) {
            rack |= ((uint32_t)(slots[r][s] & 3)) << (s * 2);
        }
        board |= rack << (r * 8);
    }
    return board;
}

void wr_unpack(uint32_t board, uint8_t slots[WR_RACKS][WR_SLOTS]) {
    for (int r = 0; r < WR_RACKS; r++) {
        uint32_t rack = (board >> (r * 8)) & RACK_MASK;
        for (int s = 0; s < WR_SLOTS; s++) {
            slots[r][s] = (uint8_t)((rack >> (s * 2)) & SLOT_MASK);
        }
    }
}

int wr_validate_counts(const uint8_t slots[WR_RACKS][WR_SLOTS], char *err, size_t err_cap) {
    int totals[4] = {0, 0, 0, 0};
    for (int r = 0; r < WR_RACKS; r++) {
        int n = 0;
        for (int s = 0; s < WR_SLOTS; s++) {
            uint8_t c = slots[r][s];
            if (c > 3) {
                snprintf(err, err_cap, "Invalid weapon code");
                return 0;
            }
            if (c) {
                totals[c]++;
                n++;
            }
        }
        if (n > WR_SLOTS) {
            snprintf(err, err_cap, "Rack %d has %d weapons; max is 4", r + 1, n);
            return 0;
        }
    }
    err[0] = 0;
    int ok = 1;
    size_t used = 0;
    const char *names[] = {"", "Swords", "Spears", "Axes"};
    for (int c = 1; c <= 3; c++) {
        if (totals[c] != 4) {
            ok = 0;
            int n = snprintf(err + used, err_cap > used ? err_cap - used : 0,
                             "%s%s %d/4", used ? "; " : "", names[c], totals[c]);
            if (n > 0) used += (size_t)n;
        }
    }
    return ok;
}

int wr_is_goal(uint32_t board) {
    int seen_empty = 0;
    int seen = 0;
    for (int r = 0; r < WR_RACKS; r++) {
        uint32_t rack = (board >> (r * 8)) & RACK_MASK;
        if (rack == 0) {
            seen_empty++;
        } else if (rack == FULL_SW) {
            seen |= 1;
        } else if (rack == FULL_SP) {
            seen |= 2;
        } else if (rack == FULL_AX) {
            seen |= 4;
        } else {
            return 0;
        }
    }
    return seen_empty == 1 && seen == 7;
}

static uint16_t meta_pack(int src, int dst, int code) {
    return (uint16_t)(src | (dst << 2) | (code << 4));
}

static void fill_labels(const uint8_t slots[WR_RACKS][WR_SLOTS], int labels[WR_RACKS]) {
    for (int r = 0; r < WR_RACKS; r++) {
        int n = 0;
        int first = 0;
        int mono = 1;
        for (int s = 0; s < WR_SLOTS; s++) {
            if (slots[r][s]) {
                if (!n) first = slots[r][s];
                else if (slots[r][s] != first) mono = 0;
                n++;
            }
        }
        if (n == 0) labels[r] = 0;
        else if (n == 4 && mono) labels[r] = first; /* 1/2/3 Sword/Spear/Axe */
        else labels[r] = 4;
    }
}

static void format_paste(const WrMove *moves, int count, char *out, size_t cap) {
    out[0] = 0;
    size_t used = 0;
    for (int i = 0; i < count; i++) {
        char piece[32];
        snprintf(piece, sizeof(piece), "R%d%s>R%d", moves[i].src, wr_code_abbrev(moves[i].code),
                 moves[i].dst);
        int n = snprintf(out + used, cap > used ? cap - used : 0, "%s%s", i ? ", " : "", piece);
        if (n > 0) used += (size_t)n;
    }
}

static void chunk_paste(const char *paste, WrSolveResult *result) {
    result->chunk_count = 0;
    if (!paste[0]) return;
    size_t plen = strlen(paste);
    if (plen <= WR_CHUNK_LIMIT) {
        strncpy(result->chunks[0], paste, sizeof(result->chunks[0]) - 1);
        result->chunks[0][sizeof(result->chunks[0]) - 1] = 0;
        result->chunk_count = 1;
        return;
    }
    /* Split on ", " */
    char buf[WR_PASTE_CAP];
    strncpy(buf, paste, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char *parts[512];
    int nparts = 0;
    char *p = buf;
    while (p && nparts < 512) {
        parts[nparts++] = p;
        char *comma = strstr(p, ", ");
        if (!comma) break;
        *comma = 0;
        p = comma + 2;
    }
    char current[WR_CHUNK_LIMIT + 64];
    current[0] = 0;
    for (int i = 0; i < nparts && result->chunk_count < WR_MAX_CHUNKS; i++) {
        char candidate[WR_CHUNK_LIMIT + 64];
        if (!current[0]) {
            strncpy(candidate, parts[i], sizeof(candidate) - 1);
        } else {
            snprintf(candidate, sizeof(candidate), "%s, %s", current, parts[i]);
        }
        candidate[sizeof(candidate) - 1] = 0;
        if (strlen(candidate) <= WR_CHUNK_LIMIT) {
            strncpy(current, candidate, sizeof(current) - 1);
            current[sizeof(current) - 1] = 0;
        } else {
            if (current[0]) {
                strncpy(result->chunks[result->chunk_count], current,
                        sizeof(result->chunks[0]) - 1);
                result->chunks[result->chunk_count][sizeof(result->chunks[0]) - 1] = 0;
                result->chunk_count++;
            }
            strncpy(current, parts[i], sizeof(current) - 1);
            current[sizeof(current) - 1] = 0;
        }
    }
    if (current[0] && result->chunk_count < WR_MAX_CHUNKS) {
        strncpy(result->chunks[result->chunk_count], current, sizeof(result->chunks[0]) - 1);
        result->chunks[result->chunk_count][sizeof(result->chunks[0]) - 1] = 0;
        result->chunk_count++;
    }
}

static void init_goals(uint32_t goals[24]) {
    const uint32_t monos[3] = {FULL_SW, FULL_SP, FULL_AX};
    int gi = 0;
    for (int empty = 0; empty < WR_RACKS; empty++) {
        /* 6 permutations of 3 monos */
        int perm[6][3] = {
            {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
        };
        for (int p = 0; p < 6; p++) {
            uint32_t racks[4] = {0, 0, 0, 0};
            int mi = 0;
            for (int r = 0; r < WR_RACKS; r++) {
                if (r == empty) racks[r] = 0;
                else racks[r] = monos[perm[p][mi++]];
            }
            goals[gi++] = racks[0] | (racks[1] << 8) | (racks[2] << 16) | (racks[3] << 24);
        }
    }
}

typedef struct {
    uint32_t *data;
    size_t head;
    size_t len;
    size_t cap;
} RingQueue;

static int rq_init(RingQueue *q, size_t cap) {
    q->data = (uint32_t *)malloc(cap * sizeof(uint32_t));
    q->head = 0;
    q->len = 0;
    q->cap = cap;
    return q->data != NULL;
}

static void rq_free(RingQueue *q) {
    free(q->data);
    memset(q, 0, sizeof(*q));
}

static int rq_push(RingQueue *q, uint32_t v) {
    if (q->len >= q->cap) {
        size_t ncap = q->cap * 2;
        uint32_t *nd = (uint32_t *)malloc(ncap * sizeof(uint32_t));
        if (!nd) return 0;
        for (size_t i = 0; i < q->len; i++) {
            nd[i] = q->data[(q->head + i) % q->cap];
        }
        free(q->data);
        q->data = nd;
        q->head = 0;
        q->cap = ncap;
    }
    q->data[(q->head + q->len) % q->cap] = v;
    q->len++;
    return 1;
}

static uint32_t rq_pop(RingQueue *q) {
    uint32_t v = q->data[q->head];
    q->head = (q->head + 1) % q->cap;
    q->len--;
    return v;
}

static uint32_t expand_layer(
    RingQueue *queue,
    BoardMap *seen,
    const BoardMap *other_seen,
    int *found
) {
    *found = 0;
    size_t layer = queue->len;
    for (size_t n = 0; n < layer; n++) {
        uint32_t current = rq_pop(queue);
        int8_t fs = -1, fd = -1;
        board_map_get(seen, current, NULL, NULL, &fs, &fd);

        uint32_t racks[4];
        racks[0] = current & RACK_MASK;
        racks[1] = (current >> 8) & RACK_MASK;
        racks[2] = (current >> 16) & RACK_MASK;
        racks[3] = (current >> 24) & RACK_MASK;

        int src_slots[4] = {-1, -1, -1, -1};
        int src_codes[4] = {0, 0, 0, 0};
        int place_slots[4] = {-1, -1, -1, -1};
        for (int r = 0; r < 4; r++) {
            int b = -1;
            for (int s = 3; s >= 0; s--) {
                if ((racks[r] >> (s * 2)) & SLOT_MASK) {
                    b = s;
                    break;
                }
            }
            if (b >= 0) {
                src_slots[r] = b;
                src_codes[r] = (int)((racks[r] >> (b * 2)) & SLOT_MASK);
                place_slots[r] = b < 3 ? b + 1 : -1;
            } else {
                place_slots[r] = 0;
            }
        }

        for (int src = 0; src < 4; src++) {
            int src_slot = src_slots[src];
            if (src_slot < 0) continue;
            int weapon_code = src_codes[src];
            uint32_t src_rack = racks[src];
            for (int dst = 0; dst < 4; dst++) {
                if (dst == src) continue;
                int dst_slot = place_slots[dst];
                if (dst_slot < 0) continue;
                if (src == (int)fd && dst == (int)fs) continue;
                /* Must match destination bottom (weapon above), or dest empty. */
                if (src_slots[dst] >= 0 && src_codes[dst] != weapon_code) continue;

                uint32_t dst_rack = racks[dst];
                uint32_t new_src = src_rack & ~(SLOT_MASK << (src_slot * 2));
                uint32_t new_dst = dst_rack | ((uint32_t)weapon_code << (dst_slot * 2));
                int src_shift = src << 3;
                int dst_shift = dst << 3;
                uint32_t nxt = current;
                nxt = (nxt & ~(RACK_MASK << src_shift)) | (new_src << src_shift);
                nxt = (nxt & ~(RACK_MASK << dst_shift)) | (new_dst << dst_shift);

                int ins = board_map_insert(seen, nxt, current, meta_pack(src, dst, weapon_code),
                                           (int8_t)src, (int8_t)dst);
                if (ins == 0) continue;
                if (ins < 0) return 0;
                if (!rq_push(queue, nxt)) return 0;
                if (board_map_contains(other_seen, nxt)) {
                    *found = 1;
                    return nxt;
                }
            }
        }
    }
    return 0;
}

int wr_solve(const uint8_t slots[WR_RACKS][WR_SLOTS], WrSolveResult *result) {
    memset(result, 0, sizeof(*result));
    if (!wr_validate_counts(slots, result->error, sizeof(result->error))) {
        result->ok = 0;
        return 0;
    }

    clock_t t0 = clock();
    uint32_t start = wr_pack(slots);

    if (wr_is_goal(start)) {
        result->ok = 1;
        result->move_count = 0;
        result->solve_ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        memcpy(result->final_board, slots, sizeof(result->final_board));
        fill_labels(slots, result->final_labels);
        return 1;
    }

    uint32_t goals[24];
    init_goals(goals);

    BoardMap fwd_seen, back_seen;
    if (!board_map_init(&fwd_seen, 1 << 16) || !board_map_init(&back_seen, 1 << 16)) {
        snprintf(result->error, sizeof(result->error), "Out of memory");
        board_map_free(&fwd_seen);
        board_map_free(&back_seen);
        return 0;
    }

    RingQueue fwd_q, back_q;
    if (!rq_init(&fwd_q, 4096) || !rq_init(&back_q, 4096)) {
        snprintf(result->error, sizeof(result->error), "Out of memory");
        board_map_free(&fwd_seen);
        board_map_free(&back_seen);
        rq_free(&fwd_q);
        rq_free(&back_q);
        return 0;
    }

    board_map_insert(&fwd_seen, start, start, 0, -1, -1);
    rq_push(&fwd_q, start);
    for (int i = 0; i < 24; i++) {
        board_map_insert(&back_seen, goals[i], goals[i], 0, -1, -1);
        rq_push(&back_q, goals[i]);
    }

    uint32_t meet = 0;
    int found = 0;
    while (fwd_q.len && back_q.len && !found) {
        if (fwd_q.len <= back_q.len) {
            meet = expand_layer(&fwd_q, &fwd_seen, &back_seen, &found);
        } else {
            meet = expand_layer(&back_q, &back_seen, &fwd_seen, &found);
        }
    }

    if (!found) {
        snprintf(result->error, sizeof(result->error), "No solution found");
        board_map_free(&fwd_seen);
        board_map_free(&back_seen);
        rq_free(&fwd_q);
        rq_free(&back_q);
        return 0;
    }

    /* Forward path start → meet (start root has parent==start). */
    WrMove fwd_path[WR_MAX_MOVES];
    int fwd_n = 0;
    uint32_t node = meet;
    for (;;) {
        uint32_t parent;
        uint16_t meta;
        if (!board_map_get(&fwd_seen, node, &parent, &meta, NULL, NULL)) break;
        if (node == start) break;
        int s = meta & 3;
        int d = (meta >> 2) & 3;
        int code = (meta >> 4) & 3;
        if (fwd_n < WR_MAX_MOVES) {
            fwd_path[fwd_n].src = s + 1;
            fwd_path[fwd_n].dst = d + 1;
            fwd_path[fwd_n].code = code;
            fwd_n++;
        }
        if (parent == node) break;
        node = parent;
    }
    for (int i = 0; i < fwd_n / 2; i++) {
        WrMove tmp = fwd_path[i];
        fwd_path[i] = fwd_path[fwd_n - 1 - i];
        fwd_path[fwd_n - 1 - i] = tmp;
    }

    /* Backward: invert edges meet → goal */
    WrMove back_path[WR_MAX_MOVES];
    int back_n = 0;
    uint32_t trail[WR_MAX_MOVES];
    int trail_n = 0;
    node = meet;
    for (;;) {
        uint32_t parent;
        uint16_t meta;
        if (!board_map_get(&back_seen, node, &parent, &meta, NULL, NULL)) break;
        if (parent == node) break; /* goal root */
        if (trail_n < WR_MAX_MOVES) trail[trail_n++] = node;
        node = parent;
    }
    uint32_t goal_board = node;
    for (int i = 0; i < trail_n; i++) {
        uint32_t child = trail[i];
        uint32_t parent;
        uint16_t meta;
        board_map_get(&back_seen, child, &parent, &meta, NULL, NULL);
        int s = meta & 3;
        int d = (meta >> 2) & 3;
        int code = (meta >> 4) & 3;
        if (back_n < WR_MAX_MOVES) {
            back_path[back_n].src = d + 1;
            back_path[back_n].dst = s + 1;
            back_path[back_n].code = code;
            back_n++;
        }
    }

    result->move_count = fwd_n + back_n;
    for (int i = 0; i < fwd_n; i++) result->moves[i] = fwd_path[i];
    for (int i = 0; i < back_n; i++) result->moves[fwd_n + i] = back_path[i];

    format_paste(result->moves, result->move_count, result->paste, sizeof(result->paste));
    chunk_paste(result->paste, result);

    wr_unpack(goal_board, result->final_board);
    fill_labels(result->final_board, result->final_labels);

    result->solve_ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
    result->ok = 1;

    board_map_free(&fwd_seen);
    board_map_free(&back_seen);
    rq_free(&fwd_q);
    rq_free(&back_q);
    return 1;
}
