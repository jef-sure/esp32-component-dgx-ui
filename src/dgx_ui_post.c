/*
 * dgx_ui_post.c
 *
 *  Messages put into the program's queue, never waiting.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_post.h"
#include "esp_attr.h"

bool dgx_ui_post(QueueHandle_t q, int what, const void *data, size_t size)
{
    dgx_ui_msg_t msg;
    return dgx_ui_msg_value(&msg, what, data, size) && xQueueSend(q, &msg, 0) == pdTRUE;
}

bool dgx_ui_post_ref(QueueHandle_t q, int what, void *data, size_t size, void (*done)(void *data, size_t size))
{
    dgx_ui_msg_t msg;
    dgx_ui_msg_ref(&msg, what, data, size, done);
    return xQueueSend(q, &msg, 0) == pdTRUE;
}

bool dgx_ui_post_const(QueueHandle_t q, int what, const void *data, size_t size)
{
    return dgx_ui_post_ref(q, what, (void *)data, size, NULL); /* NULL done: the data is never written nor freed */
}

/* in IRAM, as an interrupt that runs with the flash cache off may call it; the linker drops it when it is unused.
   The message is filled here, not by dgx_ui_msg_value(), whose memcpy of the C library is not sure to lie in IRAM. */
bool IRAM_ATTR dgx_ui_post_from_isr(QueueHandle_t q, int what, const void *data, size_t size, BaseType_t *woken)
{
    if (size > DGX_UI_MSG_BYTES) return false;
    dgx_ui_msg_t msg = {.what = what, .count = (uint8_t)size};
    for (size_t k = 0; k < size; ++k) msg.bytes[k] = ((const uint8_t *)data)[k];
    return xQueueSendFromISR(q, &msg, woken) == pdTRUE;
}
