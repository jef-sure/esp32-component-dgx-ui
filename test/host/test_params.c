/*
 * The descriptions of the parameters (dgx_ui_params.c) held against the structs they describe and against the format.
 * Each struct filled by its description, a distinct value in every field, is the struct written member by member by
 * hand: a member the description lacks, or one at the wrong place or size, makes them differ. The size and alignment
 * are the struct's. The names of the fields, which are the format, are written here a second time, so that a member
 * renamed in C changes the format only on purpose. A word's place is its constant's value, a flag's bit its constant.
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_appearance.h"

/* a struct filled by its description: field k takes 1 + k, a bool true, a bit its bits, a pointer the address of a marker */
static int marker;
static void fill(const dgx_ui_params_t *described, uint8_t *base)
{
    for (int k = 0; k < described->field_count; ++k) {
        const dgx_ui_field_t *field = &described->fields[k];
        const int32_t         value = field->kind == DGX_UI_FIELD_BIT ? field->high : field->kind == DGX_UI_FIELD_BOOL ? 1 : 1 + k;
        if (field->kind == DGX_UI_FIELD_BANDS || field->kind == DGX_UI_FIELD_VARIANTS || field->kind == DGX_UI_FIELD_FONT ||
            field->kind == DGX_UI_FIELD_HAND) {
            const void *pointer = &marker;
            memcpy(base + field->offset, &pointer, sizeof pointer);
            if (field->kind == DGX_UI_FIELD_BANDS || field->kind == DGX_UI_FIELD_VARIANTS) base[field->count_offset] = (uint8_t)(1 + k);
        } else if (field->size == 1) {
            base[field->offset] |= (uint8_t)value;
        } else if (field->size == 2) {
            const int16_t narrow = (int16_t)value;
            memcpy(base + field->offset, &narrow, 2);
        } else {
            int32_t wide;
            memcpy(&wide, base + field->offset, 4), wide |= value, memcpy(base + field->offset, &wide, 4);
        }
    }
}

/* the description's names in order, against the format's written here */
static bool named(const dgx_ui_params_t *described, const char *names)
{
    char said[512] = "";
    for (int k = 0; k < described->field_count; ++k) strcat(strcat(said, k ? " " : ""), described->fields[k].name);
    if (strcmp(said, names) != 0) fprintf(stderr, "  fields \"%s\", the format's \"%s\"\n", said, names);
    return strcmp(said, names) == 0;
}

#define HOLDS(described, type, names, ...)                                                                    \
    do {                                                                                                     \
        type filled, written;                                                                                \
        memset(&filled, 0, sizeof filled), memset(&written, 0, sizeof written);                              \
        fill(&(described), (uint8_t *)&filled);                                                              \
        written = (type)__VA_ARGS__;                                                                          \
        CHECK((described).size == sizeof(type) && (described).align == _Alignof(type));                       \
        CHECK(memcmp(&filled, &written, sizeof(type)) == 0);                                                   \
        CHECK(named(&(described), names));                                                                    \
    } while (0)

/* the place of a word in words */
static int place(const char *words, const char *word)
{
    for (int k = 0; *words; words += strlen(words) + 1, ++k)
        if (strcmp(words, word) == 0) return k;
    return -1;
}

int main(void)
{
    const void *const m = &marker;
    HOLDS(dgx_ui_params_frame, dgx_ui_frame_look_t, "border width", {.border = 1, .width = 2});
    HOLDS(dgx_ui_params_text, dgx_ui_text_look_t, "ink paper", {.ink = 1, .paper = 2});
    HOLDS(dgx_ui_params_lines, dgx_ui_text_look_t, "ink paper", {.ink = 1, .paper = 2});
    HOLDS(dgx_ui_params_hand_lines, dgx_ui_text_look_t, "ink paper", {.ink = 1, .paper = 2});
    HOLDS(dgx_ui_params_button, dgx_ui_button_look_t, "face pressed focused disabled text text_disabled shadow depth",
          {.face = 1, .pressed = 2, .focused = 3, .disabled = 4, .text = 5, .text_disabled = 6, .shadow = 7, .depth = 8});
    HOLDS(dgx_ui_params_picture_button, dgx_ui_button_look_t, "face pressed focused disabled text text_disabled shadow depth",
          {.face = 1, .pressed = 2, .focused = 3, .disabled = 4, .text = 5, .text_disabled = 6, .shadow = 7, .depth = 8});
    HOLDS(dgx_ui_params_input, dgx_ui_input_look_t, "ink caret paper secret", {.ink = 1, .caret = 2, .paper = 3, .secret = true});
    HOLDS(dgx_ui_params_switch, dgx_ui_switch_look_t, "off on knob focused disabled", {.off = 1, .on = 2, .knob = 3, .focused = 4, .disabled = 5});
    HOLDS(dgx_ui_params_slider, dgx_ui_slider_look_t, "track fill knob focused disabled track_h",
          {.track = 1, .fill = 2, .knob = 3, .focused = 4, .disabled = 5, .track_h = 6});
    HOLDS(dgx_ui_params_list, dgx_ui_list_look_t, "ink paper pick pick_ink focused disabled bar",
          {.ink = 1, .paper = 2, .pick = 3, .pick_ink = 4, .focused = 5, .disabled = 6, .bar = 7});
    HOLDS(dgx_ui_params_dropdown, dgx_ui_dropdown_look_t, "face pressed focused disabled text text_disabled arrow",
          {.face = 1, .pressed = 2, .focused = 3, .disabled = 4, .text = 5, .text_disabled = 6, .arrow = 7});
    HOLDS(dgx_ui_params_picture, dgx_ui_picture_look_t, "ink paper", {.ink = 1, .paper = 2});
    HOLDS(dgx_ui_params_rgb565, dgx_ui_picture_look_t, "ink paper", {.ink = 1, .paper = 2});
    HOLDS(dgx_ui_params_indexed, dgx_ui_picture_look_t, "ink paper", {.ink = 1, .paper = 2});
    HOLDS(dgx_ui_params_radio, dgx_ui_check_look_t, "mark focused disabled sign inside pressed ink ink_disabled paper",
          {.mark = 1, .focused = 2, .disabled = 3, .sign = 4, .inside = 5, .pressed = 6, .ink = 7, .ink_disabled = 8, .paper = 9});
    HOLDS(dgx_ui_params_checkbox, dgx_ui_check_look_t, "mark focused disabled sign inside pressed ink ink_disabled paper",
          {.mark = 1, .focused = 2, .disabled = 3, .sign = 4, .inside = 5, .pressed = 6, .ink = 7, .ink_disabled = 8, .paper = 9});
    HOLDS(dgx_ui_params_level, dgx_ui_level_look_t, "bar rest from bands_by bands",
          {.bar = 1, .rest = 2, .from = 3, .band_count = 5, .bands_by = 4, .bands = (const dgx_ui_band_t *)m});
    HOLDS(dgx_ui_params_ring, dgx_ui_ring_look_t, "on off paper width start sweep bands_by bands",
          {.on = 1, .off = 2, .paper = 3, .width = 4, .start = 5, .sweep = 6, .band_count = 8, .bands_by = 7, .bands = (const dgx_ui_band_t *)m});
    HOLDS(dgx_ui_params_text_style, dgx_ui_text_style_t, "fonts align flags scroll_ms",
          {.variants = (const dgx_ui_font_variant_t *)m, .count = 1, .flags = 3, .align = (dgx_font_align_t)2, .scroll_ms = 4});
    HOLDS(dgx_ui_params_font_variant, dgx_ui_font_variant_t, "font hand height narrow joined pen soft points",
          {.font = (const dgx_font_t *)m, .height = 3, .narrow = 4, .joined = true, .pen = 6 | DGX_PEN_SOFT, .hand = (const dgx_ui_hand_t *)m, .points = true});
    CHECK(dgx_ui_params_box.size == 0 && dgx_ui_params_box.field_count == 0);

    /* words and flags: a word's place its constant, a flag's bit its constant */
    const dgx_ui_field_t *from = &dgx_ui_params_level.fields[2], *by = &dgx_ui_params_level.fields[3], *ring_by = &dgx_ui_params_ring.fields[6];
    CHECK(place(from->words, "left") == DGX_UI_FROM_LEFT && place(from->words, "bottom") == DGX_UI_FROM_BOTTOM && place(from->words, "right") == DGX_UI_FROM_RIGHT &&
          place(from->words, "top") == DGX_UI_FROM_TOP);
    CHECK(place(by->words, "value") == DGX_UI_BANDS_BY_VALUE && place(by->words, "place") == DGX_UI_BANDS_BY_PLACE && ring_by->words == by->words);
    const dgx_ui_field_t *align = &dgx_ui_params_text_style.fields[1], *flags = &dgx_ui_params_text_style.fields[2];
    CHECK(place(align->words, "left") == DGX_FONT_ALIGN_LEFT && place(align->words, "center") == DGX_FONT_ALIGN_CENTER && place(align->words, "right") == DGX_FONT_ALIGN_RIGHT);
    CHECK(1 << place(flags->words, "middle") == DGX_UI_TEXT_MIDDLE && 1 << place(flags->words, "important") == DGX_UI_TEXT_IMPORTANT);
    CHECK(dgx_ui_params_font_variant.fields[6].high == (int32_t)DGX_PEN_SOFT);
    /* the kinds a draw draws, as the looks' header says */
    CHECK(dgx_ui_params_button.kinds == (1u << DGX_UI_BUTTON | 1u << DGX_UI_DROPDOWN) && dgx_ui_params_checkbox.kinds == 1u << DGX_UI_SWITCH &&
          dgx_ui_params_picture_button.kinds == (1u << DGX_UI_BUTTON | 1u << DGX_UI_SWITCH | 1u << DGX_UI_RADIO) && dgx_ui_params_ring.kinds == 1u << DGX_UI_OWN);
    CHECK_DONE();
}
