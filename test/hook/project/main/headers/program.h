/* the program's objects the names of vocab.txt lead to */
#pragma once

#include <stdint.h>

#include "dgx_ui.h"
#include "dgx_ui_text.h"

extern int32_t             volume, muted;
extern dgx_ui_text_style_t text_style;
extern int                 title_index;

void on_saved(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
