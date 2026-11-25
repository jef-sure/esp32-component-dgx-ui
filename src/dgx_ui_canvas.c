/*
 * dgx_ui_canvas.c
 *
 *  The canvases of widgets of DGX_UI_CANVAS: 8-bit virtual screens as large as their boxes, kept by the paint in a
 *  table of its own from pass to pass, and put on the paint's screen through a table of its colours. The painter
 *  reaches the table through two pointers it holds, so a program whose looks ask for no canvas carries none of this.
 *  It needs the virtual screens of DGX (CONFIG_DGX_ENABLE_VSCREEN). The same table keeps the memory a look keeps for
 *  a widget (dgx_ui_kept()), freed as a canvas is, which needs no virtual screen; and with each canvas sixteen bytes
 *  of its look's (dgx_ui_canvas_noted()), what it wrote there, zeros with a new canvas.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdlib.h>
#include <string.h>

#include "dgx_draw.h"
#include "dgx_lut.h"
#include "dgx_ui_paint_priv.h"
#include "dgx_vscreen.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

/* a canvas of a widget: which, and of what tree; the tree is read only while the paint draws it */
typedef struct {
    dgx_ui_t              *ui;
    const dgx_ui_widget_t *widget;
    int16_t                i;
    int16_t                none_w, none_h; /* the size no canvas could be had for: told once for it */
    bool                   tell;           /* DGX_UI_EVENT_NO_MEMORY to be told at the end of the next pass */
    dgx_screen_t          *scr;            /* NULL: none to be had */
    void                  *kept;           /* the memory its look keeps, kept_size bytes; NULL none */
    size_t                 kept_size;
    uint32_t               note[DGX_UI_CANVAS_NOTE / 4]; /* the look's own about what its canvas holds; zeros with a new one */
} entry_t;

typedef struct {
    struct dgx_ui_canvases_t ops; /* first: the painter's view of it */
    int                      count, room;
    entry_t                  at[];
} table_t;

static void end(dgx_ui_paint_t *paint, dgx_ui_t *ui);
static void free_all(dgx_ui_paint_t *paint);

/* the entry of widget i of a tree, made when asked; NULL none, or no memory for the table */
static entry_t *entry_of(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const dgx_ui_widget_t *w, bool make)
{
    table_t *t = (table_t *)paint->canvases;
    for (int k = 0; t && k < t->count; ++k)
        if (t->at[k].ui == ui && t->at[k].i == i && t->at[k].widget == w) return &t->at[k];
    if (!make) return NULL;
    if (!t || t->count == t->room) {
        const int room = t ? 2 * t->room : 4;
        table_t  *more = realloc(t, sizeof *t + (size_t)room * sizeof(entry_t));
        if (!more) return NULL;
        if (!t) *more = (table_t){.ops = {.end = end, .free = free_all}};
        t = more, t->room = room, paint->canvases = &t->ops;
    }
    entry_t *e = &t->at[t->count++];
    *e         = (entry_t){.ui = ui, .widget = w, .i = (int16_t)i};
    return e;
}

dgx_screen_t *dgx_ui_canvas_noted(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, bool *made, void **note)
{
    dgx_ui_node_t *n = dgx_ui_node(ui, i);
    *made = false, *note = NULL;
    if (!n || !n->widget || !(n->widget->flags & DGX_UI_CANVAS) || n->w <= 0 || n->h <= 0) return NULL;
    entry_t *e = entry_of(paint, ui, i, n->widget, true);
    if (!e) {
        static bool told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
        if (!told) ESP_LOGE(TAG, "widget %d: no memory for the paint's table of canvases", i);
        told = true;
        return NULL;
    }
    *note = e->note; /* the canvas's own, when there is one */
    if (e->scr && ((e->scr->width == n->w && e->scr->height == n->h) || (n->marks & DGX_UI_MARK_HARD_SIZE))) return e->scr;
    /* of another size, or none yet: a new one first, then the old one freed */
    dgx_screen_t *c = dgx_vscreen_init(n->w, n->h, 8, DgxScreenRGB);
    if (c) {
        if (e->scr) dgx_screen_destroy(&e->scr);
        e->scr = c, e->none_w = e->none_h = 0;
        memset(e->note, 0, sizeof e->note);
        *made = true;
        return c;
    }
    if (e->scr) {
        n->marks |= DGX_UI_MARK_HARD_SIZE; /* the old one kept, never changed again */
        return e->scr;
    }
    if (e->none_w != n->w || e->none_h != n->h) {
        ESP_LOGE(TAG, "widget %d: no memory for its canvas of %dx%d", i, n->w, n->h);
        e->none_w = n->w, e->none_h = n->h, e->tell = true;
    }
    *note = NULL;
    return NULL;
}

dgx_screen_t *dgx_ui_canvas(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, bool *made)
{
    void *note;
    return dgx_ui_canvas_noted(paint, ui, i, made, &note);
}

void *dgx_ui_kept(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, size_t size, bool *made)
{
    dgx_ui_node_t *n = dgx_ui_node(ui, i);
    *made            = false;
    if (!n || !n->widget || !size) return NULL;
    entry_t *e = entry_of(paint, ui, i, n->widget, true);
    if (!e) return NULL;
    if (e->kept && e->kept_size >= size) return e->kept;
    void *more = realloc(e->kept, size);
    if (!more) return NULL; /* the smaller one kept, not handed out */
    e->kept = more, e->kept_size = size, *made = true;
    return more;
}

/* the box less a hole, its paper laid */
static void paper_around(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, const dgx_ui_rect_t *hole)
{
    const dgx_ui_rect_t box = dgx_ui_rect(n->x, n->y, n->w, n->h);
    dgx_ui_rect_t       parts[4];
    const int           k = dgx_ui_rect_subtract(&box, hole, parts);
    for (int j = 0; j < k; ++j) dgx_ui_lay_paper(paint, DGX_UI_PAPER, parts[j].x, parts[j].y, parts[j].w, parts[j].h);
}

/* a colour of 16 bits as 0xRRGGBB, each channel stretched to 8 bits as the table makes it back */
static uint32_t rgb888(uint32_t c)
{
    const uint32_t r = c >> 11 & 31, g = c >> 5 & 63, b = c & 31;
    return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
}

void dgx_ui_canvas_put(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, int ink)
{
    const dgx_ui_node_t *n    = dgx_ui_node(ui, i);
    const entry_t       *e    = n ? entry_of(paint, ui, i, n->widget, false) : NULL;
    dgx_screen_t        *c    = e ? e->scr : NULL;
    const dgx_ui_rect_t  none = dgx_ui_rect(0, 0, 0, 0);
    if (!n) return;
    if (c && paint->scr->color_bits != 16) {
        static bool told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
        if (!told) ESP_LOGW(TAG, "widget %d: a canvas goes to a screen of 16 bits only: its box is paper", i);
        told = true;
        c    = NULL;
    }
    if (c && !paint->lut) paint->lut = malloc(256 * sizeof *paint->lut);
    if (!c || !paint->lut) {
        paper_around(paint, n, &none);
        return;
    }
    /* at the top left of the box, cut to it; the rest of the box paper. On the drawn background the whole box laid,
       the canvas over it */
    const bool          drawn = dgx_ui_paper_drawn(paint, DGX_UI_PAPER);
    const int           cw = c->width < n->w ? c->width : n->w, ch = c->height < n->h ? c->height : n->h;
    const dgx_ui_rect_t hole = dgx_ui_rect(n->x, n->y, cw, ch);
    paper_around(paint, n, drawn ? &none : &hole);
    const uint32_t paper = dgx_ui_color(paint, paint->paper);
    uint32_t       flags = DGX_VSCREEN_EXACT;
    if (ink < 0) {
        /* indices of the table, 0 the paper */
        for (int k = 0; k < 256; ++k) paint->lut[k] = (uint16_t)(k ? dgx_ui_color(paint, k) : paper);
        if (drawn) flags |= DGX_VSCREEN_TRANSPARENT;
    } else {
        /* amounts of the ink over the paper; on the drawn background over the picture where the screen can be read */
        dgx_lut_gradient(paint->lut, 0, 256, rgb888(paper), rgb888(dgx_ui_color(paint, ink)));
        if (drawn) flags |= dgx_screen_is_readable(paint->scr) ? DGX_VSCREEN_MASK : DGX_VSCREEN_TRANSPARENT;
    }
    /* only its rows in the clip */
    int cx, cy, cwid, chei;
    dgx_screen_clip_get(paint->scr, &cx, &cy, &cwid, &chei);
    const int top = cy + paint->oy > n->y ? cy + paint->oy - n->y : 0;
    const int bot = cy + paint->oy + chei < n->y + ch ? cy + paint->oy + chei - n->y : ch;
    if (bot > top) dgx_vscreen8_region_to_screen16(paint->scr, n->x - paint->ox, n->y + top - paint->oy, c, 0, top, cw, bot - top, paint->lut, flags);
}

/* after a pass over ui: an entry of a widget no longer drawn by this paint freed, its canvas and its kept memory (its
   tree not in the pass, its slot holding another widget); a canvas of a widget asking for none any more freed; then
   DGX_UI_EVENT_NO_MEMORY told */
static void end(dgx_ui_paint_t *paint, dgx_ui_t *ui)
{
    table_t *t    = (table_t *)paint->canvases;
    int      kept = 0;
    for (int k = 0; k < t->count; ++k) {
        entry_t   *e    = &t->at[k];
        bool       live = false;
        const dgx_ui_t *u = ui;
        while (u && u != e->ui) u = u->popup;
        const dgx_ui_node_t *n = u ? dgx_ui_node(u, e->i) : NULL;
        live                   = n && n->widget == e->widget;
        if (e->scr && (!live || !(n->widget->flags & DGX_UI_CANVAS))) dgx_screen_destroy(&e->scr);
        if (!live) {
            free(e->kept);
            continue;
        }
        t->at[kept++] = *e;
    }
    t->count = kept;
    /* told one by one: an on_event() may change the tree, and the table with it, so both are looked at again before each */
    for (int k = 0; paint->canvases && k < ((table_t *)paint->canvases)->count; ++k) {
        entry_t *e = &((table_t *)paint->canvases)->at[k];
        if (!e->tell) continue;
        e->tell                = false;
        const dgx_ui_node_t *n = dgx_ui_node(e->ui, e->i);
        if (n && n->widget == e->widget && n->widget->on_event) n->widget->on_event(e->ui, e->i, n, DGX_UI_EVENT_NO_MEMORY, n->widget->ctx);
    }
}

static void free_all(dgx_ui_paint_t *paint)
{
    table_t *t = (table_t *)paint->canvases;
    for (int k = 0; k < t->count; ++k) {
        if (t->at[k].scr) dgx_screen_destroy(&t->at[k].scr);
        free(t->at[k].kept);
    }
    free(t);
    paint->canvases = NULL;
}
