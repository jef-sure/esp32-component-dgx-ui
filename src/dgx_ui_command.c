/*
 * dgx_ui_command.c
 *
 *  The action command, DGX_UI_ACTION_NAME(command) and the button of DGX_UI_BUTTON(): a widget activated sends the
 *  number of its ctx as a message with no data to the queue the program named by dgx_ui_commands_to(), the queue of
 *  the task that owns the trees. The program's loop offers it to the tree after the input call that activated the
 *  widget, as every message: a take() may claim it, else the loop's own switch has it. The send never waits: a full
 *  queue, or none named, loses the command, told once in the log. A program that names neither the action nor the
 *  button carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#include <stdint.h>

#include "dgx_ui_paint.h"
#include "dgx_ui_post.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

static QueueHandle_t commands; /* the program's queue, named once */

void dgx_ui_commands_to(QueueHandle_t q)
{
    commands = q;
}

void dgx_ui_action_command(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    (void)ui, (void)node;
    if (what != DGX_UI_EVENT_ACTIVATED) return;
    const int command = (int)(intptr_t)ctx;
    if (!commands) {
        static bool told; /* once for the program */
        if (!told) ESP_LOGW(TAG, "widget %d: command %d, and no queue named by dgx_ui_commands_to(): lost", i, command);
        told = true;
        return;
    }
    if (!dgx_ui_post(commands, command, NULL, 0)) { /* no wait: the loop that takes the queue is this task's */
        static bool told;
        if (!told) ESP_LOGW(TAG, "widget %d: command %d lost, the queue is full", i, command);
        told = true;
    }
}
