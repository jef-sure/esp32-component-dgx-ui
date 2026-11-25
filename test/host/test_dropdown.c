/*
 * The list of a dropdown opened as a popup (dgx_ui_dropdown.h): under the dropdown, above it, moved up to fit, or the
 * whole height shared; its rows as high as the dropdown. A row chosen by a finger or by the keys sets the dropdown's
 * value and tells it CHANGED once; the option it had chosen again tells nothing; a finger outside closes it, the
 * value kept; a key the list does not take does not reach the tree under it; back closes it, the value kept. A dropdown in a popup, its list hanging
 * out of it; one dd for two dropdowns; a unit of 2; drawn, every pixel once. Its options set anew while the list is
 * open and scrolled: shown at once, the window kept, the screen as a fresh paint composes it; fewer options than the
 * pick: the value kept, the row of the pick and the face with no text.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_paint.h"
#include "dgx_vscreen.h"
#include "dgx_ui_text.h"
#include "fonts/TerminusTTFMedium12.h"
/* a screen of 96x80: a list of three rows of 16 goes under a dropdown at its top, above one at its bottom, and in
   neither way around one at 24 */
#define PANEL_W 96
#define PANEL_H 80
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

static int  changed, last_value, activated;
static void on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_CHANGED) ++changed, last_value = dgx_ui_value_of(node);
    if (what == DGX_UI_EVENT_ACTIVATED) ++activated;
}

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static char                  opt_buf[40], many_buf[80];
static dgx_ui_textbox_t      opt_box, many_box;

static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .pad = 0}; /* children where they ask */
static const dgx_ui_widget_t column_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2};
/* three options numbered 1..3, a dropdown 16 high: placed by at_y */
static dgx_ui_widget_t       top_w, bottom_w, middle_w;
static int32_t slider_w_place; /* its value */
static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .value = &slider_w_place, .max = 10, .want_w = 30, .want_h = 6, .at_x = 60, .at_y = 2};
static dgx_ui_widget_t       many_w; /* 6 options: 6 rows of 16 do not go into 80 */

static dgx_ui_dropdown_t dd;
static dgx_ui_node_t     nodes[8];
static dgx_ui_t          ui;

static void make_of(const dgx_ui_widget_t *root)
{
    dgx_ui_init(&ui, nodes, 8, 1);
    dgx_ui_add(&ui, -1, root);
    dgx_ui_screen(&ui, PANEL_W, PANEL_H, 0);
    changed = activated = 0;
}

static void make(void)
{
    make_of(&root_w);
}

/* the pixels of ink 0, the text's on the bar of the pick, on row k of an open list as the window shows it */
static int text_on_row(dgx_screen_t *shown, const dgx_ui_node_t *list, int rows, int k)
{
    int       ink = 0;
    const int row_y = list->y + k * (list->h / rows);
    for (int y = row_y; y < row_y + list->h / rows; ++y)
        for (int x = list->x; x < list->x + list->w; ++x) ink += dgx_get_pixel(shown, x, y) == colors[0];
    return ink;
}

static void tap(dgx_ui_t *t, int x, int y)
{
    dgx_ui_pointer(t, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .down = true, .pressed = true});
    dgx_ui_pointer(t, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .released = true});
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
    dgx_ui_textbox_init(&opt_box, opt_buf, sizeof opt_buf, 1, &style);
    dgx_ui_textbox_init(&many_box, many_buf, sizeof many_buf, 1, &style);
    strcpy(opt_buf, "Apple\nBanana\nCherry"), strcpy(many_buf, "1\n2\n3\n4\n5\n6");
    static int32_t base_place = 1; /* its value */
    static int32_t copy_place[5] = {1, 1, 1, 1, 1}; /* a copy of a widget shares its place otherwise */
    const dgx_ui_widget_t base = {.kind = DGX_UI_DROPDOWN, .value = &base_place, .text = &opt_box, .min = 1, .max = 3, .want_w = 40, .want_h = 16,
                                  .look = &dgx_ui_look_dropdown, .on_event = on};
    top_w = base, top_w.at_y = 0, top_w.value = &copy_place[0];
    bottom_w = base, bottom_w.at_y = -0, bottom_w.value = &copy_place[1]; /* from the far edge: at the bottom */
    bottom_w.at_y = -1, bottom_w.at_x = 0;
    middle_w = base, middle_w.at_y = 24, middle_w.value = &copy_place[2];
    many_w = base, many_w.value = &copy_place[3], many_w.text = &many_box, many_w.max = 6, many_w.at_x = 50;

    /* under it: rows of 16, a frame of 1 around: 3 * 16 + 2 high, at the dropdown's bottom */
    make();
    int d = dgx_ui_add(&ui, 0, &top_w);
    dgx_ui_layout(&ui);
    const dgx_ui_node_t *dn = dgx_ui_node(&ui, d);
    CHECK(dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
    CHECK(ui.popup == &dd.tree && dd.tree.root_rect.y == dn->y + dn->h && dd.tree.root_rect.h == 3 * 16 + 2 && dd.tree.root_rect.w == dn->w);
    CHECK(!dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL)); /* open already */
    /* a finger on the third row: Cherry chosen, told once, the list closed */
    const dgx_ui_rect_t r = dd.tree.root_rect;
    tap(&ui, r.x + 5, r.y + 1 + 2 * 16 + 8);
    CHECK(ui.popup == NULL && changed == 1 && last_value == 3 && dgx_ui_value_of(dgx_ui_node(&ui, d)) == 3);
    /* the keys: open, PREV moves the pick and tells the dropdown nothing, ACTIVATE chooses */
    CHECK(dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_PREV) && changed == 1 && dgx_ui_value_of(dgx_ui_node(&ui, d)) == 3);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE) && ui.popup == NULL && changed == 2 && last_value == 2);
    /* the option it had chosen again: closed, nothing told */
    CHECK(dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE) && ui.popup == NULL && changed == 2);
    /* a finger outside: closed, the value kept, nothing told */
    CHECK(dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
    tap(&ui, PANEL_W - 2, PANEL_H - 2);
    CHECK(ui.popup == NULL && changed == 2 && dgx_ui_value_of(dgx_ui_node(&ui, d)) == 2);
    /* the dropdown removed while its list is open, a slider added in its number: a row tapped closes the list and sets
       nothing of the slider */
    {
        CHECK(dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
        const dgx_ui_rect_t lr = dd.tree.root_rect;
        dgx_ui_remove(&ui, d);
        const int s2 = dgx_ui_add(&ui, 0, &slider_w);
        CHECK(s2 == d);
        changed = 0;
        tap(&ui, lr.x + 5, lr.y + 1 + 2 * 16 + 8);
        CHECK(ui.popup == NULL && changed == 0 && dgx_ui_value_of(dgx_ui_node(&ui, s2)) == 0);
        dgx_ui_remove(&ui, s2);
        d  = dgx_ui_add(&ui, 0, &top_w);
        dn = dgx_ui_node(&ui, d);
        dgx_ui_layout(&ui);
        changed = 0;
    }
    /* a click on the dropdown tells it ACTIVATED: the program opens its list there */
    tap(&ui, dn->x + 2, dn->y + 2);
    CHECK(activated == 1);
    dgx_ui_free(&ui);

    /* above it: at the bottom of the screen */
    make();
    d  = dgx_ui_add(&ui, 0, &bottom_w);
    dgx_ui_layout(&ui);
    dn = dgx_ui_node(&ui, d);
    CHECK(dn->y + dn->h == PANEL_H - 1 && dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
    CHECK(dd.tree.root_rect.y + dd.tree.root_rect.h == dn->y);
    dgx_ui_popup_close(&ui);
    dgx_ui_free(&ui);

    /* no room under it nor above it: moved up to fit the screen, over the dropdown */
    make();
    d  = dgx_ui_add(&ui, 0, &middle_w);
    dgx_ui_layout(&ui);
    dn = dgx_ui_node(&ui, d);
    CHECK(dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
    CHECK(dd.tree.root_rect.y == PANEL_H - (3 * 16 + 2) && dd.tree.root_rect.y < dn->y + dn->h);
    dgx_ui_popup_close(&ui);

    /* six rows of 16 in a screen of 80, the dropdown at its top: as many as the room under it takes, three, rows as
       high as the dropdown, the list scrolling and under it, not over it */
    const int m = dgx_ui_add(&ui, 0, &many_w);
    dgx_ui_layout(&ui);
    CHECK(dgx_ui_dropdown_open(&ui, m, &dd, NULL, NULL));
    dgx_ui_layout(&dd.tree);
    const dgx_ui_node_t *mn = dgx_ui_node(&ui, m);
    CHECK(dd.tree.root_rect.y == mn->y + mn->h && dd.tree.root_rect.h == 3 * 16 + 2 && dd.list.step == 16 && dgx_ui_node(&dd.tree, 1)->h == 3 * 16);
    dgx_ui_popup_close(&ui);
    /* one dd serves the other dropdown after: opened again for the first */
    CHECK(dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL) && dd.i == d);
    dgx_ui_popup_close(&ui);
    dgx_ui_free(&ui);

    /* modal: a key the list does not take (MORE) does not reach the slider focused under it */
    make();
    d             = dgx_ui_add(&ui, 0, &top_w);
    const int sl  = dgx_ui_add(&ui, 0, &slider_w);
    dgx_ui_layout(&ui);
    dgx_ui_focus(&ui, sl);
    CHECK(dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
    dgx_ui_key(&ui, DGX_UI_KEY_MORE);
    CHECK(dgx_ui_value_of(dgx_ui_node(&ui, sl)) == 0 && ui.popup == &dd.tree);
    /* back closes the list, the value kept; then, nothing open, back is the program's */
    const int kept = dgx_ui_value_of(dgx_ui_node(&ui, d));
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_BACK) && ui.popup == NULL && dgx_ui_value_of(dgx_ui_node(&ui, d)) == kept);
    CHECK(!dgx_ui_key(&ui, DGX_UI_KEY_BACK));
    dgx_ui_free(&ui);

    /* a dropdown in a popup of 100x40 at the bottom of the screen: its list above it, taller than the popup, hanging
       out of it; a finger on the part outside the popup chooses */
    {
        static dgx_ui_node_t         pn[2];
        static dgx_ui_t              pop;
        static const dgx_ui_widget_t pop_w = {.kind = DGX_UI_FRAME, .pad = 1};
        dgx_ui_widget_t              in_pop = base;
        in_pop.value = &copy_place[4];
        in_pop.at_y = 2, in_pop.at_x = 2;
        make();
        dgx_ui_init(&pop, pn, 2, 1);
        dgx_ui_add(&pop, -1, &pop_w);
        const int pd = dgx_ui_add(&pop, 0, &in_pop);
        CHECK(dgx_ui_popup_open(&ui, &pop, 0, PANEL_H - 24, PANEL_W, 24, DGX_UI_POPUP_MODAL));
        dgx_ui_screen(&pop, PANEL_W, PANEL_H, 0);
        dgx_ui_layout(&pop);
        const dgx_ui_node_t *pdn = dgx_ui_node(&pop, pd);
        CHECK(dgx_ui_dropdown_open(&pop, pd, &dd, NULL, NULL) && pop.popup == &dd.tree);
        const dgx_ui_rect_t lr = dd.tree.root_rect;
        CHECK(lr.y + lr.h == pdn->y && lr.y < PANEL_H - 24);
        tap(&ui, lr.x + 5, lr.y + 1 + 8); /* the first row, above the popup */
        CHECK(pop.popup == NULL && ui.popup == &pop && changed == 0 && dgx_ui_value_of(dgx_ui_node(&pop, pd)) == 1); /* Apple, as it was */
        CHECK(dgx_ui_dropdown_open(&pop, pd, &dd, NULL, NULL));
        tap(&ui, lr.x + 5, lr.y + 1 + 16 + 8); /* the second row */
        CHECK(pop.popup == NULL && changed == 1 && last_value == 2);
        dgx_ui_popup_close(&ui);
        dgx_ui_free(&ui), dgx_ui_free(&pop);
    }

    /* a unit of 2: the frame 2 pixels, the rows as high as the dropdown (32) */
    {
        static dgx_ui_node_t un[2];
        static dgx_ui_t      u2;
        dgx_ui_init(&u2, un, 2, 2);
        dgx_ui_add(&u2, -1, &root_w);
        dgx_ui_screen(&u2, 200, 200, 0);
        const int ud = dgx_ui_add(&u2, 0, &top_w);
        dgx_ui_layout(&u2);
        CHECK(dgx_ui_node(&u2, ud)->h == 32 && dgx_ui_dropdown_open(&u2, ud, &dd, NULL, NULL));
        CHECK(dd.tree.root_rect.h == 3 * 32 + 4 && dd.tree.unit == 2);
        dgx_ui_popup_close(&u2);
        dgx_ui_free(&u2);
    }

    /* drawn: the dropdown and its list open, every pixel once */
    {
        panel_t       *panel = panel_make();
        dgx_ui_paint_t pp    = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        make_of(&column_w);
        d = dgx_ui_add(&ui, 0, &top_w);
        dgx_ui_paint(&ui, &pp, 0);
        CHECK(dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
        dgx_ui_redraw(&ui, 0);
        panel_count_from_now(panel);
        dgx_ui_paint(&ui, &pp, 1);
        CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H);
        dgx_ui_popup_close(&ui);
        dgx_ui_free(&ui);
        free(panel);
    }

    /* the options of an open dropdown set anew while its list scrolls, as a message may set them: the list shows them
       at once, its window kept at the pick, the screen as a fresh paint composes it. Then fewer options than the pick:
       the value stays where it is, within min..max as always; the list's row of the pick and the dropdown's face show
       no text, as for an option that is empty */
    {
        dgx_screen_t  *shown = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), *fresh = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t pp    = {.scr = shown, .colors = colors, .color_count = 4};
        make();
        strcpy(many_buf, "1\n2\n3\n4\n5\n6");
        d = dgx_ui_add(&ui, 0, &many_w);
        dgx_ui_set_value(&ui, d, 6);
        dgx_fill_rectangle(shown, 0, 0, PANEL_W, PANEL_H, 0);
        dgx_ui_paint(&ui, &pp, 0);
        const dgx_ui_node_t *face = dgx_ui_node(&ui, d);
        int                  face_ink = 0; /* the ink of the text in the left half of the face, the arrow on the right */
        for (int y = face->y; y < face->y + face->h; ++y)
            for (int x = face->x; x < face->x + face->w / 2; ++x) face_ink += dgx_get_pixel(shown, x, y) == colors[1];
        CHECK(face_ink > 0 && dgx_ui_dropdown_open(&ui, d, &dd, NULL, NULL));
        dgx_ui_paint(&ui, &pp, 10);
        dgx_ui_key(&dd.tree, DGX_UI_KEY_PREV), dgx_ui_key(&dd.tree, DGX_UI_KEY_PREV); /* the list's pick 4, inside the window; the
                                                                                         dropdown's value 6 till one is chosen */
        dgx_ui_paint(&ui, &pp, 15);
        const int top = many_box.top, rows = dgx_ui_node(&dd.tree, 1)->h / 16;
        dgx_ui_text_set(&ui, d, "one\ntwo\nthree\nfour\nfive\nsix");
        dgx_ui_paint(&ui, &pp, 20);
        dgx_ui_paint_t fresh_paint = {.scr = fresh, .colors = colors, .color_count = 4, .now_ms = 20};
        dgx_fill_rectangle(fresh, 0, 0, PANEL_W, PANEL_H, 0);
        dgx_ui_paint_area_to(&ui, &fresh_paint, fresh, NULL, 0, 0, PANEL_W, PANEL_H);
        int off = 0;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) off += dgx_get_pixel(shown, x, y) != dgx_get_pixel(fresh, x, y);
        CHECK(top > 0 && rows < 6 && many_box.top == top && off == 0 && dgx_ui_value(&ui, d) == 6 &&
              text_on_row(shown, dgx_ui_node(&dd.tree, 1), rows, 4 - 1 - top) > 0);
        /* two options now, the list's pick the fourth, the dropdown's value the sixth */
        dgx_ui_text_set(&ui, d, "one\ntwo");
        dgx_ui_paint(&ui, &pp, 30);
        fresh_paint.now_ms = 30;
        dgx_fill_rectangle(fresh, 0, 0, PANEL_W, PANEL_H, 0);
        dgx_ui_paint_area_to(&ui, &fresh_paint, fresh, NULL, 0, 0, PANEL_W, PANEL_H);
        off = 0;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) off += dgx_get_pixel(shown, x, y) != dgx_get_pixel(fresh, x, y);
        const dgx_ui_node_t *list = dgx_ui_node(&dd.tree, 1);
        CHECK(off == 0 && dgx_ui_value(&ui, d) == 6 && dgx_ui_value(&dd.tree, 1) == 4 && many_box.top == top &&
              text_on_row(shown, list, rows, 4 - 1 - many_box.top) == 0);
        dgx_ui_popup_close(&ui);
        dgx_ui_paint(&ui, &pp, 40);
        face_ink = 0;
        for (int y = face->y; y < face->y + face->h; ++y)
            for (int x = face->x; x < face->x + face->w / 2; ++x) face_ink += dgx_get_pixel(shown, x, y) == colors[1];
        CHECK(face_ink == 0);
        dgx_ui_paint_free(&pp), dgx_ui_paint_free(&fresh_paint), dgx_ui_free(&ui);
        dgx_screen_destroy(&shown), dgx_screen_destroy(&fresh);
    }
    CHECK_DONE();
}
