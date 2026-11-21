#pragma once
/*
 * A panel behind a bus, as the host tests of DGX model it (test/host/test_once.c there): its memory, how many times
 * each pixel was written, and the windows. Copied, as DGX keeps it inside a test of its own. Of 16 bits, or of
 * PANEL_BITS defined before (18 or 24: three bytes a pixel, as such a panel takes them).
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_screen_functions.h"
#include "dgx_screen_with_bus.h"

#ifndef PANEL_W
#define PANEL_W 96
#endif
#ifndef PANEL_H
#define PANEL_H 64
#endif
#ifndef PANEL_BITS
#define PANEL_BITS 16
#endif
#define PANEL_BYTES (PANEL_BITS == 16 ? 2 : 3)

typedef struct {
    dgx_screen_with_bus_t sbus; /* first: the screen is the panel */
    dgx_bus_protocols_t   bus;
    uint8_t               buffer[256];
    uint32_t              mem[PANEL_H][PANEL_W]; /* the pixel as its bytes came, high byte first */
    uint8_t               writes[PANEL_H][PANEL_W];
    int                   windows;
    int                   left, right, top, bottom, x, y;
} panel_t;

static void panel_set_area(dgx_screen_t *scr, uint16_t left, uint16_t right, uint16_t top, uint16_t bottom)
{
    panel_t *p = (panel_t *)scr;
    if (left > right) DGX_INT_SWAP(left, right);
    if (top > bottom) DGX_INT_SWAP(top, bottom);
    p->left = left, p->right = right, p->top = top, p->bottom = bottom, p->x = left, p->y = top;
    ++p->windows;
}

static void panel_write_area(dgx_screen_t *scr, uint8_t *data, uint32_t lenbits)
{
    panel_t *p = (panel_t *)scr;
    for (uint32_t i = 0; i < lenbits / (8u * PANEL_BYTES); ++i) {
        if (p->x < PANEL_W && p->y < PANEL_H) {
            const uint8_t *b = data + PANEL_BYTES * i;
            p->mem[p->y][p->x] = PANEL_BYTES == 2 ? (uint32_t)(b[0] << 8 | b[1]) : (uint32_t)(b[0] << 16 | b[1] << 8 | b[2]);
            if (p->writes[p->y][p->x] < 255) ++p->writes[p->y][p->x];
        }
        if (++p->x > p->right) {
            p->x = p->left;
            if (++p->y > p->bottom) p->y = p->top;
        }
    }
}

static void panel_wait(dgx_screen_t *scr) { (void)scr; }

static void panel_quiet(dgx_screen_t *scr, int left, int right, int top, int bottom) { (void)scr, (void)left, (void)right, (void)top, (void)bottom; }

static panel_t *panel_make(void)
{
    panel_t *p = calloc(1, sizeof(*p));
    p->bus.buffer = p->buffer, p->bus.buffer_len = sizeof p->buffer;
    p->sbus.bus       = &p->bus;
    dgx_screen_t *scr = &p->sbus.scr;
    scr->width = PANEL_W, scr->height = PANEL_H, scr->color_bits = PANEL_BITS, scr->screen_subtype = DgxPhysicalScreenWithBus;
    dgx_scr_init_slow_bus_optimized_funcs(scr);
    scr->set_area = panel_set_area, scr->write_area = panel_write_area, scr->wait = panel_wait, scr->update_screen = panel_quiet;
    return p;
}

/* the counts cleared */
static inline void panel_count_from_now(panel_t *p)
{
    memset(p->writes, 0, sizeof(p->writes));
    p->windows = 0;
}

/* pixels written more than once, and pixels written outside a rectangle */
static inline int panel_twice(const panel_t *p)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->writes[y][x] > 1;
    return n;
}

static inline int panel_written_outside(const panel_t *p, int left, int top, int right, int bottom)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x)
            if (p->writes[y][x] && !(x >= left && x < right && y >= top && y < bottom)) ++n;
    return n;
}

static inline int panel_written(const panel_t *p)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->writes[y][x] != 0;
    return n;
}
