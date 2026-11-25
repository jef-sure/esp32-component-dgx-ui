#pragma once
/*
 * dgx_ui_priv.h
 *
 *  What the library's own files share: the node of a number found with no checks, and the mark that tells the painter
 *  where to go down.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui.h"

/* the node of a number that holds one (0 .. count-1), with no checks, for the library's walks; dgx_ui_node() is the
   checked one. One function, not inline: inlined in every walk it costs more flash than the painter, which goes down
   only where something changed, saves in time */
dgx_ui_node_t *dgx_ui_at(const dgx_ui_t *ui, int i);

/* the marks that say a widget itself is to be drawn: any of them, or a state that differs from the state drawn */
#define DGX_UI_MARKS_TO_DRAW (DGX_UI_MARK_WHOLE | DGX_UI_MARK_AROUND | DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT | DGX_UI_MARK_TIME | DGX_UI_MARK_WINDOW)

/*
 * Widget i is to be drawn: a mark set, or its state changed. Every widget above it is marked DGX_UI_MARK_BELOW, so the
 * painter goes down to it and skips the rest. Called by every writer of a mark or of state.
 */
void dgx_ui_touched(dgx_ui_t *ui, int i);

/* what a widget shows of its place, the one question the painter's look at the places and its walk both ask, kept in
   state_drawn.value when it is drawn: a RADIO whether it is chosen, 1 or 0, so that a button the change of its group
   did not touch is neither marked nor drawn; any other its value. Inline in the look at the places, which asks it of
   every node at every pass, and always: a build for size calls a plain inline as a function. dgx_ui_shown() is the
   same as one function, for the painter's walk */
__attribute__((always_inline)) /* the row of a list or the option of a dropdown a value picks, from the first; -1 none, a value outside min..max,
   counted wide: the value is the program's, any int32_t */
static inline int dgx_ui_row_of(const dgx_ui_widget_t *w, int32_t value)
{
    const int64_t row = (int64_t)value - w->min;
    return row >= 0 && row <= (int64_t)w->max - w->min ? (int)row : -1;
}

static inline int32_t dgx_ui_shown_inline(const dgx_ui_node_t *n)
{
    return n->widget->kind == DGX_UI_RADIO ? dgx_ui_radio_chosen(n) : dgx_ui_value_of(n);
}
int32_t dgx_ui_shown(const dgx_ui_node_t *n);

/* the user's idle (dgx_ui_idle.c), named by dgx_ui_idle(): the input calls and dgx_ui_time() reach it only through this
   pointer, so a program that names none carries its tests alone. An input of a kind below, at ms (0: none known), the
   pointer's state for the finger, the pad's buttons held, which it may take away while they are spent; true: the
   input is spent. REPEAT_ENDED: a repeat of the tree ended, the idle's moment put back in its two members, nothing
   told */
enum { DGX_UI_INPUT_FINGER, DGX_UI_INPUT_BUTTONS, DGX_UI_INPUT_KEY, DGX_UI_INPUT_TYPED, DGX_UI_INPUT_REPEAT_ENDED };
extern bool (*dgx_ui_idle_hook)(dgx_ui_t *ui, int input, uint32_t ms, const dgx_ui_pointer_t *p, uint32_t *held);
/* the idle's step of dgx_ui_time(), named with the hook: of two arguments, as it is in every loop */
extern void (*dgx_ui_idle_step)(dgx_ui_t *ui, uint32_t now_ms);

/* repeat_key past the keys: the idle's moment (its time repeat_at, read by dgx_ui_wake() as a repeat's), the finger's
   repeat */
enum { DGX_UI_REPEAT_IDLE = 0x7e, DGX_UI_REPEAT_FINGER = 0x7f };

/* every widget whose place holds other than what was drawn marked: called by the painter before a pass */
void dgx_ui_places(dgx_ui_t *ui);

/*
 * The rows of a list shown, the one count the core's finger and the look both go by: all of them, sharing its box,
 * when they all go in at the larger of a line of its text and its step (points); else as many as go in, at least
 * one, the list scrolling. *rh: the height of each; *top: the first shown, its text box's top within the rows.
 */
int dgx_ui_list_rows(const dgx_ui_t *ui, const dgx_ui_node_t *n, int *rh, int *top);

/* a text set on a widget of the tree told to the widgets of the popups over it that show the same box (a dropdown's
   list): named by dgx_ui_dropdown_open(), so that a program of no dropdown carries none of it; NULL none */
extern void (*dgx_ui_text_shown_over)(dgx_ui_t *ui, const dgx_ui_textbox_t *box);
