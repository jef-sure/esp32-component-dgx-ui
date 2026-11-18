#pragma once
/*
 * dgx_ui_dropdown.h
 *
 *  The list of a dropdown opened as a popup. A dropdown (DGX_UI_DROPDOWN) is a button showing the option chosen: its
 *  options are the lines of its text box, as a list's ("Apple\nBanana\nCherry"), numbered min..max, its value the one
 *  chosen. Clicked, or DGX_UI_KEY_ACTIVATE while it has the focus, it is told DGX_UI_ACTIVATED; the program opens its
 *  list then, with one call:
 *
 *      static dgx_ui_dropdown_t dropdown; // one serves every dropdown of the program: one list is open at a time
 *
 *      static void on_fruit(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
 *      {
 *          if (what == DGX_UI_ACTIVATED) dgx_ui_dropdown_open(ui, i, &dropdown, NULL, NULL);
 *          if (what == DGX_UI_CHANGED) printf("the fruit: %d\n", node->state_value.value); // the user chose
 *      }
 *
 *  The list stands under the dropdown, as wide as it; above it when the screen has no room below; else moved up so
 *  that it fits the screen, lying over the dropdown. Its rows are as high as the dropdown, so a finger that hits the
 *  dropdown hits a row (never lower than a line of the text). More options than the screen takes rows: the list is as
 *  high as the screen allows and scrolls, its window at the option chosen.
 *
 *  The popup is modal and closed by a finger outside it: the keys stay in the list (NEXT and PREV move the pick,
 *  ACTIVATE chooses), a finger elsewhere closes it, and BACK is the program's, which closes it by dgx_ui_popup_close().
 *  A row chosen, by a click or by ACTIVATE, sets the dropdown's value and tells it DGX_UI_CHANGED, then the list
 *  closes; the option it had, chosen again, closes it and tells nothing; closed otherwise, the value stays and nothing
 *  is told. The list shows the dropdown's own text box: the program does not change the options while it is open.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui.h"
#include "dgx_ui_paint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The popup of a dropdown's list, in the program's RAM, zeroed before its first use (a static is). The popup
 * keeps pointers to its widgets while it is open, so they live here. 264 bytes on an ESP32.
 */
typedef struct dgx_ui_dropdown_t {
    dgx_ui_t        tree;     ///< the popup: a frame holding the list
    dgx_ui_node_t   nodes[2];
    dgx_ui_widget_t frame, list;
    dgx_ui_t       *ui;       ///< the tree of the dropdown whose list it shows
    int16_t         i;        ///< that dropdown
} dgx_ui_dropdown_t;

/**
 * @brief The list of dropdown i of a tree opened as a popup over it (the tree may be a popup itself).
 *
 * @param frame_look The popup's frame, its border one point; NULL: dgx_ui_look_frame. A look of a wider border is for
 *                   a popup the program builds itself.
 * @param list_look  The list; NULL: dgx_ui_look_list.
 * @return false, nothing opened: i is no dropdown, dd shows a list already, the screen is not known yet (the tree is
 * painted first), or the popup is refused (dgx_ui_popup_open()).
 */
bool dgx_ui_dropdown_open(dgx_ui_t *ui, int i, dgx_ui_dropdown_t *dd, const dgx_ui_look_t *frame_look, const dgx_ui_look_t *list_look);

#ifdef __cplusplus
}
#endif
