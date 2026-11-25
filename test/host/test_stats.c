/*
 * The statistics of painting (dgx_ui_stats.h), by a clock of the test's own: each read of it 10 us, and each look of
 * the test costs it a set number of microseconds. A pass that draws three widgets, its time, the looks' and the
 * slowest draw to the microsecond; a pass that draws nothing; one widget drawn again. A popup's widget the slowest,
 * the popup closed and freed, the line still right; a tree past its array, its block counted in the room; the place of a closed popup and of a band gone composed inside a
 * measured pass, counted; a draw of two parts under a popup. The program's strips named and not, on a display behind
 * a bus of 18 bits (PANEL_BITS, given by the Makefile): the picture as by dgx_ui_paint(), the member given back.
 * Two trees drawn in turn by one paint, a struct each: dgx_ui_wake() still the least moment. A second paint of the tree
 * unmeasured; dgx_ui_paint_changes() on the same paint before and after a measured pass. A struct begun anew keeps its clock.
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

enum { WIDTH = 96, HEIGHT = 64 };
static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

/* the test's clock: 10 us a read; the looks below move it by their cost */
static int64_t fake_us;
static int64_t fake_clock(void)
{
    return fake_us += 10;
}

/* a look that draws nothing and costs so many microseconds */
typedef struct {
    uint32_t cost_us;
} cost_t;
static bool draw_costly(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    fake_us += ((const cost_t *)look->params)->cost_us;
    return false;
}
static const cost_t        cost_50 = {50}, cost_100 = {100}, cost_300 = {300}, cost_900 = {900};
static const dgx_ui_look_t look_50 = {.draw = draw_costly, .params = &cost_50, .fill = DGX_UI_PAPER}, look_100 = {.draw = draw_costly, .params = &cost_100, .fill = DGX_UI_PAPER},
                           look_300 = {.draw = draw_costly, .params = &cost_300, .fill = DGX_UI_PAPER}, look_900 = {.draw = draw_costly, .params = &cost_900, .fill = DGX_UI_PAPER};

static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .look = &look_50};
static const dgx_ui_widget_t first_w = {.kind = DGX_UI_LABEL, .want_h = 10, .look = &look_300};
static const dgx_ui_widget_t other_w = {.kind = DGX_UI_LABEL, .want_h = 10, .look = &look_100};

static int  told;
static void tell_count(void *ctx, dgx_ui_rect_t rect)
{
    ++told;
}

int main(void)
{
    dgx_screen_t  *screen = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
    static dgx_ui_node_t nodes[4];
    static dgx_ui_t      ui;
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_add(&ui, -1, &root_w);
    const int first = dgx_ui_add(&ui, 0, &first_w), other = dgx_ui_add(&ui, 0, &other_w);
    dgx_ui_paint_t       paint = {.scr = screen, .colors = colors, .color_count = 4};
    dgx_ui_paint_stats_t stats = {.clock_us = fake_clock};

    /* the first pass draws the three: the call 10 to 530; the root 20 to 80, the first 90 to 400, the other 410 to 520 */
    dgx_ui_paint_measured(&ui, &paint, 0, &stats);
    CHECK(stats.passes == 1 && stats.passes_us == 520 && stats.last_us == 520 && stats.drawing_passes == 1 && stats.drawing_us == 520);
    CHECK(stats.last_drawing_us == 520 && stats.longest_drawing_us == 520 && stats.looks_last_us == 480 && stats.looks_us == 480);
    CHECK(stats.draws_last == 3 && stats.parts_last == 3 && stats.draws == 3 && stats.parts == 3 && stats.widgets == 3 && stats.room == 4);
    CHECK(stats.slowest_last.level == 0 && stats.slowest_last.widget == first && stats.slowest_last.us == 310);
    CHECK(stats.slowest.level == 0 && stats.slowest.widget == first && stats.slowest.us == 310);
    CHECK(paint.strips == NULL);
    /* nothing changed: a pass of 10 us that drew nothing */
    dgx_ui_paint_measured(&ui, &paint, 100, &stats);
    CHECK(stats.passes == 2 && stats.passes_us == 530 && stats.last_us == 10 && stats.drawing_passes == 1 && stats.drawing_us == 520);
    CHECK(stats.draws_last == 0 && stats.parts_last == 0 && stats.looks_last_us == 0 && stats.slowest_last.widget == -1 && stats.slowest_last.level == -1);
    /* the other drawn again: 560 to 690, its draw 570 to 680; the slowest of all still the first */
    dgx_ui_redraw(&ui, other);
    dgx_ui_paint_measured(&ui, &paint, 200, &stats);
    CHECK(stats.passes == 3 && stats.last_us == 130 && stats.drawing_passes == 2 && stats.drawing_us == 650 && stats.last_drawing_us == 130);
    CHECK(stats.longest_drawing_us == 520 && stats.draws_last == 1 && stats.parts_last == 1 && stats.looks_last_us == 110 && stats.looks_us == 590);
    CHECK(stats.slowest_last.widget == other && stats.slowest_last.us == 110 && stats.slowest.widget == first && stats.slowest.us == 310);
    char text[400];
    dgx_ui_paint_stats_text(&stats, "home", text, sizeof text);
    CHECK(strstr(text, "stats home: 3 passes, 2 drew; a pass that drew 325 us mean, 130 last, 520 longest, its drawing 295 us; one that did not 10 us") &&
          strstr(text, "4 draws and 4 parts in all, 1 and 1 at the last pass; 3 widgets in use, room for 4") &&
          strstr(text, "the slowest draw widget 1 of the tree, 310 us; at the last pass widget 2 of the tree, 110 us"));

    /* a popup over it, its widget the slowest: widget 1 of popup 1, 900 us and its two reads */
    {
        static dgx_ui_node_t popup_nodes[2];
        static dgx_ui_t      popup;
        static const dgx_ui_widget_t popup_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .look = &look_50};
        static const dgx_ui_widget_t slow_w       = {.kind = DGX_UI_LABEL, .want_h = 10, .look = &look_900};
        dgx_ui_init(&popup, popup_nodes, 2, 1);
        dgx_ui_add(&popup, -1, &popup_root_w);
        const int slow = dgx_ui_add(&popup, 0, &slow_w);
        CHECK(dgx_ui_popup_open(&ui, &popup, 10, 10, 60, 30, 0));
        dgx_ui_paint_measured(&ui, &paint, 300, &stats);
        CHECK(stats.draws_last == 2 && stats.slowest_last.level == 1 && stats.slowest_last.widget == slow && stats.slowest_last.us == 910);
        CHECK(stats.slowest.level == 1 && stats.slowest.widget == slow && stats.slowest.us == 910 && stats.widgets == 5 && stats.room == 6);
        /* the other drawn again under the popup, which lies across it: one draw of two parts, the look called for each,
           its time both, 2 x (100 + 10) */
        dgx_ui_redraw(&ui, other);
        dgx_ui_paint_measured(&ui, &paint, 350, &stats);
        CHECK(stats.draws_last == 1 && stats.parts_last == 2 && stats.slowest_last.widget == other && stats.slowest_last.us == 220 && stats.looks_last_us == 220);
        /* closed and freed: its place composed again inside the measured pass, the widgets under it drawn and counted */
        dgx_ui_popup_close(&ui);
        dgx_ui_free(&popup);
        memset(popup_nodes, 0, sizeof popup_nodes);
        dgx_ui_paint_measured(&ui, &paint, 400, &stats);
        CHECK(stats.draws_last >= 2 && stats.parts_last >= 2 && stats.widgets == 3 && stats.room == 4);
        dgx_ui_paint_stats_text(&stats, "home", text, sizeof text);
        CHECK(strstr(text, "the slowest draw widget 1 of popup 1, 910 us") != NULL);
    }
    /* a keyboard's band laid over the tree, then gone: its place composed again inside the measured pass, counted */
    paint.cover = dgx_ui_rect(0, 40, WIDTH, 24);
    dgx_ui_paint_measured(&ui, &paint, 500, &stats);
    paint.cover = dgx_ui_rect(0, 0, 0, 0);
    dgx_ui_paint_measured(&ui, &paint, 600, &stats);
    CHECK(stats.draws_last >= 1 && stats.parts_last >= 1 && paint.strips == NULL);

    /* a tree past its array: three widgets in an array of two, the third in a block of DGX_UI_BLOCK; as the dump says
       "3 in use ... of the array of 2, blocks: 1" */
    {
        static dgx_ui_node_t small_nodes[2];
        static dgx_ui_t      small;
        dgx_ui_init(&small, small_nodes, 2, 1);
        dgx_ui_add(&small, -1, &root_w);
        dgx_ui_add(&small, 0, &first_w), dgx_ui_add(&small, 0, &other_w);
        dgx_ui_paint_t       small_paint = {.scr = screen, .colors = colors, .color_count = 4};
        dgx_ui_paint_stats_t small_stats = {.clock_us = fake_clock};
        dgx_ui_paint_measured(&small, &small_paint, 0, &small_stats);
        CHECK(small.block_count == 1 && small_stats.widgets == 3 && small_stats.room == 2 + DGX_UI_BLOCK && small_stats.draws_last == 3);
        dgx_ui_paint_free(&small_paint), dgx_ui_free(&small);
    }

    /* begun anew, the clock kept: a struct of zeros tells no draw, nor after a pass that drew nothing */
    stats = (dgx_ui_paint_stats_t){.clock_us = stats.clock_us};
    dgx_ui_paint_stats_text(&stats, "home", text, sizeof text);
    CHECK(strstr(text, "stats home: 0 passes, 0 drew;") && strstr(text, "the slowest draw none; at the last pass none"));
    dgx_ui_paint_measured(&ui, &paint, 650, &stats);
    dgx_ui_paint_stats_text(&stats, "home", text, sizeof text);
    CHECK(stats.passes == 1 && stats.drawing_passes == 0 && strstr(text, "the slowest draw none; at the last pass none"));
    stats = (dgx_ui_paint_stats_t){.clock_us = stats.clock_us};
    dgx_ui_redraw(&ui, first);
    dgx_ui_paint_measured(&ui, &paint, 700, &stats);
    CHECK(stats.passes == 1 && stats.drawing_passes == 1 && stats.draws_last == 1 && stats.slowest.widget == first && stats.slowest.us == 310);

    /* dgx_ui_paint_changes() on the same paint before and after a measured pass: its teller lent and given back each time */
    told = 0;
    dgx_ui_redraw(&ui, other);
    dgx_ui_paint_changes(&ui, &paint, 800, tell_count, NULL);
    CHECK(told > 0 && paint.strips == NULL);
    dgx_ui_redraw(&ui, other);
    dgx_ui_paint_measured(&ui, &paint, 900, &stats);
    CHECK(paint.strips == NULL && stats.draws_last == 1);
    told = 0;
    dgx_ui_redraw(&ui, first);
    dgx_ui_paint_changes(&ui, &paint, 1000, tell_count, NULL);
    CHECK(told > 0 && paint.strips == NULL);
    dgx_ui_paint_free(&paint), dgx_ui_free(&ui);

    /* real looks on a display behind a bus of 18 bits: the picture of a measured pass as of dgx_ui_paint(), the
       program's strips named and not. Two trees alike, a text box each: one measured, the other painted plainly. Named,
       the strips draw: the label changed writes no pixel twice, where straight it blinks */
    {
        static uint32_t              colors18[4] = {0x101820, 0xf0f0e8, 0x2c3c54, 0xf0a03c};
        static dgx_ui_font_variant_t variant[1];
        static dgx_ui_text_style_t   style;
        variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
        style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
        static char                  label_bufs[2][24];
        static dgx_ui_textbox_t      label_boxes[2];
        static dgx_ui_widget_t       label_ws[2];
        static const dgx_ui_widget_t box_w    = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2};
        static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .max = 10, .want_h = 10, .look = &dgx_ui_look_slider};
        static dgx_ui_node_t         real_nodes[2][3];
        static dgx_ui_t              real[2];
        static uint8_t               buffers[2][PANEL_W * 3 * 8];
        for (int named = 0; named < 2; ++named) {
            panel_t *panels[2] = {panel_make(), panel_make()};
            for (int k = 0; k < 2; ++k) {
                dgx_ui_textbox_init(&label_boxes[k], label_bufs[k], sizeof label_bufs[k], 1, &style);
                label_ws[k] = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &label_boxes[k], .want_h = 14};
                dgx_ui_init(&real[k], real_nodes[k], 3, 1);
                dgx_ui_add(&real[k], -1, &box_w);
                dgx_ui_add(&real[k], 0, &label_ws[k]), dgx_ui_add(&real[k], 0, &slider_w);
                dgx_ui_text_set(&real[k], 1, "21.5 C");
            }
            const dgx_ui_strips_t *strips   = named ? &dgx_ui_strips : NULL;
            dgx_ui_paint_t         measured = {.scr = &panels[0]->sbus.scr, .colors = colors18, .color_count = 4, .strips = strips, .buffer = buffers[0], .buffer_size = sizeof buffers[0]};
            dgx_ui_paint_t         plain    = {.scr = &panels[1]->sbus.scr, .colors = colors18, .color_count = 4, .strips = strips, .buffer = buffers[1], .buffer_size = sizeof buffers[1]};
            dgx_ui_paint_stats_t   real_stats = {0}; /* the platform's clock */
            int                    off = 0, twice = 0;
            for (uint32_t now = 0; now <= 300; now += 100) {
                for (int k = 0; now == 200 && k < 2; ++k) dgx_ui_text_set(&real[k], 1, "22.0 C"), dgx_ui_set_value(&real[k], 2, 7);
                if (now == 200) panel_count_from_now(panels[0]);
                dgx_ui_paint_measured(&real[0], &measured, now, &real_stats);
                dgx_ui_paint(&real[1], &plain, now);
                if (now == 200) twice = panel_twice(panels[0]);
                off += memcmp(panels[0]->mem, panels[1]->mem, sizeof panels[0]->mem) != 0;
            }
            CHECK(off == 0 && measured.strips == strips && real_stats.passes == 4 && real_stats.drawing_passes == 2 && real_stats.draws >= 4);
            CHECK(named ? measured.strip != NULL && twice == 0 : measured.strip == NULL && twice > 0);
            /* a second paint of the tree, unmeasured, composes it: the counts are not touched, its member not lent */
            dgx_screen_t  *fresh  = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
            dgx_ui_paint_t second = {.scr = fresh, .colors = colors18, .color_count = 4, .now_ms = 300};
            const uint32_t passes = real_stats.passes;
            dgx_ui_paint_area_to(&real[0], &second, fresh, NULL, 0, 0, PANEL_W, PANEL_H);
            CHECK(real_stats.passes == passes && second.strips == NULL && measured.strips == strips);
            dgx_ui_paint_free(&measured), dgx_ui_paint_free(&plain), dgx_ui_paint_free(&second), dgx_ui_free(&real[0]), dgx_ui_free(&real[1]);
            dgx_screen_destroy(&fresh), free(panels[0]), free(panels[1]);
        }
    }

    /* two trees drawn in turn by one paint, a struct each, at the same moment: the bar's letters ask for 40 ms, and
       dgx_ui_wake() after the page gives it */
    {
        static char             bar_buf[24];
        static dgx_ui_textbox_t bar_box;
        static dgx_ui_font_variant_t bar_variant[1];
        static dgx_ui_text_style_t   bar_style;
        bar_variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
        bar_style      = (dgx_ui_text_style_t){.fonts = bar_variant, .count = 1};
        dgx_ui_textbox_init(&bar_box, bar_buf, sizeof bar_buf, 1, &bar_style);
        static const dgx_ui_widget_t plain_root = {.kind = DGX_UI_BOX};
        const dgx_ui_widget_t        bar_w      = {.kind = DGX_UI_LABEL, .text = &bar_box, .look = &dgx_ui_look_lines, .want_h = 13};
        static dgx_ui_node_t         bar_nodes[2], page_nodes[2];
        static dgx_ui_t              bar, page;
        dgx_ui_init(&bar, bar_nodes, 2, 1), dgx_ui_init(&page, page_nodes, 2, 1);
        dgx_ui_add(&bar, -1, &plain_root), dgx_ui_add(&page, -1, &plain_root);
        dgx_ui_add(&bar, 0, &bar_w), dgx_ui_add(&page, 0, &other_w);
        dgx_ui_set_root(&bar, 0, 0, WIDTH, 16), dgx_ui_set_root(&page, 0, 16, WIDTH, HEIGHT - 16);
        dgx_ui_text_set(&bar, 1, "letters coming");
        dgx_ui_text_appear(&bar, 1, 40);
        dgx_ui_paint_t       two = {.scr = screen, .colors = colors, .color_count = 4};
        dgx_ui_paint_stats_t bar_stats = {.clock_us = fake_clock}, page_stats = {.clock_us = fake_clock};
        bool                 least = true;
        for (uint32_t now = 1000; now <= 1200; now += 20) {
            dgx_ui_paint_measured(&bar, &two, now, &bar_stats);
            dgx_ui_paint_measured(&page, &two, now, &page_stats);
            const uint32_t wait = dgx_ui_wake(&two, &page, now);
            least &= wait != DGX_UI_WAKE_NONE && wait <= 40;
        }
        CHECK(least && bar_stats.passes == 11 && page_stats.passes == 11 && bar_stats.drawing_passes > 1 && page_stats.drawing_passes == 1);
        CHECK(bar_stats.widgets == 2 && page_stats.widgets == 2 && two.strips == NULL);
        dgx_ui_paint_free(&two), dgx_ui_free(&bar), dgx_ui_free(&page);
    }

    dgx_screen_destroy(&screen);
    CHECK_DONE();
}
