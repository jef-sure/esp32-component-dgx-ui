/*
 * Letters of a font of 4 bits a pixel (DGX_FONT_BITMAP_DEPTH4) in the library's looks, on a 16-bit display behind a
 * bus: a label, a button, a list, the look of lines, a dropdown's face and a field typed into. Their edges mixed with
 * the paper of each look: more than two colours in each box; every pixel written once; the panel the tree composed on
 * a screen in memory, at the first pass and after texts set anew, the pick moved, the button pressed and the field
 * focused. The letters of the look of lines coming one by one: each pass writes no pixel twice and leaves the panel as
 * the tree composed, so a row is laid anew and not drawn over itself, whose soft edges would grow darker.
 */
#include <string.h>

#define PANEL_W 120
#define PANEL_H 110
#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "panel.h"

/* a font of two letters, A and B, their pixels of 16 levels */
enum { A_WIDTH = 5, A_HEIGHT = 4, B_WIDTH = 6, B_HEIGHT = 5 };
static const uint8_t levels_a[A_HEIGHT][A_WIDTH] = {{0, 3, 15, 3, 0}, {2, 15, 0, 15, 2}, {15, 15, 15, 15, 15}, {15, 7, 0, 6, 15}};
static const uint8_t levels_b[B_HEIGHT][B_WIDTH] = {{1, 0, 0, 0, 0, 14}, {0, 8, 15, 15, 8, 0}, {0, 15, 0, 0, 15, 0}, {0, 8, 15, 15, 8, 0}, {14, 0, 0, 0, 0, 1}};
static uint8_t       soft_bits[2][64];
static glyph_t       soft_glyphs[2];
static glyph_array_t soft_ranges[1];
static dgx_font_t    soft_font;

static void make_soft_font(void)
{
    for (int letter = 0; letter < 2; ++letter) {
        const int width = letter ? B_WIDTH : A_WIDTH, height = letter ? B_HEIGHT : A_HEIGHT, pitch = (width * 4 + 7) / 8;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                soft_bits[letter][y * pitch + x / 2] |= (uint8_t)((letter ? levels_b[y][x] : levels_a[y][x]) << (x % 2 ? 0 : 4));
        soft_glyphs[letter] = (glyph_t){.bitmap   = soft_bits[letter],
                                        .width    = (int16_t)width,
                                        .height   = (int16_t)height,
                                        .xAdvance = 7,
                                        .xOffset  = 0,
                                        .yOffset  = (int16_t)(letter ? -5 : -4)};
    }
    soft_ranges[0] = (glyph_array_t){.first = 'A', .number = 2, .glyphs = soft_glyphs};
    soft_font      = (dgx_font_t){.glyph_ranges = soft_ranges, .yAdvance = 8, .yOffsetLowest = -5, .xWidest = B_WIDTH, .xWidthAverage = 6,
                                  .f_type = DGX_FONT_BITMAP_DEPTH4, .yBottomMax = 1, .xRightMax = 6, .number_of_ranges = 1};
}

/* how many colours lie in a widget's box on the panel */
static int colours_in(const panel_t *panel, const dgx_ui_node_t *node)
{
    uint32_t seen[64];
    int      count = 0;
    for (int y = node->y; y < node->y + node->h; ++y)
        for (int x = node->x; x < node->x + node->w; ++x) {
            bool known = false;
            for (int k = 0; k < count; ++k) known |= seen[k] == panel->mem[y][x];
            if (!known && count < 64) seen[count++] = panel->mem[y][x];
        }
    return count;
}

/* the pixels of the panel that differ from the tree composed by a fresh paint on a screen in memory; the panel holds a
   pixel's bytes as they came, the high first */
static int off_composed(dgx_ui_t *ui, const panel_t *panel, dgx_screen_t *memory, const uint32_t *colors, uint32_t now_ms)
{
    dgx_ui_paint_t fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = now_ms};
    dgx_fill_rectangle(memory, 0, 0, PANEL_W, PANEL_H, colors[0]);
    dgx_ui_paint_area_to(ui, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint_free(&fresh);
    int off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            const uint32_t sent = panel->mem[y][x], kept = dgx_get_pixel(memory, x, y);
            off += sent != kept && sent != (((kept & 0xff) << 8) | (kept >> 8));
        }
    return off;
}

int main(void)
{
    make_soft_font();
    static dgx_ui_font_variant_t variant[1];
    static dgx_ui_text_style_t   style;
    variant[0] = (dgx_ui_font_variant_t){.font = &soft_font};
    style      = (dgx_ui_text_style_t){.variants = variant, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    static char             label_text[16], button_text[16], list_text[32], lines_text[48], choice_text[32], field_text[16];
    static dgx_ui_textbox_t label_box, button_box, list_box, lines_box, choice_box, field_box;
    dgx_ui_textbox_init(&label_box, label_text, sizeof label_text, 1, &style);
    dgx_ui_textbox_init(&button_box, button_text, sizeof button_text, 1, &style);
    dgx_ui_textbox_init(&list_box, list_text, sizeof list_text, 1, &style);
    dgx_ui_textbox_init(&lines_box, lines_text, sizeof lines_text, 2, &style);
    dgx_ui_textbox_init(&choice_box, choice_text, sizeof choice_text, 1, &style);
    dgx_ui_textbox_init(&field_box, field_text, sizeof field_text, 1, &style);
    static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
    static const dgx_ui_widget_t label_w  = {.kind = DGX_UI_LABEL, .text = &label_box};
    static const dgx_ui_widget_t button_w = {.kind = DGX_UI_BUTTON, .text = &button_box, .want_h = 14, .look = &dgx_ui_look_button};
    static const dgx_ui_widget_t list_w   = {.kind = DGX_UI_LIST, .min = 0, .max = 1, .text = &list_box, .want_h = 20, .look = &dgx_ui_look_list};
    static const dgx_ui_widget_t lines_w  = {.kind = DGX_UI_LABEL, .text = &lines_box, .look = &dgx_ui_look_lines};
    static const dgx_ui_widget_t choice_w = {.kind = DGX_UI_DROPDOWN, .min = 0, .max = 1, .text = &choice_box, .want_h = 14, .look = &dgx_ui_look_dropdown};
    static const dgx_ui_widget_t field_w  = {.kind = DGX_UI_INPUT, .text = &field_box, .want_h = 12, .look = &dgx_ui_look_input};
    static dgx_ui_node_t         nodes[8];
    static dgx_ui_t              ui;
    panel_t                     *panel  = panel_make();
    dgx_screen_t                *memory = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    static uint32_t              colors[4];
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 18, 22, 30), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    dgx_ui_paint_t paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .background = 0};
    dgx_ui_init(&ui, nodes, 8, 1);
    dgx_ui_add(&ui, -1, &root_w);
    const int label = dgx_ui_add(&ui, 0, &label_w), button = dgx_ui_add(&ui, 0, &button_w), list = dgx_ui_add(&ui, 0, &list_w);
    const int lines = dgx_ui_add(&ui, 0, &lines_w), choice = dgx_ui_add(&ui, 0, &choice_w), field = dgx_ui_add(&ui, 0, &field_w);
    const int widgets[] = {label, button, list, lines, choice, field};
    dgx_ui_text_set(&ui, label, "ABBA"), dgx_ui_text_set(&ui, button, "BAB"), dgx_ui_text_set(&ui, list, "AB\nBA");
    dgx_ui_text_set(&ui, lines, "ABBA BAB AB BABA ABAB"), dgx_ui_text_set(&ui, choice, "BA\nAB"), dgx_ui_text_set(&ui, field, "ABA");

    /* the first pass: soft edges in each box, every pixel once, the picture of a screen in memory */
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &paint, 0);
    int few = 0;
    for (int k = 0; k < 6; ++k) few += colours_in(panel, dgx_ui_node(&ui, widgets[k])) <= 3;
    CHECK(few == 0 && panel_twice(panel) == 0 && off_composed(&ui, panel, memory, colors, 0) == 0);

    /* texts set anew, the pick moved, the button pressed, the field focused: every pixel once, nothing left of the old
       letters, the edges still soft */
    dgx_ui_text_set(&ui, label, "BAB"), dgx_ui_set_value(&ui, list, 1), dgx_ui_set_state(&ui, button, DGX_UI_PRESSED, true);
    dgx_ui_text_set(&ui, lines, "BAB ABBA"), dgx_ui_set_value(&ui, choice, 1), dgx_ui_text_set(&ui, field, "BB");
    dgx_ui_focus(&ui, field);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &paint, 100);
    few = 0;
    for (int k = 0; k < 6; ++k) few += colours_in(panel, dgx_ui_node(&ui, widgets[k])) <= 3;
    CHECK(few == 0 && panel_twice(panel) == 0 && off_composed(&ui, panel, memory, colors, 100) == 0);

    /* the letters of the lines coming one by one, a pass every 20 ms: each pass writes no pixel twice and leaves the
       panel as the tree composed at its moment */
    dgx_ui_text_set(&ui, lines, "ABBA BAB AB BABA ABAB BA");
    dgx_ui_paint(&ui, &paint, 200);
    dgx_ui_text_appear(&ui, lines, 40);
    int  twice = 0, off = 0, passes = 0;
    bool more  = true;
    for (uint32_t now = 1000; more && now <= 3000; now += 20, ++passes) {
        panel_count_from_now(panel);
        more = dgx_ui_paint(&ui, &paint, now);
        twice += panel_twice(panel);
        off += off_composed(&ui, panel, memory, colors, now);
    }
    CHECK(!more && passes > 20 && twice == 0 && off == 0 && colours_in(panel, dgx_ui_node(&ui, lines)) > 3);

    dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    dgx_screen_destroy(&memory), free(panel);
    CHECK_DONE();
}
