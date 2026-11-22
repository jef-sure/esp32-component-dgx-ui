/*
 * The focus and the pressed state of the looks of the controls: a button (with a shadow), a dropdown, a switch, a
 * slider, a radio button and a checkbox. The focus of a button, a dropdown, a radio and a checkbox is a frame of a point
 * round the box (a button's round its face, moving with it) in `focused`, the inside the same picture as without the
 * focus; a switch's and a slider's is their knob drawn hollow, its border in `focused`. A finger on a button or a
 * dropdown turns its face to `pressed`, on a knob widens it, on a radio or a checkbox fills its mark's inside. Of each
 * look, two widgets in a column: the focus put on the first, moved to the second by the key, put back and the first
 * pressed, then all taken away; each step's pass writing only the widgets whose state changed, every pixel once, and the
 * panel the tree as it is composed anew. Parameters whose `focused` is DGX_UI_PAPER show no focus, on the paper and
 * over a drawn background; a widget of two points is all frame or all knob; a widget half under
 * a popup is framed in the part that shows. At points of 1, 2 and 3: paper between the mark of the focus and what it
 * marks, a box of its own height and a mark with no text in a narrow box. On a panel behind a bus, 16 bits straight or
 * 18 through the strips (PANEL_BITS, by the Makefile).
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t       colors[4];
static panel_t       *panel;
static dgx_screen_t  *memory;
static uint8_t        buffer[PANEL_W * 3 * 8];
static dgx_ui_paint_t paint;

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style = {.variants = variant, .count = 1};

static dgx_ui_textbox_t go_box = DGX_UI_TEXT_CONST("Go", 1, &style), options_box = DGX_UI_TEXT_CONST("One\nTwo", 1, &style);
static dgx_ui_textbox_t radio_box = DGX_UI_TEXT_CONST("Radio", 1, &style), tick_box = DGX_UI_TEXT_CONST("Tick", 1, &style);
static dgx_ui_textbox_t go2_box = DGX_UI_TEXT_CONST("Go", 1, &style), options2_box = DGX_UI_TEXT_CONST("One\nTwo", 1, &style);
static dgx_ui_textbox_t radio2_box = DGX_UI_TEXT_CONST("Radio", 1, &style), tick2_box = DGX_UI_TEXT_CONST("Tick", 1, &style);

/* the program's memory */
static int32_t option, on, level, choice = 1, ticked;

/* each look's widget, twice: the first and the second of its tree */
typedef struct look_case_t {
    const char     *name;
    dgx_ui_widget_t widget[2];
    bool            face; /* a button: its frame round its face, the shadow outside it */
    bool            knob; /* a switch, a slider: the focus on the knob, hollow, not round the box */
} look_case_t;
static const look_case_t cases[] = {
    {"button",
     {{.kind = DGX_UI_BUTTON, .want_h = 14, .text = &go_box, .look = &dgx_ui_look_button},
      {.kind = DGX_UI_BUTTON, .want_h = 14, .text = &go2_box, .look = &dgx_ui_look_button}},
     true, false},
    {"dropdown",
     {{.kind = DGX_UI_DROPDOWN, .value = &option, .max = 1, .want_h = 14, .text = &options_box, .look = &dgx_ui_look_dropdown},
      {.kind = DGX_UI_DROPDOWN, .value = &option, .max = 1, .want_h = 14, .text = &options2_box, .look = &dgx_ui_look_dropdown}},
     false, false},
    {"switch",
     {{.kind = DGX_UI_SWITCH, .value = &on, .want_w = 30, .want_h = 12, .look = &dgx_ui_look_switch},
      {.kind = DGX_UI_SWITCH, .value = &on, .want_w = 30, .want_h = 12, .look = &dgx_ui_look_switch}},
     false, true},
    {"slider",
     {{.kind = DGX_UI_SLIDER, .value = &level, .max = 10, .want_h = 10, .look = &dgx_ui_look_slider},
      {.kind = DGX_UI_SLIDER, .value = &level, .max = 10, .want_h = 10, .look = &dgx_ui_look_slider}},
     false, true},
    {"radio",
     {{.kind = DGX_UI_RADIO, .value = &choice, .step = 1, .text = &radio_box, .look = &dgx_ui_look_radio},
      {.kind = DGX_UI_RADIO, .value = &choice, .step = 2, .text = &radio2_box, .look = &dgx_ui_look_radio}},
     false, false},
    {"checkbox",
     {{.kind = DGX_UI_SWITCH, .value = &ticked, .text = &tick_box, .look = &dgx_ui_look_checkbox},
      {.kind = DGX_UI_SWITCH, .value = &ticked, .text = &tick2_box, .look = &dgx_ui_look_checkbox}},
     false, false},
};

static const dgx_ui_widget_t column_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 3};

static void paint_anew(void)
{
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
    paint.strips = &dgx_ui_strips;
#endif
}

/* a pass counted from now: no pixel twice, nothing written outside the boxes given (a, b; b may be NULL), and the
   panel the tree as a fresh paint composes it in the state it is in */
static bool pass(dgx_ui_t *ui, uint32_t now_ms, const dgx_ui_node_t *a, const dgx_ui_node_t *b)
{
    panel_count_from_now(panel);
    dgx_ui_paint(ui, &paint, now_ms);
    int outside = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            const bool in_a = a && x >= a->x && x < a->x + a->w && y >= a->y && y < a->y + a->h;
            const bool in_b = b && x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h;
            outside += panel->writes[y][x] && !in_a && !in_b;
        }
    dgx_ui_paint_t fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = now_ms};
    dgx_fill_rectangle(memory, 0, 0, PANEL_W, PANEL_H, colors[3]);
    dgx_ui_paint_area_to(ui, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint_free(&fresh);
    int off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) off += panel->mem[y][x] != dgx_get_pixel(memory, x, y);
    return panel_twice(panel) == 0 && outside == 0 && off == 0;
}

/* a widget's box as the panel holds it */
static uint32_t kept[PANEL_H][PANEL_W];
static void     keep(void)
{
    memcpy(kept, panel->mem, sizeof kept);
}

/* the rectangle of the pixels of a colour within a box */
static dgx_ui_rect_t bounds_of(const dgx_ui_node_t *n, uint32_t color);

/* the knob of a switch or a slider, the ink of its plain picture: hollow with the focus, its border a pixel of the ink
   and its inside without it; the pixels outside the knob as they were */
static bool knob_hollow(const dgx_ui_node_t *n, dgx_ui_rect_t k, uint32_t ink)
{
    int off = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) {
            const bool in = x >= k.x && x < k.x + k.w && y >= k.y && y < k.y + k.h;
            const bool border = in && (x == k.x || x == k.x + k.w - 1 || y == k.y || y == k.y + k.h - 1);
            if (border) off += panel->mem[y][x] != ink;
            else if (in) off += panel->mem[y][x] == ink;
            else off += panel->mem[y][x] != kept[y][x];
        }
    return off == 0;
}

/* the rectangle framed: the box, or a button's face where it stands */
static dgx_ui_rect_t framed_rect(const dgx_ui_t *ui, const dgx_ui_node_t *n, bool face)
{
    if (!face) return dgx_ui_rect(n->x, n->y, n->w, n->h);
    const int depth = 1 * ui->unit, d = (n->state.bits & DGX_UI_PRESSED) ? depth : 0;
    return dgx_ui_rect(n->x + d, n->y + d, n->w - depth, n->h - depth);
}

/* within the rectangle framed: the ring all of `ring`, and the pixels inside it and outside it as kept */
static bool frame_seen(const dgx_ui_node_t *n, dgx_ui_rect_t r, uint32_t ring)
{
    int ring_off = 0, inside_off = 0, outside_off = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) {
            const bool in      = x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
            const bool on_ring = in && (x == r.x || x == r.x + r.w - 1 || y == r.y || y == r.y + r.h - 1);
            if (on_ring) ring_off += panel->mem[y][x] != ring;
            else if (in) inside_off += panel->mem[y][x] != kept[y][x];
            else outside_off += panel->mem[y][x] != kept[y][x];
        }
    return ring_off == 0 && inside_off == 0 && outside_off == 0;
}

/* the ring of a rectangle all of one colour */
static bool ring_is(dgx_ui_rect_t r, uint32_t ring)
{
    int off = 0;
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x)
            if (x == r.x || x == r.x + r.w - 1 || y == r.y || y == r.y + r.h - 1) off += panel->mem[y][x] != ring;
    return off == 0;
}

/* pixels of a widget's box that differ from those kept */
static int changed_in(const dgx_ui_node_t *n)
{
    int count = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) count += panel->mem[y][x] != kept[y][x];
    return count;
}

static uint32_t panel_color(int k)
{
    static dgx_screen_t *probe;
    if (!probe) probe = dgx_vscreen_init(1, 1, PANEL_BITS, DgxScreenRGB);
    dgx_set_pixel(probe, 0, 0, colors[k]);
    return dgx_get_pixel(probe, 0, 0);
}

/* the mark of the focus in a widget's box at a point of `point` pixels: the ring of the rectangle framed (box looks),
   or the border of the knob (a switch's, a slider's), the rectangle of the ink the focused knob is drawn in; true for a
   pixel of it */
static bool in_ring(dgx_ui_rect_t r, int point, int x, int y)
{
    const bool in = x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    return in && (x < r.x + point || x >= r.x + r.w - point || y < r.y + point || y >= r.y + r.h - point);
}

/* paper between the focus's mark and what it marks: no pixel of the ring has a neighbour within the box of the
   ring's colour that is not of the ring. The count of such neighbours */
static int touching(const dgx_ui_node_t *n, dgx_ui_rect_t ring, int point, uint32_t ink)
{
    int count = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) {
            if (!in_ring(ring, point, x, y)) continue;
            static const int steps[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (int k = 0; k < 4; ++k) {
                const int nx = x + steps[k][0], ny = y + steps[k][1];
                if (nx < n->x || nx >= n->x + n->w || ny < n->y || ny >= n->y + n->h || in_ring(ring, point, nx, ny)) continue;
                count += panel->mem[ny][nx] == ink;
            }
        }
    return count;
}

/* the rectangle of the pixels of a colour within a box */
static dgx_ui_rect_t bounds_of(const dgx_ui_node_t *n, uint32_t color)
{
    int x0 = n->x + n->w, y0 = n->y + n->h, x1 = n->x - 1, y1 = n->y - 1;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x)
            if (panel->mem[y][x] == color) x0 = x < x0 ? x : x0, y0 = y < y0 ? y : y0, x1 = x > x1 ? x : x1, y1 = y > y1 ? y : y1;
    return x1 < x0 ? dgx_ui_rect(0, 0, 0, 0) : dgx_ui_rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}

/* the program's picture under the tree: bands of two colours */
static void draw_bands(dgx_ui_paint_t *p, int x, int y, int w, int h, void *ctx)
{
    for (int row = y; row < y + h; ++row) dgx_fill_rectangle(p->scr, x - p->ox, row - p->oy, w, 1, row % 6 < 3 ? colors[0] : colors[2]);
}

int main(void)
{
    panel  = panel_make();
    memory = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    variant[0]         = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    const uint32_t ink = panel_color(1);

    for (size_t c = 0; c < sizeof cases / sizeof *cases; ++c) {
        const look_case_t   *lc = &cases[c];
        static dgx_ui_node_t nodes[3];
        dgx_ui_t             ui;
        dgx_ui_init(&ui, nodes, 3, 1);
        dgx_ui_add(&ui, -1, &column_w);
        const int first = dgx_ui_add(&ui, 0, &lc->widget[0]), second = dgx_ui_add(&ui, 0, &lc->widget[1]);
        paint_anew();
        dgx_ui_paint(&ui, &paint, 0);
        const dgx_ui_node_t *a = dgx_ui_node(&ui, first), *b = dgx_ui_node(&ui, second);
        keep(); /* the first without the focus */

        /* the focus on the first: its box alone written, every pixel once; the frame in the ink, the inside as it was;
           a knob hollow, the rest as it was */
        const dgx_ui_rect_t plain_knob = bounds_of(a, ink);
        dgx_ui_focus(&ui, first);
        CHECK(pass(&ui, 10, a, NULL));
        const bool seen = lc->knob ? knob_hollow(a, plain_knob, ink) : frame_seen(a, framed_rect(&ui, a, lc->face), ink);
        CHECK(seen);
        if (!seen) fprintf(stderr, "  the %s focused\n", lc->name);

        /* moved to the second by the key: both drawn, nothing else; no ring left on the first */
        CHECK(dgx_ui_key(&ui, DGX_UI_KEY_NEXT) && ui.focus == second);
        CHECK(pass(&ui, 20, a, b) && changed_in(a) == 0);
        if (!lc->knob) CHECK(panel->mem[b->y][b->x] == ink && panel->mem[b->y + b->h - 1 - (lc->face ? 1 : 0)][b->x] == ink);

        /* the focus back on the first, then a finger on it: a third picture, unlike the focused and the plain ones */
        dgx_ui_focus(&ui, first);
        CHECK(pass(&ui, 30, a, b));
        uint32_t focused_picture[PANEL_H][PANEL_W];
        memcpy(focused_picture, panel->mem, sizeof focused_picture);
        dgx_ui_set_state(&ui, first, DGX_UI_PRESSED, true);
        CHECK(pass(&ui, 40, a, NULL) && changed_in(a) > 0);
        int unlike_focused = 0;
        for (int y = a->y; y < a->y + a->h; ++y)
            for (int x = a->x; x < a->x + a->w; ++x) unlike_focused += panel->mem[y][x] != focused_picture[y][x];
        CHECK(unlike_focused > 0);
        if (lc->face) CHECK(ring_is(framed_rect(&ui, a, true), ink)); /* the frame moved with the face */

        /* let go, the focus taken away: the first as it was at the start */
        dgx_ui_set_state(&ui, first, DGX_UI_PRESSED, false);
        dgx_ui_focus(&ui, -1);
        CHECK(pass(&ui, 50, a, NULL) && changed_in(a) == 0);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }

    /* parameters whose focused is DGX_UI_PAPER show no focus, on the paper and over a drawn background, in each of the
       six looks; and a dropdown's of the face's colour */
    for (int pictured = 0; pictured < 2; ++pictured) {
        static const dgx_ui_button_look_t   button_quiet   = {.face = 2, .pressed = 3, .focused = DGX_UI_PAPER, .disabled = 0, .text = 1, .text_disabled = 2, .shadow = 0, .depth = 1};
        static const dgx_ui_dropdown_look_t dropdown_quiet = {.face = 2, .pressed = 3, .focused = 2, .disabled = 0, .text = 1, .text_disabled = 2, .arrow = 1};
        static const dgx_ui_switch_look_t   switch_quiet   = {.off = 2, .on = 3, .knob = 1, .focused = DGX_UI_PAPER, .disabled = 2};
        static const dgx_ui_slider_look_t   slider_quiet   = {.track = 2, .fill = 3, .knob = 1, .focused = DGX_UI_PAPER, .disabled = 2, .track_h = 2};
        static const dgx_ui_check_look_t    check_quiet    = {
                  .mark = 1, .focused = DGX_UI_PAPER, .disabled = 2, .sign = 1, .inside = DGX_UI_PAPER, .pressed = 2, .ink = 1, .ink_disabled = 2, .paper = DGX_UI_PAPER};
        static const dgx_ui_look_t quiet_looks[6] = {
            {.draw = dgx_ui_draw_button, .params = &button_quiet, .fill = -1},  {.draw = dgx_ui_draw_dropdown, .params = &dropdown_quiet, .fill = -1},
            {.draw = dgx_ui_draw_switch, .params = &switch_quiet, .fill = -1},  {.draw = dgx_ui_draw_slider, .params = &slider_quiet, .fill = -1},
            {.draw = dgx_ui_draw_radio, .params = &check_quiet, .fill = -1},    {.draw = dgx_ui_draw_checkbox, .params = &check_quiet, .fill = -1},
        };
        for (size_t c = 0; c < sizeof cases / sizeof *cases; ++c) {
            dgx_ui_widget_t quiet_w = cases[c].widget[0];
            quiet_w.look            = &quiet_looks[c];
            static dgx_ui_node_t nodes[2];
            dgx_ui_t             ui;
            dgx_ui_init(&ui, nodes, 2, 1);
            dgx_ui_add(&ui, -1, &column_w);
            const int i = dgx_ui_add(&ui, 0, &quiet_w);
            paint_anew();
            if (pictured) dgx_ui_background_set(&paint, draw_bands, NULL);
            dgx_ui_paint(&ui, &paint, 0);
            keep();
            dgx_ui_focus(&ui, i);
            panel_count_from_now(panel);
            dgx_ui_paint(&ui, &paint, 10);
            /* over a drawn background straight letters are drawn over the picture laid for their field, twice as the
               painter means them: there only the picture is compared */
            const int changed = changed_in(dgx_ui_node(&ui, i));
            CHECK((pictured && PANEL_BITS == 16 ? true : panel_twice(panel) == 0) && changed == 0);
            if (changed) fprintf(stderr, "  the %s quiet%s: %d pixels show the focus\n", cases[c].name, pictured ? " over the background" : "", changed);
            dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
        }
    }
    {
        static const dgx_ui_button_look_t quiet = {.face = 2, .pressed = 3, .focused = 2, .disabled = 0, .text = 1, .text_disabled = 2, .shadow = 0, .depth = 0};
        static const dgx_ui_look_t        quiet_look = {.draw = dgx_ui_draw_button, .params = &quiet, .fill = -1};
        static const dgx_ui_widget_t      quiet_w    = {.kind = DGX_UI_BUTTON, .want_h = 14, .text = &go_box, .look = &quiet_look};
        static dgx_ui_node_t              nodes[2];
        dgx_ui_t                          ui;
        dgx_ui_init(&ui, nodes, 2, 1);
        dgx_ui_add(&ui, -1, &column_w);
        const int i = dgx_ui_add(&ui, 0, &quiet_w);
        paint_anew();
        dgx_ui_paint(&ui, &paint, 0);
        keep();
        dgx_ui_focus(&ui, i);
        CHECK(pass(&ui, 10, dgx_ui_node(&ui, i), NULL) && changed_in(dgx_ui_node(&ui, i)) == 0);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }

    /* a slider of two points: its knob, the whole of it, is all border with the focus */
    {
        static const dgx_ui_widget_t tiny_w = {.kind = DGX_UI_SLIDER, .value = &level, .max = 10, .want_w = 2, .want_h = 2, .look = &dgx_ui_look_slider};
        static dgx_ui_node_t         nodes[2];
        dgx_ui_t                     ui;
        dgx_ui_init(&ui, nodes, 2, 1);
        dgx_ui_add(&ui, -1, &(const dgx_ui_widget_t){.kind = DGX_UI_BOX, .pad = 2});
        const int i = dgx_ui_add(&ui, 0, &tiny_w);
        paint_anew();
        dgx_ui_paint(&ui, &paint, 0);
        dgx_ui_focus(&ui, i);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, i);
        CHECK(n->w == 2 && n->h == 2 && pass(&ui, 10, n, NULL));
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) CHECK(panel->mem[y][x] == ink);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }

    /* a widget half under a popup: framed where it shows, every pixel once, the popup left alone */
    {
        static dgx_ui_node_t         nodes[3], popup_nodes[1];
        static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .look = &dgx_ui_look_frame};
        dgx_ui_t                     ui, popup;
        dgx_ui_init(&ui, nodes, 3, 1);
        dgx_ui_add(&ui, -1, &column_w);
        const int i = dgx_ui_add(&ui, 0, &cases[0].widget[0]);
        paint_anew();
        dgx_ui_paint(&ui, &paint, 0);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, i);
        dgx_ui_init(&popup, popup_nodes, 1, 1), dgx_ui_add(&popup, -1, &frame_w);
        CHECK(dgx_ui_popup_open(&ui, &popup, n->x + n->w / 2, 0, PANEL_W - n->x - n->w / 2, PANEL_H, 0));
        dgx_ui_paint(&ui, &paint, 10);
        dgx_ui_focus(&ui, i);
        CHECK(pass(&ui, 20, n, NULL) && panel_written_outside(panel, n->x, n->y, n->x + n->w / 2, n->y + n->h) == 0);
        CHECK(panel->mem[n->y][n->x] == ink && panel->mem[n->y + 3][n->x] == ink);
        dgx_ui_popup_close(&ui);
        CHECK(pass(&ui, 30, dgx_ui_node(&ui, 0), NULL)); /* the popup's place composed again */
        dgx_ui_free(&popup);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }
    /* at points of 1, 2 and 3, each look at its own height (a switch and a slider, which have no text, at 7 points):
       paper between the focus's mark and what it marks, by the default colours; the inside unchanged */
    for (int point = 1; point <= 3; ++point)
        for (size_t c = 0; c < sizeof cases / sizeof *cases; ++c) {
            dgx_ui_widget_t own = cases[c].widget[0];
            own.want_h          = own.text ? 0 : 7;
            if (own.kind == DGX_UI_SWITCH && !own.text) own.want_w = 14;
            static dgx_ui_node_t nodes[2];
            dgx_ui_t             ui;
            dgx_ui_init(&ui, nodes, 2, point);
            dgx_ui_add(&ui, -1, &(const dgx_ui_widget_t){.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1});
            const int i = dgx_ui_add(&ui, 0, &own);
            paint_anew();
            dgx_ui_paint(&ui, &paint, 0);
            const dgx_ui_node_t *n    = dgx_ui_node(&ui, i);
            const bool           knob = own.kind == DGX_UI_SLIDER || (own.kind == DGX_UI_SWITCH && !own.text);
            if (own.text) CHECK(n->h == dgx_ui_textbox_height(&ui, own.text) + 2 * point); /* the frame's room */
            keep();
            dgx_ui_focus(&ui, i);
            CHECK(pass(&ui, 10, n, NULL));
            const dgx_ui_rect_t ring = knob ? bounds_of(n, ink) : framed_rect(&ui, n, cases[c].face);
            const int           near = touching(n, ring, point, ink);
            CHECK(!dgx_ui_rect_empty(&ring) && near == 0);
            if (near) fprintf(stderr, "  the %s at a point of %d: %d pixels of the ink touch the focus's mark\n", cases[c].name, point, near);
            dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
        }

    /* at points of 1, 2 and 3, a mark with no text in a box narrower than it is high, and in a square one: nothing of
       the plain mark where the frame lies, and paper between the frame and the mark */
    for (int point = 1; point <= 3; ++point) {
        static const dgx_ui_widget_t narrow[3] = {
            {.kind = DGX_UI_RADIO, .value = &choice, .step = 1, .want_w = 6, .want_h = 10, .look = &dgx_ui_look_radio},
            {.kind = DGX_UI_SWITCH, .value = &ticked, .want_w = 5, .want_h = 12, .look = &dgx_ui_look_checkbox},
            {.kind = DGX_UI_RADIO, .value = &choice, .step = 1, .want_w = 8, .want_h = 8, .look = &dgx_ui_look_radio},
        };
        for (int k = 0; k < 3; ++k) {
            static dgx_ui_node_t nodes[2];
            dgx_ui_t             ui;
            dgx_ui_init(&ui, nodes, 2, point);
            dgx_ui_add(&ui, -1, &(const dgx_ui_widget_t){.kind = DGX_UI_BOX, .pad = 1});
            const int i = dgx_ui_add(&ui, 0, &narrow[k]);
            ticked      = 1;
            paint_anew();
            dgx_ui_paint(&ui, &paint, 0);
            const dgx_ui_node_t *n    = dgx_ui_node(&ui, i);
            const dgx_ui_rect_t  ring = dgx_ui_rect(n->x, n->y, n->w, n->h);
            int                  under = 0;
            for (int y = n->y; y < n->y + n->h; ++y)
                for (int x = n->x; x < n->x + n->w; ++x) under += in_ring(ring, point, x, y) && panel->mem[y][x] == ink;
            dgx_ui_focus(&ui, i);
            CHECK(pass(&ui, 10, n, NULL));
            const int near = touching(n, ring, point, ink);
            CHECK(under == 0 && near == 0);
            if (under || near) fprintf(stderr, "  narrow mark %d at a point of %d: %d of its ink where the frame lies, %d touching it\n", k, point, under, near);
            dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
        }
    }
    ticked = 0;
    CHECK_DONE();
}
