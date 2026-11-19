/*
 * Handwriting in the one-line look of a text: DGX's handwritten font font0ss in a variant that names the library's
 * handwriting. Fitted at the variant's height by DGX's own measure, narrowed in whole percents as far as the variant
 * lets it, cut by the box past that, its beginning shown however it is aligned; drawn straight equal to DGX drawing it
 * at the same place. A smaller variant stands on the first's base line, handwritten or a bitmap, either way round, or
 * in the middle by the style. Its letters coming stroke by stroke, pass after pass equal to DGX writing it from
 * nothing to the same effort, a frame asked meanwhile: a mirror composed between the passes the same and changing
 * nothing, a text changed while written begun again, the clock running over, written in parts around a popup, each
 * pass writing only its new strokes. A pen of 0 a hairline; a soft pen straight the hard one of its width. On a widget
 * of a canvas, straight by dgx_ui_handwriting and into the canvas by dgx_ui_handwriting_canvas, by a soft pen, put
 * over the paper as DGX's soft pen and table would. Over a drawn background, in strips on a panel as straight on a
 * screen.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_hw_font.h"
#include "dgx_lut.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

static dgx_ui_font_variant_t variant[2];
static dgx_ui_text_style_t   style;

/* a tree of one label of the handwriting, its box at 4, 4 */
typedef struct {
    dgx_ui_node_t    nodes[2];
    dgx_ui_t         ui;
    char             buf[64];
    dgx_ui_textbox_t box;
    dgx_ui_widget_t  widget;
} tree_t;

static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX};

static void make(tree_t *tree, int width, int height, bool canvas, const char *text)
{
    memset(tree, 0, sizeof *tree);
    dgx_ui_textbox_init(&tree->box, tree->buf, sizeof tree->buf, 1, &style);
    tree->widget = (dgx_ui_widget_t){.kind   = DGX_UI_LABEL,
                                     .flags  = canvas ? DGX_UI_CANVAS : 0,
                                     .text   = &tree->box,
                                     .at_x   = 4,
                                     .at_y   = 4,
                                     .want_w = (int16_t)width,
                                     .want_h = (int16_t)height};
    dgx_ui_init(&tree->ui, tree->nodes, 2, 1);
    dgx_ui_add(&tree->ui, -1, &root_w);
    dgx_ui_add(&tree->ui, 0, &tree->widget);
    dgx_ui_text_set(&tree->ui, 1, text);
}

/* the context DGX is asked to write a text by, as the look should: its scale from the variant's height, across
   scale_x when narrowed (0: not) */
/* the width of a variant's pen, 1 for 0 */
static int dgx_ui_hand_pen_of(const dgx_ui_font_variant_t *font_variant)
{
    const int width = (int)(font_variant->pen & ~(uint32_t)DGX_PEN_SOFT);
    return width > 0 ? width : 1;
}

static dgx_hw_context_t context(const dgx_ui_font_variant_t *font_variant, float scale_x, dgx_screen_t *screen, uint32_t color, int pen_width)
{
    const float scale_y = (float)font_variant->height / font_variant->font->hw->symbol_size_y;
    return (dgx_hw_context_t){.scr     = screen,
                              .font    = font_variant->font,
                              .scale_x = scale_x > 0 ? scale_x : scale_y,
                              .scale_y = scale_y,
                              .width   = pen_width,
                              .color   = color,
                              .joined  = font_variant->joined};
}

/* where: the text at the box's left, its base line base_line cells under the box's top */
static void place(const dgx_ui_font_variant_t *font_variant, const dgx_hw_context_t *ctx, const dgx_ui_node_t *node, const char *text, int *x, int *y)
{
    int left = 0;
    dgx_hw_bounds(ctx, text, &left, NULL, NULL, NULL);
    *x = node->x - left;
    const int size = font_variant->font->hw->symbol_size_y;
    *y             = node->y + (2 * font_variant->font->hw->base_line * font_variant->height + size) / (2 * size);
}

/* the map, a drawn background: no pixel of it one of the table's colours (its blue never under 16) */
static dgx_screen_t *map;
static void          draw_map(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    dgx_vscreen_region_to_screen(paint->scr, x - paint->ox, y - paint->oy, (dgx_screen_t *)ctx, x, y, w, h, 0);
}

static int panel_differ(const panel_t *panel, dgx_screen_t *screen)
{
    int count = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) count += panel->mem[y][x] != dgx_get_pixel(screen, x, y);
    return count;
}

static int differ(dgx_screen_t *one, dgx_screen_t *other)
{
    int count = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) count += dgx_get_pixel(one, x, y) != dgx_get_pixel(other, x, y);
    return count;
}

/* pixels that differ outside a hole */
static int differ_outside(dgx_screen_t *one, dgx_screen_t *other, int hole_x, int hole_y, int hole_w, int hole_h)
{
    int count = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            const bool in_hole = x >= hole_x && x < hole_x + hole_w && y >= hole_y && y < hole_y + hole_h;
            count += !in_hole && dgx_get_pixel(one, x, y) != dgx_get_pixel(other, x, y);
        }
    return count;
}

/* the expected screen: paper, then the text written by DGX from nothing to an effort (or whole, effort < 0) */
static void reference(dgx_screen_t *expected, const dgx_ui_font_variant_t *font_variant, float scale_x, const dgx_ui_node_t *node, const char *text,
                      float effort, int pen_width)
{
    dgx_fill_rectangle(expected, 0, 0, PANEL_W, PANEL_H, colors[0]);
    dgx_hw_context_t ctx = context(font_variant, scale_x, expected, colors[1], pen_width);
    int              x, y;
    place(font_variant, &ctx, node, text, &x, &y);
    if (effort < 0) {
        dgx_hw_draw(&ctx, x, y, text);
    } else {
        dgx_hw_writing_t writing;
        dgx_hw_writing_begin_with(&writing, &ctx, x, y, text);
        dgx_hw_writing_draw_to(&writing, effort);
    }
}

/* the text written by DGX whole, its base line at base_y, from the box's left */
static void reference_at(dgx_screen_t *expected, const dgx_ui_font_variant_t *font_variant, const dgx_ui_node_t *node, int base_y, const char *text)
{
    dgx_fill_rectangle(expected, 0, 0, PANEL_W, PANEL_H, colors[0]);
    dgx_hw_context_t ctx  = context(font_variant, 0, expected, colors[1], dgx_ui_hand_pen_of(font_variant));
    int              left = 0;
    dgx_hw_bounds(&ctx, text, &left, NULL, NULL, NULL);
    dgx_hw_draw(&ctx, node->x - left, base_y, text);
}

/* the lowest row of ink in a widget's box */
static int ink_bottom(dgx_screen_t *screen, const dgx_ui_node_t *node)
{
    int bottom = -1;
    for (int y = node->y; y < node->y + node->h; ++y)
        for (int x = node->x; x < node->x + node->w; ++x)
            if (dgx_get_pixel(screen, x, y) == colors[1]) bottom = y;
    return bottom;
}

/* a text in a label of the given width, narrowed: the pixels that differ from DGX writing it at the widest whole
   percent that goes in; the guess by the ratio of the widths, and that percent */
static int narrowed(dgx_screen_t *drawn, dgx_screen_t *expected, const char *text, int box_width, int *guess, int *percent)
{
    static tree_t    tree;
    dgx_hw_context_t ctx  = context(&variant[0], 0, NULL, 0, 2);
    int              wide = 0, width = 0;
    dgx_hw_bounds(&ctx, text, NULL, NULL, &wide, NULL);
    make(&tree, box_width, 30, false, text);
    dgx_ui_paint_t paint = {.scr = drawn, .colors = colors, .color_count = 4};
    dgx_ui_paint(&tree.ui, &paint, 0);
    const dgx_ui_node_t *node    = dgx_ui_node(&tree.ui, 1);
    const float          scale_y = (float)variant[0].height / variant[0].font->hw->symbol_size_y;
    *guess                       = (int)(100.0f * (float)node->w / (float)wide);
    for (*percent = 99; *percent >= 70; --*percent) {
        ctx.scale_x = scale_y * (float)*percent / 100.0f;
        dgx_hw_bounds(&ctx, text, NULL, NULL, &width, NULL);
        if (width <= node->w) break;
    }
    reference(expected, &variant[0], scale_y * (float)*percent / 100.0f, node, text, -1, 2);
    const int off = node->w == box_width && *percent >= 70 ? differ(drawn, expected) : -1;
    dgx_ui_paint_free(&paint);
    dgx_ui_free(&tree.ui);
    return off;
}

int main(void)
{
    variant[0]             = (dgx_ui_font_variant_t){.font = font0ss(), .height = 24, .narrow = 70, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting};
    variant[1]             = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    style                  = (dgx_ui_text_style_t){.variants = variant, .count = 2};
    dgx_screen_t *drawn    = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB); /* what the look draws */
    dgx_screen_t *expected = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB); /* what DGX draws by hand */
    static tree_t tree;

    /* fitted whole: the variant of the handwriting, drawn as DGX draws it at the same place */
    make(&tree, 88, 30, false, "Hello");
    dgx_ui_paint_t paint = {.scr = drawn, .colors = colors, .color_count = 4};
    dgx_ui_paint(&tree.ui, &paint, 0);
    const dgx_ui_node_t *node = dgx_ui_node(&tree.ui, 1);
    reference(expected, &variant[0], 0, node, "Hello", -1, 2);
    CHECK(tree.box.variant == 0 && differ(drawn, expected) == 0);

    /* too wide at its height: narrowed across to the widest whole percent from 99 down to 70 that goes into 88 */
    {
        const char      *text      = "Hello there you";
        dgx_hw_context_t ctx       = context(&variant[0], 0, NULL, 0, 2);
        int              wide      = 0;
        dgx_hw_bounds(&ctx, text, NULL, NULL, &wide, NULL);
        CHECK(wide > 88);
        dgx_ui_text_set(&tree.ui, 1, text);
        dgx_ui_paint(&tree.ui, &paint, 1);
        const float scale_y = (float)variant[0].height / variant[0].font->hw->symbol_size_y;
        int         percent = 99, width = 0;
        for (; percent >= 70; --percent) {
            ctx.scale_x = scale_y * (float)percent / 100.0f;
            dgx_hw_bounds(&ctx, text, NULL, NULL, &width, NULL);
            if (width <= 88) break;
        }
        CHECK(percent >= 70 && percent < 99);
        reference(expected, &variant[0], scale_y * (float)percent / 100.0f, node, text, -1, 2);
        CHECK(tree.box.variant == 0 && differ(drawn, expected) == 0);
    }
    /* narrowed where the guess by the ratio of the widths is off: "Hello there you" in 91 is a percent short of the
       widest that goes in, widened; "Hi there" in 43 two percents over it, narrowed two steps */
    {
        int guess, percent;
        CHECK(narrowed(drawn, expected, "Hello there you", 91, &guess, &percent) == 0 && percent > guess);
        CHECK(narrowed(drawn, expected, "Hi there", 43, &guess, &percent) == 0 && guess - percent >= 2);
    }
    /* where the line stands. A ladder of one face, 32 and 16 high, in a label 120 by 40: a text that takes the smaller
       stands on the first's base line, the bottoms of its letters on the row of the larger's. A bitmap variant first and
       the handwriting after it: on the bitmap's base line. DGX_UI_TEXT_MIDDLE: the line in the middle of the box */
    {
        const dgx_ui_font_variant_t saved[2]   = {variant[0], variant[1]};
        const dgx_ui_text_style_t   saved_style = style;
        static tree_t               ladder;
        variant[0] = (dgx_ui_font_variant_t){.font = font0ss(), .height = 32, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting};
        variant[1] = (dgx_ui_font_variant_t){.font = font0ss(), .height = 16, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting};
        style      = (dgx_ui_text_style_t){.variants = variant, .count = 2};
        dgx_ui_paint_t ladder_paint = {.scr = drawn, .colors = colors, .color_count = 4};
        make(&ladder, 120, 40, false, "mmm");
        dgx_ui_paint(&ladder.ui, &ladder_paint, 0);
        const dgx_ui_node_t *ladder_node = dgx_ui_node(&ladder.ui, 1);
        const int            large       = ink_bottom(drawn, ladder_node), large_variant = ladder.box.variant;
        dgx_ui_text_set(&ladder.ui, 1, "mmm mmm mmm mmm mmm");
        dgx_ui_paint(&ladder.ui, &ladder_paint, 1);
        CHECK(large_variant == 0 && ladder.box.variant == 1 && large > ladder_node->y && ink_bottom(drawn, ladder_node) == large);
        const int size = variant[0].font->hw->symbol_size_y;
        reference_at(expected, &variant[1], ladder_node, ladder_node->y + (2 * variant[0].font->hw->base_line * 32 + size) / (2 * size), "mmm mmm mmm mmm mmm");
        CHECK(differ(drawn, expected) == 0);
        dgx_ui_paint_free(&ladder_paint), dgx_ui_free(&ladder.ui);
        /* the handwriting first and a bitmap after it: the bitmap's letters on the handwriting's base line */
        variant[1] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
        ladder_paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        make(&ladder, 120, 40, false, "mmm mmm mmm mmm mmm");
        dgx_ui_paint(&ladder.ui, &ladder_paint, 0);
        ladder_node              = dgx_ui_node(&ladder.ui, 1);
        const int rows           = (2 * variant[0].font->hw->base_line * 32 + size) / (2 * size);
        const int behind_variant = ladder.box.variant, behind = ink_bottom(drawn, ladder_node) - (ladder_node->y + rows);
        dgx_ui_paint_free(&ladder_paint), dgx_ui_free(&ladder.ui);
        /* the same text with the bitmap alone, on its own base line: its letters as far from the line */
        style        = (dgx_ui_text_style_t){.variants = &variant[1], .count = 1};
        ladder_paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        make(&ladder, 120, 40, false, "mmm mmm mmm mmm mmm");
        dgx_ui_paint(&ladder.ui, &ladder_paint, 0);
        ladder_node      = dgx_ui_node(&ladder.ui, 1);
        const int alone  = ink_bottom(drawn, ladder_node) - (ladder_node->y - TerminusTTFMedium12()->yOffsetLowest);
        CHECK(behind_variant == 1 && behind == alone);
        dgx_ui_paint_free(&ladder_paint), dgx_ui_free(&ladder.ui);
        style = (dgx_ui_text_style_t){.variants = variant, .count = 2};
        variant[1] = (dgx_ui_font_variant_t){.font = font0ss(), .height = 16, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting};
        /* a bitmap first: the handwriting on its base line */
        variant[0] = (dgx_ui_font_variant_t){.font = DejaVuSansBook32()};
        ladder_paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        make(&ladder, 120, 40, false, "mmmmmm");
        dgx_ui_paint(&ladder.ui, &ladder_paint, 0);
        ladder_node = dgx_ui_node(&ladder.ui, 1);
        reference_at(expected, &variant[1], ladder_node, ladder_node->y - DejaVuSansBook32()->yOffsetLowest, "mmmmmm");
        CHECK(ladder.box.variant == 1 && differ(drawn, expected) == 0);
        dgx_ui_paint_free(&ladder_paint), dgx_ui_free(&ladder.ui);
        /* in the middle: one variant 24 high in a box 40 high */
        variant[0] = (dgx_ui_font_variant_t){.font = font0ss(), .height = 24, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting};
        style      = (dgx_ui_text_style_t){.variants = variant, .count = 1, .flags = DGX_UI_TEXT_MIDDLE};
        ladder_paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        make(&ladder, 120, 40, false, "mmm");
        dgx_ui_paint(&ladder.ui, &ladder_paint, 0);
        ladder_node = dgx_ui_node(&ladder.ui, 1);
        reference_at(expected, &variant[0], ladder_node, ladder_node->y + (40 - 24) / 2 + (2 * variant[0].font->hw->base_line * 24 + size) / (2 * size), "mmm");
        CHECK(differ(drawn, expected) == 0);
        dgx_ui_paint_free(&ladder_paint), dgx_ui_free(&ladder.ui);
        variant[0] = saved[0], variant[1] = saved[1], style = saved_style;
    }
    /* too wide even narrowed, centred or to the right: from the box's left all the same, its beginning shown */
    {
        const dgx_ui_font_variant_t saved       = variant[0];
        const dgx_ui_text_style_t   saved_style = style;
        const char                 *text        = "lmmmmmmmmmmmmmmmmmmmmmmml";
        variant[0] = (dgx_ui_font_variant_t){.font = font0ss(), .height = 24, .narrow = 90, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting};
        dgx_hw_context_t ctx   = context(&variant[0], (float)variant[0].height / variant[0].font->hw->symbol_size_y * 0.9f, NULL, 0, 2);
        int              width = 0;
        dgx_hw_bounds(&ctx, text, NULL, NULL, &width, NULL);
        CHECK(width > 60);
        static tree_t           aligned;
        const dgx_font_align_t  aligns[3] = {DGX_FONT_ALIGN_LEFT, DGX_FONT_ALIGN_CENTER, DGX_FONT_ALIGN_RIGHT};
        for (int k = 0; k < 3; ++k) {
            style = (dgx_ui_text_style_t){.variants = variant, .count = 1, .align = aligns[k]};
            dgx_ui_paint_t aligned_paint = {.scr = k ? expected : drawn, .colors = colors, .color_count = 4};
            make(&aligned, 60, 30, false, text);
            dgx_ui_paint(&aligned.ui, &aligned_paint, 0);
            if (k) CHECK(differ(drawn, expected) == 0);
            dgx_ui_paint_free(&aligned_paint), dgx_ui_free(&aligned.ui);
        }
        variant[0] = saved, style = saved_style;
    }
    /* too wide even narrowed, and no bitmap variant to take: cut by its box, nothing outside it */
    {
        panel_t       *panel       = panel_make();
        dgx_ui_paint_t panel_paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        style.count                = 1;
        dgx_ui_text_set(&tree.ui, 1, "Hello there, all of you");
        panel_count_from_now(panel);
        dgx_ui_paint(&tree.ui, &panel_paint, 2);
        CHECK(panel_written(panel) > 0 && panel_written_outside(panel, node->x, node->y, node->x + node->w, node->y + node->h) == 0);
        style.count = 2;
        dgx_ui_paint_free(&panel_paint);
        free(panel);
    }
    dgx_ui_free(&tree.ui);

    /* letters coming, 30 ms each: written stroke by stroke, each pass as DGX writes the text from nothing to the same
       effort; the clock running over in the middle */
    {
        const char    *text  = "Hi there";
        const uint32_t start = 4294967100u;
        make(&tree, 88, 30, false, text);
        paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        dgx_ui_paint(&tree.ui, &paint, start - 1000); /* the text there whole a second before */
        dgx_ui_text_appear(&tree.ui, 1, 30);
        dgx_hw_context_t ctx      = context(&variant[0], 0, NULL, 0, 2);
        const float      effort   = dgx_hw_text_effort(variant[0].font, text, true, &ctx.pace);
        const uint32_t   total_ms = 8 * 30;
        int              off = 0, passes = 0, mirror_off = 0, woke_wrong = 0;
        bool             more = true;
        /* a mirror composed before the first pass and after each, at the time of the pass before: it stores nothing,
           so the writing starts at the first pass and goes on where it stood */
        dgx_screen_t *mirror = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_area_to(&tree.ui, &paint, mirror, NULL, 0, 0, PANEL_W, PANEL_H);
        for (uint32_t ms = 0; more && ms <= total_ms + 100; ms += 20, ++passes) {
            more = dgx_ui_paint(&tree.ui, &paint, start + ms);
            woke_wrong += paint.wake_ms != (more ? (uint32_t)DGX_UI_HAND_FRAME_MS : DGX_UI_WAKE_NONE); /* a frame asked while written */
            reference(expected, &variant[0], 0, node, text, ms >= total_ms ? effort + 1.0f : effort * (float)ms / (float)total_ms, 2);
            off += differ(drawn, expected);
            dgx_ui_paint_area_to(&tree.ui, &paint, mirror, NULL, 0, 0, PANEL_W, PANEL_H);
            mirror_off += differ(mirror, drawn);
        }
        CHECK(passes > 5 && !more && off == 0 && mirror_off == 0 && woke_wrong == 0);
        dgx_screen_destroy(&mirror);
        /* changed while written: begun again from nothing */
        dgx_ui_text_appear(&tree.ui, 1, 30);
        dgx_ui_paint(&tree.ui, &paint, 5000);
        dgx_ui_paint(&tree.ui, &paint, 5060);
        dgx_ui_text_set(&tree.ui, 1, "Yo");
        dgx_ui_paint(&tree.ui, &paint, 5100);
        dgx_ui_paint(&tree.ui, &paint, 5130);
        const float yo_effort = dgx_hw_text_effort(variant[0].font, "Yo", true, &ctx.pace);
        reference(expected, &variant[0], 0, node, "Yo", yo_effort * 30.0f / 60.0f, 2);
        CHECK(differ(drawn, expected) == 0);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&tree.ui);
    }

    /* letters coming in parts: a popup over the middle of the label, its look called for the parts left and right of
       it, each written as DGX writes the text to the same effort */
    {
        const char *text = "Hi there";
        make(&tree, 88, 30, false, text);
        paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        dgx_ui_paint(&tree.ui, &paint, 0);
        static dgx_ui_node_t         popup_nodes[1];
        static dgx_ui_t              popup;
        static const dgx_ui_widget_t popup_w = {.kind = DGX_UI_FRAME};
        dgx_ui_init(&popup, popup_nodes, 1, 1);
        dgx_ui_add(&popup, -1, &popup_w);
        const int popup_x = node->x + 30, popup_y = node->y - 2, popup_width = 20, popup_height = node->h + 4;
        CHECK(dgx_ui_popup_open(&tree.ui, &popup, popup_x, popup_y, popup_width, popup_height, 0));
        dgx_ui_text_appear(&tree.ui, 1, 30);
        dgx_hw_context_t ctx      = context(&variant[0], 0, NULL, 0, 2);
        const float      effort   = dgx_hw_text_effort(variant[0].font, text, true, &ctx.pace);
        const uint32_t   total_ms = 8 * 30;
        int              off = 0, passes = 0;
        bool             more = true;
        for (uint32_t ms = 0; more && ms <= total_ms + 100; ms += 20, ++passes) {
            more = dgx_ui_paint(&tree.ui, &paint, 1000 + ms);
            reference(expected, &variant[0], 0, node, text, ms >= total_ms ? effort + 1.0f : effort * (float)ms / (float)total_ms, 2);
            off += differ_outside(drawn, expected, popup_x, popup_y, popup_width, popup_height);
        }
        CHECK(passes > 5 && !more && off == 0);
        dgx_ui_popup_close(&tree.ui);
        dgx_ui_free(&popup);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&tree.ui);
    }

    /* letters coming straight on a panel: each pass writes its new strokes only, those before them replayed under an
       empty clip; all the passes together write about what the whole text inks, not all of it again in every pass */
    {
        panel_t       *panel       = panel_make();
        dgx_ui_paint_t panel_paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        make(&tree, 88, 30, false, "Hi there");
        dgx_ui_paint(&tree.ui, &panel_paint, 0);
        dgx_ui_text_appear(&tree.ui, 1, 30);
        dgx_ui_paint(&tree.ui, &panel_paint, 1000); /* begun again: the box laid, no stroke yet */
        int  written = 0, passes = 0;
        bool more    = true;
        for (uint32_t ms = 20; more && ms <= 8 * 30 + 100; ms += 20, ++passes) {
            panel_count_from_now(panel);
            more = dgx_ui_paint(&tree.ui, &panel_paint, 1000 + ms);
            written += panel_written(panel);
        }
        int ink = 0;
        for (int y = node->y; y < node->y + node->h; ++y)
            for (int x = node->x; x < node->x + node->w; ++x) ink += panel->mem[y][x] == colors[1];
        CHECK(passes > 5 && !more && ink > 50 && written <= 2 * ink);
        dgx_ui_paint_free(&panel_paint);
        dgx_ui_free(&tree.ui);
        free(panel);
    }

    /* a pen of 0: a hairline; a soft pen straight: the hard pen of its width, told in the log */
    {
        variant[0].pen = 0;
        make(&tree, 88, 30, false, "Hello");
        paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        dgx_ui_paint(&tree.ui, &paint, 0);
        reference(expected, &variant[0], 0, node, "Hello", -1, 1);
        CHECK(differ(drawn, expected) == 0);
        variant[0].pen = 3 | DGX_PEN_SOFT;
        dgx_ui_redraw(&tree.ui, 1);
        dgx_ui_paint(&tree.ui, &paint, 1);
        reference(expected, &variant[0], 0, node, "Hello", -1, 3);
        CHECK(differ(drawn, expected) == 0);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&tree.ui);
    }

    /* a widget of DGX_UI_CANVAS with dgx_ui_handwriting: written straight, the hard pen; with
       dgx_ui_handwriting_canvas, into the canvas by the soft pen: the amounts of ink DGX's soft pen writes in an 8-bit
       screen, put through the table from the paper to the ink */
    {
        make(&tree, 88, 30, true, "Hello");
        paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4};
        dgx_ui_paint(&tree.ui, &paint, 0);
        reference(expected, &variant[0], 0, node, "Hello", -1, 3);
        CHECK(differ(drawn, expected) == 0 && paint.canvases == NULL);
        variant[0].hand = &dgx_ui_handwriting_canvas;
        dgx_ui_redraw(&tree.ui, 1);
        dgx_ui_paint(&tree.ui, &paint, 1);
        dgx_screen_t    *amounts = dgx_vscreen_init(node->w, node->h, 8, DgxScreenRGB);
        dgx_hw_context_t ctx     = context(&variant[0], 0, amounts, 255, 3 | (int)DGX_PEN_SOFT);
        int              x, y;
        place(&variant[0], &ctx, node, "Hello", &x, &y);
        dgx_hw_draw(&ctx, x - node->x, y - node->y, "Hello");
        static uint16_t table[256];
        dgx_lut_gradient(table, 0, 256, 0x000000, 0xffffff);
        dgx_fill_rectangle(expected, 0, 0, PANEL_W, PANEL_H, colors[0]);
        dgx_vscreen8_to_screen16(expected, node->x, node->y, amounts, table, DGX_VSCREEN_EXACT);
        int soft = 0; /* pixels neither paper nor ink: the pen's soft edge */
        for (int row = node->y; row < node->y + node->h; ++row)
            for (int column = node->x; column < node->x + node->w; ++column) {
                const uint32_t pixel = dgx_get_pixel(expected, column, row);
                soft += pixel != colors[0] && pixel != colors[1];
            }
        CHECK(soft > 0 && differ(drawn, expected) == 0 && paint.canvases != NULL);
        dgx_screen_destroy(&amounts);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&tree.ui);
        variant[0].hand = &dgx_ui_handwriting;
    }

    /* over a drawn background, its letters coming: in strips on a panel that cannot be read as straight on a screen,
       pass after pass, the label sent once in each; the letters on the map */
    {
        variant[0].pen = 2;
        map            = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) dgx_set_pixel(map, x, y, (uint32_t)((x * 3 & 0x1f) << 11 | (y * 2 & 0x1f) << 5 | ((x + y) & 0x0f) | 0x10));
        panel_t       *panel = panel_make();
        static uint8_t strip_buffer[PANEL_W * 2 * 8], small_buffer[64];
        static tree_t  on_panel;
        make(&on_panel, 88, 30, false, "Hi there"), make(&tree, 88, 30, false, "Hi there");
        dgx_ui_paint_t panel_paint = {.scr         = &panel->sbus.scr,
                                      .colors      = colors,
                                      .color_count = 4,
                                      .buffer      = strip_buffer,
                                      .buffer_size = sizeof strip_buffer,
                                      .strips      = &dgx_ui_strips};
        paint = (dgx_ui_paint_t){.scr = drawn, .colors = colors, .color_count = 4, .buffer = small_buffer, .buffer_size = sizeof small_buffer};
        dgx_ui_background_set(&panel_paint, draw_map, map), dgx_ui_background_set(&paint, draw_map, map);
        dgx_ui_paint(&on_panel.ui, &panel_paint, 0), dgx_ui_paint(&tree.ui, &paint, 0);
        CHECK(panel_differ(panel, drawn) == 0);
        dgx_ui_text_appear(&on_panel.ui, 1, 30), dgx_ui_text_appear(&tree.ui, 1, 30);
        int  off = 0, twice = 0, passes = 0;
        bool more = true;
        for (uint32_t ms = 100; more && ms <= 100 + 8 * 30 + 100; ms += 20, ++passes) {
            panel_count_from_now(panel);
            more = dgx_ui_paint(&on_panel.ui, &panel_paint, ms);
            dgx_ui_paint(&tree.ui, &paint, ms);
            off += panel_differ(panel, drawn), twice += panel_twice(panel);
        }
        int inked = 0, mapped = 0;
        for (int y = node->y; y < node->y + node->h; ++y)
            for (int x = node->x; x < node->x + node->w; ++x) inked += panel->mem[y][x] == colors[1], mapped += panel->mem[y][x] == dgx_get_pixel(map, x, y);
        CHECK(passes > 5 && !more && off == 0 && twice == 0 && inked > 50 && inked + mapped == node->w * node->h);
        dgx_ui_paint_free(&panel_paint), dgx_ui_paint_free(&paint);
        dgx_ui_free(&on_panel.ui), dgx_ui_free(&tree.ui);
        dgx_screen_destroy(&map);
        free(panel);
    }
    drawn->destroy(&drawn), expected->destroy(&expected);
    CHECK_DONE();
}
