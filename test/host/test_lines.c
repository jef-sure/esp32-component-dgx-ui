/*
 * The look of lines: a text laid word by word into the rows of its box, against the lines drawn one by one by the
 * font; a text changed on the counting panel writing every pixel once, within the box; an important text scrolled
 * by time; a text not important cut; letters appearing where they will stand; the rows composed in strips. Letters
 * coming at the top and in the middle: each pass writes the fields of the rows whose letters changed and no more; in
 * the middle the block drawn whole only when the number of its rows shown changed, an important text's scroll step
 * its rows, the passes between nothing. And after every pass of letters coming, in each align, at the top and in the
 * middle, cut or scrolled, the screen as the tree composed anew; a cut text's last row with the dots of its ellipsis.
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
#include "composed.h"
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
    const dgx_font_context_t m = {.font = &TerminusTTFMedium12};
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
    const dgx_font_t *f = &TerminusTTFMedium12;
    dgx_fill_rectangle(e, 0, 0, e->width, e->height, 0);
    for (int k = 0; k < count; ++k) {
        const dgx_font_context_t c = {.scr = e, .font = f, .color = 0xffff, .bg = 0, .field = n->w, .align = align};
        dgx_font_draw(&c, n->x, n->y - f->y_offset_lowest + k * f->y_advance, lines[k]);
    }
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    plain      = (dgx_ui_text_style_t){.fonts = variant, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    important  = (dgx_ui_text_style_t){.fonts = variant, .count = 1, .align = DGX_FONT_ALIGN_LEFT, .flags = DGX_UI_STYLE_IMPORTANT, .scroll_ms = 100};
    centred    = (dgx_ui_text_style_t){.fonts = variant, .count = 1, .align = DGX_FONT_ALIGN_CENTER};
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

    /* a scroll step drawn in every part the keyboard's band leaves of the box */
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

    /* a text that scrolls by time, composed where a popup over its left half closed between two of its steps: at the
       time of the pass, as a tree with no popup draws it then, not at time 0; and at the next step too */
    {
        static char                  ba[200], bb[200];
        static dgx_ui_textbox_t      ta, tb;
        static dgx_ui_widget_t       wa, wb;
        static dgx_ui_node_t         na[2], nb[2], np[1];
        static dgx_ui_t              ua, ub, pop;
        static const dgx_ui_widget_t pop_w = {.kind = DGX_UI_FRAME};
        dgx_ui_textbox_init(&ta, ba, sizeof ba, 2, &important), dgx_ui_textbox_init(&tb, bb, sizeof bb, 2, &important);
        wa = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &ta, .look = &dgx_ui_look_lines, .want_w = 60, .at_x = 4, .at_y = 4};
        wb = wa, wb.text = &tb;
        dgx_ui_init(&ua, na, 2, 1), dgx_ui_init(&ub, nb, 2, 1);
        dgx_ui_add(&ua, -1, &root_w), dgx_ui_add(&ub, -1, &root_w);
        dgx_ui_add(&ua, 0, &wa), dgx_ui_add(&ub, 0, &wb);
        const char *const text = "one two three four five six seven eight nine ten";
        dgx_ui_text_set(&ua, 1, text), dgx_ui_text_set(&ub, 1, text);
        dgx_screen_t  *sa = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), *sb = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t pa = {.scr = sa, .colors = colors, .color_count = 2}, pb = {.scr = sb, .colors = colors, .color_count = 2};
        dgx_ui_paint(&ua, &pa, 1000), dgx_ui_paint(&ub, &pb, 1000);
        dgx_ui_init(&pop, np, 1, 1);
        dgx_ui_add(&pop, -1, &pop_w);
        CHECK(dgx_ui_popup_open(&ua, &pop, 0, 0, 34, PANEL_H, 0));
        dgx_ui_paint(&ua, &pa, 1150), dgx_ui_paint(&ub, &pb, 1150);
        dgx_ui_paint(&ua, &pa, 1250), dgx_ui_paint(&ub, &pb, 1250);
        dgx_ui_popup_close(&ua);
        dgx_ui_paint(&ua, &pa, 1290), dgx_ui_paint(&ub, &pb, 1290);
        const dgx_ui_node_t *n = dgx_ui_node(&ub, 1);
        CHECK(ta.top > 0 && ta.top == tb.top && differ_box(sa, sb, n) == 0);
        dgx_ui_paint(&ua, &pa, 1350), dgx_ui_paint(&ub, &pb, 1350);
        CHECK(differ_box(sa, sb, n) == 0);
        dgx_ui_paint_free(&pa), dgx_ui_paint_free(&pb);
        dgx_ui_free(&ua), dgx_ui_free(&ub), dgx_ui_free(&pop);
        sa->destroy(&sa), sb->destroy(&sb);
    }

    /* letters coming on the counting panel, at the top and in the middle: after the first pass each writes the fields
       of the rows whose letters changed and no more; in the middle a line more while they come, the block whole once; an
       important text scrolled in the middle, a step writes its rows, the passes between nothing */
    {
        const dgx_font_t          *font   = &TerminusTTFMedium12;
        static dgx_ui_text_style_t middle, important_middle;
        middle                            = plain, middle.flags = DGX_UI_STYLE_MIDDLE;
        important_middle                  = important, important_middle.flags |= DGX_UI_STYLE_MIDDLE;
        const dgx_ui_text_style_t *styles[2] = {&plain, &middle};
        for (int k = 0; k < 2; ++k) {
            const int      id     = make(styles[k], 4, 90);
            dgx_ui_paint_t pp     = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 2};
            dgx_ui_text_set(&ui, id, "one two three four five six seven");
            dgx_ui_paint(&ui, &pp, 0);
            const dgx_ui_node_t *n = dgx_ui_node(&ui, id);
            const int            field = n->w * (font->y_bottom_max - font->y_offset_lowest);
            dgx_ui_text_appear(&ui, id, 40);
            int  most = 0, passes = 0;
            bool more = true;
            for (uint32_t ms = 1000; more && ms <= 4000; ms += 20, ++passes) {
                panel_count_from_now(panel);
                more = dgx_ui_paint(&ui, &pp, ms);
                if (passes > 0 && panel_written(panel) > most) most = panel_written(panel);
            }
            CHECK(!more && passes > 20 && most > 0 && most <= 2 * field);
            if (k == 1) {
                /* a line more while its letters come: the block moves, whole once; the pass after writes its new
                   letters' row only */
                dgx_ui_text_set(&ui, id, "one");
                dgx_ui_paint(&ui, &pp, 5000);
                dgx_ui_text_appear(&ui, id, 40);
                dgx_ui_paint(&ui, &pp, 5100);
                dgx_ui_text_set(&ui, id, "one\ntwo");
                panel_count_from_now(panel);
                dgx_ui_paint(&ui, &pp, 5200);
                const int moved = panel_written(panel);
                panel_count_from_now(panel);
                dgx_ui_paint(&ui, &pp, 5240);
                CHECK(moved >= n->w * n->h && panel_written(panel) > 0 && panel_written(panel) <= field);
            }
            dgx_ui_paint_free(&pp), dgx_ui_free(&ui);
        }
        /* important, too long, in the middle: a step every 100 ms writes its rows; between the steps nothing */
        const int      id = make(&important_middle, 2, 90);
        dgx_ui_paint_t pp = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 2};
        dgx_ui_text_set(&ui, id, "a\nb\nc\nd\ne\nf");
        dgx_ui_paint(&ui, &pp, 0);
        const dgx_ui_node_t *n     = dgx_ui_node(&ui, id);
        const int            field = n->w * (font->y_bottom_max - font->y_offset_lowest);
        int                  step_least = n->w * n->h, between_most = 0;
        for (uint32_t ms = 50; ms <= 400; ms += 50) {
            panel_count_from_now(panel);
            dgx_ui_paint(&ui, &pp, ms);
            if (ms % 100 == 0) step_least = panel_written(panel) < step_least ? panel_written(panel) : step_least;
            else between_most = panel_written(panel) > between_most ? panel_written(panel) : between_most;
        }
        CHECK(box.top >= 3 && step_least >= 2 * field && between_most == 0);
        dgx_ui_paint_free(&pp), dgx_ui_free(&ui);
    }

    /* letters coming at 30 ms in a box of 88 by 56, each align, at the top and in the middle, the text cut or important
       and scrolled: after every pass the screen as the tree composed anew (composed.h), the text changed while they
       come and set again; at the end the cut text as drawn at once, all three dots of its ellipsis there. Two texts:
       one whose last row seen is shortened for the dots, one whose last row takes them as it is */
    static const char *const samples[2] = {"The quick brown fox jumps over the lazy dog and runs on and on over the hills", "one\ntwo\nthree\nfour\nfive\nsix"};
    for (int sample = 0; sample < 2; ++sample) {
        const char *const          quick = samples[sample];
        static dgx_ui_text_style_t styles[12];
        const dgx_font_align_t     aligns[3] = {DGX_FONT_ALIGN_LEFT, DGX_FONT_ALIGN_CENTER, DGX_FONT_ALIGN_RIGHT};
        for (int k = 0; k < 12; ++k) {
            const bool middle_k = k / 3 % 2, important_k = k / 6;
            styles[k]           = (dgx_ui_text_style_t){.fonts  = variant,
                                                        .count     = 1,
                                                        .align     = aligns[k % 3],
                                                        .flags     = (uint8_t)((middle_k ? DGX_UI_STYLE_MIDDLE : 0) | (important_k ? DGX_UI_STYLE_IMPORTANT : 0)),
                                                        .scroll_ms = (uint16_t)(important_k ? 150 : 0)};
            int id        = make(&styles[k], 4, 88);
            text_w.want_h = 56;
            dgx_ui_paint_t painted = {.scr = v, .colors = colors, .color_count = 2};
            dgx_fill_rectangle(v, 0, 0, PANEL_W, PANEL_H, 0);
            dgx_ui_text_set(&ui, id, quick);
            dgx_ui_text_appear(&ui, id, 30);
            int differing = 0;
            for (uint32_t ms = 0; ms <= 6000; ms += 20) {
                if (ms == 1200) dgx_ui_text_set(&ui, id, "a shorter one now");
                if (ms == 1800) dgx_ui_text_set(&ui, id, quick), dgx_ui_text_appear(&ui, id, 30);
                dgx_ui_paint(&ui, &painted, ms);
                differing += differ_from_composed(&ui, &painted, e, id) > 0;
            }
            const int lines = box.total_lines, came = box.letters_drawn == box.total_letters;
            CHECK(lines > 4 && came && differing == 0);
            dgx_ui_paint_free(&painted), dgx_ui_free(&ui);
            if (important_k) continue;
            id            = make(&styles[k], 4, 88);
            text_w.want_h = 56;
            dgx_ui_paint_t at_once = {.scr = e, .colors = colors, .color_count = 2};
            dgx_fill_rectangle(e, 0, 0, PANEL_W, PANEL_H, 0);
            dgx_ui_text_set(&ui, id, quick);
            dgx_ui_paint(&ui, &at_once, 0);
            CHECK(differ_box(v, e, dgx_ui_node(&ui, id)) == 0);
            dgx_ui_paint_free(&at_once), dgx_ui_free(&ui);
        }
    }

    v->destroy(&v), e->destroy(&e);
    free(panel);
    CHECK_DONE();
}
