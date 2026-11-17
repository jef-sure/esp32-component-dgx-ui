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
#define DGX_UI_MARKS_TO_DRAW (DGX_UI_MARK_WHOLE | DGX_UI_MARK_AROUND | DGX_UI_MARK_CHANGED | DGX_UI_MARK_FIT | DGX_UI_MARK_TIME)

/*
 * Widget i is to be drawn: a mark set, or its state changed. Every widget above it is marked DGX_UI_MARK_BELOW, so the
 * painter goes down to it and skips the rest. Called by every writer of a mark or of state_value.
 */
void dgx_ui_touched(dgx_ui_t *ui, int i);
