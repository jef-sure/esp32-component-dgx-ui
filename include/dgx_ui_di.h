#pragma once
/*
 * dgx_ui_di.h
 *
 *  dgx-ui and the touch of DGX-DI (https://github.com/jef-sure/esp32-component-dgx-di): the first finger of its
 *  pointer as dgx-ui's pointer, and the gestures of its touch sorted between the widgets and the program. All of it
 *  static inline, and only when DGX-DI is there: dgx-ui does not depend on it.
 *
 *  Called by the task that owns the display, as DGX-DI is: the program reads the pointer (dgx_di_pointer_read()),
 *  lets its keyboard and its pad take their fingers, then gives the rest to this. A widget pressed at once, at an
 *  edge too. While a widget holds the finger, a plain swipe and a long press are its own; a swipe from an edge is
 *  the program's, unless the widget is dragged (a slider, a list, a button with DGX_UI_DRAG), which owns the finger.
 *  A swiped widget, from an edge too, stays pressed till the finger goes up, and is no click. A finger down on no
 *  widget gives the program all its gestures.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui.h"

#if __has_include("dgx_di.h")
#include "dgx_di.h"
#include "dgx_di_pad.h"

#ifdef __cplusplus
extern "C" {
#define DGX_UI_DI_ASSERT static_assert
#else
#define DGX_UI_DI_ASSERT _Static_assert
#endif

DGX_UI_DI_ASSERT((unsigned)DGX_UI_PAD_LEFT == DGX_DI_PAD_LEFT && (unsigned)DGX_UI_PAD_RIGHT == DGX_DI_PAD_RIGHT &&
                     (unsigned)DGX_UI_PAD_UP == DGX_DI_PAD_UP && (unsigned)DGX_UI_PAD_DOWN == DGX_DI_PAD_DOWN &&
                     (unsigned)DGX_UI_PAD_A == DGX_DI_PAD_A && (unsigned)DGX_UI_PAD_START == DGX_DI_PAD_START &&
                     (unsigned)DGX_UI_PAD_BACK == DGX_DI_PAD_BACK && (unsigned)DGX_UI_PAD_B == DGX_DI_PAD_B,
                 "the buttons of dgx-ui are those of DGX-DI's pad");

/** @brief The gestures of a touch that are the program's, since the last look. */
typedef struct dgx_ui_di_gestures_t {
    uint8_t edge;       ///< DGX_DI_EDGE_*: swiped from that edge
    uint8_t swipe;      ///< DGX_DI_SWIPE_*: swiped that way
    bool    long_press; ///< held long
    bool    taken;      ///< a widget took the finger: it went down on one
} dgx_ui_di_gestures_t;

/**
 * @brief A touch taken since the last look (dgx_di_touch_take()) at now_ms, the time it was taken at, given to the
 * tree; the gestures that are the program's in *g.
 */
static inline void dgx_ui_di_touch(dgx_ui_t *ui, const dgx_di_touch_t *touch, uint32_t now_ms, dgx_ui_di_gestures_t *g)
{
    const bool             was_holding = dgx_ui_holding(ui), was_dragging = dgx_ui_dragging(ui);
    const dgx_ui_pointer_t p           = {.x        = touch->x,
                                          .y        = touch->y,
                                          .down     = touch->down,
                                          .pressed  = touch->pressed,
                                          .released = touch->released,
                                          .swipe    = touch->edge || touch->swipe,
                                          .ms       = now_ms};
    const bool             took        = dgx_ui_pointer(ui, &p);
    const bool             holding     = was_holding || (touch->pressed && dgx_ui_holding(ui));
    const bool             dragging    = was_dragging || dgx_ui_dragging(ui);
    g->taken      = took && touch->pressed;
    g->edge       = dragging ? 0 : touch->edge;
    g->swipe      = holding ? 0 : touch->swipe;
    g->long_press = holding ? false : touch->long_press;
}

/**
 * @brief The first finger of a pointer read (and left by the keyboard and the pad) taken into a touch at now_ms and
 * given to the tree, as dgx_ui_di_touch().
 */
static inline void dgx_ui_di_pointer(dgx_ui_t *ui, const dgx_di_pointer_t *pointer, dgx_di_touch_t *touch, uint32_t now_ms, dgx_ui_di_gestures_t *g)
{
    int        x = 0, y = 0;
    const bool down = dgx_di_pointer_first(pointer, &x, &y);
    dgx_di_touch_take(touch, x, y, down, now_ms);
    dgx_ui_di_touch(ui, touch, now_ms, g);
}

#ifdef __cplusplus
}
#endif
#endif
