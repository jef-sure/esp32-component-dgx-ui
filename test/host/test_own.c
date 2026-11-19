/*
 * Widgets of the program's own (DGX_UI_OWN) worked by a behaviour named by their look: a dial whose value is the angle
 * of the finger round its middle, taken as a drag; a pad of two axes, its x the value and its y in `own`, of the
 * finger alone. The core does the common part: the hit, PRESSED, the focus (the pad takes none and leaves it), the rule
 * of a click, the keys to the widget first and NEXT on past it, hidden and disabled, the finger owned while dragged.
 * up() comes before DGX_UI_ACTIVATED, and a value it sets is the one on() hears of; one of no move() is clicked as a
 * button; up() with no click after a drag and after 600 ms; a widget removed by its own on() in a move; a look of no
 * behaviour, not hit. The key ACTIVATE on the widget of no move() and on the dial, whose key() takes it not: told as
 * a click, up() not called; up() putting another widget in its slot, the newcomer told nothing. dgx_ui_user_value()
 * on a slider: CHANGED once, nothing for the same value.
 */
#include <math.h>
#include <string.h>

#include "check.h"
#include "dgx_ui.h"

static bool draw_none(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms) { return false; }

/* the dial: 0 to its right, 25 under its middle, 50 to its left, 75 over it */
static void dial_down(dgx_ui_t *ui, int i, int x, int y)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    double               a = atan2(y - n->h / 2, x - n->w / 2) * 180.0 / M_PI;
    if (a < 0) a += 360.0;
    dgx_ui_user_value(ui, i, (int)lround(a * 100.0 / 360.0) % 100);
}
static bool dial_move(dgx_ui_t *ui, int i, int x, int y)
{
    dial_down(ui, i, x, y);
    return true;
}
static bool dial_key(dgx_ui_t *ui, int i, int key)
{
    if (key != DGX_UI_KEY_MORE && key != DGX_UI_KEY_LESS) return false;
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    dgx_ui_user_value(ui, i, dgx_ui_value_of(n) + (key == DGX_UI_KEY_MORE ? 5 : -5));
    return true;
}
static int  ups, up_click = -1;
static void dial_up(dgx_ui_t *ui, int i, int x, int y, bool click)
{
    ++ups, up_click = click;
    if (click) dgx_ui_user_value(ui, i, 42); /* a value set in up(): heard of with the click */
}
/* up() that takes its widget away and puts another in its slot: that one is not told the click */
static const dgx_ui_widget_t *swap_in;
static int                    swapped = -1;
static void                   swap_up(dgx_ui_t *ui, int i, int x, int y, bool click)
{
    dgx_ui_remove(ui, i);
    swapped = dgx_ui_add(ui, 0, swap_in);
}
static const dgx_ui_behavior_t swap_behavior = {.up = swap_up};
static const dgx_ui_look_t     swap_look     = {.draw = draw_none, .fill = -1, .behavior = &swap_behavior};

static const dgx_ui_behavior_t dial_behavior = {.flags = DGX_UI_BEHAVIOR_FOCUS, .down = dial_down, .move = dial_move, .up = dial_up, .key = dial_key};
static const dgx_ui_look_t     dial_look     = {.draw = draw_none, .fill = -1, .behavior = &dial_behavior};

/* the pad: x the value 0..100, y 0..100 in own, told by dgx_ui_tell(); the finger alone, no focus */
static int  pad_y;
static void pad_at(dgx_ui_t *ui, int i, int x, int y)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    int                 *py = (int *)n->widget->own;
    const int            ny = y * 100 / (n->h - 1);
    if (ny != *py) *py = ny, dgx_ui_tell(ui, i, DGX_UI_CHANGED);
    dgx_ui_user_value(ui, i, x * 100 / (n->w - 1));
}
static bool pad_move(dgx_ui_t *ui, int i, int x, int y)
{
    pad_at(ui, i, x, y);
    return true;
}
static const dgx_ui_behavior_t pad_behavior = {.down = pad_at, .move = pad_move};
static const dgx_ui_look_t     pad_look     = {.draw = draw_none, .fill = -1, .behavior = &pad_behavior};

/* a widget of no move(): pressed, clicked as a button */
static const dgx_ui_behavior_t press_behavior = {.flags = DGX_UI_BEHAVIOR_FOCUS, .up = dial_up};
static const dgx_ui_look_t     press_look     = {.draw = draw_none, .fill = -1, .behavior = &press_behavior};
/* a look of no behaviour */
static const dgx_ui_look_t plain_look = {.draw = draw_none, .fill = -1};

static int  told[32], told_n, told_value[32], remove_at = -1;
static void on_any(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_FOCUS) return;
    if (told_n < 32) told_value[told_n] = dgx_ui_value_of(node), told[told_n++] = what;
    if (remove_at == i && what == DGX_UI_CHANGED) dgx_ui_remove(ui, i), remove_at = -1;
}

static void finger(dgx_ui_t *ui, int x, int y, bool pressed, bool released, uint32_t ms)
{
    dgx_ui_pointer(ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .down = !released, .pressed = pressed, .released = released, .ms = ms});
}

int main(void)
{
    static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 0, .gap = 0};
    static const dgx_ui_widget_t button_w = {.kind = DGX_UI_BUTTON, .want_w = 20, .want_h = 41, .on = on_any};
    static int32_t dial_w_place; /* its value */
    const dgx_ui_widget_t        dial_w   = {.kind = DGX_UI_OWN, .value = &dial_w_place, .max = 99, .want_w = 41, .want_h = 41, .look = &dial_look, .on = on_any};
    static int32_t pad_w_place; /* its value */
    const dgx_ui_widget_t        pad_w    = {.kind = DGX_UI_OWN, .value = &pad_w_place, .max = 100, .want_w = 101, .want_h = 41, .look = &pad_look, .on = on_any, .own = &pad_y};
    static int32_t press_w_place; /* its value */
    const dgx_ui_widget_t        press_w  = {.kind = DGX_UI_OWN, .value = &press_w_place, .max = 100, .want_w = 20, .want_h = 41, .look = &press_look, .on = on_any};
    static int32_t plain_w_place; /* its value */
    const dgx_ui_widget_t        plain_w  = {.kind = DGX_UI_OWN, .value = &plain_w_place, .want_w = 20, .want_h = 41, .look = &plain_look, .on = on_any};
    static dgx_ui_node_t         nodes[8];
    static dgx_ui_t              ui;
    dgx_ui_init(&ui, nodes, 8, 1);
    dgx_ui_add(&ui, -1, &root_w);
    const int bt = dgx_ui_add(&ui, 0, &button_w), d = dgx_ui_add(&ui, 0, &dial_w), p = dgx_ui_add(&ui, 0, &pad_w);
    const int pr = dgx_ui_add(&ui, 0, &press_w), pl = dgx_ui_add(&ui, 0, &plain_w);
    dgx_ui_screen(&ui, 240, 64, 0);
    dgx_ui_layout(&ui);
    const dgx_ui_node_t *dn = dgx_ui_node(&ui, d), *pn = dgx_ui_node(&ui, p), *prn = dgx_ui_node(&ui, pr), *pln = dgx_ui_node(&ui, pl);
    const int            cx = dn->x + 20, cy = dn->y + 20;

    /* hit: the dial, the pad, the widget of no move; the look of no behaviour, not */
    CHECK(dgx_ui_hit(&ui, cx, cy) == d && dgx_ui_hit(&ui, pn->x + 5, pn->y + 5) == p && dgx_ui_hit(&ui, prn->x + 5, prn->y + 5) == pr);
    CHECK(dgx_ui_hit(&ui, pln->x + 5, pln->y + 5) != pl);

    /* the dial: down under its middle 25, the focus and PRESSED; round to its left 50, to over it 75, a drag; let go,
       up() with no click, no ACTIVATED; the finger owned while dragged */
    told_n = 0;
    finger(&ui, cx, cy + 15, true, false, 1000);
    CHECK(dgx_ui_value_of(dn) == 25 && (dn->state.bits & (DGX_UI_PRESSED | DGX_UI_FOCUSED)) == (DGX_UI_PRESSED | DGX_UI_FOCUSED));
    CHECK(!dgx_ui_dragging(&ui));
    finger(&ui, cx - 15, cy, false, false, 1050);
    CHECK(dgx_ui_value_of(dn) == 50 && dgx_ui_dragging(&ui));
    finger(&ui, cx, cy - 15, false, false, 1100);
    CHECK(dgx_ui_value_of(dn) == 75);
    ups = 0;
    finger(&ui, cx, cy - 15, false, true, 1150);
    CHECK(ups == 1 && up_click == 0 && !(dn->state.bits & DGX_UI_PRESSED) && told_n == 3);
    for (int k = 0; k < told_n; ++k) CHECK(told[k] == DGX_UI_CHANGED);
    /* the keys: MORE taken by it, five on; NEXT past it to the next that takes the focus, past the pad */
    told_n = 0;
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_MORE) && dgx_ui_value_of(dn) == 80 && told_n == 1);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_NEXT) && ui.focus == pr);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_NEXT) && ui.focus == bt);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_NEXT) && ui.focus == d);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_PREV) && ui.focus == bt);

    /* the pad: the finger alone, the focus left on the button; x the value, y its own, told CHANGED for each */
    told_n = 0;
    finger(&ui, pn->x + 50, pn->y + 10, true, false, 2000);
    CHECK(ui.focus == bt && dgx_ui_value_of(pn) == 50 && pad_y == 25 && told_n == 2);
    finger(&ui, pn->x + 100, pn->y + 40, false, false, 2050);
    CHECK(dgx_ui_value_of(pn) == 100 && pad_y == 100 && dgx_ui_dragging(&ui));
    finger(&ui, pn->x + 100, pn->y + 40, false, true, 2100);

    /* of no move(): pressed and clicked as a button, up() first, the value it set heard of with the click */
    told_n = 0, ups = 0;
    finger(&ui, prn->x + 5, prn->y + 5, true, false, 3000);
    finger(&ui, prn->x + 6, prn->y + 5, false, false, 3020);
    finger(&ui, prn->x + 6, prn->y + 5, false, true, 3100);
    CHECK(ups == 1 && up_click == 1 && told_n == 2 && told[0] == DGX_UI_CHANGED && told[1] == DGX_UI_ACTIVATED && told_value[1] == 42);
    /* held 600 ms: up() with no click, nothing activated */
    told_n = 0, ups = 0;
    finger(&ui, prn->x + 5, prn->y + 5, true, false, 4000);
    finger(&ui, prn->x + 5, prn->y + 5, false, true, 4600);
    CHECK(ups == 1 && up_click == 0 && told_n == 0);
    /* the key ACTIVATE on it, focused, and on the dial, whose key() takes MORE and LESS alone: told as a click, up() not
       called */
    told_n = 0, ups = 0;
    dgx_ui_focus(&ui, pr);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE) && told_n == 1 && told[0] == DGX_UI_ACTIVATED && ups == 0);
    dgx_ui_focus(&ui, d);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE) && told_n == 2 && told[1] == DGX_UI_ACTIVATED && ups == 0);
    /* a click whose up() puts another widget in its slot: the new one told nothing */
    {
        static const dgx_ui_widget_t newcomer_w = {.kind = DGX_UI_BUTTON, .want_w = 20, .want_h = 20, .on = on_any};
        static int32_t swap_w_place; /* its value */
        const dgx_ui_widget_t        swap_w     = {.kind = DGX_UI_OWN, .value = &swap_w_place, .want_w = 20, .want_h = 41, .look = &swap_look, .on = on_any};
        swap_in                                 = &newcomer_w;
        const int sw                            = dgx_ui_add(&ui, 0, &swap_w);
        dgx_ui_layout(&ui);
        const dgx_ui_node_t *sn = dgx_ui_node(&ui, sw);
        const int            sx = sn->x + 5, sy = sn->y + 5;
        told_n                  = 0;
        finger(&ui, sx, sy, true, false, 4800);
        finger(&ui, sx, sy, false, true, 4850);
        CHECK(swapped == sw && dgx_ui_widget(&ui, sw) == &newcomer_w && told_n == 0);
        dgx_ui_remove(&ui, sw);
    }

    /* disabled, hidden: not hit, no focus */
    dgx_ui_set_state(&ui, d, DGX_UI_DISABLED, true);
    CHECK(dgx_ui_hit(&ui, cx, cy) != d);
    dgx_ui_focus(&ui, d);
    CHECK(ui.focus != d);
    dgx_ui_set_state(&ui, d, DGX_UI_DISABLED, false);
    dgx_ui_show(&ui, p, false);
    dgx_ui_layout(&ui);
    CHECK(dgx_ui_hit(&ui, pn->x + 5, pn->y + 5) != p);
    dgx_ui_show(&ui, p, true);
    dgx_ui_layout(&ui);

    /* removed by its own on() in a move: nothing more of it, the finger held by nobody */
    finger(&ui, cx, cy + 15, true, false, 5000);
    CHECK(ui.pressed == d);
    remove_at = d;
    finger(&ui, cx - 15, cy, false, false, 5050);
    CHECK(!dgx_ui_widget(&ui, d) && ui.pressed == -1 && !dgx_ui_dragging(&ui));
    finger(&ui, cx - 15, cy, false, true, 5100);

    /* a widget of a core kind whose look names a behaviour: worked as its kind, the behaviour not called */
    {
        static const dgx_ui_widget_t odd_w = {.kind = DGX_UI_BUTTON, .want_w = 20, .want_h = 20, .look = &dial_look, .on = on_any};
        const int                    o     = dgx_ui_add(&ui, 0, &odd_w);
        dgx_ui_layout(&ui);
        const dgx_ui_node_t *on = dgx_ui_node(&ui, o);
        told_n = 0, ups = 0;
        finger(&ui, on->x + 5, on->y + 5, true, false, 6000);
        finger(&ui, on->x + 5, on->y + 5, false, true, 6050);
        CHECK(ups == 0 && dgx_ui_value_of(on) == 0 && told_n == 1 && told[0] == DGX_UI_ACTIVATED);
    }

    /* keys pressed, no buttons ever read: no key is held, dgx_ui_time() has nothing to step and calls nothing */
    CHECK(!dgx_ui_time(&ui, 5000) && !dgx_ui_time(&ui, 0));
    /* dgx_ui_user_value() on a slider, as the user: CHANGED once; the same value, nothing */
    {
        static int32_t slider_w_place; /* its value */
        static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .value = &slider_w_place, .max = 10, .want_w = 40, .want_h = 10, .on = on_any};
        const int                    s        = dgx_ui_add(&ui, 0, &slider_w);
        told_n                                = 0;
        dgx_ui_user_value(&ui, s, 7);
        dgx_ui_user_value(&ui, s, 7);
        dgx_ui_user_value(&ui, s, 70); /* within its range: 10 */
        CHECK(told_n == 2 && told[0] == DGX_UI_CHANGED && told_value[1] == 10);
        dgx_ui_tell(&ui, s, DGX_UI_ACTIVATED);
        CHECK(told_n == 3 && told[2] == DGX_UI_ACTIVATED);
    }
    dgx_ui_free(&ui);
    CHECK_DONE();
}
