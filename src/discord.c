#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "discord.h"

#include <windows.h>
#include <winhttp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "winhttp.lib")

static void set_err(char *err, size_t err_cap, const char *msg) {
    if (!err || err_cap == 0) return;
    if (!msg) {
        err[0] = 0;
        return;
    }
    snprintf(err, err_cap, "%s", msg);
}

static int append_bytes(uint8_t **buf, size_t *len, size_t *cap, const void *src, size_t n) {
    uint8_t *p;
    size_t need = *len + n;
    size_t c = *cap;
    if (need > c) {
        if (!c) c = 8192;
        while (c < need) {
            if (c > SIZE_MAX / 2) return 0;
            c *= 2;
        }
        p = (uint8_t *)realloc(*buf, c);
        if (!p) return 0;
        *buf = p;
        *cap = c;
    }
    memcpy(*buf + *len, src, n);
    *len += n;
    return 1;
}

static int append_str(uint8_t **buf, size_t *len, size_t *cap, const char *s) {
    return append_bytes(buf, len, cap, s, strlen(s));
}

static void json_escape(const char *in, char *out, size_t out_cap) {
    size_t o = 0;
    if (!out || out_cap == 0) return;
    out[0] = 0;
    if (!in) return;
    for (; *in && o + 2 < out_cap; in++) {
        unsigned char c = (unsigned char)*in;
        if (c == '"' || c == '\\') {
            if (o + 3 >= out_cap) break;
            out[o++] = '\\';
            out[o++] = (char)c;
        } else if (c == '\n') {
            if (o + 3 >= out_cap) break;
            out[o++] = '\\';
            out[o++] = 'n';
        } else if (c == '\r') {
            if (o + 3 >= out_cap) break;
            out[o++] = '\\';
            out[o++] = 'r';
        } else if (c < 0x20) {
            continue;
        } else {
            out[o++] = (char)c;
        }
    }
    out[o] = 0;
}

static int build_body(const char *content, const DiscordFile *files, int file_count, const char *boundary,
                      uint8_t **out_body, size_t *out_len) {
    uint8_t *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    char esc[DISCORD_CONTENT_CAP * 2];
    char part[1024];
    int i;

    json_escape(content ? content : "", esc, sizeof(esc));
    {
        char json[2048];
        size_t jlen = 0;
        jlen += (size_t)snprintf(json + jlen, sizeof(json) - jlen, "{\"content\":\"%s\"", esc);
        if (file_count > 0 && jlen + 16 < sizeof(json)) {
            jlen += (size_t)snprintf(json + jlen, sizeof(json) - jlen, ",\"attachments\":[");
            for (i = 0; i < file_count; i++) {
                const char *name = files[i].filename[0] ? files[i].filename : "file.bin";
                jlen += (size_t)snprintf(json + jlen, sizeof(json) - jlen, "%s{\"id\":%d,\"filename\":\"%s\"}",
                                         i ? "," : "", i, name);
            }
            if (jlen + 2 < sizeof(json)) jlen += (size_t)snprintf(json + jlen, sizeof(json) - jlen, "]");
        }
        if (jlen + 2 < sizeof(json)) jlen += (size_t)snprintf(json + jlen, sizeof(json) - jlen, "}");
        snprintf(part, sizeof(part),
                 "--%s\r\nContent-Disposition: form-data; name=\"payload_json\"\r\nContent-Type: application/json\r\n\r\n",
                 boundary);
        if (!append_str(&buf, &len, &cap, part)) goto fail;
        if (!append_bytes(&buf, &len, &cap, json, strlen(json))) goto fail;
        if (!append_str(&buf, &len, &cap, "\r\n")) goto fail;
    }

    for (i = 0; i < file_count; i++) {
        const DiscordFile *f = &files[i];
        const char *name = f->filename[0] ? f->filename : "file.bin";
        const char *ctype = "application/octet-stream";
        size_t nlen = strlen(name);
        if (nlen >= 4 && _stricmp(name + nlen - 4, ".png") == 0) ctype = "image/png";
        else if (nlen >= 4 && _stricmp(name + nlen - 4, ".gif") == 0) ctype = "image/gif";
        snprintf(part, sizeof(part),
                 "--%s\r\nContent-Disposition: form-data; name=\"files[%d]\"; filename=\"%s\"\r\n"
                 "Content-Type: %s\r\n\r\n",
                 boundary, i, name, ctype);
        if (!append_str(&buf, &len, &cap, part)) goto fail;
        if (f->data && f->len) {
            if (!append_bytes(&buf, &len, &cap, f->data, f->len)) goto fail;
        }
        if (!append_str(&buf, &len, &cap, "\r\n")) goto fail;
    }
    snprintf(part, sizeof(part), "--%s--\r\n", boundary);
    if (!append_str(&buf, &len, &cap, part)) goto fail;

    *out_body = buf;
    *out_len = len;
    return 1;

fail:
    free(buf);
    *out_body = NULL;
    *out_len = 0;
    return 0;
}

int discord_webhook_send(const char *url, const char *content, const DiscordFile *files, int file_count, char *err,
                         size_t err_cap) {
    URL_COMPONENTS uc;
    wchar_t wurl[2048];
    wchar_t host[256];
    wchar_t path[1024];
    wchar_t extra[1024];
    wchar_t full_path[2048];
    char boundary[64];
    wchar_t whdr[320];
    uint8_t *body = NULL;
    size_t body_len = 0;
    HINTERNET session = NULL;
    HINTERNET connect = NULL;
    HINTERNET request = NULL;
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    BOOL ok_http = FALSE;
    int result = 0;
    INTERNET_PORT port;
    DWORD flags;

    set_err(err, err_cap, "");
    if (!url || !url[0]) {
        set_err(err, err_cap, "Missing webhook URL");
        return 0;
    }
    if (file_count < 0) file_count = 0;
    if (file_count > DISCORD_MAX_FILES) file_count = DISCORD_MAX_FILES;

    if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 2048)) {
        set_err(err, err_cap, "Invalid webhook URL encoding");
        return 0;
    }

    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 1024;
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = 1024;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) {
        set_err(err, err_cap, "Invalid webhook URL");
        return 0;
    }
    if (uc.nScheme != INTERNET_SCHEME_HTTPS && uc.nScheme != INTERNET_SCHEME_HTTP) {
        set_err(err, err_cap, "Webhook must be http(s)");
        return 0;
    }
    full_path[0] = 0;
    wcsncpy(full_path, path, 2047);
    full_path[2047] = 0;
    if (extra[0]) {
        size_t used = wcslen(full_path);
        wcsncat(full_path, extra, 2047 - used);
    }

    snprintf(boundary, sizeof(boundary), "----wrc%d", (int)(GetTickCount() & 0x7fffffff));
    if (!build_body(content, files, file_count, boundary, &body, &body_len)) {
        set_err(err, err_cap, "Failed to build webhook body");
        return 0;
    }

    session = WinHttpOpen(L"WeaponRackSolver/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        set_err(err, err_cap, "WinHttpOpen failed");
        goto done;
    }
    port = uc.nPort ? uc.nPort : (uc.nScheme == INTERNET_SCHEME_HTTPS ? (INTERNET_PORT)443 : (INTERNET_PORT)80);
    connect = WinHttpConnect(session, host, port, 0);
    if (!connect) {
        set_err(err, err_cap, "WinHttpConnect failed");
        goto done;
    }
    flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    request = WinHttpOpenRequest(connect, L"POST", full_path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 flags);
    if (!request) {
        set_err(err, err_cap, "WinHttpOpenRequest failed");
        goto done;
    }

    {
        char hdr[256];
        snprintf(hdr, sizeof(hdr), "Content-Type: multipart/form-data; boundary=%s\r\n", boundary);
        MultiByteToWideChar(CP_UTF8, 0, hdr, -1, whdr, 320);
    }
    if (!WinHttpAddRequestHeaders(request, whdr, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
        set_err(err, err_cap, "Failed to set Content-Type");
        goto done;
    }

    ok_http = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, body, (DWORD)body_len, (DWORD)body_len, 0);
    if (!ok_http) {
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
    for (;;) {
        DWORD avail = 0, read = 0;
        char tmp[512];
        if (!WinHttpQueryDataAvailable(request, &avail) || avail == 0) break;
        if (avail > sizeof(tmp)) avail = (DWORD)sizeof(tmp);
        if (!WinHttpReadData(request, tmp, avail, &read) || read == 0) break;
    }
    if (status >= 200 && status < 300) {
        result = 1;
    } else {
        snprintf(err, err_cap, "HTTP %lu", (unsigned long)status);
    }

done:
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    free(body);
    return result;
}
