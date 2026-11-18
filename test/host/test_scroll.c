/*
 * A list of more rows than its box takes: a window of them from its text box's top, as many as go in at a line of its
 * text, and a bar on its right edge. The twelve months in four lines: the window at every key, each state equal to a
 * list of four rows of the same lines drawn by hand, the window moved writing every pixel of the box once; a finger
 * put down picks nothing, moved up or down moves the pick a row for each row's height as DGX-GE does, the window
 * following, and let go without a move picks the row it went down on; dgx_ui_set_value() far down, the window at it;
 * dgx_ui_text_scroll() moves the window and not the pick; min other than 0; a box lower than a line; the text set anew
 * with fewer lines; a value set while a finger scrolls; the bar seen in a frame; a dropdown of twenty options on a
 * small screen, its window at the option chosen.
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800}; /* paper, ink, a fill, the accent: the pick and the bar */

static const char *const months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static int                   told[16], told_n;

static void on_list(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (told_n < 16 && (what == DGX_UI_CHANGED || what == DGX_UI_ACTIVATED)) told[told_n++] = what; /* the focus not counted */
}

static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN | DGX_UI_REST, .pad = 2};

/* a tree of a list, in a frame or not */
typedef struct {
    dgx_ui_node_t    nodes[4];
    dgx_ui_t         ui;
    char             buf[160];
    dgx_ui_textbox_t box;
    dgx_ui_widget_t  w;
    int              i;
} tree_t;

static void make(tree_t *t, const char *text, int min, int max, int want_w, int want_h, bool framed)
{
    memset(t, 0, sizeof *t);
    dgx_ui_textbox_init(&t->box, t->buf, sizeof t->buf, 1, &style);
    t->w = (dgx_ui_widget_t){.kind = DGX_UI_LIST, .text = &t->box, .min = (int16_t)min, .max = (int16_t)max, .want_w = (int16_t)want_w,
                             .want_h = (int16_t)want_h, .look = &dgx_ui_look_list, .on = on_list};
    dgx_ui_init(&t->ui, t->nodes, 4, 1);
    dgx_ui_add(&t->ui, -1, &root_w);
    const int holder = framed ? dgx_ui_add(&t->ui, 0, &frame_w) : 0;
    t->i             = dgx_ui_add(&t->ui, holder, &t->w);
    dgx_ui_text_set(&t->ui, t->i, text);
}

/* the lines from..from + n - 1 of the months, joined */
static const char *lines(int from, int n)
{
    static char s[160];
    s[0] = 0;
    for (int k = 0; k < n; ++k) strcat(strcat(s, k ? "\n" : ""), months[from + k]);
    return s;
}

/* the rows of the scrolled list on the panel, as wide as w, against the reference's on a virtual screen */
static int rows_differ(const panel_t *p, const dgx_ui_node_t *n, dgx_screen_t *v, const dgx_ui_node_t *r, int w)
{
    int d = 0;
    for (int y = 0; y < n->h; ++y)
        for (int x = 0; x < w; ++x) d += p->mem[n->y + y][n->x + x] != dgx_get_pixel(v, r->x + x, r->y + y);
    return d;
}

static void finger(dgx_ui_t *ui, int x, int y, bool pressed, bool released, uint32_t ms)
{
    dgx_ui_pointer(ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .down = !released, .pressed = pressed, .released = released, .ms = ms});
}

int main(void)
{
    variant[0]     = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    style          = (dgx_ui_text_style_t){.variants = variant, .count = 1};
    const int line = dgx_ui_variant_line(&variant[0]);
    panel_t       *panel = panel_make();
    dgx_screen_t  *vr    = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    static tree_t  s, r;
    make(&s, lines(0, 12), 0, 11, 0, 4 * line, false);
    dgx_ui_paint_t ps = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
    dgx_ui_paint_t pr = {.scr = vr, .colors = colors, .color_count = 4};
    panel_count_from_now(panel);
    dgx_ui_paint(&s.ui, &ps, 0);
    const dgx_ui_node_t *n  = dgx_ui_node(&s.ui, s.i);
    const int            rw = n->w - 2, rh = n->h / 4;
    CHECK(panel_twice(panel) == 0 && s.box.top == 0 && rh == line);
    /* the reference: a list of four rows of the lines shown, as wide as the rows, its pick where the window has it */
    make(&r, lines(0, 4), 0, 3, rw, 4 * line, false);
    dgx_ui_paint(&r.ui, &pr, 0);
    const dgx_ui_node_t *rn = dgx_ui_node(&r.ui, r.i);
    CHECK(rn->w == rw && rows_differ(panel, n, vr, rn, rw) == 0);
    /* the bar: a third of the height at the top, the paper under it */
    {
        int off = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x + rw; x < n->x + n->w; ++x) off += panel->mem[y][x] != (y < n->y + n->h * 4 / 12 ? colors[3] : colors[0]);
        CHECK(off == 0);
    }

    /* the keys: NEXT to May, the window a row down at the fifth; the pick moved within it, the two rows; the window
       moved, all of the box once */
    dgx_ui_focus(&s.ui, s.i), dgx_ui_focus(&r.ui, r.i);
    for (int k = 1; k <= 5; ++k) {
        panel_count_from_now(panel);
        CHECK(dgx_ui_key(&s.ui, DGX_UI_KEY_NEXT));
        dgx_ui_paint(&s.ui, &ps, (uint32_t)k);
        const int top = k < 4 ? 0 : k - 3;
        CHECK(n->state_value.value == k && s.box.top == top);
        if (k >= 4) CHECK(panel_twice(panel) == 0 && panel_written(panel) == n->w * n->h);
        dgx_ui_text_set(&r.ui, r.i, lines(top, 4)), dgx_ui_set_value(&r.ui, r.i, k - top);
        dgx_ui_paint(&r.ui, &pr, (uint32_t)k);
        CHECK(rows_differ(panel, n, vr, rn, rw) == 0);
    }
    dgx_ui_focus(&s.ui, -1), dgx_ui_focus(&r.ui, -1);
    dgx_ui_paint(&s.ui, &ps, 10);

    /* a finger put down on the second row: nothing picked; up half a row and on, the pick a row along for each row's
       height (up: on down the list), the window following; let go, no click */
    told_n = 0;
    const int fx = n->x + 5, fy = n->y + rh + rh / 2, half = (rh + 1) / 2; /* half a row, rounded up: DGX-GE's 2 * ay >= rh */
    finger(&s.ui, fx, fy, true, false, 100);
    CHECK(n->state_value.value == 5 && told_n == 0);
    finger(&s.ui, fx, fy - half + 1, false, false, 110); /* short of half a row: nothing yet */
    CHECK(n->state_value.value == 5);
    finger(&s.ui, fx, fy - half, false, false, 120);
    CHECK(n->state_value.value == 6 && s.box.top == 3);
    finger(&s.ui, fx, fy - 2 * rh, false, false, 140);
    CHECK(n->state_value.value == 7 && s.box.top == 4);
    finger(&s.ui, fx, fy + rh, false, false, 160); /* back down past where it went down: back up the list */
    CHECK(n->state_value.value == 4 && s.box.top == 4);
    finger(&s.ui, fx, fy + rh, false, true, 180);
    CHECK(told_n == 3 && told[0] == DGX_UI_CHANGED && told[2] == DGX_UI_CHANGED);
    /* a tap on the third row shown: picked, and activated */
    told_n = 0;
    finger(&s.ui, fx, n->y + 2 * rh + rh / 2, true, false, 1000);
    finger(&s.ui, fx, n->y + 2 * rh + rh / 2, false, true, 1100);
    CHECK(n->state_value.value == 6 && told_n == 2 && told[0] == DGX_UI_CHANGED && told[1] == DGX_UI_ACTIVATED);
    dgx_ui_focus(&s.ui, -1); /* the finger brought the focus: the reference has none */
    dgx_ui_paint(&s.ui, &ps, 11);

    /* the value set far down, composed first by another paint (a mirror of the screen), which stores nothing: the
       panel's pass after it moves its window, all of the box drawn */
    dgx_ui_set_value(&s.ui, s.i, 11);
    {
        const int      before = s.box.top;
        dgx_screen_t  *m      = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t pm     = {.scr = m, .colors = colors, .color_count = 4};
        dgx_ui_paint_area(&s.ui, &pm, 0, 0, PANEL_W, PANEL_H);
        CHECK(before != 8 && s.box.top == before);
        dgx_ui_paint_free(&pm);
        m->destroy(&m);
    }
    dgx_ui_paint(&s.ui, &ps, 12);
    CHECK(s.box.top == 8);
    {
        /* the bar at the bottom: its third of the height from 8/12 of it */
        int off = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x + rw; x < n->x + n->w; ++x) off += panel->mem[y][x] != (y >= n->y + n->h * 8 / 12 ? colors[3] : colors[0]);
        CHECK(off == 0);
    }
    dgx_ui_text_set(&r.ui, r.i, lines(8, 4)), dgx_ui_set_value(&r.ui, r.i, 3);
    dgx_ui_paint(&r.ui, &pr, 12);
    CHECK(rows_differ(panel, n, vr, rn, rw) == 0);
    /* the window scrolled by hand: up five, the pick out of it; past the end, at the end */
    dgx_ui_text_scroll(&s.ui, s.i, -5);
    dgx_ui_paint(&s.ui, &ps, 13);
    {
        int picked = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + rw; ++x) picked += panel->mem[y][x] == colors[3];
        CHECK(s.box.top == 3 && n->state_value.value == 11 && picked == 0);
    }
    dgx_ui_text_scroll(&s.ui, s.i, 100);
    CHECK(s.box.top == 8);
    /* the text set anew with two lines, the window far down: the rows past them paper */
    dgx_ui_text_set(&s.ui, s.i, "A\nB");
    dgx_ui_set_value(&s.ui, s.i, 9);
    dgx_ui_paint(&s.ui, &ps, 14);
    {
        int ink = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + rw; ++x) ink += panel->mem[y][x] == colors[1];
        CHECK(s.box.top == 8 && ink == 0);
    }
    /* a value set while a finger scrolls: the window at it; the finger moved on, from where it went down */
    dgx_ui_text_set(&s.ui, s.i, lines(0, 12));
    dgx_ui_set_value(&s.ui, s.i, 2);
    dgx_ui_paint(&s.ui, &ps, 15);
    finger(&s.ui, fx, fy, true, false, 2000);
    finger(&s.ui, fx, fy - rh, false, false, 2020);
    CHECK(n->state_value.value == 3);
    dgx_ui_set_value(&s.ui, s.i, 10);
    dgx_ui_paint(&s.ui, &ps, 2030); /* the window brought to it when drawn */
    CHECK(s.box.top == 7);
    finger(&s.ui, fx, fy - 2 * rh, false, false, 2040);
    CHECK(n->state_value.value == 4 && s.box.top == 4);
    finger(&s.ui, fx, fy - 2 * rh, false, true, 2060);
    dgx_ui_paint(&s.ui, &ps, 16);
    dgx_ui_paint_free(&ps), dgx_ui_paint_free(&pr);
    dgx_ui_free(&s.ui), dgx_ui_free(&r.ui);

    /* min other than 0: 5..16, the value 12 set before the layout, the window at it after */
    {
        static tree_t  m;
        dgx_screen_t  *v  = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4};
        make(&m, lines(0, 12), 5, 16, 0, 4 * line, false);
        dgx_ui_set_value(&m.ui, m.i, 12);
        dgx_ui_paint(&m.ui, &pv, 0);
        CHECK(m.box.top == 4);
        /* a box lower than a line: one row shown, the list scrolling, the window at the pick */
        static tree_t low;
        make(&low, lines(0, 12), 0, 11, 0, line / 2, false);
        dgx_ui_set_value(&low.ui, low.i, 7);
        dgx_ui_paint(&low.ui, &pv, 0);
        CHECK(low.box.top == 7);
        /* a step of 20 points over a line of 13: two rows of the box of four lines, the window going at the third */
        static tree_t big;
        make(&big, lines(0, 12), 0, 11, 0, 4 * line, false);
        big.w.step = 20;
        dgx_ui_set_value(&big.ui, big.i, 2);
        dgx_ui_paint(&big.ui, &pv, 0);
        CHECK(big.box.top == 1);
        /* grown with its window at the end: the window kept within the rows, its first row the sixth */
        static tree_t grow;
        static const dgx_ui_widget_t rest_root = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        memset(&grow, 0, sizeof grow);
        dgx_ui_textbox_init(&grow.box, grow.buf, sizeof grow.buf, 1, &style);
        grow.w = (dgx_ui_widget_t){.kind = DGX_UI_LIST, .flags = DGX_UI_REST, .text = &grow.box, .max = 11, .look = &dgx_ui_look_list, .on = on_list};
        dgx_ui_init(&grow.ui, grow.nodes, 4, 1);
        dgx_ui_add(&grow.ui, -1, &rest_root);
        grow.i = dgx_ui_add(&grow.ui, 0, &grow.w);
        dgx_ui_text_set(&grow.ui, grow.i, lines(0, 12));
        dgx_ui_set_root(&grow.ui, 0, 0, PANEL_W, 4 * line);
        dgx_ui_set_value(&grow.ui, grow.i, 11);
        dgx_ui_paint(&grow.ui, &pv, 0);
        CHECK(grow.box.top == 8);
        dgx_ui_set_root(&grow.ui, 0, 0, PANEL_W, 6 * line);
        dgx_ui_paint(&grow.ui, &pv, 1);
        const dgx_ui_node_t *gn = dgx_ui_node(&grow.ui, grow.i);
        finger(&grow.ui, gn->x + 5, gn->y + line / 2, true, false, 3000), finger(&grow.ui, gn->x + 5, gn->y + line / 2, false, true, 3050);
        CHECK(gn->h == 6 * line && gn->state_value.value == 6);
        dgx_ui_free(&big.ui), dgx_ui_free(&grow.ui);
        /* in a frame: the bar of the colour 3 seen on its fill 2 */
        static tree_t f;
        make(&f, lines(0, 12), 0, 11, 0, 4 * line, true);
        dgx_ui_paint(&f.ui, &pv, 0);
        const dgx_ui_node_t *fn = dgx_ui_node(&f.ui, f.i);
            CHECK(dgx_get_pixel(v, fn->x + fn->w - 1, fn->y) == colors[3] && dgx_get_pixel(v, fn->x + fn->w - 1, fn->y + fn->h - 1) == colors[2]);
        dgx_ui_paint_free(&pv);
        dgx_ui_free(&m.ui), dgx_ui_free(&low.ui), dgx_ui_free(&f.ui);
        v->destroy(&v);
    }

    /* a dropdown of twenty options, 12 high at the top of a screen of 64: under it, as many rows as the room under it
       takes, three of 13 (a line), never lying over it; its window at the one chosen */
    {
        static char                  buf[160];
        static dgx_ui_textbox_t      box;
        static dgx_ui_node_t         nodes[2];
        static dgx_ui_t              ui;
        static dgx_ui_dropdown_t     dd;
        static const dgx_ui_widget_t col = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        dgx_ui_textbox_init(&box, buf, sizeof buf, 1, &style);
        const dgx_ui_widget_t drop = {.kind = DGX_UI_DROPDOWN, .text = &box, .min = 0, .max = 19, .want_h = 12, .state = {.value = 15},
                                      .look = &dgx_ui_look_dropdown};
        dgx_ui_init(&ui, nodes, 2, 1);
        dgx_ui_add(&ui, -1, &col);
        const int d = dgx_ui_add(&ui, 0, &drop);
        strcpy(buf, "o0\no1\no2\no3\no4\no5\no6\no7\no8\no9\no10\no11\no12\no13\no14\no15\no16\no17\no18\no19");
        dgx_ui_screen(&ui, PANEL_W, PANEL_H, 0);
        dgx_ui_layout(&ui);
        CHECK(dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
        dgx_screen_t  *v  = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_paint(&ui, &pv, 0); /* the list drawn: its window brought to the option chosen */
        const dgx_ui_node_t *ln = dgx_ui_node(&dd.tree, 1);
        CHECK(dd.tree.root_rect.y == 12 && ln->h == 3 * line && box.top == 13 && dgx_ui_node(&dd.tree, 1)->state_value.value == 15);
        dgx_ui_popup_close(&ui);
        dgx_ui_paint_free(&pv);
        dgx_ui_free(&ui);
        v->destroy(&v);
    }

    vr->destroy(&vr);
    free(panel);
    CHECK_DONE();
}
