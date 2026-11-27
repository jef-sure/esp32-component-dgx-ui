#pragma once
/*
 * dgx_ui_post.h
 *
 *  Messages sent to the task that owns a tree, through a queue of FreeRTOS that the program makes with items of
 *  sizeof(dgx_ui_msg_t). No call waits: a full queue loses the message, and the call says so.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_msg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief A message by value, up to 12 bytes, from any task; false: the data is larger, or the queue is full. */
bool dgx_ui_post(QueueHandle_t q, int what, const void *data, size_t size);

/**
 * @brief A message by reference: the message owns the data until its done() is called, in the task that owns the
 * tree. False: the queue is full; done() is not called then, the data is still the sender's.
 */
bool dgx_ui_post_ref(QueueHandle_t q, int what, void *data, size_t size, void (*done)(void *data, size_t size));

/** @brief A message by reference to data that lives for ever: nothing is done with it. */
bool dgx_ui_post_const(QueueHandle_t q, int what, const void *data, size_t size);

/**
 * @brief A message by value from an interrupt: as dgx_ui_post(); *woken as xQueueSendFromISR() sets it. It lies in
 * IRAM, so an interrupt of ESP_INTR_FLAG_IRAM may call it while the cache of the flash is off.
 */
bool dgx_ui_post_from_isr(QueueHandle_t q, int what, const void *data, size_t size, BaseType_t *woken);

/**
 * @brief The queue the commands of buttons go to (dgx_ui_action_command(), DGX_UI_BUTTON()): the queue of the task that
 * owns the trees, which its loop takes and offers to the tree. Named once; NULL: none, a command is lost and told once
 * in the log. One for the program: a program whose trees two tasks own gives the other task's buttons an on_event() of
 * their own.
 */
void dgx_ui_commands_to(QueueHandle_t q);

/**
 * @brief A message of a number with data of a type, by value: msg_post_<name>(q, &value) and
 * msg_decode_<name>(msg, &out). The type is bound to the number, and one larger than a message carries does not
 * compile.
 */
#ifdef __cplusplus
#define DGX_UI_STATIC_ASSERT static_assert
#else
#define DGX_UI_STATIC_ASSERT _Static_assert
#endif
#define DGX_UI_MESSAGE(name, number, type)                                                                             \
    DGX_UI_STATIC_ASSERT(sizeof(type) <= DGX_UI_MSG_BYTES, #type " is larger than a message carries by value");     \
    static inline bool msg_post_##name(QueueHandle_t q, const type *value)                                           \
    {                                                                                                                  \
        return dgx_ui_post(q, (number), value, sizeof(type));                                                         \
    }                                                                                                                  \
    DGX_UI_DECODER(name, number, type)

#ifdef __cplusplus
}
#endif
