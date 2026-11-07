#ifndef WEAPON_RACK_SOLVER_H
#define WEAPON_RACK_SOLVER_H

#include <stdint.h>
#include <stddef.h>

#define WR_RACKS 4
#define WR_SLOTS 4
#define WR_CHUNK_LIMIT 200
#define WR_MAX_MOVES 256
#define WR_MAX_CHUNKS 32
#define WR_PASTE_CAP 4096
#define WR_ERR_CAP 256

/* Slot codes: 0 empty, 1 Sw, 2 Sp, 3 Ax */
typedef int8_t WrSlot; /* -1 unused in arrays; packed uses 0-3 */

typedef struct {
    int src;   /* 1-based */
    int dst;   /* 1-based */
    int code;  /* 1=Sw 2=Sp 3=Ax */
} WrMove;

typedef struct {
    int ok; /* 1 success, 0 failure */
    char error[WR_ERR_CAP];
    int move_count;
    double solve_ms;
    WrMove moves[WR_MAX_MOVES];
    char paste[WR_PASTE_CAP];
    char chunks[WR_MAX_CHUNKS][WR_CHUNK_LIMIT + 64];
    int chunk_count;
    /* final[rack][slot]: 0 empty, 1 Sw, 2 Sp, 3 Ax */
    uint8_t final_board[WR_RACKS][WR_SLOTS];
    /* labels: 0 Empty, 1 Sword, 2 Spear, 3 Axe, 4 Mixed */
    int final_labels[WR_RACKS];
} WrSolveResult;

/* slots[rack][slot]: 0 empty, 1 Sw, 2 Sp, 3 Ax (top→bottom). */
int wr_validate_counts(const uint8_t slots[WR_RACKS][WR_SLOTS], char *err, size_t err_cap);
uint32_t wr_pack(const uint8_t slots[WR_RACKS][WR_SLOTS]);
void wr_unpack(uint32_t board, uint8_t slots[WR_RACKS][WR_SLOTS]);
int wr_is_goal(uint32_t board);

/* Fill result; returns 1 on success, 0 on failure (result->error set). */
int wr_solve(const uint8_t slots[WR_RACKS][WR_SLOTS], WrSolveResult *result);

const char *wr_code_abbrev(int code);
const char *wr_label_name(int label);

#endif
