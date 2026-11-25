#pragma once
/*
 * dgx_ui_scroll_priv.h
 *
 *  What the behaviour of a scrolling box (dgx_ui_scroll.c) and its look (dgx_ui_scroll_look.c) share: its parts as
 *  the layout laid them, its parameters, its bar's parts.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_paint.h"
#include "dgx_ui_priv.h"

/* the parts of a box */
typedef struct {
    dgx_ui_rect_t rows;         /* the box's width by its inside's rows: what a move moves */
    dgx_ui_rect_t column;       /* the children's clip: the rows less the bar's column */
    int           bar_x, bar;   /* the bar's column: its left, its width; 0 none */
    int           content;      /* the children's height */
    int           offset, most; /* where they are laid, and the most it may be */
} dgx_ui_scroll_box_t;

/* the library's parameters: track 3, knob 1, arrows 1, a bar of 5 points */
extern const dgx_ui_scroll_look_t dgx_ui_scroll_default;

/* a box's parameters: its look's, else the library's */
const dgx_ui_scroll_look_t *dgx_ui_scroll_params(const dgx_ui_node_t *n);

/* the parts as the layout laid them: the content and the offset from where the children lie */
void dgx_ui_scroll_measure(const dgx_ui_t *ui, int i, dgx_ui_scroll_box_t *b);

/* the bar's parts along the view: the arrows' height (0 none), the knob's place from the top of the rows and its length */
void dgx_ui_scroll_bar(const dgx_ui_scroll_look_t *p, const dgx_ui_scroll_box_t *b, int *ah, int *ky, int *kh);
