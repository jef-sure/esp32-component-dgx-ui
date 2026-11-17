/*
 * dgx_ui_msg.c
 *
 *  A message offered to a tree, its data copied out by number and size, done with once; code points typed into the
 *  focused field. No FreeRTOS: sending is dgx_ui_post.c.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <string.h>

#include "dgx_ui_msg.h"
#include "dgx_ui_text.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

const void *dgx_ui_msg_data(const dgx_ui_msg_t *msg, size_t *size)
{
    if (msg->flags & DGX_UI_MSG_REF) {
        if (size) *size = msg->ref.size;
        return msg->ref.data;
    }
    if (size) *size = msg->count;
    return msg->bytes;
}

esp_err_t dgx_ui_msg_copy(const dgx_ui_msg_t *msg, int what, void *out, size_t size)
{
    if (msg->what != what) return ESP_ERR_NOT_FOUND;
    size_t      have;
    const void *data = dgx_ui_msg_data(msg, &have);
    if (have != size) {
        static int logged = -1; /* once a number: a sender and a decoder that disagree are a bug of the program */
        if (logged != what) {
            ESP_LOGE(TAG, "message %d has %u bytes, its decoder wants %u", what, (unsigned)have, (unsigned)size);
            logged = what;
        }
        return ESP_ERR_INVALID_SIZE;
    }
    if (size) memcpy(out, data, size);
    return ESP_OK;
}

void dgx_ui_msg_keep(dgx_ui_t *ui, dgx_ui_msg_t *copy)
{
    if (!ui->offered || (ui->offered->flags & DGX_UI_MSG_KEPT)) { /* outside a take, or kept already: one keeper */
        memset(copy, 0, sizeof(*copy));
        return;
    }
    *copy = *ui->offered;
    copy->flags &= (uint8_t)~DGX_UI_MSG_KEPT;
    ui->offered->flags |= DGX_UI_MSG_KEPT; /* reached through the tree: take() is given it const */
}

void dgx_ui_msg_done(dgx_ui_msg_t *msg)
{
    if (msg->flags & DGX_UI_MSG_KEPT) return; /* a widget's copy is done with instead */
    if ((msg->flags & DGX_UI_MSG_REF) && msg->ref.done) msg->ref.done(msg->ref.data, msg->ref.size);
    msg->flags &= (uint8_t)~DGX_UI_MSG_REF; /* done once */
}

/* the next widget in the order of the tree, parents before children, -1 at the end */
static int next_in_order(const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    if (n->child >= 0) return n->child;
    for (; i >= 0; i = dgx_ui_node(ui, i)->parent) {
        const int next = dgx_ui_node(ui, i)->next;
        if (next >= 0) return next;
    }
    return -1;
}

/* the next widget in the order of the tree past i and all under it, -1 at the end */
static int next_past(const dgx_ui_t *ui, int i)
{
    for (; i >= 0; i = dgx_ui_node(ui, i)->parent) {
        const int next = dgx_ui_node(ui, i)->next;
        if (next >= 0) return next;
    }
    return -1;
}

bool dgx_ui_take(dgx_ui_t *ui, dgx_ui_msg_t *msg)
{
    msg->flags &= (uint8_t)~DGX_UI_MSG_KEPT;
    dgx_ui_msg_t *const outer = ui->offered; /* a take() that offers a message of its own to the tree */
    ui->offered               = msg;
    bool claimed              = false;
    for (int i = dgx_ui_widget(ui, 0) ? 0 : -1; i >= 0 && !claimed;) {
        const dgx_ui_widget_t *w     = dgx_ui_widget(ui, i);
        const int              after = next_past(ui, i); /* where the walk goes on if take() removes i */
        if (w->take) claimed = w->take(ui, i, msg) || (msg->flags & DGX_UI_MSG_KEPT);
        i = dgx_ui_widget(ui, i) == w ? next_in_order(ui, i) : after;
        if (i >= 0 && !dgx_ui_widget(ui, i)) break; /* take() removed that one too */
    }
    ui->offered = outer;
    if (!claimed && ui->popup) claimed = dgx_ui_take(ui->popup, msg); /* the popup's widgets too */
    return claimed;
}

/* ---- code points typed into the focused field ---- */

static int utf8_of(uint32_t c, char *out)
{
    if (c < 0x80) return out[0] = (char)c, 1;
    if (c < 0x800) return out[0] = (char)(0xC0 | c >> 6), out[1] = (char)(0x80 | (c & 0x3F)), 2;
    if (c < 0x10000) return out[0] = (char)(0xE0 | c >> 12), out[1] = (char)(0x80 | (c >> 6 & 0x3F)), out[2] = (char)(0x80 | (c & 0x3F)), 3;
    if (c < 0x110000)
        return out[0] = (char)(0xF0 | c >> 18), out[1] = (char)(0x80 | (c >> 12 & 0x3F)), out[2] = (char)(0x80 | (c >> 6 & 0x3F)), out[3] = (char)(0x80 | (c & 0x3F)), 4;
    return 0;
}

bool dgx_ui_typed(dgx_ui_t *ui, const uint32_t *code_points, int count)
{
    if (ui->popup && dgx_ui_typed(ui->popup, code_points, count)) return true; /* a field of the popup first */
    const dgx_ui_widget_t *w = ui->focus >= 0 ? dgx_ui_widget(ui, ui->focus) : NULL;
    if (!w || w->kind != DGX_UI_INPUT || !w->text || !w->text->buf || w->text->size < 1) return false;
    dgx_ui_textbox_t *box = w->text;
    const int         f   = ui->focus;
    dgx_ui_node_t    *n   = dgx_ui_node(ui, f);
    for (int k = 0; k < count; ++k) {
        const uint32_t c   = code_points[k];
        size_t         len = strlen(box->buf);
        if (c == DGX_UI_CP_ERASE) {
            if (!len) continue;
            do --len;
            while (len > 0 && ((unsigned char)box->buf[len] & 0xC0) == 0x80);
            box->buf[len] = 0;
        } else if (c == DGX_UI_CP_DONE || c == DGX_UI_CP_CANCEL) {
            if (w->on) w->on(ui, f, c == DGX_UI_CP_DONE ? DGX_UI_ACTIVATED : DGX_UI_BACK, w->ctx);
            if (dgx_ui_widget(ui, f) != w) return true; /* on() removed it */
            continue;
        } else {
            char      bytes[4];
            const int bytes_n = utf8_of(c, bytes);
            if (bytes_n == 0 || c < 0x20 || c == 0x7f || len + (size_t)bytes_n >= box->size) continue; /* a control, or no room */
            memcpy(box->buf + len, bytes, (size_t)bytes_n);
            box->buf[len + (size_t)bytes_n] = 0;
        }
        n->marks |= DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT;
        if (w->on) w->on(ui, f, DGX_UI_CHANGED, w->ctx); /* the user typed: the value changed */
        if (dgx_ui_widget(ui, f) != w) return true;
    }
    return true;
}
