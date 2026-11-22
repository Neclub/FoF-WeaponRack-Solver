#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "update.h"

#include <windows.h>
#include <winhttp.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "winhttp.lib")

#define UPDATE_HOST L"api.github.com"
#define UPDATE_PATH L"/repos/Neclub/FoF-WeaponRack-Solver/releases/latest"
#define UPDATE_PAGE "https://github.com/Neclub/FoF-WeaponRack-Solver/releases/latest"
#define UPDATE_UA L"FoF_WeaponRack_Solver"

static void set_err(char *err, size_t err_cap, const char *msg) {
    if (!err || err_cap == 0) return;
    if (!msg) {
        err[0] = 0;
        return;
    }
    snprintf(err, err_cap, "%s", msg);
}

static const char *skip_v(const char *s) {
    if (!s) return "";
    if (s[0] == 'v' || s[0] == 'V') return s + 1;
    return s;
}

static int parse_parts(const char *s, int *a, int *b, int *c) {
    int n;
    *a = *b = *c = 0;
    if (!s || !s[0]) return 0;
    n = sscanf(skip_v(s), "%d.%d.%d", a, b, c);
    return n >= 1;
}

int update_is_newer(const char *current, const char *latest) {
    int ca, cb, cc, la, lb, lc;
    if (!parse_parts(current, &ca, &cb, &cc)) return 0;
    if (!parse_parts(latest, &la, &lb, &lc)) return 0;
    if (la != ca) return la > ca;
    if (lb != cb) return lb > cb;
    return lc > cc;
}

static int extract_json_string(const char *json, const char *key, char *out, size_t out_cap) {
    char pattern[64];
    const char *p;
    size_t i;
    if (!json || !key || !out || out_cap == 0) return 0;
    out[0] = 0;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    if (*p != ':') return 0;
    p++;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    if (*p != '"') return 0;
    p++;
    for (i = 0; i + 1 < out_cap && *p && *p != '"'; p++) {
        if (*p == '\\' && p[1]) {
            p++;
            out[i++] = *p;
        } else {
            out[i++] = *p;
        }
    }
    out[i] = 0;
    return i > 0;
}

static int append_body(char **buf, size_t *len, size_t *cap, const char *src, DWORD n) {
    char *p;
    size_t need = *len + (size_t)n + 1;
    if (need > *cap) {
        size_t c = *cap ? *cap : 4096;
        while (c < need) {
            if (c > SIZE_MAX / 2) return 0;
            c *= 2;
        }
        p = (char *)realloc(*buf, c);
        if (!p) return 0;
        *buf = p;
        *cap = c;
    }
    memcpy(*buf + *len, src, (size_t)n);
    *len += (size_t)n;
    (*buf)[*len] = 0;
    return 1;
}

int update_check_latest(UpdateInfo *out, char *err, size_t err_cap) {
    HINTERNET session = NULL, connect = NULL, request = NULL;
    DWORD status = 0, status_size = sizeof(status);
    char *body = NULL;
    size_t body_len = 0, body_cap = 0;
    char tag[UPDATE_VER_CAP];
    int ok = 0;

    if (err && err_cap) err[0] = 0;
    if (!out) {
        set_err(err, err_cap, "Missing output");
        return 0;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->page_url, sizeof(out->page_url), "%s", UPDATE_PAGE);

    session = WinHttpOpen(UPDATE_UA, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        set_err(err, err_cap, "WinHttpOpen failed");
        goto done;
    }
    connect = WinHttpConnect(session, UPDATE_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) {
        set_err(err, err_cap, "WinHttpConnect failed");
        goto done;
    }
    request = WinHttpOpenRequest(connect, L"GET", UPDATE_PATH, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 WINHTTP_FLAG_SECURE);
    if (!request) {
        set_err(err, err_cap, "WinHttpOpenRequest failed");
        goto done;
    }
    if (!WinHttpAddRequestHeaders(request,
                                  L"Accept: application/vnd.github+json\r\n"
                                  L"User-Agent: FoF_WeaponRack_Solver\r\n",
                                  (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD)) {
        set_err(err, err_cap, "Failed to set headers");
        goto done;
    }
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        set_err(err, err_cap, "WinHttpSendRequest failed");
        goto done;
    }
    if (!WinHttpReceiveResponse(request, NULL)) {
        set_err(err, err_cap, "WinHttpReceiveResponse failed");
        goto done;
    }
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                             &status, &status_size, WINHTTP_NO_HEADER_INDEX)) {
        set_err(err, err_cap, "Missing HTTP status");
        goto done;
    }
    if (status < 200 || status >= 300) {
        snprintf(err, err_cap, "HTTP %lu", (unsigned long)status);
        goto done;
    }
    for (;;) {
        DWORD avail = 0, read = 0;
        char tmp[1024];
        if (!WinHttpQueryDataAvailable(request, &avail) || avail == 0) break;
        if (avail > sizeof(tmp)) avail = (DWORD)sizeof(tmp);
        if (!WinHttpReadData(request, tmp, avail, &read) || read == 0) break;
        if (!append_body(&body, &body_len, &body_cap, tmp, read)) {
            set_err(err, err_cap, "Out of memory");
            goto done;
        }
    }
    if (!body || !extract_json_string(body, "tag_name", tag, sizeof(tag))) {
        set_err(err, err_cap, "Missing tag_name");
        goto done;
    }
    {
        const char *ver = skip_v(tag);
        size_t i = 0;
        while (ver[i] && i + 1 < sizeof(out->version)) {
            if (!(isdigit((unsigned char)ver[i]) || ver[i] == '.')) break;
            out->version[i] = ver[i];
            i++;
        }
        out->version[i] = 0;
    }
    if (!out->version[0]) {
        set_err(err, err_cap, "Empty version");
        goto done;
    }
    ok = 1;

done:
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    free(body);
    return ok;
}
