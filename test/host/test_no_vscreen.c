/*
 * dgx-ui on a chip whose DGX is built with no virtual screens (no CONFIG_DGX_ENABLE_VSCREEN), stood for by
 * DGX_UI_NO_VSCREEN: no canvas is to be had. A label and lines in handwriting by a soft pen on dgx_ui_handwriting_canvas
 * are written straight by the hard pen, the same as dgx_ui_handwriting writes them, and take no canvas; the lines keep
 * their line breaks all the same; a widget's own canvas is none.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_hw_font.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/font0ss.h"

#ifndef DGX_UI_NO_VSCREEN
#error "built with -DDGX_UI_NO_VSCREEN"
#endif

enum { SCREEN_W = 240, SCREEN_H = 100 };

static uint32_t              colors[2] = {0x0000, 0xffff};
static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX};

typedef struct {
    dgx_ui_node_t    nodes[2];
    dgx_ui_t         ui;
    char             buf[128];
    dgx_ui_textbox_t box;
    dgx_ui_widget_t  widget;
} tree_t;

/* a tree of one widget of DGX_UI_CANVAS, a label or lines in the hand given, written whole on a screen of its own */
static dgx_screen_t *written(tree_t *tree, const dgx_ui_hand_t *hand, bool lines, const char *text, dgx_ui_paint_t *paint)
{
    memset(tree, 0, sizeof *tree);
    variant[0] = (dgx_ui_font_variant_t){.font = &font0ss, .height = 20, .joined = true, .pen = 2 | DGX_PEN_SOFT, .hand = hand};
    style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
    dgx_ui_textbox_init(&tree->box, tree->buf, sizeof tree->buf, lines ? 3 : 1, &style);
    tree->widget = (dgx_ui_widget_t){.kind   = DGX_UI_LABEL,
                                     .flags  = DGX_UI_CANVAS,
                                     .text   = &tree->box,
                                     .look   = lines ? &dgx_ui_look_hand_lines : NULL,
                                     .at_x   = 4,
                                     .at_y   = 4,
                                     .want_w = 200};
    dgx_ui_init(&tree->ui, tree->nodes, 2, 1);
    dgx_ui_add(&tree->ui, -1, &root_w);
    dgx_ui_add(&tree->ui, 0, &tree->widget);
    dgx_ui_text_set(&tree->ui, 1, text);
    dgx_screen_t *screen = dgx_vscreen_init(SCREEN_W, SCREEN_H, 16, DgxScreenRGB);
    *paint               = (dgx_ui_paint_t){.scr = screen, .colors = colors, .color_count = 2};
    dgx_ui_paint(&tree->ui, paint, 0);
    return screen;
}

static int differ(dgx_screen_t *one, dgx_screen_t *other)
{
    int count = 0;
    for (int y = 0; y < SCREEN_H; ++y)
        for (int x = 0; x < SCREEN_W; ++x) count += dgx_get_pixel(one, x, y) != dgx_get_pixel(other, x, y);
    return count;
}

static int inked(dgx_screen_t *screen)
{
    int count = 0;
    for (int y = 0; y < SCREEN_H; ++y)
        for (int x = 0; x < SCREEN_W; ++x) count += dgx_get_pixel(screen, x, y) == colors[1];
    return count;
}

int main(void)
{
    static tree_t  hard_tree, soft_tree;
    dgx_ui_paint_t hard_paint, soft_paint;
    for (int lines = 0; lines < 2; ++lines) {
        const char   *text = lines ? "The quick brown fox jumps over the lazy dog and runs away." : "Hello";
        dgx_screen_t *hard = written(&hard_tree, &dgx_ui_handwriting, lines, text, &hard_paint);
        dgx_screen_t *soft = written(&soft_tree, &dgx_ui_handwriting_canvas, lines, text, &soft_paint);
        bool          made = true;
        CHECK(inked(soft) > 0 && differ(hard, soft) == 0);
        CHECK(dgx_ui_canvas(&soft_paint, &soft_tree.ui, 1, &made) == NULL && !made);
        /* a label keeps nothing; the lines keep their line breaks in the paint's table, which needs no virtual screen */
        CHECK(lines ? soft_paint.canvases != NULL : soft_paint.canvases == NULL);
        dgx_ui_paint_free(&hard_paint), dgx_ui_paint_free(&soft_paint);
        dgx_ui_free(&hard_tree.ui), dgx_ui_free(&soft_tree.ui);
        dgx_screen_destroy(&hard), dgx_screen_destroy(&soft);
    }
    CHECK_DONE();
}
