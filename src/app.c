#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <direct.h>

#include "solver.h"
#include "logmon.h"
#include "wrc_res.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

#define APP_TITLE "Weapon Rack Solver"
#define SETTINGS_NAME "wrc_settings.ini"
#define ID_TIMER_TOPMOST 1
#define ID_TIMER_LOGMON 3
#define ID_TIMER_DEMO 4
#define ID_TIMER_STATUS 5
#define TOPMOST_MS 750
#define LOGMON_MS 250
#define DEMO_STEP_MS 700
#define LOGMON_READ_CAP (64 * 1024)
#define LOGMON_LINE_CAP 4096
#define LOGMON_SCAN_RECENT (64 * 1024)
#define WM_APP_SOLVED (WM_APP + 1)
#define HIT_CAP 96

#define COL_BG RGB(18, 20, 26)
#define COL_PANEL RGB(28, 32, 48)
#define COL_PANEL2 RGB(37, 43, 61)
#define COL_BORDER RGB(58, 66, 88)
#define COL_TEXT RGB(232, 236, 245)
#define COL_MUTED RGB(154, 163, 184)
#define COL_SW RGB(79, 140, 255)
#define COL_SP RGB(62, 207, 142)
#define COL_AX RGB(255, 122, 69)
#define COL_FOCUS RGB(245, 215, 110)
#define COL_BAD RGB(255, 92, 108)
#define COL_OK RGB(62, 207, 142)

#define IDC_ERR 100
#define IDC_PATH 110
#define IDC_BROWSE 111
#define IDC_LOGEN 112
#define IDC_SCAN 113
#define IDC_STATUS 114
#define IDC_RACK0 120
#define IDC_AXE 130
#define IDC_SWORD 131
#define IDC_SPEAR 132
#define IDC_BLANK 133
#define IDC_EXAMPLE 134
#define IDC_HK0 140
#define IDC_RESET_PARSE 160
#define IDC_RESET_HK 161
#define IDC_SAVE 162
#define IDC_LBL_LOG 170
#define IDC_LBL_PARSE 171
#define IDC_LBL_HK 172
#define IDC_TIP_PARSE 174
#define IDC_TIP_HK 175
#define IDC_LBL_RACK0 180
#define IDC_LBL_CHAR0 184
#define IDC_LBL_HK0 190

enum {
    HIT_NONE = 0,
    HIT_TOP = 1,
    HIT_AUTO = 2,
    HIT_AUTODEMO = 3,
    HIT_AUTOCODES = 16,
    HIT_SCAN = 4,
    HIT_SETTINGS = 5,
    HIT_AXE = 6,
    HIT_SWORD = 7,
    HIT_SPEAR = 8,
    HIT_GAP = 9,
    HIT_UNDO = 10,
    HIT_CLEAR = 11,
    HIT_SOLVE = 12,
    HIT_COPY = 13,
    HIT_DEMO = 14,
    HIT_CODES = 15,
    HIT_SLOT0 = 100,
    HIT_DEMO_PLAY = 200,
    HIT_DEMO_STEP = 201,
    HIT_DEMO_RESTART = 202,
    HIT_DEMO_CLOSE = 203
};

typedef enum {
    HK_SWORD = 0,
    HK_SPEAR,
    HK_AXE,
    HK_GAP,
    HK_UNDO,
    HK_SOLVE,
    HK_RACK1,
    HK_RACK2,
    HK_RACK3,
    HK_RACK4,
    HK_COUNT
} HotkeyId;

typedef struct {
    char root[MAX_PATH];
    char logs[MAX_PATH];
    char log_file[MAX_PATH];
    char log_name[64];
    char settings[MAX_PATH];
    CRITICAL_SECTION log_lock;
} Paths;

typedef struct {
    int x, y, w, h;
    int placed;
    int maximized;
} ToolRect;

typedef struct {
    int has;
    uint8_t slots[4];
    char line[LOGMON_LINE_CAP];
} RackOrigin;

typedef struct {
    RECT rc;
    int id;
} Hit;

typedef struct SolveJob {
    HWND hwnd;
    int gen;
    int ok;
    uint8_t packed[4][4];
    uint8_t shown[4][4];
    uint8_t snap[4][4];
    WrSolveResult result;
} SolveJob;

static HINSTANCE g_inst;
static HWND g_hwnd;
static HWND g_demo_hwnd;
static HWND g_codes_hwnd;
static HWND g_settings_hwnd;
static Paths g_paths;
static CRITICAL_SECTION g_lock;
static int g_lock_ready;

static int g_win_x = CW_USEDEFAULT;
static int g_win_y = CW_USEDEFAULT;
static int g_win_w = 1000;
static int g_win_h = 860;
static int g_topmost = 1;
static ToolRect g_demo_rect = {CW_USEDEFAULT, CW_USEDEFAULT, 1000, 760, 0, 0};
static ToolRect g_codes_rect = {CW_USEDEFAULT, CW_USEDEFAULT, 720, 180, 0, 0};
static ToolRect g_settings_rect = {CW_USEDEFAULT, CW_USEDEFAULT, 760, 520, 0, 0};
static int g_settings_fit;
static int g_settings_need_w;
static int g_settings_need_h;
static int g_codes_scroll;
static int g_codes_content_h;
static int g_auto_solve = 0;
static int g_auto_demo = 0;
static int g_auto_codes = 0;
static UINT g_hotkeys[HK_COUNT];
static UINT g_hk_draft[HK_COUNT];
static int g_hk_capture = -1;
static HWND g_settings_hover;
static HWND g_settings_tip;
static WNDPROC g_button_proc;
static WNDPROC g_edit_proc;

static char g_eq_log_path[MAX_PATH];
static int g_eq_log_enabled;
static HANDLE g_eq_log_handle = INVALID_HANDLE_VALUE;
static LONGLONG g_eq_log_offset;
static char g_eq_line_buf[LOGMON_LINE_CAP];
static int g_eq_line_len;
static char g_eq_last_match[32];
static char g_eq_status[128];
static LmParseConfig g_parse_cfg;
static int g_seq;
static int g_seen_seq;
static int g_rack_seq[4];
static int g_rack_has[4];
static uint8_t g_rack_slots[4][4];
static RackOrigin g_origin[4];
static char g_view_status[128];
static int g_view_enabled;

static uint8_t g_board[4][4];
static int g_sel_r;
static int g_sel_s;
static int g_board_gen;
static int g_solving;
static char g_error[256];
static int g_has_result;
static WrSolveResult g_result;
static uint8_t g_snap[4][4];
static WrMove g_demo_moves[WR_MAX_MOVES];
static int g_demo_count;
static uint8_t g_demo_board[4][4];
static int g_demo_index;
static int g_demo_playing;
static int g_demo_hl;
static int g_demo_from_r, g_demo_from_s, g_demo_to_r, g_demo_to_s;

static HFONT g_font;
static HFONT g_font_bold;
static HFONT g_font_title;
static HFONT g_font_small;
static int g_font_dpi;
static int g_ui_dpi = 96;
static HBRUSH g_br_bg;
static HBRUSH g_br_panel;
static HBRUSH g_br_edit;
static HICON g_icon_big;
static HICON g_icon_small;

static Hit g_hits[HIT_CAP];
static int g_nhits;
static int g_hover;
static HWND g_hover_hwnd;
static HWND g_hit_hwnd;
static int g_draw;
static int g_scroll;
static int g_content_h;
static int g_result_y;
static int g_scroll_result;

static const char *HK_LABELS[HK_COUNT] = {
    "Sword", "Spear", "Axe", "Gap", "Undo", "Solve", "Rack 1", "Rack 2", "Rack 3", "Rack 4"
};

static int S(int px) { return MulDiv(px, g_ui_dpi > 0 ? g_ui_dpi : 96, 96); }

static void save_settings(HWND hwnd);
static void eq_log_start_monitor(void);
static void eq_log_stop_monitor(void);
static void pull_matches(void);
static void apply_topmost(void);
static void start_solve(int automatic);
static void open_settings(void);
static void open_demo(void);
static void open_codes(void);
static int copy_solution(int automatic);
static void set_error(const char *msg);
static void layout_main(HWND hwnd, HDC hdc);
static void layout_demo(HWND hwnd, HDC hdc);
static void layout_settings(HWND hwnd);
static void apply_settings_fonts(void);
static void refresh_hk_buttons(void);
static void update_example(HWND hwnd);

static void copy_str(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    snprintf(dst, cap, "%s", src ? src : "");
}

static void append_text(char *dst, size_t cap, const char *src) {
    size_t n;
    if (!dst || cap == 0 || !src) return;
    n = strlen(dst);
    if (n >= cap) return;
    snprintf(dst + n, cap - n, "%s", src);
}

static void trim_inplace(char *s) {
    char *start;
    size_t n;
    if (!s) return;
    start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

static void log_write(const char *event, const char *detail) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    EnterCriticalSection(&g_paths.log_lock);
    FILE *f = fopen(g_paths.log_file, "a");
    if (f) {
        fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] %s", st.wYear, st.wMonth, st.wDay, st.wHour,
                st.wMinute, st.wSecond, event);
        if (detail && detail[0]) fprintf(f, ": %s", detail);
        fprintf(f, "\n");
        fclose(f);
    }
    LeaveCriticalSection(&g_paths.log_lock);
}

static const char *layout_cell(uint8_t cell) {
    if (cell == 1) return "Sw";
    if (cell == 2) return "Sp";
    if (cell == 3) return "Ax";
    if (cell == 4) return "gap";
    return "-";
}

static void append_rack_layout(char *dst, size_t cap, int rack, const uint8_t slots[4], int first) {
    char piece[48];
    snprintf(piece, sizeof(piece), "%sR%d %s %s %s %s", first ? "" : " | ", rack + 1, layout_cell(slots[0]),
             layout_cell(slots[1]), layout_cell(slots[2]), layout_cell(slots[3]));
    append_text(dst, cap, piece);
}

static int origin_matches(const RackOrigin *origin, const uint8_t solved[4]) {
    if (!origin || !origin->has) return 0;
    for (int i = 0; i < 4; i++) {
        uint8_t stored = origin->slots[i] == 4 ? 0 : origin->slots[i];
        uint8_t got = solved[i] == 4 ? 0 : solved[i];
        if (stored != got) return 0;
    }
    return 1;
}

static int init_paths(void) {
    char exe[MAX_PATH];
    char *slash;
    char *slash2;
    SYSTEMTIME st;
    if (!GetModuleFileNameA(NULL, exe, MAX_PATH)) return 0;
    slash = strrchr(exe, '\\');
    if (!slash) return 0;
    *slash = 0;
    slash2 = strrchr(exe, '\\');
    if (slash2 && _stricmp(slash2 + 1, "build") == 0) *slash2 = 0;
    copy_str(g_paths.root, sizeof(g_paths.root), exe);
    snprintf(g_paths.logs, sizeof(g_paths.logs), "%s\\logs", g_paths.root);
    _mkdir(g_paths.logs);
    snprintf(g_paths.settings, sizeof(g_paths.settings), "%s\\%s", g_paths.root, SETTINGS_NAME);
    GetLocalTime(&st);
    snprintf(g_paths.log_name, sizeof(g_paths.log_name), "wrc_%04d-%02d-%02d_%02d-%02d-%02d.log", st.wYear,
             st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    snprintf(g_paths.log_file, sizeof(g_paths.log_file), "%s\\%s", g_paths.logs, g_paths.log_name);
    InitializeCriticalSection(&g_paths.log_lock);
    return 1;
}

static const char *hk_ini_key(HotkeyId id) {
    static const char *keys[HK_COUNT] = {"sword", "spear", "axe", "gap", "undo", "solve",
                                         "rack1", "rack2", "rack3", "rack4"};
    return keys[id];
}

static void hotkeys_set_defaults(UINT *hk) {
    hk[HK_SWORD] = 'S';
    hk[HK_SPEAR] = 'D';
    hk[HK_AXE] = 'A';
    hk[HK_GAP] = VK_SPACE;
    hk[HK_UNDO] = VK_BACK;
    hk[HK_SOLVE] = VK_RETURN;
    hk[HK_RACK1] = '1';
    hk[HK_RACK2] = '2';
    hk[HK_RACK3] = '3';
    hk[HK_RACK4] = '4';
}

static void vk_to_name(UINT vk, char *out, size_t cap) {
    if (!out || cap == 0) return;
    out[0] = 0;
    if (vk >= 'A' && vk <= 'Z') {
        snprintf(out, cap, "%c", (char)vk);
        return;
    }
    if (vk >= '0' && vk <= '9') {
        snprintf(out, cap, "%c", (char)vk);
        return;
    }
    switch (vk) {
    case VK_SPACE: snprintf(out, cap, "Space"); break;
    case VK_BACK: snprintf(out, cap, "Bksp"); break;
    case VK_RETURN: snprintf(out, cap, "Enter"); break;
    case VK_TAB: snprintf(out, cap, "Tab"); break;
    case VK_ESCAPE: snprintf(out, cap, "Esc"); break;
    case VK_DELETE: snprintf(out, cap, "Del"); break;
    case VK_INSERT: snprintf(out, cap, "Ins"); break;
    case VK_HOME: snprintf(out, cap, "Home"); break;
    case VK_END: snprintf(out, cap, "End"); break;
    case VK_PRIOR: snprintf(out, cap, "PgUp"); break;
    case VK_NEXT: snprintf(out, cap, "PgDn"); break;
    case VK_OEM_3: snprintf(out, cap, "`"); break;
    case VK_OEM_MINUS: snprintf(out, cap, "-"); break;
    case VK_OEM_PLUS: snprintf(out, cap, "="); break;
    default:
        if (vk >= VK_F1 && vk <= VK_F12)
            snprintf(out, cap, "F%d", (int)(vk - VK_F1 + 1));
        else if (vk)
            snprintf(out, cap, "VK%d", (int)vk);
        else
            snprintf(out, cap, "?");
        break;
    }
}

static UINT name_to_vk(const char *name) {
    char c;
    if (!name || !name[0]) return 0;
    if (_stricmp(name, "Space") == 0) return VK_SPACE;
    if (_stricmp(name, "Bksp") == 0 || _stricmp(name, "Backspace") == 0) return VK_BACK;
    if (_stricmp(name, "Enter") == 0 || _stricmp(name, "Return") == 0) return VK_RETURN;
    if (_stricmp(name, "Tab") == 0) return VK_TAB;
    if (_stricmp(name, "Esc") == 0 || _stricmp(name, "Escape") == 0) return VK_ESCAPE;
    if (_stricmp(name, "Del") == 0 || _stricmp(name, "Delete") == 0) return VK_DELETE;
    if (_stricmp(name, "Ins") == 0 || _stricmp(name, "Insert") == 0) return VK_INSERT;
    if (_stricmp(name, "Home") == 0) return VK_HOME;
    if (_stricmp(name, "End") == 0) return VK_END;
    if (_stricmp(name, "PgUp") == 0) return VK_PRIOR;
    if (_stricmp(name, "PgDn") == 0) return VK_NEXT;
    if (name[0] == '`' && name[1] == 0) return VK_OEM_3;
    if (name[0] == '-' && name[1] == 0) return VK_OEM_MINUS;
    if (name[0] == '=' && name[1] == 0) return VK_OEM_PLUS;
    if (name[0] == 'F' || name[0] == 'f') {
        int n = atoi(name + 1);
        if (n >= 1 && n <= 12) return (UINT)(VK_F1 + n - 1);
    }
    if (_strnicmp(name, "VK", 2) == 0) return (UINT)atoi(name + 2);
    if (name[1] == 0) {
        c = name[0];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return (UINT)c;
    }
    return 0;
}

static void load_tool_rect(const char *section);

static void load_settings(void) {
    char buf[MAX_PATH];
    GetPrivateProfileStringA("window", "x", "", buf, sizeof(buf), g_paths.settings);
    if (buf[0]) g_win_x = atoi(buf);
    GetPrivateProfileStringA("window", "y", "", buf, sizeof(buf), g_paths.settings);
    if (buf[0]) g_win_y = atoi(buf);
    GetPrivateProfileStringA("window", "w", "", buf, sizeof(buf), g_paths.settings);
    if (buf[0]) g_win_w = atoi(buf);
    GetPrivateProfileStringA("window", "h", "", buf, sizeof(buf), g_paths.settings);
    if (buf[0]) g_win_h = atoi(buf);
    GetPrivateProfileStringA("window", "topmost", "1", buf, sizeof(buf), g_paths.settings);
    g_topmost = atoi(buf) ? 1 : 0;
    GetPrivateProfileStringA("window", "auto_solve", "0", buf, sizeof(buf), g_paths.settings);
    g_auto_solve = atoi(buf) ? 1 : 0;
    GetPrivateProfileStringA("window", "auto_demo", "0", buf, sizeof(buf), g_paths.settings);
    g_auto_demo = atoi(buf) ? 1 : 0;
    GetPrivateProfileStringA("window", "auto_codes", "0", buf, sizeof(buf), g_paths.settings);
    g_auto_codes = atoi(buf) ? 1 : 0;
    if (g_win_w < 640) g_win_w = 640;
    if (g_win_h < 520) g_win_h = 520;

    GetPrivateProfileStringA("log", "path", "", g_eq_log_path, sizeof(g_eq_log_path), g_paths.settings);
    GetPrivateProfileStringA("log", "enabled", "0", buf, sizeof(buf), g_paths.settings);
    g_eq_log_enabled = atoi(buf) ? 1 : 0;

    lm_config_defaults(&g_parse_cfg);
    GetPrivateProfileStringA("parse", "rack1", "R1", g_parse_cfg.rack_tag[0], LM_TAG_CAP, g_paths.settings);
    GetPrivateProfileStringA("parse", "rack2", "R2", g_parse_cfg.rack_tag[1], LM_TAG_CAP, g_paths.settings);
    GetPrivateProfileStringA("parse", "rack3", "R3", g_parse_cfg.rack_tag[2], LM_TAG_CAP, g_paths.settings);
    GetPrivateProfileStringA("parse", "rack4", "R4", g_parse_cfg.rack_tag[3], LM_TAG_CAP, g_paths.settings);
    GetPrivateProfileStringA("parse", "axe", "1", buf, sizeof(buf), g_paths.settings);
    if (buf[0]) g_parse_cfg.axe = buf[0];
    GetPrivateProfileStringA("parse", "sword", "2", buf, sizeof(buf), g_paths.settings);
    if (buf[0]) g_parse_cfg.sword = buf[0];
    GetPrivateProfileStringA("parse", "spear", "3", buf, sizeof(buf), g_paths.settings);
    if (buf[0]) g_parse_cfg.spear = buf[0];
    GetPrivateProfileStringA("parse", "blank", "`", buf, sizeof(buf), g_paths.settings);
    if (buf[0]) g_parse_cfg.blank = buf[0];
    if (!lm_config_validate(&g_parse_cfg, NULL, 0)) lm_config_defaults(&g_parse_cfg);

    hotkeys_set_defaults(g_hotkeys);
    for (int i = 0; i < HK_COUNT; i++) {
        GetPrivateProfileStringA("hotkeys", hk_ini_key((HotkeyId)i), "", buf, sizeof(buf), g_paths.settings);
        if (buf[0]) {
            UINT vk = name_to_vk(buf);
            if (vk) g_hotkeys[i] = vk;
        }
    }
    copy_str(g_eq_status, sizeof(g_eq_status), "Idle");
    load_tool_rect("demo");
    load_tool_rect("codes");
    load_tool_rect("settings");
}

static ToolRect *tool_rect(const char *section) {
    if (section && strcmp(section, "settings") == 0) return &g_settings_rect;
    if (section && strcmp(section, "codes") == 0) return &g_codes_rect;
    return &g_demo_rect;
}

static void placement_floor(const char *section, int *w, int *h) {
    int minw = 480;
    int minh = 360;
    if (section && strcmp(section, "codes") == 0) {
        minw = 160;
        minh = 100;
    }
    if (*w < minw) *w = minw;
    if (*h < minh) *h = minh;
}

static void load_tool_rect(const char *section) {
    ToolRect *place = tool_rect(section);
    char xs[32], ys[32], ws[32], hs[32], maxed[32];
    GetPrivateProfileStringA(section, "x", "", xs, sizeof(xs), g_paths.settings);
    GetPrivateProfileStringA(section, "y", "", ys, sizeof(ys), g_paths.settings);
    GetPrivateProfileStringA(section, "w", "", ws, sizeof(ws), g_paths.settings);
    GetPrivateProfileStringA(section, "h", "", hs, sizeof(hs), g_paths.settings);
    if (!xs[0] || !ys[0] || !ws[0] || !hs[0]) return;
    place->x = atoi(xs);
    place->y = atoi(ys);
    place->w = atoi(ws);
    place->h = atoi(hs);
    placement_floor(section, &place->w, &place->h);
    GetPrivateProfileStringA(section, "max", "0", maxed, sizeof(maxed), g_paths.settings);
    place->maximized = atoi(maxed) ? 1 : 0;
    place->placed = 1;
}

static void store_tool_placement(const char *section, HWND hwnd) {
    WINDOWPLACEMENT wp;
    char buf[64];
    ToolRect *place;
    RECT r;
    int x, y, w, h, maxed;
    wp.length = sizeof(wp);
    if (!section || !section[0] || !hwnd || !GetWindowPlacement(hwnd, &wp)) return;
    place = tool_rect(section);
    r = wp.rcNormalPosition;
    x = (int)r.left;
    y = (int)r.top;
    w = (int)(r.right - r.left);
    h = (int)(r.bottom - r.top);
    placement_floor(section, &w, &h);
    maxed = (wp.showCmd == SW_SHOWMAXIMIZED || (wp.flags & WPF_RESTORETOMAXIMIZED)) ? 1 : 0;
    place->x = x;
    place->y = y;
    place->w = w;
    place->h = h;
    place->maximized = maxed;
    place->placed = 1;
    snprintf(buf, sizeof(buf), "%d", x);
    WritePrivateProfileStringA(section, "x", buf, g_paths.settings);
    snprintf(buf, sizeof(buf), "%d", y);
    WritePrivateProfileStringA(section, "y", buf, g_paths.settings);
    snprintf(buf, sizeof(buf), "%d", w);
    WritePrivateProfileStringA(section, "w", buf, g_paths.settings);
    snprintf(buf, sizeof(buf), "%d", h);
    WritePrivateProfileStringA(section, "h", buf, g_paths.settings);
    WritePrivateProfileStringA(section, "max", maxed ? "1" : "0", g_paths.settings);
}

static void save_char_ini(const char *key, char c) {
    char one[4];
    one[0] = c ? c : ' ';
    one[1] = 0;
    WritePrivateProfileStringA("parse", key, one, g_paths.settings);
}

static void save_settings(HWND hwnd) {
    char buf[64];
    WINDOWPLACEMENT wp;
    wp.length = sizeof(wp);
    EnterCriticalSection(&g_lock);
    if (hwnd && GetWindowPlacement(hwnd, &wp)) {
        RECT r = wp.rcNormalPosition;
        snprintf(buf, sizeof(buf), "%d", (int)r.left);
        WritePrivateProfileStringA("window", "x", buf, g_paths.settings);
        snprintf(buf, sizeof(buf), "%d", (int)r.top);
        WritePrivateProfileStringA("window", "y", buf, g_paths.settings);
        snprintf(buf, sizeof(buf), "%d", (int)(r.right - r.left));
        WritePrivateProfileStringA("window", "w", buf, g_paths.settings);
        snprintf(buf, sizeof(buf), "%d", (int)(r.bottom - r.top));
        WritePrivateProfileStringA("window", "h", buf, g_paths.settings);
        g_win_x = (int)r.left;
        g_win_y = (int)r.top;
        g_win_w = (int)(r.right - r.left);
        g_win_h = (int)(r.bottom - r.top);
    }
    WritePrivateProfileStringA("window", "topmost", g_topmost ? "1" : "0", g_paths.settings);
    WritePrivateProfileStringA("window", "auto_solve", g_auto_solve ? "1" : "0", g_paths.settings);
    WritePrivateProfileStringA("window", "auto_demo", g_auto_demo ? "1" : "0", g_paths.settings);
    WritePrivateProfileStringA("window", "auto_codes", g_auto_codes ? "1" : "0", g_paths.settings);
    WritePrivateProfileStringA("log", "path", g_eq_log_path, g_paths.settings);
    WritePrivateProfileStringA("log", "enabled", g_eq_log_enabled ? "1" : "0", g_paths.settings);
    WritePrivateProfileStringA("parse", "rack1", g_parse_cfg.rack_tag[0], g_paths.settings);
    WritePrivateProfileStringA("parse", "rack2", g_parse_cfg.rack_tag[1], g_paths.settings);
    WritePrivateProfileStringA("parse", "rack3", g_parse_cfg.rack_tag[2], g_paths.settings);
    WritePrivateProfileStringA("parse", "rack4", g_parse_cfg.rack_tag[3], g_paths.settings);
    save_char_ini("axe", g_parse_cfg.axe);
    save_char_ini("sword", g_parse_cfg.sword);
    save_char_ini("spear", g_parse_cfg.spear);
    save_char_ini("blank", g_parse_cfg.blank);
    for (int i = 0; i < HK_COUNT; i++) {
        char name[32];
        vk_to_name(g_hotkeys[i], name, sizeof(name));
        WritePrivateProfileStringA("hotkeys", hk_ini_key((HotkeyId)i), name, g_paths.settings);
    }
    LeaveCriticalSection(&g_lock);
}

static void queue_match(const LmMatch *m, const char *line) {
    if (!m || m->rack < 0 || m->rack >= 4) return;
    EnterCriticalSection(&g_lock);
    g_seq++;
    g_rack_seq[m->rack] = g_seq;
    g_rack_has[m->rack] = 1;
    memcpy(g_rack_slots[m->rack], m->slots, 4);
    copy_str(g_eq_last_match, sizeof(g_eq_last_match), m->token);
    snprintf(g_eq_status, sizeof(g_eq_status), "Matched %s", m->token);
    g_origin[m->rack].has = 1;
    memcpy(g_origin[m->rack].slots, m->slots, 4);
    copy_str(g_origin[m->rack].line, sizeof(g_origin[m->rack].line), line ? line : "");
    LeaveCriticalSection(&g_lock);
}

static void log_parsed_line(const char *line, const LmMatch *matches, int n) {
    char layout[192];
    char *detail;
    size_t cap;
    int present[4] = {0, 0, 0, 0};
    int first = 1;
    if (!line || !matches || n <= 0) return;
    layout[0] = 0;
    for (int k = 0; k < n; k++) {
        int rack = matches[k].rack;
        if (rack < 0 || rack >= 4) continue;
        present[rack] = 1;
    }
    for (int rack = 0; rack < 4; rack++) {
        if (!present[rack]) continue;
        for (int k = n - 1; k >= 0; k--) {
            if (matches[k].rack != rack) continue;
            append_rack_layout(layout, sizeof(layout), rack, matches[k].slots, first);
            first = 0;
            break;
        }
    }
    cap = strlen(layout) + strlen(line) + 24;
    detail = (char *)malloc(cap);
    if (!detail) {
        log_write("log_parse", line);
        return;
    }
    snprintf(detail, cap, "layout=%s; line=%s", layout, line);
    log_write("log_parse", detail);
    free(detail);
}

static void log_board_solution(const uint8_t slots[4][4], const WrSolveResult *result, int ok) {
    RackOrigin snap[4];
    uint8_t shown[4][4];
    const char *srcs[4];
    char layout[192];
    char *detail;
    size_t cap;
    int nsrc = 0;
    const char *paste = (result && result->paste[0]) ? result->paste : "(none)";
    const char *err = (result && result->error[0]) ? result->error : "Solve failed";

    EnterCriticalSection(&g_lock);
    memcpy(snap, g_origin, sizeof(snap));
    LeaveCriticalSection(&g_lock);

    for (int r = 0; r < 4; r++) {
        if (origin_matches(&snap[r], slots[r])) {
            memcpy(shown[r], snap[r].slots, 4);
            if (snap[r].line[0]) {
                int seen = 0;
                for (int i = 0; i < nsrc; i++) {
                    if (strcmp(srcs[i], snap[r].line) == 0) seen = 1;
                }
                if (!seen) srcs[nsrc++] = snap[r].line;
            }
        } else {
            memcpy(shown[r], slots[r], 4);
        }
    }

    layout[0] = 0;
    for (int r = 0; r < 4; r++) append_rack_layout(layout, sizeof(layout), r, shown[r], r == 0);

    cap = 640 + WR_PASTE_CAP;
    for (int i = 0; i < nsrc; i++) cap += strlen(srcs[i]) + 4;
    detail = (char *)malloc(cap);
    if (!detail) {
        log_write(ok ? "solve" : "solve_failed", layout);
        return;
    }
    detail[0] = 0;
    if (ok) {
        char head[256];
        snprintf(head, sizeof(head), "moves=%d; solve_ms=%.1f; layout=%s", result ? result->move_count : 0,
                 result ? result->solve_ms : 0.0, layout);
        append_text(detail, cap, head);
    } else {
        char head[512];
        snprintf(head, sizeof(head), "layout=%s; error=%s", layout, err);
        append_text(detail, cap, head);
    }
    if (nsrc > 0) {
        append_text(detail, cap, "; log=");
        for (int i = 0; i < nsrc; i++) {
            if (i) append_text(detail, cap, " || ");
            append_text(detail, cap, srcs[i]);
        }
    }
    if (ok) {
        append_text(detail, cap, "; paste=");
        append_text(detail, cap, paste);
    }
    log_write(ok ? "solve" : "solve_failed", detail);
    free(detail);
}

static void set_eq_status(const char *text) {
    EnterCriticalSection(&g_lock);
    copy_str(g_eq_status, sizeof(g_eq_status), text ? text : "");
    LeaveCriticalSection(&g_lock);
}

static void eq_log_close(void) {
    if (g_eq_log_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(g_eq_log_handle);
        g_eq_log_handle = INVALID_HANDLE_VALUE;
    }
    g_eq_log_offset = 0;
    g_eq_line_len = 0;
    g_eq_line_buf[0] = 0;
}

static int eq_log_open_at_end(void) {
    LARGE_INTEGER size;
    LARGE_INTEGER pos;
    eq_log_close();
    if (!g_eq_log_path[0]) {
        set_eq_status("Idle - no log path");
        return 0;
    }
    g_eq_log_handle = CreateFileA(g_eq_log_path, GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_eq_log_handle == INVALID_HANDLE_VALUE) {
        set_eq_status("Idle - cannot open log");
        return 0;
    }
    if (!GetFileSizeEx(g_eq_log_handle, &size)) {
        eq_log_close();
        set_eq_status("Idle - size error");
        return 0;
    }
    g_eq_log_offset = size.QuadPart;
    pos.QuadPart = g_eq_log_offset;
    SetFilePointerEx(g_eq_log_handle, pos, NULL, FILE_BEGIN);
    set_eq_status("Watching");
    return 1;
}

static void feed_line(const char *line) {
    LmMatch matches[4];
    int n = lm_parse_line(line, &g_parse_cfg, matches, 4);
    for (int k = 0; k < n; k++) queue_match(&matches[k], line);
    if (n > 0) log_parsed_line(line, matches, n);
}

static void eq_log_feed_bytes(const char *data, DWORD len) {
    for (DWORD i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\r') continue;
        if (c == '\n') {
            g_eq_line_buf[g_eq_line_len] = 0;
            if (g_eq_line_len > 0) feed_line(g_eq_line_buf);
            g_eq_line_len = 0;
            continue;
        }
        if (g_eq_line_len + 1 < LOGMON_LINE_CAP) g_eq_line_buf[g_eq_line_len++] = c;
    }
}

static void eq_log_poll(void) {
    LARGE_INTEGER size;
    LONGLONG remain;
    if (!g_eq_log_enabled || !g_eq_log_path[0]) return;
    if (g_eq_log_handle == INVALID_HANDLE_VALUE) {
        if (!eq_log_open_at_end()) return;
    }
    if (!GetFileSizeEx(g_eq_log_handle, &size)) return;
    if (size.QuadPart < g_eq_log_offset) {
        LARGE_INTEGER pos;
        g_eq_log_offset = size.QuadPart;
        g_eq_line_len = 0;
        pos.QuadPart = g_eq_log_offset;
        SetFilePointerEx(g_eq_log_handle, pos, NULL, FILE_BEGIN);
        set_eq_status("Watching (rotated)");
        return;
    }
    if (size.QuadPart == g_eq_log_offset) return;
    remain = size.QuadPart - g_eq_log_offset;
    while (remain > 0) {
        DWORD chunk = (DWORD)((remain > LOGMON_READ_CAP) ? LOGMON_READ_CAP : remain);
        char buf[LOGMON_READ_CAP];
        DWORD got = 0;
        if (!ReadFile(g_eq_log_handle, buf, chunk, &got, NULL) || got == 0) break;
        eq_log_feed_bytes(buf, got);
        g_eq_log_offset += got;
        remain -= got;
    }
}

static int scan_file(const char *path, char *status, size_t status_cap) {
    HANDLE h;
    LARGE_INTEGER size;
    LARGE_INTEGER pos;
    LONGLONG start;
    char buf[LOGMON_READ_CAP];
    DWORD got = 0;
    char line[LOGMON_LINE_CAP];
    int llen = 0;
    int found = 0;
    if (!path || !path[0]) {
        snprintf(status, status_cap, "No log path");
        return 0;
    }
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        snprintf(status, status_cap, "Cannot open for scan");
        return 0;
    }
    if (!GetFileSizeEx(h, &size)) {
        CloseHandle(h);
        snprintf(status, status_cap, "Cannot read log size");
        return 0;
    }
    start = size.QuadPart > LOGMON_SCAN_RECENT ? size.QuadPart - LOGMON_SCAN_RECENT : 0;
    pos.QuadPart = start;
    SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
    while (ReadFile(h, buf, sizeof(buf), &got, NULL) && got > 0) {
        for (DWORD i = 0; i < got; i++) {
            char c = buf[i];
            if (c == '\r') continue;
            if (c == '\n') {
                line[llen] = 0;
                if (llen > 0) {
                    LmMatch matches[4];
                    int n = lm_parse_line(line, &g_parse_cfg, matches, 4);
                    for (int k = 0; k < n; k++) queue_match(&matches[k], line);
                    if (n > 0) {
                        log_parsed_line(line, matches, n);
                        found += n;
                    }
                }
                llen = 0;
                continue;
            }
            if (llen + 1 < LOGMON_LINE_CAP) line[llen++] = c;
        }
    }
    CloseHandle(h);
    if (found)
        snprintf(status, status_cap, "Scan found %d match(es)", found);
    else
        snprintf(status, status_cap, "Scan - no matches in last 64KB");
    set_eq_status(status);
    return 1;
}

static void eq_log_start_monitor(void) {
    if (g_hwnd) KillTimer(g_hwnd, ID_TIMER_LOGMON);
    eq_log_close();
    if (g_eq_log_enabled && g_eq_log_path[0]) {
        eq_log_open_at_end();
        if (g_hwnd) SetTimer(g_hwnd, ID_TIMER_LOGMON, LOGMON_MS, NULL);
    } else {
        set_eq_status("Idle");
    }
}

static void eq_log_stop_monitor(void) {
    if (g_hwnd) KillTimer(g_hwnd, ID_TIMER_LOGMON);
    eq_log_close();
    set_eq_status("Idle");
}

static void push_zorder(void) {
    HWND list[4];
    HWND z = g_topmost ? HWND_TOPMOST : HWND_NOTOPMOST;
    list[0] = g_hwnd;
    list[1] = g_demo_hwnd;
    list[2] = g_codes_hwnd;
    list[3] = g_settings_hwnd;
    for (int i = 0; i < 4; i++) {
        if (list[i] && IsWindow(list[i]))
            SetWindowPos(list[i], z, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

static void apply_topmost(void) {
    push_zorder();
    if (!g_hwnd) return;
    if (g_topmost)
        SetTimer(g_hwnd, ID_TIMER_TOPMOST, TOPMOST_MS, NULL);
    else
        KillTimer(g_hwnd, ID_TIMER_TOPMOST);
}

static int copy_clipboard(const char *text) {
    int n;
    int wn;
    wchar_t *wide;
    HGLOBAL hansi;
    HGLOBAL hwide;
    char *pa;
    wchar_t *pw;
    int opened = 0;
    if (!text) text = "";
    n = (int)strlen(text);
    wide = (wchar_t *)malloc(((size_t)n + 1) * sizeof(wchar_t));
    if (!wide) return 0;
    wn = MultiByteToWideChar(CP_ACP, 0, text, n + 1, wide, n + 1);
    if (wn <= 0) {
        free(wide);
        return 0;
    }
    for (int i = 0; i < 10; i++) {
        if (OpenClipboard(g_hwnd)) {
            opened = 1;
            break;
        }
        Sleep(15);
    }
    if (!opened) {
        free(wide);
        return 0;
    }
    EmptyClipboard();
    hansi = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)n + 1);
    hwide = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)wn * sizeof(wchar_t));
    if (!hansi || !hwide) {
        if (hansi) GlobalFree(hansi);
        if (hwide) GlobalFree(hwide);
        CloseClipboard();
        free(wide);
        return 0;
    }
    pa = (char *)GlobalLock(hansi);
    memcpy(pa, text, (size_t)n + 1);
    GlobalUnlock(hansi);
    pw = (wchar_t *)GlobalLock(hwide);
    memcpy(pw, wide, (size_t)wn * sizeof(wchar_t));
    GlobalUnlock(hwide);
    free(wide);
    if (!SetClipboardData(CF_UNICODETEXT, hwide)) {
        GlobalFree(hwide);
        GlobalFree(hansi);
        CloseClipboard();
        return 0;
    }
    if (!SetClipboardData(CF_TEXT, hansi)) GlobalFree(hansi);
    CloseClipboard();
    return 1;
}

static int copy_solution(int automatic) {
    if (!g_has_result || !g_result.paste[0]) {
        if (!automatic) set_error("Nothing to copy.");
        return 0;
    }
    if (!copy_clipboard(g_result.paste)) {
        if (!automatic) set_error("Could not copy to clipboard.");
        return 0;
    }
    log_write("copy", automatic ? "solution (auto)" : "solution");
    if (!automatic) set_error("");
    return 1;
}

static void board_totals(int out[3]) {
    out[0] = out[1] = out[2] = 0;
    for (int r = 0; r < 4; r++) {
        for (int s = 0; s < 4; s++) {
            uint8_t c = g_board[r][s];
            if (c >= 1 && c <= 3) out[c - 1]++;
        }
    }
}

static int counts_ready(char *status, size_t cap) {
    int t[3];
    const char *names[3] = {"Swords", "Spears", "Axes"};
    int ok;
    board_totals(t);
    ok = t[0] == 4 && t[1] == 4 && t[2] == 4;
    if (!status || cap == 0) return ok;
    if (g_solving) {
        snprintf(status, cap, "Solving");
        return ok;
    }
    if (ok) {
        char key[32];
        vk_to_name(g_hotkeys[HK_SOLVE], key, sizeof(key));
        snprintf(status, cap, "Ready - press %s", key);
        return 1;
    }
    status[0] = 0;
    for (int i = 0; i < 3; i++) {
        char piece[48];
        if (t[i] == 4) continue;
        snprintf(piece, sizeof(piece), "%s%s %d/4", status[0] ? "  |  " : "", names[i], t[i]);
        append_text(status, cap, piece);
    }
    return 0;
}

static void hide_result(void) { g_has_result = 0; }

static void set_error(const char *msg) {
    copy_str(g_error, sizeof(g_error), msg ? msg : "");
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

static int bottom_index(const uint8_t rack[4]) {
    for (int i = 3; i >= 0; i--) {
        if (rack[i] >= 1 && rack[i] <= 3) return i;
    }
    return -1;
}

static int place_index(const uint8_t rack[4]) {
    int b = bottom_index(rack);
    if (b < 0) return 0;
    if (b >= 3) return -1;
    return b + 1;
}

static void demo_stop(void) {
    g_demo_playing = 0;
    if (g_demo_hwnd) KillTimer(g_demo_hwnd, ID_TIMER_DEMO);
}

static void demo_reset(void) {
    memcpy(g_demo_board, g_snap, sizeof(g_demo_board));
    g_demo_index = 0;
    g_demo_hl = 0;
}

static int demo_step(void) {
    WrMove m;
    int src, dst, from_s, to_s;
    if (g_demo_index >= g_demo_count) {
        demo_stop();
        return 0;
    }
    m = g_demo_moves[g_demo_index];
    src = m.src - 1;
    dst = m.dst - 1;
    if (src < 0 || src >= 4 || dst < 0 || dst >= 4) {
        demo_stop();
            return 0;
        }
    from_s = bottom_index(g_demo_board[src]);
    to_s = place_index(g_demo_board[dst]);
    if (from_s < 0 || to_s < 0 || g_demo_board[src][from_s] != (uint8_t)m.code) {
        demo_stop();
        return 0;
    }
    g_demo_board[src][from_s] = 0;
    g_demo_board[dst][to_s] = (uint8_t)m.code;
    g_demo_from_r = src;
    g_demo_from_s = from_s;
    g_demo_to_r = dst;
    g_demo_to_s = to_s;
    g_demo_hl = 1;
    g_demo_index++;
    if (g_demo_index >= g_demo_count) demo_stop();
    if (g_demo_hwnd) InvalidateRect(g_demo_hwnd, NULL, FALSE);
    return g_demo_playing && g_demo_index < g_demo_count;
}

static void apply_solve_result(SolveJob *job) {
    g_solving = 0;
    if (!job) return;
    if (job->gen != g_board_gen) {
        free(job);
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
        if (g_auto_solve) start_solve(1);
        return;
    }
    log_board_solution(job->shown, &job->result, job->ok);
    if (!job->ok) {
        hide_result();
        set_error(job->result.error[0] ? job->result.error : "Solve failed");
    } else {
        g_result = job->result;
        g_has_result = 1;
        memcpy(g_snap, job->snap, sizeof(g_snap));
        g_demo_count = g_result.move_count;
        if (g_demo_count < 0) g_demo_count = 0;
        if (g_demo_count > WR_MAX_MOVES) g_demo_count = WR_MAX_MOVES;
        if (g_demo_count > 0) memcpy(g_demo_moves, g_result.moves, sizeof(WrMove) * (size_t)g_demo_count);
        set_error("");
        g_scroll_result = 1;
        if (g_result.paste[0]) copy_solution(1);
        if (g_auto_demo && g_demo_count > 0) open_demo();
        if (g_auto_codes && g_demo_count > 0) open_codes();
        if (g_codes_hwnd && IsWindow(g_codes_hwnd)) {
            g_codes_scroll = 0;
            InvalidateRect(g_codes_hwnd, NULL, FALSE);
        }
    }
    free(job);
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

static DWORD WINAPI solve_thread(LPVOID arg) {
    SolveJob *job = (SolveJob *)arg;
    job->ok = wr_solve(job->packed, &job->result);
    if (!PostMessageA(job->hwnd, WM_APP_SOLVED, 0, (LPARAM)job)) free(job);
    return 0;
}

static void start_solve(int automatic) {
    char summary[256];
    int ready;
    SolveJob *job;
    HANDLE thread;
    if (g_solving) return;
    ready = counts_ready(summary, sizeof(summary));
    if (!ready) {
        if (!automatic) set_error(summary);
        return;
    }
    job = (SolveJob *)calloc(1, sizeof(SolveJob));
    if (!job) {
        set_error("Out of memory");
        return;
    }
    job->hwnd = g_hwnd;
    job->gen = g_board_gen;
    for (int r = 0; r < 4; r++) {
        for (int s = 0; s < 4; s++) {
            uint8_t cell = g_board[r][s];
            job->shown[r][s] = cell;
            job->packed[r][s] = cell == 4 ? 0 : cell;
            job->snap[r][s] = cell == 4 ? 0 : cell;
        }
    }
    g_solving = 1;
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
    thread = CreateThread(NULL, 0, solve_thread, job, 0, NULL);
    if (!thread) {
        job->ok = wr_solve(job->packed, &job->result);
        apply_solve_result(job);
        return;
    }
    CloseHandle(thread);
}

static void place_weapon(uint8_t cell) {
    g_board[g_sel_r][g_sel_s] = cell;
    if (g_sel_s < 3)
        g_sel_s++;
    else if (g_sel_r < 3) {
        g_sel_r++;
        g_sel_s = 0;
    }
    g_board_gen++;
    hide_result();
    set_error("");
    if (g_auto_solve) start_solve(1);
}

static void undo_cell(void) {
    if (g_board[g_sel_r][g_sel_s] != 0) {
        g_board[g_sel_r][g_sel_s] = 0;
    } else {
        if (g_sel_s > 0)
            g_sel_s--;
        else if (g_sel_r > 0) {
            g_sel_r--;
            g_sel_s = 3;
        }
        g_board[g_sel_r][g_sel_s] = 0;
    }
    g_board_gen++;
    hide_result();
    set_error("");
    if (g_auto_solve) start_solve(1);
}

static void clear_board(void) {
    memset(g_board, 0, sizeof(g_board));
    g_sel_r = 0;
    g_sel_s = 0;
    g_board_gen++;
    hide_result();
    set_error("");
    log_write("clear", "Board cleared");
    if (g_codes_hwnd && IsWindow(g_codes_hwnd)) InvalidateRect(g_codes_hwnd, NULL, FALSE);
}

static void pull_matches(void) {
    uint8_t incoming[4][4];
    int apply[4] = {0, 0, 0, 0};
    int any = 0;
    char status[128];
    int enabled;
    int changed;
    EnterCriticalSection(&g_lock);
    enabled = g_eq_log_enabled;
    copy_str(status, sizeof(status), g_eq_status);
    for (int r = 0; r < 4; r++) {
        if (g_rack_has[r] && g_rack_seq[r] > g_seen_seq) {
            memcpy(incoming[r], g_rack_slots[r], 4);
            apply[r] = 1;
            any = 1;
        }
    }
    if (g_seq > g_seen_seq) g_seen_seq = g_seq;
    LeaveCriticalSection(&g_lock);
    changed = strcmp(status, g_view_status) != 0 || enabled != g_view_enabled;
    copy_str(g_view_status, sizeof(g_view_status), status);
    g_view_enabled = enabled;
    if (any) {
        for (int r = 0; r < 4; r++) {
            if (apply[r]) memcpy(g_board[r], incoming[r], 4);
        }
        g_board_gen++;
        hide_result();
        set_error("");
        if (g_auto_solve) start_solve(1);
    } else if (changed && g_hwnd) {
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static void do_scan(const char *path) {
    char status[128];
    if (!scan_file(path, status, sizeof(status)))
        set_error(status);
    else
        set_error("");
    pull_matches();
}

static COLORREF blend_color(COLORREF a, COLORREF b, int t) {
    int u;
    if (t < 0) t = 0;
    if (t > 255) t = 255;
    u = 255 - t;
    return RGB((GetRValue(a) * u + GetRValue(b) * t) / 255, (GetGValue(a) * u + GetGValue(b) * t) / 255,
               (GetBValue(a) * u + GetBValue(b) * t) / 255);
}

static RECT rect_xywh(int x, int y, int w, int h) {
    RECT r;
    r.left = x;
    r.top = y;
    r.right = x + w;
    r.bottom = y + h;
    return r;
}

static void add_hit(RECT r, int id) {
    if (g_nhits >= HIT_CAP) return;
    g_hits[g_nhits].rc = r;
    g_hits[g_nhits].id = id;
    g_nhits++;
}

static int hit_test(int x, int y) {
    POINT pt;
    pt.x = x;
    pt.y = y;
    for (int i = g_nhits - 1; i >= 0; i--) {
        if (PtInRect(&g_hits[i].rc, pt)) return g_hits[i].id;
    }
    return HIT_NONE;
}

static int is_hot(HWND hwnd, int id) { return g_hover_hwnd == hwnd && g_hover == id; }

static void fill_round(HDC hdc, RECT r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH brush;
    HPEN pen;
    HGDIOBJ old_b;
    HGDIOBJ old_p;
    if (!g_draw) return;
    if (r.right <= r.left || r.bottom <= r.top) return;
    brush = CreateSolidBrush(fill);
    pen = CreatePen(PS_SOLID, 1, border);
    old_b = SelectObject(hdc, brush);
    old_p = SelectObject(hdc, pen);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(hdc, old_b);
    SelectObject(hdc, old_p);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void draw_text(HDC hdc, RECT r, const char *text, COLORREF color, HFONT font, UINT fmt) {
    HFONT old;
    if (!g_draw) return;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    old = (HFONT)SelectObject(hdc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextA(hdc, text ? text : "", -1, &r, fmt | DT_NOPREFIX);
    SelectObject(hdc, old);
}

static int text_height(HDC hdc, HFONT font, const char *text, int width, UINT fmt) {
    RECT r;
    HFONT old;
    int h;
    r.left = 0;
    r.top = 0;
    r.right = width > 0 ? width : 1;
    r.bottom = 0;
    old = (HFONT)SelectObject(hdc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextA(hdc, text ? text : "", -1, &r, fmt | DT_CALCRECT | DT_NOPREFIX);
    SelectObject(hdc, old);
    h = r.bottom - r.top;
    return h > 0 ? h : S(16);
}

static void draw_check_mark(HDC hdc, RECT b) {
    int box = b.right - b.left;
    int x1, y1, x2, y2, x3, y3;
    HPEN pen;
    HPEN old;
    if (!g_draw || box < 4) return;
    x1 = b.left + box / 5;
    y1 = b.top + box / 2;
    x2 = b.left + (box * 2) / 5;
    y2 = b.bottom - box / 4;
    x3 = b.right - box / 5;
    y3 = b.top + box / 4;
    pen = CreatePen(PS_SOLID, S(2) > 0 ? S(2) : 1, RGB(255, 255, 255));
    old = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    LineTo(hdc, x3, y3);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void draw_check(HDC hdc, HWND hwnd, RECT r, const char *text, int on, int id) {
    int box = S(16);
    int cy = r.top + ((r.bottom - r.top) - box) / 2;
    RECT b = rect_xywh(r.left, cy, box, box);
    RECT label = r;
    add_hit(r, id);
    fill_round(hdc, b, S(4), on ? COL_OK : COL_PANEL2, on ? COL_OK : (is_hot(hwnd, id) ? COL_FOCUS : COL_BORDER));
    if (on) draw_check_mark(hdc, b);
    label.left = b.right + S(8);
        draw_text(hdc, label, text, COL_TEXT, g_font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void draw_button(HDC hdc, HWND hwnd, RECT r, const char *text, int id, int primary, int enabled, COLORREF accent) {
    int hot = enabled && is_hot(hwnd, id);
    COLORREF fill = primary ? RGB(58, 99, 224) : COL_PANEL;
    COLORREF border = accent ? accent : (primary ? RGB(107, 143, 255) : COL_BORDER);
    COLORREF textc = enabled ? COL_TEXT : COL_MUTED;
    RECT label = r;
    if (!enabled) fill = primary ? RGB(36, 44, 64) : COL_PANEL;
    if (hot) fill = blend_color(fill, RGB(255, 255, 255), 22);
    if (enabled) add_hit(r, id);
    fill_round(hdc, r, S(8), fill, border);
    InflateRect(&label, -S(4), 0);
    draw_text(hdc, label, text, textc, g_font_bold, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void slot_colors(uint8_t cell, COLORREF *fill, COLORREF *border, COLORREF *text) {
    *fill = COL_PANEL2;
    *border = COL_BORDER;
    *text = COL_TEXT;
    if (cell == 1) {
        *fill = blend_color(COL_PANEL2, COL_SW, 72);
        *border = blend_color(COL_BORDER, COL_SW, 180);
        *text = RGB(219, 232, 255);
    } else if (cell == 2) {
        *fill = blend_color(COL_PANEL2, COL_SP, 72);
        *border = blend_color(COL_BORDER, COL_SP, 180);
        *text = RGB(217, 255, 233);
    } else if (cell == 3) {
        *fill = blend_color(COL_PANEL2, COL_AX, 72);
        *border = blend_color(COL_BORDER, COL_AX, 180);
        *text = RGB(255, 229, 216);
    } else if (cell == 4 || cell == 0) {
        *fill = COL_PANEL;
        *border = blend_color(COL_BORDER, COL_BG, 100);
        *text = COL_MUTED;
    }
}

static void draw_slot(HDC hdc, RECT r, uint8_t cell, COLORREF border_override, int use_override) {
    COLORREF fill, border, text;
    const char *label;
    slot_colors(cell, &fill, &border, &text);
    if (use_override) border = border_override;
    fill_round(hdc, r, S(6), fill, border);
    label = cell == 1 ? "Sw" : cell == 2 ? "Sp" : cell == 3 ? "Ax" : cell == 4 ? "-" : "";
    draw_text(hdc, r, label, text, g_font_bold, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void draw_racks(HDC hdc, HWND hwnd, int x, int y, int width, int slot_h, const uint8_t board[4][4],
                       int interactive, int highlight) {
    int gap = S(6);
    int inner = S(5);
    int title_h = S(22);
    int rack_w = (width - gap * 3) / 4;
    int rack_h = title_h + inner + slot_h * 4 + S(6) * 3 + inner;
    for (int r = 0; r < 4; r++) {
        int rx = x + r * (rack_w + gap);
        RECT panel = rect_xywh(rx, y, rack_w, rack_h);
        RECT title = rect_xywh(rx, y + S(4), rack_w, title_h);
        char caption[32];
        fill_round(hdc, panel, S(8), COL_PANEL, COL_BORDER);
        if (highlight) {
            int lab = g_result.final_labels[r];
            const char *abbr = lab == 1 ? "Sw" : lab == 2 ? "Sp" : lab == 3 ? "Ax" : "";
            if (abbr[0])
                snprintf(caption, sizeof(caption), "Rack %d  %s", r + 1, abbr);
            else
                snprintf(caption, sizeof(caption), "Rack %d", r + 1);
            draw_text(hdc, title, caption, lab == 1   ? COL_SW
                                            : lab == 2 ? COL_SP
                                            : lab == 3 ? COL_AX
                                                       : COL_MUTED,
                      g_font_bold, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            snprintf(caption, sizeof(caption), "Rack %d", r + 1);
            draw_text(hdc, title, caption, COL_MUTED, g_font_bold, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        for (int s = 0; s < 4; s++) {
            int sy = y + title_h + inner + s * (slot_h + S(6));
            RECT slot = rect_xywh(rx + inner, sy, rack_w - inner * 2, slot_h);
            int selected = interactive && g_sel_r == r && g_sel_s == s;
            /* Demo highlights belong on the demo window only. The solved view stays still. */
            int show_move = !interactive && !highlight && g_demo_hl;
            int from = show_move && g_demo_from_r == r && g_demo_from_s == s;
            int to = show_move && g_demo_to_r == r && g_demo_to_s == s;
            COLORREF border = COL_FOCUS;
            int override = 0;
            if (interactive) add_hit(slot, HIT_SLOT0 + r * 4 + s);
            if (selected || (interactive && is_hot(hwnd, HIT_SLOT0 + r * 4 + s))) {
                border = COL_FOCUS;
                override = 1;
            }
            if (!interactive && from) {
                border = COL_FOCUS;
                override = 1;
            }
            if (!interactive && to) {
                border = COL_OK;
                override = 1;
            }
            draw_slot(hdc, slot, board[r][s], border, override);
        }
    }
}

static int text_span(HDC hdc, HFONT font, const char *text) {
    SIZE sz;
    HFONT old;
    sz.cx = 0;
    sz.cy = 0;
    if (!text || !text[0]) return 0;
    old = (HFONT)SelectObject(hdc, font ? font : g_font_small);
    GetTextExtentPoint32A(hdc, text, (int)strlen(text), &sz);
    SelectObject(hdc, old);
    return sz.cx;
}

/* Draw the shortcut line. Hotkeys sit in key-shaped boxes. Returns the height used. */
static int draw_hint(HDC hdc, int left, int top, int width) {
    char axe[16], sword[16], spear[16], gapk[16], undo[16], r1[16], r4[16], solve[16];
    const char *text[24];
    int kind[24]; /* 0=label, 1=key, 2=group gap */
    int n = 0;
    int x = left;
    int y = top;
    int right = left + width;
    int line_h = S(22);
    int gap_px = S(4);
    int group_gap = S(14);
    int i;
    vk_to_name(g_hotkeys[HK_AXE], axe, sizeof(axe));
    vk_to_name(g_hotkeys[HK_SWORD], sword, sizeof(sword));
    vk_to_name(g_hotkeys[HK_SPEAR], spear, sizeof(spear));
    vk_to_name(g_hotkeys[HK_GAP], gapk, sizeof(gapk));
    vk_to_name(g_hotkeys[HK_UNDO], undo, sizeof(undo));
    vk_to_name(g_hotkeys[HK_RACK1], r1, sizeof(r1));
    vk_to_name(g_hotkeys[HK_RACK4], r4, sizeof(r4));
    vk_to_name(g_hotkeys[HK_SOLVE], solve, sizeof(solve));
    /* Weapons */
    text[n] = axe;
    kind[n++] = 1;
    text[n] = sword;
    kind[n++] = 1;
    text[n] = spear;
    kind[n++] = 1;
    text[n] = "";
    kind[n++] = 2;
    /* Editing */
    text[n] = gapk;
    kind[n++] = 1;
    text[n] = "gap";
    kind[n++] = 0;
    text[n] = undo;
    kind[n++] = 1;
    text[n] = "undo";
    kind[n++] = 0;
    text[n] = "";
    kind[n++] = 2;
    /* Racks */
    text[n] = r1;
    kind[n++] = 1;
    text[n] = "-";
    kind[n++] = 0;
    text[n] = r4;
    kind[n++] = 1;
    text[n] = "rack";
    kind[n++] = 0;
    text[n] = "arrows";
    kind[n++] = 0;
    text[n] = "";
    kind[n++] = 2;
    /* Solve */
    text[n] = solve;
    kind[n++] = 1;
    text[n] = "solve";
    kind[n++] = 0;
    for (i = 0; i < n; i++) {
        int tw, box_w, box_h, item_w, step;
        RECT r;
        if (kind[i] == 2) {
            x += group_gap - gap_px;
            continue;
        }
        tw = text_span(hdc, g_font_small, text[i]);
        box_w = kind[i] == 1 ? tw + S(10) : tw;
        box_h = kind[i] == 1 ? S(20) : line_h;
        item_w = box_w;
        if (kind[i] == 1 && box_w < S(20)) {
            box_w = S(20);
            item_w = box_w;
        }
        step = item_w + gap_px;
        if (x > left && x + item_w > right) {
            x = left;
            y += line_h + S(4);
        }
        r = rect_xywh(x, y + (line_h - box_h) / 2, box_w, box_h);
        if (kind[i] == 1) {
            fill_round(hdc, r, S(5), COL_PANEL2, COL_BORDER);
            draw_text(hdc, r, text[i], COL_TEXT, g_font_small, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            draw_text(hdc, r, text[i], COL_MUTED, g_font_small, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        x += step;
    }
    return (y - top) + line_h;
}

static void visible_eq_status(char *out, size_t cap) {
    out[0] = 0;
    if (g_view_enabled) {
        copy_str(out, cap, g_view_status);
        return;
    }
    if (strncmp(g_view_status, "Scan", 4) == 0) copy_str(out, cap, g_view_status);
}

static void layout_main(HWND hwnd, HDC hdc) {
    RECT client;
    int pad, gap, width, y, row_h;
    char logline[96];
    char eq[128];
    int totals[3];
    int ready;
    int hint_h;
    int slot_h;
    int solve_on;
    GetClientRect(hwnd, &client);
    g_nhits = 0;
    g_hit_hwnd = hwnd;
    pad = S(16);
    gap = S(10);
    width = client.right - pad * 2;
    if (width < S(240)) width = S(240);
    y = pad - g_scroll;

    row_h = S(32);
    {
        int settings_w = S(96);
        int top_w = S(90);
        int title_w = width - settings_w - top_w - S(16);
        RECT title = rect_xywh(pad, y, title_w > S(80) ? title_w : S(80), S(34));
        int cy = y + (S(34) - row_h) / 2;
        draw_text(hdc, title, APP_TITLE, COL_TEXT, g_font_title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        draw_check(hdc, hwnd, rect_xywh(pad + width - settings_w - gap - top_w, cy, top_w, row_h), "On top", g_topmost,
                   HIT_TOP);
        draw_button(hdc, hwnd, rect_xywh(pad + width - settings_w, cy, settings_w, row_h), "Settings", HIT_SETTINGS, 0, 1,
                    0);
        y += S(36);
    }

    hint_h = draw_hint(hdc, pad, y, width);
    if (hint_h < S(22)) hint_h = S(22);
    y += hint_h + S(8);

    {
        int scan_w = S(114);
        int bx = pad + width;
        int check_w = pad;
        if (g_eq_log_path[0]) {
            bx -= scan_w;
            draw_button(hdc, hwnd, rect_xywh(bx, y, scan_w, row_h), "Scan recent", HIT_SCAN, 0, 1, 0);
        }
        {
            const char *labels[3] = {"Auto solve", "Auto demo", "Auto codes"};
            int states[3] = {g_auto_solve, g_auto_demo, g_auto_codes};
            int ids[3] = {HIT_AUTO, HIT_AUTODEMO, HIT_AUTOCODES};
            for (int i = 0; i < 3; i++) {
                int w = S(16) + S(8) + text_span(hdc, g_font, labels[i]) + S(4);
                if (check_w + w > bx - gap) w = bx - gap - check_w;
                if (w < S(20)) break;
                draw_check(hdc, hwnd, rect_xywh(check_w, y, w, row_h), labels[i], states[i], ids[i]);
                check_w += w + S(12);
            }
        }
        y += row_h + S(6);
    }

    snprintf(logline, sizeof(logline), "Log: %s", g_paths.log_name);
    visible_eq_status(eq, sizeof(eq));
    {
        int half = width / 2;
        RECT left = rect_xywh(pad, y, half, S(18));
        RECT right = rect_xywh(pad + half, y, width - half, S(18));
        draw_text(hdc, left, logline, COL_MUTED, g_font_small, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        draw_text(hdc, right, eq, COL_FOCUS, g_font_small, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += S(20);
    }

    board_totals(totals);
    ready = totals[0] == 4 && totals[1] == 4 && totals[2] == 4;
    {
        const char *names[3] = {"Sw", "Sp", "Ax"};
        COLORREF accents[3] = {COL_SW, COL_SP, COL_AX};
        int chip_w = S(88);
        int chip_h = S(28);
        for (int i = 0; i < 3; i++) {
            char chip[24];
            RECT cr = rect_xywh(pad + i * (chip_w + S(8)), y, chip_w, chip_h);
            COLORREF border = COL_BORDER;
            if (totals[i] == 4) border = COL_OK;
            else if (totals[i] > 4) border = COL_BAD;
            else border = blend_color(COL_BORDER, accents[i], 140);
            snprintf(chip, sizeof(chip), "%s %d/4", names[i], totals[i]);
            fill_round(hdc, cr, S(8), COL_PANEL, border);
            draw_text(hdc, cr, chip, COL_TEXT, g_font_bold, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        y += chip_h + gap;
    }

    {
        int gutter = S(18);
        int side_w = (width - gutter) / 2;
        int left_x = pad;
        int right_x = pad + side_w + gutter;
        int div_x = pad + side_w + gutter / 2;
        int racks_h;
        RECT left_cap = rect_xywh(left_x, y, side_w, S(20));
        RECT right_cap = rect_xywh(right_x, y, side_w, S(20));
        draw_text(hdc, left_cap, "Puzzle", COL_TEXT, g_font_bold, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        draw_text(hdc, right_cap, "Solved", COL_TEXT, g_font_bold, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += S(22);

        slot_h = S(50);
        racks_h = S(22) + S(5) + slot_h * 4 + S(6) * 3 + S(5);
        draw_racks(hdc, hwnd, left_x, y, side_w, slot_h, g_board, 1, 0);
        if (g_has_result) {
            draw_racks(hdc, hwnd, right_x, y, side_w, slot_h, g_result.final_board, 0, 1);
        } else {
            uint8_t empty[4][4] = {0};
            draw_racks(hdc, hwnd, right_x, y, side_w, slot_h, empty, 0, 0);
        }
        {
            HPEN pen = CreatePen(PS_SOLID, S(2) > 0 ? S(2) : 1, COL_FOCUS);
            HPEN old = (HPEN)SelectObject(hdc, pen);
            MoveToEx(hdc, div_x, y, NULL);
            LineTo(hdc, div_x, y + racks_h);
            SelectObject(hdc, old);
            DeleteObject(pen);
        }
        y += racks_h + gap;
    }

    {
        const char *labels[7] = {"Axe", "Sword", "Spear", "Gap", "Undo", "Clear", "Solve"};
        int ids[7] = {HIT_AXE, HIT_SWORD, HIT_SPEAR, HIT_GAP, HIT_UNDO, HIT_CLEAR, HIT_SOLVE};
        COLORREF accents[7] = {blend_color(COL_BORDER, COL_AX, 170), blend_color(COL_BORDER, COL_SW, 170),
                               blend_color(COL_BORDER, COL_SP, 170), 0, 0, 0, 0};
        int cluster = S(16);
        int inner = gap;
        int usable = width - cluster * 2 - inner * 4;
        int bw = usable / 7;
        int x = pad;
        solve_on = ready && !g_solving;
        for (int i = 0; i < 7; i++) {
            RECT br = rect_xywh(x, y, bw, row_h);
            int primary = i == 6;
            int enabled = i == 6 ? solve_on : 1;
            draw_button(hdc, hwnd, br, labels[i], ids[i], primary, enabled, accents[i]);
            x += bw;
            if (i == 2 || i == 5)
                x += cluster;
            else if (i < 6)
                x += inner;
        }
        y += row_h + gap;
    }

    if (g_solving) {
        RECT banner = rect_xywh(pad, y, width, S(26));
        draw_text(hdc, banner, "Solving - please wait...", COL_FOCUS, g_font_bold,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += S(30);
    }

    if (g_error[0]) {
        int eh = text_height(hdc, g_font, g_error, width, DT_WORDBREAK);
        RECT er = rect_xywh(pad, y, width, eh);
        draw_text(hdc, er, g_error, COL_BAD, g_font, DT_LEFT | DT_TOP | DT_WORDBREAK);
        y += eh + gap;
    }

    if (g_has_result) {
        char title[128];
        char timing[48];
        const char *paste = g_result.paste[0] ? g_result.paste : "(no moves)";
        int paste_max = S(36);
        int paste_h = text_height(hdc, g_font_small, paste, width - S(24), DT_WORDBREAK);
        int panel_h;
        RECT panel;
        int btn_w = S(86);
        int inner_y;
        int code_btns = g_result.move_count > 0 ? 2 : 0;
        if (paste_h < S(16)) paste_h = S(16);
        if (paste_h > paste_max) paste_h = paste_max;
        if (g_result.solve_ms < 1000.0)
            snprintf(timing, sizeof(timing), "%.1f ms", g_result.solve_ms);
        else
            snprintf(timing, sizeof(timing), "%.2f s", g_result.solve_ms / 1000.0);
        if (g_result.move_count == 0)
            snprintf(title, sizeof(title), "Already solved - %s", timing);
        else
            snprintf(title, sizeof(title), "%d move%s - %s", g_result.move_count,
                     g_result.move_count == 1 ? "" : "s", timing);
        panel_h = S(12) + row_h + S(6) + paste_h + S(12);
        g_result_y = y + g_scroll;
        panel = rect_xywh(pad, y, width, panel_h);
        fill_round(hdc, panel, S(10), COL_PANEL, COL_BORDER);
        inner_y = y + S(8);
        {
            RECT tr = rect_xywh(pad + S(12), inner_y, width - btn_w * (1 + code_btns) - S(12) - S(8) * code_btns, row_h);
            draw_text(hdc, tr, title, COL_TEXT, g_font_bold, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (g_result.move_count > 0) {
                RECT codes_r = rect_xywh(pad + width - S(12) - btn_w * 3 - S(16), inner_y, btn_w, row_h);
                RECT dr = rect_xywh(pad + width - S(12) - btn_w * 2 - S(8), inner_y, btn_w, row_h);
                draw_button(hdc, hwnd, codes_r, "Codes", HIT_CODES, 0, 1, 0);
                draw_button(hdc, hwnd, dr, "Demo", HIT_DEMO, 0, 1, 0);
            }
            {
                RECT cr = rect_xywh(pad + width - S(12) - btn_w, inner_y, btn_w, row_h);
                draw_button(hdc, hwnd, cr, "Copy", HIT_COPY, 1, g_result.paste[0] ? 1 : 0, 0);
            }
        }
        inner_y += row_h + S(4);
        {
            RECT tr = rect_xywh(pad + S(12), inner_y, width - S(24), paste_h);
            draw_text(hdc, tr, paste, COL_MUTED, g_font_small,
                      DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
        }
        y += panel_h + gap;
    }

    g_content_h = y + g_scroll + pad;
    if (g_content_h < client.bottom) g_content_h = client.bottom;
}

static void layout_demo(HWND hwnd, HDC hdc) {
    RECT client;
    int pad, gap, width, y, row_h, slot_h, avail;
    char status[128];
    GetClientRect(hwnd, &client);
    g_nhits = 0;
    g_hit_hwnd = hwnd;
    pad = S(16);
    gap = S(10);
    width = client.right - pad * 2;
    if (width < S(200)) width = S(200);
    y = pad;
    row_h = S(32);
    {
        RECT title = rect_xywh(pad, y, width - S(110), row_h);
        RECT close_r = rect_xywh(pad + width - S(100), y, S(100), row_h);
        draw_text(hdc, title, "Solution demo", COL_TEXT, g_font_title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        draw_button(hdc, hwnd, close_r, "Close", HIT_DEMO_CLOSE, 0, 1, 0);
        y += row_h + S(6);
    }
    if (g_demo_count <= 0)
        snprintf(status, sizeof(status), "No moves");
    else if (g_demo_index >= g_demo_count)
        snprintf(status, sizeof(status), "Done - %d move%s", g_demo_count, g_demo_count == 1 ? "" : "s");
    else {
        WrMove m = g_demo_moves[g_demo_index];
        snprintf(status, sizeof(status), "Move %d/%d - R%d%s>R%d", g_demo_index + 1, g_demo_count, m.src,
                 wr_code_abbrev(m.code), m.dst);
    }
    {
        RECT sr = rect_xywh(pad, y, width, S(22));
        draw_text(hdc, sr, status, COL_FOCUS, g_font_bold, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += S(28);
    }
    avail = client.bottom - y - row_h - pad - gap;
    slot_h = (avail - S(22) - S(5) - S(6) * 3 - S(5)) / 4;
    if (slot_h < S(36)) slot_h = S(36);
    if (slot_h > S(84)) slot_h = S(84);
    draw_racks(hdc, hwnd, pad, y, width, slot_h, g_demo_board, 0, 0);
    {
        int bw = S(110);
        int by = client.bottom - pad - row_h;
        draw_button(hdc, hwnd, rect_xywh(pad, by, bw, row_h), g_demo_playing ? "Pause" : "Play", HIT_DEMO_PLAY, 1,
                    g_demo_count > 0, 0);
        draw_button(hdc, hwnd, rect_xywh(pad + bw + gap, by, bw, row_h), "Step", HIT_DEMO_STEP, 0, g_demo_count > 0, 0);
        draw_button(hdc, hwnd, rect_xywh(pad + (bw + gap) * 2, by, bw, row_h), "Restart", HIT_DEMO_RESTART, 0,
                    g_demo_count > 0, 0);
    }
}

static void ensure_fonts(HWND hwnd) {
    int dpi = 96;
    if (hwnd) dpi = (int)GetDpiForWindow(hwnd);
    if (dpi < 96) dpi = 96;
    g_ui_dpi = dpi;
    if (g_font && dpi == g_font_dpi) return;
    if (g_font) DeleteObject(g_font);
    if (g_font_bold) DeleteObject(g_font_bold);
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_small) DeleteObject(g_font_small);
    g_font_dpi = dpi;
    g_font = CreateFontA(-MulDiv(11, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                         "Segoe UI");
    g_font_bold = CreateFontA(-MulDiv(11, dpi, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                              "Segoe UI");
    g_font_title = CreateFontA(-MulDiv(18, dpi, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                               "Segoe UI");
    g_font_small = CreateFontA(-MulDiv(10, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                               "Segoe UI");
    apply_settings_fonts();
}

static void clamp_scroll(HWND hwnd) {
    RECT rc;
    int maxs;
    GetClientRect(hwnd, &rc);
    maxs = g_content_h - rc.bottom;
    if (maxs < 0) maxs = 0;
    if (g_scroll > maxs) g_scroll = maxs;
    if (g_scroll < 0) g_scroll = 0;
}

static void update_scrollbar(HWND hwnd) {
    RECT rc;
    SCROLLINFO si;
    GetClientRect(hwnd, &rc);
    clamp_scroll(hwnd);
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = g_content_h > 0 ? g_content_h - 1 : 0;
    si.nPage = (UINT)(rc.bottom > 0 ? rc.bottom : 1);
    si.nPos = g_scroll;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

static void update_codes_scroll(HWND hwnd) {
    RECT rc;
    SCROLLINFO si;
    int maxs;
    GetClientRect(hwnd, &rc);
    maxs = g_codes_content_h - rc.bottom;
    if (maxs < 0) maxs = 0;
    if (g_codes_scroll > maxs) g_codes_scroll = maxs;
    if (g_codes_scroll < 0) g_codes_scroll = 0;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;
    si.nMax = g_codes_content_h > 0 ? g_codes_content_h - 1 : 0;
    si.nPage = (UINT)(rc.bottom > 0 ? rc.bottom : 1);
    si.nPos = g_codes_scroll;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

static void layout_codes(HWND hwnd, HDC hdc) {
    RECT client;
    int pad, gap, width, row_h, header, x, row_top, chip_h, count;
    GetClientRect(hwnd, &client);
    g_nhits = 0;
    g_hit_hwnd = hwnd;
    pad = S(14);
    gap = S(8);
    row_h = S(32);
    width = client.right - pad * 2;
    if (width < S(80)) width = S(80);
    {
        RECT title = rect_xywh(pad, pad, width, row_h);
        draw_text(hdc, title, "Solution codes", COL_TEXT, g_font_title,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    header = pad + row_h + S(10);
    row_top = header - g_codes_scroll;
    x = pad;
    chip_h = S(34);
    count = g_has_result ? g_result.move_count : 0;
    if (count < 0) count = 0;
    if (count > WR_MAX_MOVES) count = WR_MAX_MOVES;
    if (count == 0) {
        RECT empty = rect_xywh(pad, row_top, width, chip_h);
        draw_text(hdc, empty, "No moves", COL_MUTED, g_font, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        g_codes_content_h = header + chip_h + pad;
        return;
    }
    for (int i = 0; i < count; i++) {
        WrMove move = g_result.moves[i];
        char code[24];
        int tw, cw;
        RECT chip;
        COLORREF accent = COL_BORDER;
        snprintf(code, sizeof(code), "R%d%s>R%d", move.src, wr_code_abbrev(move.code), move.dst);
        tw = text_span(hdc, g_font_bold, code);
        cw = tw + S(18);
        if (cw < S(92)) cw = S(92);
        if (cw > width) cw = width;
        if (x > pad && x + cw > pad + width) {
            x = pad;
            row_top += chip_h + gap;
        }
        if (move.code == 1) accent = COL_SW;
        else if (move.code == 2) accent = COL_SP;
        else if (move.code == 3) accent = COL_AX;
        chip = rect_xywh(x, row_top, cw, chip_h);
        fill_round(hdc, chip, S(8), blend_color(COL_PANEL2, accent, 70), blend_color(COL_BORDER, accent, 170));
        draw_text(hdc, chip, code, COL_TEXT, g_font_bold, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        x += cw + gap;
    }
    g_codes_content_h = row_top + chip_h + g_codes_scroll + pad;
    if (g_codes_content_h < client.bottom) g_codes_content_h = client.bottom;
}

static void paint_window(HWND hwnd, int demo) {
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    HDC mem;
    HBITMAP bmp;
    HBITMAP old;
    int saved_scroll;
    hdc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) {
        EndPaint(hwnd, &ps);
        return;
    }
    ensure_fonts(hwnd);
    mem = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    old = (HBITMAP)SelectObject(mem, bmp);
    FillRect(mem, &rc, g_br_bg);
    if (demo == 2) {
        int saved = g_codes_scroll;
        g_draw = 0;
        layout_codes(hwnd, mem);
        update_codes_scroll(hwnd);
        if (g_codes_scroll != saved) {
            g_draw = 0;
            layout_codes(hwnd, mem);
        }
        FillRect(mem, &rc, g_br_bg);
        g_draw = 1;
        layout_codes(hwnd, mem);
    } else if (!demo) {
        saved_scroll = g_scroll;
        g_draw = 0;
        layout_main(hwnd, mem);
        if (g_scroll_result) {
            g_scroll = g_result_y - S(8);
            g_scroll_result = 0;
        }
        clamp_scroll(hwnd);
        if (g_scroll != saved_scroll) {
            g_draw = 0;
            layout_main(hwnd, mem);
            clamp_scroll(hwnd);
        }
        update_scrollbar(hwnd);
        FillRect(mem, &rc, g_br_bg);
        g_draw = 1;
        layout_main(hwnd, mem);
    } else {
        g_draw = 1;
        layout_demo(hwnd, mem);
    }
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static void prepare_hits(HWND hwnd, int demo) {
    HDC hdc = GetDC(hwnd);
    ensure_fonts(hwnd);
    g_draw = 0;
    if (demo == 2)
        layout_codes(hwnd, hdc);
    else if (demo)
        layout_demo(hwnd, hdc);
    else
        layout_main(hwnd, hdc);
    ReleaseDC(hwnd, hdc);
}

static void toggle_flag(int which) {
    if (which == HIT_TOP) g_topmost = !g_topmost;
    if (which == HIT_AUTO) g_auto_solve = !g_auto_solve;
    if (which == HIT_AUTODEMO) g_auto_demo = !g_auto_demo;
    if (which == HIT_AUTOCODES) g_auto_codes = !g_auto_codes;
    save_settings(g_hwnd);
    if (which == HIT_TOP) apply_topmost();
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

static void on_main_click(int id) {
    if (id >= HIT_SLOT0 && id < HIT_SLOT0 + 16) {
        g_sel_r = (id - HIT_SLOT0) / 4;
        g_sel_s = (id - HIT_SLOT0) % 4;
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    switch (id) {
    case HIT_TOP:
    case HIT_AUTO:
    case HIT_AUTODEMO:
    case HIT_AUTOCODES: toggle_flag(id); break;
    case HIT_SCAN: do_scan(g_eq_log_path); break;
    case HIT_SETTINGS: open_settings(); break;
    case HIT_AXE: place_weapon(3); break;
    case HIT_SWORD: place_weapon(1); break;
    case HIT_SPEAR: place_weapon(2); break;
    case HIT_GAP: place_weapon(4); break;
    case HIT_UNDO: undo_cell(); break;
    case HIT_CLEAR: clear_board(); break;
    case HIT_SOLVE: start_solve(0); break;
    case HIT_COPY: copy_solution(0); break;
    case HIT_DEMO: open_demo(); break;
    case HIT_CODES: open_codes(); break;
    default: break;
    }
}

static void move_selection(int key) {
    if (key == VK_RIGHT) {
        if (g_sel_s < 3)
            g_sel_s++;
        else if (g_sel_r < 3) {
            g_sel_r++;
            g_sel_s = 0;
        }
    } else if (key == VK_LEFT) {
        if (g_sel_s > 0)
            g_sel_s--;
        else if (g_sel_r > 0) {
            g_sel_r--;
            g_sel_s = 3;
        }
    } else if (key == VK_DOWN) {
        if (g_sel_s < 3) g_sel_s++;
    } else if (key == VK_UP) {
        if (g_sel_s > 0) g_sel_s--;
    }
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
}

static int on_main_key(UINT vk) {
    if (GetKeyState(VK_CONTROL) & 0x8000) return 0;
    if (GetKeyState(VK_MENU) & 0x8000) return 0;
    if (vk == g_hotkeys[HK_SOLVE]) {
        start_solve(0);
    return 1;
    }
    if (vk == g_hotkeys[HK_UNDO]) {
        undo_cell();
        return 1;
    }
    if (vk == g_hotkeys[HK_GAP]) {
        place_weapon(4);
        return 1;
    }
    if (vk == VK_RIGHT || vk == VK_LEFT || vk == VK_DOWN || vk == VK_UP) {
        move_selection((int)vk);
        return 1;
    }
    if (vk == g_hotkeys[HK_SWORD]) {
        place_weapon(1);
        return 1;
    }
    if (vk == g_hotkeys[HK_SPEAR]) {
        place_weapon(2);
        return 1;
    }
    if (vk == g_hotkeys[HK_AXE]) {
        place_weapon(3);
        return 1;
    }
    if (vk == g_hotkeys[HK_RACK1] || vk == g_hotkeys[HK_RACK2] || vk == g_hotkeys[HK_RACK3] ||
        vk == g_hotkeys[HK_RACK4]) {
        if (vk == g_hotkeys[HK_RACK1]) g_sel_r = 0;
        if (vk == g_hotkeys[HK_RACK2]) g_sel_r = 1;
        if (vk == g_hotkeys[HK_RACK3]) g_sel_r = 2;
        if (vk == g_hotkeys[HK_RACK4]) g_sel_r = 3;
        g_sel_s = 0;
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
        return 1;
    }
    return 0;
}

static void on_demo_click(int id) {
    if (id == HIT_DEMO_CLOSE) {
        if (g_demo_hwnd) DestroyWindow(g_demo_hwnd);
        return;
    }
    if (id == HIT_DEMO_RESTART) {
        demo_stop();
        demo_reset();
        if (g_demo_hwnd) InvalidateRect(g_demo_hwnd, NULL, FALSE);
        return;
    }
    if (id == HIT_DEMO_STEP) {
        demo_stop();
        demo_step();
        if (g_demo_hwnd) InvalidateRect(g_demo_hwnd, NULL, FALSE);
        return;
    }
    if (id == HIT_DEMO_PLAY) {
        if (g_demo_playing) {
            demo_stop();
            if (g_demo_hwnd) InvalidateRect(g_demo_hwnd, NULL, FALSE);
            return;
        }
        if (g_demo_index >= g_demo_count) demo_reset();
        g_demo_playing = 1;
        if (demo_step() && g_demo_hwnd) SetTimer(g_demo_hwnd, ID_TIMER_DEMO, DEMO_STEP_MS, NULL);
        if (g_demo_hwnd) InvalidateRect(g_demo_hwnd, NULL, FALSE);
    }
}

static void enable_dark_scrollbars(void) {
    HMODULE theme;
    typedef int(WINAPI * SetPreferredAppModeFn)(int);
    typedef void(WINAPI * FlushMenuThemesFn)(void);
    SetPreferredAppModeFn set_mode;
    FlushMenuThemesFn flush;
    theme = LoadLibraryW(L"uxtheme.dll");
    if (!theme) return;
    set_mode = (SetPreferredAppModeFn)GetProcAddress(theme, MAKEINTRESOURCEA(135));
    flush = (FlushMenuThemesFn)GetProcAddress(theme, MAKEINTRESOURCEA(136));
    /* 2 is ForceDark, so the scrollbar stays dark even if Windows is in light mode. */
    if (set_mode) set_mode(2);
    if (flush) flush();
}

static void dark_title(HWND hwnd) {
    BOOL on = TRUE;
    /* 20 is DWMWA_USE_IMMERSIVE_DARK_MODE. The title bar is ours, not the browser's. */
    DwmSetWindowAttribute(hwnd, 20, &on, sizeof(on));
}

static void dark_scrollbar(HWND hwnd) {
    HMODULE theme = GetModuleHandleW(L"uxtheme.dll");
    typedef BOOL(WINAPI * AllowDarkModeForWindowFn)(HWND, BOOL);
    AllowDarkModeForWindowFn allow;
    if (theme) {
        allow = (AllowDarkModeForWindowFn)GetProcAddress(theme, MAKEINTRESOURCEA(133));
        if (allow) allow(hwnd, TRUE);
    }
    SetWindowTheme(hwnd, L"DarkMode_Explorer", NULL);
}

static void set_icon(HWND hwnd) {
    if (g_icon_big) SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_icon_big);
    if (g_icon_small) SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_icon_small);
}

static void ensure_on_screen(HWND hwnd) {
    RECT wr;
    RECT inter;
    MONITORINFO mi;
    HMONITOR mon;
    int ww, wh, iw, ih, maxw, maxh;
    if (!hwnd || !GetWindowRect(hwnd, &wr)) return;
    if (IsZoomed(hwnd)) return;
    ww = wr.right - wr.left;
    wh = wr.bottom - wr.top;
    mon = MonitorFromRect(&wr, MONITOR_DEFAULTTONEAREST);
    mi.cbSize = sizeof(mi);
    if (!mon || !GetMonitorInfoA(mon, &mi)) return;
    if (IntersectRect(&inter, &wr, &mi.rcWork)) {
        iw = inter.right - inter.left;
        ih = inter.bottom - inter.top;
        if (iw >= 160 && ih >= 120) return;
    }
    maxw = mi.rcWork.right - mi.rcWork.left;
    maxh = mi.rcWork.bottom - mi.rcWork.top;
    if (ww > maxw) ww = maxw;
    if (wh > maxh) wh = maxh;
    SetWindowPos(hwnd, NULL, mi.rcWork.left + 24, mi.rcWork.top + 24, ww, wh, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void show_tool(HWND hwnd, const ToolRect *place) {
    if (place && place->placed && place->maximized)
        ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    else
        ShowWindow(hwnd, SW_SHOW);
    ensure_on_screen(hwnd);
}

static LRESULT CALLBACK DemoProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_demo_hwnd = hwnd;
        dark_title(hwnd);
        set_icon(hwnd);
        if (g_topmost) push_zorder();
            return 0;
    case WM_PAINT:
        paint_window(hwnd, 1);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        prepare_hits(hwnd, 1);
        on_demo_click(hit_test(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        return 0;
    case WM_MOUSEMOVE: {
        int id;
        prepare_hits(hwnd, 1);
        id = hit_test(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (id != g_hover || g_hover_hwnd != hwnd) {
            g_hover = id;
            g_hover_hwnd = hwnd;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        {
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            tme.dwHoverTime = 0;
            TrackMouseEvent(&tme);
        }
            return 0;
        }
    case WM_MOUSELEAVE:
        g_hover = 0;
        g_hover_hwnd = NULL;
        InvalidateRect(hwnd, NULL, FALSE);
            return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (g_hit_hwnd == hwnd && hit_test(pt.x, pt.y)) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_TIMER:
        if (wParam == ID_TIMER_DEMO) {
            if (!g_demo_playing || !demo_step()) demo_stop();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_SIZE: InvalidateRect(hwnd, NULL, FALSE); return 0;
    case WM_EXITSIZEMOVE: store_tool_placement("demo", hwnd); return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lParam;
        mm->ptMinTrackSize.x = 640;
        mm->ptMinTrackSize.y = 480;
                return 0;
            }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        demo_stop();
        store_tool_placement("demo", hwnd);
        if (g_demo_hwnd == hwnd) g_demo_hwnd = NULL;
                return 0;
    default: break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK CodesProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_codes_hwnd = hwnd;
        dark_title(hwnd);
        dark_scrollbar(hwnd);
        set_icon(hwnd);
        if (g_topmost) push_zorder();
        return 0;
    case WM_PAINT:
        paint_window(hwnd, 2);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_codes_scroll -= (delta / WHEEL_DELTA) * S(48);
        update_codes_scroll(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_VSCROLL: {
        RECT rc;
        int pos = g_codes_scroll;
        int line = S(40);
        int page;
        GetClientRect(hwnd, &rc);
        page = rc.bottom - line;
        if (page < line) page = line;
        switch (LOWORD(wParam)) {
        case SB_TOP: pos = 0; break;
        case SB_BOTTOM: pos = g_codes_content_h; break;
        case SB_LINEUP: pos -= line; break;
        case SB_LINEDOWN: pos += line; break;
        case SB_PAGEUP: pos -= page; break;
        case SB_PAGEDOWN: pos += page; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si;
            memset(&si, 0, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;
            GetScrollInfo(hwnd, SB_VERT, &si);
            pos = si.nTrackPos;
            break;
        }
        default: break;
        }
        g_codes_scroll = pos;
        update_codes_scroll(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_EXITSIZEMOVE: store_tool_placement("codes", hwnd); return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lParam;
        mm->ptMinTrackSize.x = 180;
        mm->ptMinTrackSize.y = 120;
        return 0;
    }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        store_tool_placement("codes", hwnd);
        if (g_codes_hwnd == hwnd) g_codes_hwnd = NULL;
        return 0;
    default: break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static HWND make_ctl(HWND parent, const char *cls, const char *text, DWORD style, DWORD ex, int id) {
    HWND h = CreateWindowExA(ex, cls, text ? text : "", WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, parent,
                             (HMENU)(INT_PTR)id, g_inst, NULL);
    if (h && g_font) SendMessageA(h, WM_SETFONT, (WPARAM)g_font, TRUE);
    return h;
}

static void move_ctl(HWND parent, int id, int x, int y, int w, int ht) {
    HWND ctl = GetDlgItem(parent, id);
    if (ctl) MoveWindow(ctl, x, y, w, ht, TRUE);
}

static void refresh_hk_buttons(void) {
    if (!g_settings_hwnd) return;
    for (int i = 0; i < HK_COUNT; i++) {
        char name[32];
        if (g_hk_capture == i)
            copy_str(name, sizeof(name), "Press a key");
        else
            vk_to_name(g_hk_draft[i], name, sizeof(name));
        SetWindowTextA(GetDlgItem(g_settings_hwnd, IDC_HK0 + i), name);
    }
}

static void update_example(HWND hwnd) {
    LmParseConfig cfg;
    char line[256];
    char tags[4][LM_TAG_CAP];
    char chars[4][8];
    int ok = 1;
    if (!hwnd) return;
    memset(&cfg, 0, sizeof(cfg));
    for (int i = 0; i < 4; i++) {
        GetWindowTextA(GetDlgItem(hwnd, IDC_RACK0 + i), tags[i], LM_TAG_CAP);
        trim_inplace(tags[i]);
        copy_str(cfg.rack_tag[i], LM_TAG_CAP, tags[i]);
        if (!tags[i][0]) ok = 0;
    }
    GetWindowTextA(GetDlgItem(hwnd, IDC_AXE), chars[0], 8);
    GetWindowTextA(GetDlgItem(hwnd, IDC_SWORD), chars[1], 8);
    GetWindowTextA(GetDlgItem(hwnd, IDC_SPEAR), chars[2], 8);
    GetWindowTextA(GetDlgItem(hwnd, IDC_BLANK), chars[3], 8);
    if (!chars[0][0] || !chars[1][0] || !chars[2][0] || !chars[3][0]) ok = 0;
    cfg.axe = chars[0][0];
    cfg.sword = chars[1][0];
    cfg.spear = chars[2][0];
    cfg.blank = chars[3][0];
    line[0] = 0;
    if (ok) {
        copy_str(line, sizeof(line), "Example: ");
        for (int i = 0; i < 4; i++) {
            char ex[64];
            lm_format_example(&cfg, i, ex, sizeof(ex));
            if (i) append_text(line, sizeof(line), "    ");
            append_text(line, sizeof(line), ex);
        }
    }
    SetWindowTextA(GetDlgItem(hwnd, IDC_EXAMPLE), line);
}

static void fill_parse_edits(HWND hwnd, const LmParseConfig *cfg) {
    char one[2];
    for (int i = 0; i < 4; i++) SetWindowTextA(GetDlgItem(hwnd, IDC_RACK0 + i), cfg->rack_tag[i]);
    one[1] = 0;
    one[0] = cfg->axe;
    SetWindowTextA(GetDlgItem(hwnd, IDC_AXE), one);
    one[0] = cfg->sword;
    SetWindowTextA(GetDlgItem(hwnd, IDC_SWORD), one);
    one[0] = cfg->spear;
    SetWindowTextA(GetDlgItem(hwnd, IDC_SPEAR), one);
    one[0] = cfg->blank;
    SetWindowTextA(GetDlgItem(hwnd, IDC_BLANK), one);
    update_example(hwnd);
}

static void refresh_settings_status(HWND hwnd) {
    char status[128];
    EnterCriticalSection(&g_lock);
    copy_str(status, sizeof(status), g_eq_status);
    LeaveCriticalSection(&g_lock);
    SetWindowTextA(GetDlgItem(hwnd, IDC_STATUS), status);
}

static void fill_settings_form(HWND hwnd) {
    SendMessageA(GetDlgItem(hwnd, IDC_LOGEN), BM_SETCHECK, g_eq_log_enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextA(GetDlgItem(hwnd, IDC_PATH), g_eq_log_path);
    fill_parse_edits(hwnd, &g_parse_cfg);
    memcpy(g_hk_draft, g_hotkeys, sizeof(g_hk_draft));
    g_hk_capture = -1;
    refresh_hk_buttons();
    SetWindowTextA(GetDlgItem(hwnd, IDC_ERR), "");
    refresh_settings_status(hwnd);
}

static void show_settings_error(HWND hwnd, const char *msg) {
    SetWindowTextA(GetDlgItem(hwnd, IDC_ERR), msg ? msg : "");
}

static int read_one_char(HWND hwnd, int id, char *out, char *err, size_t err_cap) {
    char buf[8];
    GetWindowTextA(GetDlgItem(hwnd, id), buf, 8);
    if (!buf[0] || buf[1]) {
        snprintf(err, err_cap, "Weapon characters must be a single character");
        return 0;
    }
    *out = buf[0];
    return 1;
}

static int commit_settings(HWND hwnd) {
    char err[256];
    char path[MAX_PATH];
    LmParseConfig parse;
    int enabled;
    int path_changed, enabled_changed, parse_changed;
    err[0] = 0;
    enabled = SendMessageA(GetDlgItem(hwnd, IDC_LOGEN), BM_GETCHECK, 0, 0) == BST_CHECKED;
    GetWindowTextA(GetDlgItem(hwnd, IDC_PATH), path, MAX_PATH);
    trim_inplace(path);
    memset(&parse, 0, sizeof(parse));
    for (int i = 0; i < 4; i++) {
        GetWindowTextA(GetDlgItem(hwnd, IDC_RACK0 + i), parse.rack_tag[i], LM_TAG_CAP);
        trim_inplace(parse.rack_tag[i]);
    }
    if (!read_one_char(hwnd, IDC_AXE, &parse.axe, err, sizeof(err)) ||
        !read_one_char(hwnd, IDC_SWORD, &parse.sword, err, sizeof(err)) ||
        !read_one_char(hwnd, IDC_SPEAR, &parse.spear, err, sizeof(err)) ||
        !read_one_char(hwnd, IDC_BLANK, &parse.blank, err, sizeof(err))) {
        show_settings_error(hwnd, err);
        return 0;
    }
    if (!lm_config_validate(&parse, err, sizeof(err))) {
        show_settings_error(hwnd, err[0] ? err : "Could not save settings");
        return 0;
    }
    for (int i = 0; i < HK_COUNT; i++) {
        if (!g_hk_draft[i]) {
            show_settings_error(hwnd, "Missing shortcut");
            return 0;
        }
    }
    path_changed = strcmp(path, g_eq_log_path) != 0;
    enabled_changed = enabled != g_eq_log_enabled;
    parse_changed = memcmp(&parse, &g_parse_cfg, sizeof(parse)) != 0;
    EnterCriticalSection(&g_lock);
    g_eq_log_enabled = enabled;
    copy_str(g_eq_log_path, sizeof(g_eq_log_path), path);
    g_parse_cfg = parse;
    memcpy(g_hotkeys, g_hk_draft, sizeof(g_hotkeys));
    LeaveCriticalSection(&g_lock);
    save_settings(g_hwnd);
    if (path_changed || enabled_changed || parse_changed) eq_log_start_monitor();
    log_write("settings", "updated");
    pull_matches();
    if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
    return 1;
}

static void browse_log(HWND owner) {
    char file[MAX_PATH];
    OPENFILENAMEA ofn;
    file[0] = 0;
    GetWindowTextA(GetDlgItem(owner, IDC_PATH), file, MAX_PATH);
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = "EverQuest logs (eqlog_*.txt)\0eqlog_*.txt\0Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "EverQuest log file";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) SetWindowTextA(GetDlgItem(owner, IDC_PATH), file);
}

static const char *TIP_PARSE =
    "Rack tags and the characters for axe, sword, spear, and a blank. The example is a line the log monitor will match.";
static const char *TIP_SHORTCUTS = "Click a key, then press the new shortcut. Esc cancels.";

static int text_px(HWND hwnd, HFONT font, const char *text) {
    HDC hdc;
    SIZE sz;
    HFONT old;
    hdc = GetDC(hwnd);
    sz.cx = 0;
    sz.cy = 0;
    old = (HFONT)SelectObject(hdc, font ? font : g_font);
    GetTextExtentPoint32A(hdc, text ? text : "", (int)strlen(text ? text : ""), &sz);
    SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
    return sz.cx;
}

static void paint_settings_control(HWND ctl, HDC hdc) {
    RECT rc;
    RECT box;
    char text[160];
    int id;
    int saved;
    int hot;
    LONG style;
    int kind;
    GetClientRect(ctl, &rc);
    id = GetDlgCtrlID(ctl);
    hot = ctl == g_settings_hover;
    saved = g_draw;
    g_draw = 1;
    FillRect(hdc, &rc, g_br_bg);
    GetWindowTextA(ctl, text, 160);
    if (id == IDC_TIP_PARSE || id == IDC_TIP_HK) {
        int d = rc.bottom - rc.top;
        if (d > rc.right - rc.left) d = rc.right - rc.left;
        box.left = rc.left + (rc.right - rc.left - d) / 2;
        box.top = rc.top + (rc.bottom - rc.top - d) / 2;
        box.right = box.left + d;
        box.bottom = box.top + d;
        fill_round(hdc, box, d, hot ? blend_color(COL_PANEL2, COL_FOCUS, 90) : COL_PANEL2,
                   hot ? COL_FOCUS : COL_BORDER);
        draw_text(hdc, box, "?", hot ? COL_FOCUS : COL_TEXT, g_font_bold, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        g_draw = saved;
        return;
    }
    style = GetWindowLongA(ctl, GWL_STYLE);
    kind = style & BS_TYPEMASK;
    if (kind == BS_AUTOCHECKBOX) {
        int on = SendMessageA(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED;
        int side = S(16);
        int cy = rc.top + ((rc.bottom - rc.top) - side) / 2;
        RECT mark;
        RECT label = rc;
        mark = rect_xywh(rc.left, cy, side, side);
        fill_round(hdc, mark, S(4), on ? COL_OK : COL_PANEL2, on ? COL_OK : (hot ? COL_FOCUS : COL_BORDER));
        if (on) draw_check_mark(hdc, mark);
        label.left = mark.right + S(8);
        draw_text(hdc, label, text, COL_TEXT, g_font, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        int primary = id == IDC_SAVE;
        int capture = id >= IDC_HK0 && id < IDC_HK0 + HK_COUNT && g_hk_capture == id - IDC_HK0;
        COLORREF fill = primary ? RGB(58, 99, 224) : COL_PANEL;
        COLORREF border = primary ? RGB(107, 143, 255) : COL_BORDER;
        RECT label = rc;
        if (capture) border = COL_FOCUS;
        if (hot) fill = blend_color(fill, RGB(255, 255, 255), 22);
        fill_round(hdc, rc, S(8), fill, border);
        InflateRect(&label, -S(4), 0);
        draw_text(hdc, label, text, COL_TEXT, g_font_bold, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    g_draw = saved;
}

static void note_settings_hover(HWND ctl) {
    if (g_settings_hover == ctl) return;
    if (g_settings_hover && IsWindow(g_settings_hover)) InvalidateRect(g_settings_hover, NULL, FALSE);
    g_settings_hover = ctl;
    if (ctl) InvalidateRect(ctl, NULL, FALSE);
}

static LRESULT CALLBACK settings_button_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        paint_settings_control(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_MOUSEMOVE) {
        TRACKMOUSEEVENT tme;
        note_settings_hover(hwnd);
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        tme.dwHoverTime = 0;
        TrackMouseEvent(&tme);
    } else if (msg == WM_MOUSELEAVE) {
        if (g_settings_hover == hwnd) note_settings_hover(NULL);
    } else if (msg == WM_SETCURSOR) {
        SetCursor(LoadCursor(NULL, IDC_HAND));
        return TRUE;
    } else if (msg == WM_NCDESTROY) {
        if (g_settings_hover == hwnd) g_settings_hover = NULL;
    }
    return CallWindowProcA(g_button_proc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK settings_edit_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCPAINT) {
        HDC hdc;
        RECT rc;
        HBRUSH brush;
        CallWindowProcA(g_edit_proc, hwnd, msg, wParam, lParam);
        hdc = GetWindowDC(hwnd);
        GetWindowRect(hwnd, &rc);
        OffsetRect(&rc, -rc.left, -rc.top);
        brush = CreateSolidBrush(GetFocus() == hwnd ? COL_FOCUS : COL_BORDER);
        FrameRect(hdc, &rc, brush);
        DeleteObject(brush);
        ReleaseDC(hwnd, hdc);
        return 0;
    }
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {
        LRESULT result = CallWindowProcA(g_edit_proc, hwnd, msg, wParam, lParam);
        RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
        return result;
    }
    return CallWindowProcA(g_edit_proc, hwnd, msg, wParam, lParam);
}

static void subclass_settings_child(HWND child) {
    char cls[32];
    GetClassNameA(child, cls, 32);
    if (_stricmp(cls, "Button") == 0) {
        SetWindowTheme(child, L"", L"");
        if (!g_button_proc) g_button_proc = (WNDPROC)GetWindowLongPtrA(child, GWLP_WNDPROC);
        SetWindowLongPtrA(child, GWLP_WNDPROC, (LONG_PTR)settings_button_proc);
    } else if (_stricmp(cls, "Edit") == 0) {
        SetWindowTheme(child, L"", L"");
        if (!g_edit_proc) g_edit_proc = (WNDPROC)GetWindowLongPtrA(child, GWLP_WNDPROC);
        SetWindowLongPtrA(child, GWLP_WNDPROC, (LONG_PTR)settings_edit_proc);
    }
}

static void add_settings_tip(HWND parent, int id, const char *text) {
    TOOLINFOA ti;
    HWND ctl = GetDlgItem(parent, id);
    if (!g_settings_tip || !ctl || !text) return;
    memset(&ti, 0, sizeof(ti));
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS | TTF_TRANSPARENT;
    ti.hwnd = parent;
    ti.uId = (UINT_PTR)ctl;
    ti.lpszText = (LPSTR)text;
    SendMessageA(g_settings_tip, TTM_ADDTOOLA, 0, (LPARAM)&ti);
}

static void apply_settings_fonts(void) {
    static const int bold_ids[] = {IDC_LBL_LOG, IDC_LBL_PARSE, IDC_LBL_HK};
    HWND child;
    int i;
    if (!g_settings_hwnd || !g_font) return;
    child = GetWindow(g_settings_hwnd, GW_CHILD);
    while (child) {
        SendMessageA(child, WM_SETFONT, (WPARAM)g_font, TRUE);
        child = GetWindow(child, GW_HWNDNEXT);
    }
    for (i = 0; i < 3; i++) {
        HWND heading = GetDlgItem(g_settings_hwnd, bold_ids[i]);
        if (heading && g_font_bold) SendMessageA(heading, WM_SETFONT, (WPARAM)g_font_bold, TRUE);
    }
    if (g_settings_tip && g_font) SendMessageA(g_settings_tip, WM_SETFONT, (WPARAM)g_font, TRUE);
    InvalidateRect(g_settings_hwnd, NULL, TRUE);
}

static void layout_settings(HWND hwnd) {
    RECT rc;
    int pad, gap, row, gutter, shortcuts_w, left_w, x, y, field_w, label_h, rx;
    int path_w, browse_w, path_gap, col_w, btn_w;
    int left_bottom, hk_bottom, by, natural, content_right;
    static const char *rack_labels[4] = {"Rack 1", "Rack 2", "Rack 3", "Rack 4"};
    static const char *char_labels[4] = {"Axe", "Sword", "Spear", "Blank"};
    GetClientRect(hwnd, &rc);
    ensure_fonts(hwnd);
    pad = S(16);
    gap = S(10);
    row = S(26);
    gutter = S(28);
    label_h = S(18);
    x = pad;
    field_w = text_px(hwnd, g_font, "WWW") + S(16);
    if (field_w < S(36)) field_w = S(36);
    col_w = field_w;
    for (int i = 0; i < 4; i++) {
        int lw = text_px(hwnd, g_font, rack_labels[i]);
        if (lw > col_w) col_w = lw;
        lw = text_px(hwnd, g_font, char_labels[i]);
        if (lw > col_w) col_w = lw;
    }
    browse_w = S(96);
    path_gap = S(8);
    path_w = text_px(hwnd, g_font, "D:\\Everquest\\Logs\\eqlog_Character_server.txt") + S(24);
    if (path_w < S(180)) path_w = S(180);
    left_w = path_w + path_gap + browse_w;
    {
        int ids = col_w * 4 + gap * 3;
        int scan = S(210) + S(8) + S(120);
        int example = text_px(hwnd, g_font, "Example: R112`3    R212`3    R312`3    R412`3");
        if (ids > left_w) left_w = ids;
        if (scan > left_w) left_w = scan;
        if (example > left_w) left_w = example;
    }
    btn_w = S(108);
    shortcuts_w = text_px(hwnd, g_font, "Rack 1") + S(16) + btn_w;
    if (shortcuts_w < btn_w + S(48)) shortcuts_w = btn_w + S(48);
    rx = x + left_w + gutter;
    content_right = rx + shortcuts_w;
    y = pad;
    move_ctl(hwnd, IDC_ERR, x, y, content_right - x, S(18));
    y += S(20);
    move_ctl(hwnd, IDC_LBL_LOG, x, y, left_w, label_h);
    y += label_h + S(2);
    move_ctl(hwnd, IDC_PATH, x, y, path_w, row);
    move_ctl(hwnd, IDC_BROWSE, x + path_w + path_gap, y, browse_w, row);
    y += row + gap;
    move_ctl(hwnd, IDC_LOGEN, x, y, S(210), row);
    move_ctl(hwnd, IDC_SCAN, x + S(218), y, S(120), row);
    y += row + S(2);
    move_ctl(hwnd, IDC_STATUS, x, y, left_w, label_h);
    y += label_h + gap;
    {
        int title_w = text_px(hwnd, g_font_bold, "Log search identifiers");
        int token = S(18);
        move_ctl(hwnd, IDC_LBL_PARSE, x, y, title_w + S(4), label_h);
        move_ctl(hwnd, IDC_TIP_PARSE, x + title_w + S(8), y + (label_h - token) / 2, token, token);
    }
    y += label_h + S(4);
    {
        int fx = x;
        int row_y = y;
        for (int i = 0; i < 4; i++) {
            move_ctl(hwnd, IDC_LBL_RACK0 + i, fx, row_y, col_w, label_h);
            move_ctl(hwnd, IDC_RACK0 + i, fx, row_y + label_h + S(2), field_w, row);
            fx += col_w + gap;
        }
        row_y += label_h + S(2) + row + gap;
        fx = x;
        for (int i = 0; i < 4; i++) {
            move_ctl(hwnd, IDC_LBL_CHAR0 + i, fx, row_y, col_w, label_h);
            move_ctl(hwnd, IDC_AXE + i, fx, row_y + label_h + S(2), field_w, row);
            fx += col_w + gap;
        }
        y = row_y + label_h + S(2) + row + S(6);
    }
    move_ctl(hwnd, IDC_EXAMPLE, x, y, left_w, S(20));
    left_bottom = y + S(20);

    y = pad + S(20);
    {
        int title_w = text_px(hwnd, g_font_bold, "App shortcuts");
        int token = S(18);
        move_ctl(hwnd, IDC_LBL_HK, rx, y, title_w + S(4), label_h);
        move_ctl(hwnd, IDC_TIP_HK, rx + title_w + S(8), y + (label_h - token) / 2, token, token);
    }
    y += label_h + gap;
    for (int i = 0; i < HK_COUNT; i++) {
        move_ctl(hwnd, IDC_LBL_HK0 + i, rx, y, shortcuts_w - btn_w - S(8), row);
        move_ctl(hwnd, IDC_HK0 + i, rx + shortcuts_w - btn_w, y, btn_w, row);
        y += row + S(4);
    }
    hk_bottom = y;
    natural = (left_bottom > hk_bottom ? left_bottom : hk_bottom) + gap;
    by = natural;
    {
        int limit = rc.bottom - pad - row;
        if (by > limit) by = limit;
    }
    move_ctl(hwnd, IDC_RESET_PARSE, pad, by, S(160), row);
    move_ctl(hwnd, IDC_RESET_HK, pad + S(168), by, S(150), row);
    move_ctl(hwnd, IDC_SAVE, content_right - S(150), by, S(150), row);
    {
        RECT wr;
        int extra_w, extra_h, need_w, need_h;
        need_w = content_right + pad;
        need_h = natural + row + pad;
        GetWindowRect(hwnd, &wr);
        extra_w = (wr.right - wr.left) - rc.right;
        extra_h = (wr.bottom - wr.top) - rc.bottom;
        if (extra_w < 0) extra_w = 0;
        if (extra_h < 0) extra_h = 0;
        g_settings_need_w = need_w + extra_w;
        g_settings_need_h = need_h + extra_h;
        if (g_settings_fit) {
            g_settings_fit = 0;
            if (rc.right != need_w || rc.bottom != need_h)
                SetWindowPos(hwnd, NULL, 0, 0, g_settings_need_w, g_settings_need_h,
                             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

static void create_settings_controls(HWND hwnd) {
    const char *chars[4] = {"Axe", "Sword", "Spear", "Blank"};
    HWND edit;
    make_ctl(hwnd, "STATIC", "", SS_LEFT | SS_NOPREFIX, 0, IDC_ERR);
    make_ctl(hwnd, "STATIC", "EQ log file", SS_LEFT | SS_NOPREFIX, 0, IDC_LBL_LOG);
    edit = make_ctl(hwnd, "EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, 0, IDC_PATH);
    SendMessageA(edit, EM_SETLIMITTEXT, MAX_PATH - 1, 0);
    make_ctl(hwnd, "BUTTON", "Browse...", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BROWSE);
    make_ctl(hwnd, "BUTTON", "Enable log monitor", BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_LOGEN);
    make_ctl(hwnd, "BUTTON", "Scan recent", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_SCAN);
    make_ctl(hwnd, "STATIC", "", SS_LEFT | SS_NOPREFIX, 0, IDC_STATUS);
    make_ctl(hwnd, "STATIC", "Log search identifiers", SS_LEFT | SS_NOPREFIX | SS_NOTIFY, 0, IDC_LBL_PARSE);
    make_ctl(hwnd, "BUTTON", "?", BS_PUSHBUTTON, 0, IDC_TIP_PARSE);
    for (int i = 0; i < 4; i++) {
        char label[16];
        snprintf(label, sizeof(label), "Rack %d", i + 1);
        make_ctl(hwnd, "STATIC", label, SS_LEFT | SS_NOPREFIX, 0, IDC_LBL_RACK0 + i);
        edit = make_ctl(hwnd, "EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, 0, IDC_RACK0 + i);
        SendMessageA(edit, EM_SETLIMITTEXT, 12, 0);
    }
    for (int i = 0; i < 4; i++) {
        make_ctl(hwnd, "STATIC", chars[i], SS_LEFT | SS_NOPREFIX, 0, IDC_LBL_CHAR0 + i);
        edit = make_ctl(hwnd, "EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL | ES_CENTER | WS_BORDER, 0, IDC_AXE + i);
        SendMessageA(edit, EM_SETLIMITTEXT, 1, 0);
    }
    make_ctl(hwnd, "STATIC", "", SS_LEFT | SS_NOPREFIX, 0, IDC_EXAMPLE);
    make_ctl(hwnd, "STATIC", "App shortcuts", SS_LEFT | SS_NOPREFIX | SS_NOTIFY, 0, IDC_LBL_HK);
    make_ctl(hwnd, "BUTTON", "?", BS_PUSHBUTTON, 0, IDC_TIP_HK);
    for (int i = 0; i < HK_COUNT; i++) {
        make_ctl(hwnd, "STATIC", HK_LABELS[i], SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE, 0, IDC_LBL_HK0 + i);
        make_ctl(hwnd, "BUTTON", "?", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_HK0 + i);
    }
    make_ctl(hwnd, "BUTTON", "Reset identifiers", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_RESET_PARSE);
    make_ctl(hwnd, "BUTTON", "Reset shortcuts", BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_RESET_HK);
    make_ctl(hwnd, "BUTTON", "Save and Close", BS_DEFPUSHBUTTON | WS_TABSTOP, 0, IDC_SAVE);
    {
        HWND child = GetWindow(hwnd, GW_CHILD);
        while (child) {
            HWND next = GetWindow(child, GW_HWNDNEXT);
            subclass_settings_child(child);
            child = next;
        }
    }
    g_settings_tip = CreateWindowExA(WS_EX_TOPMOST, TOOLTIPS_CLASSA, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, NULL, g_inst,
                                     NULL);
    if (g_settings_tip) {
        SetWindowTheme(g_settings_tip, L"", L"");
        SendMessageA(g_settings_tip, TTM_SETTIPBKCOLOR, COL_PANEL2, 0);
        SendMessageA(g_settings_tip, TTM_SETTIPTEXTCOLOR, COL_TEXT, 0);
        SendMessageA(g_settings_tip, TTM_SETMAXTIPWIDTH, 0, S(280));
        if (g_font) SendMessageA(g_settings_tip, WM_SETFONT, (WPARAM)g_font, TRUE);
        add_settings_tip(hwnd, IDC_LBL_PARSE, TIP_PARSE);
        add_settings_tip(hwnd, IDC_TIP_PARSE, TIP_PARSE);
        add_settings_tip(hwnd, IDC_LBL_HK, TIP_SHORTCUTS);
        add_settings_tip(hwnd, IDC_TIP_HK, TIP_SHORTCUTS);
    }
}

static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_settings_hwnd = hwnd;
        dark_title(hwnd);
        set_icon(hwnd);
        create_settings_controls(hwnd);
        if (g_topmost) push_zorder();
        SetTimer(hwnd, ID_TIMER_STATUS, 1000, NULL);
        return 0;
    case WM_SIZE: layout_settings(hwnd); return 0;
    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wParam, &rc, g_br_bg);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wParam;
        int id = GetDlgCtrlID((HWND)lParam);
        int muted = id == IDC_EXAMPLE || (id >= IDC_LBL_RACK0 && id < IDC_LBL_RACK0 + 4) ||
                    (id >= IDC_LBL_CHAR0 && id < IDC_LBL_CHAR0 + 4);
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, msg == WM_CTLCOLOREDIT ? COL_PANEL2 : COL_BG);
        if (id == IDC_ERR)
            SetTextColor(dc, COL_BAD);
        else if (id == IDC_STATUS)
            SetTextColor(dc, COL_FOCUS);
        else if (muted)
            SetTextColor(dc, COL_MUTED);
        else
            SetTextColor(dc, COL_TEXT);
        return (LRESULT)(msg == WM_CTLCOLOREDIT ? g_br_edit : g_br_bg);
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if (code == EN_CHANGE && ((id >= IDC_RACK0 && id < IDC_RACK0 + 4) || (id >= IDC_AXE && id <= IDC_BLANK))) {
            update_example(hwnd);
            return 0;
        }
        if (code == EN_SETFOCUS) {
            g_hk_capture = -1;
            refresh_hk_buttons();
            return 0;
        }
        if (code == BN_CLICKED && id >= IDC_HK0 && id < IDC_HK0 + HK_COUNT) {
            g_hk_capture = id - IDC_HK0;
            refresh_hk_buttons();
            SetFocus(hwnd);
            return 0;
        }
        if (code == BN_CLICKED && id == IDC_BROWSE) {
            browse_log(hwnd);
            return 0;
        }
        if (code == BN_CLICKED && id == IDC_SCAN) {
            char path[MAX_PATH];
            char status[128];
            GetWindowTextA(GetDlgItem(hwnd, IDC_PATH), path, MAX_PATH);
            trim_inplace(path);
            show_settings_error(hwnd, "");
            if (!scan_file(path, status, sizeof(status)))
                show_settings_error(hwnd, status);
            refresh_settings_status(hwnd);
            pull_matches();
            return 0;
        }
        if (code == BN_CLICKED && id == IDC_RESET_PARSE) {
            LmParseConfig defaults;
            lm_config_defaults(&defaults);
            fill_parse_edits(hwnd, &defaults);
            return 0;
        }
        if (code == BN_CLICKED && id == IDC_RESET_HK) {
            hotkeys_set_defaults(g_hk_draft);
            g_hk_capture = -1;
            refresh_hk_buttons();
            return 0;
        }
        if (code == BN_CLICKED && id == IDC_SAVE) {
            if (commit_settings(hwnd)) DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (g_hk_capture >= 0) {
            UINT vk = (UINT)wParam;
            if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN) return 0;
            if (vk == VK_ESCAPE) {
                g_hk_capture = -1;
                refresh_hk_buttons();
                return 0;
            }
            g_hk_draft[g_hk_capture] = vk;
            g_hk_capture = -1;
            refresh_hk_buttons();
            return 0;
        }
        break;
    case WM_TIMER:
        if (wParam == ID_TIMER_STATUS) refresh_settings_status(hwnd);
        return 0;
    case WM_EXITSIZEMOVE: store_tool_placement("settings", hwnd); return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lParam;
        mm->ptMinTrackSize.x = g_settings_need_w > 0 ? g_settings_need_w : 640;
        mm->ptMinTrackSize.y = g_settings_need_h > 0 ? g_settings_need_h : 420;
        return 0;
    }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_STATUS);
        store_tool_placement("settings", hwnd);
        g_hk_capture = -1;
        g_settings_hover = NULL;
        if (g_settings_tip) {
            DestroyWindow(g_settings_tip);
            g_settings_tip = NULL;
        }
        if (g_settings_hwnd == hwnd) g_settings_hwnd = NULL;
        return 0;
    default: break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static int settings_key(const MSG *msg) {
    HWND focus;
    char cls[16];
    if (!g_settings_hwnd || !IsWindow(g_settings_hwnd)) return 0;
    if (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN) return 0;
    focus = GetFocus();
    if (focus != g_settings_hwnd && !(focus && IsChild(g_settings_hwnd, focus))) return 0;
    if (g_hk_capture >= 0) return 0;
    if (msg->wParam == VK_TAB) {
        BOOL back = (GetKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;
        HWND next = GetNextDlgTabItem(g_settings_hwnd, focus == g_settings_hwnd ? NULL : focus, back);
        if (next) SetFocus(next);
        return 1;
    }
    if (msg->wParam == VK_ESCAPE) {
        DestroyWindow(g_settings_hwnd);
        return 1;
    }
    if (msg->wParam == VK_RETURN) {
        cls[0] = 0;
        if (focus) GetClassNameA(focus, cls, 16);
        if (_stricmp(cls, "Button") == 0) return 0;
        SendMessageA(g_settings_hwnd, WM_COMMAND, MAKEWPARAM(IDC_SAVE, BN_CLICKED),
                     (LPARAM)GetDlgItem(g_settings_hwnd, IDC_SAVE));
        return 1;
    }
    return 0;
}

static void open_tool(const char *cls, const char *title, const char *section, int def_w, int def_h, HWND *slot) {
    ToolRect *place = tool_rect(section);
    DWORD ex = g_topmost ? WS_EX_TOPMOST : 0;
    DWORD style = WS_OVERLAPPEDWINDOW;
    int x = place->placed ? place->x : CW_USEDEFAULT;
    int y = place->placed ? place->y : CW_USEDEFAULT;
    int w = place->w > 0 ? place->w : def_w;
    int h = place->h > 0 ? place->h : def_h;
    HWND hwnd;
    if (cls && strcmp(cls, "WRcCodes") == 0) style |= WS_VSCROLL;
    if (*slot && IsWindow(*slot)) {
        ShowWindow(*slot, SW_SHOW);
        SetForegroundWindow(*slot);
        return;
    }
    hwnd = CreateWindowExA(ex, cls, title, style, x, y, w, h, g_hwnd, NULL, g_inst, NULL);
    if (!hwnd) return;
    *slot = hwnd;
    show_tool(hwnd, place);
}

static void open_codes(void) {
    if (!g_has_result || g_result.move_count <= 0) return;
    g_codes_scroll = 0;
    open_tool("WRcCodes", "Solution codes", "codes", 720, 180, &g_codes_hwnd);
    if (g_codes_hwnd) InvalidateRect(g_codes_hwnd, NULL, FALSE);
}

static void open_demo(void) {
    char detail[64];
    if (g_demo_count <= 0) return;
    demo_stop();
    demo_reset();
    open_tool("WRcDemo", "Solution demo", "demo", 1000, 760, &g_demo_hwnd);
    if (g_demo_hwnd) InvalidateRect(g_demo_hwnd, NULL, FALSE);
    snprintf(detail, sizeof(detail), "opened; moves=%d", g_demo_count);
    log_write("demo", detail);
}

static void open_settings(void) {
    if (g_settings_hwnd && IsWindow(g_settings_hwnd)) {
        ShowWindow(g_settings_hwnd, SW_SHOW);
        SetForegroundWindow(g_settings_hwnd);
        return;
    }
    g_settings_fit = 1;
    open_tool("WRcSettings", "Settings", "settings", 760, 520, &g_settings_hwnd);
    if (g_settings_hwnd) fill_settings_form(g_settings_hwnd);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        dark_title(hwnd);
        dark_scrollbar(hwnd);
        set_icon(hwnd);
        ensure_fonts(hwnd);
    if (g_topmost) SetTimer(hwnd, ID_TIMER_TOPMOST, TOPMOST_MS, NULL);
    eq_log_start_monitor();
        pull_matches();
        return 0;
    case WM_PAINT:
        paint_window(hwnd, 0);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        prepare_hits(hwnd, 0);
        on_main_click(hit_test(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        return 0;
    case WM_MOUSEMOVE: {
        int id;
        prepare_hits(hwnd, 0);
        id = hit_test(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (id != g_hover || g_hover_hwnd != hwnd) {
            g_hover = id;
            g_hover_hwnd = hwnd;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        {
            TRACKMOUSEEVENT tme;
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            tme.dwHoverTime = 0;
            TrackMouseEvent(&tme);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_hover = 0;
        g_hover_hwnd = NULL;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (g_hit_hwnd == hwnd && hit_test(pt.x, pt.y)) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
        }
        break;
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_scroll -= (delta / WHEEL_DELTA) * S(56);
        clamp_scroll(hwnd);
        update_scrollbar(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_VSCROLL: {
        RECT rc;
        int pos = g_scroll;
        int line = S(40);
        int page;
        GetClientRect(hwnd, &rc);
        page = rc.bottom - line;
        if (page < line) page = line;
        switch (LOWORD(wParam)) {
        case SB_TOP: pos = 0; break;
        case SB_BOTTOM: pos = g_content_h; break;
        case SB_LINEUP: pos -= line; break;
        case SB_LINEDOWN: pos += line; break;
        case SB_PAGEUP: pos -= page; break;
        case SB_PAGEDOWN: pos += page; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si;
            memset(&si, 0, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;
            GetScrollInfo(hwnd, SB_VERT, &si);
            pos = si.nTrackPos;
            break;
        }
        default: break;
        }
        g_scroll = pos;
        clamp_scroll(hwnd);
        update_scrollbar(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (on_main_key((UINT)wParam)) return 0;
        break;
    case WM_TIMER:
        if (wParam == ID_TIMER_TOPMOST) {
            if (g_topmost) push_zorder();
        } else if (wParam == ID_TIMER_LOGMON) {
        eq_log_poll();
            pull_matches();
        }
        return 0;
    case WM_APP_SOLVED:
        apply_solve_result((SolveJob *)lParam);
        return 0;
    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_DPICHANGED: {
        RECT *r = (RECT *)lParam;
        ensure_fonts(hwnd);
        SetWindowPos(hwnd, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_EXITSIZEMOVE: save_settings(hwnd); return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lParam;
        mm->ptMinTrackSize.x = 720;
        mm->ptMinTrackSize.y = 560;
        return 0;
    }
    case WM_CLOSE:
        if (g_demo_hwnd) DestroyWindow(g_demo_hwnd);
        if (g_codes_hwnd) DestroyWindow(g_codes_hwnd);
        if (g_settings_hwnd) DestroyWindow(g_settings_hwnd);
    save_settings(hwnd);
    eq_log_stop_monitor();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        log_write("session_end", "Window closed");
        if (g_font) DeleteObject(g_font);
        if (g_font_bold) DeleteObject(g_font_bold);
        if (g_font_title) DeleteObject(g_font_title);
        if (g_font_small) DeleteObject(g_font_small);
        g_font = g_font_bold = g_font_title = g_font_small = NULL;
        g_hwnd = NULL;
        PostQuitMessage(0);
        return 0;
    default: break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static int register_classes(void) {
    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = g_icon_big;
    wc.hIconSm = g_icon_small;

    wc.lpfnWndProc = MainProc;
    wc.hbrBackground = g_br_bg;
    wc.lpszClassName = "WRcMain";
    if (!RegisterClassExA(&wc)) return 0;

    wc.lpfnWndProc = DemoProc;
    wc.lpszClassName = "WRcDemo";
    if (!RegisterClassExA(&wc)) return 0;

    wc.lpfnWndProc = CodesProc;
    wc.lpszClassName = "WRcCodes";
    if (!RegisterClassExA(&wc)) return 0;

    wc.lpfnWndProc = SettingsProc;
    wc.hbrBackground = g_br_bg;
    wc.lpszClassName = "WRcSettings";
    if (!RegisterClassExA(&wc)) return 0;
    return 1;
}

static void enable_dpi(void) {
    HMODULE user = GetModuleHandleA("user32.dll");
    typedef BOOL(WINAPI * SetCtxFn)(HANDLE);
    SetCtxFn set_ctx = user ? (SetCtxFn)GetProcAddress(user, "SetProcessDpiAwarenessContext") : NULL;
    if (set_ctx) {
        if (set_ctx((HANDLE)-4)) return;
    }
    SetProcessDPIAware();
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    INITCOMMONCONTROLSEX icc;
    HWND hwnd;
    MSG message;
    char detail[192];
    DWORD ex;
    (void)prev;
    (void)cmd;
    (void)show;
    enable_dpi();
    enable_dark_scrollbars();
    if (!init_paths()) {
        MessageBoxA(NULL, "Failed to initialize paths.", APP_TITLE, MB_ICONERROR);
        return 1;
    }
    InitializeCriticalSection(&g_lock);
    g_lock_ready = 1;
    load_settings();
    g_inst = inst ? inst : GetModuleHandleA(NULL);
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);
    g_br_bg = CreateSolidBrush(COL_BG);
    g_br_panel = CreateSolidBrush(COL_PANEL);
    g_br_edit = CreateSolidBrush(COL_PANEL2);
    g_icon_big = (HICON)LoadImageA(g_inst, MAKEINTRESOURCEA(IDI_APP), IMAGE_ICON, 32, 32, 0);
    g_icon_small = (HICON)LoadImageA(g_inst, MAKEINTRESOURCEA(IDI_APP), IMAGE_ICON, 16, 16, 0);
    if (!register_classes()) {
        MessageBoxA(NULL, "Could not register the window.", APP_TITLE, MB_ICONERROR);
        DeleteCriticalSection(&g_lock);
        DeleteCriticalSection(&g_paths.log_lock);
        return 1;
    }
    ex = WS_EX_APPWINDOW;
    if (g_topmost) ex |= WS_EX_TOPMOST;
    hwnd = CreateWindowExA(ex, "WRcMain", APP_TITLE, WS_OVERLAPPEDWINDOW | WS_VSCROLL, g_win_x, g_win_y, g_win_w,
                           g_win_h, NULL, NULL, g_inst, NULL);
    if (!hwnd) {
        MessageBoxA(NULL, "Could not create the window.", APP_TITLE, MB_ICONERROR);
        DeleteCriticalSection(&g_lock);
        DeleteCriticalSection(&g_paths.log_lock);
        return 1;
    }
    snprintf(detail, sizeof(detail), "native log=%s topmost=%d", g_paths.log_name, g_topmost);
    log_write("session_start", detail);
    ShowWindow(hwnd, SW_SHOW);
    ensure_on_screen(hwnd);
    UpdateWindow(hwnd);
    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        if (settings_key(&message)) continue;
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    if (g_br_bg) DeleteObject(g_br_bg);
    if (g_br_panel) DeleteObject(g_br_panel);
    if (g_br_edit) DeleteObject(g_br_edit);
    if (g_icon_big) DestroyIcon(g_icon_big);
    if (g_icon_small) DestroyIcon(g_icon_small);
    DeleteCriticalSection(&g_lock);
    DeleteCriticalSection(&g_paths.log_lock);
    return (int)message.wParam;
}
