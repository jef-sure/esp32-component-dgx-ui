/*
 * The caret of a field that blinks (dgx_ui_input_look_t's blink), on a panel of 16 bits and of 18 (PANEL_BITS), over
 * five periods: drawn with the field when the focus comes, then at each half period its rectangle alone, of the
 * caret's colour or the paper, nothing else written, and in between nothing; the loop woken by the paint for the next
 * toggle. A letter typed between two: the caret seen at once, its phase from there. Not focused, hidden, under a modal
 * popup, wholly under the keyboard's band: no caret and no time asked; the band gone, drawn by its phase. blink 0: a
 * steady caret, no time asked. A secret field the same. A mirror composed by a second paint at each pass: the same
 * picture, and the phase it leaves the first paint's. At 18 bits through the strips: the field sent whole and once.
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#define PANEL_W 120
#define PANEL_H 60
#include "panel.h"

static uint32_t colors[4];
enum { PAPER = 0, INK = 1, CARET = 3 };

static const dgx_ui_font_variant_t variant[1] = {{.font = &TerminusTTFMedium12}};
static const dgx_ui_text_style_t   style      = {.fonts = variant, .count = 1};
static dgx_ui_input_look_t         params     = {.ink = INK, .caret = CARET, .paper = DGX_UI_PAPER, .blink = 50};
static const dgx_ui_look_t         look       = {.draw = dgx_ui_draw_input, .params = &params, .fill = -1};
static char                        text[16];
static dgx_ui_textbox_t            box;
static dgx_ui_node_t               nodes[4];
static dgx_ui_t                    ui;
static int                         field, other;

static void make(void)
{
    static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
    static const dgx_ui_widget_t field_w  = {.kind = DGX_UI_INPUT, .want_h = 16, .text = &box, .look = &look};
    static const dgx_ui_widget_t button_w = {.kind = DGX_UI_BUTTON, .want_h = 16, .look = &dgx_ui_look_button};
    dgx_ui_textbox_init(&box, text, sizeof text, 1, &style);
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_add(&ui, -1, &root_w);
    field = dgx_ui_add(&ui, 0, &field_w);
    other = dgx_ui_add(&ui, 0, &button_w);
    dgx_ui_text_set(&ui, field, "ab");
}

/* the rectangle a pass wrote, and whether it is all it wrote: one rectangle a point wide inside the field */
static bool wrote_caret(panel_t *p, dgx_ui_rect_t *r)
{
    int x0 = PANEL_W, y0 = PANEL_H, x1 = -1, y1 = -1, n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x)
            if (p->writes[y][x]) {
                ++n, x0 = x < x0 ? x : x0, y0 = y < y0 ? y : y0, x1 = x > x1 ? x : x1, y1 = y > y1 ? y : y1;
                if (p->writes[y][x] > 1) return false;
            }
    *r = dgx_ui_rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    const dgx_ui_node_t *f = dgx_ui_node(&ui, field);
    return n && n == r->w * r->h && r->w == 1 && r->x >= f->x && r->x < f->x + f->w && r->y >= f->y && r->y + r->h <= f->y + f->h;
}

static bool all_of(panel_t *p, dgx_ui_rect_t r, uint32_t colour)
{
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x)
            if (p->mem[y][x] != colour) return false;
    return true;
}

int main(void)
{
    const uint32_t rgb[4] = {0x102030, 0xe0e0e0, 0x405060, 0xff8000};
    panel_t       *panel  = panel_make();
    dgx_screen_t  *scr    = &panel->sbus.scr;
    for (int k = 0; k < 4; ++k) colors[k] = dgx_screen_rgb(scr, (uint8_t)(rgb[k] >> 16), (uint8_t)(rgb[k] >> 8), (uint8_t)rgb[k]);
    make();
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 4};
    dgx_ui_paint(&ui, &paint, 100);
    CHECK(paint.wake_ms == DGX_UI_WAKE_NONE); /* not focused: no time */
    /* the focus comes at 1000: the caret at once, the next toggle asked at 1500 */
    dgx_ui_focus(&ui, field);
    dgx_ui_paint(&ui, &paint, 1000);
    CHECK(paint.wake_ms == 500);
    /* five periods: between toggles nothing, at each the caret's rectangle alone, of the caret or the paper */
    dgx_ui_rect_t caret = {0}, again;
    int           toggles = 0, between = 0, steady = 0;
    for (uint32_t now = 1250; now <= 6000; now += 250) {
        panel_count_from_now(panel);
        const bool more = dgx_ui_paint(&ui, &paint, now);
        const bool on   = (now - 1000) / 500 % 2 == 0;
        if (now % 500 == 0) {
            const bool alone = wrote_caret(panel, &again);
            if (!toggles) caret = again;
            toggles += alone && again.x == caret.x && again.y == caret.y && again.h == caret.h && all_of(panel, caret, on ? colors[CARET] : colors[PAPER]);
        } else {
            between += panel_written(panel) == 0;
        }
        steady += more && paint.wake_ms == 500 - (now - 1000) % 500;
    }
    CHECK(toggles == 10 && between == 10 && steady == 20);
    /* a letter at 6100, the caret off since 6000: seen at once, the phase from there */
    dgx_ui_text_set(&ui, field, "abc");
    dgx_ui_paint(&ui, &paint, 6100);
    dgx_ui_rect_t moved;
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &paint, 6600);
    CHECK(wrote_caret(panel, &moved) && moved.x > caret.x && all_of(panel, moved, colors[PAPER]) && paint.wake_ms == 500);
    /* the focus gone: no caret, no time */
    dgx_ui_focus(&ui, other);
    CHECK(!dgx_ui_paint(&ui, &paint, 6700) && paint.wake_ms == DGX_UI_WAKE_NONE && all_of(panel, moved, colors[PAPER]));
    dgx_ui_focus(&ui, field);
    dgx_ui_paint(&ui, &paint, 7000);
    CHECK(all_of(panel, moved, colors[CARET]) && paint.wake_ms == 500);
    /* hidden: no time */
    dgx_ui_show(&ui, field, false);
    CHECK(!dgx_ui_paint(&ui, &paint, 7100) && paint.wake_ms == DGX_UI_WAKE_NONE);
    dgx_ui_show(&ui, field, true), dgx_ui_focus(&ui, field);
    dgx_ui_paint(&ui, &paint, 7200);
    /* under a modal popup: no focus shown, no caret, no time */
    static dgx_ui_node_t pop_nodes[2];
    static dgx_ui_t      pop;
    static const dgx_ui_widget_t pop_w = {.kind = DGX_UI_BOX};
    dgx_ui_init(&pop, pop_nodes, 2, 1);
    dgx_ui_add(&pop, -1, &pop_w);
    CHECK(dgx_ui_popup_open(&ui, &pop, 80, 40, 30, 15, DGX_UI_POPUP_MODAL));
    CHECK(!dgx_ui_paint(&ui, &paint, 7300) && paint.wake_ms == DGX_UI_WAKE_NONE && all_of(panel, moved, colors[PAPER]));
    dgx_ui_popup_close(&ui);
    dgx_ui_paint(&ui, &paint, 7400);
    CHECK(paint.wake_ms == 500 && all_of(panel, moved, colors[CARET])); /* the focus shown again: as come */
    /* wholly under the keyboard's band: no time; the band gone, drawn by its phase */
    const dgx_ui_node_t *f = dgx_ui_node(&ui, field);
    paint.cover = dgx_ui_rect(0, f->y - 1, PANEL_W, f->h + 2);
    dgx_ui_paint(&ui, &paint, 7650);
    CHECK(!dgx_ui_paint(&ui, &paint, 7900) && paint.wake_ms == DGX_UI_WAKE_NONE);
    paint.cover = dgx_ui_rect(0, 0, 0, 0);
    dgx_ui_paint(&ui, &paint, 8000);
    CHECK(paint.wake_ms != DGX_UI_WAKE_NONE);
    /* a mirror composed at each pass: the same picture, the phase the first paint's */
    dgx_screen_t  *mirror = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    dgx_ui_paint_t second = {.scr = mirror, .colors = colors, .color_count = 4};
    int            same = 0;
    for (uint32_t now = 8100; now <= 10000; now += 100) {
        dgx_ui_paint(&ui, &paint, now);
        second.now_ms = now + 37; /* another moment: it must not move the phase */
        dgx_ui_paint_area_to(&ui, &second, mirror, NULL, 0, 0, PANEL_W, PANEL_H);
        const bool on = all_of(panel, moved, colors[CARET]);
        same += on == ((now - 7400) / 500 % 2 == 0);
    }
    CHECK(same == 20);
    /* the mirror past a toggle the screen has not drawn yet: the screen's next pass still draws it */
    const uint32_t edge = 7400 + 500 * ((10000 - 7400) / 500 + 1); /* the next toggle after 10000 */
    dgx_ui_paint(&ui, &paint, edge - 50);
    const bool before = all_of(panel, moved, colors[CARET]);
    second.now_ms = edge + 10;
    dgx_ui_paint_area_to(&ui, &second, mirror, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint(&ui, &paint, edge + 20);
    CHECK(all_of(panel, moved, colors[CARET]) == !before);
    dgx_ui_paint_free(&second), dgx_screen_destroy(&mirror);
    /* blink 0: steady, no time; a secret field blinks as a plain one */
    params.blink = 0;
    dgx_ui_redraw(&ui, field);
    CHECK(!dgx_ui_paint(&ui, &paint, 10100) && paint.wake_ms == DGX_UI_WAKE_NONE && all_of(panel, moved, colors[CARET]));
    params.blink = 50, params.secret = true;
    dgx_ui_text_set(&ui, field, "xyz");
    dgx_ui_paint(&ui, &paint, 11000);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &paint, 11500);
    CHECK(wrote_caret(panel, &again) && paint.wake_ms == 500);
    params.secret = false;
#if PANEL_BITS != 16
    /* through the strips at 18 bits: a toggle sends the field whole, every pixel once */
    static uint8_t buffer[4096];
    dgx_ui_paint_t strips = {.scr = scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer, .strips = &dgx_ui_strips};
    dgx_ui_redraw(&ui, 0);
    dgx_ui_paint(&ui, &strips, 12000);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &strips, 12500);
    CHECK(panel_twice(panel) == 0 && panel_written_outside(panel, f->x, f->y, f->x + f->w, f->y + f->h) == 0 && panel_written(panel) > 0);
    dgx_ui_paint_free(&strips);
#endif
    dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    free(panel);
    CHECK_DONE();
}
