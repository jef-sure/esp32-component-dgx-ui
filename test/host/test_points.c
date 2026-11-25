/*
 * A bitmap variant whose pixel is a point (dgx_ui_font_variant_t::points): at a unit of 1, 2 and 4 a label's text the
 * same as DGX draws it at that scale, every pixel of the font a square; the line's height, the layout's natural
 * height and the text box's height so many times the font's; the ellipsis cut by widths so many times; a ladder of a
 * variant of points and one at the font's own size choosing by the widths it will have. The look of lines, and that of
 * handwritten lines over a bitmap variant, at a unit of 2 the same as at a unit of 1 in a box so many times smaller,
 * blown up; a list's rows and an open dropdown's a line of the text high. Every other look of a text, a button, a
 * field, a check, a radio, a list and a dropdown, draws the letter of a unit of 1 so many times larger; in the middle
 * of a box, the cell so many times high centred. Over a drawn background a text set anew leaves nothing of the old one.
 * A handwritten variant's height and pen are points: at a unit of 2 a line twice as tall, pixel for pixel the variant
 * of twice the height and pen at a unit of 1, straight and into a canvas. A handwritten variant reads the field as
 * nothing; a soft font of 4 bits gives squares of its coverage.
 */
#define PANEL_W 320
#define PANEL_H 240
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"
#include "panel.h"

static uint32_t       colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};
static const uint16_t PAPER = 0x0000, INK = 0xffff;

static dgx_ui_node_t         nodes[8];
static dgx_ui_t              ui;
static char                  buf[64];
static dgx_ui_textbox_t      box;
static dgx_ui_widget_t       text_w;
static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
static int                   text_i;

/* a font of two letters, A and B, their pixels of 16 levels, as test_soft.c makes it */
enum { A_WIDTH = 5, A_HEIGHT = 4, B_WIDTH = 6, B_HEIGHT = 5 };
static const uint8_t levels_a[A_HEIGHT][A_WIDTH] = {{0, 3, 15, 3, 0}, {2, 15, 0, 15, 2}, {15, 15, 15, 15, 15}, {15, 7, 0, 6, 15}};
static const uint8_t levels_b[B_HEIGHT][B_WIDTH] = {{1, 0, 0, 0, 0, 14}, {0, 8, 15, 15, 8, 0}, {0, 15, 0, 0, 15, 0}, {0, 8, 15, 15, 8, 0}, {14, 0, 0, 0, 0, 1}};
static uint8_t           soft_bits[2][64];
static dgx_glyph_t       soft_glyphs[2];
static dgx_glyph_array_t soft_ranges[2];
static dgx_font_t        soft_font;

static void make_soft_font(void)
{
    for (int letter = 0; letter < 2; ++letter) {
        const int width = letter ? B_WIDTH : A_WIDTH, height = letter ? B_HEIGHT : A_HEIGHT, pitch = (width * 4 + 7) / 8;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                soft_bits[letter][y * pitch + x / 2] |= (uint8_t)((letter ? levels_b[y][x] : levels_a[y][x]) << (x % 2 ? 0 : 4));
        soft_glyphs[letter] = (dgx_glyph_t){.offset = (uint32_t)(letter * sizeof soft_bits[0]), .width = (int16_t)width, .height = (int16_t)height,
                                            .x_advance = 7, .x_offset = 0, .y_offset = (int16_t)(letter ? -5 : -4)};
    }
    soft_ranges[0] = (dgx_glyph_array_t){.first = 'A', .number = 2, .glyph = 0};
    soft_font      = (dgx_font_t){.glyph_ranges = soft_ranges, .glyphs = soft_glyphs, .data = soft_bits[0], .y_advance = 8, .y_offset_lowest = -5,
                                  .x_widest = B_WIDTH, .x_width_average = 6, .f_type = DGX_FONT_BITMAP_DEPTH4, .y_bottom_max = 1, .x_right_max = 6,
                                  .number_of_ranges = 1};
}

/* a tree of a widget of a kind and a look over the text in a style, wanting w x h points, at a unit */
static uint16_t make_flags; /* the flags of the widget the next make makes: DGX_UI_CANVAS for the handwriting into a canvas */

static void make_of(int kind, const dgx_ui_look_t *look, const dgx_ui_text_style_t *style, const char *text, int w, int h, int unit, int max)
{
    static bool made;
    if (made) dgx_ui_free(&ui);
    made = true;
    static int32_t value;
    dgx_ui_textbox_init(&box, buf, sizeof buf, 1, style);
    text_w = (dgx_ui_widget_t){.kind = (uint8_t)kind, .flags = make_flags, .text = &box, .look = look, .want_w = (int16_t)w, .want_h = (int16_t)h,
                               .value = &value, .max = (int16_t)max};
    dgx_ui_init(&ui, nodes, 8, unit);
    dgx_ui_add(&ui, -1, &root_w);
    text_i = dgx_ui_add(&ui, 0, &text_w);
    dgx_ui_text_set(&ui, text_i, text);
}

static void make(int kind, const dgx_ui_look_t *look, const dgx_ui_text_style_t *style, const char *text, int w, int h, int unit)
{
    make_of(kind, look, style, text, w, h, unit, 1);
}

static void paint_on(dgx_screen_t *scr)
{
    dgx_fill_rectangle(scr, 0, 0, scr->width, scr->height, PAPER);
    dgx_ui_paint_t p = {.scr = scr, .colors = colors, .color_count = 4};
    dgx_ui_paint(&ui, &p, 0);
    dgx_ui_paint_free(&p);
}

/* the box of the pixels of a colour, or of every pixel not of a colour, within a widget: false when none */
static bool ink_box(dgx_screen_t *scr, const dgx_ui_node_t *n, uint16_t colour, bool other_than, int out[4])
{
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x)
            if ((dgx_get_pixel(scr, x, y) == colour) != other_than) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    out[0] = x0, out[1] = y0, out[2] = x1 - x0 + 1, out[3] = y1 - y0 + 1;
    return x1 >= 0;
}

/* a widget at a unit against the same at a unit of 1, its box so many times larger: the pixels where its picture is not
   the small one blown up; -1 when the box is not so many times larger */
static int off_blown_up(int kind, const dgx_ui_look_t *look, const dgx_ui_text_style_t *style, const char *text, int w, int h, int max,
                        int unit, dgx_screen_t *small_on, dgx_screen_t *large_on)
{
    make_of(kind, look, style, text, w, h, 1, max);
    paint_on(small_on);
    const dgx_ui_node_t small = *dgx_ui_node(&ui, text_i);
    make_of(kind, look, style, text, w, h, unit, max);
    paint_on(large_on);
    const dgx_ui_node_t *n = dgx_ui_node(&ui, text_i);
    if (n->w != unit * small.w || n->h != unit * small.h) return -1;
    int apart = 0;
    for (int y = 0; y < n->h; ++y)
        for (int x = 0; x < n->w; ++x)
            apart += dgx_get_pixel(large_on, n->x + x, n->y + y) != dgx_get_pixel(small_on, small.x + x / unit, small.y + y / unit);
    return apart;
}

/* the box of the pixels where two screens differ within a widget: false when none */
static bool differ_box(dgx_screen_t *one, dgx_screen_t *other, const dgx_ui_node_t *n, int out[4])
{
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x)
            if (dgx_get_pixel(one, x, y) != dgx_get_pixel(other, x, y)) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    out[0] = x0, out[1] = y0, out[2] = x1 - x0 + 1, out[3] = y1 - y0 + 1;
    return x1 >= 0;
}

/* every pixel of a rectangle the same as the top left one of its square of s x s, counted from its corner */
static bool squares(dgx_screen_t *scr, const int r[4], int s)
{
    for (int y = r[1]; y < r[1] + r[3]; ++y)
        for (int x = r[0]; x < r[0] + r[2]; ++x)
            if (dgx_get_pixel(scr, x, y) != dgx_get_pixel(scr, r[0] + (x - r[0]) / s * s, r[1] + (y - r[1]) / s * s)) return false;
    return r[2] % s == 0 && r[3] % s == 0;
}

static int glyph_width(const dgx_font_t *f, const char *s)
{
    const dgx_font_context_t c = {.font = f};
    return dgx_font_bounds(&c, s, NULL, NULL);
}

/* a drawn background: a pattern of no colour of the table, set straight on the screen */
static uint16_t pattern(int x, int y) { return (uint16_t)(((x * 3) & 0x1f) << 11 | ((y * 2) & 0x1f) << 5 | ((x + y) & 0x0f) | 0x10); }
static void     draw_pattern(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    (void)ctx;
    for (int row = y; row < y + h; ++row)
        for (int column = x; column < x + w; ++column) dgx_set_pixel(paint->scr, column - paint->ox, row - paint->oy, pattern(column, row));
}

int main(void)
{
    make_soft_font();
    const dgx_font_t           *terminus = &TerminusTTFMedium12;
    const dgx_ui_font_variant_t points[1] = {{.font = terminus, .points = true}}, own[1] = {{.font = terminus}};
    const dgx_ui_font_variant_t ladder[2] = {{.font = terminus, .points = true}, {.font = terminus}};
    const dgx_ui_text_style_t   in_points = {.fonts = points, .count = 1}, in_own = {.fonts = own, .count = 1};
    const dgx_ui_text_style_t   middle = {.fonts = points, .count = 1, .flags = DGX_UI_STYLE_MIDDLE}, two = {.fonts = ladder, .count = 2};
    dgx_screen_t               *v = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), *ref = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);

    /* a label at a unit of 1, 2 and 4: as DGX draws the text at that scale; its height the font's line so many times */
    for (int unit = 1; unit <= 4; unit *= 2) {
        make(DGX_UI_LABEL, &dgx_ui_look_text, &in_points, "Hi 42", 0, 0, unit);
        paint_on(v);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, text_i);
        CHECK(n->h == terminus->y_advance * unit && dgx_ui_textbox_height(&ui, &box) == n->h);
        CHECK(dgx_ui_variant_line(&ui, &points[0]) == terminus->y_advance * unit && dgx_ui_variant_line(&ui, &own[0]) == terminus->y_advance);
        dgx_fill_rectangle(ref, 0, 0, PANEL_W, PANEL_H, PAPER);
        const dgx_font_context_t c = {.scr = ref, .font = terminus, .color = INK, .bg = PAPER, .scale = unit, .field = n->w};
        dgx_font_draw(&c, n->x, n->y - terminus->y_offset_lowest * unit, "Hi 42");
        int apart = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) apart += dgx_get_pixel(v, x, y) != dgx_get_pixel(ref, x, y);
        CHECK(apart == 0);
        int ink[4];
        CHECK(ink_box(v, n, INK, false, ink) && squares(v, ink, unit));
        /* the same text in the font's own size: a line of the font's height */
        make(DGX_UI_LABEL, &dgx_ui_look_text, &in_own, "Hi 42", 0, 0, unit);
        paint_on(v);
        CHECK(dgx_ui_node(&ui, text_i)->h == terminus->y_advance);
    }

    /* the ellipsis: cut by the widths at the scale, as DGX's scaled draw of the cut text */
    {
        const int unit = 3, field = (glyph_width(terminus, "abcdef") + 2) * unit;
        make(DGX_UI_LABEL, &dgx_ui_look_text, &in_points, "abcdefghij", field / unit, 0, unit);
        paint_on(v);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, text_i);
        CHECK(n->w == field);
        char cut[32] = "";
        for (int k = 10; k > 0; --k) { /* the longest beginning that goes with its dots */
            char try_[32];
            memcpy(try_, "abcdefghij", (size_t)k), try_[k] = 0;
            if ((glyph_width(terminus, try_) + glyph_width(terminus, "...")) * unit <= field) {
                snprintf(cut, sizeof cut, "%s...", try_);
                break;
            }
        }
        dgx_fill_rectangle(ref, 0, 0, PANEL_W, PANEL_H, PAPER);
        const dgx_font_context_t c = {.scr = ref, .font = terminus, .color = INK, .bg = PAPER, .scale = unit, .field = n->w};
        dgx_font_draw(&c, n->x, n->y - terminus->y_offset_lowest * unit, cut);
        int apart = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) apart += dgx_get_pixel(v, x, y) != dgx_get_pixel(ref, x, y);
        CHECK(cut[0] && apart == 0);
    }

    /* a ladder: the variant of points taken when its widths, so many times the font's, go in; else the next */
    {
        const int unit = 3, text = glyph_width(terminus, "Wide");
        make(DGX_UI_LABEL, &dgx_ui_look_text, &two, "Wide", text, 0, unit); /* text x unit pixels wide */
        paint_on(v);
        CHECK(box.variant == 0);
        make(DGX_UI_LABEL, &dgx_ui_look_text, &two, "Wide", text - 1, 0, unit); /* too narrow by a point */
        paint_on(v);
        CHECK(box.variant == 1);
    }

    /* the look of lines, and the look of handwritten lines over a bitmap variant: at a unit of 2 in a box of w x h the
       same as at a unit of 1 in one of w/2 x h/2, blown up, its words laid alike; a list of three rows, two shown */
    {
        static const char words[] = "aa bb cc dd ee ff gg";
        CHECK(off_blown_up(DGX_UI_LABEL, &dgx_ui_look_lines, &in_points, words, 40, 40, 1, 2, ref, v) == 0);
        const int lines = box.total_lines;
        make(DGX_UI_LABEL, &dgx_ui_look_lines, &in_points, words, 40, 40, 1);
        paint_on(ref);
        CHECK(lines > 3 && box.total_lines == lines);
        CHECK(off_blown_up(DGX_UI_LABEL, &dgx_ui_look_hand_lines, &in_points, words, 40, 40, 1, 2, ref, v) == 0);
        make(DGX_UI_LABEL, &dgx_ui_look_hand_lines, &in_points, words, 40, 40, 2);
        dgx_screen_t *hand_on = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        paint_on(hand_on);
        make(DGX_UI_LABEL, &dgx_ui_look_lines, &in_points, words, 40, 40, 2);
        paint_on(v);
        int apart = 0;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) apart += dgx_get_pixel(v, x, y) != dgx_get_pixel(hand_on, x, y);
        CHECK(apart == 0);
        hand_on->destroy(&hand_on);
    }

    /* a list of three rows in a box of two lines: its rows a line of its text high, so many times the font's, the
       second row's letters within the second, as high as at a unit of 1 so many times */
    {
        int heights[2];
        for (int at = 0; at < 2; ++at) {
            const int unit = at ? 3 : 1, line = terminus->y_advance * unit;
            make_of(DGX_UI_LIST, &dgx_ui_look_list, &in_points, "Aa\nBb\nCc", 40, 2 * terminus->y_advance, unit, 2);
            paint_on(v);
            const dgx_ui_node_t *n      = dgx_ui_node(&ui, text_i);
            dgx_ui_node_t        second = *n;
            second.y += line, second.h = line;
            int ink[4];
            CHECK(n->h == 2 * line && ink_box(v, &second, INK, false, ink));
            heights[at] = ink[3];
        }
        CHECK(heights[1] == 3 * heights[0]);
    }

    /* an open dropdown: its rows as high as a line of its text, so many times the font's, under a dropdown lower */
    {
        const int unit = 3;
        make_of(DGX_UI_DROPDOWN, &dgx_ui_look_dropdown, &in_points, "A\nB\nC", 40, 4, unit, 2);
        paint_on(v);
        static dgx_ui_dropdown_t dd;
        const dgx_ui_node_t     *n = dgx_ui_node(&ui, text_i);
        CHECK(dgx_ui_dropdown_open(&ui, text_i, &dd, NULL, NULL));
        CHECK(dd.tree.root_rect.y == n->y + n->h && dd.tree.root_rect.h == 3 * terminus->y_advance * unit + 2 * unit);
        dgx_ui_popup_close(&ui);
    }

    /* a ladder of a variant of points and one at the font's own size in the look of lines and in that of handwritten
       lines: words that go into the box in lines of the font's own size, not in lines twice as wide and high, take the
       second; fewer words, the first */
    {
        static const char many[] = "aa bb cc dd ee ff gg hh", few[] = "aa bb cc";
        const dgx_ui_look_t *ladders[] = {&dgx_ui_look_lines, &dgx_ui_look_hand_lines};
        for (int k = 0; k < 2; ++k) {
            make(DGX_UI_LABEL, ladders[k], &two, many, 40, 40, 2);
            paint_on(v);
            const int fewer_fit = box.variant;
            make(DGX_UI_LABEL, ladders[k], &two, few, 40, 40, 2);
            paint_on(v);
            if (fewer_fit != 1 || box.variant != 0) fprintf(stderr, "  ladder %d: variants %d and %d\n", k, fewer_fit, box.variant);
            CHECK(fewer_fit == 1 && box.variant == 0);
        }
        /* by the height alone: one word a line, as many lines as go in at the font's own size */
        for (int k = 0; k < 2; ++k) {
            make(DGX_UI_LABEL, ladders[k], &two, "a b c d", 6, 4 * terminus->y_advance - 1, 2);
            paint_on(v);
            const int tall = box.variant;
            make(DGX_UI_LABEL, ladders[k], &two, "a b", 6, 4 * terminus->y_advance - 1, 2);
            paint_on(v);
            if (tall != 1 || box.variant != 0) fprintf(stderr, "  ladder by height %d: variants %d and %d\n", k, tall, box.variant);
            CHECK(tall == 1 && box.variant == 0);
        }
    }

    /* on a panel behind a bus, every look of a text of points: every pixel written once, at the first pass and when its
       text is set anew */
    {
        panel_t       *panel = panel_make();
        dgx_ui_paint_t onto  = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        static const struct {
            int                  kind;
            const dgx_ui_look_t *look;
        } every[] = {{DGX_UI_LABEL, &dgx_ui_look_text}, {DGX_UI_LABEL, &dgx_ui_look_lines}, {DGX_UI_LABEL, &dgx_ui_look_hand_lines},
                     {DGX_UI_BUTTON, &dgx_ui_look_button}, {DGX_UI_INPUT, &dgx_ui_look_input}, {DGX_UI_SWITCH, &dgx_ui_look_checkbox},
                     {DGX_UI_RADIO, &dgx_ui_look_radio}, {DGX_UI_LIST, &dgx_ui_look_list}, {DGX_UI_DROPDOWN, &dgx_ui_look_dropdown}};
        for (size_t k = 0; k < sizeof every / sizeof *every; ++k) {
            make(every[k].kind, every[k].look, &middle, "Hg", 30, 2 * terminus->y_advance, 3);
            panel_count_from_now(panel);
            dgx_ui_paint(&ui, &onto, 0);
            const int first = panel_twice(panel);
            dgx_ui_text_set(&ui, text_i, "gH");
            panel_count_from_now(panel);
            dgx_ui_paint(&ui, &onto, 100);
            const int again = panel_twice(panel);
            if (first || again) fprintf(stderr, "  look %zu on a panel: %d and %d pixels twice\n", k, first, again);
            CHECK(first == 0 && again == 0);
            dgx_ui_paint_free(&onto);
            onto = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        }
        free(panel);
    }

    /* every other look of a text: where "H" differs from " " at a unit of 3, the letter of a unit of 1 three times
       larger, pixel by pixel */
    {
        static const struct {
            int                  kind;
            const dgx_ui_look_t *look;
            int                  w, h;
        } looks[] = {
            {DGX_UI_BUTTON, &dgx_ui_look_button, 40, 20}, {DGX_UI_INPUT, &dgx_ui_look_input, 40, 16}, {DGX_UI_SWITCH, &dgx_ui_look_checkbox, 40, 16},
            {DGX_UI_RADIO, &dgx_ui_look_radio, 40, 16},   {DGX_UI_LIST, &dgx_ui_look_list, 40, 16},   {DGX_UI_DROPDOWN, &dgx_ui_look_dropdown, 50, 16},
            {DGX_UI_LABEL, &dgx_ui_look_text, 40, 16},    {DGX_UI_LABEL, &dgx_ui_look_lines, 40, 16},
        };
        for (size_t k = 0; k < sizeof looks / sizeof *looks; ++k) {
            int      letter[2][4];
            uint16_t small[24][24];
            bool     right = true;
            for (int at = 0; at < 2; ++at) {
                const int unit = at ? 3 : 1;
                make(looks[k].kind, looks[k].look, &in_points, " ", looks[k].w, looks[k].h, unit);
                paint_on(ref);
                make(looks[k].kind, looks[k].look, &in_points, "H", looks[k].w, looks[k].h, unit);
                paint_on(v);
                right &= differ_box(v, ref, dgx_ui_node(&ui, text_i), letter[at]);
                if (!right) break;
                for (int y = 0; y < letter[at][3]; ++y)
                    for (int x = 0; x < letter[at][2]; ++x) {
                        const uint16_t pixel = (uint16_t)dgx_get_pixel(v, letter[at][0] + x, letter[at][1] + y);
                        if (!at && x < 24 && y < 24) small[y][x] = pixel;
                        if (at) right &= pixel == small[y / unit][x / unit];
                    }
            }
            right &= letter[1][2] == 3 * letter[0][2] && letter[1][3] == 3 * letter[0][3] && letter[0][2] <= 24 && letter[0][3] <= 24;
            if (!right)
                fprintf(stderr, "  look %zu: the letter %dx%d at a unit of 3, %dx%d at 1\n", k, letter[1][2], letter[1][3], letter[0][2], letter[0][3]);
            CHECK(right);
        }
    }

    /* in the middle of the box (DGX_UI_STYLE_MIDDLE): the font's cell, so many times high, centred; the letter in it as
       many times further down from its top */
    {
        int depth[2], height[2];
        for (int at = 0; at < 2; ++at) {
            const int unit = at + 1;
            make(DGX_UI_LABEL, &dgx_ui_look_text, &middle, "H", 40, 40, unit);
            paint_on(v);
            const dgx_ui_node_t *n = dgx_ui_node(&ui, text_i);
            const int            cell = (terminus->y_bottom_max - terminus->y_offset_lowest) * unit;
            int                  drawn[4];
            CHECK(ink_box(v, n, PAPER, true, drawn) && squares(v, drawn, unit));
            depth[at] = drawn[1] - (n->y + (n->h - cell) / 2), height[at] = drawn[3];
        }
        CHECK(depth[0] >= 0 && depth[1] == 2 * depth[0] && height[1] == 2 * height[0]);
    }

    /* over a drawn background: the field's rows laid as many times high as the letters, so that a text set anew
       leaves nothing of the old one */
    {
        const int unit = 3;
        make(DGX_UI_LABEL, &dgx_ui_look_text, &in_points, "Hg", 20, 0, unit);
        dgx_ui_paint_t over = {.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_background_set(&over, draw_pattern, NULL);
        dgx_fill_rectangle(v, 0, 0, PANEL_W, PANEL_H, PAPER);
        dgx_ui_paint(&ui, &over, 0);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, text_i);
        int                  ink[4];
        bool                 inked = ink_box(v, n, INK, false, ink); /* the pattern under them: the letters' squares by their ink */
        for (int y = ink[1]; inked && y < ink[1] + ink[3]; ++y)
            for (int x = ink[0]; x < ink[0] + ink[2]; ++x)
                inked &= (dgx_get_pixel(v, x, y) == INK) == (dgx_get_pixel(v, ink[0] + (x - ink[0]) / unit * unit, ink[1] + (y - ink[1]) / unit * unit) == INK);
        CHECK(inked && ink[2] % unit == 0 && ink[3] % unit == 0);
        dgx_ui_text_set(&ui, text_i, " ");
        dgx_ui_paint(&ui, &over, 100);
        int left = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) left += dgx_get_pixel(v, x, y) != pattern(x, y);
        CHECK(n->h == terminus->y_advance * unit && left == 0);
        dgx_ui_paint_free(&over);
    }

    /* a handwritten variant in points: at a unit of 2, a height of 20 and a pen of 1 are a line of 40 pixels and a pen of
       2, pixel for pixel the label of a height of 40 and a pen of 2 at a unit of 1 in a box as large in pixels; straight
       and into a canvas, by a hard pen, a soft one and a hairline. A bitmap variant and one of points keep their lines */
    {
        static const struct {
            const dgx_ui_hand_t *hand;
            uint32_t             pen; /* in points at a unit of 2; at a unit of 1, twice its width */
        } ways[] = {{&dgx_ui_handwriting, 1}, {&dgx_ui_handwriting_canvas, 1}, {&dgx_ui_handwriting_canvas, 1 | DGX_PEN_SOFT}, {&dgx_ui_handwriting, 0}};
        for (size_t k = 0; k < sizeof ways / sizeof *ways; ++k) {
            const uint32_t              width   = ways[k].pen & ~(uint32_t)DGX_PEN_SOFT, soft = ways[k].pen & DGX_PEN_SOFT;
            const dgx_ui_font_variant_t small[1] = {{.font = &font0ss, .height = 20, .joined = true, .pen = ways[k].pen, .hand = ways[k].hand}};
            const dgx_ui_font_variant_t large[1] = {
                {.font = &font0ss, .height = 40, .joined = true, .pen = (width ? 2 * width : 0) | soft, .hand = ways[k].hand}};
            const dgx_ui_text_style_t in_small = {.fonts = small, .count = 1}, in_large = {.fonts = large, .count = 1};
            make_flags = ways[k].hand == &dgx_ui_handwriting_canvas ? DGX_UI_CANVAS : 0;
            make(DGX_UI_LABEL, &dgx_ui_look_text, &in_large, "Hand 42", 120, 0, 1);
            paint_on(ref);
            const dgx_ui_node_t at_one = *dgx_ui_node(&ui, text_i);
            make(DGX_UI_LABEL, &dgx_ui_look_text, &in_small, "Hand 42", 60, 0, 2);
            paint_on(v);
            const dgx_ui_node_t *n = dgx_ui_node(&ui, text_i);
            CHECK(dgx_ui_variant_line(&ui, &small[0]) == 40 && n->h == 40 && dgx_ui_textbox_height(&ui, &box) == 40);
            CHECK(n->x == at_one.x && n->y == at_one.y && n->w == at_one.w && n->h == at_one.h);
            int apart = 0, inked = 0;
            for (int y = 0; y < PANEL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x) apart += dgx_get_pixel(v, x, y) != dgx_get_pixel(ref, x, y), inked += dgx_get_pixel(v, x, y) != PAPER;
            CHECK(apart == 0 && inked > 0);
            if (apart || !inked) fprintf(stderr, "  the handwriting of way %zu at a unit of 2: %d pixels apart, %d inked\n", k, apart, inked);
        }
        make_flags = 0;
        make(DGX_UI_LABEL, &dgx_ui_look_text, &in_own, "Hi", 0, 0, 2);
        CHECK(dgx_ui_variant_line(&ui, &own[0]) == terminus->y_advance && dgx_ui_variant_line(&ui, &points[0]) == terminus->y_advance * 2);
    }

    /* a handwritten variant reads the field as nothing */
    {
        const dgx_ui_font_variant_t hand_on[1]  = {{.font = &font0ss, .height = 20, .pen = 1, .hand = &dgx_ui_handwriting, .points = true}};
        const dgx_ui_font_variant_t hand_off[1] = {{.font = &font0ss, .height = 20, .pen = 1, .hand = &dgx_ui_handwriting}};
        const dgx_ui_text_style_t   on = {.fonts = hand_on, .count = 1}, off = {.fonts = hand_off, .count = 1};
        make(DGX_UI_LABEL, &dgx_ui_look_text, &off, "Hand", 0, 0, 3);
        paint_on(ref);
        make(DGX_UI_LABEL, &dgx_ui_look_text, &on, "Hand", 0, 0, 3);
        paint_on(v);
        int apart = 0;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) apart += dgx_get_pixel(v, x, y) != dgx_get_pixel(ref, x, y);
        CHECK(apart == 0 && dgx_ui_variant_line(&ui, &hand_on[0]) == 20 * 3); /* its height in points, at a unit of 3 */
    }

    /* a soft font of 4 bits: squares of its coverage, its levels kept */
    {
        const dgx_ui_font_variant_t soft[1] = {{.font = &soft_font, .points = true}};
        const dgx_ui_text_style_t   style   = {.fonts = soft, .count = 1};
        make(DGX_UI_LABEL, &dgx_ui_look_text, &style, "AB", 0, 0, 3);
        paint_on(v);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, text_i);
        int                  drawn[4];
        CHECK(ink_box(v, n, PAPER, true, drawn) && squares(v, drawn, 3));
        uint16_t seen[32];
        int      count = 0;
        for (int y = drawn[1]; y < drawn[1] + drawn[3]; ++y)
            for (int x = drawn[0]; x < drawn[0] + drawn[2]; ++x) {
                bool known = false;
                for (int k = 0; k < count; ++k) known |= seen[k] == dgx_get_pixel(v, x, y);
                if (!known && count < 32) seen[count++] = (uint16_t)dgx_get_pixel(v, x, y);
            }
        CHECK(count > 3 && n->h == soft_font.y_advance * 3);
    }

    dgx_ui_free(&ui);
    v->destroy(&v), ref->destroy(&ref);
    CHECK_DONE();
}
