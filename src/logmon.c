#include "logmon.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

void lm_config_defaults(LmParseConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->rack_tag[0], "R1", LM_TAG_CAP - 1);
    strncpy(cfg->rack_tag[1], "R2", LM_TAG_CAP - 1);
    strncpy(cfg->rack_tag[2], "R3", LM_TAG_CAP - 1);
    strncpy(cfg->rack_tag[3], "R4", LM_TAG_CAP - 1);
    cfg->axe = '1';
    cfg->sword = '2';
    cfg->spear = '3';
    cfg->blank = '`';
}

int lm_config_validate(const LmParseConfig *cfg, char *err, size_t err_cap) {
    if (!cfg) {
        if (err && err_cap) snprintf(err, err_cap, "Missing parse config");
        return 0;
    }
    for (int i = 0; i < LM_RACKS; i++) {
        if (!cfg->rack_tag[i][0]) {
            if (err && err_cap) snprintf(err, err_cap, "Rack %d tag is empty", i + 1);
            return 0;
        }
        for (int j = i + 1; j < LM_RACKS; j++) {
            if (strcmp(cfg->rack_tag[i], cfg->rack_tag[j]) == 0) {
                if (err && err_cap)
                    snprintf(err, err_cap, "Rack tags must be unique (%s)", cfg->rack_tag[i]);
                return 0;
            }
        }
    }
    char w[4] = {cfg->axe, cfg->sword, cfg->spear, cfg->blank};
    for (int i = 0; i < 4; i++) {
        if (w[i] == 0) {
            if (err && err_cap) snprintf(err, err_cap, "Weapon characters cannot be empty");
            return 0;
        }
        for (int j = i + 1; j < 4; j++) {
            if (w[i] == w[j]) {
                if (err && err_cap)
                    snprintf(err, err_cap, "Weapon characters must be distinct");
                return 0;
            }
        }
    }
    if (err && err_cap) err[0] = 0;
    return 1;
}

int lm_code_to_cell(char c, const LmParseConfig *cfg) {
    if (!cfg) return -1;
    if (c == cfg->axe) return 3;   /* Ax */
    if (c == cfg->sword) return 1; /* Sw */
    if (c == cfg->spear) return 2; /* Sp */
    if (c == cfg->blank) return 4; /* gap */
    return -1;
}

static int ok_before(const char *line, const char *p) {
    if (p == line) return 1;
    unsigned char prev = (unsigned char)p[-1];
    return !(isalnum(prev) || prev == '_' || prev == '`');
}

static int ok_after(char after) {
    return (after == 0 || after == ' ' || after == '\t' || after == '\r' || after == '\n' ||
            after == '\'' || after == '"' || after == ',' || after == '.' || after == ')' ||
            after == ']' || after == '!');
}

static int try_match_at(const char *p, const LmParseConfig *cfg, LmMatch *out) {
    int best_rack = -1;
    size_t best_len = 0;
    for (int r = 0; r < LM_RACKS; r++) {
        size_t tlen = strlen(cfg->rack_tag[r]);
        if (tlen == 0 || tlen < best_len) continue;
        if (strncmp(p, cfg->rack_tag[r], tlen) != 0) continue;
        /* Prefer longest tag when several match. */
        if (tlen > best_len) {
            best_len = tlen;
            best_rack = r;
        }
    }
    if (best_rack < 0) return 0;

    const char *codes = p + best_len;
    for (int i = 0; i < LM_SLOTS; i++) {
        if (lm_code_to_cell(codes[i], cfg) < 0) return 0;
    }
    if (!ok_after(codes[LM_SLOTS])) return 0;

    out->rack = best_rack;
    for (int i = 0; i < LM_SLOTS; i++)
        out->slots[i] = (uint8_t)lm_code_to_cell(codes[i], cfg);
    size_t total = best_len + LM_SLOTS;
    if (total >= LM_TOKEN_CAP) total = LM_TOKEN_CAP - 1;
    memcpy(out->token, p, total);
    out->token[total] = 0;
    return (int)(best_len + LM_SLOTS);
}

int lm_parse_line(const char *line, const LmParseConfig *cfg, LmMatch *out, int max_out) {
    if (!line || !cfg || !out || max_out <= 0) return 0;
    int n = 0;
    const char *p = line;
    while (*p && n < max_out) {
        if (ok_before(line, p)) {
            LmMatch m;
            int advance = try_match_at(p, cfg, &m);
            if (advance > 0) {
                out[n++] = m;
                p += advance;
                continue;
            }
        }
        p++;
    }
    return n;
}

void lm_format_example(const LmParseConfig *cfg, int rack0, char *out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = 0;
    if (!cfg || rack0 < 0 || rack0 >= LM_RACKS) return;
    snprintf(out, cap, "%s%c%c%c%c", cfg->rack_tag[rack0], cfg->axe, cfg->sword, cfg->blank,
             cfg->spear);
}
