/*
 * dgx_ui_page.c
 *
 *  Pages: popups of DGX_UI_POPUP_MODAL | DGX_UI_POPUP_BACK | DGX_UI_POPUP_SCREEN, a screen of its own over the one
 *  under it, or of DGX_UI_POPUP_PLACE in place of the screen while home names a place (dgx_ui_page_place()), closed by
 *  the key back (the core's) or by dgx_ui_page_back(). A program that calls neither carries none of this.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui.h"

enum { PAGE_FLAGS = DGX_UI_POPUP_MODAL | DGX_UI_POPUP_BACK };

/* the popup over this tree is a page: closed by back, of the screen or of the place */
static bool page_over(const dgx_ui_t *tree)
{
    return tree->popup && (tree->popup_flags & DGX_UI_POPUP_BACK) && (tree->popup_flags & (DGX_UI_POPUP_SCREEN | DGX_UI_POPUP_PLACE));
}

/* the top page, or home */
static dgx_ui_t *top_page(dgx_ui_t *home)
{
    dgx_ui_t *base = home;
    for (dgx_ui_t *tree = home; tree->popup; tree = tree->popup)
        if (page_over(tree)) base = tree->popup;
    return base;
}

bool dgx_ui_page_open(dgx_ui_t *home, dgx_ui_t *page)
{
    /* over the top page, from the top down, the popups a finger elsewhere would close: closed, to the first that is
       not. The top page found again after each: a popup's on(DGX_UI_COVERED) may have gone back a page */
    for (dgx_ui_t *base = top_page(home); base->popup; base = top_page(home)) {
        dgx_ui_t *under_top = base;
        while (under_top->popup->popup) under_top = under_top->popup;
        if (!(under_top->popup_flags & DGX_UI_POPUP_CLOSE_OUTSIDE)) break;
        dgx_ui_popup_close(under_top);
    }
    dgx_ui_t *top = home; /* the top of the chain as it stands now */
    while (top->popup) top = top->popup;
    return dgx_ui_popup_open(top, page, 0, 0, 0, 0, PAGE_FLAGS | (home->page_place ? DGX_UI_POPUP_PLACE : DGX_UI_POPUP_SCREEN));
}

bool dgx_ui_page_back(dgx_ui_t *home)
{
    dgx_ui_t *under = NULL; /* the tree the top page lies over */
    for (dgx_ui_t *tree = home; tree->popup; tree = tree->popup)
        if (page_over(tree)) under = tree;
    return under && dgx_ui_popup_back(under);
}
