/*
 * The program's drawn background: a map, a virtual screen of 16 bits as large as the panel, put under the tree by
 * background_draw. The same tree on a panel behind a bus of 16 bits, the strips named, and on a virtual screen drawn
 * straight: every look lays the map where its paper is what lies under it and draws over it (the letters, a picture of
 * 1 bit, one of 4 bits with its index 0, a slider, a button, a field, a list of lines), a frame shows its fill, an icon
 * of no picture is the map, a look of the program's own lays it by dgx_ui_lay_paper(). Through the strips every pixel
 * once, as the virtual screen; straight on the panel, a label changed is written twice. The letters stand where they
 * stand on plain paper, cut to their field as there; the program's function, drawing more than it is asked, is cut
 * to the rectangle by the clip. A popup whose root does not
 * fill shows the map. dgx_ui_background_changed() at two corners writes those two rectangles alone, as a whole new
 * composing would have them; partly off the screen, its part on it; under a frame that fills, nothing. A picture of 4
 * bits in a strip with no room left: its box the map. Letters of a font of 4 bits: in a strip their soft edges mix
 * with the map, as on the virtual screen; straight on the panel, which cannot be read, with the colour of the paper.
 */
#include <string.h>

#define PANEL_H 152
#include "check.h"
#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

/* a place for the value of a widget made at run time, a new one each time, 0 in it */
static int32_t  place_pool[256];
static int      place_next;
static int32_t *new_place(void)
{
    int32_t *place = &place_pool[place_next++ % 256];
    *place         = 0;
    return place;
}

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800}; /* the background's colour, ink, a fill, the accent */

/* the map: no pixel of it one of the table's colours (its blue never under 16, its green never 63) */
static dgx_screen_t *map;
static int           laid;

static uint32_t map_at(int x, int y, int seed)
{
    return (uint32_t)(((x * 3 + seed) & 0x1f) << 11 | ((y * 2 + seed) & 0x1f) << 5 | (((x + y) & 0x0f) | 0x10));
}

static void map_fill(int x0, int y0, int w, int h, int seed)
{
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x) dgx_set_pixel(map, x, y, map_at(x, y, seed));
}

/* the program's function: a point more of the map on every side than asked, which the clip cuts */
static void draw_map(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    ++laid;
    const int x0 = x > 0 ? x - 1 : x, y0 = y > 0 ? y - 1 : y, x1 = x + w < PANEL_W ? x + w + 1 : x + w, y1 = y + h < PANEL_H ? y + h + 1 : y + h;
    dgx_vscreen_region_to_screen(paint->scr, x0 - paint->ox, y0 - paint->oy, (dgx_screen_t *)ctx, x0, y0, x1 - x0, y1 - y0, 0);
}

/* pictures: 8x8 of 1 bit, a frame; 6x5 of 4 bits, the value (x + y) % 4 */
static const uint8_t     frame_bits[8] = {0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff};
static const dgx_image_t im1           = {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = frame_bits};
static uint8_t           bits4[5 * 3];
static const uint8_t     map4[15] = {1, 2, 3};
static const dgx_image_t im4      = {.width = 6, .height = 5, .format = DGX_IMAGE_4BPP, .data = bits4};
static uint16_t          px565[6 * 6];
static dgx_ui_picture_t  pic1 = {.image = &im1}, pic4 = {.image = &im4, .map = map4}, pic565 = {.rgb565 = px565, .width = 6, .height = 6};

/* a font of 4 bits a pixel, its two letters A and B with levels of every kind, made as DGX's test of soft text makes
   it */
enum { AW = 5, AH = 4, BW = 6, BH = 5 };
static const uint8_t levels_a[AH][AW] = {{0, 3, 15, 3, 0}, {2, 15, 0, 15, 2}, {15, 15, 15, 15, 15}, {15, 7, 0, 6, 15}};
static const uint8_t levels_b[BH][BW] = {{1, 0, 0, 0, 0, 14}, {0, 8, 15, 15, 8, 0}, {0, 15, 0, 0, 15, 0}, {0, 8, 15, 15, 8, 0}, {14, 0, 0, 0, 0, 1}};
static uint8_t           soft_bits[2][64];
static dgx_glyph_t       soft_glyphs[2];
static dgx_glyph_array_t soft_ranges[2]; /* the last ends the table */
static dgx_font_t        soft_font;

static void make_soft_font(void)
{
    for (int k = 0; k < 2; ++k) {
        const int w = k ? BW : AW, h = k ? BH : AH, pitch = (w * 4 + 7) / 8;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) soft_bits[k][y * pitch + x / 2] |= (uint8_t)((k ? levels_b[y][x] : levels_a[y][x]) << (x % 2 ? 0 : 4));
        soft_glyphs[k] = (dgx_glyph_t){.offset    = (uint32_t)(k * sizeof soft_bits[0]),
                                       .width     = (int16_t)w,
                                       .height    = (int16_t)h,
                                       .x_advance = 7,
                                       .x_offset  = 0,
                                       .y_offset  = (int16_t)(k ? -5 : -4)};
    }
    soft_ranges[0] = (dgx_glyph_array_t){.first = 'A', .number = 2, .glyph = 0};
    soft_font      = (dgx_font_t){.glyph_ranges     = soft_ranges,
                                  .glyphs           = soft_glyphs,
                                  .data             = soft_bits[0],
                                  .y_advance        = 8,
                                  .y_offset_lowest  = -5,
                                  .x_widest         = BW,
                                  .x_width_average  = 6,
                                  .f_type           = DGX_FONT_BITMAP_DEPTH4,
                                  .y_bottom_max     = 1,
                                  .x_right_max      = 6,
                                  .number_of_ranges = 1};
}

/* a look of the program's own: the paper laid on its left half, the accent on its right */
static bool draw_own(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    dgx_ui_lay_paper(paint, DGX_UI_PAPER, n->x, n->y, n->w / 2, n->h);
    dgx_ui_lay_paper(paint, 3, n->x + n->w / 2, n->y, n->w - n->w / 2, n->h);
    return false;
}
static const dgx_ui_look_t own_look = {.draw = draw_own, .fill = DGX_UI_PAPER};

static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t frame_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .want_h = 18};
static int32_t slider_w_place; /* its value */
static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .value = &slider_w_place, .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t row_w    = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 2, .want_h = 16};
static const dgx_ui_widget_t p1_w     = {.kind = DGX_UI_PICTURE, .picture = &pic1, .want_w = 12, .look = &dgx_ui_look_picture};
static const dgx_ui_widget_t p4_w     = {.kind = DGX_UI_PICTURE, .picture = &pic4, .want_w = 12, .look = &dgx_ui_look_indexed};
static int32_t none_w_place; /* its value */
static const dgx_ui_widget_t none_w   = {.kind = DGX_UI_ICON, .value = &none_w_place, .want_w = 12, .look = &dgx_ui_look_picture};
static const dgx_ui_widget_t own_w    = {.kind = DGX_UI_BOX, .want_w = 20, .look = &own_look};
static const dgx_ui_widget_t p565_w   = {.kind = DGX_UI_PICTURE, .picture = &pic565, .want_w = 12, .look = &dgx_ui_look_rgb565};

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;

/* the tree: a label, a frame holding a label, a button, a slider, a field, lines, a list, a row of pictures and the
   look of the program's; three of it, for the panel, for the virtual screen, and for one of plain paper */
typedef struct {
    dgx_ui_node_t    nodes[18];
    dgx_ui_t         ui;
    char             b[6][40];
    dgx_ui_textbox_t t[6];
    dgx_ui_widget_t  label, inner, button, input, lines, list;
    int              l, f, in, bt, sl, ip, ln, li, p1, p4, none, own, p565;
} tree_t;

static void make(tree_t *t)
{
    memset(t, 0, sizeof *t);
    for (int k = 0; k < 6; ++k) dgx_ui_textbox_init(&t->t[k], t->b[k], sizeof t->b[k], k >= 4 ? 2 : 1, &style);
    t->label  = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t->t[0], .want_h = 12};
    t->inner  = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t->t[1], .want_h = 12};
    t->button = (dgx_ui_widget_t){.kind = DGX_UI_BUTTON, .text = &t->t[2], .want_h = 14, .look = &dgx_ui_look_button};
    t->input  = (dgx_ui_widget_t){.kind = DGX_UI_INPUT, .text = &t->t[3], .want_h = 12, .look = &dgx_ui_look_input};
    t->lines  = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t->t[4], .want_h = 24, .look = &dgx_ui_look_lines};
    t->list   = (dgx_ui_widget_t){.kind = DGX_UI_LIST, .value = new_place(), .text = &t->t[5], .max = 1, .want_h = 26, .look = &dgx_ui_look_list};
    dgx_ui_init(&t->ui, t->nodes, 18, 1);
    dgx_ui_add(&t->ui, -1, &root_w);
    t->l  = dgx_ui_add(&t->ui, 0, &t->label);
    t->f  = dgx_ui_add(&t->ui, 0, &frame_w);
    t->in = dgx_ui_add(&t->ui, t->f, &t->inner);
    t->bt = dgx_ui_add(&t->ui, 0, &t->button);
    t->sl     = dgx_ui_add(&t->ui, 0, &slider_w);
    t->ip     = dgx_ui_add(&t->ui, 0, &t->input);
    t->ln     = dgx_ui_add(&t->ui, 0, &t->lines);
    t->li     = dgx_ui_add(&t->ui, 0, &t->list);
    const int r = dgx_ui_add(&t->ui, 0, &row_w);
    t->p1     = dgx_ui_add(&t->ui, r, &p1_w);
    t->p4     = dgx_ui_add(&t->ui, r, &p4_w);
    t->none   = dgx_ui_add(&t->ui, r, &none_w);
    t->own    = dgx_ui_add(&t->ui, r, &own_w);
    t->p565   = dgx_ui_add(&t->ui, r, &p565_w);
    dgx_ui_text_set(&t->ui, t->l, "5 m/s");
    dgx_ui_text_set(&t->ui, t->in, "framed");
    dgx_ui_text_set(&t->ui, t->bt, "OK");
    dgx_ui_text_set(&t->ui, t->ip, "abcdefghijklmnopqrstuvwxyz"); /* longer than the field: set from its right */
    dgx_ui_text_set(&t->ui, t->ln, "wind from the west");
    dgx_ui_text_set(&t->ui, t->li, "a long option of the list\nb");
    dgx_ui_set_value(&t->ui, t->li, 1); /* the second picked: the first on the list's paper */
}

static int differ(const panel_t *p, dgx_screen_t *v)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->mem[y][x] != dgx_get_pixel(v, x, y);
    return n;
}

/* pixels of a widget's box on the virtual screen that are neither the ink nor the map; *inked: those of the ink */
static int neither(dgx_screen_t *v, const dgx_ui_t *t, int i, int *inked)
{
    const dgx_ui_node_t *n   = dgx_ui_node(t, i);
    int                  bad = 0;
    *inked                   = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) {
            const uint32_t c = dgx_get_pixel(v, x, y);
            if (c == colors[1]) ++*inked;
            else bad += c != map_at(x, y, 0);
        }
    return bad;
}

/* pixels of a rectangle on the virtual screen neither the ink nor the map; *inked: those of the ink */
static int neither_in(dgx_screen_t *v, int x0, int y0, int w, int h, int *inked)
{
    int bad = 0;
    *inked  = 0;
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x) {
            const uint32_t c = dgx_get_pixel(v, x, y);
            if (c == colors[1]) ++*inked;
            else bad += c != map_at(x, y, 0);
        }
    return bad;
}

/* the ink of a rectangle where it is on plain paper: pixels of the ink on one and not on the other */
static int ink_moved(dgx_screen_t *v, dgx_screen_t *plain, int x0, int y0, int w, int h)
{
    int n = 0;
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x) n += (dgx_get_pixel(v, x, y) == colors[1]) != (dgx_get_pixel(plain, x, y) == colors[1]);
    return n;
}

/* written outside two rectangles */
static int written_beside(const panel_t *p, dgx_ui_rect_t a, dgx_ui_rect_t b)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x)
            n += p->writes[y][x] && !(x >= a.x && x < a.x + a.w && y >= a.y && y < a.y + a.h) && !(x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h);
    return n;
}

int main(void)
{
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 6; ++x) bits4[y * 3 + x / 2] |= (uint8_t)(((x + y) % 4) << (x % 2 ? 0 : 4));
    for (int k = 0; k < 6 * 6; ++k) px565[k] = (uint16_t)(0xf81f - k); /* its bytes in memory: low first on this machine */
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1, .flags = DGX_UI_STYLE_MIDDLE, .align = DGX_FONT_ALIGN_CENTER};
    map        = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    map_fill(0, 0, PANEL_W, PANEL_H, 0);
    panel_t       *panel = panel_make();
    dgx_screen_t  *v     = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    static uint8_t buffer[PANEL_W * 2 * 16], vbuffer[64];
    static tree_t  a, b, c;
    make(&a), make(&b), make(&c);
    /* the colour of the background 2, the fill: where it is wrongly taken for the map, it shows */
    dgx_ui_paint_t pa = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .background = 2, .buffer = buffer, .buffer_size = sizeof buffer,
                         .strips = &dgx_ui_strips};
    dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4, .background = 2, .buffer = vbuffer, .buffer_size = sizeof vbuffer};
    dgx_ui_background_set(&pa, draw_map, map), dgx_ui_background_set(&pv, draw_map, map);
    /* the same on plain paper, where the letters stand as they should: its function written in by hand, not set, is
       not called */
    static uint8_t pbuffer[64];
    dgx_screen_t  *plain = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_ui_paint_t pp    = {.scr = plain, .colors = colors, .color_count = 4, .background = 2, .background_draw = draw_map, .background_ctx = map,
                            .buffer = pbuffer, .buffer_size = sizeof pbuffer};
    dgx_ui_paint(&c.ui, &pp, 0);
    dgx_ui_paint_area(&c.ui, &pp, 0, 0, PANEL_W, PANEL_H); /* composed too: plain still */

    /* drawn whole: every pixel once through the strips, as the virtual screen has it */
    dgx_ui_paint(&a.ui, &pa, 0), dgx_ui_paint(&b.ui, &pv, 0);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H && differ(panel, v) == 0 && laid > 0);
    /* the map in the holes of every look, on the virtual screen */
    {
        int inked;
        CHECK(neither(v, &b.ui, b.l, &inked) == 0 && inked > 0);   /* the label: its letters on the map */
        CHECK(neither(v, &b.ui, b.ip, &inked) == 0 && inked > 0);  /* the field */
        CHECK(neither(v, &b.ui, b.ln, &inked) == 0 && inked > 0);  /* the lines */
        CHECK(neither(v, &b.ui, b.p1, &inked) == 0 && inked == 28); /* the picture of 1 bit: its 28 ones */
        CHECK(neither(v, &b.ui, b.none, &inked) == 0 && inked == 0); /* the icon of no picture */
        /* the list's first row, on its paper: its long line cut to its field, as on plain paper */
        const dgx_ui_node_t *li = dgx_ui_node(&b.ui, b.li);
        CHECK(neither_in(v, li->x, li->y, li->w, li->h / 2, &inked) == 0 && inked > 0);
        CHECK(ink_moved(v, plain, li->x, li->y, li->w, li->h / 2) == 0);
        /* the letters of the label, the field and the lines where plain paper has them */
        const int texts[3] = {b.l, b.ip, b.ln};
        for (int k = 0; k < 3; ++k) {
            const dgx_ui_node_t *n = dgx_ui_node(&b.ui, texts[k]);
            CHECK(ink_moved(v, plain, n->x, n->y, n->w, n->h) == 0);
        }
        /* the frame: its fill around and under the label it holds, its letters on that fill */
        const dgx_ui_node_t *f = dgx_ui_node(&b.ui, b.f), *in = dgx_ui_node(&b.ui, b.in);
        int                  not_fill = 0;
        for (int y = in->y; y < in->y + in->h; ++y)
            for (int x = in->x; x < in->x + in->w; ++x) not_fill += dgx_get_pixel(v, x, y) != colors[2] && dgx_get_pixel(v, x, y) != colors[1];
        CHECK(dgx_get_pixel(v, f->x + 1, f->y + 1) == colors[2] && in->y > f->y + 1 && not_fill == 0);
        /* the button's corner the shadow leaves, and the slider above its track: the map */
        const dgx_ui_node_t *bt = dgx_ui_node(&b.ui, b.bt), *sl = dgx_ui_node(&b.ui, b.sl);
        CHECK(dgx_get_pixel(v, bt->x + bt->w - 1, bt->y) == map_at(bt->x + bt->w - 1, bt->y, 0));
        CHECK(dgx_get_pixel(v, sl->x + sl->w - 1, sl->y) == map_at(sl->x + sl->w - 1, sl->y, 0));
        /* the picture of RGB565: its pixels, the map around it */
        const dgx_ui_node_t *q  = dgx_ui_node(&b.ui, b.p565);
        const int            qx = q->x + (q->w - 6) / 2, qy = q->y + (q->h - 6) / 2;
        int                  qbad = 0;
        for (int y = q->y; y < q->y + q->h; ++y)
            for (int x = q->x; x < q->x + q->w; ++x) {
                const bool in_pic = x >= qx && y >= qy && x < qx + 6 && y < qy + 6;
                const uint16_t px = in_pic ? px565[(y - qy) * 6 + x - qx] : 0; /* its bytes as they lie: high first */
                qbad += dgx_get_pixel(v, x, y) != (in_pic ? (uint32_t)(uint16_t)(px >> 8 | px << 8) : map_at(x, y, 0));
            }
        CHECK(qbad == 0);
        const dgx_ui_node_t *o = dgx_ui_node(&b.ui, b.own);
        CHECK(dgx_get_pixel(v, o->x, o->y) == map_at(o->x, o->y, 0) && dgx_get_pixel(v, o->x + o->w - 1, o->y) == colors[3]);
        /* the picture of 4 bits: its index 0 the map, the rest of the box the map */
        const dgx_ui_node_t *n  = dgx_ui_node(&b.ui, b.p4);
        const int            x0 = n->x + (n->w - 6) / 2, y0 = n->y + (n->h - 5) / 2;
        int                  bad = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) {
                const bool in_pic = x >= x0 && y >= y0 && x < x0 + 6 && y < y0 + 5;
                const int  val    = in_pic ? (x - x0 + y - y0) % 4 : 0;
                bad += dgx_get_pixel(v, x, y) != (val ? colors[map4[val - 1]] : map_at(x, y, 0));
            }
        CHECK(bad == 0);
    }

    /* the label changed: once through the strips; straight, its box written twice, the map then the letters */
    panel_count_from_now(panel);
    dgx_ui_text_set(&a.ui, a.l, "6 m/s"), dgx_ui_text_set(&b.ui, b.l, "6 m/s");
    dgx_ui_paint(&a.ui, &pa, 1), dgx_ui_paint(&b.ui, &pv, 1);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) > 0 && differ(panel, v) == 0);
    pa.strips = NULL;
    panel_count_from_now(panel);
    dgx_ui_text_set(&a.ui, a.l, "7 m/s"), dgx_ui_text_set(&b.ui, b.l, "7 m/s");
    dgx_ui_paint(&a.ui, &pa, 2), dgx_ui_paint(&b.ui, &pv, 2);
    CHECK(panel_twice(panel) > 0 && differ(panel, v) == 0);
    pa.strips = &dgx_ui_strips;

    /* a popup whose root does not fill: the map in it, not the tree under it; closed, the tree again */
    {
        static const dgx_ui_widget_t pop_root = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4};
        static dgx_ui_node_t         pa_nodes[2], pb_nodes[2];
        static dgx_ui_t              popa, popb;
        static char                  ba[8], bb[8];
        static dgx_ui_textbox_t      ta, tb;
        dgx_ui_textbox_init(&ta, ba, sizeof ba, 1, &style), dgx_ui_textbox_init(&tb, bb, sizeof bb, 1, &style);
        static dgx_ui_widget_t wa, wb;
        wa = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &ta, .want_h = 12}, wb = wa, wb.text = &tb;
        dgx_ui_init(&popa, pa_nodes, 2, 1), dgx_ui_init(&popb, pb_nodes, 2, 1);
        dgx_ui_add(&popa, -1, &pop_root), dgx_ui_add(&popb, -1, &pop_root);
        dgx_ui_add(&popa, 0, &wa), dgx_ui_add(&popb, 0, &wb);
        dgx_ui_text_set(&popa, 1, "pop"), dgx_ui_text_set(&popb, 1, "pop");
        CHECK(dgx_ui_popup_open(&a.ui, &popa, 10, 20, 60, 30, 0) && dgx_ui_popup_open(&b.ui, &popb, 10, 20, 60, 30, 0));
        panel_count_from_now(panel);
        dgx_ui_paint(&a.ui, &pa, 3), dgx_ui_paint(&b.ui, &pv, 3);
        CHECK(panel_twice(panel) == 0 && differ(panel, v) == 0);
        const dgx_ui_node_t *pl  = dgx_ui_node(&popb, 1);
        int                  off = 0;
        for (int y = 20; y < 50; ++y)
            for (int x = 10; x < 70; ++x)
                if (!(x >= pl->x && x < pl->x + pl->w && y >= pl->y && y < pl->y + pl->h)) off += dgx_get_pixel(v, x, y) != map_at(x, y, 0);
        CHECK(off == 0);
        dgx_ui_popup_close(&a.ui), dgx_ui_popup_close(&b.ui);
        panel_count_from_now(panel);
        dgx_ui_paint(&a.ui, &pa, 4), dgx_ui_paint(&b.ui, &pv, 4);
        CHECK(panel_twice(panel) == 0 && differ(panel, v) == 0);
        dgx_ui_free(&popa), dgx_ui_free(&popb);
    }

    /* the keyboard's band over the lower half, then gone: the map and the widgets there again, once */
    pa.cover = pv.cover = dgx_ui_rect(0, 64, PANEL_W, PANEL_H - 64);
    dgx_ui_paint(&a.ui, &pa, 5), dgx_ui_paint(&b.ui, &pv, 5);
    pa.cover = pv.cover = dgx_ui_rect(0, 0, 0, 0);
    panel_count_from_now(panel);
    dgx_ui_paint(&a.ui, &pa, 6), dgx_ui_paint(&b.ui, &pv, 6);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * (PANEL_H - 64) && differ(panel, v) == 0);

    /* the map changed at two opposite corners: the two rectangles written and nothing else, every pixel once, as a
       whole new composing over the new map has them */
    {
        const dgx_ui_rect_t r1 = dgx_ui_rect(0, 0, 20, 20), r2 = dgx_ui_rect(PANEL_W - 20, PANEL_H - 20, 20, 20);
        map_fill(r1.x, r1.y, r1.w, r1.h, 7), map_fill(r2.x, r2.y, r2.w, r2.h, 7);
        panel_count_from_now(panel);
        dgx_ui_background_changed(&a.ui, &pa, r1.x, r1.y, r1.w, r1.h), dgx_ui_background_changed(&a.ui, &pa, r2.x, r2.y, r2.w, r2.h);
        dgx_ui_background_changed(&b.ui, &pv, r1.x, r1.y, r1.w, r1.h), dgx_ui_background_changed(&b.ui, &pv, r2.x, r2.y, r2.w, r2.h);
        /* the corner of the frame in the first rectangle not written: it fills, nothing of the map shows there */
        const dgx_ui_node_t *f      = dgx_ui_node(&a.ui, a.f);
        int                  hidden = 0;
        for (int y = r1.y; y < r1.y + r1.h; ++y)
            for (int x = r1.x; x < r1.x + r1.w; ++x) hidden += x >= f->x && x < f->x + f->w && y >= f->y && y < f->y + f->h;
        CHECK(hidden > 0 && written_beside(panel, r1, r2) == 0 && panel_written(panel) == 2 * 20 * 20 - hidden);
        CHECK(panel_twice(panel) == 0 && differ(panel, v) == 0);
        static uint8_t v2buffer[64];
        dgx_screen_t  *v2  = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t pv2 = {.scr = v2, .colors = colors, .color_count = 4, .background = 2, .buffer = v2buffer, .buffer_size = sizeof v2buffer};
        dgx_ui_background_set(&pv2, draw_map, map);
        dgx_ui_paint_area(&b.ui, &pv2, 0, 0, PANEL_W, PANEL_H);
        CHECK(differ(panel, v2) == 0);
        dgx_ui_paint_free(&pv2);
        v2->destroy(&v2);
        /* the next pass draws nothing: nothing was marked */
        panel_count_from_now(panel);
        CHECK(!dgx_ui_paint(&a.ui, &pa, 7) && panel_written(panel) == 0);
    }
    /* partly off the screen: its part on it; within a frame that fills: nothing */
    {
        map_fill(0, 40, 20, 10, 9);
        panel_count_from_now(panel);
        dgx_ui_background_changed(&a.ui, &pa, -10, 40, 30, 10), dgx_ui_background_changed(&b.ui, &pv, -10, 40, 30, 10);
        CHECK(written_beside(panel, dgx_ui_rect(0, 40, 20, 10), dgx_ui_rect(0, 0, 0, 0)) == 0 && panel_written(panel) == 20 * 10 && differ(panel, v) == 0);
        const dgx_ui_node_t *f = dgx_ui_node(&a.ui, a.f);
        panel_count_from_now(panel);
        dgx_ui_background_changed(&a.ui, &pa, f->x + 1, f->y + 1, f->w - 2, f->h - 2);
        CHECK(panel_written(panel) == 0);
    }
    /* no drawn background: the call does nothing */
    {
        dgx_ui_paint_t plain = pa;
        dgx_ui_background_set(&plain, NULL, NULL);
        CHECK(plain.background_draw == NULL && plain.background_ctx == NULL && plain.drawn == NULL);
        panel_count_from_now(panel);
        dgx_ui_background_changed(&a.ui, &plain, 0, 0, PANEL_W, PANEL_H);
        CHECK(panel_written(panel) == 0);
    }

    /* the picture of 4 bits in its strip: drawn with room left in the buffer for a row of it; with none left, its box
       the map, told once in the log */
    {
        const dgx_ui_node_t *n   = dgx_ui_node(&a.ui, a.p4);
        const size_t         row = (size_t)n->w * 2;
        pa.buffer_size           = row * (size_t)n->h + 6;
        dgx_ui_redraw(&a.ui, a.p4), dgx_ui_redraw(&b.ui, b.p4);
        dgx_ui_paint(&a.ui, &pa, 8), dgx_ui_paint(&b.ui, &pv, 8);
        CHECK(differ(panel, v) == 0);
        pa.buffer_size = row * (size_t)n->h + 5;
        panel_count_from_now(panel);
        dgx_ui_redraw(&a.ui, a.p4);
        dgx_ui_paint(&a.ui, &pa, 9);
        int off = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) off += panel->mem[y][x] != dgx_get_pixel(map, x, y);
        CHECK(off == 0 && panel_twice(panel) == 0);
        pa.buffer_size = sizeof buffer;
    }

    /* a dialog in the middle of the screen, its root given (dgx_ui_set_root()), the map changed over all of the screen:
       around the dialog the map, every pixel once, as the virtual screen has it */
    {
        static const dgx_ui_widget_t dlg_root = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4};
        static dgx_ui_node_t         da_nodes[2], db_nodes[2];
        static dgx_ui_t              da, db;
        static char                  ba[8], bb[8];
        static dgx_ui_textbox_t      ta, tb;
        static dgx_ui_widget_t       wa, wb;
        dgx_ui_textbox_init(&ta, ba, sizeof ba, 1, &style), dgx_ui_textbox_init(&tb, bb, sizeof bb, 1, &style);
        wa = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &ta, .want_h = 12}, wb = wa, wb.text = &tb;
        dgx_ui_init(&da, da_nodes, 2, 1), dgx_ui_init(&db, db_nodes, 2, 1);
        dgx_ui_add(&da, -1, &dlg_root), dgx_ui_add(&db, -1, &dlg_root);
        dgx_ui_add(&da, 0, &wa), dgx_ui_add(&db, 0, &wb);
        dgx_ui_text_set(&da, 1, "go?"), dgx_ui_text_set(&db, 1, "go?");
        dgx_ui_set_root(&da, 20, 30, 50, 30), dgx_ui_set_root(&db, 20, 30, 50, 30);
        dgx_ui_paint(&da, &pa, 10), dgx_ui_paint(&db, &pv, 10);
        map_fill(0, 0, PANEL_W, PANEL_H, 3);
        panel_count_from_now(panel);
        dgx_ui_background_changed(&da, &pa, 0, 0, PANEL_W, PANEL_H), dgx_ui_background_changed(&db, &pv, 0, 0, PANEL_W, PANEL_H);
        int off = 0;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x)
                if (!(x >= 20 && x < 70 && y >= 30 && y < 60)) off += panel->mem[y][x] != map_at(x, y, 3);
        CHECK(off == 0 && panel_written(panel) == PANEL_W * PANEL_H && panel_twice(panel) == 0 && differ(panel, v) == 0);
        dgx_ui_paint_free(&pa), dgx_ui_paint_free(&pv);
        dgx_ui_free(&da), dgx_ui_free(&db);
    }

    /* soft letters over the map: in a strip as on the virtual screen, their edges mixed with the map, not with the
       colour of the paper as on plain paper; straight on the panel, which cannot be read, with that colour */
    {
        make_soft_font();
        static dgx_ui_font_variant_t sv[1];
        static dgx_ui_text_style_t   ss;
        sv[0] = (dgx_ui_font_variant_t){.font = &soft_font};
        ss    = (dgx_ui_text_style_t){.fonts = sv, .count = 1, .flags = DGX_UI_STYLE_MIDDLE};
        static dgx_ui_node_t    sn[3][2];
        static dgx_ui_t         su[3];
        static char             sb[3][8];
        static dgx_ui_textbox_t st[3];
        static dgx_ui_widget_t  sw[3];
        static const dgx_ui_widget_t soft_root = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2};
        dgx_ui_paint_t *const        to[3] = {&pa, &pv, &pp};
        for (int k = 0; k < 3; ++k) {
            dgx_ui_textbox_init(&st[k], sb[k], sizeof sb[k], 1, &ss);
            sw[k] = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &st[k], .want_h = 10};
            dgx_ui_init(&su[k], sn[k], 2, 1);
            dgx_ui_add(&su[k], -1, &soft_root), dgx_ui_add(&su[k], 0, &sw[k]);
            dgx_ui_text_set(&su[k], 1, "ABAB");
            dgx_ui_set_root(&su[k], 10, 70, 60, 14);
        }
        map_fill(0, 0, PANEL_W, PANEL_H, 0);
        for (int k = 0; k < 3; ++k) dgx_ui_paint(&su[k], to[k], 20);
        const dgx_ui_node_t *n    = dgx_ui_node(&su[1], 1);
        int                  soft = 0, like_paper = 0, off = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) {
                const uint32_t c = dgx_get_pixel(v, x, y), q = dgx_get_pixel(plain, x, y);
                if (q != colors[1] && q != colors[2]) ++soft, like_paper += c == q; /* an edge on plain paper */
                off += panel->mem[y][x] != c;
            }
        CHECK(soft > 0 && like_paper == 0 && off == 0);
        pa.strips = NULL;
        dgx_ui_redraw(&su[0], 1);
        dgx_ui_paint(&su[0], &pa, 21);
        int halo = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) {
                const uint32_t q = dgx_get_pixel(plain, x, y);
                if (q != colors[1] && q != colors[2]) halo += panel->mem[y][x] == q; /* mixed with the paper's colour */
            }
        CHECK(halo == soft);
        pa.strips = &dgx_ui_strips;
        for (int k = 0; k < 3; ++k) dgx_ui_free(&su[k]);
    }

    dgx_ui_paint_free(&pa), dgx_ui_paint_free(&pv), dgx_ui_paint_free(&pp);
    dgx_ui_free(&a.ui), dgx_ui_free(&b.ui), dgx_ui_free(&c.ui);
    v->destroy(&v), map->destroy(&map), plain->destroy(&plain);
    free(panel);
    CHECK_DONE();
}
