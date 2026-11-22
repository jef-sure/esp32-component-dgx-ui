/*
 * dgx_ui_load_tables.h
 *
 *  The loader's tables of the format of a page: its kinds by their words, the words of "flags" and "state", the keys of
 *  a widget and the kinds each means something for. The library's, not a public header. Defined static here, so that
 *  the loader (dgx_ui_load.c) compiles them as it did and a reader of the format beside it, the preview of a
 *  description, takes the same text: one writing of the format, each its own copy.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#pragma once

#include <stdint.h>

#include "dgx_ui.h"

static const char *const kinds[] = {"box", "frame", "label", "picture", "icon", "list", "switch", "slider", "button", "input", "dropdown", "own", "radio"};
enum { KIND_COUNT = sizeof kinds / sizeof *kinds };

/* the words of "flags" and "state", each at its bit's place: DGX_UI_COLUMN is 1 << 0 ... DGX_UI_REPEAT 1 << 7 */
static const char flag_words[]  = "column\0row\0stretch\0swipe\0drag\0canvas\0rest\0repeat\0";
static const char state_words[] = "hidden\0disabled\0";
_Static_assert(DGX_UI_COLUMN == 1 << 0 && DGX_UI_ROW == 1 << 1 && DGX_UI_STRETCH == 1 << 2 && DGX_UI_SWIPE == 1 << 3 && DGX_UI_DRAG == 1 << 4 &&
                   DGX_UI_CANVAS == 1 << 5 && DGX_UI_REST == 1 << 6 && DGX_UI_REPEAT == 1 << 7 && DGX_UI_HIDDEN == 1 << 0 && DGX_UI_DISABLED == 1 << 1,
               "a word's bit is 1 << its place");

/* the keys of a widget, and the kinds each means something for */
enum { K_KIND, K_FLAGS, K_STATE, K_PAD, K_GAP, K_AT, K_WANT, K_MIN, K_MAX, K_STEP, K_TAG, K_LOOK, K_VALUE, K_PICTURE, K_STYLE, K_ON, K_TAKE, K_CTX, K_TEXT, K_TEXT_SIZE, K_LINES, K_ID, K_CHILDREN, K_EDITOR, KEY_COUNT };
static const char *const keys[KEY_COUNT] = {"kind", "flags", "state", "pad", "gap", "at", "want", "min", "max", "step", "tag", "look", "value", "picture", "style", "on", "take", "ctx", "text", "text_size", "lines", "id", "children", "editor"};
enum {
    ALL       = 0x1fff,
    VALUED    = 1u << DGX_UI_LIST | 1u << DGX_UI_SWITCH | 1u << DGX_UI_SLIDER | 1u << DGX_UI_DROPDOWN | 1u << DGX_UI_OWN | 1u << DGX_UI_RADIO | 1u << DGX_UI_ICON,
    TEXTED    = 1u << DGX_UI_LABEL | 1u << DGX_UI_BUTTON | 1u << DGX_UI_LIST | 1u << DGX_UI_INPUT | 1u << DGX_UI_DROPDOWN | 1u << DGX_UI_RADIO | 1u << DGX_UI_SWITCH,
    PICTURED  = 1u << DGX_UI_PICTURE | 1u << DGX_UI_ICON | 1u << DGX_UI_BUTTON | 1u << DGX_UI_SWITCH | 1u << DGX_UI_RADIO,
    HOLDING   = 1u << DGX_UI_BOX | 1u << DGX_UI_FRAME | 1u << DGX_UI_OWN,
    OWN_LOOKS = 1u << DGX_UI_BOX | 1u << DGX_UI_FRAME | 1u << DGX_UI_LABEL, /* the core's own look when none is said */
};
static const uint16_t key_kinds[KEY_COUNT] = {ALL,    ALL,    ALL,    HOLDING, HOLDING,  ALL,    ALL,    VALUED, VALUED, VALUED, ALL, ALL,
                                              VALUED, PICTURED, TEXTED, ALL,   ALL,      ALL,    TEXTED, TEXTED, TEXTED, ALL, HOLDING, ALL};
