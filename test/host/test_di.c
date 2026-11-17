/*
 * The touch of DGX-DI given to dgx-ui (dgx_ui_di.h), DGX-DI's own touch working out the gestures: a slider at the
 * left edge dragged owns the finger, its swipe from the edge not the program's; a button at the edge gives it, and is
 * no click; a finger on no widget gives all; a long press on a button is the button's, and no click; a short tap
 * clicks it. And a field typed into: every pixel once, the caret only while it is focused.
 */
#include <string.h>

#include "check.h"
#include "dgx_ui_di.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

/* a column on a display of 96x64 pixels, 48x32 mm: a slider and a button from the left edge, paper under them */
static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .want_h = 10, .max = 100, .look = &dgx_ui_look_slider};
static int                   clicks;
static void                  on_button(dgx_ui_t *u, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    clicks += what == DGX_UI_ACTIVATED;
}
static const dgx_ui_widget_t button_w = {.kind = DGX_UI_BUTTON, .want_h = 10, .look = &dgx_ui_look_button, .on = on_button};
static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static char                  field_buf[16];
static dgx_ui_textbox_t      field_box;
static const dgx_ui_widget_t field_w = {.kind = DGX_UI_INPUT, .text = &field_box, .want_h = 14, .look = &dgx_ui_look_input};
static dgx_ui_node_t         nodes[6];
static dgx_ui_t              ui;
static int                   sl, bt, fi;

/* a finger: down at x, y at t, moved to x2, y2 by steps, held, let go */
static dgx_ui_di_gestures_t all;
static void take(dgx_di_touch_t *touch, int x, int y, bool down, uint32_t t)
{
    dgx_ui_di_gestures_t g;
    dgx_di_touch_take(touch, x, y, down, t);
    dgx_ui_di_touch(&ui, touch, t, &g);
    all.edge |= g.edge, all.swipe |= g.swipe, all.long_press |= g.long_press, all.taken |= g.taken;
}
static void stroke(dgx_di_touch_t *touch, int x, int y, int x2, int y2, uint32_t t, uint32_t hold)
{
    all = (dgx_ui_di_gestures_t){0};
    take(touch, x, y, true, t);
    for (int k = 1; k <= 8; ++k) take(touch, x + (x2 - x) * k / 8, y + (y2 - y) * k / 8, true, t + 10u * k);
    if (hold) take(touch, x2, y2, true, t + 80 + hold);
    take(touch, x2, y2, false, t + 100 + hold);
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    style      = (dgx_ui_text_style_t){.variants = variant, .count = 1};
    dgx_ui_textbox_init(&field_box, field_buf, sizeof field_buf, 1, &style);
    panel_t       *panel = panel_make();
    dgx_screen_t  *scr   = &panel->sbus.scr;
    dgx_ui_paint_t pp    = {.scr = scr, .colors = colors, .color_count = 4};
    dgx_ui_init(&ui, nodes, 6, 1);
    dgx_ui_add(&ui, -1, &root_w);
    sl = dgx_ui_add(&ui, 0, &slider_w);
    bt = dgx_ui_add(&ui, 0, &button_w);
    fi = dgx_ui_add(&ui, 0, &field_w);
    dgx_ui_paint(&ui, &pp, 0);
    dgx_di_touch_t touch = {0};
    dgx_di_touch_display(&touch, scr, 48, 32); /* 2 pixels a mm: the band of an edge 18 pixels, a swipe 30 */

    /* the slider at the left edge, dragged right 40 pixels: it owns the finger, no swipe from the edge */
    stroke(&touch, 2, 5, 42, 5, 1000, 0);
    CHECK(all.taken && all.edge == 0 && dgx_ui_node(&ui, sl)->state_value.value > 0);
    /* the button at the left edge, the same way, in 180 ms: the swipe from the edge is the program's, no plain
       swipe; the button pressed till the finger went up, no click */
    stroke(&touch, 2, 15, 42, 15, 2000, 0);
    CHECK(all.taken && all.edge == DGX_DI_EDGE_LEFT && all.swipe == 0 && clicks == 0);
    /* a plain swipe begun on the button, away from the edges: the button's, no click */
    stroke(&touch, 30, 15, 70, 15, 2500, 0);
    CHECK(all.taken && all.edge == 0 && all.swipe == 0 && clicks == 0);
    /* a finger on no widget, below them, from the left edge: all its gestures the program's */
    stroke(&touch, 2, 55, 42, 55, 3000, 0);
    CHECK(!all.taken && all.edge == DGX_DI_EDGE_LEFT);
    /* a long press on the button, away from the edges: the button's, no click */
    stroke(&touch, 40, 15, 40, 15, 4000, 700);
    CHECK(all.taken && !all.long_press && clicks == 0);
    /* a tap on it, 100 ms: a click */
    stroke(&touch, 40, 15, 40, 15, 4500, 0);
    CHECK(all.taken && clicks == 1);
    /* a long press on no widget, out of the bands of the edges (18 pixels): the program's */
    stroke(&touch, 40, 40, 40, 40, 5000, 700);
    CHECK(!all.taken && all.long_press);

    /* the field: typed into while focused, every pixel of its box once, the caret after the text */
    dgx_ui_focus(&ui, fi);
    dgx_ui_paint(&ui, &pp, 6000);
    const uint32_t ab[] = {'a', 'b'};
    panel_count_from_now(panel);
    CHECK(dgx_ui_typed(&ui, ab, 2));
    dgx_ui_paint(&ui, &pp, 6001);
    const dgx_ui_node_t *n = dgx_ui_node(&ui, fi);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == n->w * n->h);
    const dgx_font_context_t m  = {.font = TerminusTTFMedium12()};
    const int                cx = n->x + dgx_font_bounds(&m, "ab", NULL, NULL);
    const int                cy = n->y - TerminusTTFMedium12()->yOffsetLowest - 2;
    CHECK(panel->mem[cy][cx] == colors[3]);
    dgx_ui_focus(&ui, -1);
    dgx_ui_paint(&ui, &pp, 6002);
    CHECK(panel->mem[cy][cx] == colors[0]);

    dgx_ui_free(&ui);
    free(panel);
    CHECK_DONE();
}
