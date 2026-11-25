/*
 * A group of radio buttons and a checkbox. Three radios of one place, numbered 0, 1 and 2 by their step, a list of
 * three rows of the same place beside them, and a checkbox under them; a popup with a fourth radio of the place and
 * one of the same number as the second. A click and DGX_UI_KEY_ACTIVATE choose, the focus walks the buttons, a
 * button chosen again tells nothing, the program writes the place, dgx_ui_set_value() on any button writes the
 * group's; the list and the radios follow each other; a place of no button's number, a radio of no place, a stray
 * min and max, a button disabled or hidden while chosen, a finger on one while the program writes another's number.
 * After each pass on a panel behind a bus (16 bits straight, 18 through the strips: PANEL_BITS, by the Makefile), on
 * the paper and over a drawn background: no pixel written twice, the panel as a fresh paint composes the tree, only
 * the buttons whose choice changed drawn, and the next pass idle: no mark left, no part drawn, no look called.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_stats.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t       colors[4];
static panel_t       *panel;
static dgx_screen_t  *memory, *map, *probe;
static uint8_t        buffer[PANEL_W * 3 * 16]; /* a strip of 16 rows of the panel: a button of the column in one strip */
static dgx_ui_paint_t paint;
static bool           with_map;

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style = {.fonts = variant, .count = 1};

/* the program's memory: the group's place and the checkbox's */
static int32_t choice, ticked;

/* the trees: the home and a popup over it */
static dgx_ui_t home, popup;

/* the looks called and the changes told, by tree (0 the home, 1 the popup) and widget; the looks called by the last
   pass, the fresh composing that checks it left out */
static int looked[2][8], told[2][8], drew[2][8];

static int tree_of(const dgx_ui_t *ui)
{
    return ui == &home ? 0 : 1;
}

static bool counted_radio(const dgx_ui_look_t *look, dgx_ui_paint_t *p, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    ++looked[tree_of(ui)][i];
    return dgx_ui_look_radio.draw(&dgx_ui_look_radio, p, ui, i, now_ms);
}
static bool counted_checkbox(const dgx_ui_look_t *look, dgx_ui_paint_t *p, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    ++looked[tree_of(ui)][i];
    return dgx_ui_look_checkbox.draw(&dgx_ui_look_checkbox, p, ui, i, now_ms);
}
static const dgx_ui_look_t radio_look    = {.draw = counted_radio, .params = NULL, .fill = DGX_UI_PAPER};
static const dgx_ui_look_t checkbox_look = {.draw = counted_checkbox, .params = NULL, .fill = DGX_UI_PAPER};

static void on_told(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_CHANGED) ++told[tree_of(ui)][i];
}

static dgx_ui_textbox_t one_box   = {.buf = (char *)"One", .lines = 1, .style = &style};
static dgx_ui_textbox_t two_box   = {.buf = (char *)"Two", .lines = 1, .style = &style};
static dgx_ui_textbox_t three_box = {.buf = (char *)"Six", .lines = 1, .style = &style};
static dgx_ui_textbox_t tick_box  = {.buf = (char *)"Tick", .lines = 1, .style = &style};
static dgx_ui_textbox_t rows_box  = {.buf = (char *)"A\nB\nC", .lines = 3, .style = &style};
static dgx_ui_textbox_t four_box  = {.buf = (char *)"Four", .lines = 1, .style = &style};

static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 1, .gap = 2};
static const dgx_ui_widget_t left_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .want_w = 56};
static const dgx_ui_widget_t radio_w[3] = {
    {.kind = DGX_UI_RADIO, .value = &choice, .step = 0, .text = &one_box, .look = &radio_look, .on_event = on_told},
    {.kind = DGX_UI_RADIO, .value = &choice, .step = 1, .text = &two_box, .look = &radio_look, .on_event = on_told},
    {.kind = DGX_UI_RADIO, .value = &choice, .step = 2, .text = &three_box, .look = &radio_look, .on_event = on_told},
};
static const dgx_ui_widget_t tick_w = {.kind = DGX_UI_SWITCH, .value = &ticked, .text = &tick_box, .look = &checkbox_look, .on_event = on_told};
static const dgx_ui_widget_t list_w = {
    .kind = DGX_UI_LIST, .flags = DGX_UI_REST, .value = &choice, .min = 0, .max = 2, .text = &rows_box, .look = &dgx_ui_look_list, .on_event = on_told};

/* the popup: a frame holding a fourth radio of the place and a second of number 1 */
static const dgx_ui_widget_t frame_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .look = &dgx_ui_look_frame};
static const dgx_ui_widget_t four_w   = {.kind = DGX_UI_RADIO, .value = &choice, .step = 3, .text = &four_box, .look = &radio_look, .on_event = on_told};
static const dgx_ui_widget_t second_w = {.kind = DGX_UI_RADIO, .value = &choice, .step = 1, .text = &two_box, .look = &radio_look, .on_event = on_told};

/* a box that is hidden, holding a radio of the group and a slider */
static int32_t               level;
static const dgx_ui_widget_t column_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1};
static const dgx_ui_widget_t shelf_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .want_h = 30};
static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .value = &level, .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};

/* odd ones, each alone in a tree of its own */
static int32_t               stray_place;
static const dgx_ui_widget_t stray_w    = {.kind = DGX_UI_RADIO, .value = &stray_place, .min = 0, .max = 2, .step = 3, .look = &dgx_ui_look_radio};
static const dgx_ui_widget_t no_place_w = {.kind = DGX_UI_RADIO, .step = 0, .text = &one_box, .look = &radio_look, .on_event = on_told};

static dgx_ui_node_t home_nodes[8], popup_nodes[3];
static int           left, radio[3], tick, list, four, second;

/* the program's picture under the tree: a checker of two colours */
static void draw_map(dgx_ui_paint_t *p, int x, int y, int w, int h, void *ctx)
{
    dgx_vscreen_region_to_screen(p->scr, x - p->ox, y - p->oy, (dgx_screen_t *)ctx, x, y, w, h, 0);
}

static int64_t fake_us;
static int64_t fake_clock(void)
{
    return fake_us += 10;
}

static dgx_ui_paint_stats_t stats = {.clock_us = fake_clock};

static int mark_side(const dgx_ui_node_t *n);
static int mark_left(const dgx_ui_node_t *n);

/* pixels written twice in the columns of the marks: the point before it, the ring's or the square's, the gap after */
static int twice_in_marks(void)
{
    int count = 0;
    for (const dgx_ui_t *tree = &home; tree; tree = tree->popup)
        for (int i = 0; i < tree->count; ++i) {
            const dgx_ui_node_t *n = dgx_ui_node(tree, i);
            if (!n || !n->widget || (n->widget->kind != DGX_UI_RADIO && n->widget->kind != DGX_UI_SWITCH) || dgx_ui_hidden(tree, i)) continue;
            const int side = mark_side(n);
            for (int y = n->y; y < n->y + n->h; ++y)
                for (int x = n->x; x < mark_left(n) + side + side / 2 && x < n->x + n->w; ++x) count += panel->writes[y][x] > 1;
        }
    return count;
}

/* after a pass: no pixel twice, and the panel as a fresh paint composes the tree at now_ms. Over a drawn background
   straight (16 bits, no strips) letters are drawn over the picture laid for their field, written twice as the painter
   means them: there the columns of the marks are counted */
static int twice_or_off(uint32_t now_ms)
{
    dgx_ui_paint_t fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = now_ms};
    if (with_map) dgx_ui_background_set(&fresh, draw_map, map);
    dgx_fill_rectangle(memory, 0, 0, PANEL_W, PANEL_H, colors[3]);
    dgx_ui_paint_area_to(&home, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint_free(&fresh);
    int off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) off += panel->mem[y][x] != dgx_get_pixel(memory, x, y);
    return (with_map && PANEL_BITS == 16 ? twice_in_marks() : panel_twice(panel)) + off;
}

/* a pass, counted from now and measured; then its pixels checked */
static int pass(uint32_t now_ms)
{
    memset(looked, 0, sizeof looked);
    panel_count_from_now(panel);
    dgx_ui_paint_measured(&home, &paint, now_ms, &stats);
    memcpy(drew, looked, sizeof drew);
    return twice_or_off(now_ms);
}

/* the painter's look at the places before a pass, the library's own (src/dgx_ui_priv.h): called here to see that it
   finds nothing to mark, as the pass that follows would clear what it marked and show nothing of it */
void dgx_ui_places(dgx_ui_t *ui);

/* the next pass draws nothing: the look at the places marks nothing, no mark is left, no part drawn, no look called. A
   hidden widget keeps its marks till it is shown, and a tree under a popup those of the widgets the popup covers, to
   draw them when it closes: their marks are not looked at */
static bool idle(uint32_t now_ms)
{
    for (dgx_ui_t *tree = &home; tree; tree = tree->popup) {
        if (tree->popup) continue;
        dgx_ui_places(tree);
        for (int i = 0; i < tree->count; ++i)
            if (dgx_ui_node(tree, i) && dgx_ui_node(tree, i)->marks && !dgx_ui_hidden(tree, i)) return false;
    }
    memset(looked, 0, sizeof looked);
    panel_count_from_now(panel);
    dgx_ui_paint_measured(&home, &paint, now_ms, &stats);
    int calls = 0;
    for (int t = 0; t < 2; ++t)
        for (int i = 0; i < 8; ++i) calls += looked[t][i];
    return stats.parts_last == 0 && stats.draws_last == 0 && calls == 0 && panel_written(panel) == 0;
}

/* the colour k as the panel holds it */
static uint32_t panel_color(int k)
{
    dgx_set_pixel(probe, 0, 0, colors[k]);
    return dgx_get_pixel(probe, 0, 0);
}

/* the mark's square of a radio or a checkbox, as the looks lay it with this font, two points (pixels here) in from the
   box's edges: the frame of the focus, then a point of paper. Its side, its left */
static int mark_side(const dgx_ui_node_t *n)
{
    const dgx_font_t *f    = variant[0].font;
    int               side = f->y_bottom_max - f->y_offset_lowest;
    if (side > n->h - 4) side = n->h - 4;
    return side < n->w - 2 ? side : n->w - 2;
}
static int mark_left(const dgx_ui_node_t *n)
{
    return n->x + 2;
}

/* the pixel in the middle of the mark: the dot or the tick's, or the inside */
static uint32_t middle_of(const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t *n    = dgx_ui_node(ui, i);
    const int            side = mark_side(n);
    return panel->mem[n->y + (n->h - side) / 2 + side / 2][mark_left(n) + side / 2];
}

/* a pixel of the ring at the top of the mark */
static uint32_t ring_of(const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t *n    = dgx_ui_node(ui, i);
    const int            side = mark_side(n);
    return panel->mem[n->y + (n->h - side) / 2][mark_left(n) + side / 2];
}

/* the frame of the focus round the box: its two far corners in the ink */
static bool framed(const dgx_ui_t *ui, int i, uint32_t ink)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    return panel->mem[n->y][n->x] == ink && panel->mem[n->y + n->h - 1][n->x + n->w - 1] == ink;
}

/* the pixels of the ink inside a checkbox's square, within its border: the tick's */
static int tick_ink(const dgx_ui_t *ui, int i, uint32_t ink)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    const int side = mark_side(n), top = n->y + (n->h - side) / 2, border = 1 + side / 12;
    int       count = 0;
    for (int y = top + border; y < top + side - border; ++y)
        for (int x = mark_left(n) + border; x < mark_left(n) + side - border; ++x) count += panel->mem[y][x] == ink;
    return count;
}

static void finger(dgx_ui_t *ui, int i, bool down, uint32_t ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    const int16_t        x = (int16_t)(n->x + n->w / 2), y = (int16_t)(n->y + n->h / 2);
    dgx_ui_pointer(&home, &(dgx_ui_pointer_t){.x = x, .y = y, .down = down, .pressed = down, .released = !down, .ms = ms});
}

static void click(dgx_ui_t *ui, int i, uint32_t ms)
{
    finger(ui, i, true, ms), finger(ui, i, false, ms + 20);
}

static int told_all(void)
{
    int all = 0;
    for (int t = 0; t < 2; ++t)
        for (int i = 0; i < 8; ++i) all += told[t][i];
    return all;
}

static void make_home(void)
{
    dgx_ui_init(&home, home_nodes, 8, 1);
    dgx_ui_add(&home, -1, &root_w);
    left = dgx_ui_add(&home, 0, &left_w);
    for (int k = 0; k < 3; ++k) radio[k] = dgx_ui_add(&home, left, &radio_w[k]);
    tick = dgx_ui_add(&home, left, &tick_w);
    list = dgx_ui_add(&home, 0, &list_w);
    choice = 0, ticked = 0;
    memset(told, 0, sizeof told);
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
    paint.strips = &dgx_ui_strips;
#endif
    if (with_map) dgx_ui_background_set(&paint, draw_map, map);
    stats = (dgx_ui_paint_stats_t){.clock_us = fake_clock};
}

static void done_home(void)
{
    dgx_ui_paint_free(&paint), dgx_ui_free(&home);
}

int main(void)
{
    panel  = panel_make();
    memory = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    map    = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    probe  = dgx_vscreen_init(1, 1, PANEL_BITS, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) dgx_set_pixel(map, x, y, (x / 4 + y / 4) % 2 ? colors[2] : colors[0]);
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    const uint32_t ink = panel_color(1), fill = panel_color(2);

    for (int pictured = 0; pictured < 2; ++pictured) {
        with_map = pictured;
        make_home();
        uint32_t now = 0;

        /* the first pass: button 0 of place 0 chosen, nothing told; each takes a line of its text and a point above and
           below for the frame of the focus, the checkbox too */
        CHECK(pass(now += 10) == 0 && told_all() == 0);
        CHECK(middle_of(&home, radio[0]) == ink && middle_of(&home, radio[1]) != ink && middle_of(&home, radio[2]) != ink);
        CHECK(dgx_ui_node(&home, tick)->h == dgx_ui_textbox_height(&home, &tick_box) + 2 && dgx_ui_node(&home, radio[1])->h == dgx_ui_textbox_height(&home, &two_box) + 2);
        CHECK(dgx_ui_node(&home, tick)->y + dgx_ui_node(&home, tick)->h <= PANEL_H);
        CHECK(idle(now += 10));

        /* a click on the second: its number in the place, it alone told; the first and it drawn, the third not */
        click(&home, radio[1], now += 10);
        CHECK(choice == 1 && told[0][radio[1]] == 1 && told_all() == 1);
        CHECK(pass(now += 30) == 0);
        CHECK(drew[0][radio[0]] == 1 && drew[0][radio[1]] == 1 && drew[0][radio[2]] == 0);
        CHECK(middle_of(&home, radio[1]) == ink && middle_of(&home, radio[0]) != ink);
        CHECK(idle(now += 10));
        /* the list followed: its pick the second row */
        CHECK(dgx_ui_value(&home, list) == 1);

        /* chosen again: nothing told, the place the same */
        click(&home, radio[1], now += 10);
        CHECK(choice == 1 && told_all() == 1);
        CHECK(pass(now += 30) == 0 && idle(now += 10));

        /* the keys: the focus walks the buttons, a frame round the one that has it, the ring in the ink; ACTIVATE
           chooses, as a knob's click does */
        dgx_ui_focus(&home, radio[0]);
        CHECK(dgx_ui_key(&home, DGX_UI_KEY_NEXT) && home.focus == radio[1]);
        CHECK(dgx_ui_key(&home, DGX_UI_KEY_NEXT) && home.focus == radio[2]);
        CHECK(pass(now += 10) == 0 && framed(&home, radio[2], ink) && !framed(&home, radio[1], ink) && ring_of(&home, radio[2]) == ink);
        CHECK(dgx_ui_key(&home, DGX_UI_KEY_ACTIVATE) && choice == 2 && told[0][radio[2]] == 1);
        CHECK(pass(now += 10) == 0 && drew[0][radio[0]] == 0 && drew[0][radio[1]] == 1 && drew[0][radio[2]] == 1);
        CHECK(idle(now += 10));
        /* MORE and LESS are not a radio's: the place stays */
        CHECK(!dgx_ui_key(&home, DGX_UI_KEY_MORE) && choice == 2);

        /* the program writes the place: the one that was and the one that is drawn, nothing told */
        const int before = told_all();
        choice           = 0;
        CHECK(pass(now += 10) == 0 && drew[0][radio[0]] == 1 && drew[0][radio[1]] == 0 && drew[0][radio[2]] == 1);
        CHECK(told_all() == before && idle(now += 10));

        /* the value of any button is the group's; dgx_ui_set_value() on any writes the group's place */
        CHECK(dgx_ui_value(&home, radio[0]) == 0 && dgx_ui_value(&home, radio[2]) == 0);
        dgx_ui_set_value(&home, radio[0], 2);
        CHECK(choice == 2 && dgx_ui_value(&home, radio[1]) == 2 && told_all() == before);
        CHECK(pass(now += 10) == 0 && drew[0][radio[0]] == 1 && drew[0][radio[1]] == 0 && drew[0][radio[2]] == 1);
        CHECK(idle(now += 10));

        /* the list's row taken: the radios follow; a radio clicked: the list follows */
        click(&home, list, now += 10);
        const int32_t row = choice;
        CHECK(pass(now += 30) == 0 && middle_of(&home, radio[row]) == ink && idle(now += 10));
        click(&home, radio[row == 0 ? 1 : 0], now += 10);
        CHECK(dgx_ui_value(&home, list) == (row == 0 ? 1 : 0));
        CHECK(pass(now += 30) == 0 && idle(now += 10));

        /* a place of no button's number: none shown chosen, the memory left as it is */
        choice = 7;
        CHECK(pass(now += 10) == 0 && choice == 7 && idle(now += 10));
        for (int k = 0; k < 3; ++k) CHECK(middle_of(&home, radio[k]) != ink);

        /* chosen and disabled: drawn so, its dot of the colour disabled; a click on it takes nothing */
        choice = 1;
        dgx_ui_set_state(&home, radio[1], DGX_UI_DISABLED, true);
        CHECK(pass(now += 10) == 0 && middle_of(&home, radio[1]) == fill && idle(now += 10));
        const int told_before = told_all();
        click(&home, radio[1], now += 10);
        dgx_ui_focus(&home, radio[0]);
        CHECK(dgx_ui_key(&home, DGX_UI_KEY_NEXT) && home.focus == radio[2]); /* the walk passes it */
        CHECK(choice == 1 && told_all() == told_before);
        dgx_ui_set_state(&home, radio[1], DGX_UI_DISABLED, false);
        dgx_ui_focus(&home, -1);
        CHECK(pass(now += 10) == 0 && middle_of(&home, radio[1]) == ink && idle(now += 10));

        /* chosen and hidden: none shown chosen; the place written while it is hidden, the passes after idle; shown
           again, it is chosen */
        dgx_ui_show(&home, radio[1], false);
        CHECK(pass(now += 10) == 0 && idle(now += 10));
        for (int k = 0; k < 3; ++k)
            if (k != 1) CHECK(middle_of(&home, radio[k]) != ink);
        choice = 2;
        CHECK(pass(now += 10) == 0 && drew[0][radio[2]] == 1 && idle(now += 10));
        choice = 1;
        CHECK(pass(now += 10) == 0 && drew[0][radio[2]] == 1 && idle(now += 10));
        dgx_ui_show(&home, radio[1], true);
        CHECK(pass(now += 10) == 0 && middle_of(&home, radio[1]) == ink && idle(now += 10));

        /* a finger on the first, inside the mark in the fill; the program writes the third's number meanwhile; let
           go, the click is the last writer: three buttons drawn at most, each once */
        finger(&home, radio[0], true, now += 10);
        CHECK(pass(now += 10) == 0 && middle_of(&home, radio[0]) == fill);
        choice = 2;
        finger(&home, radio[0], false, now += 20);
        CHECK(choice == 0 && pass(now += 10) == 0);
        CHECK(drew[0][radio[0]] == 1 && drew[0][radio[1]] == 1 && drew[0][radio[2]] == 0);
        CHECK(idle(now += 10));
        choice = 1;
        finger(&home, radio[0], true, now += 10);
        choice = 2;
        finger(&home, radio[0], false, now += 20);
        CHECK(choice == 0 && pass(now += 10) == 0 && drew[0][radio[0]] <= 1 && drew[0][radio[1]] <= 1 && drew[0][radio[2]] == 0);
        CHECK(idle(now += 10));

        /* the checkbox: a click ticks it, again clears it; ACTIVATE, MORE and LESS as a switch's */
        click(&home, tick, now += 10);
        CHECK(ticked == 1 && told[0][tick] == 1 && pass(now += 30) == 0 && tick_ink(&home, tick, ink) > 4 && idle(now += 10));
        click(&home, tick, now += 10);
        CHECK(ticked == 0 && told[0][tick] == 2 && pass(now += 30) == 0 && tick_ink(&home, tick, ink) == 0 && idle(now += 10));
        dgx_ui_focus(&home, tick);
        CHECK(dgx_ui_key(&home, DGX_UI_KEY_ACTIVATE) && ticked == 1);
        CHECK(pass(now += 10) == 0 && framed(&home, tick, ink) && ring_of(&home, tick) == ink && idle(now += 10));
        CHECK(dgx_ui_key(&home, DGX_UI_KEY_LESS) && ticked == 0 && pass(now += 10) == 0 && idle(now += 10));
        dgx_ui_focus(&home, -1);
        CHECK(pass(now += 10) == 0 && idle(now += 10));

        /* a popup with a fourth radio of the place and another of number 1: both of number 1 shown chosen */
        dgx_ui_init(&popup, popup_nodes, 3, 1);
        dgx_ui_add(&popup, -1, &frame_w);
        four = dgx_ui_add(&popup, 0, &four_w), second = dgx_ui_add(&popup, 0, &second_w);
        choice = 1;
        CHECK(dgx_ui_popup_open(&home, &popup, 0, 0, 60, 40, DGX_UI_POPUP_MODAL));
        CHECK(pass(now += 10) == 0 && middle_of(&popup, second) == ink && middle_of(&popup, four) != ink && idle(now += 10));
        click(&popup, four, now += 10);
        CHECK(choice == 3 && told[1][four] == 1);
        CHECK(pass(now += 30) == 0 && drew[1][four] == 1 && drew[1][second] == 1 && idle(now += 10));
        /* closed: the home composed where it lay, none of its buttons chosen, each drawn once */
        dgx_ui_popup_close(&home);
        CHECK(pass(now += 10) == 0 && idle(now += 10));
        for (int k = 0; k < 3; ++k) CHECK(middle_of(&home, radio[k]) != ink);
        dgx_ui_free(&popup);
        done_home();
    }

    /* a box hidden holding a radio of the group and a slider, their places written: the button that shows drawn, the
       hidden ones not marked, nor anything above them, pass after pass; the box shown, drawn whole and right */
    {
        with_map = false;
        static dgx_ui_node_t nodes[5];
        dgx_ui_init(&home, nodes, 5, 1);
        dgx_ui_add(&home, -1, &column_w);
        const int shown_radio = dgx_ui_add(&home, 0, &radio_w[0]), shelf = dgx_ui_add(&home, 0, &shelf_w);
        const int hidden_radio = dgx_ui_add(&home, shelf, &radio_w[1]);
        dgx_ui_add(&home, shelf, &slider_w);
        choice = 0, level = 0;
        memset(told, 0, sizeof told);
        paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
        paint.strips = &dgx_ui_strips;
#endif
        stats = (dgx_ui_paint_stats_t){.clock_us = fake_clock};
        uint32_t now = 0;
        CHECK(pass(now += 10) == 0 && middle_of(&home, shown_radio) == ink && idle(now += 10));
        dgx_ui_show(&home, shelf, false);
        CHECK(pass(now += 10) == 0 && idle(now += 10));
        choice = 1, level = 7;
        /* a button as wide as the panel goes through the strips of 18 bits in two: its look called for each */
        CHECK(pass(now += 10) == 0 && drew[0][shown_radio] >= 1 && drew[0][hidden_radio] == 0 && middle_of(&home, shown_radio) != ink);
        for (int k = 0; k < 5; ++k) CHECK(idle(now += 10));
        dgx_ui_show(&home, shelf, true);
        CHECK(pass(now += 10) == 0 && drew[0][hidden_radio] >= 1 && middle_of(&home, hidden_radio) == ink && idle(now += 10));
        done_home();
    }
    /* a stray min and max: the place read as it is, a click writes the button's own number */
    {
        static dgx_ui_node_t nodes[1];
        dgx_ui_t             tree;
        dgx_ui_init(&tree, nodes, 1, 1);
        dgx_ui_add(&tree, -1, &stray_w);
        stray_place = 5;
        CHECK(dgx_ui_value(&tree, 0) == 5 && !dgx_ui_radio_chosen(dgx_ui_node(&tree, 0)));
        stray_place = 2;
        CHECK(!dgx_ui_radio_chosen(dgx_ui_node(&tree, 0)));
        dgx_ui_set_value(&tree, 0, 3);
        CHECK(stray_place == 3 && dgx_ui_radio_chosen(dgx_ui_node(&tree, 0)));
        dgx_ui_free(&tree);
    }
    /* a radio of no place: number 0, yet never chosen; a click takes nothing and tells nothing */
    {
        with_map = false;
        static dgx_ui_node_t nodes[1];
        dgx_ui_init(&home, nodes, 1, 1);
        dgx_ui_add(&home, -1, &no_place_w);
        memset(told, 0, sizeof told);
        paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
        paint.strips = &dgx_ui_strips;
#endif
        stats = (dgx_ui_paint_stats_t){.clock_us = fake_clock};
        CHECK(pass(10) == 0 && !dgx_ui_radio_chosen(dgx_ui_node(&home, 0)) && middle_of(&home, 0) != ink);
        click(&home, 0, 20);
        CHECK(told_all() == 0 && pass(50) == 0 && idle(60));
        done_home();
    }
    CHECK_DONE();
}
