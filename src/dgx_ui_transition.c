/*
 * dgx_ui_transition.c
 *
 *  Pages changed by a transition: the screen as it stands composed into the program's first picture, the page opened
 *  or closed, the trees marked drawn by dgx_ui_paint_changes() with nobody told, so that the looks keep what a pass keeps,
 *  and the screen as it will be composed into the second. The frames are made from the two pictures alone: a slide by
 *  two transfers at an offset, a crossfade in the frame buffer of a display that reads it, or strip by strip in the
 *  paint's buffer on one that cannot be read. A page of home's place moves in the place alone: what else changed,
 *  outside it, is written by the last frame. A program that names none of it carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_transition.h"

#include "dgx_draw.h"
#include "dgx_vscreen.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

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

/* the hull of what changed outside the frames' area */
static void outside_noted(void *ctx, dgx_ui_rect_t rect)
{
    dgx_ui_transition_t *tr = ctx;
    dgx_ui_rect_t        parts[4];
    const int            count = dgx_ui_rect_subtract(&rect, &tr->area, parts);
    for (int k = 0; k < count; ++k) {
        dgx_ui_rect_t *o = &tr->outside;
        if (dgx_ui_rect_empty(o)) {
            *o = parts[k];
            continue;
        }
        const int x0 = o->x < parts[k].x ? o->x : parts[k].x, y0 = o->y < parts[k].y ? o->y : parts[k].y;
        const int x1 = o->x + o->w > parts[k].x + parts[k].w ? o->x + o->w : parts[k].x + parts[k].w;
        const int y1 = o->y + o->h > parts[k].y + parts[k].h ? o->y + o->h : parts[k].y + parts[k].h;
        *o = dgx_ui_rect(x0, y0, x1 - x0, y1 - y0);
    }
}

/* the page opened or closed: the trees marked drawn first, the looks keeping what a pass keeps, what changed outside
   the area noted, then the screen as they show it into `to` */
static void begin(dgx_ui_t *home, dgx_ui_paint_t *paint, dgx_ui_transition_t *tr, int kind, dgx_ui_rect_t area)
{
    if (dgx_ui_rect_empty(&area)) return; /* a place of no width or height, hidden: nothing to move, the page at once */
    tr->area = area, tr->outside = dgx_ui_rect(0, 0, 0, 0);
    dgx_ui_paint_changes(home, paint, paint->now_ms, outside_noted, tr);
    compose_whole(home, paint, tr->to);
    tr->kind = (uint8_t)kind, tr->running = true, tr->started = false;
}

/* where the frames of a page move: the rectangle of the top page when it lies in home's place, else the screen */
static dgx_ui_rect_t area_of(const dgx_ui_t *home, const dgx_ui_paint_t *paint)
{
    const dgx_ui_t *page = NULL;
    for (const dgx_ui_t *tree = home; tree->popup; tree = tree->popup)
        if ((tree->popup_flags & DGX_UI_POPUP_BACK) && (tree->popup_flags & (DGX_UI_POPUP_SCREEN | DGX_UI_POPUP_PLACE)))
            page = (tree->popup_flags & DGX_UI_POPUP_PLACE) && home->page_place ? tree->popup : NULL;
    return page ? page->root_rect : dgx_ui_rect(0, 0, paint->scr->width, paint->scr->height);
}

/* the top of the chain of popups over home */
static const dgx_ui_t *top_of(const dgx_ui_t *home)
{
    while (home->popup) home = home->popup;
    return home;
}

bool dgx_ui_page_open_by(dgx_ui_t *home, dgx_ui_t *page, dgx_ui_paint_t *paint, dgx_ui_transition_t *tr, int kind)
{
    /* from an on_event() told within another's setting up (a page's DGX_UI_EVENT_SHOWN): the page at once, the outer one's pictures
       holding it; or no transition to be had */
    if (home->setting_up || !ready(tr, paint, kind)) return dgx_ui_page_open(home, page);
    compose_whole(home, paint, tr->from); /* the screen as it stands: what the display shows */
    const dgx_ui_t *was = top_of(home);
    home->setting_up    = true;
    const bool opened   = dgx_ui_page_open(home, page);
    home->setting_up    = false;
    /* the trees as they are after every on_event() told: what comes in; nothing to slide to when an on_event() went back at once */
    if (opened && top_of(home) != was) begin(home, paint, tr, kind, area_of(home, paint));
    return opened;
}

bool dgx_ui_page_back_by(dgx_ui_t *home, dgx_ui_paint_t *paint, dgx_ui_transition_t *tr, int kind)
{
    /* from an on_event() told within another's setting up (the page's DGX_UI_EVENT_BACK, a tree's DGX_UI_EVENT_SHOWN): the page back at
       once, the outer one's pictures holding it; or no transition to be had. The mark is the tree's own: a back over a
       tree of another task is made meanwhile */
    if (home->setting_up || !ready(tr, paint, kind)) return dgx_ui_page_back(home);
    compose_whole(home, paint, tr->from); /* the top page still open */
    const dgx_ui_t     *was  = top_of(home);
    const dgx_ui_rect_t area = area_of(home, paint); /* the page that goes */
    home->setting_up         = true;
    const bool closed        = dgx_ui_page_back(home);
    home->setting_up         = false;
    /* the trees as they are after every on_event() told: what comes in; nothing to slide to when an on_event() opened it again */
    if (closed && top_of(home) != was) begin(home, paint, tr, kind, area);
    return closed;
}

/* a crossfade of the area strip by strip in the paint's buffer: `from`'s rows, `to`'s over them by the alpha, sent */
static void crossfade_in_strips(dgx_screen_t *screen, const dgx_ui_transition_t *tr, const dgx_ui_paint_t *paint, uint32_t alpha)
{
    const dgx_ui_rect_t a         = tr->area;
    const int           row_bytes = a.w * ((screen->color_bits + 7) / 8);
    if (row_bytes <= 0) return;
    const int           rows      = (int)(paint->buffer_size / (size_t)row_bytes);
    dgx_screen_t       *strip     = dgx_vscreen_from_array(paint->buffer, a.w, rows, screen->color_bits, screen->rgb_order);
    if (!strip) return;
    for (int y = a.y; y < a.y + a.h; y += rows) {
        const int height = y + rows <= a.y + a.h ? rows : a.y + a.h - y;
        dgx_vscreen_region_to_screen(strip, 0, 0, tr->from, a.x, y, a.w, height, 0);
        dgx_vscreen_region_to_screen(strip, 0, 0, tr->to, a.x, y, a.w, height, DGX_VSCREEN_ALPHA(alpha));
        dgx_vscreen_region_to_screen(screen, a.x, y, strip, 0, 0, a.w, height, 0);
    }
    dgx_screen_destroy(&strip);
}

bool dgx_ui_transition_frame(dgx_ui_transition_t *tr, dgx_ui_paint_t *paint, uint32_t now_ms)
{
    if (!tr->running) return false;
    if (!tr->started) tr->started = true, tr->start_ms = now_ms;
    dgx_screen_t       *screen  = paint->scr;
    const dgx_ui_rect_t a       = tr->area; /* the screen, or a page's place */
    const int           x = a.x, y = a.y, width = a.w, height = a.h;
    const uint32_t      elapsed = now_ms - tr->start_ms; /* by difference: the clock may run over */
    paint->wake_ms              = 0;                     /* the next frame at once: a frame of the area is its own pace */
    dgx_screen_batch_begin(screen);
    if (elapsed >= tr->ms) {
        /* the last: the new area whole, and what else changed outside it */
        dgx_vscreen_region_to_screen(screen, x, y, tr->to, x, y, width, height, 0);
        const dgx_ui_rect_t o = tr->outside;
        if (!dgx_ui_rect_empty(&o)) dgx_vscreen_region_to_screen(screen, o.x, o.y, tr->to, o.x, o.y, o.w, o.h, 0);
        tr->running = false;
    } else if (tr->kind == DGX_UI_CROSSFADE) {
        const uint32_t alpha = 255u * elapsed / tr->ms;
        if (dgx_screen_is_readable(screen)) {
            dgx_vscreen_region_to_screen(screen, x, y, tr->from, x, y, width, height, 0);
            dgx_vscreen_region_to_screen(screen, x, y, tr->to, x, y, width, height, DGX_VSCREEN_ALPHA(alpha));
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
            dgx_vscreen_region_to_screen(screen, x, y, tr->from, x + shift, y, width - shift, height, 0);
            dgx_vscreen_region_to_screen(screen, x + width - shift, y, tr->to, x, y, shift, height, 0);
            break;
        case DGX_UI_SLIDE_RIGHT: /* from the left */
            dgx_vscreen_region_to_screen(screen, x, y, tr->to, x + width - shift, y, shift, height, 0);
            dgx_vscreen_region_to_screen(screen, x + shift, y, tr->from, x, y, width - shift, height, 0);
            break;
        case DGX_UI_SLIDE_UP: /* from below */
            dgx_vscreen_region_to_screen(screen, x, y, tr->from, x, y + shift, width, height - shift, 0);
            dgx_vscreen_region_to_screen(screen, x, y + height - shift, tr->to, x, y, width, shift, 0);
            break;
        default: /* DGX_UI_SLIDE_DOWN, from above */
            dgx_vscreen_region_to_screen(screen, x, y, tr->to, x, y + height - shift, width, shift, 0);
            dgx_vscreen_region_to_screen(screen, x, y + shift, tr->from, x, y, width, height - shift, 0);
            break;
        }
    }
    dgx_screen_batch_end(screen);
    return true;
}
