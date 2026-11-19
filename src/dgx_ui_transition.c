/*
 * dgx_ui_transition.c
 *
 *  Pages changed by a transition: the screen as it stands composed into the program's first picture, the page opened
 *  or closed, the trees marked drawn by dgx_ui_changed() with nobody told, so that the looks keep what a pass keeps,
 *  and the screen as it will be composed into the second. The frames are made from the two pictures alone: a slide by
 *  two transfers at an offset, a crossfade in the frame buffer of a display that reads it, or strip by strip in the
 *  paint's buffer on one that cannot be read. A program that names none of it carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_transition.h"

#include "dgx_draw.h"
#include "dgx_vscreen.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

/* the transition of a back that is being made: a call of dgx_ui_page_back_by() from the page's root's on(), within
   it, leaves the closing and the frames to it */
static const dgx_ui_transition_t *backing;

/* the two pictures can make this transition on this screen; else the page changed at once, and why told in the log
   unless the program gave no pictures or no time: that is its choice, the same code on a board without the memory */
static bool ready(const dgx_ui_transition_t *tr, const dgx_ui_paint_t *paint, int kind)
{
    const dgx_screen_t *screen = paint->scr;
    const char         *why    = NULL;
    if (!tr || !tr->from || !tr->to || !tr->ms) return false;
    if (tr->from->width != screen->width || tr->from->height != screen->height || tr->to->width != screen->width ||
             tr->to->height != screen->height || tr->from->color_bits != screen->color_bits || tr->to->color_bits != screen->color_bits)
        why = "a picture is not of the screen's size and depth";
    else if (!dgx_ui_rect_empty(&paint->cover)) why = "the keyboard's band is open";
    else if (kind == DGX_UI_CROSSFADE && !dgx_screen_is_readable(screen) &&
             (!paint->buffer || paint->buffer_size < (size_t)screen->width * ((screen->color_bits + 7) / 8)))
        why = "a crossfade on a display that cannot be read wants a buffer of a row";
    if (why) ESP_LOGW(TAG, "a page changed at once: %s", why);
    return !why;
}

static void compose_whole(dgx_ui_t *home, dgx_ui_paint_t *paint, dgx_screen_t *picture)
{
    dgx_ui_paint_area_to(home, paint, picture, NULL, 0, 0, picture->width, picture->height);
}

/* the page opened or closed: the trees marked drawn first, the looks keeping what a pass keeps, then the screen as
   they show it into `to` */
static void begin(dgx_ui_t *home, dgx_ui_paint_t *paint, dgx_ui_transition_t *tr, int kind)
{
    dgx_ui_changed(home, paint, paint->now_ms, NULL, NULL);
    compose_whole(home, paint, tr->to);
    tr->kind = (uint8_t)kind, tr->running = true, tr->started = false;
}

bool dgx_ui_page_open_by(dgx_ui_t *home, dgx_ui_t *page, dgx_ui_paint_t *paint, dgx_ui_transition_t *tr, int kind)
{
    if (!ready(tr, paint, kind)) return dgx_ui_page_open(home, page);
    compose_whole(home, paint, tr->from); /* the screen as it stands: what the display shows */
    if (!dgx_ui_page_open(home, page)) return false;
    begin(home, paint, tr, kind);
    return true;
}

bool dgx_ui_page_back_by(dgx_ui_t *home, dgx_ui_paint_t *paint, dgx_ui_transition_t *tr, int kind)
{
    if (backing) return true; /* from the root's on() within a back by a transition: that one closes and runs */
    if (!ready(tr, paint, kind)) return dgx_ui_page_back(home);
    compose_whole(home, paint, tr->from); /* the top page still open */
    backing           = tr;
    const bool closed = dgx_ui_page_back(home);
    backing           = NULL;
    if (closed) begin(home, paint, tr, kind);
    return closed;
}

/* a crossfade strip by strip in the paint's buffer: `from`'s rows, `to`'s over them by the alpha, sent */
static void crossfade_in_strips(dgx_screen_t *screen, const dgx_ui_transition_t *tr, const dgx_ui_paint_t *paint, uint32_t alpha)
{
    const int     row_bytes = screen->width * ((screen->color_bits + 7) / 8);
    const int     rows      = (int)(paint->buffer_size / (size_t)row_bytes);
    dgx_screen_t *strip     = dgx_vscreen_from_array(paint->buffer, screen->width, rows, screen->color_bits, screen->rgb_order);
    if (!strip) return;
    for (int y = 0; y < screen->height; y += rows) {
        const int height = y + rows <= screen->height ? rows : screen->height - y;
        dgx_vscreen_region_to_screen(strip, 0, 0, tr->from, 0, y, screen->width, height, 0);
        dgx_vscreen_region_to_screen(strip, 0, 0, tr->to, 0, y, screen->width, height, DGX_VSCREEN_ALPHA(alpha));
        dgx_vscreen_region_to_screen(screen, 0, y, strip, 0, 0, screen->width, height, 0);
    }
    dgx_screen_destroy(&strip);
}

bool dgx_ui_transition_frame(dgx_ui_transition_t *tr, dgx_ui_paint_t *paint, uint32_t now_ms)
{
    if (!tr->running) return false;
    if (!tr->started) tr->started = true, tr->start_ms = now_ms;
    dgx_screen_t  *screen  = paint->scr;
    const int      width   = screen->width, height = screen->height;
    const uint32_t elapsed = now_ms - tr->start_ms; /* by difference: the clock may run over */
    paint->wake_ms         = 0;                     /* the next frame at once: a frame of the whole screen is its own pace */
    dgx_screen_batch_begin(screen);
    if (elapsed >= tr->ms) {
        dgx_vscreen_to_screen(screen, 0, 0, tr->to, 0); /* the last: the new screen whole */
        tr->running = false;
    } else if (tr->kind == DGX_UI_CROSSFADE) {
        const uint32_t alpha = 255u * elapsed / tr->ms;
        if (dgx_screen_is_readable(screen)) {
            dgx_vscreen_to_screen(screen, 0, 0, tr->from, 0);
            dgx_vscreen_to_screen(screen, 0, 0, tr->to, DGX_VSCREEN_ALPHA(alpha));
        } else {
            crossfade_in_strips(screen, tr, paint, alpha);
        }
    } else {
        /* a slide: `to` comes in by `shift` pixels and `from` goes out by as many, each pixel written once */
        const bool across = tr->kind == DGX_UI_SLIDE_LEFT || tr->kind == DGX_UI_SLIDE_RIGHT;
        const int  span   = across ? width : height;
        const int  shift  = (int)((uint32_t)span * elapsed / tr->ms);
        switch (tr->kind) {
        case DGX_UI_SLIDE_LEFT: /* from the right */
            dgx_vscreen_region_to_screen(screen, 0, 0, tr->from, shift, 0, width - shift, height, 0);
            dgx_vscreen_region_to_screen(screen, width - shift, 0, tr->to, 0, 0, shift, height, 0);
            break;
        case DGX_UI_SLIDE_RIGHT: /* from the left */
            dgx_vscreen_region_to_screen(screen, 0, 0, tr->to, width - shift, 0, shift, height, 0);
            dgx_vscreen_region_to_screen(screen, shift, 0, tr->from, 0, 0, width - shift, height, 0);
            break;
        case DGX_UI_SLIDE_UP: /* from below */
            dgx_vscreen_region_to_screen(screen, 0, 0, tr->from, 0, shift, width, height - shift, 0);
            dgx_vscreen_region_to_screen(screen, 0, height - shift, tr->to, 0, 0, width, shift, 0);
            break;
        default: /* DGX_UI_SLIDE_DOWN, from above */
            dgx_vscreen_region_to_screen(screen, 0, 0, tr->to, 0, height - shift, width, shift, 0);
            dgx_vscreen_region_to_screen(screen, 0, shift, tr->from, 0, 0, width, height - shift, 0);
            break;
        }
    }
    dgx_screen_batch_end(screen);
    return true;
}
