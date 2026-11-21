/*
 * dgx_ui_hand_lines.c
 *
 *  The look of a text of lines in handwriting, dgx_ui_look_hand_lines: the text laid into lines word by word at the
 *  variant's height, not narrowed (the ladder of the style fits it), by the same source as the look of lines
 *  (dgx_ui_lines_layout.h); a bitmap variant of the ladder drawn by that look. Each row written by the hand under a
 *  clip of its band, straight or into the widget's canvas as the variant's handwriting says. Letters coming are
 *  written line after line stroke by stroke, the whole text as long as its letters of letter_ms: the lines done are
 *  drawn only when the widget is drawn whole or anew, the line being written replays its strokes under an empty clip
 *  and draws on. An important text too long scrolls a line every scroll_ms once all came, each step drawn whole; one
 *  that is not is cut, its last row seen ending in "..." in the same hand. The line breaks and the rows' efforts are
 *  kept for the widget in the paint's table (dgx_ui_kept()), laid again when the text, the width or the variant
 *  changed, so that a pass costs the row being written; with no memory for them, laid at every pass, slower. A
 *  program that names it not carries none.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <string.h>

#include "dgx_ui_hand_priv.h"
#include "dgx_ui_paint_priv.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

/* a variant of the ladder: a handwritten one measured at its height, its line that height; a bitmap one by its font */
static int measure_variant(const dgx_ui_font_variant_t *variant, const char *s, int n)
{
    return variant->hand ? dgx_ui_hand_span(variant, s, n) : dgx_ui_width_of(variant->font, s, n);
}
static int line_height_of(const dgx_ui_font_variant_t *variant)
{
    if (variant->hand) return dgx_ui_hand_base_rows(variant) >= 0 ? variant->height : 0;
    return dgx_ui_variant_usable(variant->font) ? variant->font->y_advance : 0;
}
#define LINES_MEASURE(v, s, n) measure_variant(v, s, n)
#define LINES_HEIGHT(v)        line_height_of(v)
#include "dgx_ui_lines_layout.h"

enum { MAX_LINES = 1000 };
static const char ELLIPSIS[] = "...";

/* the line breaks kept for a widget, with the key they were laid for: the text's hash, the width, the variant */
typedef struct {
    uint32_t hash;
    int16_t  width;
    uint8_t  variant;
    uint16_t count;
    struct {
        uint16_t start, end;
        float    effort; /* of the row's whole line; < 0 not counted yet */
    } line[];
} laid_t;

/* the lines of the text as kept for the widget, laid again when its key changed; NULL: none kept (no memory, or the
   plain way asked for a timing), the rows laid at every pass */
static laid_t *laid_lines(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, const dgx_ui_font_variant_t *variant, int v, const char *text, int len, int width, int total)
{
#if DGX_UI_HAND_LINES_PLAIN
    return NULL;
#endif
    bool      made;
    laid_t   *laid = dgx_ui_kept(paint, ui, i, sizeof(laid_t) + (size_t)total * sizeof laid->line[0], &made);
    const uint32_t hash = dgx_ui_hand_hash(text, len);
    if (!laid) return NULL;
    if (!made && laid->hash == hash && laid->width == width && laid->variant == v && laid->count == total) return laid;
    laid->hash = hash, laid->width = (int16_t)width, laid->variant = (uint8_t)v, laid->count = 0;
    for (int from = 0; from < len && laid->count < total;) {
        int       end;
        const int next                 = lay_line(text, len, from, variant, width, &end);
        laid->line[laid->count].start  = (uint16_t)from;
        laid->line[laid->count].end    = (uint16_t)end;
        laid->line[laid->count].effort = -1.0f;
        ++laid->count, from = next;
    }
    return laid;
}

/* a row's text: its line, or for the last row of a text cut that line with "..." as far as both go into width */
static void row_text(char *row, const char *text, int start, int end, bool cut, const dgx_ui_font_variant_t *variant, int width)
{
    int n = end - start > DGX_UI_LINE ? DGX_UI_LINE : end - start;
    memcpy(row, text + start, (size_t)n);
    row[n] = 0;
    if (!cut) return;
    while (n > 0) {
        while (n > 0 && row[n - 1] == ' ') --n;
        memcpy(row + n, ELLIPSIS, sizeof ELLIPSIS);
        if (dgx_ui_hand_span(variant, row, (int)strlen(row)) <= width) return;
        while (n > 0 && ((unsigned char)row[--n] & 0xC0) == 0x80) {}
        row[n] = 0;
    }
    memcpy(row, ELLIPSIS, sizeof ELLIPSIS);
}

/* the clip of the screen cut to rows top .. bottom of the tree's screen, within the part; false: nothing left */
static bool clip_band(dgx_ui_paint_t *paint, const int part[4], int top, int bottom)
{
    const int t = top - paint->oy, b = bottom - paint->oy;
    const int y0 = part[1] > t ? part[1] : t, y1 = part[1] + part[3] < b ? part[1] + part[3] : b;
    if (y1 <= y0) return false;
    dgx_screen_clip(paint->scr, part[0], y0, part[2], y1 - y0);
    return true;
}

static bool write_lines(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms, int v)
{
    dgx_ui_node_t               *node    = dgx_ui_node(ui, i);
    dgx_ui_textbox_t            *box     = node->widget->text;
    const dgx_ui_text_look_t    *params  = look->params;
    const dgx_ui_text_style_t   *style   = box->style;
    const dgx_ui_font_variant_t *variant = &style->variants[v];
    const char                  *text    = box->buf ? box->buf : "";
    const int                    len     = (int)strlen(text);
    const bool                   commit  = !paint->composing && paint->part + 1 >= paint->parts; /* the painter's own pass, its last part */
    const bool                   other   = v != box->variant;
    if (other) paint->whole = true; /* another height: drawn whole */
    const int line_height = variant->height, rows = node->h / line_height > 0 ? node->h / line_height : 1;

    /* the lines and letters: counted again only when the text, the width or the variant changed */
    int total, all_letters;
    if (!(box->flags & DGX_UI_TEXTBOX_COUNTED) || (node->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT)) || other) {
        total = count_lines(text, len, variant, node->w, MAX_LINES, MAX_LINES, &all_letters);
    } else {
        total = box->total_lines, all_letters = box->total_letters;
    }
    /* the time: the whole text as long as its letters of letter_ms, from its first pass; counted by difference. How
       far the pen got is kept in units a text of any length fits in 16 bits: a millisecond for one under 65.5 s */
    const bool     start      = box->flags & DGX_UI_TEXTBOX_START;
    const uint32_t since      = start ? now_ms : box->since_ms;
    const uint32_t total_ms   = box->letter_ms ? (uint32_t)all_letters * box->letter_ms : 0;
    const uint32_t elapsed    = now_ms - since;
    const bool     came       = !box->letter_ms || (box->flags & DGX_UI_TEXTBOX_CAME) || elapsed >= total_ms;
    const uint32_t written_ms = came ? total_ms : elapsed;
    const uint32_t unit_ms    = total_ms / 65536u + 1u;
    /* an important text too long: a line up every scroll_ms once all came, as the look of lines does */
    const bool important = style->flags & DGX_UI_TEXT_IMPORTANT;
    const int  last_top  = total > rows ? total - rows : 0;
    int        top       = box->top;
    bool       more      = !came;
    if (important && style->scroll_ms && total > rows) {
        if (box->flags & DGX_UI_TEXTBOX_ENDED) {
            top = last_top;
        } else {
            top = !came || elapsed < total_ms ? 0 : (int)((elapsed - total_ms) / style->scroll_ms);
            if (top > last_top) top = last_top;
            more |= top < last_top;
        }
    }
    if (top > last_top) top = last_top;
    const bool anew = start || (node->marks & (DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT)) || other || top != box->top;
    if (!came) dgx_ui_wake_in(paint, DGX_UI_HAND_FRAME_MS); /* the next frame of the pen */
    else if (more) dgx_ui_wake_in(paint, total_ms + (uint32_t)(top + 1) * style->scroll_ms - elapsed);

    /* where: into the widget's canvas when its handwriting writes into one, else straight */
    bool          made   = false;
    void         *note   = NULL;
    dgx_screen_t *canvas = (node->widget->flags & DGX_UI_CANVAS) && variant->hand->canvas ? variant->hand->canvas->of(paint, ui, i, &made, &note) : NULL;
    bool          fresh  = canvas ? made || anew : anew || paint->whole;
    const bool    cut    = !important && total - top > rows;
    const int     shown  = total - top < rows ? total - top : rows;
    /* the rows the last pass showed: the block in the middle moves when their number changed, only then */
    const int     shown_before = (box->flags & DGX_UI_TEXTBOX_COUNTED) ? (box->total_lines - box->top < rows ? box->total_lines - box->top : rows) : -1;
    const int     own    = dgx_ui_hand_base_rows(variant);
    int           base0; /* the first row's base line: the first variant's, or the block of the rows in the middle */
    if (style->flags & DGX_UI_TEXT_MIDDLE) {
        const int block = (shown > 0 ? shown : 1) * line_height;
        base0           = node->y + (node->h - block) / 2 + own;
        if (rows > 1 && shown != shown_before) {
            fresh = true; /* the block moved: drawn whole, the rows around it too */
            if (!canvas) paint->whole = true;
        }
    } else {
        const int first = dgx_ui_first_base(style);
        base0           = node->y + (first >= 0 ? first : own);
    }
    /* a canvas: what it holds against what this call writes (dgx_ui_hand_held_t), kept with it; straight, how far the
       pen got on the screen, kept in the box by the painter's own pass */
    dgx_ui_hand_held_t      *held = canvas ? note : NULL;
    const dgx_ui_hand_held_t now  = {.text_hash  = canvas ? dgx_ui_hand_hash(text, len) : 0,
                                     .since_ms   = since,
                                     .written_ms = written_ms,
                                     .top        = (uint16_t)top,
                                     .variant    = (uint8_t)v,
                                     .written    = 1};
    if (canvas)
        fresh |= !held || !held->written || held->text_hash != now.text_hash || held->since_ms != now.since_ms || held->variant != now.variant ||
                 held->top != now.top || now.written_ms < held->written_ms;
    const uint32_t had_ms = fresh ? 0 : canvas ? held->written_ms : (uint32_t)box->letters_drawn * unit_ms;
    if (commit) {
        box->variant = (uint8_t)v, box->top = (uint16_t)top, box->total_lines = (uint16_t)total;
        box->total_letters = (uint16_t)(all_letters > 0xFFFF ? 0xFFFF : all_letters);
        box->since_ms      = since;
        box->flags         = (uint8_t)((box->flags & ~DGX_UI_TEXTBOX_START) | DGX_UI_TEXTBOX_COUNTED);
        if (came) box->flags |= DGX_UI_TEXTBOX_CAME;
        if (important && style->scroll_ms && total > rows && top >= last_top) box->flags |= DGX_UI_TEXTBOX_ENDED;
        if (!canvas) box->letters_drawn = (uint16_t)(written_ms / unit_ms);
    }
    if (!fresh && written_ms <= had_ms) {
        if (canvas) variant->hand->canvas->put(paint, ui, i, params->ink);
        return more; /* nothing new to write */
    }
    if (!canvas && (variant->pen & DGX_PEN_SOFT)) {
        static bool told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
        if (!told) ESP_LOGW(TAG, "widget %d: a soft pen is soft in a canvas (dgx_ui_handwriting_canvas); straight, a hard one", i);
        told = true;
    }

    /* the pen: on the canvas as amounts of ink, by a soft pen too; straight in the ink, a hard one */
    const float      scale = (float)variant->height / variant->font->hw->symbol_size_y;
    dgx_hw_context_t pen   = {.font = variant->font, .scale_x = scale, .scale_y = scale, .width = dgx_ui_hand_pen(variant), .joined = variant->joined};
    const int        off_x = canvas ? node->x : paint->ox, off_y = canvas ? node->y : paint->oy;
    if (canvas) {
        pen.scr = canvas, pen.color = 255, pen.width |= (int)(variant->pen & DGX_PEN_SOFT);
        if (fresh) dgx_fill_rectangle(canvas, 0, 0, canvas->width, canvas->height, 0);
    } else {
        pen.scr = paint->scr, pen.color = dgx_ui_color(paint, params->ink);
        if (fresh) dgx_ui_lay_paper(paint, params->paper, node->x, node->y, node->w, node->h);
    }
    int part[4];
    dgx_screen_clip_get(paint->scr, &part[0], &part[1], &part[2], &part[3]);

    /* each row: its line, kept or laid now, the time its letters take, as much of it written as the time came to */
    laid_t *laid = laid_lines(paint, ui, i, variant, v, text, len, node->w, total);
    int     from = 0, line = 0, letters_before = 0; /* the window moves only once all came: the time of the lines above it no longer counts */
    for (int k = 0; k < rows; ++k) {
        int row_start = len, row_end = len;
        if (laid) {
            if (top + k >= laid->count) break; /* past the last line */
            row_start = laid->line[top + k].start, row_end = laid->line[top + k].end;
        } else {
            while (from < len && line <= top + k) {
                const int next = lay_line(text, len, from, variant, node->w, &row_end);
                row_start      = from;
                if (line < top + k) letters_before += letters_in(text, from, row_end);
                from = next, ++line;
            }
            if (line <= top + k) break; /* past the last line */
        }
        const int      row_letters = letters_in(text, row_start, row_end);
        const uint32_t t0 = (uint32_t)letters_before * box->letter_ms, t1 = t0 + (uint32_t)row_letters * box->letter_ms;
        letters_before += row_letters;
        const bool done_before = !fresh && had_ms >= t1 && box->letter_ms; /* on the screen, or in the canvas, already */
        if (!row_letters || done_before || (box->letter_ms && written_ms <= t0)) continue;
        char       row[DGX_UI_LINE + sizeof ELLIPSIS];
        const bool cut_row = cut && k == rows - 1;
        row_text(row, text, row_start, row_end, cut_row, variant, node->w);
        /* across by the style's align, from the left when it does not go in; down on its row's base line */
        int left = 0, width = 0;
        dgx_hw_bounds(&pen, row, &left, NULL, &width, NULL);
        int room = 0;
        if (width <= node->w) room = style->align == DGX_FONT_ALIGN_CENTER ? (node->w - width) / 2 : style->align == DGX_FONT_ALIGN_RIGHT ? node->w - width : 0;
        const int base = base0 + k * line_height, band_top = base - own;
        if (!canvas && !clip_band(paint, part, band_top, band_top + line_height)) continue;
        /* the effort of the row from what was written to what is now, in the time its letters take */
        const bool whole = !box->letter_ms || (written_ms >= t1 && (fresh || had_ms <= t0));
        float      from_effort = 0, to_effort = 0;
        if (!whole) {
            /* the row's effort: kept for a whole line, counted for one cut */
            float effort;
            if (laid && !cut_row && laid->line[top + k].effort >= 0) effort = laid->line[top + k].effort;
            else effort = dgx_hw_text_effort(variant->font, row, variant->joined, &pen.pace);
            if (laid && !cut_row) laid->line[top + k].effort = effort;
            const float span   = (float)(t1 - t0);
            from_effort        = had_ms <= t0 ? 0 : effort * (float)(had_ms - t0) / span;
            to_effort          = written_ms >= t1 ? effort + 1.0f : effort * (float)(written_ms - t0) / span; /* +1: its last stroke too */
        }
        dgx_ui_hand_write(&pen, node->x - left + room - off_x, base - off_y, row, from_effort, to_effort, whole);
    }
    dgx_screen_clip(paint->scr, part[0], part[1], part[2], part[3]);
    if (canvas) {
        if (held) *held = now; /* the canvas holds what was written */
        variant->hand->canvas->put(paint, ui, i, params->ink);
    }
    return more;
}

bool dgx_ui_draw_hand_lines(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    dgx_ui_node_t            *node   = dgx_ui_node(ui, i);
    dgx_ui_textbox_t         *box    = node->widget->text;
    const dgx_ui_text_look_t *params = look->params;
    if (!box || !box->style || !box->style->count || node->w <= 0) {
        if (paint->whole) dgx_ui_lay_paper(paint, params->paper, node->x, node->y, node->w, node->h);
        return false;
    }
    const dgx_ui_text_style_t *style = box->style;
    const char                *text  = box->buf ? box->buf : "";
    int                        v     = box->variant;
    if (node->marks & DGX_UI_MARK_FIT) v = fit_lines(style, text, (int)strlen(text), node->w, node->h);
    if (v < 0 || v >= style->count || line_height_of(&style->variants[v]) <= 0) {
        if (paint->whole) dgx_ui_lay_paper(paint, params->paper, node->x, node->y, node->w, node->h);
        return false;
    }
    /* a bitmap variant of the ladder: the look of lines draws it, on the first variant's base line */
    if (!style->variants[v].hand) return dgx_ui_draw_lines_in(look, paint, ui, i, now_ms, v, dgx_ui_first_base(style));
    return write_lines(look, paint, ui, i, now_ms, v);
}

static const dgx_ui_text_look_t hand_lines_default = {.ink = 1, .paper = DGX_UI_PAPER};
const dgx_ui_look_t             dgx_ui_look_hand_lines = {.draw = dgx_ui_draw_hand_lines, .params = &hand_lines_default, .fill = -1};
