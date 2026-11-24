/*
 * The painter of dgx-ui on a 16-bit panel behind a bus that counts the writes of every pixel, beside a virtual
 * screen that keeps the picture to compare: a tree drawn whole, then a value changed writing only its field, every
 * pixel once; nothing drawn when nothing changed; the screen's clip given back; the keyboard's band left alone; the
 * variant of a font fitted; a text cut with an ellipsis.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_font.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800}; /* paper, ink, fill, border */

/* the tree: a column with a frame holding a text, and a text under it */
typedef struct {
    dgx_ui_node_t             nodes[6];
    dgx_ui_widget_t           w1, w2; /* the texts: the tree's own, as each points to its own text box */
    dgx_ui_t                  ui;
    char                      b1[24], b2[24];
    dgx_ui_textbox_t          t1, t2;
    int                       frame, text1, text2;
} tree_t;

static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .want_h = 20};

static dgx_ui_font_variant_t      small[1], two[2];
static const dgx_ui_text_style_t *style_small, *style_two;
static dgx_ui_text_style_t        st_small, st_two;

static void make_styles(void)
{
    small[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    two[0]   = (dgx_ui_font_variant_t){.font = &DejaVuSansBook32};
    two[1]   = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    st_small = (dgx_ui_text_style_t){.fonts = small, .count = 1, .align = DGX_FONT_ALIGN_RIGHT};
    st_two   = (dgx_ui_text_style_t){.fonts = two, .count = 2, .align = DGX_FONT_ALIGN_LEFT, .flags = DGX_UI_TEXT_MIDDLE};
    style_small = &st_small, style_two = &st_two;
}

static void make_tree(tree_t *t)
{
    memset(t, 0, sizeof(*t));
    dgx_ui_init(&t->ui, t->nodes, 6, 1);
    dgx_ui_add(&t->ui, -1, &root_w);
    t->frame = dgx_ui_add(&t->ui, 0, &frame_w);
    dgx_ui_textbox_init(&t->t1, t->b1, sizeof t->b1, 1, style_small);
    dgx_ui_textbox_init(&t->t2, t->b2, sizeof t->b2, 1, style_two);
    t->w1    = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t->t1};
    t->w2    = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t->t2, .want_w = 80};
    t->text1 = dgx_ui_add(&t->ui, t->frame, &t->w1);
    t->text2 = dgx_ui_add(&t->ui, 0, &t->w2);
}

static int differ(const panel_t *p, dgx_screen_t *v)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->mem[y][x] != dgx_get_pixel(v, x, y);
    return n;
}

int main(void)
{
    make_styles();
    panel_t       *panel = panel_make();
    dgx_screen_t  *ps    = &panel->sbus.scr;
    dgx_screen_t  *v     = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    static tree_t  a, b;
    dgx_ui_paint_t pa = {.scr = ps, .colors = colors, .color_count = 4, .background = 0};
    dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4, .background = 0};

    /* drawn whole: the same picture on the panel and in memory */
    make_tree(&a), make_tree(&b);
    dgx_ui_text_set(&a.ui, a.text1, "412 ppm"), dgx_ui_text_set(&b.ui, b.text1, "412 ppm");
    dgx_ui_text_set(&a.ui, a.text2, "21.5"), dgx_ui_text_set(&b.ui, b.text2, "21.5");
    CHECK(!dgx_ui_paint(&a.ui, &pa, 0));
    dgx_ui_paint(&b.ui, &pv, 0);
    CHECK(differ(panel, v) == 0);
    /* every pixel of the screen written, once: the boxes fill around their children */
    CHECK(panel_written(panel) == PANEL_W * PANEL_H && panel_twice(panel) == 0);

    /* nothing changed: nothing written */
    panel_count_from_now(panel);
    dgx_ui_paint(&a.ui, &pa, 10);
    CHECK(panel_written(panel) == 0);

    /* a value changed: only its field written, every pixel once; the picture as if drawn whole */
    const dgx_ui_node_t *w1 = dgx_ui_node(&a.ui, a.text1);
    panel_count_from_now(panel);
    CHECK(dgx_ui_text_set(&a.ui, a.text1, "98 ppm"));
    dgx_ui_paint(&a.ui, &pa, 20);
    const dgx_font_t *t12 = &TerminusTTFMedium12;
    const int   base = w1->y - t12->y_offset_lowest;
    CHECK(panel_twice(panel) == 0);
    CHECK(panel_written_outside(panel, w1->x, base + t12->y_offset_lowest, w1->x + w1->w, base + t12->y_bottom_max) == 0);
    CHECK(panel_written(panel) > 0);
    tree_t *c = &b;
    make_tree(c);
    dgx_ui_text_set(&c->ui, c->text1, "98 ppm"), dgx_ui_text_set(&c->ui, c->text2, "21.5");
    dgx_ui_paint(&c->ui, &pv, 0);
    CHECK(differ(panel, v) == 0);

    /* the program's clip on the screen given back */
    dgx_screen_clip(ps, 3, 4, 50, 20);
    dgx_ui_text_set(&a.ui, a.text1, "7 ppm");
    dgx_ui_paint(&a.ui, &pa, 30);
    int cx, cy, cw, ch;
    dgx_screen_clip_get(ps, &cx, &cy, &cw, &ch);
    CHECK(cx == 3 && cy == 4 && cw == 50 && ch == 20);
    dgx_screen_unclip(ps);

    /* a variant fitted: "21.5" goes into 80 pixels in the large font, a longer text only in the small one */
    CHECK(a.t2.variant == 0);
    dgx_ui_text_set(&a.ui, a.text2, "21.5 C, 40%");
    dgx_ui_paint(&a.ui, &pa, 40);
    CHECK(a.t2.variant == 1);
    /* the same tree drawn whole in memory gives the same picture */
    make_tree(c);
    dgx_ui_text_set(&c->ui, c->text1, "7 ppm"), dgx_ui_text_set(&c->ui, c->text2, "21.5 C, 40%");
    dgx_ui_paint(&c->ui, &pv, 0);
    CHECK(differ(panel, v) == 0);

    /* too long for any: cut with an ellipsis, as the font draws the cut text */
    dgx_ui_text_set(&a.ui, a.text2, "a text far too long for it");
    dgx_ui_paint(&a.ui, &pa, 50);
    const dgx_ui_node_t *w2 = dgx_ui_node(&a.ui, a.text2);
    dgx_screen_t          *e  = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_fill_rectangle(e, 0, 0, PANEL_W, PANEL_H, 0);
    const int base2 = w2->y + (w2->h - (t12->y_bottom_max - t12->y_offset_lowest)) / 2 - t12->y_offset_lowest; /* middle */
    int       shown = 0;
    for (int n = 1; n <= 26; ++n) {
        char s[40];
        memcpy(s, "a text far too long for it", (size_t)n), memcpy(s + n, "...", 4);
        const dgx_font_context_t m = {.font = t12};
        if (dgx_font_bounds(&m, s, NULL, NULL) <= w2->w) shown = n;
    }
    char cut[40];
    memcpy(cut, "a text far too long for it", (size_t)shown), memcpy(cut + shown, "...", 4);
    const dgx_font_context_t ec = {.scr = e, .font = t12, .color = 0xffff, .bg = 0, .field = w2->w, .align = DGX_FONT_ALIGN_LEFT};
    dgx_font_draw(&ec, w2->x, base2, cut);
    int off = 0;
    for (int y = base2 + t12->y_offset_lowest; y < base2 + t12->y_bottom_max; ++y)
        for (int x = w2->x; x < w2->x + w2->w; ++x) off += panel->mem[y][x] != dgx_get_pixel(e, x, y);
    CHECK(shown > 0 && off == 0);

    /* the screen composed in strips of 5 rows in a buffer of its own, each strip placed by ox and oy: every look
       draws whole under any clip at any origin, and the strips give the picture drawn in one piece */
    make_tree(c);
    dgx_ui_text_set(&c->ui, c->text1, "1234 ppm"), dgx_ui_text_set(&c->ui, c->text2, "a text far too long for it");
    dgx_ui_paint(&c->ui, &pv, 0);
    dgx_screen_t  *band = dgx_vscreen_init(PANEL_W, 5, 16, DgxScreenRGB);
    dgx_ui_paint_t pb   = {.scr = band, .colors = colors, .color_count = 4, .background = 0};
    int            strip_off = 0;
    for (int y0 = 0; y0 < PANEL_H; y0 += 5) {
        const int hh = PANEL_H - y0 < 5 ? PANEL_H - y0 : 5;
        dgx_fill_rectangle(band, 0, 0, PANEL_W, 5, 0x1234); /* what was there: all of it must be written */
        pb.oy = (int16_t)y0;
        dgx_ui_paint_area(&c->ui, &pb, 0, y0, PANEL_W, hh);
        for (int y = 0; y < hh; ++y)
            for (int x = 0; x < PANEL_W; ++x) strip_off += dgx_get_pixel(band, x, y) != dgx_get_pixel(v, x, y0 + y);
    }
    CHECK(strip_off == 0);
    /* and in columns of 7, by ox */
    dgx_screen_t  *col = dgx_vscreen_init(7, PANEL_H, 16, DgxScreenRGB);
    dgx_ui_paint_t pc  = {.scr = col, .colors = colors, .color_count = 4, .background = 0};
    int            col_off = 0;
    for (int x0 = 0; x0 < PANEL_W; x0 += 7) {
        const int ww = PANEL_W - x0 < 7 ? PANEL_W - x0 : 7;
        pc.ox = (int16_t)x0;
        dgx_ui_paint_area(&c->ui, &pc, x0, 0, ww, PANEL_H);
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < ww; ++x) col_off += dgx_get_pixel(col, x, y) != dgx_get_pixel(v, x0 + x, y);
    }
    CHECK(col_off == 0);
    band->destroy(&band), col->destroy(&col);

    /* a text of more than 256 bytes, a letter of three at byte 255, in a field wide enough for all
       of it: cut without writing past the line (the sanitizer is the proof) */
    {
        static char                  longest[300];
        static dgx_ui_textbox_t      lbox;
        static const dgx_ui_widget_t lroot = {.kind = DGX_UI_BOX};
        static const dgx_ui_widget_t ltext = {.kind = DGX_UI_LABEL, .text = &lbox, .want_w = 3000};
        dgx_ui_node_t                ln[2];
        dgx_ui_t                     lu;
        dgx_ui_textbox_init(&lbox, longest, sizeof longest, 1, style_small);
        dgx_ui_init(&lu, ln, 2, 1);
        dgx_ui_add(&lu, -1, &lroot);
        const int lt = dgx_ui_add(&lu, 0, &ltext);
        char      s[300];
        memset(s, 'i', 255);
        memcpy(s + 255, "\xe2\x82\xac" "xyz", 7);
        CHECK(dgx_ui_text_set(&lu, lt, s));
        dgx_ui_paint(&lu, &pv, 0);
        dgx_ui_free(&lu);
    }

    /* a strip composed before the paint does not hide from it that the variant changed: the rows of
       the large letters above and below the small field are drawn again */
    make_tree(&a);
    dgx_ui_text_set(&a.ui, a.text1, "1"), dgx_ui_text_set(&a.ui, a.text2, "21.5");
    dgx_ui_paint(&a.ui, &pa, 70);
    CHECK(a.t2.variant == 0);
    dgx_ui_text_set(&a.ui, a.text2, "21.5 C, 40%");
    {
        dgx_screen_t  *strip = dgx_vscreen_init(PANEL_W, 4, 16, DgxScreenRGB);
        dgx_ui_paint_t ps    = {.scr = strip, .colors = colors, .color_count = 4, .background = 0};
        const dgx_ui_node_t *t2 = dgx_ui_node(&a.ui, a.text2);
        ps.oy = (int16_t)(t2->y + t2->h / 2);
        dgx_ui_paint_area(&a.ui, &ps, 0, ps.oy, PANEL_W, 4);
        strip->destroy(&strip);
    }
    dgx_ui_paint(&a.ui, &pa, 80);
    make_tree(c);
    dgx_ui_text_set(&c->ui, c->text1, "1"), dgx_ui_text_set(&c->ui, c->text2, "21.5 C, 40%");
    dgx_ui_paint(&c->ui, &pv, 0);
    CHECK(differ(panel, v) == 0);

    /* the keyboard's band: nothing of the tree written there, whatever changes */
    panel_count_from_now(panel);
    pa.cover = dgx_ui_rect(0, 0, PANEL_W, 24); /* over the frame and its text */
    dgx_ui_redraw(&a.ui, 0);
    dgx_ui_paint(&a.ui, &pa, 60);
    CHECK(panel_written_outside(panel, 0, 24, PANEL_W, PANEL_H) == 0 && panel_written(panel) > 0);

    /* a text changed under the band, then the band gone: the place it leaves composed again */
    dgx_ui_text_set(&a.ui, a.text1, "5555");
    dgx_ui_paint(&a.ui, &pa, 90); /* under the band: not drawn */
    pa.cover = dgx_ui_rect(0, 0, 0, 0);
    dgx_ui_paint(&a.ui, &pa, 100);
    make_tree(c);
    dgx_ui_text_set(&c->ui, c->text1, "5555"), dgx_ui_text_set(&c->ui, c->text2, "21.5 C, 40%");
    dgx_ui_paint(&c->ui, &pv, 0);
    CHECK(differ(panel, v) == 0);

    dgx_ui_free(&a.ui), dgx_ui_free(&b.ui);
    v->destroy(&v), e->destroy(&e);
    free(panel);
    CHECK_DONE();
}
