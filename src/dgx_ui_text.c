/*
 * dgx_ui_text.c
 *
 *  The text box of a widget: its string kept in the program's buffer and compared when it is set. Measuring and
 *  fitting are the painter's (dgx_ui_paint.c): nothing here calls DGX.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdio.h>
#include <string.h>

#include "dgx_ui_priv.h"
#include "dgx_ui_text.h"

int dgx_ui_variant_line(const dgx_ui_font_variant_t *v)
{
    if (!v || !v->font) return 0;
    return v->font->f_type == DGX_FONT_HW ? v->height : v->font->y_advance;
}

int dgx_ui_textbox_height(const dgx_ui_textbox_t *box)
{
    if (!box || !box->style || !box->style->count) return 0;
    return box->lines * dgx_ui_variant_line(&box->style->variants[0]);
}

void dgx_ui_textbox_init(dgx_ui_textbox_t *box, char *buf, int size, int lines, const dgx_ui_text_style_t *style)
{
    memset(box, 0, sizeof(*box));
    box->buf   = buf;
    box->size  = (uint16_t)(size < 0 ? 0 : size > 65535 ? 65535 : size);
    box->lines = (uint8_t)(lines < 1 ? 1 : lines > 255 ? 255 : lines);
    box->style = style;
    if (buf && box->size) buf[0] = 0;
}

/* where a text of n bytes kept of a longer one ends on a whole letter of UTF-8: before a letter cut short */
static size_t whole_chars(const char *s, size_t n)
{
    if (n == 0) return 0;
    size_t k = n - 1;
    while (k > 0 && ((unsigned char)s[k] & 0xC0) == 0x80) --k; /* back to the first byte of the last letter */
    const unsigned char lead = (unsigned char)s[k];
    const size_t        len  = lead < 0x80 ? 1 : lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    return k + len > n ? k : n;
}

/* a text of a buffer up to this long is compared with the one before; a longer buffer is taken as changed */
enum { COMPARED = 256 };

/* the lists in the popups over a tree that show the box of a widget of it, told its text changed (dgx_ui_priv.h).
   Shared by the trees of every task, and harmless: each call writes it with the one value it can have, and a pointer
   is read whole */
void (*dgx_ui_text_shown_over)(dgx_ui_t *ui, const dgx_ui_textbox_t *box);

/* a text set anew: its scroll and its letters from the start at the next pass; a list keeps its window, as its rows
   are its range and its pick stays in view, and a dropdown keeps its list's */
static void restart(dgx_ui_textbox_t *box, const dgx_ui_widget_t *w)
{
    if (w->kind != DGX_UI_LIST && w->kind != DGX_UI_DROPDOWN) box->top = 0;
    box->flags = (uint8_t)((box->flags | DGX_UI_TEXTBOX_START) & ~(DGX_UI_TEXTBOX_CAME | DGX_UI_TEXTBOX_ENDED | DGX_UI_TEXTBOX_COUNTED |
                                                                 DGX_UI_TEXTBOX_SEEN | DGX_UI_TEXTBOX_WHOLE));
}

/* the text of widget i set: drawn and fitted again, begun anew, and the lists over its tree that show its box told */
static bool text_changed(dgx_ui_t *ui, int i, dgx_ui_node_t *w, dgx_ui_textbox_t *box)
{
    w->marks |= DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT;
    dgx_ui_touched(ui, i);
    restart(box, w->widget);
    if (dgx_ui_text_shown_over) dgx_ui_text_shown_over(ui, box);
    return true;
}

bool dgx_ui_text_vprintf(dgx_ui_t *ui, int i, const char *format, va_list args)
{
    dgx_ui_node_t    *w   = dgx_ui_node(ui, i);
    dgx_ui_textbox_t *box = w && w->widget ? w->widget->text : NULL;
    if (!box || !box->buf || box->size < 1) return false;
    if (box->size > COMPARED) {
        const int n = vsnprintf(box->buf, box->size, format, args);
        if (n < 0) box->buf[0] = 0;
        else if (n >= box->size) box->buf[whole_chars(box->buf, box->size - 1)] = 0;
        return text_changed(ui, i, w, box);
    }
    /* written beside the old one and compared: the same text changes nothing */
    char      text[COMPARED];
    const int n = vsnprintf(text, box->size, format, args);
    if (n < 0) text[0] = 0;
    else if (n >= box->size) text[whole_chars(text, box->size - 1)] = 0;
    if (strcmp(text, box->buf) == 0) return false;
    strcpy(box->buf, text);
    return text_changed(ui, i, w, box);
}

bool dgx_ui_text_printf(dgx_ui_t *ui, int i, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const bool changed = dgx_ui_text_vprintf(ui, i, format, args);
    va_end(args);
    return changed;
}

bool dgx_ui_text_set(dgx_ui_t *ui, int i, const char *text)
{
    return dgx_ui_text_printf(ui, i, "%s", text ? text : "");
}

void dgx_ui_text_written(dgx_ui_t *ui, int i)
{
    dgx_ui_node_t    *w   = dgx_ui_node(ui, i);
    dgx_ui_textbox_t *box = w && w->widget ? w->widget->text : NULL;
    if (!box || !box->buf) return;
    if (box->size && !memchr(box->buf, 0, box->size)) box->buf[whole_chars(box->buf, box->size - 1)] = 0; /* no end: cut on a letter */
    text_changed(ui, i, w, box);
}

void dgx_ui_text_appear(dgx_ui_t *ui, int i, int letter_ms)
{
    dgx_ui_node_t    *n   = dgx_ui_node(ui, i);
    dgx_ui_textbox_t *box = n && n->widget ? n->widget->text : NULL;
    if (!box) return;
    box->letter_ms = (uint8_t)(letter_ms < 0 ? 0 : letter_ms > 255 ? 255 : letter_ms);
    box->flags     = (uint8_t)((box->flags | DGX_UI_TEXTBOX_START) & ~(DGX_UI_TEXTBOX_CAME | DGX_UI_TEXTBOX_ENDED | DGX_UI_TEXTBOX_SEEN |
                                                                       DGX_UI_TEXTBOX_WHOLE));
    n->marks |= DGX_UI_MARK_CHANGED;
    dgx_ui_touched(ui, i);
}

int dgx_ui_text_state(const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t    *n   = dgx_ui_node(ui, i);
    const dgx_ui_textbox_t *box = n && n->widget ? n->widget->text : NULL;
    if (!box) return 0;
    if (box->flags & DGX_UI_TEXTBOX_SEEN) return DGX_UI_TEXT_SEEN;
    return box->letter_ms && !(box->flags & DGX_UI_TEXTBOX_CAME) ? DGX_UI_TEXT_COMING : 0;
}

void dgx_ui_text_whole(dgx_ui_t *ui, int i)
{
    dgx_ui_node_t    *n   = dgx_ui_node(ui, i);
    dgx_ui_textbox_t *box = n && n->widget ? n->widget->text : NULL;
    if (!box || (box->flags & DGX_UI_TEXTBOX_CAME)) return; /* all came: scrolling or at its end, left as it is */
    /* START with CAME: the time counts again from the next pass, for the scroll alone, as WHOLE gives the letters no time
       there; START clears CAME where it begins a text anew (restart(), dgx_ui_text_appear()), not where it is set
       beside it, as here, and the looks keep a CAME they find */
    box->flags |= DGX_UI_TEXTBOX_START | DGX_UI_TEXTBOX_CAME | DGX_UI_TEXTBOX_WHOLE;
    n->marks |= DGX_UI_MARK_CHANGED;
    dgx_ui_touched(ui, i);
}

void dgx_ui_text_scroll(dgx_ui_t *ui, int i, int lines)
{
    dgx_ui_node_t    *n   = dgx_ui_node(ui, i);
    dgx_ui_textbox_t *box = n && n->widget ? n->widget->text : NULL;
    if (!box || !lines) return;
    int top = box->top + lines; /* a text's bound by the painter, which knows how many lines there are */
    if (n->widget->kind == DGX_UI_LIST) {
        /* a list's window: within its rows, its pick staying */
        int       rh, at;
        const int shown = dgx_ui_list_rows(ui, n, &rh, &at), last = n->widget->max - n->widget->min + 1 - shown;
        top             = top > last ? last : top;
    }
    box->top = (uint16_t)(top < 0 ? 0 : top > 0xFFFF ? 0xFFFF : top);
    n->marks |= DGX_UI_MARK_CHANGED;
    dgx_ui_touched(ui, i);
}
