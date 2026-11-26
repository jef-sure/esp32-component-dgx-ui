/*
 * The lines a text takes, dgx_ui_lines_count() and dgx_ui_hand_lines_count(), held to the looks that lay it: on many
 * texts made at random, of words short and wider than the line, line breaks, runs of spaces and spaces at the end, at
 * many widths, a label of so many lines drawn by dgx_ui_look_lines and by dgx_ui_look_hand_lines counts the same lines
 * and shows them whole in the style's first variant, no ellipsis, no empty row. Styles of one variant and of a
 * ladder, a handwritten variant first and a bitmap one first, at a unit of 1 and of 2. Empty text, no style, no width.
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "dgx_hw_font.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"

enum { SCREEN_W = 320, SCREEN_H = 400, TEXTS = 150 };

static uint32_t colors[2] = {0x0000, 0xffff};

typedef struct {
    dgx_ui_node_t    nodes[2];
    dgx_ui_t         ui;
    char             buf[400];
    dgx_ui_textbox_t box;
    dgx_ui_widget_t  widget;
} tree_t;

static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX};

static uint32_t seed = 12345;
static int      rnd(int below)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) % (uint32_t)below);
}

/* a text of words of 1 to 30 letters, a line break now and then, runs of spaces, spaces at the end */
static void made_text(char *s, int size)
{
    int       at    = 0;
    const int words = 1 + rnd(25);
    for (int k = 0; k < words && at < size - 40; ++k) {
        const int letters = rnd(8) ? 1 + rnd(8) : 10 + rnd(21);
        for (int l = 0; l < letters; ++l) s[at++] = (char)("abcdefghijklmnopqrstuvwxyzMWil.,"[rnd(32)]);
        const int gap = rnd(10);
        if (gap == 0) s[at++] = '\n';
        else if (gap == 1) s[at++] = ' ', s[at++] = ' ', s[at++] = ' ';
        else if (gap == 2) s[at++] = ' ', s[at++] = '\n';
        else s[at++] = ' ';
    }
    if (rnd(3) == 0) at -= at > 0; /* no space at the end */
    s[at] = '\0';
}

/* a label of the lines counted, at a width, drawn once by the look: whether the look counted the same and showed them
   whole in the variant expected */
static bool held(const dgx_ui_look_t *look, const dgx_ui_text_style_t *style, int unit, const char *text, int width, int count, int expected)
{
    static tree_t tree;
    memset(&tree, 0, sizeof tree);
    dgx_ui_textbox_init(&tree.box, tree.buf, sizeof tree.buf, count, style);
    tree.widget = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &tree.box, .look = look, .want_w = (int16_t)(width / unit)}; /* points of the tree */
    dgx_ui_init(&tree.ui, tree.nodes, 2, unit);
    dgx_ui_set_root(&tree.ui, 0, 0, SCREEN_W, SCREEN_H);
    dgx_ui_add(&tree.ui, -1, &root_w);
    dgx_ui_add(&tree.ui, 0, &tree.widget);
    dgx_ui_text_set(&tree.ui, 1, text);
    dgx_screen_t  *screen = dgx_vscreen_init(SCREEN_W, SCREEN_H, 16, DgxScreenRGB);
    dgx_ui_paint_t paint  = {.scr = screen, .colors = colors, .color_count = 2};
    dgx_ui_paint(&tree.ui, &paint, 0);
    const dgx_ui_node_t *node = dgx_ui_node(&tree.ui, 1);
    const int            line = dgx_ui_variant_line(&tree.ui, &style->fonts[expected]);
    const bool           ok   = node->w == width && tree.box.total_lines == count && tree.box.variant == expected && node->h / line >= count;
    if (!ok)
        fprintf(stderr, "  width %d unit %d: counted %d, the look %d lines in variant %d, rows %d: \"%s\"\n", width, unit, count,
                tree.box.total_lines, tree.box.variant, node->h / line, text);
    dgx_ui_paint_free(&paint);
    dgx_ui_free(&tree.ui);
    dgx_screen_destroy(&screen);
    return ok;
}

int main(void)
{
    static const dgx_ui_font_variant_t bitmap[2] = {{.font = &TerminusTTFMedium12}, {.font = &TerminusTTFMedium12, .points = 1}};
    static const dgx_ui_font_variant_t hand[2]   = {{.font = &font0ss, .height = 20, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting},
                                                    {.font = &TerminusTTFMedium12}};
    static const dgx_ui_font_variant_t hand_after[2] = {{.font = &TerminusTTFMedium12},
                                                        {.font = &font0ss, .height = 16, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting}};
    const dgx_ui_text_style_t one = {.fonts = bitmap, .count = 1}, ladder = {.fonts = bitmap, .count = 2};
    const dgx_ui_text_style_t written = {.fonts = hand, .count = 2}, written_after = {.fonts = hand_after, .count = 2};
    struct {
        const dgx_ui_look_t       *look;
        const dgx_ui_text_style_t *style;
        int (*count)(const dgx_ui_t *, const dgx_ui_text_style_t *, const char *, int);
    } const cases[] = {
        {&dgx_ui_look_lines, &one, dgx_ui_lines_count},
        {&dgx_ui_look_lines, &ladder, dgx_ui_lines_count},
        {&dgx_ui_look_hand_lines, &written, dgx_ui_hand_lines_count},
        {&dgx_ui_look_hand_lines, &written_after, dgx_ui_hand_lines_count},
        {&dgx_ui_look_hand_lines, &one, dgx_ui_hand_lines_count},
    };
    char text[400];
    int  wrong = 0, many = 0;
    for (int c = 0; c < (int)(sizeof cases / sizeof cases[0]); ++c)
        for (int unit = 1; unit <= 2; ++unit)
            for (int t = 0; t < TEXTS; ++t) {
                made_text(text, sizeof text);
                const int width = unit * (8 + rnd(SCREEN_W / unit - 8)); /* pixels, a whole number of points */
                dgx_ui_t  ui;
                dgx_ui_init(&ui, NULL, 0, unit);
                const int count = cases[c].count(&ui, cases[c].style, text, width);
                if (count > 255 || count * dgx_ui_variant_line(&ui, &cases[c].style->fonts[0]) > SCREEN_H) continue; /* taller than the screen */
                many += count > 1;
                wrong += !held(cases[c].look, cases[c].style, unit, text, width, count, 0);
            }
    CHECK(wrong == 0 && many > 300);

    /* the bounds: an empty text one line, no width and no style none; a style of no variant its look draws none */
    dgx_ui_t ui;
    dgx_ui_init(&ui, NULL, 0, 1);
    CHECK(dgx_ui_lines_count(&ui, &one, "", 100) == 1 && dgx_ui_hand_lines_count(&ui, &written, "", 100) == 1);
    CHECK(dgx_ui_lines_count(&ui, &one, NULL, 100) == 1 && dgx_ui_lines_count(&ui, &one, "a b", 0) == 0 && dgx_ui_lines_count(&ui, NULL, "a", 100) == 0);
    const dgx_ui_text_style_t only_hand = {.fonts = hand, .count = 1};
    CHECK(dgx_ui_lines_count(&ui, &only_hand, "a b", 100) == 0 && dgx_ui_hand_lines_count(&ui, &only_hand, "a b", 100) == 1);
    /* a word wider than the line: by its letters, as many as go, one at least */
    CHECK(dgx_ui_lines_count(&ui, &one, "abcdefghij", 1) == 10);
    CHECK_DONE();
}
