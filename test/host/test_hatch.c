/*
 * The hatched fill on a screen of 1 bit, DGX_UI_HATCH in the table: a page of 128 x 64 of a frame whose fill and border
 * are hatched, a label on a hatched paper, a label shown inverted (ink 0) over it, two hatched boxes side by side, a
 * soft ring whose track is hatched, a level bar. The same page drawn on a screen of 8 bits of the table's indices says
 * which pixel is of which colour. Every pixel of the hatched colour is lit where (x + y) & 1 of the screen, the borders
 * of the frame and their corners, the ring's soft edges and the seam of the two boxes included; under letters the
 * paper is the opposite of their ink, and the letters show. A label written again, a popup over the page and closed,
 * the page composed onto a screen at an offset of 1 and of 2: each the same pixels as the page drawn whole. No pixel
 * written twice in a pass, counted by a screen that counts its writes. A scrolling box whose ground and child are
 * hatched, moved by 1, 3 and 2 rows: each the box drawn whole. A button whose face is hatched and a picture of 1 bit on
 * a hatched paper: under their letters and their ones the opposite of the ink. A page slid in and one crossfaded: they
 * run, and at their end the screen is the page drawn whole. On a screen of 16 bits DGX_UI_HATCH fills with 0.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_ui_transition.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"

enum { W = 128, H = 64, PAPER = 0, INK = 1, HATCH = 2 };

/* a screen of 1 bit that counts the writes of each pixel within its clip */
static uint8_t                     writes[H][W];
static dgx_scr_draw_pixel_func     own_pixel;
static dgx_scr_fill_rectangle_func own_fill;
static void                        count(dgx_screen_t *s, int x, int y, int w, int h)
{
    int cx, cy, cw, ch;
    dgx_screen_clip_get(s, &cx, &cy, &cw, &ch);
    for (int v = y; v < y + h; ++v)
        for (int u = x; u < x + w; ++u)
            if (u >= cx && u < cx + cw && v >= cy && v < cy + ch && u >= 0 && v >= 0 && u < W && v < H && writes[v][u] < 255) ++writes[v][u];
}
static void counted_pixel(dgx_screen_t *s, int x, int y, uint32_t c)
{
    count(s, x, y, 1, 1), own_pixel(s, x, y, c);
}
static void counted_fill(dgx_screen_t *s, int x, int y, int w, int h, uint32_t c)
{
    count(s, x, y, w, h), own_fill(s, x, y, w, h, c);
}
static dgx_screen_t *counting_screen(void)
{
    dgx_screen_t *s = dgx_vscreen_init(W, H, 1, DgxScreenRGB);
    own_pixel = s->set_pixel, own_fill = s->fill_rectangle;
    s->set_pixel = counted_pixel, s->fill_rectangle = counted_fill;
    return s;
}
static int label, inverse, ring;
static int twice(dgx_ui_t *ui)
{
    int n = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            bool in_label = false;
            for (int k = 0; ui && k < 2; ++k) {
                const dgx_ui_node_t *l = dgx_ui_node(ui, k ? inverse : label);
                in_label |= x >= l->x && x < l->x + l->w && y >= l->y && y < l->y + l->h;
            }
            n += writes[y][x] > 1 && !in_label;
        }
    return n;
}

/* the page */
static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static char                  text_buf[24], inverse_buf[8];
static dgx_ui_textbox_t      text_box, inverse_box;
static int32_t               ring_value = 600, bar_value = 40;
static const dgx_ui_frame_look_t  frame_params = {.border = HATCH, .width = 1};
static const dgx_ui_look_t        frame_look   = {.draw = dgx_ui_draw_frame, .params = &frame_params, .fill = HATCH};
static const dgx_ui_text_look_t   text_params = {.ink = INK, .paper = HATCH}, inverse_params = {.ink = PAPER, .paper = HATCH};
static const dgx_ui_look_t        text_look = {.draw = dgx_ui_draw_text, .params = &text_params, .fill = DGX_UI_PAPER};
static const dgx_ui_look_t        inverse_look = {.draw = dgx_ui_draw_text, .params = &inverse_params, .fill = DGX_UI_PAPER};
static const dgx_ui_ring_look_t   ring_params = {.fill = INK, .track = HATCH, .paper = DGX_UI_PAPER, .width = 5, .start = -135, .sweep = 270, .soft = &dgx_ui_ring_soft};
static const dgx_ui_look_t        ring_look   = {.draw = dgx_ui_draw_ring, .params = &ring_params, .fill = DGX_UI_PAPER};
static const dgx_ui_level_look_t  bar_params  = {.fill = INK, .track = HATCH, .from = DGX_UI_FROM_LEFT};
static const dgx_ui_look_t        bar_look    = {.draw = dgx_ui_draw_level, .params = &bar_params, .fill = DGX_UI_PAPER};
static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 0};
static const dgx_ui_widget_t left_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .gap = 2, .want_w = 70, .look = &frame_look};
static const dgx_ui_widget_t text_w = {.kind = DGX_UI_LABEL, .text = &text_box, .want_h = 14, .look = &text_look};
static const dgx_ui_widget_t inverse_w = {.kind = DGX_UI_LABEL, .text = &inverse_box, .want_h = 14, .look = &inverse_look};
static const dgx_ui_widget_t bar_w  = {.kind = DGX_UI_OWN, .value = &bar_value, .max = 100, .want_h = 7, .look = &bar_look};
static const dgx_ui_widget_t one_w  = {.kind = DGX_UI_FRAME, .want_w = 13, .look = &frame_look}; /* two hatched frames side by side */
static const dgx_ui_widget_t two_w  = {.kind = DGX_UI_FRAME, .want_w = 14, .look = &frame_look};
static const dgx_ui_widget_t ring_w = {.kind = DGX_UI_OWN, .value = &ring_value, .min = 400, .max = 2000, .want_w = 31, .look = &ring_look};

static dgx_ui_node_t nodes[12];

static void page(dgx_ui_t *ui)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
    dgx_ui_textbox_init(&text_box, text_buf, sizeof text_buf, 1, &style);
    dgx_ui_textbox_init(&inverse_box, inverse_buf, sizeof inverse_buf, 1, &style);
    dgx_ui_init(ui, nodes, 12, 1);
    dgx_ui_set_root(ui, 0, 0, W, H);
    const int root = dgx_ui_add(ui, -1, &root_w), left = dgx_ui_add(ui, root, &left_w);
    label = dgx_ui_add(ui, left, &text_w), inverse = dgx_ui_add(ui, left, &inverse_w);
    dgx_ui_add(ui, left, &bar_w);
    dgx_ui_add(ui, root, &one_w), dgx_ui_add(ui, root, &two_w);
    ring = dgx_ui_add(ui, root, &ring_w);
    dgx_ui_text_set(ui, label, "CO2 812");
    dgx_ui_text_set(ui, inverse, "ON");
}

static const uint32_t one_bit[3] = {0, 1, DGX_UI_HATCH}, indices[3] = {0, 1, 2};

/* the page composed whole onto a screen of bits with a table */
static dgx_screen_t *whole(dgx_ui_t *ui, int bits, const uint32_t *table)
{
    dgx_screen_t  *s = dgx_vscreen_init(W, H, (uint8_t)bits, DgxScreenRGB);
    dgx_ui_paint_t p = {.scr = s, .colors = table, .color_count = 3};
    dgx_ui_paint_area_to(ui, &p, s, NULL, 0, 0, W, H);
    dgx_ui_paint_free(&p);
    return s;
}

static int differ(dgx_screen_t *a, dgx_screen_t *b, int dx)
{
    int n = 0;
    for (int y = 0; y < H; ++y)
        for (int x = dx; x < W; ++x) n += dgx_get_pixel(a, x, y) != dgx_get_pixel(b, x - dx, y);
    return n;
}

/* the 1-bit screen against the indices, the labels apart: the hatched colour by parity, ink and paper as they are */
static int off_hatch(dgx_ui_t *ui, dgx_screen_t *one, dgx_screen_t *ind)
{
    int off = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const uint32_t i = dgx_get_pixel(ind, x, y), c = dgx_get_pixel(one, x, y);
            bool           in_label = false;
            for (int k = 0; k < 2; ++k) {
                const dgx_ui_node_t *n = dgx_ui_node(ui, k ? inverse : label);
                in_label |= x >= n->x && x < n->x + n->w && y >= n->y && y < n->y + n->h;
            }
            if (in_label) continue; /* letters_show() */
            if (i == HATCH) off += c != (uint32_t)((x + y) & 1);
            else if (i != HATCH) off += c != (i == INK);
        }
    return off;
}

/* a label on a hatched paper: each of its rows either hatched by parity (around the field) or the field, solid, the
   opposite of the ink; every row its letters reach a row of the field; the letters lit as their ink */
static bool letters_show(dgx_ui_t *ui, dgx_screen_t *one, dgx_screen_t *ind, int i, uint32_t ink)
{
    const dgx_ui_node_t *n       = dgx_ui_node(ui, i);
    int                  letters = 0, wrong = 0;
    for (int y = n->y; y < n->y + n->h; ++y) {
        int hatch = 0, solid = 0, inked = 0;
        for (int x = n->x; x < n->x + n->w; ++x) {
            const uint32_t v = dgx_get_pixel(ind, x, y), c = dgx_get_pixel(one, x, y);
            if (v == ink) inked += 1, wrong += c != ink;
            else if (v == HATCH) hatch += c == (uint32_t)((x + y) & 1), solid += c == (ink ? 0u : 1u);
        }
        const int paper = n->w - inked;
        letters += inked;
        wrong += inked ? solid != paper : (hatch != paper && solid != paper);
    }
    if (wrong) fprintf(stderr, "  label %d: %d of its rows or letters off, %d letters' pixels\n", i, wrong, letters);
    return letters > 20 && wrong == 0;
}

/* within the box of an element's ink, its letters or its picture's ones: the hatched colour solid, the opposite of
   the ink */
static bool solid_under(dgx_ui_t *ui, dgx_screen_t *one, dgx_screen_t *ind, int i, uint32_t ink)
{
    const dgx_ui_node_t *n  = dgx_ui_node(ui, i);
    int                  x0 = W, y0 = H, x1 = -1, y1 = -1, wrong = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x)
            if (dgx_get_pixel(ind, x, y) == ink) x0 = x < x0 ? x : x0, y0 = y < y0 ? y : y0, x1 = x > x1 ? x : x1, y1 = y > y1 ? y : y1;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const uint32_t v = dgx_get_pixel(ind, x, y), c = dgx_get_pixel(one, x, y);
            wrong += v == ink ? c != ink : v == HATCH ? c != (ink ? 0u : 1u) : 0;
        }
    if (wrong || x1 < 0) fprintf(stderr, "  element %d: %d pixels under its ink off\n", i, wrong);
    return x1 > x0 && wrong == 0;
}

int main(void)
{
    static dgx_ui_t ui;
    page(&ui);
    /* drawn whole, by a pass, on the counting screen: no pixel twice */
    dgx_screen_t  *screen = counting_screen();
    dgx_ui_paint_t paint  = {.scr = screen, .colors = one_bit, .color_count = 3};
    memset(writes, 0, sizeof writes);
    dgx_ui_paint(&ui, &paint, 0);
    CHECK(twice(&ui) == 0);
    dgx_screen_t *ind = whole(&ui, 8, indices), *one = whole(&ui, 1, one_bit);
    CHECK(differ(screen, one, 0) == 0);
    /* (b, g, j) the hatch by the screen's parity: the frame's fill and border and corners, the boxes and their seam, the
       ring's track and its soft edges, the bar's track */
    CHECK(off_hatch(&ui, one, ind) == 0);
    int hatched = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) hatched += dgx_get_pixel(ind, x, y) == HATCH;
    CHECK(hatched > 2000);
    /* (e, f) letters on a hatched paper: on paper 0, and inverted, ink 0 on 1 */
    CHECK(letters_show(&ui, one, ind, label, 1) && letters_show(&ui, one, ind, inverse, 0));
    /* (a) a label written again, the ring and the bar moved: the pass the same pixels as the page composed anew */
    dgx_ui_text_set(&ui, label, "CO2 1490"), ring_value = 1490, bar_value = 77;
    memset(writes, 0, sizeof writes);
    dgx_ui_paint(&ui, &paint, 10);
    CHECK(twice(&ui) == 0);
    dgx_screen_destroy(&ind), dgx_screen_destroy(&one);
    ind = whole(&ui, 8, indices), one = whole(&ui, 1, one_bit);
    CHECK(differ(screen, one, 0) == 0 && off_hatch(&ui, one, ind) == 0);
    /* (c) a popup over the page and closed; the page composed at an offset of 1 and of 2 */
    static dgx_ui_node_t         popup_nodes[1];
    static const dgx_ui_widget_t popup_w = {.kind = DGX_UI_FRAME, .look = &dgx_ui_look_frame};
    dgx_ui_t                     popup;
    dgx_ui_init(&popup, popup_nodes, 1, 1), dgx_ui_add(&popup, -1, &popup_w);
    CHECK(dgx_ui_popup_open(&ui, &popup, 21, 9, 37, 23, 0));
    dgx_ui_paint(&ui, &paint, 20);
    dgx_ui_popup_close(&ui);
    memset(writes, 0, sizeof writes);
    dgx_ui_paint(&ui, &paint, 30);
    CHECK(differ(screen, one, 0) == 0 && twice(&ui) == 0);
    dgx_ui_free(&popup);
    for (int dx = 1; dx <= 2; ++dx) {
        dgx_screen_t  *at = dgx_vscreen_init(W - dx, H, 1, DgxScreenRGB);
        dgx_ui_paint_t p  = {.scr = at, .colors = one_bit, .color_count = 3};
        dgx_ui_paint_area_to(&ui, &p, at, NULL, dx, 0, W - dx, H);
        dgx_ui_paint_free(&p);
        CHECK(differ(one, at, dx) == 0);
        dgx_screen_destroy(&at);
    }
    dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    dgx_screen_destroy(&screen), dgx_screen_destroy(&ind), dgx_screen_destroy(&one);

    /* (h) a scrolling box of a hatched ground and a hatched child, moved by 1, 3 and 2 rows: the box drawn whole */
    {
        static int32_t                    offset;
        static const dgx_ui_scroll_look_t bar = {.track = HATCH, .knob = INK, .focused = DGX_UI_PAPER, .arrows = DGX_UI_PAPER, .width = 3};
        static const dgx_ui_look_t        scroll_look = {.draw = dgx_ui_draw_scroll, .params = &bar, .fill = HATCH, .behavior = &dgx_ui_scroll_behavior};
        static const dgx_ui_widget_t      box_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN | DGX_UI_SCROLL, .gap = 3, .want_h = 40, .value = &offset, .look = &scroll_look};
        static const dgx_ui_widget_t      row_w = {.kind = DGX_UI_FRAME, .want_h = 11, .look = &frame_look};
        static const dgx_ui_widget_t      top_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 5};
        static dgx_ui_node_t              scroll_nodes[12];
        dgx_ui_t                          tree;
        dgx_ui_init(&tree, scroll_nodes, 12, 1);
        dgx_ui_set_root(&tree, 0, 0, W, H);
        const int top = dgx_ui_add(&tree, -1, &top_w), box = dgx_ui_add(&tree, top, &box_w);
        for (int k = 0; k < 8; ++k) dgx_ui_add(&tree, box, &row_w);
        dgx_screen_t  *s = dgx_vscreen_init(W, H, 1, DgxScreenRGB);
        dgx_ui_paint_t p = {.scr = s, .colors = one_bit, .color_count = 3};
        offset           = 0;
        dgx_ui_paint(&tree, &p, 0);
        const int moves[3] = {1, 3, 2};
        for (int k = 0; k < 3; ++k) {
            offset += moves[k];
            dgx_ui_paint(&tree, &p, 10 * (uint32_t)(k + 1));
            dgx_screen_t *fresh = whole(&tree, 1, one_bit);
            const int     off   = differ(s, fresh, 0);
            CHECK(off == 0);
            if (off) fprintf(stderr, "  scrolled by %d: %d pixels off the box drawn whole\n", moves[k], off);
            dgx_screen_destroy(&fresh);
        }
        dgx_ui_paint_free(&p), dgx_ui_free(&tree);
        dgx_screen_destroy(&s);
        (void)box;
    }

    /* a button whose face is hatched, and a picture of 1 bit on a hatched paper: under the letters and the picture the
       opposite of the ink */
    {
        static const dgx_ui_button_look_t  button_params = {.face = HATCH, .pressed = INK, .focused = DGX_UI_PAPER, .disabled = HATCH, .ink = INK,
                                                            .ink_disabled = INK, .shadow = DGX_UI_PAPER, .depth = 0};
        static const dgx_ui_look_t         button_look = {.draw = dgx_ui_draw_button, .params = &button_params, .fill = DGX_UI_PAPER};
        static const dgx_ui_picture_look_t picture_params = {.ink = INK, .paper = HATCH};
        static const dgx_ui_look_t         picture_look = {.draw = dgx_ui_draw_picture, .params = &picture_params, .fill = DGX_UI_PAPER};
        static const uint8_t               arrow[8] = {0x10, 0x30, 0x7F, 0xFF, 0x7F, 0x30, 0x10, 0x00};
        static const dgx_image_t           arrow_image = {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = arrow};
        static const dgx_ui_picture_t      arrow_pic = {.image = &arrow_image};
        static dgx_ui_textbox_t            ok_box;
        static const dgx_ui_widget_t       column_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4, .gap = 4};
        static const dgx_ui_widget_t       button_w = {.kind = DGX_UI_BUTTON, .text = &ok_box, .want_h = 20, .look = &button_look};
        static const dgx_ui_widget_t       picture_w = {.kind = DGX_UI_PICTURE, .picture = &arrow_pic, .want_h = 16, .look = &picture_look};
        static dgx_ui_node_t               two_nodes[4];
        dgx_ui_t                           two;
        ok_box = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST("OK", 1, &style);
        dgx_ui_init(&two, two_nodes, 4, 1);
        dgx_ui_set_root(&two, 0, 0, W, H);
        dgx_ui_add(&two, -1, &column_w);
        const int button = dgx_ui_add(&two, 0, &button_w), picture = dgx_ui_add(&two, 0, &picture_w);
        dgx_screen_t *b_one = whole(&two, 1, one_bit), *b_ind = whole(&two, 8, indices);
        CHECK(solid_under(&two, b_one, b_ind, button, 1) && solid_under(&two, b_one, b_ind, picture, 1));
        dgx_ui_free(&two);
        dgx_screen_destroy(&b_one), dgx_screen_destroy(&b_ind);
    }

    /* a page slid in and one crossfaded on a screen of 1 bit: they run, and at their end the screen is the page drawn
       whole, its hatch in step with the screen's */
    for (int kind = 0; kind < 2; ++kind) {
        static const dgx_ui_widget_t home_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 8};
        static const dgx_ui_widget_t inner_w = {.kind = DGX_UI_FRAME, .want_h = 20, .look = &frame_look};
        static const dgx_ui_widget_t page_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .look = &frame_look};
        static dgx_ui_node_t         home_nodes[2], page_nodes[1];
        static uint8_t               rows[W * 8];
        dgx_ui_t                     home, next;
        dgx_ui_init(&home, home_nodes, 2, 1), dgx_ui_set_root(&home, 0, 0, W, H);
        dgx_ui_add(&home, -1, &home_w), dgx_ui_add(&home, 0, &inner_w);
        dgx_ui_init(&next, page_nodes, 1, 1), dgx_ui_add(&next, -1, &page_w);
        dgx_screen_t       *s  = dgx_vscreen_init(W, H, 1, DgxScreenRGB);
        dgx_ui_transition_t tr = {.from = dgx_vscreen_init(W, H, 1, DgxScreenRGB), .to = dgx_vscreen_init(W, H, 1, DgxScreenRGB), .ms = 200};
        dgx_ui_paint_t      p  = {.scr = s, .colors = one_bit, .color_count = 3, .buffer = rows, .buffer_size = sizeof rows};
        dgx_ui_paint(&home, &p, 0);
        CHECK(dgx_ui_page_open_by(&home, &next, &p, &tr, kind ? DGX_UI_CROSSFADE : DGX_UI_SLIDE_LEFT));
        int frames = 0;
        for (uint32_t t = 10; tr.running && t < 1000; t += 20) dgx_ui_transition_frame(&tr, &p, t), ++frames;
        dgx_ui_paint(&home, &p, 1000);
        dgx_screen_t *fresh = whole(&home, 1, one_bit);
        CHECK(!tr.running && frames > 5 && differ(s, fresh, 0) == 0);
        dgx_ui_paint_free(&p), dgx_ui_free(&next), dgx_ui_free(&home);
        dgx_screen_destroy(&s), dgx_screen_destroy(&fresh), dgx_screen_destroy(&tr.from), dgx_screen_destroy(&tr.to);
    }

    /* (i) on a screen of 16 bits the hatch is an error of the table: 0 */
    {
        page(&ui);
        const uint32_t deep[3] = {0x1234, 0xffff, DGX_UI_HATCH};
        dgx_screen_t  *s       = whole(&ui, 16, deep);
        dgx_screen_t  *ix      = whole(&ui, 8, indices);
        int            off     = 0;
        const dgx_ui_node_t *r = dgx_ui_node(&ui, ring); /* its soft edges mixed of the colours, the hatch's as 0 */
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                if (dgx_get_pixel(ix, x, y) == HATCH && !(x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h)) {
                    if (dgx_get_pixel(s, x, y) != 0 && off < 5) fprintf(stderr, "  16 bits at %d,%d: %04x\n", x, y, (unsigned)dgx_get_pixel(s, x, y));
                    off += dgx_get_pixel(s, x, y) != 0;
                }
        CHECK(off == 0);
        dgx_ui_free(&ui);
        dgx_screen_destroy(&s), dgx_screen_destroy(&ix);
    }
    CHECK_DONE();
}
