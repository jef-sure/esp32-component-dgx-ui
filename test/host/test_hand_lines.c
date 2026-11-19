/*
 * Handwriting in lines, dgx_ui_look_hand_lines, with DGX's handwritten font font0ss. The wrap equal to one the test
 * makes by dgx_hw_bounds(); each row equal to DGX writing its line at the row's place; empty lines, a text ending in a
 * new line, a line of spaces. A ladder of a handwritten variant and a bitmap one: each taken when it should be, both on
 * the first variant's base line. Cut with "..." in the same hand; important and scrolled, each step drawn whole;
 * DGX_UI_TEXT_MIDDLE. Letters coming over three lines, pass by pass equal to DGX writing line after line to the same
 * effort, the clock running over; a text changed and a widget hidden and shown while written; a mirror composed
 * between the passes equal to the screen; written in parts around a popup; told by dgx_ui_changed() and composed,
 * equal to painted. A text longer than 65.5 s: each pass writes its new strokes only. A soft pen in a canvas.
 * dgx_ui_look_lines with a handwritten variant in its style passes it over, its picture as without it. In the middle,
 * each pass writes its new strokes only, the block drawn whole once when a line comes. Built twice: with the line
 * breaks kept in the paint, and laid at every pass (DGX_UI_HAND_LINES_PLAIN), the same pictures. And after every pass
 * of letters coming, straight and in a canvas, left and centred, at the top and in the middle, cut or scrolled, the
 * screen as the tree composed anew.
 */
#include <string.h>

#define PANEL_W 320
#define PANEL_H 120
#include "check.h"
#include "composed.h"
#include "dgx_draw.h"
#include "dgx_hw_font.h"
#include "dgx_lut.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"
#include "panel.h"

static uint32_t              colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};
static dgx_ui_font_variant_t variant[2];
static dgx_ui_text_style_t   style;

/* a tree of one label of handwritten lines at 4, 4 */
typedef struct {
    dgx_ui_node_t    nodes[2];
    dgx_ui_t         ui;
    char             buf[1100];
    dgx_ui_textbox_t box;
    dgx_ui_widget_t  widget;
} tree_t;
static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX};

static void make(tree_t *tree, int width, int height, bool canvas, const char *text, const dgx_ui_look_t *look)
{
    memset(tree, 0, sizeof *tree);
    dgx_ui_textbox_init(&tree->box, tree->buf, sizeof tree->buf, 1, &style);
    tree->widget = (dgx_ui_widget_t){.kind   = DGX_UI_LABEL,
                                     .flags  = canvas ? DGX_UI_CANVAS : 0,
                                     .text   = &tree->box,
                                     .at_x   = 4,
                                     .at_y   = 4,
                                     .want_w = (int16_t)width,
                                     .want_h = (int16_t)height,
                                     .look   = look ? look : &dgx_ui_look_hand_lines};
    dgx_ui_init(&tree->ui, tree->nodes, 2, 1);
    dgx_ui_add(&tree->ui, -1, &root_w);
    dgx_ui_add(&tree->ui, 0, &tree->widget);
    dgx_ui_text_set(&tree->ui, 1, text);
}

/* the pen DGX writes a variant by, as the look should: its height, not narrowed */
static dgx_hw_context_t pen_of(const dgx_ui_font_variant_t *font_variant, dgx_screen_t *screen, uint32_t color, int width)
{
    const float scale = (float)font_variant->height / font_variant->font->hw->symbol_size_y;
    return (dgx_hw_context_t){.scr = screen, .font = font_variant->font, .scale_x = scale, .scale_y = scale, .width = width, .color = color, .joined = font_variant->joined};
}
static int span(const dgx_ui_font_variant_t *font_variant, const char *s, int n)
{
    char copy[300];
    memcpy(copy, s, (size_t)n), copy[n] = 0;
    dgx_hw_context_t pen   = pen_of(font_variant, NULL, 0, 2);
    int              width = 0;
    if (n > 0) dgx_hw_bounds(&pen, copy, NULL, NULL, &width, NULL);
    return width;
}
static int base_rows(const dgx_ui_font_variant_t *font_variant)
{
    const int size = font_variant->font->hw->symbol_size_y;
    return (2 * font_variant->font->hw->base_line * font_variant->height + size) / (2 * size);
}
static int letters_of(const char *s, int from, int to)
{
    int n = 0;
    for (int k = from; k < to; ++k) n += ((unsigned char)s[k] & 0xC0) != 0x80;
    return n;
}

/* the test's own wrap: words that go into width, a word wider by its letters, a new line breaking; [start, end) each */
static int wrap(const dgx_ui_font_variant_t *font_variant, const char *text, int width, int starts[], int ends[], int most)
{
    const int len = (int)strlen(text);
    int       count = 0, from = 0;
    while (from < len && count < most) {
        int e = from;
        for (;;) {
            int w = e;
            while (w < len && text[w] == ' ') ++w;
            int we = w;
            while (we < len && text[we] != ' ' && text[we] != '\n') ++we;
            if (span(font_variant, text + from, we - from) > width) {
                if (e == from) {
                    int c = from + 1;
                    while (c < we && span(font_variant, text + from, c + 1 - from) <= width) ++c;
                    e = c;
                }
                break;
            }
            e = we;
            if (we >= len || text[we] == '\n') break;
        }
        starts[count] = from, ends[count] = e, ++count;
        int next = e;
        while (next < len && text[next] == ' ') ++next;
        if (next < len && text[next] == '\n') ++next;
        from = next;
    }
    return count;
}

/* the expected screen: paper, then the rows of the lines from `top`, each written by DGX as far as `elapsed` of its
   letters' time came (letter_ms 0: whole); the last row of a text cut ending in "..." */
static void reference(dgx_screen_t *expected, const dgx_ui_font_variant_t *font_variant, const dgx_ui_node_t *node, const char *text, int top,
                      uint32_t elapsed, int letter_ms, bool cut, int first_rows)
{
    dgx_fill_rectangle(expected, 0, 0, PANEL_W, PANEL_H, colors[0]);
    int       starts[200], ends[200];
    const int lines = wrap(font_variant, text, node->w, starts, ends, 200);
    const int rows  = node->h / font_variant->height > 0 ? node->h / font_variant->height : 1;
    uint32_t  t0    = 0;
    for (int line = 0; line < lines; ++line) {
        const int      letters = letters_of(text, starts[line], ends[line]);
        const uint32_t t1      = t0 + (uint32_t)letters * (uint32_t)letter_ms;
        const int      k       = line - top;
        if (k >= 0 && k < rows && letters) {
            char row[300];
            int  n = ends[line] - starts[line];
            memcpy(row, text + starts[line], (size_t)n), row[n] = 0;
            if (cut && k == rows - 1) {
                while (n > 0) {
                    while (n > 0 && row[n - 1] == ' ') --n;
                    strcpy(row + n, "...");
                    if (span(font_variant, row, (int)strlen(row)) <= node->w) break;
                    row[--n] = 0;
                }
            }
            dgx_hw_context_t pen  = pen_of(font_variant, expected, colors[1], 2);
            int              left = 0;
            dgx_hw_bounds(&pen, row, &left, NULL, NULL, NULL);
            const int base = node->y + (first_rows >= 0 ? first_rows : base_rows(font_variant)) + k * font_variant->height;
            dgx_screen_clip(expected, 0, base - base_rows(font_variant), PANEL_W, font_variant->height);
            if (!letter_ms || elapsed >= t1) {
                dgx_hw_draw(&pen, node->x - left, base, row);
            } else if (elapsed > t0) {
                const float      effort = dgx_hw_text_effort(font_variant->font, row, font_variant->joined, &pen.pace);
                dgx_hw_writing_t writing;
                dgx_hw_writing_begin_with(&writing, &pen, node->x - left, base, row);
                dgx_hw_writing_draw_to(&writing, effort * (float)(elapsed - t0) / (float)(t1 - t0));
            }
            dgx_screen_clip(expected, 0, 0, PANEL_W, PANEL_H);
        }
        t0 = t1;
    }
}

static int differ(dgx_screen_t *one, dgx_screen_t *other)
{
    int count = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) count += dgx_get_pixel(one, x, y) != dgx_get_pixel(other, x, y);
    return count;
}

static const char *const NOTE = "Dear friend, the garden is green and the sun is warm today";

int main(void)
{
    variant[0]             = (dgx_ui_font_variant_t){.font = font0ss(), .height = 16, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting};
    variant[1]             = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    style                  = (dgx_ui_text_style_t){.variants = variant, .count = 1};
    dgx_screen_t *drawn    = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_screen_t *expected = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    static tree_t tree;
    dgx_ui_paint_t paint;

    /* the wrap and each row: a note of several lines in a label 150x64, drawn whole as DGX writes its lines */
    make(&tree, 150, 64, false, NOTE, NULL);
    paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
    dgx_ui_paint(&tree.ui, &paint, 0);
    const dgx_ui_node_t *node = dgx_ui_node(&tree.ui, 1);
    int                  starts[200], ends[200];
    const int            lines = wrap(&variant[0], NOTE, node->w, starts, ends, 200);
    CHECK(lines >= 3 && lines <= 4 && tree.box.total_lines == lines);
    reference(expected, &variant[0], node, NOTE, 0, 0, 0, false, -1);
    CHECK(differ(drawn, expected) == 0);
    /* empty lines, a text ending in a new line, a line of spaces */
    dgx_ui_text_set(&tree.ui, 1, "one\n\ntwo   \n   \n"); /* four lines in four rows */
    dgx_ui_paint(&tree.ui, &paint, 1);
    reference(expected, &variant[0], node, "one\n\ntwo   \n   \n", 0, 0, 0, false, -1);
    CHECK(differ(drawn, expected) == 0);
    /* another text of as many lines, broken elsewhere: laid again, not taken for the one before */
    dgx_ui_text_set(&tree.ui, 1, "one\ntwo");
    dgx_ui_paint(&tree.ui, &paint, 2);
    dgx_ui_text_set(&tree.ui, 1, "three\nfour");
    dgx_ui_paint(&tree.ui, &paint, 3);
    reference(expected, &variant[0], node, "three\nfour", 0, 0, 0, false, -1);
    CHECK(differ(drawn, expected) == 0);
    dgx_ui_paint_free(&paint), dgx_ui_free(&tree.ui);

    /* cut, not important: the last row ends in "..." in the same hand */
    make(&tree, 150, 32, false, NOTE, NULL);
    paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
    dgx_ui_paint(&tree.ui, &paint, 0);
    node = dgx_ui_node(&tree.ui, 1);
    reference(expected, &variant[0], node, NOTE, 0, 0, 0, true, -1);
    CHECK(differ(drawn, expected) == 0);
    /* important: a line up every scroll_ms once all came, each step drawn whole, ending at the last lines */
    style.flags = DGX_UI_TEXT_IMPORTANT, style.scroll_ms = 500;
    dgx_ui_text_set(&tree.ui, 1, NOTE);
    dgx_ui_paint(&tree.ui, &paint, 1000);
    int scroll_off = 0;
    for (uint32_t now_ms = 1000; now_ms <= 1000 + 500 * 4; now_ms += 250) {
        dgx_ui_paint(&tree.ui, &paint, now_ms);
        int top = (int)((now_ms - 1000) / 500);
        if (top > lines - 2) top = lines - 2;
        reference(expected, &variant[0], node, NOTE, top, 0, 0, false, -1);
        scroll_off += differ(drawn, expected);
    }
    CHECK(scroll_off == 0 && tree.box.top == lines - 2);
    style.flags = 0, style.scroll_ms = 0;
    dgx_ui_paint_free(&paint), dgx_ui_free(&tree.ui);

    /* a ladder, handwritten 16 then Terminus, in a box of 30: a short note in the hand's one row, a long one in the
       bitmap font's two rows, its first row on the hand's base line */
    style.count = 2;
    make(&tree, 150, 30, false, "Hello", NULL);
    paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
    dgx_ui_paint(&tree.ui, &paint, 0);
    node = dgx_ui_node(&tree.ui, 1);
    reference(expected, &variant[0], node, "Hello", 0, 0, 0, false, -1);
    CHECK(tree.box.variant == 0 && differ(drawn, expected) == 0);
    dgx_ui_text_set(&tree.ui, 1, "a much wider note than this hand holds in one row"); /* no letter below the line */
    dgx_ui_paint(&tree.ui, &paint, 1);
    int bottom = -1; /* of the first row: the ink down to a row under the hand's base line */
    for (int y = node->y; y <= node->y + base_rows(&variant[0]) + 1; ++y)
        for (int x = node->x; x < node->x + node->w; ++x)
            if (dgx_get_pixel(drawn, x, y) == colors[1]) bottom = y;
    CHECK(tree.box.variant == 1 && bottom >= node->y + base_rows(&variant[0]) - 1 && bottom <= node->y + base_rows(&variant[0]));
    style.count = 1;
    dgx_ui_paint_free(&paint), dgx_ui_free(&tree.ui);

    /* in the middle: two lines in a box of 64 */
    style.flags = DGX_UI_TEXT_MIDDLE;
    make(&tree, 150, 64, false, "one\ntwo", NULL);
    paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
    dgx_ui_paint(&tree.ui, &paint, 0);
    node = dgx_ui_node(&tree.ui, 1);
    reference(expected, &variant[0], node, "one\ntwo", 0, 0, 0, false, (64 - 2 * 16) / 2 + base_rows(&variant[0]));
    CHECK(differ(drawn, expected) == 0);
    style.flags = 0;
    dgx_ui_paint_free(&paint), dgx_ui_free(&tree.ui);

    /* letters coming, 40 ms each, over three lines: pass by pass as DGX writes line after line to the same effort, the
       clock running over; a mirror composed between the passes equal to the screen, and storing nothing */
    {
        const uint32_t start = 4294967000u;
        make(&tree, 150, 64, false, NOTE, NULL);
        paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        dgx_ui_paint(&tree.ui, &paint, start - 1000);
        dgx_ui_text_appear(&tree.ui, 1, 40);
        node                  = dgx_ui_node(&tree.ui, 1);
        dgx_screen_t *mirror  = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        const uint32_t total  = (uint32_t)letters_of(NOTE, 0, (int)strlen(NOTE)) * 40;
        int            off = 0, mirror_off = 0, wake_off = 0, passes = 0;
        bool           more = true;
        for (uint32_t ms = 0; more && ms <= total + 200; ms += 20, ++passes) {
            more = dgx_ui_paint(&tree.ui, &paint, start + ms);
            wake_off += more && paint.wake_ms != DGX_UI_HAND_FRAME_MS;
            reference(expected, &variant[0], node, NOTE, 0, ms, 40, false, -1);
            off += differ(drawn, expected);
            dgx_ui_paint_area_to(&tree.ui, &paint, mirror, NULL, 0, 0, PANEL_W, PANEL_H);
            mirror_off += differ(mirror, drawn);
        }
        CHECK(passes > 50 && !more && off == 0 && mirror_off == 0 && wake_off == 0);
        /* changed while written: begun again; hidden and shown while written: drawn whole as it stands */
        dgx_ui_text_appear(&tree.ui, 1, 40);
        dgx_ui_paint(&tree.ui, &paint, 5000);
        dgx_ui_paint(&tree.ui, &paint, 5200);
        dgx_ui_text_set(&tree.ui, 1, "Two words\nand more");
        dgx_ui_paint(&tree.ui, &paint, 5300);
        dgx_ui_paint(&tree.ui, &paint, 5500);
        reference(expected, &variant[0], node, "Two words\nand more", 0, 200, 40, false, -1);
        CHECK(differ(drawn, expected) == 0);
        dgx_ui_show(&tree.ui, 1, false);
        dgx_ui_paint(&tree.ui, &paint, 5600);
        dgx_ui_show(&tree.ui, 1, true);
        dgx_ui_paint(&tree.ui, &paint, 5700);
        reference(expected, &variant[0], node, "Two words\nand more", 0, 400, 40, false, -1);
        CHECK(differ(drawn, expected) == 0);
        mirror->destroy(&mirror);
        dgx_ui_paint_free(&paint), dgx_ui_free(&tree.ui);
    }

    /* in parts around a popup over the middle of the label; and told by dgx_ui_changed(), each rectangle composed:
       equal to the painted screen at every pass */
    {
        static tree_t        told;
        static dgx_ui_node_t popup_nodes[1];
        static dgx_ui_t      popup;
        static const dgx_ui_widget_t popup_w = {.kind = DGX_UI_FRAME};
        make(&tree, 150, 64, false, NOTE, NULL), make(&told, 150, 64, false, NOTE, NULL);
        dgx_screen_t  *told_screen = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t told_paint  = {.scr = told_screen, .colors = colors, .color_count = 4};
        paint                      = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        dgx_ui_paint(&tree.ui, &paint, 0);
        dgx_ui_paint(&told.ui, &told_paint, 0);
        dgx_ui_init(&popup, popup_nodes, 1, 1);
        dgx_ui_add(&popup, -1, &popup_w);
        CHECK(dgx_ui_popup_open(&tree.ui, &popup, 60, 10, 30, 40, 0));
        dgx_ui_text_appear(&tree.ui, 1, 40), dgx_ui_text_appear(&told.ui, 1, 40);
        node = dgx_ui_node(&tree.ui, 1);
        int parts_off = 0, told_off = 0;
        bool more = true;
        for (uint32_t ms = 0; more && ms <= 3000; ms += 20) {
            more = dgx_ui_paint(&tree.ui, &paint, 1000 + ms);
            reference(expected, &variant[0], node, NOTE, 0, ms, 40, false, -1);
            for (int y = 0; y < PANEL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x)
                    parts_off += !(x >= 60 && x < 90 && y >= 10 && y < 50) && dgx_get_pixel(drawn, x, y) != dgx_get_pixel(expected, x, y);
            dgx_ui_changed(&told.ui, &told_paint, 1000 + ms, NULL, NULL);
            dgx_ui_paint_area_to(&told.ui, &told_paint, told_screen, NULL, 0, 0, PANEL_W, PANEL_H);
            told_off += differ(told_screen, expected);
        }
        CHECK(!more && parts_off == 0 && told_off == 0);
        dgx_ui_popup_close(&tree.ui);
        dgx_ui_free(&popup);
        dgx_ui_paint_free(&paint), dgx_ui_paint_free(&told_paint);
        dgx_ui_free(&tree.ui), dgx_ui_free(&told.ui);
        told_screen->destroy(&told_screen);
    }

    /* in the middle, letters coming, on a counting panel: after the first, each pass writes its new strokes only, the
       block not moved; the text grown a line while shown, drawn whole once and then its new strokes again */
    {
        panel_t       *panel       = panel_make();
        dgx_ui_paint_t panel_paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        style.flags                = DGX_UI_TEXT_MIDDLE;
        make(&tree, 150, 64, false, NOTE, NULL);
        dgx_ui_paint(&tree.ui, &panel_paint, 0);
        dgx_ui_text_appear(&tree.ui, 1, 40);
        node                = dgx_ui_node(&tree.ui, 1);
        const int box_area  = node->w * node->h;
        int       most      = 0, passes = 0;
        bool      more      = true;
        for (uint32_t ms = 0; more && ms <= 3000; ms += 20, ++passes) {
            panel_count_from_now(panel);
            more = dgx_ui_paint(&tree.ui, &panel_paint, 1000 + ms);
            if (passes > 0 && panel_written(panel) > most) most = panel_written(panel);
        }
        CHECK(!more && passes > 50 && most > 0 && most < box_area / 10);
        dgx_ui_text_set(&tree.ui, 1, "one line");
        dgx_ui_paint(&tree.ui, &panel_paint, 5000);
        dgx_ui_text_appear(&tree.ui, 1, 40);
        dgx_ui_paint(&tree.ui, &panel_paint, 5100);
        dgx_ui_text_set(&tree.ui, 1, "one line\nand one more"); /* a line more while written: the block moves */
        panel_count_from_now(panel);
        dgx_ui_paint(&tree.ui, &panel_paint, 5200);
        const int moved = panel_written(panel);
        panel_count_from_now(panel);
        dgx_ui_paint(&tree.ui, &panel_paint, 5240);
        CHECK(moved >= box_area && panel_written(panel) < box_area / 10);
        style.flags = 0;
        dgx_ui_paint_free(&panel_paint), dgx_ui_free(&tree.ui);
        free(panel);
    }

    /* a text longer than 65.5 s, 300 letters of 255 ms, on a counting panel: each pass writes its new strokes only, so
       all the passes together write about the text's ink, past 65.5 s too */
    {
        static char long_text[400];
        long_text[0] = 0;
        while (strlen(long_text) < 300) strcat(long_text, "one more word ");
        long_text[300] = 0;
        panel_t       *panel       = panel_make();
        dgx_ui_paint_t panel_paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        make(&tree, 300, 112, false, long_text, NULL);
        dgx_ui_paint(&tree.ui, &panel_paint, 0);
        dgx_ui_text_appear(&tree.ui, 1, 255);
        dgx_ui_paint(&tree.ui, &panel_paint, 1000);
        int  written = 0, late = 0;
        bool more    = true;
        for (uint32_t ms = 500; more && ms <= 300 * 255 + 1000; ms += 500) {
            panel_count_from_now(panel);
            more = dgx_ui_paint(&tree.ui, &panel_paint, 1000 + ms);
            written += panel_written(panel);
            if (ms > 66000) late += panel_written(panel);
        }
        node    = dgx_ui_node(&tree.ui, 1);
        int ink = 0;
        for (int y = node->y; y < node->y + node->h; ++y)
            for (int x = node->x; x < node->x + node->w; ++x) ink += panel->mem[y][x] == colors[1];
        CHECK(!more && ink > 500 && written <= 2 * ink && late > 0 && late < ink / 5);
        dgx_ui_paint_free(&panel_paint), dgx_ui_free(&tree.ui);
        free(panel);
    }

    /* a soft pen in a canvas, by dgx_ui_handwriting_canvas: the amounts DGX's soft pen writes, put through the table */
    {
        variant[0].pen  = 2 | DGX_PEN_SOFT;
        variant[0].hand = &dgx_ui_handwriting_canvas;
        make(&tree, 150, 32, true, "one\ntwo", NULL);
        paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        dgx_ui_paint(&tree.ui, &paint, 0);
        node                    = dgx_ui_node(&tree.ui, 1);
        dgx_screen_t    *amounts = dgx_vscreen_init(node->w, node->h, 8, DgxScreenRGB);
        dgx_hw_context_t pen     = pen_of(&variant[0], amounts, 255, 2 | (int)DGX_PEN_SOFT);
        const char      *rows[2] = {"one", "two"};
        for (int k = 0; k < 2; ++k) {
            int left = 0;
            dgx_hw_bounds(&pen, rows[k], &left, NULL, NULL, NULL);
            dgx_hw_draw(&pen, -left, base_rows(&variant[0]) + k * 16, rows[k]);
        }
        static uint16_t table[256];
        dgx_lut_gradient(table, 0, 256, 0x000000, 0xffffff);
        dgx_fill_rectangle(expected, 0, 0, PANEL_W, PANEL_H, colors[0]);
        dgx_vscreen8_to_screen16(expected, node->x, node->y, amounts, table, DGX_VSCREEN_EXACT);
        CHECK(differ(drawn, expected) == 0 && paint.canvases != NULL);
        /* the text changed: the canvas written anew, the old letters gone */
        dgx_ui_text_set(&tree.ui, 1, "six\nten");
        dgx_ui_paint(&tree.ui, &paint, 1);
        dgx_fill_rectangle(amounts, 0, 0, amounts->width, amounts->height, 0);
        const char *changed[2] = {"six", "ten"};
        for (int k = 0; k < 2; ++k) {
            int left = 0;
            dgx_hw_bounds(&pen, changed[k], &left, NULL, NULL, NULL);
            dgx_hw_draw(&pen, -left, base_rows(&variant[0]) + k * 16, changed[k]);
        }
        dgx_fill_rectangle(expected, 0, 0, PANEL_W, PANEL_H, colors[0]);
        dgx_vscreen8_to_screen16(expected, node->x, node->y, amounts, table, DGX_VSCREEN_EXACT);
        CHECK(differ(drawn, expected) == 0);
        dgx_screen_destroy(&amounts);
        dgx_ui_paint_free(&paint), dgx_ui_free(&tree.ui);
        variant[0].pen = 2, variant[0].hand = &dgx_ui_handwriting;
    }

    /* the look of lines with a handwritten variant first in its style: passed over, its picture as without it */
    {
        static tree_t plain;
        style.count = 2;
        make(&tree, 150, 64, false, NOTE, &dgx_ui_look_lines);
        paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        dgx_ui_paint(&tree.ui, &paint, 0);
        static dgx_ui_text_style_t bitmap_only;
        bitmap_only = (dgx_ui_text_style_t){.variants = &variant[1], .count = 1};
        memset(&plain, 0, sizeof plain);
        make(&plain, 150, 64, false, NOTE, &dgx_ui_look_lines);
        plain.box.style              = &bitmap_only;
        dgx_ui_paint_t expected_paint = {.scr = expected, .colors = colors, .color_count = 4};
        dgx_ui_text_set(&plain.ui, 1, NOTE);
        dgx_ui_paint(&plain.ui, &expected_paint, 0);
        CHECK(tree.box.variant == 1 && differ(drawn, expected) == 0);
        style.count = 1;
        dgx_ui_paint_free(&paint), dgx_ui_paint_free(&expected_paint);
        dgx_ui_free(&tree.ui), dgx_ui_free(&plain.ui);
    }

    /* letters coming at 40 ms, a pass every 20 ms, straight and in a canvas, left and centred, at the top and in the
       middle, the note cut in a box of two rows or important and scrolled: after every pass the screen as the tree
       composed anew (composed.h); the text changed while written and set again */
    for (int k = 0; k < 16; ++k) {
        const bool canvas = k & 1, centred = k & 2, middle = k & 4, important = k & 8;
        style.align       = centred ? DGX_FONT_ALIGN_CENTER : DGX_FONT_ALIGN_LEFT;
        style.flags       = (uint8_t)((middle ? DGX_UI_TEXT_MIDDLE : 0) | (important ? DGX_UI_TEXT_IMPORTANT : 0));
        style.scroll_ms   = (uint16_t)(important ? 300 : 0);
        variant[0].hand   = canvas ? &dgx_ui_handwriting_canvas : &dgx_ui_handwriting;
        make(&tree, 150, 40, canvas, NOTE, NULL);
        paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        dgx_fill_rectangle(drawn, 0, 0, PANEL_W, PANEL_H, 0);
        dgx_ui_text_appear(&tree.ui, 1, 40);
        const uint32_t total     = (uint32_t)letters_of(NOTE, 0, (int)strlen(NOTE)) * 40;
        int            differing = 0;
        for (uint32_t ms = 0; ms <= 2 * total + 1500; ms += 20) {
            if (ms == 1000) dgx_ui_text_set(&tree.ui, 1, "one line, then");
            if (ms == 1500) dgx_ui_text_set(&tree.ui, 1, NOTE), dgx_ui_text_appear(&tree.ui, 1, 40);
            dgx_ui_paint(&tree.ui, &paint, ms);
            differing += differ_from_composed(&tree.ui, &paint, expected, 1) > 0;
        }
        CHECK(tree.box.total_lines > 2 && tree.box.flags & DGX_UI_TEXTBOX_CAME && differing == 0);
        dgx_ui_paint_free(&paint), dgx_ui_free(&tree.ui);
    }
    style.align = DGX_FONT_ALIGN_LEFT, style.flags = 0, style.scroll_ms = 0, variant[0].hand = &dgx_ui_handwriting;

    drawn->destroy(&drawn), expected->destroy(&expected);
    CHECK_DONE();
}
