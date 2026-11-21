/* the program's objects a names file of test_generate_appearance.py names, for the files dgx_ui_c makes of it */
#pragma once

#include "dgx_ui_appearance.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"

extern int32_t             volume;
extern const dgx_ui_look_t mine;
extern dgx_ui_text_style_t plain;
bool                       draw_dial(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
dgx_font_t                *dgx_font_small(void);
dgx_font_t                *dgx_font_hand(void);
