#pragma once
/*
 * dgx_ui_msg.h
 *
 *  The messages of dgx-ui: what other tasks send the task that owns a tree. A message is a number of the program's
 *  (dgx-ui reserves none and never looks inside) and up to 12 bytes by value, or data by reference that the message
 *  owns until it is done with. Every widget is offered a message and catches its own values from it; one may claim
 *  it, and the walk stops. Only decoders give a message its meaning. Sending is dgx_ui_post.h, through a queue of
 *  FreeRTOS; this part has no FreeRTOS, so it runs in host tests.
 *
 *  The loop of the task that owns the tree, for each message it takes from its queue:
 *
 *      if (!dgx_ui_take(&ui, &msg)) { ... no widget took it: the program's ... }
 *      dgx_ui_msg_done(&msg);   // once, at the end, whatever handled it
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "dgx_ui.h"
#include "esp_err.h"

#ifndef ESP_ERR_INVALID_SIZE /* ESP-IDF's, missing from the stubs of DGX's host build */
#define ESP_ERR_INVALID_SIZE 0x104
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Bytes a message carries by value. */
enum { DGX_UI_MSG_BYTES = 12 };

/** Flags of a message. */
enum {
    DGX_UI_MSG_REF   = 1u << 0, ///< its data by reference, ref; else count bytes by value
    DGX_UI_MSG_KEPT  = 1u << 1, ///< a widget keeps its data past the walk: done with by the widget's copy, not by this one
};

/**
 * @brief A message: its number, and its data by value or by reference. 20 bytes on a 32-bit chip.
 */
struct dgx_ui_msg_t {
    int     what;  ///< the program's number
    uint8_t flags; ///< DGX_UI_MSG_REF, DGX_UI_MSG_KEPT
    uint8_t count; ///< by value: the bytes used
    union {
        uint8_t bytes[DGX_UI_MSG_BYTES]; ///< by value
        struct {
            void *data;
            size_t size;
            void (*done)(void *data, size_t size); ///< called once when the message is done with; NULL: the data lives for ever
        } ref; ///< by reference
    };
};

#if UINTPTR_MAX == 0xFFFFFFFFu
#ifdef __cplusplus
static_assert(sizeof(dgx_ui_msg_t) == 20, "a message is 20 bytes on a 32-bit chip: the item of the program's queue");
#else
_Static_assert(sizeof(dgx_ui_msg_t) == 20, "a message is 20 bytes on a 32-bit chip: the item of the program's queue");
#endif
#endif

/** @brief A message by value, filled: its number and up to 12 bytes; false when the data is larger. */
static inline bool dgx_ui_msg_value(dgx_ui_msg_t *msg, int what, const void *data, size_t size)
{
    if (size > DGX_UI_MSG_BYTES) return false;
    memset(msg, 0, sizeof(*msg));
    msg->what  = what;
    msg->count = (uint8_t)size;
    if (size) memcpy(msg->bytes, data, size);
    return true;
}

/** @brief A message by reference, filled: the message owns the data until done() is called. */
static inline void dgx_ui_msg_ref(dgx_ui_msg_t *msg, int what, void *data, size_t size, void (*done)(void *, size_t))
{
    memset(msg, 0, sizeof(*msg));
    msg->what     = what;
    msg->flags    = DGX_UI_MSG_REF;
    msg->ref.data = data, msg->ref.size = size, msg->ref.done = done;
}

/**
 * @brief The data of a message of a number, copied out.
 * @return ESP_OK; ESP_ERR_NOT_FOUND for a message of another number; ESP_ERR_INVALID_SIZE for this number with data of
 *         another size (logged once).
 */
esp_err_t dgx_ui_msg_copy(const dgx_ui_msg_t *msg, int what, void *out, size_t size);

/** @brief The data of a message and its size, where they lie: valid until the message is done with. */
const void *dgx_ui_msg_data(const dgx_ui_msg_t *msg, size_t *size);

/**
 * @brief A message offered to a tree: to every widget in its order, the hidden ones too, until one claims it.
 *
 * Nothing is released: the program may still look at the data, and calls dgx_ui_msg_done() once at the end.
 * Called by the task that owns the tree, for each message it takes from its queue.
 * @return true when a widget claimed it; false: it is the program's.
 */
bool dgx_ui_take(dgx_ui_t *ui, dgx_ui_msg_t *msg);

/**
 * @brief Called in a widget's take(): the data of the message offered now is the widget's past the walk.
 *
 * The message is copied into copy, and the one offered is marked so that its dgx_ui_msg_done() does nothing; the
 * widget calls dgx_ui_msg_done() on its copy when it is done with the data. Keeping claims the message, so one
 * widget keeps a message at most: called a second time, or outside a take(), it gives copy a message of no data. A
 * message offered to a second tree after a widget of the first kept it (two displays) is offered there as any other:
 * read, not kept, and the loop's dgx_ui_msg_done() still does nothing.
 */
void dgx_ui_msg_keep(dgx_ui_t *ui, dgx_ui_msg_t *copy);

/** @brief A message done with: its done() called once for data by reference; nothing for one by value or one kept. */
void dgx_ui_msg_done(dgx_ui_msg_t *msg);

/** @brief A decoder of messages of a number with data of a type: `esp_err_t msg_decode_<name>(msg, type *out)`. */
#define DGX_UI_DECODER(name, number, type)                                                                             \
    static inline esp_err_t msg_decode_##name(const dgx_ui_msg_t *msg, type *out)                                    \
    {                                                                                                                  \
        return dgx_ui_msg_copy(msg, (number), out, sizeof(type));                                                     \
    }

/*
 * Code points typed: by value, up to three, as DGX-DI's keyboard gives them: characters, U+0008 erase, U+000A done,
 * U+001B cancel.
 */

/** Code points of editing. */
enum { DGX_UI_CP_ERASE = 0x08, DGX_UI_CP_DONE = 0x0a, DGX_UI_CP_CANCEL = 0x1b };

/**
 * @brief Code points typed, to the focused field: characters go into its text, erase takes the last letter off, done
 * tells it DGX_UI_EVENT_ACTIVATED and cancel DGX_UI_EVENT_BACK.
 * @return true when a field took them.
 */
bool dgx_ui_typed(dgx_ui_t *ui, const uint32_t *code_points, int count);

#ifdef __cplusplus
}
#endif
