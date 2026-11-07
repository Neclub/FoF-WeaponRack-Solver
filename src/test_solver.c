#include "solver.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect(int cond, const char *msg) {
    if (!cond) {
        printf("FAIL: %s\n", msg);
        failures++;
    } else {
        printf("ok: %s\n", msg);
    }
}

static void set_rack(uint8_t slots[WR_RACKS][WR_SLOTS], int r, const int *codes, int n) {
    for (int s = 0; s < WR_SLOTS; s++) slots[r][s] = 0;
    for (int s = 0; s < n && s < WR_SLOTS; s++) slots[r][s] = (uint8_t)codes[s];
}

int main(void) {
    uint8_t slots[WR_RACKS][WR_SLOTS];
    memset(slots, 0, sizeof(slots));
    WrSolveResult result;

    /* Already solved */
    {
        int r0[] = {1, 1, 1, 1};
        int r1[] = {2, 2, 2, 2};
        int r2[] = {3, 3, 3, 3};
        set_rack(slots, 0, r0, 4);
        set_rack(slots, 1, r1, 4);
        set_rack(slots, 2, r2, 4);
        set_rack(slots, 3, r0, 0);
        expect(wr_solve(slots, &result) && result.move_count == 0, "already solved");
    }

    /* One move */
    {
        int r0[] = {1, 1, 1, 1};
        int r1[] = {2, 2, 2, 2};
        int r2[] = {3, 3, 3};
        int r3[] = {3};
        set_rack(slots, 0, r0, 4);
        set_rack(slots, 1, r1, 4);
        set_rack(slots, 2, r2, 3);
        set_rack(slots, 3, r3, 1);
        expect(wr_solve(slots, &result) && result.move_count == 1, "one move count");
        expect(strcmp(result.paste, "R4Ax>R3") == 0, "one move paste");
    }

    /* Write-up sample is unsolvable once placement must match. */
    {
        int r0[] = {1, 2, 3};
        int r1[] = {2, 3, 3, 1};
        int r2[] = {1, 2, 3};
        int r3[] = {1, 2};
        set_rack(slots, 0, r0, 3);
        set_rack(slots, 1, r1, 4);
        set_rack(slots, 2, r2, 3);
        set_rack(slots, 3, r3, 2);
        expect(!wr_solve(slots, &result), "writeup sample unsolvable");
    }

    /* Solvable match-rule layout */
    {
        int r0[] = {1, 1, 2, 2}; /* Sw Sw Sp Sp */
        int r1[] = {2, 2, 1, 1}; /* Sp Sp Sw Sw */
        int r2[] = {3, 3, 3, 3};
        set_rack(slots, 0, r0, 4);
        set_rack(slots, 1, r1, 4);
        set_rack(slots, 2, r2, 4);
        set_rack(slots, 3, r0, 0);
        expect(wr_solve(slots, &result), "match layout solves");
        expect(result.move_count > 0, "match layout has moves");
        expect(wr_is_goal(wr_pack(result.final_board)), "match layout final is goal");
        printf("  match moves=%d solve_ms=%.1f paste=%s\n", result.move_count, result.solve_ms,
               result.paste);
    }

    /* Bad counts */
    {
        int r0[] = {1, 1};
        int r1[] = {2};
        int r2[] = {3};
        set_rack(slots, 0, r0, 2);
        set_rack(slots, 1, r1, 1);
        set_rack(slots, 2, r2, 1);
        set_rack(slots, 3, r0, 0);
        expect(!wr_solve(slots, &result), "bad counts rejected");
    }

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
