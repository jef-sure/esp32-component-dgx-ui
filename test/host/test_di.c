/*
 * The touch of DGX-DI given to dgx-ui (dgx_ui_di.h), DGX-DI's own touch working out the gestures: a slider at the
 * left edge dragged owns the finger, its swipe from the edge not the program's; a button at the edge gives it, and is
 * no click; a finger on no widget gives all; a long press on a button is the button's, and no click; a short tap
 * clicks it. And a field typed into: every pixel once, the caret only while it is focused. And the pointer of DGX-DI
 * read from a device, its first finger given by dgx_ui_di_pointer(): a tap on the button clicks it; let go off it
 * after a short move it is still clicked, swiped off it it is not; a widget of the program's own let go is told the
 * place dgx_di_pointer_first() gives for a lifted finger, where it was last down; a finger the device loses for less
 * than the pointer's up stays down; a second finger is not the first.
 */
#include <string.h>

#include "check.h"
#include "dgx_ui_di.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "dgx_draw.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

/* a column on a display of 96x64 pixels, 48x32 mm: a slider and a button from the left edge, paper under them */
static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
static int32_t slider_w_place; /* its value */
static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .value = &slider_w_place, .want_h = 10, .max = 100, .look = &dgx_ui_look_slider};
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
/* a field of a text in its middle, taller than the text */
static dgx_ui_text_style_t   middle;
static char                  mid_buf[8];
static dgx_ui_textbox_t      mid_box;
static const dgx_ui_widget_t mid_w = {.kind = DGX_UI_INPUT, .text = &mid_box, .want_h = 22, .look = &dgx_ui_look_input};
/* a secret field: a star for each letter */
static const dgx_ui_input_look_t secret_colours = {.ink = 1, .caret = 3, .paper = 0, .secret = true};
static const dgx_ui_look_t       secret_look    = {.draw = dgx_ui_draw_input, .params = &secret_colours, .fill = -1};
static char                      pin_buf[8];
static dgx_ui_textbox_t          pin_box;
static const dgx_ui_widget_t     pin_w = {.kind = DGX_UI_INPUT, .text = &pin_box, .want_h = 14, .look = &secret_look};
/* a widget of the program's own: where up() was told the finger went up, in its own pixels */
static int                   up_x = -1, up_y = -1, up_clicked = -1;
static void                  own_up(dgx_ui_t *u, int i, int x, int y, bool click)
{
    up_x = x, up_y = y, up_clicked = click;
}
static bool                    own_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *u, int i, uint32_t now_ms) { return false; }
static const dgx_ui_behavior_t own_behavior = {.up = own_up};
static const dgx_ui_look_t     own_look     = {.draw = own_draw, .fill = -1, .behavior = &own_behavior};
static const dgx_ui_widget_t   own_w        = {.kind = DGX_UI_OWN, .want_h = 14, .look = &own_look};
static dgx_ui_node_t         nodes[6];
static dgx_ui_t              ui;
static int                   sl, bt, fi, mi;

/* a finger: down at x, y at t, moved to x2, y2 by steps, held, let go */
static dgx_ui_di_gestures_t all;
static void take(dgx_di_touch_t *touch, int x, int y, bool down, uint32_t t)
{
    dgx_ui_di_gestures_t g;
    dgx_di_touch_take(touch, x, y, down, t);
    dgx_ui_di_touch(&ui, touch, t, &g);
    all.edge |= g.edge, all.swipe |= g.swipe, all.long_press |= g.long_press, all.taken |= g.taken;
}
/* a device of the test: the fingers it sees now, in pixels of the display (its range is the display's) */
typedef struct {
    dgx_di_device_t device;
    int             count;
    dgx_di_point_t  points[2];
} test_device_t;
static int test_device_read(dgx_di_device_t *device, dgx_di_point_t *points, int max)
{
    const test_device_t *test = (const test_device_t *)device;
    const int            seen = test->count < max ? test->count : max;
    for (int k = 0; k < seen; ++k) points[k] = test->points[k];
    return seen;
}
/* the device seeing count fingers, the first at x, y, the second at x2, y2; the pointer read at t and given to the
   tree by the adapter */
static void read_pointer(dgx_di_pointer_t *pointer, test_device_t *device, dgx_di_touch_t *touch, int count, int x, int y, int x2, int y2, uint32_t t)
{
    device->count     = count;
    device->points[0] = (dgx_di_point_t){.x = (int16_t)x, .y = (int16_t)y, .id = 1};
    device->points[1] = (dgx_di_point_t){.x = (int16_t)x2, .y = (int16_t)y2, .id = 2};
    dgx_di_pointer_read(pointer, t);
    dgx_ui_di_gestures_t gestures;
    dgx_ui_di_pointer(&ui, pointer, touch, t, &gestures);
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
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.variants = variant, .count = 1};
    dgx_ui_textbox_init(&field_box, field_buf, sizeof field_buf, 1, &style);
    middle = (dgx_ui_text_style_t){.variants = variant, .count = 1, .flags = DGX_UI_TEXT_MIDDLE};
    dgx_ui_textbox_init(&mid_box, mid_buf, sizeof mid_buf, 1, &middle);
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
    CHECK(all.taken && all.edge == 0 && dgx_ui_value_of(dgx_ui_node(&ui, sl)) > 0);
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
    const dgx_font_context_t m  = {.font = &TerminusTTFMedium12};
    const int                cx = n->x + dgx_font_bounds(&m, "ab", NULL, NULL);
    const int                cy = n->y - TerminusTTFMedium12.y_offset_lowest - 2;
    CHECK(panel->mem[cy][cx] == colors[1]); /* the caret, in the ink */
    dgx_ui_focus(&ui, -1);
    dgx_ui_paint(&ui, &pp, 6002);
    CHECK(panel->mem[cy][cx] == colors[0]);

    /* a field of DGX_UI_TEXT_MIDDLE: the caret in the middle rows of the field, as a label sets its text; every pixel
       once */
    mi = dgx_ui_add(&ui, 0, &mid_w); /* under the others, where the strokes above found no widget */
    dgx_ui_paint(&ui, &pp, 6003);
    dgx_ui_focus(&ui, mi);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 6004);
    const dgx_ui_node_t *mn   = dgx_ui_node(&ui, mi);
    const dgx_font_t    *t12  = &TerminusTTFMedium12;
    const int            cell = t12->y_bottom_max - t12->y_offset_lowest, mtop = mn->y + (mn->h - cell) / 2;
    CHECK(mn->h > cell + 4 && panel_twice(panel) == 0 && panel_written(panel) == mn->w * mn->h);
    CHECK(panel->mem[mtop + cell / 2][mn->x] == colors[1] && panel->mem[mn->y][mn->x] == colors[0] && panel->mem[mn->y + mn->h - 1][mn->x] == colors[0]);

    /* a secret field: "b\xd0\xb1" typed shows two stars, the caret after them; the text box keeps the letters */
    dgx_ui_textbox_init(&pin_box, pin_buf, sizeof pin_buf, 1, &style);
    dgx_ui_remove(&ui, mi);
    const int pi = dgx_ui_add(&ui, 0, &pin_w);
    dgx_ui_paint(&ui, &pp, 6005);
    dgx_ui_focus(&ui, pi);
    const uint32_t bb[] = {'b', 0x0431};
    CHECK(dgx_ui_typed(&ui, bb, 2) && strcmp(pin_buf, "b\xd0\xb1") == 0);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 6006);
    const dgx_ui_node_t     *pn  = dgx_ui_node(&ui, pi);
    const int                sx  = pn->x + dgx_font_bounds(&m, "**", NULL, NULL), sy = pn->y - t12->y_offset_lowest - 2;
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == pn->w * pn->h && panel->mem[sy][sx] == colors[3]);
    /* the stars, not the letters: the row as "**" drawn by the font */
    dgx_screen_t *e = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_fill_rectangle(e, 0, 0, PANEL_W, PANEL_H, colors[0]);
    const dgx_font_context_t ec = {.scr = e, .font = t12, .color = colors[1], .bg = colors[0]};
    dgx_font_draw(&ec, pn->x, pn->y - t12->y_offset_lowest, "**");
    int off = 0;
    for (int y = pn->y; y < pn->y + pn->h; ++y)
        for (int x = pn->x; x < sx; ++x) off += panel->mem[y][x] != dgx_get_pixel(e, x, y);
    CHECK(off == 0);
    e->destroy(&e);

    /* the pointer of DGX-DI over a device of the test, steadied as a board's may be: down after 20 ms seen, up after
       30 ms lost */
    test_device_t device = {.device = {.read = test_device_read, .fingers = 2, .x_min = 0, .x_max = PANEL_W - 1, .y_min = 0, .y_max = PANEL_H - 1}};
    dgx_di_pointer_t pointer;
    dgx_di_pointer_init(&pointer, &device.device, 0, scr);
    dgx_di_pointer_debounce(&pointer, 20, 30);
    const dgx_ui_node_t *button = dgx_ui_node(&ui, bt);
    const int            bx = button->x + button->w / 2, by = button->y + button->h / 2;
    int                  first_x, first_y;
    /* a tap on the button through the adapter, 100 ms: a click, where the finger went up */
    int clicks_before = clicks;
    for (uint32_t t = 7000; t <= 7100; t += 10) read_pointer(&pointer, &device, &touch, 1, bx, by, 0, 0, t);
    CHECK(dgx_ui_holding(&ui) && clicks == clicks_before);
    for (uint32_t t = 7110; t <= 7160; t += 10) read_pointer(&pointer, &device, &touch, 0, 0, 0, 0, 0, t);
    CHECK(!dgx_di_pointer_first(&pointer, &first_x, &first_y) && first_x == bx && first_y == by && clicks == clicks_before + 1);
    /* down on the button, moved 12 pixels down (6 mm, short of a swipe) onto the field, let go there: wherever it is
       let go, the button it went down on was meant, a click */
    clicks_before = clicks;
    for (uint32_t t = 8000; t <= 8120; t += 10) read_pointer(&pointer, &device, &touch, 1, bx, by + (int)(t - 8000) / 10, 0, 0, t);
    for (uint32_t t = 8130; t <= 8180; t += 10) read_pointer(&pointer, &device, &touch, 0, 0, 0, 0, 0, t);
    CHECK(!dgx_di_pointer_first(&pointer, &first_x, &first_y) && first_x == bx && first_y == by + 12 && clicks == clicks_before + 1);
    /* moved 40 pixels down, 20 mm: a swipe, no click */
    clicks_before = clicks;
    for (uint32_t t = 8500; t <= 8600; t += 10) read_pointer(&pointer, &device, &touch, 1, bx, by + (int)(t - 8500) * 4 / 10, 0, 0, t);
    for (uint32_t t = 8610; t <= 8660; t += 10) read_pointer(&pointer, &device, &touch, 0, 0, 0, 0, 0, t);
    CHECK(!dgx_di_pointer_first(&pointer, &first_x, &first_y) && first_x == bx && first_y == by + 40 && clicks == clicks_before);
    /* the widget of the program's own, under the others: down at 10 pixels in, moved 8 right, let go; up() told
       where the finger was last down, in its own pixels, as dgx_di_pointer_first() gives it for a lifted finger */
    const int            own = dgx_ui_add(&ui, 0, &own_w);
    dgx_ui_paint(&ui, &pp, 8700);
    const dgx_ui_node_t *own_node = dgx_ui_node(&ui, own);
    for (uint32_t t = 8800; t <= 8880; t += 10) read_pointer(&pointer, &device, &touch, 1, own_node->x + 10 + (int)(t - 8800) / 10, own_node->y + 5, 0, 0, t);
    for (uint32_t t = 8890; t <= 8940; t += 10) read_pointer(&pointer, &device, &touch, 0, 0, 0, 0, 0, t);
    CHECK(own_node->h == 14 && up_x == 18 && up_y == 5 && up_clicked == 1);
    /* a finger on the button lost by the device for 20 ms, shorter than up: still down, one click when it goes up */
    clicks_before = clicks;
    for (uint32_t t = 9000; t <= 9100; t += 10) read_pointer(&pointer, &device, &touch, t >= 9040 && t < 9060 ? 0 : 1, bx, by, 0, 0, t);
    CHECK(dgx_di_pointer_first(&pointer, &first_x, &first_y) && clicks == clicks_before);
    for (uint32_t t = 9110; t <= 9160; t += 10) read_pointer(&pointer, &device, &touch, 0, 0, 0, 0, 0, t);
    CHECK(clicks == clicks_before + 1);
    /* a second finger down on the slider while the first holds the button: not the first; the first up clicks the
       button, and the second moved along the slider after it drags nothing */
    clicks_before        = clicks;
    const int slider_was = dgx_ui_value(&ui, sl);
    for (uint32_t t = 10000; t <= 10100; t += 10) read_pointer(&pointer, &device, &touch, t < 10040 ? 1 : 2, bx, by, 4, 5, t);
    for (uint32_t t = 10110; t <= 10300; t += 10) {
        device.count     = 1; /* the finger left, now the device's first */
        device.points[0] = (dgx_di_point_t){.x = (int16_t)(4 + (int)(t - 10110) / 4), .y = 5, .id = 2};
        dgx_di_pointer_read(&pointer, t);
        dgx_ui_di_gestures_t gestures;
        dgx_ui_di_pointer(&ui, &pointer, &touch, t, &gestures);
    }
    CHECK(clicks == clicks_before + 1 && dgx_ui_value(&ui, sl) == slider_was && !dgx_ui_holding(&ui));
    for (uint32_t t = 10310; t <= 10360; t += 10) read_pointer(&pointer, &device, &touch, 0, 0, 0, 0, 0, t);

    dgx_ui_free(&ui);
    free(panel);
    CHECK_DONE();
}
