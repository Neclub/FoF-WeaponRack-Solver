#ifndef WRC_IMGENC_H
#define WRC_IMGENC_H

#include <stddef.h>
#include <stdint.h>

/* Encode a top-down or bottom-up 32bpp BGRA buffer to PNG. stride is bytes per row. */
int imgenc_png_bgra(const uint8_t *bgra, int width, int height, int stride, int top_down, uint8_t **out,
                    size_t *out_len);

/* Encode frames (same size) to an animated GIF. delay_cs is delay in centiseconds per frame.
   bgra frames are top-down 32bpp BGRA with the given stride. */
int imgenc_gif_bgra(const uint8_t *const *frames, int frame_count, int width, int height, int stride, int delay_cs,
                    uint8_t **out, size_t *out_len);

void imgenc_free(void *p);

#endif
