#ifndef WRC_LOGMON_H
#define WRC_LOGMON_H

#include <stdint.h>
#include <stddef.h>

#define LM_RACKS 4
#define LM_SLOTS 4
#define LM_TAG_CAP 16
#define LM_TOKEN_CAP 32

/* Out slots: 0 empty, 1 Sw, 2 Sp, 3 Ax, 4 gap (same as app board). */
typedef struct {
    int rack; /* 0-based */
    uint8_t slots[LM_SLOTS];
    char token[LM_TOKEN_CAP];
} LmMatch;

typedef struct {
    char rack_tag[LM_RACKS][LM_TAG_CAP];
    char axe;
    char sword;
    char spear;
    char blank;
} LmParseConfig;

void lm_config_defaults(LmParseConfig *cfg);
int lm_config_validate(const LmParseConfig *cfg, char *err, size_t err_cap);

/* Map chat code to board cell using config. Returns -1 if invalid. */
int lm_code_to_cell(char c, const LmParseConfig *cfg);

/* Scan line for configured rack tags + four weapon codes. */
int lm_parse_line(const char *line, const LmParseConfig *cfg, LmMatch *out, int max_out);

/* Build example token for rack index using config (e.g. R212`3). */
void lm_format_example(const LmParseConfig *cfg, int rack0, char *out, size_t cap);

#endif
