#pragma once
/*
 * dgx_ui_ticker.h
 *
 *  A ticker: a message of a number sent every so many ms by a timer, at most one of them in the queue. While the task that owns the tree is busy, the ticks do not pile up there: the next is sent only when
 *  the one before has been done with (dgx_ui_msg_done()). It wakes the task for what only it may do, as reading the
 *  touch of DGX-DI, or for something slow that changes with time. On a chip the timer is an esp_timer, whose task
 *  no busy task of the program holds back; on the linux target of ESP-IDF, where esp_timer is a header only, a
 *  software timer of FreeRTOS.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_post.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief A ticker: the program's, made by dgx_ui_ticker_start(). */
typedef struct dgx_ui_ticker_t {
    void            *timer;      ///< the timer of the target: an esp_timer, or a timer of FreeRTOS on linux
    QueueHandle_t    queue;
    int              what;
    volatile uint8_t on_its_way; ///< a tick is in the queue or being handled: set and cleared atomically
} dgx_ui_ticker_t;

/**
 * @brief Ticks of a number into a queue every so many ms, from now on. The struct is zeroed before the first start
 * (a static one is).
 * @return false: no timer could be made or started (none is left made), or the ticker runs already.
 */
bool dgx_ui_ticker_start(dgx_ui_ticker_t *ticker, QueueHandle_t queue, int what, uint32_t period_ms);

/**
 * @brief No more ticks; the timer given back. A tick still in the queue is done with as any message, and its done()
 * writes into the ticker; the timer's task may even send one more meanwhile. So the struct must live until
 * on_its_way is 0 after this: a static one always does.
 */
void dgx_ui_ticker_stop(dgx_ui_ticker_t *ticker);

#ifdef __cplusplus
}
#endif
