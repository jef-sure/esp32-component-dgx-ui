/*
 * dgx_ui_hand.c
 *
 *  The library's handwriting, named by a variant of a handwritten font (dgx_ui_handwriting): a line of the one-line
 *  look of a text written by DGX's pen at the variant's height, narrowed as far as the variant allows to go in, set
 *  in its box by the style, straight on the screen by a hard pen. Into a canvas, by a soft pen too, it is written by
 *  dgx_ui_hand_canvas.c, named as dgx_ui_handwriting_canvas. Letters coming are written stroke by stroke at the pace
 *  of a pen: no state of the pen is kept; each pass writes the strokes already written again under an empty clip,
 *  then the new ones. A program that names no handwriting carries none.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <limits.h>
#include <string.h>

#include "dgx_ui_hand_priv.h"
#include "dgx_ui_paint_priv.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

/* the width of a text narrowed to `percent` across */
static int width_at(dgx_hw_context_t *ctx, const char *text, int percent)
{
    int width    = 0;
    ctx->scale_x = ctx->scale_y * (float)percent / 100.0f;
    dgx_hw_bounds(ctx, text, NULL, NULL, &width, NULL);
    return width;
}

/* a variant it can write: a handwritten font and a height */
static bool writable(const dgx_ui_font_variant_t *variant)
{
    return variant->font && variant->font->f_type == DGX_FONT_HW && variant->font->hw && variant->height > 0;
}

/* the width of a text at the variant's height, narrowed in whole percents to the widest that goes into `field`, never
   past the variant's least; and the scales it is written at */
static int measure(const dgx_ui_font_variant_t *variant, const char *text, int field, float *scale_x, float *scale_y)
{
    const dgx_hw_font_t *hw = variant->font->hw;
    *scale_y = *scale_x  = hw->symbol_size_y ? (float)variant->height / hw->symbol_size_y : 1.0f;
    dgx_hw_context_t ctx = {.font = variant->font, .scale_x = *scale_x, .scale_y = *scale_y, .width = dgx_ui_hand_pen(variant), .joined = variant->joined};
    int              width = 0;
    if (!dgx_hw_bounds(&ctx, text, NULL, NULL, &width, NULL)) return 0;
    if (width <= field || !variant->narrow || variant->narrow >= 100) return width;
    /* from the guess by the ratio, a percent at a time: the pen's width does not narrow, so the ratio falls short */
    int percent = (int)(100.0f * (float)field / (float)width);
    percent     = percent < variant->narrow ? variant->narrow : percent > 99 ? 99 : percent;
    width       = width_at(&ctx, text, percent);
    if (width <= field) {
        for (int wider; percent < 99 && (wider = width_at(&ctx, text, percent + 1)) <= field; ++percent) width = wider;
    } else {
        while (percent > variant->narrow && (width = width_at(&ctx, text, --percent)) > field) {}
    }
    *scale_x = *scale_y * (float)percent / 100.0f;
    return width;
}

/* letters of a text */
static int letters_of(const char *text)
{
    int letters = 0;
    for (; *text; ++text) letters += ((unsigned char)*text & 0xC0) != 0x80;
    return letters;
}

void dgx_ui_hand_write(const dgx_hw_context_t *ctx, int x, int y, const char *text, float from, float to, bool all)
{
    if (all) {
        dgx_hw_draw(ctx, x, y, text);
        return;
    }
    dgx_hw_writing_t writing;
    dgx_hw_writing_begin_with(&writing, ctx, x, y, text);
    if (from > 0) {
        int clip_x, clip_y, clip_w, clip_h;
        dgx_screen_clip_get(ctx->scr, &clip_x, &clip_y, &clip_w, &clip_h);
        dgx_screen_clip(ctx->scr, 0, 0, 0, 0);
        dgx_hw_writing_draw_to(&writing, from);
        dgx_screen_clip(ctx->scr, clip_x, clip_y, clip_w, clip_h);
    }
    dgx_hw_writing_draw_to(&writing, to);
}

/* base_line cells at the variant's height, rounded */
int dgx_ui_hand_base_rows(const dgx_ui_font_variant_t *variant)
{
    if (!writable(variant) || variant->font->hw->symbol_size_y <= 0) return -1;
    const int size = variant->font->hw->symbol_size_y;
    return (2 * variant->font->hw->base_line * variant->height + size) / (2 * size);
}

int dgx_ui_hand_span(const dgx_ui_font_variant_t *variant, const char *text, int n)
{
    char line[DGX_UI_LINE + 1];
    if (n > DGX_UI_LINE) n = DGX_UI_LINE;
    if (n <= 0 || !writable(variant)) return 0;
    memcpy(line, text, (size_t)n);
    line[n]               = 0;
    const float      scale = (float)variant->height / variant->font->hw->symbol_size_y;
    dgx_hw_context_t ctx   = {.font = variant->font, .scale_x = scale, .scale_y = scale, .width = dgx_ui_hand_pen(variant), .joined = variant->joined};
    int              width = 0;
    dgx_hw_bounds(&ctx, line, NULL, NULL, &width, NULL);
    return width;
}

int dgx_ui_hand_width(const dgx_ui_font_variant_t *variant, const char *text, int field)
{
    float scale_x, scale_y;
    return writable(variant) ? measure(variant, text, field, &scale_x, &scale_y) : INT_MAX;
}

bool dgx_ui_hand_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms, const dgx_ui_font_variant_t *variant)
{
    dgx_ui_node_t            *node   = dgx_ui_node(ui, i);
    dgx_ui_textbox_t         *box    = node->widget->text;
    const char               *text   = box->buf ? box->buf : "";
    const dgx_ui_text_look_t *params = look->params;
    if (!writable(variant)) {
        if (paint->whole) dgx_ui_lay_paper(paint, params->paper, node->x, node->y, node->w, node->h);
        return false;
    }
    float scale_x, scale_y;
    measure(variant, text, node->w, &scale_x, &scale_y);
    struct dgx_ui_hand_line_t line = {
        .ctx = {.font = variant->font, .scale_x = scale_x, .scale_y = scale_y, .width = dgx_ui_hand_pen(variant), .joined = variant->joined},
    };
    int left = 0, width = 0;
    dgx_hw_bounds(&line.ctx, text, &left, NULL, &width, NULL);
    /* where: the line in the middle of the box, or on the base line of the style's first variant (its own when that
       cannot be drawn); across as the style sets it, from the box's left when it does not go in, so that its beginning
       shows */
    const dgx_ui_text_style_t *style    = box->style;
    const int                  own_rows = dgx_ui_hand_base_rows(variant), first_rows = dgx_ui_first_base(style);
    if (style->flags & DGX_UI_TEXT_MIDDLE) line.base = node->y + (node->h - variant->height) / 2 + own_rows;
    else line.base = node->y + (first_rows >= 0 ? first_rows : own_rows);
    int room = 0;
    if (width <= node->w) room = style->align == DGX_FONT_ALIGN_CENTER ? (node->w - width) / 2 : style->align == DGX_FONT_ALIGN_RIGHT ? node->w - width : 0;
    line.x = node->x - left + room;
    /* the time: the whole text as long as its letters of letter_ms, from its first pass; counted by difference */
    const bool     start = box->flags & DGX_UI_TEXTBOX_START;
    const uint32_t since = start ? now_ms : box->since_ms;
    line.total_ms        = box->letter_ms ? (uint32_t)letters_of(text) * box->letter_ms : 0;
    line.elapsed_ms      = now_ms - since;
    if (line.elapsed_ms > line.total_ms) line.elapsed_ms = line.total_ms;
    line.came   = line.elapsed_ms >= line.total_ms;
    line.effort = line.total_ms ? dgx_hw_text_effort(variant->font, text, variant->joined, &line.ctx.pace) : 0;
    if (line.total_ms) line.to = line.came ? line.effort + 1.0f : line.effort * (float)line.elapsed_ms / (float)line.total_ms; /* +1: its last stroke too */
    line.anew         = start || (node->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT));
    const bool commit = !paint->composing && paint->part + 1 >= paint->parts; /* the painter's own pass, its last part */
    if (!line.came) dgx_ui_wake_in(paint, DGX_UI_HAND_FRAME_MS); /* the next frame of the pen */
    if (commit) box->since_ms = since, box->flags &= (uint8_t)~DGX_UI_TEXTBOX_START;
    if ((node->widget->flags & DGX_UI_CANVAS) && variant->hand->canvas && variant->hand->canvas->line(paint, ui, i, variant, text, &line, params->ink))
        return !line.came;
    /* straight on the screen: the box laid and all of it written when it is anew or whole; else the new strokes only */
    if (variant->pen & DGX_PEN_SOFT) {
        static bool told;
        if (!told) ESP_LOGW(TAG, "widget %d: a soft pen is soft in a canvas (dgx_ui_handwriting_canvas); straight, a hard one", i);
        told = true;
    }
    line.ctx.scr = paint->scr, line.ctx.color = dgx_ui_color(paint, params->ink);
    const bool     fresh  = line.anew || paint->whole;
    const uint32_t had_ms = fresh ? 0 : box->letters_drawn;
    if (fresh) dgx_ui_lay_paper(paint, params->paper, node->x, node->y, node->w, node->h);
    if (fresh || line.elapsed_ms > had_ms)
        dgx_ui_hand_write(&line.ctx, line.x - paint->ox, line.base - paint->oy, text, dgx_ui_hand_effort_at(&line, had_ms), line.to, !line.total_ms);
    if (commit) box->letters_drawn = (uint16_t)(line.elapsed_ms > 0xFFFF ? 0xFFFF : line.elapsed_ms);
    return !line.came;
}

const dgx_ui_hand_t dgx_ui_handwriting = {.width = dgx_ui_hand_width, .draw = dgx_ui_hand_draw, .base_rows = dgx_ui_hand_base_rows};
