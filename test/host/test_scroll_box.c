/*
 * A scrolling box (DGX_UI_SCROLL, dgx_ui_look_scroll): a column of rows taller than its inside, its offset set by the
 * program, by the place, by the finger and by the keys. After every pass the screen equals the tree composed anew by
 * a fresh paint: on a virtual screen, which moves any rectangle; on an ILI9341 standing, which moves a box from edge to
 * edge by the panel's own scroll; and lying, where it cannot and the box is drawn again. On the panel no pixel is
 * written twice in a pass, and a move writes no more than the rows that came in and the bar. The area the panel holds
 * let go when its page goes, when a second box needs it, and by dgx_ui_paint_free(). No move under a popup over the
 * box's rows, under dgx_ui_paint_changes() (the rectangles told cover the rows), over a drawn background the box does not
 * fill; a mirror composed by a second paint equals the screen. The finger drags the rows and the knob, gives a
 * button's press to the box when it goes up or down first, keeps a slider's sideways move; taps on the bar step and
 * page; the focus kept in view by the keys and by dgx_ui_scroll_to(); a box of text alone takes the focus and the keys
 * scroll it; an offset out of range comes back clamped, untold; the bar's column coming widens nothing moved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "composed.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "ili9341_model.h"
#define PANEL_W    240
#define PANEL_H    320
#define PANEL_BITS 18
#include "panel.h"

enum { WIDTH = 240, HEIGHT = 320, ROWS = 14, VIEW = 200, PAD = 2 };

static const uint32_t              colors[6] = {0x0000, 0xffff, 0x2945, 0x8410, 0xfd20, 0x07e0};
static const dgx_ui_font_variant_t variant[1] = {{.font = &TerminusTTFMedium12}};
static const dgx_ui_text_style_t   style      = {.fonts = variant, .count = 1};
static const dgx_ui_scroll_look_t  bar_params = {.track = 3, .knob = 1, .focused = 4, .arrows = 5, .width = 5};
static const dgx_ui_look_t         filled     = {.draw = dgx_ui_draw_scroll, .params = &bar_params, .fill = 2, .behavior = &dgx_ui_scroll_behavior};
static const dgx_ui_look_t         clear      = {.draw = dgx_ui_draw_scroll, .params = &bar_params, .fill = DGX_UI_PAPER, .behavior = &dgx_ui_scroll_behavior};

static uint32_t seed = 20261125u;
static int      rnd(int lo, int hi)
{
    seed ^= seed << 13, seed ^= seed >> 17, seed ^= seed << 5;
    return lo + (int)(seed % (uint32_t)(hi - lo + 1));
}

/* a page: a title, the scrolling box of ROWS rows (labels, buttons, a slider), a line under it */
typedef struct {
    dgx_ui_t         ui;
    dgx_ui_node_t    nodes[32];
    dgx_ui_widget_t  widgets[ROWS + 4];
    dgx_ui_textbox_t boxes[ROWS + 2];
    char             texts[ROWS + 2][24];
    int32_t          offset, level;
    int              box, row[ROWS], told, changed;
} page_t;

static void on_box(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    (void)ui, (void)i, (void)node;
    page_t *p = ctx;
    if (what == DGX_UI_EVENT_CHANGED) ++p->changed;
    ++p->told;
}

static void make(page_t *p, const dgx_ui_look_t *look, int top, int view, bool buttons)
{
    memset(p, 0, sizeof *p);
    dgx_ui_init(&p->ui, p->nodes, 32, 1);
    p->widgets[0] = (dgx_ui_widget_t){.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
    dgx_ui_add(&p->ui, -1, &p->widgets[0]);
    p->widgets[1] = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .want_h = (int16_t)top, .text = &p->boxes[ROWS]};
    p->widgets[2] = (dgx_ui_widget_t){.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN | DGX_UI_SCROLL, .pad = PAD, .gap = 2, .want_h = (int16_t)view,
                                      .value = &p->offset, .look = look, .on_event = on_box, .ctx = p};
    dgx_ui_textbox_init(&p->boxes[ROWS], p->texts[ROWS], 24, 1, &style);
    dgx_ui_add(&p->ui, 0, &p->widgets[1]);
    p->box = dgx_ui_add(&p->ui, 0, &p->widgets[2]);
    dgx_ui_text_set(&p->ui, 1, "Settings");
    for (int k = 0; k < ROWS; ++k) {
        dgx_ui_widget_t *w = &p->widgets[3 + k];
        if (k == 5) *w = (dgx_ui_widget_t){.kind = DGX_UI_SLIDER, .want_h = 16, .max = 10, .value = &p->level, .look = &dgx_ui_look_slider};
        else if (buttons && k % 2) *w = (dgx_ui_widget_t){.kind = DGX_UI_BUTTON, .want_h = 24, .text = &p->boxes[k], .look = &dgx_ui_look_button};
        else *w = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .want_h = 18, .text = &p->boxes[k]};
        if (w->text) dgx_ui_textbox_init(&p->boxes[k], p->texts[k], 24, 1, &style);
        p->row[k] = dgx_ui_add(&p->ui, p->box, w);
        if (w->text) {
            char t[24];
            snprintf(t, sizeof t, "row %d", k);
            dgx_ui_text_set(&p->ui, p->row[k], t);
        }
    }
}

/* the tree composed anew by a fresh paint */
static void fresh(page_t *p, dgx_screen_t *ref)
{
    dgx_ui_paint_t paint = {.scr = ref, .colors = colors, .color_count = 6};
    dgx_fill_rectangle(ref, 0, 0, ref->width, ref->height, 0);
    dgx_ui_paint_area_to(&p->ui, &paint, ref, NULL, 0, 0, ref->width, ref->height);
    dgx_ui_paint_free(&paint);
}

static int differ(dgx_screen_t *a, dgx_screen_t *b)
{
    int n = 0;
    for (int y = 0; y < a->height; ++y)
        for (int x = 0; x < a->width; ++x) n += dgx_get_pixel(a, x, y) != dgx_get_pixel(b, x, y);
    return n;
}

/* something done to the page at random: the offset by the place or by the call, a row's text, the slider */
static int act(page_t *p)
{
    const int what = rnd(0, 9);
    if (what <= 4) p->offset = rnd(-20, 300); /* by the place, out of range now and then */
    else if (what <= 6) dgx_ui_set_value(&p->ui, p->box, p->offset + rnd(-30, 30));
    else if (what == 7) p->offset += rnd(-8, 8), dgx_ui_text_set(&p->ui, p->row[rnd(0, 3) * 2], rnd(0, 1) ? "changed" : "again"); /* both in one pass */
    else if (what == 8) p->level = rnd(0, 10);
    else p->offset += rnd(1, 3) * (rnd(0, 1) ? VIEW : -VIEW) / 2;
    return what;
}

/* ---- the move against a redraw ---- */

static void on_vscreen(void)
{
    static page_t  p;
    dgx_screen_t  *scr = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB), *ref = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
    make(&p, &filled, 20, VIEW, true);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 6};
    dgx_fill_rectangle(scr, 0, 0, WIDTH, HEIGHT, 0);
    dgx_ui_paint(&p.ui, &paint, 0);
    fresh(&p, ref);
    CHECK(differ(scr, ref) == 0);
    int wrong = 0, pads = 0;
    for (int round = 0; round < 400; ++round) {
        act(&p);
        dgx_ui_paint(&p.ui, &paint, (uint32_t)round * 20);
        fresh(&p, ref);
        const int d = differ(scr, ref);
        if (d && !wrong) fprintf(stderr, "  vscreen: round %d: %d pixels differ, offset %d\n", round, d, (int)p.offset);
        wrong += d > 0;
        /* the pad at the left side: the fill all down, no child drawn there; at the end, the knob down to the lower arrow */
        const dgx_ui_node_t *b = dgx_ui_node(&p.ui, p.box);
        for (int y = 0; y < b->h; ++y)
            for (int x = 0; x < PAD; ++x)
                pads += dgx_get_pixel(scr, b->x + x, b->y + y) != colors[2];
        CHECK(p.offset >= 0 && p.offset <= dgx_ui_node(&p.ui, p.box)->h);
    }
    CHECK(wrong == 0 && pads == 0);
    {
        const dgx_ui_node_t *b = dgx_ui_node(&p.ui, p.box);
        const int            bar = bar_params.width, ah = (bar + 1) / 2, x = b->x + b->w - PAD - 1, low = b->y + b->h - 1;
        p.offset = 100000;
        dgx_ui_paint(&p.ui, &paint, 9000);
        CHECK(dgx_get_pixel(scr, x, low - ah) == colors[bar_params.knob] && dgx_get_pixel(scr, x, low - ah + 1) != colors[bar_params.knob]);
        p.offset = 0;
        dgx_ui_paint(&p.ui, &paint, 9020);
        CHECK(dgx_get_pixel(scr, x, b->y + ah) == colors[bar_params.knob] && dgx_get_pixel(scr, x, b->y + ah - 1) != colors[bar_params.knob]);
    }
    dgx_ui_paint_free(&paint);
    dgx_screen_destroy(&scr), dgx_screen_destroy(&ref);
}

/* on the panel standing, the box from edge to edge: moved by its own scroll; lying: drawn again. No pixel twice */
static void on_panel(bool lying)
{
    static page_t p;
    dgx_screen_t *scr = lcd_make(lying), *ref = dgx_vscreen_init(lying ? HEIGHT : WIDTH, lying ? WIDTH : HEIGHT, 16, DgxScreenRGB);
    CHECK(scr != NULL);
    if (!scr) return;
    make(&p, &filled, 20, lying ? 150 : VIEW, true);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 6};
    dgx_fill_rectangle(scr, 0, 0, scr->width, scr->height, 0);
    dgx_ui_paint(&p.ui, &paint, 0);
    int wrong = 0, twice = 0, by_panel = 0, too_many = 0;
    for (int round = 0; round < 300; ++round) {
        const int32_t was = p.offset;
        const int     vsp = lcd_vsp, what = act(&p);
        lcd_count_from_now();
        dgx_ui_paint(&p.ui, &paint, (uint32_t)round * 20);
        fresh(&p, ref);
        const int d = lcd_mismatches(scr, ref);
        if (d && !wrong) fprintf(stderr, "  panel %s: round %d: %d pixels differ, offset %d\n", lying ? "lying" : "standing", round, d, (int)p.offset);
        wrong += d > 0, twice += lcd_twice() > 0;
        if (lcd_vsp != vsp) {
            ++by_panel;
            const int moved = p.offset > was ? p.offset - was : was - p.offset, bar = bar_params.width;
            if (what <= 6 && lcd_pixels > (long)moved * WIDTH + (long)bar * VIEW) ++too_many; /* the rows that came in and the bar */
        }
    }
    CHECK(wrong == 0 && twice == 0 && too_many == 0 && lcd_bad == 0);
    CHECK(lying ? by_panel == 0 : by_panel > 100);
    if (!lying) fprintf(stderr, "  standing: %d of 300 passes moved by the panel\n", by_panel);
    dgx_ui_paint_free(&paint);
    dgx_screen_destroy(&scr), dgx_screen_destroy(&ref);
}

/* ---- the panel's area ---- */

/* a page drawn on the panel standing, scrolled; each frame against a fresh paint. by_panel: the passes it moved */
static int frames(page_t *p, dgx_ui_paint_t *paint, dgx_screen_t *ref, int passes, uint32_t *ms, int *wrong)
{
    int by_panel = 0;
    for (int k = 0; k < passes; ++k) {
        const int vsp = lcd_vsp;
        p->offset += k % 3 ? 7 : -5;
        lcd_count_from_now();
        dgx_ui_paint(&p->ui, paint, *ms += 20);
        fresh(p, ref);
        *wrong += lcd_mismatches(paint->scr, ref) > 0 || lcd_twice() > 0;
        by_panel += lcd_vsp != vsp;
    }
    return by_panel;
}

static void area_let_go(void)
{
    static page_t a, b, two;
    dgx_screen_t *scr = lcd_make(false), *ref = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
    if (!scr) return;
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 6};
    uint32_t       ms = 0;
    int            wrong = 0;
    /* page A's box moved by the panel; then page B, its box elsewhere: A's area let go, B's moved by the panel */
    make(&a, &filled, 20, VIEW, true), make(&b, &filled, 60, 150, true);
    dgx_fill_rectangle(scr, 0, 0, WIDTH, HEIGHT, 0);
    dgx_ui_paint(&a.ui, &paint, ms);
    CHECK(frames(&a, &paint, ref, 12, &ms, &wrong) > 6 && paint.scroll_held);
    dgx_ui_redraw(&b.ui, 0);
    CHECK(frames(&b, &paint, ref, 12, &ms, &wrong) >= 11); /* all but its first, drawn whole: A's area let go at once */
    /* A again, and B's box hidden while it holds: let go, drawn whole, A moved */
    dgx_ui_redraw(&a.ui, 0);
    CHECK(frames(&a, &paint, ref, 6, &ms, &wrong) >= 5);
    dgx_ui_show(&a.ui, a.box, false);
    dgx_ui_paint(&a.ui, &paint, ms += 20);
    fresh(&a, ref);
    wrong += lcd_mismatches(scr, ref) > 0;
    dgx_ui_show(&a.ui, a.box, true);
    CHECK(frames(&a, &paint, ref, 6, &ms, &wrong) > 3);
    /* two boxes of one page from edge to edge, scrolled in turn: the second needs the area, the first lets it go at the
       next pass; both move by the panel in time, every frame right */
    make(&two, &filled, 10, 120, true);
    static dgx_ui_widget_t second;
    second = two.widgets[2], second.want_h = 120, second.value = &two.level;
    const int other = dgx_ui_add(&two.ui, 0, &second);
    static dgx_ui_widget_t rows[ROWS];
    for (int k = 0; k < ROWS; ++k) rows[k] = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .want_h = 18, .text = &two.boxes[k]}, dgx_ui_add(&two.ui, other, &rows[k]);
    dgx_ui_redraw(&two.ui, 0);
    dgx_ui_paint(&two.ui, &paint, ms += 20);
    int first_moved = 0, second_moved = 0;
    for (int k = 0; k < 24; ++k) {
        const int vsp = lcd_vsp;
        if ((k / 4) % 2) two.level += 6;
        else two.offset += 6;
        lcd_count_from_now();
        dgx_ui_paint(&two.ui, &paint, ms += 20);
        fresh(&two, ref);
        wrong += lcd_mismatches(scr, ref) > 0;
        if (lcd_vsp != vsp) ((k / 4) % 2 ? ++second_moved : ++first_moved);
    }
    CHECK(first_moved >= 6 && second_moved >= 6);
    /* a box of neither, a label over the scrolling box's rows: never moved, the label not carried with it */
    {
        static page_t free_page;
        make(&free_page, &filled, 20, VIEW, true);
        free_page.widgets[0].flags = 0, free_page.widgets[2].at_y = 20, free_page.widgets[2].want_w = WIDTH;
        static dgx_ui_widget_t over;
        over = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .at_x = 40, .at_y = 80, .want_w = 100, .want_h = 18, .text = &free_page.boxes[ROWS + 1]};
        dgx_ui_textbox_init(&free_page.boxes[ROWS + 1], free_page.texts[ROWS + 1], 24, 1, &style);
        dgx_ui_text_set(&free_page.ui, dgx_ui_add(&free_page.ui, 0, &over), "over it");
        dgx_ui_redraw(&free_page.ui, 0);
        dgx_ui_paint(&free_page.ui, &paint, ms += 20);
        int moved = 0;
        for (int k = 0; k < 6; ++k) {
            const int vsp = lcd_vsp;
            free_page.offset += 7;
            dgx_ui_paint(&free_page.ui, &paint, ms += 20);
            moved += lcd_vsp != vsp;
        }
        CHECK(moved == 0);
    }
    /* the paint freed while a box holds the area: let go, the panel shows its memory in order */
    dgx_ui_redraw(&two.ui, 0);
    dgx_ui_paint(&two.ui, &paint, ms += 20);
    two.offset += 6;
    dgx_ui_paint(&two.ui, &paint, ms += 20);
    CHECK(paint.scroll_held && lcd_vsp != lcd_tfa);
    dgx_ui_paint_free(&paint);
    CHECK(!paint.scroll_held && lcd_vsp == lcd_tfa);
    CHECK(wrong == 0 && lcd_bad == 0);
    dgx_screen_destroy(&scr), dgx_screen_destroy(&ref);
}

/* ---- popups, a teller, a drawn background, a mirror ---- */

static dgx_ui_rect_t told[64];
static int           told_count;
static void          tell(void *ctx, dgx_ui_rect_t rect)
{
    (void)ctx;
    if (told_count < 64) told[told_count++] = rect;
}

static dgx_screen_t *background;
static void          draw_background(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    (void)ctx;
    dgx_vscreen_region_to_screen(paint->scr, x - paint->ox, y - paint->oy, background, x, y, w, h, 0);
}

static void covers_and_layers(void)
{
    static page_t p, pop;
    dgx_screen_t *scr = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB), *ref = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB),
                 *mirror = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
    make(&p, &filled, 20, VIEW, true);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 6}, second = {.scr = mirror, .colors = colors, .color_count = 6};
    dgx_ui_paint(&p.ui, &paint, 0);
    int wrong = 0;
    /* a popup over the lower half of the box's rows: scrolled under it, the popup as it was */
    make(&pop, &filled, 10, 40, false);
    CHECK(dgx_ui_popup_open(&p.ui, &pop.ui, 20, 140, 200, 70, 0));
    for (int k = 0; k < 10; ++k) {
        p.offset += 9;
        dgx_ui_paint(&p.ui, &paint, 20u * (uint32_t)k);
        fresh(&p, ref);
        wrong += differ(scr, ref) > 0;
        /* the mirror composed by a second paint, as a recipe does it */
        dgx_ui_paint_area_to(&p.ui, &second, mirror, NULL, 0, 0, WIDTH, HEIGHT);
        wrong += differ(mirror, ref) > 0;
    }
    dgx_ui_popup_close(&p.ui);
    dgx_ui_paint(&p.ui, &paint, 300);
    fresh(&p, ref);
    wrong += differ(scr, ref) > 0;
    /* under a teller: the rectangles told cover the rows; composed from them, the picture as a fresh one */
    p.offset = 0;
    dgx_ui_paint(&p.ui, &paint, 390);
    for (int k = 0; k < 6; ++k) {
        p.offset += 11;
        told_count = 0;
        dgx_ui_paint_changes(&p.ui, &paint, 400u + 20u * (uint32_t)k, tell, NULL);
       
        const dgx_ui_node_t *n = dgx_ui_node(&p.ui, p.box);
        bool                 covered = false;
        for (int t = 0; t < told_count; ++t)
            covered |= told[t].x <= n->x && told[t].y <= n->y + PAD && told[t].x + told[t].w >= n->x + n->w && told[t].y + told[t].h >= n->y + n->h - PAD;
        CHECK(covered);
        for (int t = 0; t < told_count; ++t) dgx_ui_paint_area(&p.ui, &paint, told[t].x, told[t].y, told[t].w, told[t].h);
        fresh(&p, ref);
        wrong += differ(scr, ref) > 0;
    }
    /* on a drawn background: a box that does not fill is drawn again; one that fills moves */
    background = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x) dgx_set_pixel(background, x, y, (uint32_t)((x * 7 + y * 13) & 0xffff));
    for (int clear_box = 0; clear_box < 2; ++clear_box) {
        make(&p, clear_box ? &clear : &filled, 20, VIEW, true);
        dgx_ui_paint_t on_picture = {.scr = scr, .colors = colors, .color_count = 6};
        dgx_ui_background_set(&on_picture, draw_background, NULL);
        dgx_ui_paint(&p.ui, &on_picture, 0);
        for (int k = 0; k < 8; ++k) {
            p.offset += 13;
            dgx_ui_paint(&p.ui, &on_picture, 20u * (uint32_t)k);
            dgx_ui_paint_t fresh_paint = {.scr = ref, .colors = colors, .color_count = 6};
            dgx_ui_background_set(&fresh_paint, draw_background, NULL);
            dgx_ui_paint_area_to(&p.ui, &fresh_paint, ref, NULL, 0, 0, WIDTH, HEIGHT);
            dgx_ui_paint_free(&fresh_paint);
            wrong += differ(scr, ref) > 0;
        }
        dgx_ui_paint_free(&on_picture);
    }
    dgx_screen_destroy(&background);
    CHECK(wrong == 0);
    dgx_ui_paint_free(&paint), dgx_ui_paint_free(&second);
    dgx_screen_destroy(&scr), dgx_screen_destroy(&ref), dgx_screen_destroy(&mirror);
}

/* ---- the finger, the keys, the focus ---- */

static bool within_box(page_t *p, int i);

static void finger(page_t *p, int x, int y, int kind, uint32_t ms)
{
    dgx_ui_pointer(&p->ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .down = kind != 2, .pressed = kind == 0, .released = kind == 2, .ms = ms});
}

/* wholly within the box's inside */
static bool in_view(page_t *p, int i)
{
    dgx_ui_layout(&p->ui);
    const dgx_ui_node_t *b = dgx_ui_node(&p->ui, p->box), *k = dgx_ui_node(&p->ui, i);
    return k->y >= b->y && k->y + k->h <= b->y + b->h;
}

static void hands_and_keys(void)
{
    static page_t p;
    make(&p, &filled, 20, VIEW, true);
    dgx_ui_set_root(&p.ui, 0, 0, WIDTH, HEIGHT);
    dgx_ui_layout(&p.ui);
    const dgx_ui_node_t *b = dgx_ui_node(&p.ui, p.box);
    const int            mid = b->x + 60;
    /* a drag on a label: the rows follow the finger from 4 points on; no click, told CHANGED */
    const dgx_ui_node_t *label = dgx_ui_node(&p.ui, p.row[0]);
    finger(&p, mid, label->y + 5, 0, 0);
    finger(&p, mid, label->y + 3, 1, 10);
    CHECK(p.offset == 0);
    finger(&p, mid, label->y - 25, 1, 20);
    CHECK(p.offset == 30 && p.changed > 0);
    finger(&p, mid, label->y - 25, 2, 30);
    /* a button pressed, then up first: the box takes the press, the button let go unclicked */
    dgx_ui_layout(&p.ui);
    const dgx_ui_node_t *button = dgx_ui_node(&p.ui, p.row[3]);
    const int32_t        before = p.offset;
    finger(&p, mid, button->y + 10, 0, 100);
    CHECK(dgx_ui_node(&p.ui, p.row[3])->state.bits & DGX_UI_PRESSED);
    finger(&p, mid, button->y + 2, 1, 110);
    CHECK(!(dgx_ui_node(&p.ui, p.row[3])->state.bits & DGX_UI_PRESSED) && p.ui.pressed == p.box && p.offset == before + 8);
    finger(&p, mid, button->y + 2, 2, 120);
    /* the slider: sideways its own; up or down first the box's, its value given back */
    dgx_ui_set_value(&p.ui, p.box, 0), dgx_ui_layout(&p.ui);
    const dgx_ui_node_t *slider = dgx_ui_node(&p.ui, p.row[5]);
    finger(&p, slider->x + 10, slider->y + 8, 0, 200);
    finger(&p, slider->x + slider->w - 10, slider->y + 14, 1, 210); /* 6 down, far more across: the slider's */
    CHECK(p.level > 5 && p.offset == 0);
    finger(&p, slider->x + slider->w - 10, slider->y + 14, 2, 220);
    p.level = 2;
    dgx_ui_layout(&p.ui);
    finger(&p, slider->x + slider->w / 2, slider->y + 8, 0, 300);
    const int32_t level_then = p.level;
    finger(&p, slider->x + slider->w / 2 + 40, slider->y + 9, 1, 305); /* across first: the slider moves */
    CHECK(p.level != level_then);
    finger(&p, slider->x + slider->w / 2 + 1, slider->y - 12, 1, 310); /* then up, from where it went down: the box's */
    CHECK(p.level == level_then && p.offset == 20);
    finger(&p, slider->x + slider->w / 2, slider->y - 12, 2, 320);
    /* a button half above the box: hit within the box, not above it */
    dgx_ui_set_value(&p.ui, p.box, 0), dgx_ui_layout(&p.ui);
    dgx_ui_set_value(&p.ui, p.box, dgx_ui_node(&p.ui, p.row[3])->y - b->y + 6), dgx_ui_layout(&p.ui);
    CHECK(dgx_ui_node(&p.ui, p.row[3])->y < b->y && dgx_ui_hit(&p.ui, mid, b->y + 1) == p.row[3] && dgx_ui_hit(&p.ui, mid, b->y - 2) != p.row[3]);
    /* the bar: a tap by its lower end a step, below the knob a page; the knob dragged */
    dgx_ui_set_value(&p.ui, p.box, 0), dgx_ui_layout(&p.ui);
    const int bar_x = b->x + b->w - PAD - 2, top = b->y, bottom = b->y + b->h - 1;
    finger(&p, bar_x, bottom, 0, 400), finger(&p, bar_x, bottom, 2, 410);
    const int32_t stepped = p.offset;
    CHECK(stepped > 0 && stepped < VIEW / 2);
    finger(&p, bar_x, bottom - 40, 0, 500), finger(&p, bar_x, bottom - 40, 2, 510);
    const int32_t paged = p.offset;
    dgx_ui_set_value(&p.ui, p.box, 100000), dgx_ui_layout(&p.ui);
    const int32_t most = p.offset;
    CHECK(paged == (stepped + VIEW < most ? stepped + VIEW : most)); /* a page: the box's height, or to the end */
    dgx_ui_set_value(&p.ui, p.box, 0), dgx_ui_layout(&p.ui);
    finger(&p, bar_x, top + 20, 0, 600), finger(&p, bar_x, top + 60, 1, 610);
    CHECK(p.offset > 40);
    finger(&p, bar_x, top + 60, 2, 620);
    /* the focus on the first button, the rows dragged up past it: the focus to the last row wholly in view that takes it */
    dgx_ui_set_value(&p.ui, p.box, 0), dgx_ui_layout(&p.ui);
    dgx_ui_focus(&p.ui, p.row[1]);
    finger(&p, mid, b->y + 150, 0, 700), finger(&p, mid, b->y + 50, 1, 710), finger(&p, mid, b->y + 50, 2, 720);
    CHECK(p.offset == 100 && p.ui.focus != p.row[1] && within_box(&p, p.ui.focus) && in_view(&p, p.ui.focus));
    /* the keys: the focus goes down the rows, each brought into view; back up */
    dgx_ui_set_value(&p.ui, p.box, 0), dgx_ui_layout(&p.ui);
    int seen = 0;
    for (int k = 0; k < 12; ++k) {
        dgx_ui_key(&p.ui, DGX_UI_KEY_NEXT);
        if (p.ui.focus >= 0 && p.ui.focus != p.box && within_box(&p, p.ui.focus)) seen += in_view(&p, p.ui.focus) ? 1 : -100;
    }
    CHECK(seen > 4 && p.offset > 0);
    for (int k = 0; k < 12; ++k) {
        dgx_ui_key(&p.ui, DGX_UI_KEY_PREV);
        if (p.ui.focus >= 0 && within_box(&p, p.ui.focus)) CHECK(in_view(&p, p.ui.focus));
    }
    /* dgx_ui_scroll_to(): the last row, the first; a row shown from hidden out of view stays out until focused */
    dgx_ui_scroll_to(&p.ui, p.row[ROWS - 1]);
    CHECK(in_view(&p, p.row[ROWS - 1]));
    dgx_ui_scroll_to(&p.ui, p.row[0]);
    CHECK(in_view(&p, p.row[0]) && p.offset == PAD); /* the least move: its top at the box's top, the pad above it out */
    dgx_ui_show(&p.ui, p.row[ROWS - 2], false), dgx_ui_layout(&p.ui);
    dgx_ui_show(&p.ui, p.row[ROWS - 2], true);
    CHECK(!in_view(&p, p.row[ROWS - 2]) && p.offset == PAD);
    dgx_ui_focus(&p.ui, p.row[ROWS - 1]);
    CHECK(in_view(&p, p.row[ROWS - 1]));
    /* an offset past the end comes back clamped, nobody told; the rows fewer: clamped again, untold */
    const int changed = p.changed;
    p.offset = 5000;
    dgx_ui_relayout(&p.ui), dgx_ui_layout(&p.ui);
    CHECK(p.offset > 0 && p.offset < 5000 && p.changed == changed);
    for (int k = 4; k < ROWS; ++k) dgx_ui_show(&p.ui, p.row[k], false);
    dgx_ui_layout(&p.ui);
    CHECK(p.offset == 0 && p.changed == changed);
}

static bool within_box(page_t *p, int i)
{
    for (; i >= 0; i = dgx_ui_node(&p->ui, i)->parent)
        if (i == p->box) return true;
    return false;
}

/* ---- a box of text alone, a row taller than the view, the bar coming, the strips ---- */

static void alone_tall_bar_strips(void)
{
    static page_t p;
    /* labels only: the box takes the focus, next scrolls it a step at a time, past its end the focus goes on */
    make(&p, &filled, 20, VIEW, false);
    static const dgx_ui_widget_t after_w = {.kind = DGX_UI_BUTTON, .want_h = 20, .look = &dgx_ui_look_button};
    const int after = dgx_ui_add(&p.ui, 0, &after_w), slider = p.row[5];
    dgx_ui_set_state(&p.ui, slider, DGX_UI_DISABLED, true); /* nothing under the box takes the focus */
    dgx_ui_set_root(&p.ui, 0, 0, WIDTH, HEIGHT);
    dgx_ui_layout(&p.ui);
    dgx_ui_key(&p.ui, DGX_UI_KEY_NEXT);
    CHECK(p.ui.focus == p.box);
    int steps = 0;
    while (p.ui.focus == p.box && steps < 40) dgx_ui_key(&p.ui, DGX_UI_KEY_NEXT), dgx_ui_layout(&p.ui), ++steps;
    CHECK(steps > 3 && p.ui.focus == after && p.offset > 0);
    dgx_ui_set_state(&p.ui, slider, DGX_UI_DISABLED, false); /* a child that takes it: the box no longer does */
    dgx_ui_focus(&p.ui, p.box);
    CHECK(p.ui.focus != p.box);
    /* a row taller than the view: brought with its top at the top */
    static dgx_ui_widget_t tall;
    tall = (dgx_ui_widget_t){.kind = DGX_UI_BUTTON, .want_h = VIEW + 40, .look = &dgx_ui_look_button};
    const int t = dgx_ui_add(&p.ui, p.box, &tall);
    dgx_ui_layout(&p.ui);
    dgx_ui_scroll_to(&p.ui, t);
    CHECK(dgx_ui_node(&p.ui, t)->y == dgx_ui_node(&p.ui, p.box)->y);
    /* few rows, no bar; rows shown in a pass that also scrolls: the bar comes, the rows narrower, all as a fresh paint */
    dgx_screen_t *scr = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB), *ref = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
    make(&p, &filled, 20, VIEW, true);
    for (int k = 6; k < ROWS; ++k) dgx_ui_show(&p.ui, p.row[k], false);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 6};
    dgx_ui_paint(&p.ui, &paint, 0);
    CHECK(dgx_ui_node(&p.ui, p.row[0])->w == WIDTH - 2 * PAD);
    for (int k = 6; k < ROWS; ++k) dgx_ui_show(&p.ui, p.row[k], true);
    p.offset = 25;
    dgx_ui_paint(&p.ui, &paint, 20);
    fresh(&p, ref);
    CHECK(differ(scr, ref) == 0 && dgx_ui_node(&p.ui, p.row[0])->w < WIDTH - 2 * PAD);
    dgx_ui_paint_free(&paint);
    dgx_screen_destroy(&scr), dgx_screen_destroy(&ref);
    /* on a panel of 18 bits through the strips: each pass as a fresh composing on another such panel, no pixel twice */
    panel_t *shown = panel_make(), *again = panel_make();
    static uint8_t buffer[8192];
    make(&p, &filled, 20, VIEW, true);
    dgx_ui_paint_t strips = {.scr = &shown->sbus.scr, .colors = colors, .color_count = 6, .buffer = buffer, .buffer_size = sizeof buffer, .strips = &dgx_ui_strips};
    dgx_ui_paint(&p.ui, &strips, 0);
    int wrong = 0, twice = 0;
    for (int k = 0; k < 40; ++k) {
        act(&p);
        panel_count_from_now(shown);
        dgx_ui_paint(&p.ui, &strips, 20u * (uint32_t)k);
        twice += panel_twice(shown) > 0;
        dgx_ui_paint_t fresh_strips = {.scr = &again->sbus.scr, .colors = colors, .color_count = 6, .buffer = buffer, .buffer_size = sizeof buffer, .strips = &dgx_ui_strips};
        dgx_ui_paint_area_to(&p.ui, &fresh_strips, &again->sbus.scr, NULL, 0, 0, WIDTH, HEIGHT);
        dgx_ui_paint_free(&fresh_strips);
        wrong += memcmp(shown->mem, again->mem, sizeof shown->mem) != 0;
    }
    CHECK(wrong == 0 && twice == 0);
    dgx_ui_paint_free(&strips);
    free(shown), free(again);
}

int main(void)
{
    on_vscreen();
    on_panel(false);
    on_panel(true);
    area_let_go();
    covers_and_layers();
    hands_and_keys();
    alone_tall_bar_strips();
    CHECK_DONE();
}
