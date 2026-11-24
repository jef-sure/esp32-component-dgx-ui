/*
 * What changed, told and not drawn (dgx_ui_changed()): two trees alike, one painted by dgx_ui_paint() on a counting
 * panel, the other told on a counting panel of its own, each rectangle told composed by dgx_ui_paint_area_to() into a
 * piece and put on a mirror, as another compositor's strips would. At every step: nothing written on the telling
 * panel, every pixel the painter wrote inside a rectangle told, the mirror equal to the painted panel. The first pass
 * tells the root, a second call at once nothing; a slider moved its box; a label added to a frame the frame alone; a
 * widget removed its parent; a widget hidden and shown; dgx_ui_redraw() of one; the screen turned the root. A look
 * with time told at every call, its moment come or not, the wake the painter's; a canvas made and kept while told. A
 * widget wholly under the band not told, its marks kept, told with the band's place when it goes; half under a popup
 * its two parts, the popup's place when it closes, nothing composed where it lay beside a root smaller than the
 * screen. The strips and a drawn background named: neither used while told,
 * the strips given back. The function told composing at once, a popup over the slider and letters coming: the same
 * mirror, more and wake. dgx_ui_paint() after it writes nothing. A call inside the function of another: each told its
 * own. Nobody told: the tree marked drawn.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;

/* a look of the program's own that adds a pixel to its canvas at every call, where the time says, and puts it */
static int canvas_calls;
static bool draw_canvas(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    bool          made;
    dgx_screen_t *canvas = dgx_ui_canvas(paint, ui, i, &made);
    if (canvas) {
        if (made) dgx_fill_rectangle(canvas, 0, 0, canvas->width, canvas->height, 0);
        if (!paint->composing) dgx_set_pixel(canvas, (int)(now_ms / 10 % (uint32_t)canvas->width), 0, 1); /* a composing stores nothing */
    }
    ++canvas_calls;
    dgx_ui_canvas_put(paint, ui, i, -1);
    return false;
}
static const dgx_ui_look_t canvas_look = {.draw = draw_canvas, .fill = -1};

/* a look of the program's own that asks for time at every call and names no moment, its paper laid; the part and the
   parts of each call of a pass (not of a composing) written down, a log for each of the two trees */
static int  ticking_log[2][256], ticking_count[2];
static bool draw_ticking(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const int            log  = look->params == NULL ? 0 : 1;
    const dgx_ui_node_t *node = dgx_ui_node(ui, i);
    if (!paint->composing && ticking_count[log] < 256) ticking_log[log][ticking_count[log]++] = paint->part * 16 + paint->parts;
    dgx_ui_lay_paper(paint, DGX_UI_PAPER, node->x, node->y, node->w, node->h);
    return true;
}
static int                 ticking_tag;
static const dgx_ui_look_t ticking_looks[2] = {{.draw = draw_ticking, .fill = -1}, {.draw = draw_ticking, .fill = -1, .params = &ticking_tag}};
static const dgx_ui_widget_t ticking_w[2]   = {{.kind = DGX_UI_OWN, .want_h = 1, .look = &ticking_looks[0]}, {.kind = DGX_UI_OWN, .want_h = 1, .look = &ticking_looks[1]}};

typedef struct {
    dgx_ui_node_t    nodes[10];
    dgx_ui_t         ui;
    char             label_buf[24], lines_buf[64], added_buf[16];
    dgx_ui_textbox_t label_box, lines_box, added_box;
    dgx_ui_widget_t  label_w, lines_w, added_w;
    int              frame, label, slider, lines, button, canvas;
} tree_t;

/* 1 + 30 + 1 + 6 + 1 + 13 + 1 + 4 + 1 + 3 + 1 = 62 rows of the panel's 64: a frame that holds two labels, a slider,
   the lines, a button, a canvas */
static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1, .gap = 1};
static const dgx_ui_widget_t frame_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .want_h = 30};
static int32_t slider_w_place; /* its value */
static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .value = &slider_w_place, .max = 100, .want_h = 6, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t button_w = {.kind = DGX_UI_BUTTON, .want_h = 4, .look = &dgx_ui_look_button};
static int32_t canvas_w_place; /* its value */
static const dgx_ui_widget_t canvas_w = {.kind = DGX_UI_OWN, .value = &canvas_w_place, .flags = DGX_UI_CANVAS, .want_h = 3, .look = &canvas_look};

static void make(tree_t *tree)
{
    memset(tree, 0, sizeof *tree);
    dgx_ui_textbox_init(&tree->label_box, tree->label_buf, sizeof tree->label_buf, 1, &style);
    dgx_ui_textbox_init(&tree->lines_box, tree->lines_buf, sizeof tree->lines_buf, 1, &style);
    dgx_ui_textbox_init(&tree->added_box, tree->added_buf, sizeof tree->added_buf, 1, &style);
    tree->label_w = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &tree->label_box, .want_h = 13};
    tree->lines_w = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &tree->lines_box, .want_h = 13, .look = &dgx_ui_look_lines};
    tree->added_w = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &tree->added_box, .want_h = 13};
    dgx_ui_init(&tree->ui, tree->nodes, 10, 1);
    dgx_ui_add(&tree->ui, -1, &root_w);
    tree->frame  = dgx_ui_add(&tree->ui, 0, &frame_w);
    tree->label  = dgx_ui_add(&tree->ui, tree->frame, &tree->label_w);
    tree->slider = dgx_ui_add(&tree->ui, 0, &slider_w);
    tree->lines  = dgx_ui_add(&tree->ui, 0, &tree->lines_w);
    tree->button = dgx_ui_add(&tree->ui, 0, &button_w);
    tree->canvas = dgx_ui_add(&tree->ui, 0, &canvas_w);
    dgx_ui_text_set(&tree->ui, tree->label, "21.5");
    dgx_ui_text_set(&tree->ui, tree->lines, "Hello");
}

typedef struct {
    dgx_ui_rect_t at[32];
    int           count;
} told_t;

static void collect(void *ctx, dgx_ui_rect_t rect)
{
    told_t *told = ctx;
    if (told->count < 32) told->at[told->count++] = rect;
}

static panel_t       *painted_panel, *telling_panel;
static dgx_screen_t  *mirror;
static tree_t         painted, told_tree;
static dgx_ui_paint_t painted_paint, telling_paint;
static told_t         told;
static bool           painted_more, told_more;

/* a rectangle told, composed into a piece of its size and put on the mirror at its place */
static void compose(dgx_ui_rect_t rect)
{
    dgx_screen_t *piece = dgx_vscreen_init(rect.w, rect.h, 16, DgxScreenRGB);
    dgx_ui_paint_area_to(&told_tree.ui, &telling_paint, piece, NULL, rect.x, rect.y, rect.w, rect.h);
    dgx_vscreen_region_to_screen(mirror, rect.x, rect.y, piece, 0, 0, rect.w, rect.h, 0);
    dgx_screen_destroy(&piece);
}

static bool told_holds(int x, int y)
{
    for (int k = 0; k < told.count; ++k) {
        const dgx_ui_rect_t *r = &told.at[k];
        if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h) return true;
    }
    return false;
}

/* told and composed at once, as a compositor with a frame buffer does: the rectangle, or the whole screen again */
static void compose_at_once(void *ctx, dgx_ui_rect_t rect)
{
    collect(ctx, rect);
    compose(rect);
}
static void compose_all_at_once(void *ctx, dgx_ui_rect_t rect)
{
    collect(ctx, rect);
    compose(dgx_ui_rect(0, 0, PANEL_W, PANEL_H));
}

/* a pass of both trees, the rectangles told composed after the call or at once: the faults, pixels written on the
   telling panel, painted outside every rectangle told, or unlike on the mirror */
enum { AFTER, AT_ONCE, ALL_AT_ONCE };
static int pass_with(uint32_t now_ms, int when)
{
    static const dgx_ui_changed_fn tells[3] = {collect, compose_at_once, compose_all_at_once};
    panel_count_from_now(painted_panel), panel_count_from_now(telling_panel);
    told.count   = 0;
    painted_more = dgx_ui_paint(&painted.ui, &painted_paint, now_ms);
    told_more    = dgx_ui_changed(&told_tree.ui, &telling_paint, now_ms, tells[when], &told);
    for (int k = 0; when == AFTER && k < told.count; ++k) compose(told.at[k]);
    int faults = panel_written(telling_panel);
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            faults += painted_panel->writes[y][x] && !told_holds(x, y);
            faults += painted_panel->mem[y][x] != dgx_get_pixel(mirror, x, y);
        }
    return faults;
}
static int pass(uint32_t now_ms) { return pass_with(now_ms, AFTER); }

static bool same_rect(dgx_ui_rect_t a, const dgx_ui_node_t *node)
{
    return a.x == node->x && a.y == node->y && a.w == node->w && a.h == node->h;
}

/* told inside a function of the program: another tree told by another function */
static tree_t         inner_tree;
static dgx_ui_paint_t inner_paint;
static told_t         inner_told, outer_told;
static void           collect_inner(void *ctx, dgx_ui_rect_t rect) { collect(ctx, rect); }
static void           collect_outer(void *ctx, dgx_ui_rect_t rect)
{
    collect(ctx, rect);
    if (outer_told.count == 1) dgx_ui_changed(&inner_tree.ui, &inner_paint, 0, collect_inner, &inner_told);
}

/* the program's picture under the tree: a colour of none of the table's, laid where asked; counted */
static int  background_calls;
static void draw_background(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    ++background_calls;
    dgx_fill_rectangle(paint->scr, x - paint->ox, y - paint->oy, w, h, 0x1234);
}

int main(void)
{
    variant[0]    = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style         = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
    painted_panel = panel_make(), telling_panel = panel_make();
    mirror        = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    make(&painted), make(&told_tree);
    painted_paint = (dgx_ui_paint_t){.scr = &painted_panel->sbus.scr, .colors = colors, .color_count = 4};
    telling_paint = (dgx_ui_paint_t){.scr = &telling_panel->sbus.scr, .colors = colors, .color_count = 4};
    tree_t *const both[2] = {&painted, &told_tree};

    /* the first pass: the root, one rectangle; a second call at once: nothing */
    CHECK(pass(0) == 0 && told.count == 1 && same_rect(told.at[0], dgx_ui_node(&told_tree.ui, 0)));
    CHECK(pass(1) == 0 && told.count == 0);

    /* a slider moved: its box alone */
    for (int k = 0; k < 2; ++k) dgx_ui_set_value(&both[k]->ui, both[k]->slider, 40);
    CHECK(pass(2) == 0 && told.count == 1 && same_rect(told.at[0], dgx_ui_node(&told_tree.ui, told_tree.slider)));

    /* a label added to the frame: the frame's box, not the label's as well */
    for (int k = 0; k < 2; ++k) dgx_ui_text_set(&both[k]->ui, dgx_ui_add(&both[k]->ui, both[k]->frame, &both[k]->added_w), "new");
    CHECK(pass(3) == 0 && told.count == 1 && same_rect(told.at[0], dgx_ui_node(&told_tree.ui, told_tree.frame)));

    /* the button removed: its parent's box, the root's */
    for (int k = 0; k < 2; ++k) dgx_ui_remove(&both[k]->ui, both[k]->button);
    CHECK(pass(4) == 0 && told.count == 1 && same_rect(told.at[0], dgx_ui_node(&told_tree.ui, 0)));

    /* the slider hidden, then shown; dgx_ui_redraw() of the label */
    for (int k = 0; k < 2; ++k) dgx_ui_show(&both[k]->ui, both[k]->slider, false);
    CHECK(pass(5) == 0 && told.count == 1 && same_rect(told.at[0], dgx_ui_node(&told_tree.ui, 0)));
    for (int k = 0; k < 2; ++k) dgx_ui_show(&both[k]->ui, both[k]->slider, true);
    CHECK(pass(6) == 0 && told.count == 1 && same_rect(told.at[0], dgx_ui_node(&told_tree.ui, 0)));
    for (int k = 0; k < 2; ++k) dgx_ui_redraw(&both[k]->ui, both[k]->label);
    CHECK(pass(7) == 0 && told.count == 1 && same_rect(told.at[0], dgx_ui_node(&told_tree.ui, told_tree.label)));

    /* a look with time: the lines' letters coming, 100 ms each, a call every 30 ms, so most come before a letter's
       moment: told at every call all the same, the wake as the painter's */
    for (int k = 0; k < 2; ++k) dgx_ui_text_appear(&both[k]->ui, both[k]->lines, 100);
    int lines_off = 0, lines_calls = 0;
    for (uint32_t now_ms = 1000; now_ms < 1700; now_ms += 30, ++lines_calls) {
        lines_off += pass(now_ms);
        const bool lines_told = told.count == 1 && same_rect(told.at[0], dgx_ui_node(&told_tree.ui, told_tree.lines));
        lines_off += (told_more ? !lines_told : 0) + (told_more != painted_more) + (telling_paint.wake_ms != painted_paint.wake_ms);
    }
    CHECK(lines_calls > 15 && lines_off == 0 && !told_more);

    /* the canvas: made and written at every call while told, nothing on the screen */
    CHECK(telling_paint.canvases != NULL);
    canvas_calls = 0;
    for (int k = 0; k < 2; ++k) dgx_ui_redraw(&both[k]->ui, both[k]->canvas);
    CHECK(pass(2000) == 0 && canvas_calls >= 2 && told.count == 1);

    /* the band over the slider: a change under it not told, its marks kept; told with the band's place when it goes */
    const dgx_ui_node_t *slider = dgx_ui_node(&told_tree.ui, told_tree.slider);
    painted_paint.cover = telling_paint.cover = dgx_ui_rect(0, slider->y, PANEL_W, slider->h);
    for (int k = 0; k < 2; ++k) dgx_ui_set_value(&both[k]->ui, both[k]->slider, 70);
    panel_count_from_now(telling_panel);
    told.count = 0;
    dgx_ui_paint(&painted.ui, &painted_paint, 2100);
    dgx_ui_changed(&told_tree.ui, &telling_paint, 2100, collect, &told);
    CHECK(told.count == 0 && panel_written(telling_panel) == 0 && (dgx_ui_node(&told_tree.ui, told_tree.slider)->marks || slider->state_drawn.value != 70));
    painted_paint.cover = telling_paint.cover = dgx_ui_rect(0, 0, 0, 0);
    CHECK(pass(2200) == 0 && told.count >= 1 && told.at[0].y == slider->y && told.at[0].w == PANEL_W && slider->state_drawn.value == 70);

    /* a popup over the middle of the slider: a change of it told in its two parts; the popup's place when it closes */
    static dgx_ui_node_t popup_nodes[2][1];
    static dgx_ui_t      popups[2];
    for (int k = 0; k < 2; ++k) {
        dgx_ui_init(&popups[k], popup_nodes[k], 1, 1);
        dgx_ui_add(&popups[k], -1, &frame_w);
        CHECK(dgx_ui_popup_open(&both[k]->ui, &popups[k], 30, slider->y - 2, 20, slider->h + 4, 0));
    }
    CHECK(pass(2300) == 0 && told.count == 1); /* the popup, drawn the first time */
    for (int k = 0; k < 2; ++k) dgx_ui_set_value(&both[k]->ui, both[k]->slider, 20);
    CHECK(pass(2400) == 0 && told.count == 2 && told.at[0].x + told.at[0].w == 30 && told.at[1].x == 50);
    for (int k = 0; k < 2; ++k) dgx_ui_popup_close(&both[k]->ui);
    CHECK(pass(2500) == 0 && told.count == 1 && told.at[0].x == 30 && told.at[0].w == 20);
    for (int k = 0; k < 2; ++k) dgx_ui_free(&popups[k]);

    /* the function told composing at once, as a compositor with a frame buffer does: the rectangle, then the whole
       screen again. The lines' letters coming and the slider changing meanwhile, a popup over the middle of all below
       the frame, a look of the program's own at the bottom asking for time and naming no moment. At every call the
       mirror, more and wake_ms as the painter's, nothing on the telling panel, nothing told under the popup but the
       popup itself; the own look called by the pass with the same parts in both trees */
    {
        static dgx_ui_node_t at_once_nodes[2][1];
        static dgx_ui_t      at_once_popups[2];
        int                  ticking[2];
        ticking_count[0] = ticking_count[1] = 0;
        for (int k = 0; k < 2; ++k) ticking[k] = dgx_ui_add(&both[k]->ui, 0, &ticking_w[k]);
        const dgx_ui_rect_t popup_rect = dgx_ui_rect(30, slider->y - 1, 20, PANEL_H - slider->y + 1);
        int                 at_once_off = 0, under = 0, calls = 0;
        for (int when = AT_ONCE; when <= ALL_AT_ONCE; ++when) {
            for (int k = 0; k < 2; ++k) {
                dgx_ui_init(&at_once_popups[k], at_once_nodes[k], 1, 1);
                dgx_ui_add(&at_once_popups[k], -1, &frame_w);
                CHECK(dgx_ui_popup_open(&both[k]->ui, &at_once_popups[k], popup_rect.x, popup_rect.y, popup_rect.w, popup_rect.h, 0));
                dgx_ui_text_appear(&both[k]->ui, both[k]->lines, 50);
            }
            const uint32_t start = 3000 + (uint32_t)(when - AT_ONCE) * 1000;
            for (uint32_t now_ms = start; now_ms < start + 700; now_ms += 20, ++calls) {
                if (calls % 5 == 0)
                    for (int k = 0; k < 2; ++k) dgx_ui_set_value(&both[k]->ui, both[k]->slider, calls * 3 % 100);
                at_once_off += pass_with(now_ms, when);
                at_once_off += (told_more != painted_more) + (telling_paint.wake_ms != painted_paint.wake_ms);
                for (int k = 0; k < told.count; ++k) {
                    dgx_ui_rect_t common;
                    const bool    popup_itself = told.at[k].x == popup_rect.x && told.at[k].w == popup_rect.w && told.at[k].y == popup_rect.y;
                    under += !popup_itself && dgx_ui_rect_intersect(&told.at[k], &popup_rect, &common);
                }
            }
            for (int k = 0; k < 2; ++k) dgx_ui_popup_close(&both[k]->ui), dgx_ui_free(&at_once_popups[k]);
            CHECK(pass_with(start + 800, when) == 0);
        }
        CHECK(calls > 60 && at_once_off == 0 && under == 0 && !told_more == !painted_more);
        CHECK(ticking_count[0] > 60 && ticking_count[0] == ticking_count[1] && memcmp(ticking_log[0], ticking_log[1], sizeof ticking_log[0]) == 0);
        for (int k = 0; k < 2; ++k) dgx_ui_remove(&both[k]->ui, ticking[k]);
        CHECK(pass(4900) == 0);
    }

    /* a root given less than the screen, a popup lying partly below it closed, then the band: their places told,
       nothing composed, so not a pixel of the paper around the root written */
    {
        static tree_t        short_tree;
        static dgx_ui_t      low_popup;
        static dgx_ui_node_t low_nodes[1];
        dgx_ui_paint_t       short_paint = {.scr = &telling_panel->sbus.scr, .colors = colors, .color_count = 4};
        make(&short_tree);
        dgx_ui_set_root(&short_tree.ui, 0, 0, PANEL_W, 40);
        dgx_ui_init(&low_popup, low_nodes, 1, 1);
        dgx_ui_add(&low_popup, -1, &frame_w);
        told_t short_told = {0};
        dgx_ui_changed(&short_tree.ui, &short_paint, 0, collect, &short_told);
        CHECK(dgx_ui_popup_open(&short_tree.ui, &low_popup, 10, 30, 40, 20, 0));
        dgx_ui_changed(&short_tree.ui, &short_paint, 1, collect, &short_told);
        dgx_ui_popup_close(&short_tree.ui);
        short_told.count = 0;
        panel_count_from_now(telling_panel);
        dgx_ui_changed(&short_tree.ui, &short_paint, 2, collect, &short_told);
        CHECK(short_told.count == 1 && short_told.at[0].x == 10 && short_told.at[0].y == 30 && short_told.at[0].h == 20 && panel_written(telling_panel) == 0);
        /* the band the same way: lying partly below the root, gone */
        short_paint.cover = dgx_ui_rect(0, 30, PANEL_W, 20);
        dgx_ui_changed(&short_tree.ui, &short_paint, 3, collect, &short_told);
        short_paint.cover = dgx_ui_rect(0, 0, 0, 0);
        short_told.count  = 0;
        panel_count_from_now(telling_panel);
        dgx_ui_changed(&short_tree.ui, &short_paint, 4, collect, &short_told);
        CHECK(short_told.count == 1 && short_told.at[0].y == 30 && short_told.at[0].w == PANEL_W && panel_written(telling_panel) == 0);
        dgx_ui_paint_free(&short_paint), dgx_ui_free(&low_popup), dgx_ui_free(&short_tree.ui);
    }

    /* the strips named: not used while told, given back; a drawn background: its function not called while told */
    static uint8_t buffer[PANEL_W * 2 * 8];
    painted_paint.buffer = telling_paint.buffer = buffer, painted_paint.buffer_size = telling_paint.buffer_size = sizeof buffer;
    telling_paint.strips = &dgx_ui_strips;
    for (int k = 0; k < 2; ++k) dgx_ui_set_value(&both[k]->ui, both[k]->slider, 91);
    CHECK(pass(5600) == 0 && told.count == 1 && telling_paint.strips == &dgx_ui_strips);
    telling_paint.strips = NULL;
    dgx_ui_background_set(&painted_paint, draw_background, NULL), dgx_ui_background_set(&telling_paint, draw_background, NULL);
    for (int k = 0; k < 2; ++k) dgx_ui_set_value(&both[k]->ui, both[k]->slider, 10);
    panel_count_from_now(telling_panel);
    background_calls = 0, told.count = 0;
    dgx_ui_changed(&told_tree.ui, &telling_paint, 5700, collect, &told);
    CHECK(told.count == 1 && background_calls == 0 && panel_written(telling_panel) == 0);
    dgx_ui_paint(&painted.ui, &painted_paint, 5700);
    for (int k = 0; k < told.count; ++k) compose(told.at[k]);
    int unlike = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) unlike += painted_panel->mem[y][x] != dgx_get_pixel(mirror, x, y);
    CHECK(unlike == 0 && background_calls > 0); /* composed, the picture laid */

    /* the screen turned: the root, one rectangle */
    telling_panel->sbus.scr.dir_x = !telling_panel->sbus.scr.dir_x;
    told.count                    = 0;
    panel_count_from_now(telling_panel);
    dgx_ui_changed(&told_tree.ui, &telling_paint, 5800, collect, &told);
    CHECK(told.count == 1 && same_rect(told.at[0], dgx_ui_node(&told_tree.ui, 0)) && panel_written(telling_panel) == 0);

    /* dgx_ui_paint() after it on the same paint: nothing left to draw */
    dgx_ui_set_value(&told_tree.ui, told_tree.slider, 55);
    dgx_ui_changed(&told_tree.ui, &telling_paint, 5900, collect, &told);
    panel_count_from_now(telling_panel);
    dgx_ui_paint(&told_tree.ui, &telling_paint, 5900);
    CHECK(panel_written(telling_panel) == 0);

    /* a call inside the function of another, each its own */
    make(&inner_tree);
    inner_paint = (dgx_ui_paint_t){.scr = &telling_panel->sbus.scr, .colors = colors, .color_count = 4};
    dgx_ui_set_value(&told_tree.ui, told_tree.slider, 33);
    outer_told.count = inner_told.count = 0;
    dgx_ui_changed(&told_tree.ui, &telling_paint, 6000, collect_outer, &outer_told);
    CHECK(outer_told.count == 1 && same_rect(outer_told.at[0], dgx_ui_node(&told_tree.ui, told_tree.slider)));
    CHECK(inner_told.count == 1 && same_rect(inner_told.at[0], dgx_ui_node(&inner_tree.ui, 0)));

    /* nobody told: the tree marked drawn all the same, a call after it tells nothing */
    dgx_ui_set_value(&told_tree.ui, told_tree.slider, 77);
    panel_count_from_now(telling_panel);
    dgx_ui_changed(&told_tree.ui, &telling_paint, 6100, NULL, NULL);
    told.count = 0;
    dgx_ui_changed(&told_tree.ui, &telling_paint, 6200, collect, &told);
    CHECK(told.count == 0 && panel_written(telling_panel) == 0);

    dgx_ui_paint_free(&painted_paint), dgx_ui_paint_free(&telling_paint), dgx_ui_paint_free(&inner_paint);
    dgx_ui_free(&painted.ui), dgx_ui_free(&told_tree.ui), dgx_ui_free(&inner_tree.ui);
    mirror->destroy(&mirror);
    free(painted_panel), free(telling_panel);
    CHECK_DONE();
}
