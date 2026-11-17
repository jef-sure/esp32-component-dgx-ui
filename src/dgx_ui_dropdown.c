/*
 * dgx_ui_dropdown.c
 *
 *  The list of a dropdown opened as a popup: a frame holding a list on the dropdown's own text box, placed under the
 *  dropdown, above it, or moved up to fit the screen; the row chosen given to the dropdown. A file of its own: a
 *  program with no dropdown carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_dropdown.h"
#include "dgx_ui_text.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

/* the list's row chosen: the dropdown's value, told when it changed; the list closed either way */
static void on_list(dgx_ui_t *tree, int li, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_ACTIVATED) return; /* a pick moved by the keys or a finger: nothing of the dropdown yet */
    dgx_ui_dropdown_t *dd = ctx;
    dgx_ui_t          *ui = dd->ui;
    const int          i = dd->i, value = node->state_value.value;
    dgx_ui_popup_close(ui);
    dd->ui                 = NULL;
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    /* gone, or another widget in its place (removed while its list was open), or the option it had: nothing told */
    if (!n || !n->widget || n->widget->kind != DGX_UI_DROPDOWN || n->widget->text != dd->list.text || n->state_value.value == value) return;
    dgx_ui_set_value(ui, i, value);
    if (n->widget->on) n->widget->on(ui, i, n, DGX_UI_CHANGED, n->widget->ctx); /* the user chose */
}

bool dgx_ui_dropdown_open(dgx_ui_t *ui, int i, dgx_ui_dropdown_t *dd, const dgx_ui_look_t *frame_look, const dgx_ui_look_t *list_look)
{
    const dgx_ui_node_t   *n = dgx_ui_node(ui, i);
    const dgx_ui_widget_t *w = n ? n->widget : NULL;
    if (!w || w->kind != DGX_UI_DROPDOWN || dd->tree.level || w->max < w->min) return false;
    const int W = ui->screen_w, H = ui->screen_h, u = ui->unit, pad = u; /* the frame's border, one point */
    if (W <= 0 || H <= 0) return false;
    /* the rows as high as the dropdown, never lower than a line of its text */
    const int rows = w->max - w->min + 1, line = w->text ? dgx_ui_textbox_height(w->text) / (w->text->lines ? w->text->lines : 1) : 0;
    int       rh = n->h > line ? n->h : line, h = rows * rh + 2 * pad;
    if (rh <= 0) return false;
    if (h > H) { /* more rows than the screen takes: they share its height */
        ESP_LOGW(TAG, "dropdown %d: %d options of %d pixels do not fit %d: they share it", i, rows, rh, H);
        h = H;
    }
    /* under it; above it; else moved up to fit, over it */
    const int width = n->w;
    int       x = n->x, y = n->y + n->h;
    if (x + width > W) x = W - width;
    if (x < 0) x = 0;
    if (y + h > H) y = n->y - h >= 0 ? n->y - h : H - h;
    /* the popup: a frame of a column holding the list on the dropdown's text box, its pick the value chosen */
    dd->frame = (dgx_ui_widget_t){.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 1, .look = frame_look};
    dd->list  = (dgx_ui_widget_t){.kind  = DGX_UI_LIST,
                                  .flags = DGX_UI_REST | DGX_UI_STRETCH,
                                  .min   = w->min,
                                  .max   = w->max,
                                  .state = {.value = n->state_value.value},
                                  .text  = w->text,
                                  .look  = list_look ? list_look : &dgx_ui_look_list,
                                  .on    = on_list,
                                  .ctx   = dd};
    dgx_ui_init(&dd->tree, dd->nodes, 2, u);
    dgx_ui_add(&dd->tree, -1, &dd->frame);
    const int li = dgx_ui_add(&dd->tree, 0, &dd->list);
    if (!dgx_ui_popup_open(ui, &dd->tree, x, y, width, h, DGX_UI_POPUP_MODAL | DGX_UI_POPUP_CLOSE_OUTSIDE)) return false;
    dd->ui = ui, dd->i = (int16_t)i;
    dgx_ui_focus(&dd->tree, li); /* the keys move the pick */
    return true;
}
