/*
 * dgx_ui_deep.c
 *
 *  Pictures on a screen of 18 or 24 bits, named by the program as the paint's strips, dgx_ui_strips_pictures: the
 *  strips of dgx_ui_strips, and the looks of RGB565 and of the frame of nine pictures put their pixels through a strip
 *  of the screen's depth in the free part of the buffer, 3 bytes a pixel, a block of rows sent by one transfer. An
 *  index is the painter's colour; RGB565 is spread to 8 bits a channel, its high bits repeated into the low ones, so
 *  that its white is 255, 255, 255. On the drawn background the strip is laid with it first and an index 0 leaves it:
 *  every pixel once. A program that names it not carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <string.h>

#include "dgx_draw.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_vscreen.h"

/* a colour of the screen's of 18 or 24 bits from RGB565: each channel's high bits repeated into its low ones */
static uint32_t deep_of_565(uint8_t bits, uint16_t v)
{
    const uint32_t r5 = v >> 11, g6 = v >> 5 & 63u, b5 = v & 31u;
    const uint32_t c  = (r5 << 3 | r5 >> 2) << 16 | (g6 << 2 | g6 >> 4) << 8 | (b5 << 3 | b5 >> 2);
    return bits == 18 ? c & 0xfcfcfcu : c;
}

/* the strip of the screen's depth: a virtual screen over the free room of the buffer, as many source rows as it holds
   (rows), made for the call; NULL: no room for a row, or no memory */
static dgx_screen_t *strip_of(dgx_ui_paint_t *paint, int w, int k, size_t skip, uint8_t **pixels, int *rows)
{
    size_t   room;
    uint8_t *free_at = dgx_ui_buffer_free(paint, &room);
    if (!free_at || room <= skip) return NULL;
    free_at += skip, room -= skip;
    const size_t row = dgx_ui_deep_row(w, k);
    *rows            = (int)(room / row);
    if (*rows < 1) return NULL;
    *pixels = free_at;
    return dgx_vscreen_from_array(free_at, w * k, k, paint->scr->color_bits, paint->scr->rgb_order);
}

/* a block of rows of the strip sent at x, y of the tree's screen; on the drawn background it was laid with it first */
static void send(dgx_ui_paint_t *paint, dgx_screen_t *strip, uint8_t *pixels, int x, int y, int w, int h)
{
    dgx_vscreen_array_set(strip, pixels, w, h);
    dgx_vscreen_to_screen(paint->scr, x - paint->ox, y - paint->oy, strip, 0);
}

/* the drawn background laid into the strip of rows at x, y: the paint pointed at it, as the strips point it */
static void lay_background(dgx_ui_paint_t *paint, dgx_screen_t *strip, uint8_t *pixels, int x, int y, int w, int h)
{
    dgx_screen_t *scr = paint->scr;
    const int16_t ox = paint->ox, oy = paint->oy;
    dgx_vscreen_array_set(strip, pixels, w, h);
    paint->scr = strip, paint->ox = (int16_t)x, paint->oy = (int16_t)y;
    paint->drawn->lay(paint, x, y, w, h);
    paint->scr = scr, paint->ox = ox, paint->oy = oy;
}

/* the source's rows put, each pixel k times across and each row k times down; an index 0 of a transparent one left as
   the background laid it */
static bool put_indexed(dgx_ui_paint_t *paint, int x, int y, const uint8_t *src, int stride, int w, int h, int k, bool transparent, size_t used)
{
    uint8_t      *pixels;
    int           rows;
    dgx_screen_t *strip = strip_of(paint, w, k, used, &pixels, &rows); /* past the caller's 8-bit strip */
    if (!strip) return false;
    const uint8_t bits = paint->scr->color_bits;
    const size_t  line = (size_t)3 * (size_t)w * (size_t)k;
    for (int y0 = 0; y0 < h; y0 += rows) {
        const int n = h - y0 < rows ? h - y0 : rows;
        if (transparent && paint->drawn) lay_background(paint, strip, pixels, x, y + y0 * k, w * k, n * k);
        for (int r = 0; r < n; ++r) {
            const uint8_t *in = src + (size_t)(y0 + r) * (size_t)stride;
            /* each of its k rows written: a transparent one keeps the background each row has; else its first row copied */
            for (int j = 0; j < (transparent ? k : 1); ++j) {
                uint8_t *out = pixels + ((size_t)r * (size_t)k + (size_t)j) * line;
                for (int c = 0; c < w; ++c) {
                    const uint8_t index = in[c];
                    if (transparent && !index) {
                        out += 3 * k; /* the background laid there stays */
                        continue;
                    }
                    uint32_t colour = index < paint->color_count && paint->colors ? paint->colors[index] : 0;
                    if (bits == 18) colour &= 0xfcfcfcu;
                    for (int x = 0; x < k; ++x) *out++ = (uint8_t)(colour >> 16), *out++ = (uint8_t)(colour >> 8), *out++ = (uint8_t)colour;
                }
            }
            for (int j = 1; !transparent && j < k; ++j) memcpy(pixels + ((size_t)r * k + j) * line, pixels + (size_t)r * k * line, line); /* its row k times */
        }
        send(paint, strip, pixels, x, y + y0 * k, w * k, n * k);
    }
    dgx_screen_destroy(&strip);
    return true;
}

static bool put_rgb565(dgx_ui_paint_t *paint, int x, int y, const uint16_t *src, int stride, int w, int h, int k)
{
    uint8_t      *pixels;
    int           rows;
    dgx_screen_t *strip = strip_of(paint, w, k, 0, &pixels, &rows);
    if (!strip) return false;
    const uint8_t bits = paint->scr->color_bits;
    const size_t  line = (size_t)3 * (size_t)w * (size_t)k;
    for (int y0 = 0; y0 < h; y0 += rows) {
        const int n = h - y0 < rows ? h - y0 : rows;
        for (int r = 0; r < n; ++r) {
            const uint8_t *in  = (const uint8_t *)(src + (size_t)(y0 + r) * (size_t)stride); /* high byte first, as DGX keeps them */
            uint8_t       *out = pixels + (size_t)r * (size_t)k * line;
            for (int c = 0; c < w; ++c) {
                const uint32_t colour = deep_of_565(bits, (uint16_t)(in[2 * c] << 8 | in[2 * c + 1]));
                for (int j = 0; j < k; ++j) *out++ = (uint8_t)(colour >> 16), *out++ = (uint8_t)(colour >> 8), *out++ = (uint8_t)colour;
            }
            for (int j = 1; j < k; ++j) memcpy(pixels + ((size_t)r * k + j) * line, pixels + (size_t)r * k * line, line);
        }
        send(paint, strip, pixels, x, y + y0 * k, w * k, n * k);
    }
    dgx_screen_destroy(&strip);
    return true;
}

static bool draw(dgx_ui_paint_t *p, const dgx_ui_look_t *look, dgx_ui_t *ui, int i, uint32_t now_ms, dgx_ui_rect_t r, bool *more)
{
    return dgx_ui_strips_draw(p, look, ui, i, now_ms, r, more, true);
}

const dgx_ui_strips_t dgx_ui_strips_pictures = {.draw = draw, .put_indexed = put_indexed, .put_rgb565 = put_rgb565};
