#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <objbase.h>
#include "imgenc.h"

static int write_file(const char *path, const uint8_t *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (fwrite(data, 1, len, f) != len) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

int main(void) {
    const int w = 80, h = 40, frames = 3;
    uint8_t *bufs[3];
    const uint8_t *ptrs[3];
    uint8_t *gif = NULL;
    size_t glen = 0;
    int f, y, x, ok;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    for (f = 0; f < frames; f++) {
        bufs[f] = (uint8_t *)malloc((size_t)w * h * 4);
        if (!bufs[f]) return 1;
        ptrs[f] = bufs[f];
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                uint8_t *p = bufs[f] + ((size_t)y * w + x) * 4;
                int on = ((x / 10) + (y / 10) + f) & 1;
                if (on) {
                    p[0] = 40;
                    p[1] = 90;
                    p[2] = 220;
                    p[3] = 255;
                } else {
                    p[0] = 26;
                    p[1] = 20;
                    p[2] = 18;
                    p[3] = 255;
                }
                /* moving bar */
                if (x >= f * 20 && x < f * 20 + 12) {
                    p[0] = 69;
                    p[1] = 122;
                    p[2] = 255;
                    p[3] = 255;
                }
            }
        }
    }

    ok = imgenc_gif_bgra(ptrs, frames, w, h, w * 4, 50, &gif, &glen);
    if (!ok || !gif) {
        printf("gif encode failed\n");
        return 1;
    }
    if (!write_file("build\\test_anim.gif", gif, glen)) {
        printf("write failed\n");
        return 1;
    }
    printf("wrote build\\test_anim.gif (%zu bytes)\n", glen);

    /* Also verify PNG path */
    {
        uint8_t *png = NULL;
        size_t plen = 0;
        if (!imgenc_png_bgra(bufs[0], w, h, w * 4, 1, &png, &plen) || !png) {
            printf("png encode failed\n");
            return 1;
        }
        if (!write_file("build\\test_frame.png", png, plen)) {
            printf("png write failed\n");
            return 1;
        }
        printf("wrote build\\test_frame.png (%zu bytes)\n", plen);
        imgenc_free(png);
    }

    imgenc_free(gif);
    for (f = 0; f < frames; f++) free(bufs[f]);
    if (SUCCEEDED(hr)) CoUninitialize();
    printf("ok\n");
    return 0;
}
