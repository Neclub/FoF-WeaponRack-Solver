#include "logmon.h"

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

int main(void) {
    LmParseConfig cfg;
    LmMatch m[4];
    int n;
    char err[128];

    lm_config_defaults(&cfg);
    expect(lm_config_validate(&cfg, err, sizeof(err)), "defaults valid");

    n = lm_parse_line("R212`3", &cfg, m, 4);
    expect(n == 1, "simple token count");
    expect(m[0].rack == 1, "rack 2 is index 1");
    expect(m[0].slots[0] == 3 && m[0].slots[1] == 1 && m[0].slots[2] == 4 && m[0].slots[3] == 2,
           "codes 1=Ax 2=Sw `=gap 3=Sp");
    expect(strcmp(m[0].token, "R212`3") == 0, "token text");

    n = lm_parse_line("[Tue Oct 20 11:00:00 2026] Bob tells the raid, 'R212`3'", &cfg, m, 4);
    expect(n == 1 && m[0].rack == 1, "embedded in EQ line");

    n = lm_parse_line("R1123` R2```` R3`123 R42112", &cfg, m, 4);
    expect(n == 4, "four racks in one line");
    expect(m[0].rack == 0 && m[1].rack == 1 && m[2].rack == 2 && m[3].rack == 3, "rack order");

    n = lm_parse_line("XR212`3", &cfg, m, 4);
    expect(n == 0, "reject glued prefix");

    n = lm_parse_line("R512`3", &cfg, m, 4);
    expect(n == 0, "reject rack 5");

    n = lm_parse_line("R21a`3", &cfg, m, 4);
    expect(n == 0, "reject bad code");

    /* Custom identifiers: WR1..WR4 and A/S/P/- */
    LmParseConfig custom;
    lm_config_defaults(&custom);
    strncpy(custom.rack_tag[0], "WR1", LM_TAG_CAP - 1);
    strncpy(custom.rack_tag[1], "WR2", LM_TAG_CAP - 1);
    strncpy(custom.rack_tag[2], "WR3", LM_TAG_CAP - 1);
    strncpy(custom.rack_tag[3], "WR4", LM_TAG_CAP - 1);
    custom.axe = 'A';
    custom.sword = 'S';
    custom.spear = 'P';
    custom.blank = '-';
    expect(lm_config_validate(&custom, err, sizeof(err)), "custom config valid");

    n = lm_parse_line("says WR2AS-P done", &custom, m, 4);
    expect(n == 1, "custom token count");
    expect(m[0].rack == 1, "custom rack 2");
    expect(m[0].slots[0] == 3 && m[0].slots[1] == 1 && m[0].slots[2] == 4 && m[0].slots[3] == 2,
           "custom A=Ax S=Sw -=gap P=Sp");
    expect(strcmp(m[0].token, "WR2AS-P") == 0, "custom token text");

    n = lm_parse_line("R212`3", &custom, m, 4);
    expect(n == 0, "defaults do not match custom config");

    char ex[32];
    lm_format_example(&cfg, 1, ex, sizeof(ex));
    expect(strcmp(ex, "R212`3") == 0, "example format default rack2");

    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("All logmon tests passed.\n");
    return 0;
}
