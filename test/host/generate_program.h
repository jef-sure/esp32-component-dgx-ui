/*
 * The program the files of dgx_ui_c are made for in test_generate: its objects, each named as the names file and the
 * rule of dgx_ui_c name them, so that both ways lead to the same ones, and its ints the ids are written to.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dgx_ui.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"

extern int32_t                volume, choice, ticked, picked, other;
extern const dgx_ui_picture_t dot[1], dots[2];
extern dgx_ui_text_style_t    text, small;
extern const dgx_ui_look_t    mine; /* a look of the program's own */
extern dgx_ui_t               settings;
extern int                    title_index, slider_index, less_index, go_index;

void saved(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
void step(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
bool take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg);
