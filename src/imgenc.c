#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif

#include "imgenc.h"

#include <windows.h>
#include <wincodec.h>
#include <objbase.h>

#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

void imgenc_free(void *p) { free(p); }

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} Buf;

static int buf_reserve(Buf *b, size_t need) {
    uint8_t *n;
    size_t cap;
    if (need <= b->cap) return 1;
    cap = b->cap ? b->cap : 4096;
    while (cap < need) {
        if (cap > (SIZE_MAX / 2)) return 0;
        cap *= 2;
    }
    n = (uint8_t *)realloc(b->data, cap);
    if (!n) return 0;
    b->data = n;
    b->cap = cap;
    return 1;
}

static int buf_append(Buf *b, const void *src, size_t n) {
    if (!buf_reserve(b, b->len + n)) return 0;
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 1;
}

static int buf_u8(Buf *b, uint8_t v) { return buf_append(b, &v, 1); }

static int buf_u16le(Buf *b, uint16_t v) {
    uint8_t p[2] = {(uint8_t)(v & 0xff), (uint8_t)((v >> 8) & 0xff)};
    return buf_append(b, p, 2);
}

/* ---------- PNG via WIC ---------- */

int imgenc_png_bgra(const uint8_t *bgra, int width, int height, int stride, int top_down, uint8_t **out,
                    size_t *out_len) {
    HRESULT hr;
    IWICImagingFactory *factory = NULL;
    IWICBitmap *bitmap = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    IStream *stream = NULL;
    HGLOBAL hg = NULL;
    STATSTG stat;
    void *locked = NULL;
    WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
    UINT w = (UINT)width;
    UINT h = (UINT)height;
    int ok = 0;
    uint8_t *owned = NULL;
    uint8_t *copy = NULL;
    int i;

    if (!bgra || width <= 0 || height <= 0 || stride < width * 4 || !out || !out_len) return 0;
    *out = NULL;
    *out_len = 0;

    if (!top_down) {
        owned = (uint8_t *)malloc((size_t)height * (size_t)stride);
        if (!owned) return 0;
        for (i = 0; i < height; i++)
            memcpy(owned + (size_t)i * (size_t)stride, bgra + (size_t)(height - 1 - i) * (size_t)stride, (size_t)stride);
        bgra = owned;
    }

    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                          (void **)&factory);
    if (FAILED(hr)) goto done;

    hr = IWICImagingFactory_CreateBitmapFromMemory(factory, w, h, &GUID_WICPixelFormat32bppBGRA, (UINT)stride,
                                                   (UINT)stride * h, (BYTE *)(UINT_PTR)bgra, &bitmap);
    if (FAILED(hr)) goto done;

    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    if (FAILED(hr)) goto done;

    hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng, NULL, &encoder);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapFrameEncode_Initialize(frame, NULL);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapFrameEncode_SetSize(frame, w, h);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &fmt);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapFrameEncode_WriteSource(frame, (IWICBitmapSource *)bitmap, NULL);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapFrameEncode_Commit(frame);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapEncoder_Commit(encoder);
    if (FAILED(hr)) goto done;

    hr = GetHGlobalFromStream(stream, &hg);
    if (FAILED(hr) || !hg) goto done;
    memset(&stat, 0, sizeof(stat));
    hr = IStream_Stat(stream, &stat, STATFLAG_NONAME);
    if (FAILED(hr)) goto done;
    locked = GlobalLock(hg);
    if (!locked) goto done;
    copy = (uint8_t *)malloc((size_t)stat.cbSize.LowPart);
    if (!copy) {
        GlobalUnlock(hg);
        goto done;
    }
    memcpy(copy, locked, (size_t)stat.cbSize.LowPart);
    GlobalUnlock(hg);
    *out = copy;
    *out_len = (size_t)stat.cbSize.LowPart;
    ok = 1;

done:
    if (frame) IWICBitmapFrameEncode_Release(frame);
    if (encoder) IWICBitmapEncoder_Release(encoder);
    if (bitmap) IWICBitmap_Release(bitmap);
    if (factory) IWICImagingFactory_Release(factory);
    if (stream) IStream_Release(stream);
    free(owned);
    return ok;
}

/* ---------- Animated GIF ---------- */

typedef struct {
    uint8_t r, g, b;
} Rgb;

static uint32_t rgb_key(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static int color_dist2(Rgb a, Rgb b) {
    int dr = (int)a.r - (int)b.r;
    int dg = (int)a.g - (int)b.g;
    int db = (int)a.b - (int)b.b;
    return dr * dr + dg * dg + db * db;
}

static int build_palette(const uint8_t *const *frames, int frame_count, int width, int height, int stride, Rgb *pal,
                         int *pal_count) {
    enum { HASH_CAP = 4096 };
    typedef struct {
        uint32_t key;
        Rgb c;
        int count;
        int used;
    } Entry;
    Entry *table;
    Entry **list;
    int unique = 0;
    int f, y, x, i, j, n;

    table = (Entry *)calloc(HASH_CAP, sizeof(Entry));
    if (!table) return 0;

    for (f = 0; f < frame_count; f++) {
        const uint8_t *row = frames[f];
        for (y = 0; y < height; y++) {
            const uint8_t *p = row + (size_t)y * (size_t)stride;
            for (x = 0; x < width; x++, p += 4) {
                uint32_t key = rgb_key(p[2], p[1], p[0]);
                uint32_t slot = key % HASH_CAP;
                for (i = 0; i < HASH_CAP; i++) {
                    uint32_t idx = (slot + (uint32_t)i) % HASH_CAP;
                    if (!table[idx].used) {
                        if (unique >= HASH_CAP) break;
                        table[idx].used = 1;
                        table[idx].key = key;
                        table[idx].c.r = p[2];
                        table[idx].c.g = p[1];
                        table[idx].c.b = p[0];
                        table[idx].count = 1;
                        unique++;
                        break;
                    }
                    if (table[idx].key == key) {
                        table[idx].count++;
                        break;
                    }
                }
            }
        }
    }

    list = (Entry **)malloc(sizeof(Entry *) * (size_t)(unique > 0 ? unique : 1));
    if (!list) {
        free(table);
        return 0;
    }
    n = 0;
    for (i = 0; i < HASH_CAP; i++) {
        if (table[i].used) list[n++] = &table[i];
    }
    for (i = 1; i < n; i++) {
        Entry *key = list[i];
        j = i - 1;
        while (j >= 0 && list[j]->count < key->count) {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = key;
    }
    *pal_count = n < 256 ? n : 256;
    for (i = 0; i < *pal_count; i++) pal[i] = list[i]->c;
    free(list);
    free(table);
    if (*pal_count < 2) {
        pal[0].r = pal[0].g = pal[0].b = 0;
        pal[1].r = pal[1].g = pal[1].b = 255;
        *pal_count = 2;
    }
    return 1;
}

static int nearest_pal(Rgb c, const Rgb *pal, int pal_count) {
    int best = 0;
    int best_d = 1 << 30;
    int i;
    for (i = 0; i < pal_count; i++) {
        int d = color_dist2(c, pal[i]);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

/* Bit-packer matching lecram/gifenc (known-good GIF LZW framing). */
typedef struct {
    Buf *out;
    uint8_t buffer[255];
    uint32_t partial;
    int offset; /* bit offset into buffer */
    int ok;
} GifBits;

static void gif_bits_init(GifBits *g, Buf *out) {
    memset(g, 0, sizeof(*g));
    g->out = out;
    g->ok = 1;
}

static void gif_put_key(GifBits *g, uint16_t key, int key_size) {
    int byte_offset, bit_offset, bits_to_write;
    if (!g->ok) return;
    byte_offset = g->offset / 8;
    bit_offset = g->offset % 8;
    g->partial |= ((uint32_t)key) << bit_offset;
    bits_to_write = bit_offset + key_size;
    while (bits_to_write >= 8) {
        g->buffer[byte_offset++] = (uint8_t)(g->partial & 0xFF);
        if (byte_offset == 0xFF) {
            if (!buf_u8(g->out, 0xFF) || !buf_append(g->out, g->buffer, 0xFF)) {
                g->ok = 0;
                return;
            }
            byte_offset = 0;
        }
        g->partial >>= 8;
        bits_to_write -= 8;
    }
    g->offset = (g->offset + key_size) % (0xFF * 8);
}

static void gif_end_key(GifBits *g) {
    int byte_offset;
    if (!g->ok) return;
    byte_offset = g->offset / 8;
    if (g->offset % 8) g->buffer[byte_offset++] = (uint8_t)(g->partial & 0xFF);
    if (byte_offset) {
        if (!buf_u8(g->out, (uint8_t)byte_offset) || !buf_append(g->out, g->buffer, (size_t)byte_offset)) {
            g->ok = 0;
            return;
        }
    }
    if (!buf_u8(g->out, 0)) g->ok = 0;
    g->offset = 0;
    g->partial = 0;
}

static int gif_lzw_encode(const uint8_t *indexed, int width, int height, int min_code_size, Buf *out) {
    /* Trie-based LZW adapted from lecram/gifenc (ISC). */
    enum { MAX_DEG = 256 };
    int degree = 1 << min_code_size;
    int nkeys, key_size, i, clear;
    int16_t *parent = NULL;  /* parent code for entries degree+2 .. */
    int16_t *suf = NULL;     /* unused; children via flat map */
    int16_t *child = NULL;   /* child[code * degree + pixel] -> next code, -1 empty */
    int cur;
    int total = width * height;
    GifBits bits;
    int ok = 0;

    if (total <= 0 || min_code_size < 2 || min_code_size > 8) return 0;
    clear = degree;

    child = (int16_t *)malloc(sizeof(int16_t) * (size_t)MAX_DEG * (size_t)degree);
    if (!child) return 0;
    /* Only need child links for codes 0..4095, but store per-code row of `degree`.
       Use a growable approach: child table for each active code. Cap at 4096 codes. */
    free(child);
    child = (int16_t *)malloc(sizeof(int16_t) * 4096 * (size_t)degree);
    parent = (int16_t *)malloc(sizeof(int16_t) * 4096);
    suf = (int16_t *)malloc(sizeof(int16_t) * 4096);
    if (!child || !parent || !suf) goto done;
    for (i = 0; i < 4096 * degree; i++) child[i] = -1;

    if (!buf_u8(out, (uint8_t)min_code_size)) goto done;
    gif_bits_init(&bits, out);

    /* Init roots 0..degree-1; nkeys skips clear+eoi like gifenc. */
    nkeys = degree + 2;
    key_size = min_code_size + 1;
    gif_put_key(&bits, (uint16_t)clear, key_size);
    cur = indexed[0] & (degree - 1);

    for (i = 1; i < total; i++) {
        int pixel = indexed[i] & (degree - 1);
        int nxt = child[cur * degree + pixel];
        if (nxt >= 0) {
            cur = nxt;
            continue;
        }
        gif_put_key(&bits, (uint16_t)cur, key_size);
        if (nkeys < 0x1000) {
            if (nkeys == (1 << key_size)) key_size++;
            child[cur * degree + pixel] = (int16_t)nkeys;
            parent[nkeys] = (int16_t)cur;
            suf[nkeys] = (int16_t)pixel;
            nkeys++;
        } else {
            int c;
            gif_put_key(&bits, (uint16_t)clear, key_size);
            for (c = 0; c < 4096 * degree; c++) child[c] = -1;
            nkeys = degree + 2;
            key_size = min_code_size + 1;
        }
        cur = pixel;
    }
    gif_put_key(&bits, (uint16_t)cur, key_size);
    gif_put_key(&bits, (uint16_t)(clear + 1), key_size);
    gif_end_key(&bits);
    ok = bits.ok;

done:
    free(child);
    free(parent);
    free(suf);
    return ok;
}

int imgenc_gif_bgra(const uint8_t *const *frames, int frame_count, int width, int height, int stride, int delay_cs,
                    uint8_t **out, size_t *out_len) {
    Rgb pal[256];
    int pal_count = 0;
    int pal_bits = 1;
    int color_count;
    int min_code;
    Buf buf = {0};
    uint8_t *indexed = NULL;
    int f, i, y, x;
    int ok = 0;

    if (!frames || frame_count <= 0 || width <= 0 || height <= 0 || stride < width * 4 || !out || !out_len) return 0;
    *out = NULL;
    *out_len = 0;
    if (delay_cs < 1) delay_cs = 1;

    if (!build_palette(frames, frame_count, width, height, stride, pal, &pal_count)) return 0;
    while ((1 << pal_bits) < pal_count && pal_bits < 8) pal_bits++;
    color_count = 1 << pal_bits;
    min_code = pal_bits < 2 ? 2 : pal_bits;

    indexed = (uint8_t *)malloc((size_t)width * (size_t)height);
    if (!indexed) return 0;

    if (!buf_append(&buf, "GIF89a", 6)) goto done;
    if (!buf_u16le(&buf, (uint16_t)width) || !buf_u16le(&buf, (uint16_t)height)) goto done;
    if (!buf_u8(&buf, (uint8_t)(0x80 | ((pal_bits - 1) & 7))) || !buf_u8(&buf, 0) || !buf_u8(&buf, 0)) goto done;
    for (i = 0; i < color_count; i++) {
        Rgb c = i < pal_count ? pal[i] : (Rgb){0, 0, 0};
        if (!buf_u8(&buf, c.r) || !buf_u8(&buf, c.g) || !buf_u8(&buf, c.b)) goto done;
    }
    /* Netscape loop forever */
    if (!buf_u8(&buf, 0x21) || !buf_u8(&buf, 0xff) || !buf_u8(&buf, 0x0b) || !buf_append(&buf, "NETSCAPE2.0", 11) ||
        !buf_u8(&buf, 0x03) || !buf_u8(&buf, 0x01) || !buf_u16le(&buf, 0) || !buf_u8(&buf, 0x00))
        goto done;

    for (f = 0; f < frame_count; f++) {
        const uint8_t *row = frames[f];
        for (y = 0; y < height; y++) {
            const uint8_t *p = row + (size_t)y * (size_t)stride;
            for (x = 0; x < width; x++, p += 4) {
                Rgb c = {p[2], p[1], p[0]};
                indexed[y * width + x] = (uint8_t)nearest_pal(c, pal, pal_count);
            }
        }
        /* Graphic Control Extension */
        if (!buf_u8(&buf, 0x21) || !buf_u8(&buf, 0xf9) || !buf_u8(&buf, 0x04) || !buf_u8(&buf, 0x00) ||
            !buf_u16le(&buf, (uint16_t)delay_cs) || !buf_u8(&buf, 0x00) || !buf_u8(&buf, 0x00))
            goto done;
        /* Image Descriptor */
        if (!buf_u8(&buf, 0x2c) || !buf_u16le(&buf, 0) || !buf_u16le(&buf, 0) || !buf_u16le(&buf, (uint16_t)width) ||
            !buf_u16le(&buf, (uint16_t)height) || !buf_u8(&buf, 0x00))
            goto done;
        if (!gif_lzw_encode(indexed, width, height, min_code, &buf)) goto done;
    }
    if (!buf_u8(&buf, 0x3b)) goto done;

    *out = buf.data;
    *out_len = buf.len;
    buf.data = NULL;
    ok = 1;

done:
    free(indexed);
    free(buf.data);
    return ok;
}
