/*
 * Values at their bounds and the guards around them. A list of one row and one of none, a finger dragged below them:
 * no pick but the row there is, none with no row; a dropdown the same by the program's user value. A slider, a list and
 * a dropdown of the widest range a widget holds, and of none, their places at INT32_MIN and INT32_MAX: drawn, stepped
 * by NEXT, PREV, MORE and LESS and by a finger, with no overflow (UBSan stops the test at one), the values as the rule
 * says. ACTIVATE held on a button of DGX_UI_REPEAT whose third repeat opens a dialog with a focused OK: the OK never
 * pressed; the focus moved to a plain button while it is held: that button never activated. A crossfade of a page in a
 * place that is hidden, on a panel that cannot be read: no division by a row of no bytes, the page shown. A square
 * screen turned through the eight orientations: laid and drawn again at each turn, every pixel written.
 */
#define PANEL_W 64
#define PANEL_H 64
#include <stdint.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_ui_transition.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

/* what on(CHANGED) heard, by the widget's ctx */
static int changed_count;
static void on_changed(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_CHANGED) *(int32_t *)ctx = dgx_ui_value_of(node), ++changed_count;
}

static const dgx_ui_widget_t column = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};

static void test_rows(void)
{
    static int32_t               one_place, none_place, one_heard = -999, none_heard = -999;
    static const dgx_ui_widget_t one  = {.kind = DGX_UI_LIST, .min = 0, .max = 0, .want_h = 24, .value = &one_place, .on = on_changed, .ctx = &one_heard};
    static const dgx_ui_widget_t none = {.kind = DGX_UI_LIST, .min = 0, .max = -1, .want_h = 24, .value = &none_place, .on = on_changed, .ctx = &none_heard};
    static dgx_ui_node_t         nodes[4];
    dgx_ui_t                     ui;
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_set_root(&ui, 0, 0, 100, 200);
    dgx_ui_add(&ui, -1, &column);
    const int l = dgx_ui_add(&ui, 0, &one), e = dgx_ui_add(&ui, 0, &none);
    dgx_ui_layout(&ui);
    /* a list of one row: a finger down on it and dragged far below, then let go */
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 10, .y = 12, .down = true, .pressed = true, .ms = 0});
    for (int y = 12; y <= 190; y += 30) dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 10, .y = (int16_t)y, .down = true, .ms = 10});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 10, .y = 190, .released = true, .ms = 20});
    CHECK(one_place == 0 && (one_heard == -999 || one_heard == 0) && dgx_ui_value(&ui, l) == 0);
    /* a list of no row: nothing picks, nothing is told */
    const dgx_ui_node_t *n = dgx_ui_node(&ui, e);
    changed_count          = 0;
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 10, .y = (int16_t)(n->y + 12), .down = true, .pressed = true, .ms = 100});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 10, .y = (int16_t)(n->y + 62), .down = true, .ms = 110});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 10, .y = (int16_t)(n->y + 62), .released = true, .ms = 120});
    dgx_ui_user_value(&ui, e, 3);
    CHECK(none_place == 0 && none_heard == -999 && changed_count == 0);
    dgx_ui_free(&ui);

    /* a dropdown of one option, and of none: the user's value bound to the one, none taken */
    static int32_t               choice_one, choice_none;
    static const dgx_ui_widget_t drop_one  = {.kind = DGX_UI_DROPDOWN, .min = 0, .max = 0, .want_h = 20, .value = &choice_one};
    static const dgx_ui_widget_t drop_none = {.kind = DGX_UI_DROPDOWN, .min = 0, .max = -1, .want_h = 20, .value = &choice_none};
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_add(&ui, -1, &column);
    const int d1 = dgx_ui_add(&ui, 0, &drop_one), d0 = dgx_ui_add(&ui, 0, &drop_none);
    dgx_ui_user_value(&ui, d1, 5), dgx_ui_user_value(&ui, d0, 5);
    CHECK(choice_one == 0 && choice_none == 0);
    dgx_ui_free(&ui);
}

/* a slider, a list and a dropdown of the widest range and of none, their places at both ends of int32_t */
static char                   rows_buf[64] = "a\nb\nc";
static dgx_ui_textbox_t       rows_box;
static dgx_ui_font_variant_t  variant[1];
static dgx_ui_text_style_t    style;
static int32_t                places[9];

static void test_extremes(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
    dgx_ui_textbox_init(&rows_box, rows_buf, sizeof rows_buf, 1, &style);
    const dgx_ui_widget_t widgets[9] = {
        {.kind = DGX_UI_SLIDER, .min = INT16_MIN, .max = INT16_MAX, .want_h = 10, .value = &places[0], .look = &dgx_ui_look_slider},
        {.kind = DGX_UI_SLIDER, .min = 0, .max = 0, .want_h = 10, .value = &places[1], .look = &dgx_ui_look_slider},
        {.kind = DGX_UI_SLIDER, .flags = DGX_UI_SWIPE, .min = 5, .max = 5, .want_h = 10, .value = &places[2], .look = &dgx_ui_look_slider},
        {.kind = DGX_UI_LIST, .min = INT16_MIN, .max = INT16_MAX, .want_h = 10, .value = &places[3], .text = &rows_box, .look = &dgx_ui_look_list},
        {.kind = DGX_UI_LIST, .min = 0, .max = -1, .want_h = 10, .value = &places[4], .text = &rows_box, .look = &dgx_ui_look_list},
        {.kind = DGX_UI_DROPDOWN, .min = INT16_MIN, .max = INT16_MAX, .want_h = 10, .value = &places[5], .text = &rows_box, .look = &dgx_ui_look_dropdown},
        /* dropdowns of no option, which draw their face by any value: min past 0 either way */
        {.kind = DGX_UI_DROPDOWN, .min = 1, .max = 0, .want_h = 10, .value = &places[6], .text = &rows_box, .look = &dgx_ui_look_dropdown},
        {.kind = DGX_UI_DROPDOWN, .min = -1, .max = -2, .want_h = 10, .value = &places[7], .text = &rows_box, .look = &dgx_ui_look_dropdown},
        {.kind = DGX_UI_SLIDER, .flags = DGX_UI_SWIPE, .min = INT16_MIN, .max = INT16_MAX, .want_h = 10, .value = &places[8], .look = &dgx_ui_look_slider},
    };
    dgx_screen_t *v = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    static const int32_t ends[2] = {INT32_MIN, INT32_MAX};
    for (int e = 0; e < 2; ++e) {
        static dgx_ui_node_t nodes[12];
        dgx_ui_t             ui;
        dgx_ui_init(&ui, nodes, 12, 1);
        dgx_ui_set_root(&ui, 0, 0, PANEL_W, 200);
        dgx_ui_add(&ui, -1, &column);
        int at[9];
        for (int k = 0; k < 9; ++k) places[k] = ends[e], at[k] = dgx_ui_add(&ui, 0, &widgets[k]);
        dgx_ui_paint_t p = {.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_paint(&ui, &p, 0);
        /* the keys on each: the values as the rule says */
        for (int k = 0; k < 6; ++k) {
            places[k] = ends[e];
            dgx_ui_focus(&ui, at[k]);
            dgx_ui_key(&ui, DGX_UI_KEY_NEXT), dgx_ui_focus(&ui, at[k]), dgx_ui_key(&ui, DGX_UI_KEY_PREV);
            dgx_ui_focus(&ui, at[k]), dgx_ui_key(&ui, DGX_UI_KEY_MORE), dgx_ui_key(&ui, DGX_UI_KEY_LESS);
        }
        /* a slider of the widest range: within it; one of none: stepped from the end inward, never past it */
        CHECK(places[0] == (e ? INT16_MAX - 1 : INT16_MIN + 1) || places[0] == (e ? INT16_MAX : INT16_MIN));
        CHECK(places[1] == (e ? INT32_MAX - 1 : INT32_MIN + 1) || places[1] == ends[e]);
        /* a list of a range: the program's value brought within it, then stepped; one of no row never moved; the
           dropdown's value not moved by these keys */
        CHECK(places[3] >= INT16_MIN && places[3] <= INT16_MAX && places[4] == ends[e] && (places[5] == ends[e] || (places[5] >= INT16_MIN && places[5] <= INT16_MAX)));
        /* the swiped sliders of no range and of the widest, and the list that scrolls, each place at an end: a finger
           across or along each, from the value the program wrote */
        static const int dragged[3] = {2, 8, 3};
        for (int k = 0; k < 3; ++k) {
            places[dragged[k]]      = ends[e];
            const dgx_ui_node_t *sn = dgx_ui_node(&ui, at[dragged[k]]);
            const int16_t        x = (int16_t)(sn->x + 2), y = (int16_t)(sn->y + 2), along = k == 2 ? 0 : 40, down = k == 2 ? 40 : 0;
            const uint32_t       ms = 100 + 100 * (uint32_t)k;
            dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = x, .y = y, .down = true, .pressed = true, .ms = ms});
            dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(x + along), .y = (int16_t)(y + down), .down = true, .ms = ms + 20});
            dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(x + along), .y = (int16_t)(y + down), .released = true, .ms = ms + 40});
        }
        /* drawn again at both ends and at the ends of the range: the list's window around its pick */
        for (int k = 0; k < 9; ++k) places[k] = ends[e];
        dgx_ui_set_value(&ui, at[3], e ? INT16_MAX : INT16_MIN);
        dgx_ui_paint(&ui, &p, 1000);
        CHECK(dgx_ui_value(&ui, at[3]) == (e ? INT16_MAX : INT16_MIN));
        dgx_ui_paint_free(&p);
        dgx_ui_free(&ui);
    }
    v->destroy(&v);
}

/* ACTIVATE held: a dialog its repeat opens, and a plain button the focus moves to */
static int      plus_told, ok_told, plain_told;
static dgx_ui_t home, dialog;
static int      ok;
static void on_plus(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_ACTIVATED) return;
    if (++plus_told == 3) {
        dgx_ui_popup_open(&home, &dialog, 10, 10, 80, 40, DGX_UI_POPUP_MODAL);
        dgx_ui_focus(&dialog, ok);
    }
}
static void on_ok(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) ++ok_told, dgx_ui_popup_close(&home);
}
static void on_plain(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) ++plain_told;
}

static void test_held_key(void)
{
    static const dgx_ui_widget_t plus_w  = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_REPEAT, .want_h = 20, .on = on_plus};
    static const dgx_ui_widget_t plain_w = {.kind = DGX_UI_BUTTON, .want_h = 20, .on = on_plain};
    static const dgx_ui_widget_t frame   = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN};
    static const dgx_ui_widget_t ok_w    = {.kind = DGX_UI_BUTTON, .want_h = 20, .on = on_ok};
    static dgx_ui_node_t         first[4], dnodes[2];
    dgx_ui_init(&home, first, 4, 1);
    dgx_ui_set_root(&home, 0, 0, 100, 100);
    dgx_ui_add(&home, -1, &column);
    const int plus = dgx_ui_add(&home, 0, &plus_w), plain = dgx_ui_add(&home, 0, &plain_w);
    dgx_ui_init(&dialog, dnodes, 2, 1);
    dgx_ui_add(&dialog, -1, &frame);
    ok = dgx_ui_add(&dialog, 0, &ok_w);
    dgx_ui_layout(&home);
    /* the dialog its third repeat opens: its OK never pressed by the key still held */
    dgx_ui_focus(&home, plus);
    dgx_ui_buttons(&home, DGX_UI_PAD_A, 0);
    for (uint32_t t = 10; t <= 2000; t += 10) dgx_ui_buttons(&home, DGX_UI_PAD_A, t), dgx_ui_time(&home, t);
    CHECK(plus_told == 3 && ok_told == 0 && home.popup == &dialog);
    dgx_ui_buttons(&home, 0, 2010);
    dgx_ui_popup_close(&home);
    /* the focus moved to a plain button while the key is held: never activated by the repeat */
    dgx_ui_focus(&home, plus);
    plus_told = -1000;
    dgx_ui_buttons(&home, DGX_UI_PAD_A, 3000);
    dgx_ui_focus(&home, plain);
    for (uint32_t t = 3010; t <= 4000; t += 10) dgx_ui_buttons(&home, DGX_UI_PAD_A, t), dgx_ui_time(&home, t);
    CHECK(plain_told == 0);
    dgx_ui_buttons(&home, 0, 4010);
    dgx_ui_free(&dialog), dgx_ui_free(&home);
}

/* a crossfade of a page in home's place, the place hidden, on a panel that cannot be read */
static void test_hidden_place(void)
{
    static const dgx_ui_widget_t home_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
    static const dgx_ui_widget_t bar_w       = {.kind = DGX_UI_FRAME, .want_h = 10};
    static const dgx_ui_widget_t place_w     = {.kind = DGX_UI_BOX, .flags = DGX_UI_REST};
    static const dgx_ui_widget_t page_root_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN};
    static dgx_ui_node_t         home_nodes[4], page_nodes[2];
    static uint8_t               buffer[PANEL_W * 2 * 8];
    static dgx_ui_t              page;
    panel_t                     *panel = panel_make();
    dgx_ui_transition_t tr = {.from = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), .to = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), .ms = 200};
    dgx_ui_paint_t      paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
    dgx_ui_init(&home, home_nodes, 4, 1);
    dgx_ui_add(&home, -1, &home_root_w);
    dgx_ui_add(&home, 0, &bar_w);
    const int place = dgx_ui_add(&home, 0, &place_w);
    dgx_ui_init(&page, page_nodes, 2, 1);
    dgx_ui_add(&page, -1, &page_root_w);
    dgx_ui_paint(&home, &paint, 0);
    dgx_ui_page_place(&home, place);
    dgx_ui_show(&home, place, false);
    dgx_ui_paint(&home, &paint, 10);
    const bool opened = dgx_ui_page_open_by(&home, &page, &paint, &tr, DGX_UI_CROSSFADE);
    dgx_ui_transition_frame(&tr, &paint, 100);
    dgx_ui_transition_frame(&tr, &paint, 120);
    CHECK(opened && !tr.running && home.popup == &page);
    dgx_ui_page_back(&home);
    dgx_ui_paint_free(&paint);
    dgx_ui_free(&page), dgx_ui_free(&home);
    tr.from->destroy(&tr.from), tr.to->destroy(&tr.to);
    free(panel);
}

/* a square screen turned through the eight orientations: laid and drawn again at each, every pixel written */
static void test_turns(void)
{
    static const dgx_ui_widget_t root_w = {.kind = DGX_UI_FRAME};
    static dgx_ui_node_t         nodes[2];
    dgx_ui_t                     ui;
    dgx_screen_t                *v = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_ui_paint_t               p = {.scr = v, .colors = colors, .color_count = 4};
    dgx_ui_init(&ui, nodes, 2, 1);
    dgx_ui_add(&ui, -1, &root_w);
    dgx_ui_paint(&ui, &p, 0);
    for (int turn = 1; turn <= 8; ++turn) {
        const dgx_output_orientation_t o = (dgx_output_orientation_t)(turn % 8);
        v->dir_x = dgx_output_orientation_xdir(o), v->dir_y = dgx_output_orientation_ydir(o), v->swap_xy = dgx_output_orientation_swap_xy(o);
        dgx_fill_rectangle(v, 0, 0, PANEL_W, PANEL_H, 0x1234); /* a colour no look draws */
        dgx_ui_paint(&ui, &p, (uint32_t)turn * 100);
        int written = 0;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) written += dgx_get_pixel(v, x, y) != 0x1234;
        if (written != PANEL_W * PANEL_H) fprintf(stderr, "  turn %d: %d pixels written\n", turn % 8, written);
        CHECK(written == PANEL_W * PANEL_H);
    }
    dgx_ui_paint_free(&p);
    dgx_ui_free(&ui);
    v->destroy(&v);
}

int main(void)
{
    test_rows();
    test_extremes();
    test_held_key();
    test_hidden_place();
    test_turns();
    CHECK_DONE();
}
