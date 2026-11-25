/*
 * dgx_ui_params.c
 *
 *  The parameters of the library's draws described field by field (dgx_ui_appearance.h), and those of a text style
 *  and of a variant of a font: what an appearance's loader lays them by, what a builder shows. A description the
 *  program does not name is not carried.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_draw.h"
#include "dgx_ui_appearance.h"

#define COLOR(type, member)       DGX_UI_COLOR_FIELD(type, member)
#define UNDER(type, member)       DGX_UI_COLOR_OR_UNDER_FIELD(type, member)
#define BYTE(type, member)        DGX_UI_NUMBER_FIELD(type, member, 0, 255)

static const char sides[]    = "left\0bottom\0right\0top\0";
static const char bands_by[] = "value\0place\0";

static const dgx_ui_field_t frame_fields[]  = {COLOR(dgx_ui_frame_look_t, border), BYTE(dgx_ui_frame_look_t, width)};
static const dgx_ui_field_t text_fields[]   = {COLOR(dgx_ui_text_look_t, ink), UNDER(dgx_ui_text_look_t, paper)};
static const dgx_ui_field_t button_fields[] = {COLOR(dgx_ui_button_look_t, face),     COLOR(dgx_ui_button_look_t, pressed),
                                               UNDER(dgx_ui_button_look_t, focused),  COLOR(dgx_ui_button_look_t, disabled),
                                               COLOR(dgx_ui_button_look_t, ink),     COLOR(dgx_ui_button_look_t, ink_disabled),
                                               COLOR(dgx_ui_button_look_t, shadow),   BYTE(dgx_ui_button_look_t, depth)};
static const dgx_ui_field_t input_fields[]  = {COLOR(dgx_ui_input_look_t, ink), COLOR(dgx_ui_input_look_t, caret), UNDER(dgx_ui_input_look_t, paper),
                                               DGX_UI_BOOL_FIELD(dgx_ui_input_look_t, secret), BYTE(dgx_ui_input_look_t, blink)};
static const dgx_ui_field_t switch_fields[] = {COLOR(dgx_ui_switch_look_t, track), COLOR(dgx_ui_switch_look_t, fill), COLOR(dgx_ui_switch_look_t, knob),
                                               UNDER(dgx_ui_switch_look_t, focused), COLOR(dgx_ui_switch_look_t, disabled)};
static const dgx_ui_field_t slider_fields[] = {COLOR(dgx_ui_slider_look_t, track),   COLOR(dgx_ui_slider_look_t, fill),     COLOR(dgx_ui_slider_look_t, knob),
                                               UNDER(dgx_ui_slider_look_t, focused), COLOR(dgx_ui_slider_look_t, disabled), BYTE(dgx_ui_slider_look_t, track_h)};
static const dgx_ui_field_t list_fields[]   = {COLOR(dgx_ui_list_look_t, ink),     UNDER(dgx_ui_list_look_t, paper),    COLOR(dgx_ui_list_look_t, pick),
                                               COLOR(dgx_ui_list_look_t, pick_ink), COLOR(dgx_ui_list_look_t, focused), COLOR(dgx_ui_list_look_t, ink_disabled),
                                               COLOR(dgx_ui_list_look_t, knob)};
static const dgx_ui_field_t dropdown_fields[] = {COLOR(dgx_ui_dropdown_look_t, face),     COLOR(dgx_ui_dropdown_look_t, pressed),
                                                 UNDER(dgx_ui_dropdown_look_t, focused),  COLOR(dgx_ui_dropdown_look_t, disabled),
                                                 COLOR(dgx_ui_dropdown_look_t, ink),     COLOR(dgx_ui_dropdown_look_t, ink_disabled),
                                                 COLOR(dgx_ui_dropdown_look_t, arrow)};
static const dgx_ui_field_t picture_fields[] = {COLOR(dgx_ui_picture_look_t, ink), UNDER(dgx_ui_picture_look_t, paper)};
static const dgx_ui_field_t check_fields[]   = {COLOR(dgx_ui_check_look_t, mark),   UNDER(dgx_ui_check_look_t, focused), COLOR(dgx_ui_check_look_t, disabled),
                                                COLOR(dgx_ui_check_look_t, sign),   UNDER(dgx_ui_check_look_t, inside),  UNDER(dgx_ui_check_look_t, pressed),
                                                COLOR(dgx_ui_check_look_t, ink),    COLOR(dgx_ui_check_look_t, ink_disabled), UNDER(dgx_ui_check_look_t, paper)};
static const dgx_ui_field_t level_fields[]   = {COLOR(dgx_ui_level_look_t, fill), UNDER(dgx_ui_level_look_t, track), DGX_UI_WORD_FIELD(dgx_ui_level_look_t, from, sides),
                                                DGX_UI_WORD_FIELD(dgx_ui_level_look_t, bands_by, bands_by), DGX_UI_BANDS_FIELD(dgx_ui_level_look_t, bands, band_count)};
static const dgx_ui_field_t ring_fields[]    = {COLOR(dgx_ui_ring_look_t, fill),
                                                COLOR(dgx_ui_ring_look_t, track),
                                                UNDER(dgx_ui_ring_look_t, paper),
                                                BYTE(dgx_ui_ring_look_t, width),
                                                DGX_UI_NUMBER_FIELD(dgx_ui_ring_look_t, start, -360, 360),
                                                DGX_UI_NUMBER_FIELD(dgx_ui_ring_look_t, sweep, -360, 360),
                                                DGX_UI_WORD_FIELD(dgx_ui_ring_look_t, bands_by, bands_by),
                                                DGX_UI_BANDS_FIELD(dgx_ui_ring_look_t, bands, band_count)};

static const dgx_ui_field_t nine_fields[]    = {COLOR(dgx_ui_nine_look_t, fill), COLOR(dgx_ui_nine_look_t, ink),
                                                DGX_UI_PICTURE_FIELD(dgx_ui_nine_look_t, pieces, 9)};

const dgx_ui_params_t dgx_ui_params_box            = {.size = 0, .align = 1, .kinds = 1u << DGX_UI_BOX | 1u << DGX_UI_OWN, .field_count = 0, .fields = NULL};
const dgx_ui_params_t dgx_ui_params_frame          = DGX_UI_PARAMS(dgx_ui_frame_look_t, 1u << DGX_UI_FRAME | 1u << DGX_UI_BOX, frame_fields);
const dgx_ui_params_t dgx_ui_params_text           = DGX_UI_PARAMS(dgx_ui_text_look_t, 1u << DGX_UI_LABEL, text_fields);
const dgx_ui_params_t dgx_ui_params_lines          = DGX_UI_PARAMS(dgx_ui_text_look_t, 1u << DGX_UI_LABEL, text_fields);
const dgx_ui_params_t dgx_ui_params_hand_lines     = DGX_UI_PARAMS(dgx_ui_text_look_t, 1u << DGX_UI_LABEL, text_fields);
const dgx_ui_params_t dgx_ui_params_button         = DGX_UI_PARAMS(dgx_ui_button_look_t, 1u << DGX_UI_BUTTON | 1u << DGX_UI_DROPDOWN, button_fields);
const dgx_ui_params_t dgx_ui_params_picture_button = DGX_UI_PARAMS(dgx_ui_button_look_t, 1u << DGX_UI_BUTTON | 1u << DGX_UI_SWITCH | 1u << DGX_UI_RADIO, button_fields);
const dgx_ui_params_t dgx_ui_params_input          = DGX_UI_PARAMS(dgx_ui_input_look_t, 1u << DGX_UI_INPUT, input_fields);
const dgx_ui_params_t dgx_ui_params_switch         = DGX_UI_PARAMS(dgx_ui_switch_look_t, 1u << DGX_UI_SWITCH, switch_fields);
const dgx_ui_params_t dgx_ui_params_slider         = DGX_UI_PARAMS(dgx_ui_slider_look_t, 1u << DGX_UI_SLIDER, slider_fields);
const dgx_ui_params_t dgx_ui_params_list           = DGX_UI_PARAMS(dgx_ui_list_look_t, 1u << DGX_UI_LIST, list_fields);
const dgx_ui_params_t dgx_ui_params_dropdown       = DGX_UI_PARAMS(dgx_ui_dropdown_look_t, 1u << DGX_UI_DROPDOWN, dropdown_fields);
const dgx_ui_params_t dgx_ui_params_picture        = DGX_UI_PARAMS(dgx_ui_picture_look_t, 1u << DGX_UI_PICTURE | 1u << DGX_UI_ICON, picture_fields);
const dgx_ui_params_t dgx_ui_params_rgb565         = DGX_UI_PARAMS(dgx_ui_picture_look_t, 1u << DGX_UI_PICTURE | 1u << DGX_UI_ICON, picture_fields);
const dgx_ui_params_t dgx_ui_params_indexed        = DGX_UI_PARAMS(dgx_ui_picture_look_t, 1u << DGX_UI_PICTURE | 1u << DGX_UI_ICON, picture_fields);
const dgx_ui_params_t dgx_ui_params_radio          = DGX_UI_PARAMS(dgx_ui_check_look_t, 1u << DGX_UI_RADIO, check_fields);
const dgx_ui_params_t dgx_ui_params_checkbox       = DGX_UI_PARAMS(dgx_ui_check_look_t, 1u << DGX_UI_SWITCH, check_fields);
const dgx_ui_params_t dgx_ui_params_level          = DGX_UI_PARAMS(dgx_ui_level_look_t, 1u << DGX_UI_OWN, level_fields);
const dgx_ui_params_t dgx_ui_params_ring           = DGX_UI_PARAMS(dgx_ui_ring_look_t, 1u << DGX_UI_OWN, ring_fields);
const dgx_ui_params_t dgx_ui_params_nine           = DGX_UI_PARAMS(dgx_ui_nine_look_t, 1u << DGX_UI_BOX | 1u << DGX_UI_FRAME, nine_fields);

/* a style and a variant of a font: "fonts" the style's variants, "soft" the pen's DGX_PEN_SOFT, named by the format */
static const dgx_ui_field_t style_fields[] = {
    {.name = "fonts", .offset = offsetof(dgx_ui_text_style_t, fonts), .count_offset = offsetof(dgx_ui_text_style_t, count), .kind = DGX_UI_FIELD_VARIANTS,
     .size = sizeof(((dgx_ui_text_style_t *)0)->fonts), .low = 1, .high = 255, .words = NULL},
    DGX_UI_WORD_FIELD(dgx_ui_text_style_t, align, "left\0center\0right\0"),
    DGX_UI_FIELD_OF(dgx_ui_text_style_t, flags, DGX_UI_FIELD_FLAGS, 0, 0, "middle\0important\0"),
    DGX_UI_NUMBER_FIELD(dgx_ui_text_style_t, scroll_ms, 0, UINT16_MAX)};
static const dgx_ui_field_t variant_fields[] = {
    DGX_UI_FIELD_OF(dgx_ui_font_variant_t, font, DGX_UI_FIELD_FONT, 0, 0, NULL),
    DGX_UI_FIELD_OF(dgx_ui_font_variant_t, hand, DGX_UI_FIELD_HAND, 0, 0, NULL),
    DGX_UI_NUMBER_FIELD(dgx_ui_font_variant_t, height, 0, INT16_MAX),
    DGX_UI_NUMBER_FIELD(dgx_ui_font_variant_t, narrow, 0, 100),
    DGX_UI_BOOL_FIELD(dgx_ui_font_variant_t, joined),
    DGX_UI_NUMBER_FIELD(dgx_ui_font_variant_t, pen, 0, 255),
    {.name = "soft", .offset = offsetof(dgx_ui_font_variant_t, pen), .count_offset = 0, .kind = DGX_UI_FIELD_BIT, .size = sizeof(((dgx_ui_font_variant_t *)0)->pen),
     .low = 0, .high = (int32_t)DGX_PEN_SOFT, .words = NULL},
    DGX_UI_BOOL_FIELD(dgx_ui_font_variant_t, points)};
const dgx_ui_params_t dgx_ui_params_text_style   = DGX_UI_PARAMS(dgx_ui_text_style_t, 0, style_fields);
const dgx_ui_params_t dgx_ui_params_font_variant = DGX_UI_PARAMS(dgx_ui_font_variant_t, 0, variant_fields);
