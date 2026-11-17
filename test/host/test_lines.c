/*
 * The look of lines: a text laid word by word into the rows of its box, against the lines drawn one by one by the
 * font; a text changed on the counting panel writing every pixel once, within the box; an important text scrolled
 * by time; a text not important cut; letters appearing where they will stand; the rows composed in strips.
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_font.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[2] = {0x0000, 0xffff};

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   plain, important, centred;
static char                  buf[200];
static dgx_ui_textbox_t      box;
static dgx_ui_widget_t       text_w; /* the program's RAM: its width is set by each case */
static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX};
static dgx_ui_node_t         nodes[2];
static dgx_ui_t              ui;

static int width(const char *s)
{
    const dgx_font_context_t m = {.font = TerminusTTFMedium12()};
    return dgx_font_bounds(&m, s, NULL, NULL);
}

/* a tree of one text of so many rows, so wide, at 4, 4 */
static int make(const dgx_ui_text_style_t *st, int rows, int w)
{
    dgx_ui_textbox_init(&box, buf, sizeof buf, rows, st);
    text_w = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &box, .look = &dgx_ui_look_lines, .want_w = (int16_t)w, .at_x = 4, .at_y = 4};
    dgx_ui_init(&ui, nodes, 2, 1);
    dgx_ui_add(&ui, -1, &root_w);
    return dgx_ui_add(&ui, 0, &text_w);
}

/* pixels of the box differing between two screens */
static int differ_box(dgx_screen_t *a, dgx_screen_t *b, const dgx_ui_node_t *n)
{
    int d = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) d += dgx_get_pixel(a, x, y) != dgx_get_pixel(b, x, y);
    return d;
}

/* the lines drawn one by one by the font, each a field of the box's width */
static void reference(dgx_screen_t *e, const dgx_ui_node_t *n, const char *const *lines, int count, dgx_font_align_t align)
{
    dgx_font_t *f = TerminusTTFMedium12();
    dgx_fill_rectangle(e, 0, 0, e->width, e->height, 0);
    for (int k = 0; k < count; ++k) {
        const dgx_font_context_t c = {.scr = e, .font = f, .color = 0xffff, .bg = 0, .field = n->w, .align = align};
        dgx_font_draw(&c, n->x, n->y - f->yOffsetLowest + k * f->yAdvance, lines[k]);
    }
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    plain      = (dgx_ui_text_style_t){.variants = variant, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    important  = (dgx_ui_text_style_t){.variants = variant, .count = 1, .align = DGX_FONT_ALIGN_LEFT, .flags = DGX_UI_TEXT_IMPORTANT, .scroll_ms = 100};
    centred    = (dgx_ui_text_style_t){.variants = variant, .count = 1, .align = DGX_FONT_ALIGN_CENTER};
    dgx_screen_t  *v = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), *e = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 2};

    /* laid word by word: "aa bb" goes into the width, "aa bb cc" does not */
    const int w = width("aa bb") + 1;
    CHECK(width("aa bb cc") > w);
    int t = make(&plain, 3, w);
    dgx_ui_text_set(&ui, t, "aa bb cc dd");
    dgx_fill_rectangle(v, 0, 0, PANEL_W, PANEL_H, 0);
    CHECK(!dgx_ui_paint(&ui, &pv, 0));
    const char *const two[] = {"aa bb", "cc dd", ""};
    reference(e, dgx_ui_node(&ui, t), two, 3, DGX_FONT_ALIGN_LEFT);
    CHECK(differ_box(v, e, dgx_ui_node(&ui, t)) == 0);
    /* a word wider than a line: by its letters; a break of the text a line of its own */
    dgx_ui_text_set(&ui, t, "abcdefghij\nx");
    dgx_ui_paint(&ui, &pv, 0);
    {
        char first[16] = {0};
        int  n         = 0;
        while (n < 10 && width((memcpy(first, "abcdefghij", (size_t)n + 1), first[n + 1] = 0, first)) <= w) ++n;
        first[n] = 0;
        char second[16];
        strcpy(second, "abcdefghij" + n);
        const char *const lines[] = {first, second, "x"};
        CHECK(n > 0 && n < 10 && width("abcdefghij" + n) <= w);
        reference(e, dgx_ui_node(&ui, t), lines, 3, DGX_FONT_ALIGN_LEFT);
        CHECK(differ_box(v, e, dgx_ui_node(&ui, t)) == 0);
    }
    dgx_ui_free(&ui);

    /* a text changed, on the panel: every pixel once, nothing outside the box */
    panel_t       *panel = panel_make();
    dgx_ui_paint_t pp    = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 2};
    t                    = make(&plain, 3, w);
    dgx_ui_text_set(&ui, t, "aa bb cc dd");
    dgx_ui_paint(&ui, &pp, 0);
    panel_count_from_now(panel);
    dgx_ui_text_set(&ui, t, "dd cc");
    dgx_ui_paint(&ui, &pp, 0);
    const dgx_ui_node_t *n = dgx_ui_node(&ui, t);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) > 0);
    CHECK(panel_written_outside(panel, n->x, n->y, n->x + n->w, n->y + n->h) == 0);
    dgx_ui_free(&ui);

    /* an important text of four lines in two rows: a line up every 100 ms, the first shown so long too */
    t = make(&important, 2, w);
    dgx_ui_text_set(&ui, t, "aa bb cc dd ee ff gg hh");
    CHECK(dgx_ui_paint(&ui, &pv, 1000) && box.top == 0);
    CHECK(dgx_ui_paint(&ui, &pv, 1099) && box.top == 0);
    dgx_ui_redraw(&ui, t);
    CHECK(dgx_ui_paint(&ui, &pv, 1100) && box.top == 1);
    dgx_ui_redraw(&ui, t);
    CHECK(!dgx_ui_paint(&ui, &pv, 1250) && box.top == 2); /* the last two lines: no more time asked */
    const char *const end[] = {"ee ff", "gg hh"};
    reference(e, dgx_ui_node(&ui, t), end, 2, DGX_FONT_ALIGN_LEFT);
    CHECK(differ_box(v, e, dgx_ui_node(&ui, t)) == 0);
    dgx_ui_free(&ui);

    /* called again for time only, the line on top as it was: nothing goes to the panel */
    t = make(&important, 2, w);
    dgx_ui_text_set(&ui, t, "aa bb cc dd ee ff gg hh");
    CHECK(dgx_ui_paint(&ui, &pp, 2000));
    panel_count_from_now(panel);
    CHECK(dgx_ui_paint(&ui, &pp, 2050) && panel_written(panel) == 0);
    CHECK(dgx_ui_paint(&ui, &pp, 2100) && panel_written(panel) > 0 && panel_twice(panel) == 0); /* a line up */
    dgx_ui_free(&ui);

    /* not important: cut, its last row ending in an ellipsis that goes in */
    t = make(&plain, 2, w);
    dgx_ui_text_set(&ui, t, "aa bb cc dd ee ff");
    dgx_ui_paint(&ui, &pv, 0);
    {
        const char last[] = "cc dd";
        char       cut[24];
        snprintf(cut, sizeof cut, "%s...", "cc dd");
        int len = 5;
        while (len > 0 && width(cut) > w) {
            --len;
            while (len > 0 && last[len - 1] == ' ') --len;
            snprintf(cut, sizeof cut, "%.*s...", len, "cc dd");
        }
        const char *const lines[] = {"aa bb", cut};
        reference(e, dgx_ui_node(&ui, t), lines, 2, DGX_FONT_ALIGN_LEFT);
        CHECK(differ_box(v, e, dgx_ui_node(&ui, t)) == 0);
    }
    dgx_ui_free(&ui);

    /* letters appearing, centred: those come stand where they stand when all have come */
    t = make(&centred, 1, 80);
    dgx_ui_text_set(&ui, t, "abcd");
    dgx_ui_text_appear(&ui, t, 10);
    CHECK(dgx_ui_paint(&ui, &pv, 500)); /* none yet */
    dgx_ui_redraw(&ui, t);
    CHECK(dgx_ui_paint(&ui, &pv, 520)); /* two of four */
    dgx_screen_t *partial = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_vscreen_copy(partial, v);
    dgx_ui_redraw(&ui, t);
    CHECK(!dgx_ui_paint(&ui, &pv, 540)); /* all: no more time */
    const dgx_ui_node_t *an   = dgx_ui_node(&ui, t);
    const int            lead = (an->w - width("abcd")) / 2, ab = width("ab");
    int                  off = 0, ink = 0;
    for (int y = an->y; y < an->y + an->h; ++y)
        for (int x = an->x + lead; x < an->x + lead + ab; ++x) {
            off += dgx_get_pixel(partial, x, y) != dgx_get_pixel(v, x, y);
            ink += dgx_get_pixel(partial, x, y) != 0;
        }
    CHECK(off == 0 && ink > 0);
    /* and "cd" not yet: paper where they will stand */
    int early = 0;
    for (int y = an->y; y < an->y + an->h; ++y)
        for (int x = an->x + lead + ab; x < an->x + an->w; ++x) early += dgx_get_pixel(partial, x, y) != 0;
    CHECK(early == 0);
    partial->destroy(&partial);
    dgx_ui_free(&ui);

    /* the rows composed in strips of 5 equal the box drawn whole */
    t = make(&plain, 3, w);
    dgx_ui_text_set(&ui, t, "aa bb cc dd ee");
    dgx_fill_rectangle(v, 0, 0, PANEL_W, PANEL_H, 0);
    dgx_ui_paint(&ui, &pv, 0);
    dgx_screen_t  *band = dgx_vscreen_init(PANEL_W, 5, 16, DgxScreenRGB);
    dgx_ui_paint_t pb   = {.scr = band, .colors = colors, .color_count = 2};
    int            boff = 0;
    for (int y0 = 0; y0 < PANEL_H; y0 += 5) {
        const int hh = PANEL_H - y0 < 5 ? PANEL_H - y0 : 5;
        dgx_fill_rectangle(band, 0, 0, PANEL_W, 5, 0x1234);
        pb.oy = (int16_t)y0;
        dgx_ui_paint_area(&ui, &pb, 0, y0, PANEL_W, hh);
        for (int y = 0; y < hh; ++y)
            for (int x = 0; x < PANEL_W; ++x) boff += dgx_get_pixel(band, x, y) != dgx_get_pixel(v, x, y0 + y);
    }
    CHECK(boff == 0);
    band->destroy(&band);
    dgx_ui_free(&ui);

    /* the audit of the look of lines: 1, a scroll step drawn in every part the keyboard's band leaves of the box */
    {
        dgx_screen_t  *v2 = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t pc = {.scr = v, .colors = colors, .color_count = 2, .cover = {30, 20, 30, 100}};
        dgx_ui_paint_t p2 = {.scr = v2, .colors = colors, .color_count = 2};
        t = make(&important, 3, 80);
        dgx_ui_text_set(&ui, t, "aa bb cc dd ee ff gg hh ii jj kk ll mm nn oo pp qq rr");
        dgx_fill_rectangle(v, 0, 0, PANEL_W, PANEL_H, 0), dgx_fill_rectangle(v2, 0, 0, PANEL_W, PANEL_H, 0);
        dgx_ui_paint(&ui, &pc, 0);
        dgx_ui_paint(&ui, &pc, 250);
        const uint16_t top_covered = box.top;
        dgx_ui_free(&ui);
        t = make(&important, 3, 80);
        dgx_ui_text_set(&ui, t, "aa bb cc dd ee ff gg hh ii jj kk ll mm nn oo pp qq rr");
        dgx_ui_paint(&ui, &p2, 0);
        dgx_ui_paint(&ui, &p2, 250);
        const dgx_ui_node_t *bn  = dgx_ui_node(&ui, t);
        int                  off = 0;
        for (int y = bn->y; y < bn->y + bn->h; ++y)
            for (int x = bn->x; x < bn->x + bn->w; ++x)
                if (!(x >= 30 && x < 60 && y >= 20)) off += dgx_get_pixel(v, x, y) != dgx_get_pixel(v2, x, y);
        CHECK(top_covered == box.top && box.top == 2 && off == 0);
        dgx_ui_free(&ui);
        v2->destroy(&v2);
    }
    /* 2: a text not important scrolled by hand to its end: no ellipsis, nothing follows */
    t = make(&plain, 2, w);
    dgx_ui_text_set(&ui, t, "aa bb cc dd ee ff");
    dgx_ui_paint(&ui, &pv, 0);
    dgx_ui_text_scroll(&ui, t, 100);
    dgx_ui_paint(&ui, &pv, 0);
    {
        const char *const tail[] = {"cc dd", "ee ff"};
        reference(e, dgx_ui_node(&ui, t), tail, 2, DGX_FONT_ALIGN_LEFT);
        CHECK(box.top == 1 && differ_box(v, e, dgx_ui_node(&ui, t)) == 0);
    }
    dgx_ui_free(&ui);
    /* 3: a text that appeared, drawn again 28 days later: as it was, and no time asked */
    t = make(&plain, 1, 80);
    dgx_ui_text_set(&ui, t, "abcd");
    dgx_ui_text_appear(&ui, t, 1);
    dgx_ui_paint(&ui, &pv, 0);
    CHECK(!dgx_ui_paint(&ui, &pv, 5000));
    dgx_ui_redraw(&ui, t);
    CHECK(!dgx_ui_paint(&ui, &pv, 0x90000000u));
    {
        const char *const whole_text[] = {"abcd"};
        reference(e, dgx_ui_node(&ui, t), whole_text, 1, DGX_FONT_ALIGN_LEFT);
        CHECK(differ_box(v, e, dgx_ui_node(&ui, t)) == 0);
    }
    dgx_ui_free(&ui);

    v->destroy(&v), e->destroy(&e);
    free(panel);
    CHECK_DONE();
}
