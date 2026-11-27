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
 *  And DGX-DI's keyboard on the display for a field (dgx_ui_di_keyboard_*): opened when the field is edited, its band
 *  kept from the painter, what it types typed into the field, closed when the field is done with.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui.h"

#if __has_include("dgx_di.h")
#include <string.h>

#include "dgx_di.h"
#include "dgx_di_keyboard.h"
#include "dgx_di_pad.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"

#ifdef __cplusplus
extern "C" {
#define DGX_UI_DI_ASSERT static_assert
#else
#define DGX_UI_DI_ASSERT _Static_assert
#endif

DGX_UI_DI_ASSERT((unsigned)DGX_UI_GAMEPAD_LEFT == DGX_DI_PAD_LEFT && (unsigned)DGX_UI_GAMEPAD_RIGHT == DGX_DI_PAD_RIGHT &&
                     (unsigned)DGX_UI_GAMEPAD_UP == DGX_DI_PAD_UP && (unsigned)DGX_UI_GAMEPAD_DOWN == DGX_DI_PAD_DOWN &&
                     (unsigned)DGX_UI_GAMEPAD_A == DGX_DI_PAD_A && (unsigned)DGX_UI_GAMEPAD_START == DGX_DI_PAD_START &&
                     (unsigned)DGX_UI_GAMEPAD_BACK == DGX_DI_PAD_BACK && (unsigned)DGX_UI_GAMEPAD_B == DGX_DI_PAD_B,
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

/**
 * @brief DGX-DI's keyboard on the display for the fields of the trees, a struct of the program's, one for all its
 * fields:
 *
 *     static dgx_ui_di_keyboard_t keys;
 *     static const dgx_ui_widget_t name_w = {.kind = DGX_UI_INPUT, .text = &name_box, .look = &dgx_ui_look_input,
 *                                            .on_event = dgx_ui_di_keyboard_on_event, .ctx = &keys};
 *     ...
 *     dgx_ui_di_keyboard_init(&keys, &paint, mm_w, mm_h, dgx_di_keyboard_latin, dgx_di_keyboard_latin_size);
 *     for (;;) {
 *         dgx_di_pointer_read(&pointer, now);
 *         dgx_ui_di_keyboard_pass(&keys, &pointer, now);          // its fingers first; true while it is open
 *         dgx_ui_di_pointer(&ui, &pointer, &touch, now, &gestures);
 *         ...
 *         dgx_ui_paint(&ui, &paint, now);
 *     }
 *
 * A field edited, DGX_UI_EVENT_EDIT (clicked, or the key of activation on it): its on_event() calls
 * dgx_ui_di_keyboard_edit(), or is dgx_ui_di_keyboard_on_event() with ctx the struct. The next pass opens the keyboard
 * away from the field, its labels in the field's font, and sets the paint's cover to its band; what it types goes to
 * the field (dgx_ui_typed()): done tells the field DGX_UI_EVENT_ACTIVATED, cancel DGX_UI_EVENT_BACK, and either closes
 * it. It closes too, the field told nothing and keeping what was typed, when the field is no longer edited: not its
 * tree's focus any more (moved, hidden, disabled, removed), a popup or a page over its tree, its tree a popup or a page
 * closed. Another field edited while it is open: the next pass moves it there. A keyboard that cannot open (the layout
 * unknown or past its limits, not fitting the display, a display not of 16 bits) cancels the edit: U+001B typed, the
 * field told DGX_UI_EVENT_BACK. The cover is the helper's while the keyboard is open and cleared when it closes: the
 * keyboard opens and closes only in its calls, so the cover they set is the one the next paint sees.
 *
 * The struct names a tree: close the keyboard, dgx_ui_di_keyboard_close(), before the field's tree is freed
 * (dgx_ui_free()); a page closed by dgx_ui_page_back() is not freed, and is seen. Not shown together with a pad of
 * DGX-DI: `open` says when the program hides its pad and brings it back. Called from the task that owns the display
 * and the trees.
 */
typedef struct dgx_ui_di_keyboard_t {
    /* the program's: set by dgx_ui_di_keyboard_init(); font may be set after it */
    dgx_ui_paint_t   *paint;               ///< its scr the display the keys are drawn on; its cover set and cleared
    int               width_mm, height_mm; ///< the glass, as DGX-DI sizes the keys
    const uint8_t    *layout;              ///< a layout of DGX-DI, a blob, and its size
    size_t            layout_size;
    const dgx_font_t *font;                ///< the labels' font; NULL: the field's
    /* the program's to read */
    bool              open;                ///< the keys are on the display
    dgx_ui_t         *ui;                  ///< the tree of the field edited, NULL none
    int               field;               ///< the field edited, -1 none
    dgx_di_keyboard_t kb;                  ///< DGX-DI's keyboard: its band kb.x, y, w, h while open
    /* the helper's */
    const dgx_ui_widget_t *widget;         ///< the field's widget at the opening
    dgx_ui_t              *under;          ///< what its tree lay over at the opening
    dgx_ui_t              *asked_ui;       ///< a field edited, opened by the next pass
    int                    asked;          ///< -1 none
    bool                   covering;       ///< the paint's cover is the band
} dgx_ui_di_keyboard_t;

/** @brief The helper set up: the paint of the display, the glass in millimetres, the layout; nothing open. */
static inline void dgx_ui_di_keyboard_init(dgx_ui_di_keyboard_t *k, dgx_ui_paint_t *paint, int width_mm, int height_mm, const uint8_t *layout, size_t size)
{
    memset(k, 0, sizeof *k);
    k->paint = paint, k->width_mm = width_mm, k->height_mm = height_mm, k->layout = layout, k->layout_size = size;
    k->field = -1, k->asked = -1;
}

/** @brief Field i of tree ui edited: called from its on_event() on DGX_UI_EVENT_EDIT; the next pass opens the keyboard. */
static inline void dgx_ui_di_keyboard_edit(dgx_ui_di_keyboard_t *k, dgx_ui_t *ui, int i)
{
    k->asked_ui = ui, k->asked = i;
}

/** @brief The on_event() of a field with nothing else to do, its ctx the struct: dgx_ui_di_keyboard_edit() on DGX_UI_EVENT_EDIT. */
static inline void dgx_ui_di_keyboard_on_event(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    (void)node;
    if (what == DGX_UI_EVENT_EDIT && ctx) dgx_ui_di_keyboard_edit((dgx_ui_di_keyboard_t *)ctx, ui, i);
}

/* the keyboard closed and its cover cleared; what was asked kept */
static inline void dgx_ui_di_keyboard_shut_(dgx_ui_di_keyboard_t *k)
{
    if (k->open) dgx_di_keyboard_close(&k->kb);
    if (k->covering) k->paint->cover = dgx_ui_rect(0, 0, 0, 0);
    k->open = false, k->covering = false, k->ui = NULL, k->field = -1, k->widget = NULL, k->under = NULL;
}

/** @brief The keyboard closed by the program, its cover cleared, a field asked for forgotten: before the field's tree is freed. */
static inline void dgx_ui_di_keyboard_close(dgx_ui_di_keyboard_t *k)
{
    dgx_ui_di_keyboard_shut_(k);
    k->asked_ui = NULL, k->asked = -1;
}

/* the labels' font for widget w: the variant its text is fitted in now, else the style's first that is a bitmap
   font (a handwritten one is drawn by strokes), else none */
static inline const dgx_font_t *dgx_ui_di_keyboard_font_(const dgx_ui_widget_t *w)
{
    const dgx_ui_text_style_t *st = w->text ? w->text->style : NULL;
    if (!st || !st->fonts) return NULL;
    if (w->text->variant < st->count && !st->fonts[w->text->variant].hand && st->fonts[w->text->variant].font) return st->fonts[w->text->variant].font;
    for (int v = 0; v < st->count; ++v)
        if (!st->fonts[v].hand && st->fonts[v].font) return st->fonts[v].font;
    return NULL;
}

/* field i of tree ui typed into by the keyboard: its tree's focus, nothing over the tree */
static inline bool dgx_ui_di_keyboard_typable_(const dgx_ui_t *ui, int i)
{
    const dgx_ui_widget_t *w = dgx_ui_widget(ui, i);
    return w && w->kind == DGX_UI_INPUT && ui->focus == i && !ui->popup;
}

/**
 * @brief Once a pass, the pointer read and before the tree gets it (dgx_ui_di_pointer()): the keyboard closed for a
 * field no more edited, opened for one edited (or moved to it), its fingers taken and what it typed typed into the
 * field, closed by done and cancel; the paint's cover its band while open. The field's tree is not freed while the
 * keyboard is open for it (dgx_ui_di_keyboard_close() first).
 * @return true while the keyboard is open.
 */
static inline bool dgx_ui_di_keyboard_pass(dgx_ui_di_keyboard_t *k, dgx_di_pointer_t *pointer, uint32_t now_ms)
{
    if (k->open && !(dgx_ui_di_keyboard_typable_(k->ui, k->field) && dgx_ui_widget(k->ui, k->field) == k->widget && k->ui->under == k->under))
        dgx_ui_di_keyboard_shut_(k); /* no more edited: the field keeps what was typed, told nothing */
    if (k->asked >= 0) {
        dgx_ui_t *const ui = k->asked_ui;
        const int       i  = k->asked;
        k->asked_ui = NULL, k->asked = -1;
        if (k->open && (ui != k->ui || i != k->field)) dgx_ui_di_keyboard_shut_(k); /* another field: moved there */
        if (!k->open && ui && dgx_ui_di_keyboard_typable_(ui, i)) {
            const dgx_ui_widget_t *w = dgx_ui_widget(ui, i);
            const dgx_ui_node_t   *n = dgx_ui_node(ui, i);
            dgx_di_rect_t          field;
            memset(&field, 0, sizeof field);
            field.x = (int16_t)(n->x - k->paint->ox), field.y = (int16_t)(n->y - k->paint->oy), field.w = n->w, field.h = n->h;
            if (dgx_di_keyboard_open(&k->kb, k->paint->scr, k->width_mm, k->height_mm, k->layout, k->layout_size, k->font ? k->font : dgx_ui_di_keyboard_font_(w), &field)) {
                k->open = true, k->ui = ui, k->field = i, k->widget = w, k->under = ui->under;
                k->paint->cover = dgx_ui_rect(k->kb.x, k->kb.y, k->kb.w, k->kb.h), k->covering = true;
            } else {
                const uint32_t cancel = DGX_DI_KEY_CANCEL; /* no keyboard: the edit cancelled, the field told DGX_UI_EVENT_BACK */
                dgx_ui_typed(ui, &cancel, 1);
            }
        }
    }
    if (k->open) {
        uint32_t  typed[DGX_DI_KEYBOARD_QUEUE];
        const int count = dgx_di_keyboard_take(&k->kb, pointer, now_ms, typed, DGX_DI_KEYBOARD_QUEUE);
        int       given = count; /* up to the first done or cancel: the field told holds what is typed, nothing after */
        bool      ended = false;
        for (int c = 0; c < count && !ended; ++c)
            if (typed[c] == DGX_DI_KEY_DONE || typed[c] == DGX_DI_KEY_CANCEL) ended = true, given = c + 1;
        if (given > 0) dgx_ui_typed(k->ui, typed, given);
        if (ended) dgx_ui_di_keyboard_shut_(k); /* done or cancel: the field told, the keyboard closed */
    }
    return k->open;
}

#ifdef __cplusplus
}
#endif
#endif
