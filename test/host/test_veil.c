/*
 * The focus under a popup: while a popup of DGX_UI_POPUP_MODAL lies over a tree, anywhere above it, the tree's looks
 * show no focus, and when it closes the mark is back where the focus is then; the focus itself, ui->focus and the bit,
 * stays, and no DGX_UI_FOCUS is told. For each look that shows the focus (a button, a dropdown, a switch, a slider, a
 * radio, a checkbox, a field and a list), in a home of two of them: a modal dialog opened and closed, the focused widget
 * and the dialog's place the only pixels written and the next pass quiet; a popup not modal, under which the mark stays
 * and the widget is not written; the focus moved by the program while covered, writing nothing; a dialog opened and
 * closed before a pass; a dialog moved; the focused widget hidden, disabled or removed while covered. A hint over a
 * modal dialog over home: the dialog shows its focus and home none; a modal dialog over a hint over home: home none,
 * and the veil set before the dialog's root is told it is shown. A page closed over a home whose focus moved
 * meanwhile, at once and by a transition; a dropdown's list open over a dropdown in a dialog, two trees veiled; a modal
 * popup freed, and a veiled tree freed. After each pass no pixel twice and the panel the trees as a fresh paint composes
 * them, on 16 bits straight and 18 through the strips (PANEL_BITS, by the Makefile), with the paper and over a drawn
 * background.
 */
#define PANEL_W 160
#define PANEL_H 120

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

static uint32_t       colors[4];
static panel_t       *panel;
static dgx_screen_t  *memory;
static uint8_t        buffer[PANEL_W * 3 * 8];
static dgx_ui_paint_t paint;
static bool           with_bands; /* over the program's picture */

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style = {.fonts = variant, .count = 1};

static dgx_ui_textbox_t go_box = DGX_UI_TEXT_CONST("Go", 1, &style), go2_box = DGX_UI_TEXT_CONST("Go", 1, &style);
static dgx_ui_textbox_t options_box = DGX_UI_TEXT_CONST("One\nTwo", 1, &style), options2_box = DGX_UI_TEXT_CONST("One\nTwo", 1, &style);
static dgx_ui_textbox_t radio_box = DGX_UI_TEXT_CONST("Radio", 1, &style), radio2_box = DGX_UI_TEXT_CONST("Radio", 1, &style);
static dgx_ui_textbox_t tick_box = DGX_UI_TEXT_CONST("Tick", 1, &style), tick2_box = DGX_UI_TEXT_CONST("Tick", 1, &style);
static dgx_ui_textbox_t field_box = DGX_UI_TEXT_CONST("42", 1, &style), field2_box = DGX_UI_TEXT_CONST("43", 1, &style);
static dgx_ui_textbox_t months_box = DGX_UI_TEXT_CONST("Jan\nFeb\nMar", 3, &style), months2_box = DGX_UI_TEXT_CONST("Jan\nFeb\nMar", 3, &style);
static dgx_ui_textbox_t ok_box = DGX_UI_TEXT_CONST("OK", 1, &style), page_box = DGX_UI_TEXT_CONST("Page", 1, &style);

/* the program's memory */
static int32_t option, on, level, choice = 1, ticked, month, month2, inner_option;

/* DGX_UI_FOCUS told to any widget */
static int focus_told;
static void on_widget(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    focus_told += what == DGX_UI_FOCUS;
}

/* each look's widget, twice: the first and the second of home */
typedef struct look_case_t {
    const char     *name;
    dgx_ui_widget_t widget[2];
} look_case_t;
static const look_case_t cases[] = {
    {"button",
     {{.kind = DGX_UI_BUTTON, .want_w = 40, .want_h = 14, .text = &go_box, .look = &dgx_ui_look_button, .on = on_widget},
      {.kind = DGX_UI_BUTTON, .want_w = 40, .want_h = 14, .text = &go2_box, .look = &dgx_ui_look_button, .on = on_widget}}},
    {"dropdown",
     {{.kind = DGX_UI_DROPDOWN, .value = &option, .max = 1, .want_w = 50, .want_h = 14, .text = &options_box, .look = &dgx_ui_look_dropdown, .on = on_widget},
      {.kind = DGX_UI_DROPDOWN, .value = &option, .max = 1, .want_w = 50, .want_h = 14, .text = &options2_box, .look = &dgx_ui_look_dropdown, .on = on_widget}}},
    {"switch",
     {{.kind = DGX_UI_SWITCH, .value = &on, .want_w = 30, .want_h = 12, .look = &dgx_ui_look_switch, .on = on_widget},
      {.kind = DGX_UI_SWITCH, .value = &on, .want_w = 30, .want_h = 12, .look = &dgx_ui_look_switch, .on = on_widget}}},
    {"slider",
     {{.kind = DGX_UI_SLIDER, .value = &level, .max = 10, .want_w = 60, .want_h = 10, .look = &dgx_ui_look_slider, .on = on_widget},
      {.kind = DGX_UI_SLIDER, .value = &level, .max = 10, .want_w = 60, .want_h = 10, .look = &dgx_ui_look_slider, .on = on_widget}}},
    {"radio",
     {{.kind = DGX_UI_RADIO, .value = &choice, .step = 1, .want_w = 60, .text = &radio_box, .look = &dgx_ui_look_radio, .on = on_widget},
      {.kind = DGX_UI_RADIO, .value = &choice, .step = 2, .want_w = 60, .text = &radio2_box, .look = &dgx_ui_look_radio, .on = on_widget}}},
    {"checkbox",
     {{.kind = DGX_UI_SWITCH, .value = &ticked, .want_w = 60, .text = &tick_box, .look = &dgx_ui_look_checkbox, .on = on_widget},
      {.kind = DGX_UI_SWITCH, .value = &ticked, .want_w = 60, .text = &tick2_box, .look = &dgx_ui_look_checkbox, .on = on_widget}}},
    {"field",
     {{.kind = DGX_UI_INPUT, .want_w = 50, .want_h = 14, .text = &field_box, .look = &dgx_ui_look_input, .on = on_widget},
      {.kind = DGX_UI_INPUT, .want_w = 50, .want_h = 14, .text = &field2_box, .look = &dgx_ui_look_input, .on = on_widget}}},
    {"list",
     {{.kind = DGX_UI_LIST, .value = &month, .max = 2, .want_w = 50, .want_h = 30, .text = &months_box, .look = &dgx_ui_look_list, .on = on_widget},
      {.kind = DGX_UI_LIST, .value = &month2, .max = 2, .want_w = 50, .want_h = 30, .text = &months2_box, .look = &dgx_ui_look_list, .on = on_widget}}},
};

static const dgx_ui_widget_t column_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 3};
static const dgx_ui_widget_t ok_w     = {.kind = DGX_UI_BUTTON, .want_w = 40, .want_h = 14, .text = &ok_box, .look = &dgx_ui_look_button, .on = on_widget};
static const dgx_ui_widget_t frame_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3};

/* the dialog's root told it is shown: whether home's first widget showed its focus at that moment, as a composing
   asked from that on() would draw it */
static int shown_told, focus_shown_then;
static void on_dialog_root(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
static const dgx_ui_widget_t told_frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .on = on_dialog_root};
static const dgx_ui_widget_t inner_dd = {.kind = DGX_UI_DROPDOWN, .value = &inner_option, .max = 1, .want_w = 50, .want_h = 14, .text = &options_box, .look = &dgx_ui_look_dropdown, .on = on_widget};
static const dgx_ui_widget_t label_w  = {.kind = DGX_UI_LABEL, .want_h = 14, .text = &page_box};

/* the program's picture under the trees: bands of two colours */
static void draw_bands(dgx_ui_paint_t *p, int x, int y, int w, int h, void *ctx)
{
    for (int row = y; row < y + h; ++row) dgx_fill_rectangle(p->scr, x - p->ox, row - p->oy, w, 1, row % 6 < 3 ? colors[0] : colors[2]);
}

static void paint_anew(void)
{
    dgx_ui_paint_free(&paint);
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
    paint.strips = &dgx_ui_strips;
#endif
    if (with_bands) dgx_ui_background_set(&paint, draw_bands, NULL);
}

static bool in_rect(dgx_ui_rect_t r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* a pass of home's chain counted from now: no pixel twice (straight over the picture a text's letters are drawn over
   the picture laid for their field, twice as the painter means them: there only the panel is compared), nothing
   written outside the rectangles given, and the panel the chain as a fresh paint composes it */
static int pass_within(dgx_ui_t *home, uint32_t now_ms, const dgx_ui_rect_t *allowed, int allowed_count)
{
    panel_count_from_now(panel);
    dgx_ui_paint(home, &paint, now_ms);
    int outside = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            if (!panel->writes[y][x]) continue;
            bool in = false;
            for (int k = 0; k < allowed_count; ++k) in |= in_rect(allowed[k], x, y);
            outside += !in;
        }
    dgx_ui_paint_t fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = now_ms};
    if (with_bands) dgx_ui_background_set(&fresh, draw_bands, NULL);
    dgx_fill_rectangle(memory, 0, 0, PANEL_W, PANEL_H, colors[3]);
    dgx_ui_paint_area_to(home, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint_free(&fresh);
    int off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) off += panel->mem[y][x] != dgx_get_pixel(memory, x, y);
    const int twice = with_bands && PANEL_BITS == 16 ? 0 : panel_twice(panel);
    return twice + outside + off;
}

/* a pass that may write anywhere */
static int pass_any(dgx_ui_t *home, uint32_t now_ms)
{
    const dgx_ui_rect_t all = {0, 0, PANEL_W, PANEL_H};
    return pass_within(home, now_ms, &all, 1);
}

/* a pass that writes nothing */
static bool quiet(dgx_ui_t *home, uint32_t now_ms)
{
    panel_count_from_now(panel);
    dgx_ui_paint(home, &paint, now_ms);
    return panel_written(panel) == 0;
}

static dgx_ui_rect_t box_of(const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    return dgx_ui_rect(n->x, n->y, n->w, n->h);
}

/* snapshots of the panel */
static uint32_t plain[PANEL_H][PANEL_W], first_marked[PANEL_H][PANEL_W], second_marked[PANEL_H][PANEL_W], inner_marked[PANEL_H][PANEL_W];

/* pixels of a box that differ from a snapshot */
static int differ_in(dgx_ui_rect_t r, uint32_t snapshot[PANEL_H][PANEL_W])
{
    int count = 0;
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) count += panel->mem[y][x] != snapshot[y][x];
    return count;
}

/* the trees */
static dgx_ui_node_t home_nodes[4], dialog_nodes[3], hint_nodes[2], page_nodes[2], top_nodes[2];
static dgx_ui_t      home, dialog, hint, page, top;
static int           first, second, ok;

/* home of a look's two widgets; the dialog of a button, or of a dropdown */
static void make_home(const look_case_t *lc)
{
    dgx_ui_init(&home, home_nodes, 4, 1);
    dgx_ui_add(&home, -1, &column_w);
    first  = dgx_ui_add(&home, 0, &lc->widget[0]);
    second = dgx_ui_add(&home, 0, &lc->widget[1]);
}
static void make_dialog(const dgx_ui_widget_t *inside)
{
    dgx_ui_init(&dialog, dialog_nodes, 3, 1);
    dgx_ui_add(&dialog, -1, &frame_w);
    ok = dgx_ui_add(&dialog, 0, inside);
}
static void make_one(dgx_ui_t *ui, dgx_ui_node_t *nodes, const dgx_ui_widget_t *inside)
{
    dgx_ui_init(ui, nodes, 2, 1);
    dgx_ui_add(ui, -1, &frame_w);
    dgx_ui_add(ui, 0, inside);
}

/* where the dialog lies: right of home's widgets, and elsewhere when moved */
static const dgx_ui_rect_t dialog_at = {96, 8, 60, 30}, moved_at = {96, 60, 60, 30}, hint_at = {100, 70, 50, 24};

/* one look under the popups, the snapshots taken first: the plain home, each widget marked */
static void look_under_popups(const look_case_t *lc)
{
    make_home(lc);
    make_dialog(&ok_w);
    paint_anew();
    CHECK(pass_any(&home, 0) == 0);
    memcpy(plain, panel->mem, sizeof plain);
    dgx_ui_focus(&home, second);
    CHECK(pass_any(&home, 10) == 0);
    memcpy(second_marked, panel->mem, sizeof second_marked);
    dgx_ui_focus(&home, first);
    CHECK(pass_any(&home, 20) == 0);
    memcpy(first_marked, panel->mem, sizeof first_marked);
    const dgx_ui_rect_t a = box_of(&home, first), b = box_of(&home, second);
    const bool          marked = differ_in(a, plain) > 0 && differ_in(b, plain) == 0;
    CHECK(marked);
    if (!marked) fprintf(stderr, "  the %s: its mark not seen\n", lc->name);
    focus_told = 0;

    /* a modal dialog: the mark goes, the widget and the dialog alone written, the focus kept; at its close back */
    const dgx_ui_rect_t at_open[2] = {a, dialog_at};
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_within(&home, 30, at_open, 2) == 0 && differ_in(a, plain) == 0 && quiet(&home, 40));
    CHECK(home.focus == first && (dgx_ui_node(&home, first)->state.bits & DGX_UI_FOCUSED));
    dgx_ui_popup_close(&home);
    CHECK(pass_within(&home, 50, at_open, 2) == 0 && differ_in(a, first_marked) == 0 && quiet(&home, 60));

    /* not modal: the mark stays, the widget not written */
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_CLOSE_OUTSIDE));
    CHECK(pass_within(&home, 70, &dialog_at, 1) == 0 && differ_in(a, first_marked) == 0);
    dgx_ui_popup_close(&home);
    CHECK(pass_within(&home, 80, &dialog_at, 1) == 0 && quiet(&home, 90));

    /* the focus moved by the program while covered: nothing written; at the close the mark on the second */
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_within(&home, 100, at_open, 2) == 0);
    dgx_ui_focus(&home, second);
    CHECK(quiet(&home, 110) && differ_in(a, plain) == 0 && differ_in(b, plain) == 0);
    dgx_ui_popup_close(&home);
    const dgx_ui_rect_t at_close[2] = {b, dialog_at};
    CHECK(pass_within(&home, 120, at_close, 2) == 0 && differ_in(b, second_marked) == 0 && differ_in(a, plain) == 0);
    dgx_ui_focus(&home, first);
    CHECK(pass_any(&home, 130) == 0);

    /* opened and closed before a pass: the widget not written */
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    dgx_ui_popup_close(&home);
    CHECK(pass_within(&home, 140, &dialog_at, 1) == 0 && differ_in(a, first_marked) == 0);

    /* moved: still veiled, the widget not written again */
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_within(&home, 150, at_open, 2) == 0);
    CHECK(dgx_ui_popup_open(&home, &dialog, moved_at.x, moved_at.y, moved_at.w, moved_at.h, DGX_UI_POPUP_MODAL));
    const dgx_ui_rect_t both[2] = {dialog_at, moved_at};
    CHECK(pass_within(&home, 160, both, 2) == 0 && differ_in(a, plain) == 0);
    dgx_ui_popup_close(&home);
    CHECK(pass_any(&home, 170) == 0 && differ_in(a, first_marked) == 0);

    /* hidden, disabled, removed while covered: at the close the panel as composed anew, every pixel once */
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_any(&home, 180) == 0);
    dgx_ui_show(&home, first, false);
    CHECK(pass_any(&home, 190) == 0);
    dgx_ui_popup_close(&home);
    CHECK(pass_any(&home, 200) == 0);
    dgx_ui_show(&home, first, true);
    dgx_ui_focus(&home, first);
    CHECK(pass_any(&home, 210) == 0);
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_any(&home, 220) == 0);
    dgx_ui_set_state(&home, first, DGX_UI_DISABLED, true);
    CHECK(pass_any(&home, 230) == 0);
    dgx_ui_popup_close(&home);
    CHECK(pass_any(&home, 240) == 0 && quiet(&home, 250));
    dgx_ui_set_state(&home, first, DGX_UI_DISABLED, false);
    dgx_ui_focus(&home, first);
    CHECK(pass_any(&home, 260) == 0);
    const int told = focus_told; /* the program's own moves of the focus tell it */
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_any(&home, 270) == 0);
    dgx_ui_popup_close(&home);
    CHECK(pass_any(&home, 280) == 0 && focus_told == told); /* the focus did not come at the close */
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_any(&home, 290) == 0);
    dgx_ui_remove(&home, first);
    CHECK(pass_any(&home, 300) == 0);
    dgx_ui_popup_close(&home);
    CHECK(pass_any(&home, 310) == 0 && quiet(&home, 320));
    dgx_ui_free(&dialog), dgx_ui_free(&home);
}

static void on_dialog_root(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_SHOWN) return;
    ++shown_told;
    focus_shown_then = dgx_ui_bits_shown(&home, dgx_ui_node(&home, first)) & DGX_UI_FOCUSED;
}

/* the cases of chains: home of buttons under a hint over a dialog, a page, a dropdown's list, trees freed */
static void chains(void)
{
    make_home(&cases[0]);
    make_dialog(&ok_w);
    make_one(&hint, hint_nodes, &label_w);
    paint_anew();
    dgx_ui_focus(&home, first);
    CHECK(pass_any(&home, 0) == 0);
    memcpy(first_marked, panel->mem, sizeof first_marked);
    const dgx_ui_rect_t a = box_of(&home, first);

    /* a hint over a modal dialog over home: the dialog keeps its mark, home has none */
    dgx_ui_focus(&dialog, ok);
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_any(&home, 10) == 0);
    const dgx_ui_rect_t d = box_of(&dialog, ok);
    memcpy(inner_marked, panel->mem, sizeof inner_marked);
    CHECK(dgx_ui_popup_open(&dialog, &hint, hint_at.x, hint_at.y, hint_at.w, hint_at.h, 0));
    CHECK(pass_within(&home, 20, &hint_at, 1) == 0 && differ_in(d, inner_marked) == 0);
    dgx_ui_popup_close(&dialog);
    CHECK(pass_within(&home, 30, &hint_at, 1) == 0 && differ_in(d, inner_marked) == 0);
    dgx_ui_popup_close(&home);
    CHECK(pass_any(&home, 40) == 0 && differ_in(a, first_marked) == 0);

    /* a modal dialog over a hint over home: home has no mark, though the popup right over it is not modal; the
       dialog's on(SHOWN), told within the call, finds the veil set already */
    dgx_ui_free(&dialog);
    dgx_ui_init(&dialog, dialog_nodes, 3, 1);
    dgx_ui_add(&dialog, -1, &told_frame_w);
    ok = dgx_ui_add(&dialog, 0, &ok_w);
    CHECK(dgx_ui_popup_open(&home, &hint, hint_at.x, hint_at.y, hint_at.w, hint_at.h, 0));
    CHECK(pass_within(&home, 41, &hint_at, 1) == 0 && differ_in(a, first_marked) == 0);
    shown_told = 0, focus_shown_then = 1;
    CHECK(dgx_ui_popup_open(&hint, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(shown_told == 1 && !focus_shown_then);
    const dgx_ui_rect_t over_hint[2] = {a, dialog_at};
    CHECK(pass_within(&home, 42, over_hint, 2) == 0 && differ_in(a, first_marked) > 0);
    dgx_ui_popup_close(&hint);
    CHECK(pass_within(&home, 43, over_hint, 2) == 0 && differ_in(a, first_marked) == 0);
    dgx_ui_popup_close(&home);
    CHECK(pass_any(&home, 44) == 0);
    dgx_ui_free(&dialog);
    make_dialog(&ok_w);

    /* a modal popup freed: the mark back; a veiled tree freed, with the modal one over it: the mark back */
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_any(&home, 50) == 0);
    dgx_ui_free(&dialog);
    CHECK(pass_any(&home, 60) == 0 && differ_in(a, first_marked) == 0);
    make_dialog(&ok_w);
    make_one(&top, top_nodes, &ok_w);
    dgx_ui_focus(&dialog, ok);
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(dgx_ui_popup_open(&dialog, &top, hint_at.x, hint_at.y, hint_at.w, hint_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_any(&home, 70) == 0);
    dgx_ui_free(&dialog);
    CHECK(home.popup == NULL && pass_any(&home, 80) == 0 && differ_in(a, first_marked) == 0);
    dgx_ui_free(&top);

    /* a page over home, home's focus moved meanwhile: closed, the screen composed and the second marked, every pixel
       once in one pass; then the same by a transition */
    make_one(&page, page_nodes, &ok_w);
    CHECK(dgx_ui_page_open(&home, &page));
    CHECK(pass_any(&home, 90) == 0);
    dgx_ui_focus(&home, second);
    CHECK(pass_any(&home, 100) == 0);
    CHECK(dgx_ui_page_back(&home));
    CHECK(pass_any(&home, 110) == 0);
    const dgx_ui_rect_t b = box_of(&home, second);
    memcpy(second_marked, panel->mem, sizeof second_marked);
    CHECK(differ_in(b, first_marked) > 0 && differ_in(a, first_marked) > 0); /* the mark moved from the first to the second */
    dgx_ui_focus(&home, first);
    CHECK(pass_any(&home, 120) == 0 && differ_in(a, first_marked) == 0);
    static dgx_ui_transition_t tr;
    if (!tr.from) tr = (dgx_ui_transition_t){.from = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), .to = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), .ms = 100};
    CHECK(dgx_ui_page_open(&home, &page));
    CHECK(pass_any(&home, 130) == 0);
    dgx_ui_focus(&home, second);
    CHECK(dgx_ui_page_back_by(&home, &paint, &tr, DGX_UI_SLIDE_RIGHT));
    for (uint32_t now_ms = 140; tr.running && now_ms < 2000; now_ms += 20) dgx_ui_transition_frame(&tr, &paint, now_ms);
    CHECK(!tr.running && pass_any(&home, 2100) == 0 && differ_in(b, second_marked) == 0 && quiet(&home, 2110));
    dgx_ui_focus(&home, first);
    CHECK(pass_any(&home, 2120) == 0);
    dgx_ui_free(&page);

    /* a dropdown in a modal dialog, its list open: home and the dialog veiled, the dropdown's frame gone; back when
       the list closes */
    make_dialog(&inner_dd);
    dgx_ui_focus(&dialog, ok);
    CHECK(dgx_ui_popup_open(&home, &dialog, dialog_at.x, dialog_at.y, dialog_at.w, dialog_at.h, DGX_UI_POPUP_MODAL));
    CHECK(pass_any(&home, 2200) == 0);
    const dgx_ui_rect_t dd_box = box_of(&dialog, ok);
    memcpy(inner_marked, panel->mem, sizeof inner_marked);
    dgx_ui_focus(&dialog, -1);
    CHECK(pass_any(&home, 2210) == 0);
    memcpy(plain, panel->mem, sizeof plain);
    CHECK(differ_in(dd_box, inner_marked) > 0);
    dgx_ui_focus(&dialog, ok);
    CHECK(pass_any(&home, 2220) == 0);
    static dgx_ui_dropdown_t dd;
    CHECK(dgx_ui_dropdown_open(&dialog, ok, &dd, NULL, NULL));
    CHECK(pass_any(&home, 2230) == 0);
    const dgx_ui_rect_t list_rect = dd.tree.root_rect;
    dgx_ui_rect_t       seen;
    if (!dgx_ui_rect_intersect(&dd_box, &list_rect, &seen)) CHECK(differ_in(dd_box, plain) == 0);
    else { /* the list over it: the rows of the dropdown above the list as unmarked */
        const dgx_ui_rect_t above = dgx_ui_rect(dd_box.x, dd_box.y, dd_box.w, list_rect.y > dd_box.y ? list_rect.y - dd_box.y : 0);
        CHECK(differ_in(above, plain) == 0);
    }
    CHECK(differ_in(a, plain) == 0);
    dgx_ui_popup_close(&dialog);
    CHECK(pass_any(&home, 2240) == 0 && differ_in(dd_box, inner_marked) == 0 && quiet(&home, 2250));
    dgx_ui_popup_close(&home);
    CHECK(pass_any(&home, 2260) == 0 && differ_in(a, first_marked) == 0);
    dgx_ui_free(&dialog), dgx_ui_free(&hint), dgx_ui_free(&home);
}

static uint32_t panel_color(int k)
{
    static dgx_screen_t *probe;
    if (!probe) probe = dgx_vscreen_init(1, 1, PANEL_BITS, DgxScreenRGB);
    dgx_set_pixel(probe, 0, 0, colors[k]);
    return dgx_get_pixel(probe, 0, 0);
}

int main(void)
{
    panel  = panel_make();
    memory = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    (void)panel_color;
    for (int bands = 0; bands < 2; ++bands) {
        with_bands = bands;
        for (size_t c = 0; c < sizeof cases / sizeof *cases; ++c) look_under_popups(&cases[c]);
        chains();
    }
    dgx_ui_paint_free(&paint);
    CHECK_DONE();
}
