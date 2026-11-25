/*
 * dgx_ui_load_actions.c
 *
 *  The two actions a description names with no code of the program's, DGX_UI_ACTION_NAME(back) and (page): back a
 *  page, and the page that is the button's ctx opened. Apart from the loader, which only lists them, so that what reads
 *  descriptions with no tree on a screen to page through, as dgx_ui_c on the computer, links no pages.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#include "dgx_ui.h"
#include "dgx_ui_load.h"

/* the tree's home, the bottom of its chain */
static dgx_ui_t *home_of(dgx_ui_t *ui)
{
    while (ui->under) ui = ui->under;
    return ui;
}

void dgx_ui_action_back(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_ACTIVATED) dgx_ui_page_back(home_of(ui));
}

void dgx_ui_action_page(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_ACTIVATED && ctx) dgx_ui_page_open(home_of(ui), (dgx_ui_t *)ctx);
}
