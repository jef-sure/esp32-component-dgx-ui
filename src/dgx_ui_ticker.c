/*
 * dgx_ui_ticker.c
 *
 *  Ticks as messages by reference to the ticker: the flag "on its way" is set before a tick is sent and cleared by
 *  its done() in the task that owns the tree, or at once when the queue is full. Set after the send, it could be
 *  cleared by a quick handler first and then set for ever: the ticks would stop.
 *
 *  On a chip the timer is an esp_timer: its task has a priority near the top, so no busy task of the program holds a
 *  tick back, and the finger is read on time. On the linux target of ESP-IDF, whose esp_timer is a header only, it
 *  is a software timer of FreeRTOS: its task has the priority CONFIG_FREERTOS_TIMER_TASK_PRIORITY, 1 by default.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_ticker.h"
#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_LINUX
#include "freertos/timers.h"
#else
#include "esp_timer.h"
#endif

static void tick_done(void *data, size_t size)
{
    (void)size;
    __atomic_store_n(&((dgx_ui_ticker_t *)data)->on_its_way, 0, __ATOMIC_RELEASE);
}

/* in the task of the timers: it never waits */
static void tick(dgx_ui_ticker_t *t)
{
    if (__atomic_exchange_n(&t->on_its_way, 1, __ATOMIC_ACQ_REL)) return; /* the one before is still on its way */
    if (!dgx_ui_post_ref(t->queue, t->what, t, 0, tick_done)) __atomic_store_n(&t->on_its_way, 0, __ATOMIC_RELEASE);
}

#if CONFIG_IDF_TARGET_LINUX

static void tick_freertos(TimerHandle_t timer)
{
    tick(pvTimerGetTimerID(timer));
}

bool dgx_ui_ticker_start(dgx_ui_ticker_t *ticker, QueueHandle_t queue, int what, uint32_t period_ms)
{
    if (ticker->timer) return false; /* runs already: stop it first */
    ticker->queue = queue, ticker->what = what, ticker->on_its_way = 0;
    const TickType_t period = pdMS_TO_TICKS(period_ms) > 0 ? pdMS_TO_TICKS(period_ms) : 1;
    TimerHandle_t    timer  = xTimerCreate("dgx_ui_ticker", period, pdTRUE, ticker, tick_freertos);
    if (timer && xTimerStart(timer, 0) == pdPASS) return ticker->timer = timer, true;
    if (timer) xTimerDelete(timer, 0);
    return false;
}

void dgx_ui_ticker_stop(dgx_ui_ticker_t *ticker)
{
    if (!ticker->timer) return;
    xTimerStop((TimerHandle_t)ticker->timer, portMAX_DELAY);
    xTimerDelete((TimerHandle_t)ticker->timer, portMAX_DELAY);
    ticker->timer = NULL;
}

#else

static void tick_esp(void *arg)
{
    tick(arg);
}

bool dgx_ui_ticker_start(dgx_ui_ticker_t *ticker, QueueHandle_t queue, int what, uint32_t period_ms)
{
    if (ticker->timer) return false; /* runs already: stop it first */
    ticker->queue = queue, ticker->what = what, ticker->on_its_way = 0;
    const esp_timer_create_args_t args  = {.callback = tick_esp, .arg = ticker, .name = "dgx_ui_ticker"};
    esp_timer_handle_t            timer = NULL;
    if (esp_timer_create(&args, &timer) != ESP_OK) return false;
    if (esp_timer_start_periodic(timer, (uint64_t)(period_ms > 0 ? period_ms : 1) * 1000u) != ESP_OK) {
        esp_timer_delete(timer);
        return false;
    }
    ticker->timer = timer;
    return true;
}

void dgx_ui_ticker_stop(dgx_ui_ticker_t *ticker)
{
    if (!ticker->timer) return;
    esp_timer_stop((esp_timer_handle_t)ticker->timer);
    esp_timer_delete((esp_timer_handle_t)ticker->timer);
    ticker->timer = NULL;
}

#endif
