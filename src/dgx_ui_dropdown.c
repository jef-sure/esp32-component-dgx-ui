/*
 * dgx_ui_dropdown.c
 *
 *  The list of a dropdown opened as a popup: a frame holding a list on the dropdown's own text box, placed under the
 *  dropdown, above it, or moved up to fit the screen, as high as the screen allows and scrolling past it; the row
 *  chosen given to the dropdown. A file of its own: a program with no dropdown carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_dropdown.h"
#include "dgx_ui_priv.h"
#include "dgx_ui_text.h"

/* the list's row chosen: the dropdown's value, told when it changed; the list closed either way */
static void on_list(dgx_ui_t *tree, int li, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_ACTIVATED) return; /* a pick moved by the keys or a finger: nothing of the dropdown yet */
    dgx_ui_dropdown_t *dd = ctx;
    dgx_ui_t          *ui = dd->ui;
    const int          i = dd->i, value = dgx_ui_value_of(node);
    dgx_ui_popup_close(ui);
    dd->ui                 = NULL;
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    /* gone, or another widget in its place (removed while its list was open), or the option it had: nothing told */
    if (!n || !n->widget || n->widget->kind != DGX_UI_DROPDOWN || n->widget->text != dd->list.text || dgx_ui_value_of(n) == value) return;
    dgx_ui_set_value(ui, i, value);
    if (n->widget->on) n->widget->on(ui, i, n, DGX_UI_CHANGED, n->widget->ctx); /* the user chose */
}

/* the text of a widget of ui set: the lists in the popups over it that show the same box (a dropdown's open list)
   drawn again; their pick is as it was, and their window with it */
static void lists_told(dgx_ui_t *ui, const dgx_ui_textbox_t *box)
{
    for (dgx_ui_t *tree = ui->popup; tree; tree = tree->popup)
        for (int k = 0; k < tree->count; ++k) {
            dgx_ui_node_t *node = dgx_ui_node(tree, k);
            if (!node || !node->widget || node->widget->kind != DGX_UI_LIST || node->widget->text != box) continue;
            node->marks |= DGX_UI_MARK_CHANGED;
            dgx_ui_touched(tree, k);
        }
}

bool dgx_ui_dropdown_open(dgx_ui_t *ui, int i, dgx_ui_dropdown_t *dd, const dgx_ui_look_t *frame_look, const dgx_ui_look_t *list_look)
{
    const dgx_ui_node_t   *n = dgx_ui_node(ui, i);
    const dgx_ui_widget_t *w = n ? n->widget : NULL;
    if (!w || w->kind != DGX_UI_DROPDOWN || dd->tree.level || w->max < w->min) return false;
    const int W = ui->screen_w, H = ui->screen_h, u = ui->unit, pad = u; /* the frame's border, one point */
    if (W <= 0 || H <= 0) return false;
    /* the rows as high as the dropdown, never lower than a line of its text: the list's step, in whole points */
    const int rows = w->max - w->min + 1, line = w->text ? dgx_ui_textbox_height(w->text) / (w->text->lines ? w->text->lines : 1) : 0;
    const int step = ((n->h > line ? n->h : line) + u - 1) / u, rh = step * u;
    if (rh <= 0) return false;
    /* all of its rows under it; above it; else, all fitting the screen, moved up to fit over it. More rows than the
       screen takes, it scrolls and never lies over the dropdown: as many rows as the larger room takes, under it or
       above it, one at least */
    const int full = rows * rh + 2 * pad, below = H - (n->y + n->h), above = n->y;
    int       h = full, y;
    if (full <= below) y = n->y + n->h;
    else if (full <= above) y = n->y - full;
    else if (full <= H) y = H - full;
    else {
        const int room  = below >= above ? below : above;
        const int shown = (room - 2 * pad) / rh > 0 ? (room - 2 * pad) / rh : 1;
        h = shown * rh + 2 * pad;
        y = below >= above ? n->y + n->h : n->y - h;
        if (y + h > H) y = H - h; /* a room lower than one row: as near it as the screen allows */
        if (y < 0) y = 0;
    }
    const int width = n->w;
    int       x     = n->x;
    if (x + width > W) x = W - width;
    if (x < 0) x = 0;
    /* the popup: a frame of a column holding the list on the dropdown's text box, its pick the value chosen */
    dd->pick  = dgx_ui_value_of(n); /* the list's own place: the dropdown's is written when a row is chosen */
    dd->frame = (dgx_ui_widget_t){.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 1, .look = frame_look};
    dd->list  = (dgx_ui_widget_t){.kind  = DGX_UI_LIST,
                                  .flags = DGX_UI_REST | DGX_UI_STRETCH,
                                  .min   = w->min,
                                  .max   = w->max,
                                  .step  = (int16_t)step,
                                  .value = &dd->pick,
                                  .text  = w->text,
                                  .look  = list_look ? list_look : &dgx_ui_look_list,
                                  .on    = on_list,
                                  .ctx   = dd};
    dgx_ui_init(&dd->tree, dd->nodes, 2, u);
    dgx_ui_add(&dd->tree, -1, &dd->frame);
    const int li = dgx_ui_add(&dd->tree, 0, &dd->list);
    if (!dgx_ui_popup_open(ui, &dd->tree, x, y, width, h, DGX_UI_POPUP_MODAL | DGX_UI_POPUP_CLOSE_OUTSIDE | DGX_UI_POPUP_BACK)) return false;
    dgx_ui_text_shown_over = lists_told; /* its options set while the list is open: the list hears of it */
    dd->ui = ui, dd->i = (int16_t)i;
    dgx_ui_focus(&dd->tree, li); /* the keys move the pick */
    return true;
}
