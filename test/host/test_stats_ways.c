/*
 * The statistics of the other ways (dgx_ui_stats.h), by a clock of the test's own, 10 us a read, its looks and told
 * functions costing it a set time. A pass by dgx_ui_changed_measured() is the same pass as by dgx_ui_changed(): the
 * same rectangles in the same order, the same marks left, with a function that composes at once and with none; its
 * rectangles counted and timed. The frames of a transition: the same screens as unmeasured, their cost and the rate
 * shown over the span with the loop's waits, one begun again before its end, none counted with no transition; by a
 * clock at 1000 s, a struct begun anew in the middle of a run and a run whose first frames were not measured, in the
 * cost only. A count
 * for each widget: a draw by a composing and by the walk of one pass, the entries' sums the whole's, a widget replaced
 * at its index, an array smaller than the tree, a page named, closed and freed, a sum near 32 bits halved, begun anew.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_stats.h"
#include "dgx_ui_transition.h"
#include "dgx_vscreen.h"

enum { WIDTH = 96, HEIGHT = 64 };
static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

/* the test's clock: 10 us a read; the looks and the told functions move it by their cost */
static int64_t fake_us;
static int64_t fake_clock(void)
{
    return fake_us += 10;
}

/* a look that draws its box in a colour of its own and costs so many microseconds */
typedef struct {
    uint32_t cost_us;
    uint8_t  color;
} cost_t;
static bool draw_costly(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const cost_t        *cost = look->params;
    const dgx_ui_node_t *node = dgx_ui_node(ui, i);
    fake_us += cost->cost_us;
    dgx_fill_rectangle(paint->scr, node->x - paint->ox, node->y - paint->oy, node->w, node->h, dgx_ui_color(paint, cost->color));
    return false;
}
static const cost_t        cost_50 = {50, 0}, cost_100 = {100, 1}, cost_300 = {300, 3};
static const dgx_ui_look_t look_50 = {.draw = draw_costly, .params = &cost_50, .fill = -1}, look_100 = {.draw = draw_costly, .params = &cost_100, .fill = -1},
                           look_300 = {.draw = draw_costly, .params = &cost_300, .fill = -1};

static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .look = &look_50};
static const dgx_ui_widget_t first_w = {.kind = DGX_UI_LABEL, .want_h = 10, .look = &look_300};
static const dgx_ui_widget_t other_w = {.kind = DGX_UI_LABEL, .want_h = 10, .look = &look_100};

/* a tree of the root and two widgets */
static void make(dgx_ui_t *ui, dgx_ui_node_t *nodes, int size)
{
    dgx_ui_init(ui, nodes, size, 1);
    dgx_ui_add(ui, -1, &root_w);
    dgx_ui_add(ui, 0, &first_w), dgx_ui_add(ui, 0, &other_w);
}

/* the rectangles told, in order, and a composing at once into a frame of the program's when it is given */
typedef struct {
    dgx_ui_rect_t   rects[16];
    int             count;
    uint32_t        cost_us;
    dgx_ui_t       *ui;
    dgx_ui_paint_t *paint;
    dgx_screen_t   *frame;
} told_t;
static void tell_rect(void *ctx, dgx_ui_rect_t rect)
{
    told_t *told = ctx;
    if (told->count < 16) told->rects[told->count] = rect;
    ++told->count;
    fake_us += told->cost_us;
    if (told->frame) dgx_ui_paint_area_to(told->ui, told->paint, told->frame, NULL, rect.x, rect.y, rect.w, rect.h);
}

static int differ(dgx_screen_t *one, dgx_screen_t *other)
{
    int count = 0;
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x) count += dgx_get_pixel(one, x, y) != dgx_get_pixel(other, x, y);
    return count;
}

int main(void)
{
    dgx_screen_t *screens[2] = {dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB), dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB)};
    dgx_screen_t *frames[2]  = {dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB), dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB)};

    /* a told pass, plain and measured: two trees alike, a function that composes each rectangle at once into a frame
       of its own, costing 200 us; then nothing told; then tell NULL */
    {
        static dgx_ui_node_t nodes[2][3];
        static dgx_ui_t      trees[2];
        static told_t        told[2];
        dgx_ui_paint_t       paints[2];
        for (int k = 0; k < 2; ++k) {
            make(&trees[k], nodes[k], 3);
            paints[k] = (dgx_ui_paint_t){.scr = screens[k], .colors = colors, .color_count = 4};
            told[k]   = (told_t){.cost_us = 200, .ui = &trees[k], .paint = &paints[k], .frame = frames[k]};
            dgx_fill_rectangle(frames[k], 0, 0, WIDTH, HEIGHT, 0);
        }
        dgx_ui_paint_stats_t stats = {.clock_us = fake_clock};
        for (uint32_t now = 0; now <= 200; now += 100) {
            if (now == 200) dgx_ui_redraw(&trees[0], 2), dgx_ui_redraw(&trees[1], 2);
            told[0].count = told[1].count = 0;
            dgx_ui_changed(&trees[0], &paints[0], now, tell_rect, &told[0]);
            dgx_ui_changed_measured(&trees[1], &paints[1], now, tell_rect, &told[1], &stats);
            CHECK(told[0].count == told[1].count && memcmp(told[0].rects, told[1].rects, sizeof told[0].rects) == 0 && differ(frames[0], frames[1]) == 0);
            CHECK(paints[0].strips == NULL && paints[1].strips == NULL);
        }
        /* the counts: three passes, two told (the first, all of the tree; the third, the other widget) */
        CHECK(stats.passes == 3 && stats.drawing_passes == 2 && stats.draws_last == 1 && stats.parts_last == 1 && stats.draws == stats.parts);
        CHECK(stats.slowest.widget == -1 && stats.slowest.level == -1 && stats.slowest.us >= 200 && stats.looks_last_us == stats.slowest_last.us);
        char text[480];
        dgx_ui_paint_stats_text(&stats, "told", text, sizeof text);
        CHECK(strstr(text, "the slowest draw a rectangle told,") && strstr(text, "its drawing"));
        /* the marks as a plain pass leaves them: a measured pass after draws nothing in either */
        dgx_ui_paint_stats_t after[2] = {{.clock_us = fake_clock}, {.clock_us = fake_clock}};
        for (int k = 0; k < 2; ++k) {
            told[k].frame = NULL;
            dgx_ui_paint_measured(&trees[k], &paints[k], 300, &after[k]);
            CHECK(after[k].draws_last == 0);
        }
        /* tell NULL: given as NULL, nobody told, the marks taken; the call timed, a pass that drew nothing */
        dgx_ui_redraw(&trees[1], 1);
        const uint32_t drew = stats.drawing_passes;
        dgx_ui_changed_measured(&trees[1], &paints[1], 400, NULL, NULL, &stats);
        CHECK(stats.passes == 4 && stats.drawing_passes == drew && stats.draws_last == 0 && paints[1].strips == NULL);
        dgx_ui_paint_measured(&trees[1], &paints[1], 500, &after[1]);
        CHECK(after[1].draws_last == 0);
        for (int k = 0; k < 2; ++k) dgx_ui_paint_free(&paints[k]), dgx_ui_free(&trees[k]);
    }

    /* the frames of a slide, plain and measured, two homes alike and a page each: the same screens at the same now; a
       frame costs two reads of the clock (10 us), the loop waits 9980 us between frames: 10 us a frame, the rate shown
       over the span */
    {
        static dgx_ui_node_t home_nodes[2][3], page_nodes[2][1];
        static dgx_ui_t      homes[2], pages[2];
        static const dgx_ui_widget_t page_w = {.kind = DGX_UI_BOX, .look = &look_100};
        dgx_screen_t        *pictures[4];
        dgx_ui_paint_t       paints[2];
        dgx_ui_transition_t  slides[2];
        for (int k = 0; k < 2; ++k) {
            make(&homes[k], home_nodes[k], 3);
            dgx_ui_init(&pages[k], page_nodes[k], 1, 1), dgx_ui_add(&pages[k], -1, &page_w);
            pictures[2 * k] = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB), pictures[2 * k + 1] = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
            paints[k] = (dgx_ui_paint_t){.scr = screens[k], .colors = colors, .color_count = 4};
            slides[k] = (dgx_ui_transition_t){.from = pictures[2 * k], .to = pictures[2 * k + 1], .ms = 100};
            dgx_ui_paint(&homes[k], &paints[k], 0);
        }
        dgx_ui_frame_stats_t frames_stats = {.clock_us = fake_clock};
        /* no transition running: called, nothing counted */
        CHECK(!dgx_ui_transition_frame_measured(&slides[1], &paints[1], 0, &frames_stats) && frames_stats.frames == 0);
        for (int k = 0; k < 2; ++k) dgx_ui_page_open_by(&homes[k], &pages[k], &paints[k], &slides[k], DGX_UI_SLIDE_LEFT);
        int off = 0, count = 0;
        for (uint32_t now = 1000; slides[0].running || slides[1].running; now += 20, ++count) {
            if (count) fake_us += 9980; /* the loop's wait between two frames */
            dgx_ui_transition_frame(&slides[0], &paints[0], now);
            dgx_ui_transition_frame_measured(&slides[1], &paints[1], now, &frames_stats);
            off += differ(screens[0], screens[1]);
        }
        /* 6 frames, at 0, 20, ... 100 ms of the slide; their starts 10000 us apart: 5 periods over 50000 us */
        CHECK(off == 0 && count == 6 && frames_stats.frames == 6 && frames_stats.transitions == 1 && frames_stats.shown_periods == 5);
        CHECK(frames_stats.frames_us == 60 && frames_stats.last_us == 10 && frames_stats.longest_us == 10 && frames_stats.shown_us == 50000);
        char text[240];
        dgx_ui_frame_stats_text(&frames_stats, "slide", text, sizeof text);
        CHECK(strcmp(text, "frames slide: 6 frames, transitions run to their end: 1; a frame 10 us mean, 10 last, 10 longest; 100.0 frames a second shown") == 0);
        /* back by a slide, begun again after two frames: the run before dropped from the rate, its frames in the cost */
        dgx_ui_page_back_by(&homes[1], &paints[1], &slides[1], DGX_UI_SLIDE_RIGHT);
        for (uint32_t now = 2000; now <= 2020; now += 20) dgx_ui_transition_frame_measured(&slides[1], &paints[1], now, &frames_stats);
        dgx_ui_page_open_by(&homes[1], &pages[1], &paints[1], &slides[1], DGX_UI_SLIDE_LEFT);
        for (uint32_t now = 3000; slides[1].running; now += 20) dgx_ui_transition_frame_measured(&slides[1], &paints[1], now, &frames_stats);
        CHECK(frames_stats.frames == 6 + 2 + 6 && frames_stats.transitions == 2 && frames_stats.shown_periods == 10);
        for (int k = 0; k < 2; ++k) {
            dgx_ui_paint_free(&paints[k]), dgx_ui_free(&homes[k]), dgx_ui_free(&pages[k]);
            dgx_screen_destroy(&pictures[2 * k]), dgx_screen_destroy(&pictures[2 * k + 1]);
        }
    }

    /* a clock that does not begin at 0 (at 1000 s): a struct begun anew in the middle of a run, and a run whose first
       frames were drawn unmeasured, are in the cost only and leave the rate alone; then a whole run measured, the rate
       of it alone: 5 periods of 10000 us */
    {
        static dgx_ui_node_t         home_nodes[3], page_nodes[1];
        static dgx_ui_t              home, page;
        static const dgx_ui_widget_t page_w   = {.kind = DGX_UI_BOX, .look = &look_100};
        dgx_screen_t                *pictures[2] = {dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB), dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB)};
        make(&home, home_nodes, 3);
        dgx_ui_init(&page, page_nodes, 1, 1), dgx_ui_add(&page, -1, &page_w);
        dgx_ui_paint_t       paint = {.scr = screens[0], .colors = colors, .color_count = 4};
        dgx_ui_transition_t  slide = {.from = pictures[0], .to = pictures[1], .ms = 100};
        dgx_ui_frame_stats_t stats = {.clock_us = fake_clock};
        fake_us                    = 1000000000;
        dgx_ui_paint(&home, &paint, 0);
        /* begun anew after the third frame */
        dgx_ui_page_open_by(&home, &page, &paint, &slide, DGX_UI_SLIDE_LEFT);
        int frame = 0;
        for (uint32_t now = 1000; slide.running; now += 20, ++frame) {
            if (frame == 3) stats = (dgx_ui_frame_stats_t){.clock_us = stats.clock_us};
            fake_us += 9980;
            dgx_ui_transition_frame_measured(&slide, &paint, now, &stats);
        }
        CHECK(stats.frames == 3 && stats.transitions == 0 && stats.shown_periods == 0 && stats.shown_us == 0);
        char text[240];
        dgx_ui_frame_stats_text(&stats, "slide", text, sizeof text);
        CHECK(strstr(text, "transitions run to their end: 0;") && strstr(text, "no transition run to its end"));
        /* its first two frames drawn unmeasured */
        dgx_ui_page_back_by(&home, &paint, &slide, DGX_UI_SLIDE_RIGHT);
        frame = 0;
        for (uint32_t now = 2000; slide.running; now += 20, ++frame) {
            fake_us += 9980;
            if (frame < 2) dgx_ui_transition_frame(&slide, &paint, now);
            else dgx_ui_transition_frame_measured(&slide, &paint, now, &stats);
        }
        CHECK(stats.frames == 3 + 4 && stats.transitions == 0 && stats.shown_us == 0);
        /* a whole run */
        dgx_ui_page_open_by(&home, &page, &paint, &slide, DGX_UI_SLIDE_LEFT);
        for (uint32_t now = 3000; slide.running; now += 20) {
            fake_us += 9980;
            dgx_ui_transition_frame_measured(&slide, &paint, now, &stats);
        }
        dgx_ui_frame_stats_text(&stats, "slide", text, sizeof text);
        CHECK(stats.transitions == 1 && stats.shown_periods == 5 && stats.shown_us == 50000 && strstr(text, "100.0 frames a second shown"));
        dgx_ui_paint_free(&paint), dgx_ui_free(&home), dgx_ui_free(&page);
        dgx_screen_destroy(&pictures[0]), dgx_screen_destroy(&pictures[1]);
    }

    /* a count for each widget of the home tree */
    {
        static dgx_ui_node_t         nodes[3];
        static dgx_ui_t              home;
        static dgx_ui_widget_stats_t entries[3];
        make(&home, nodes, 3);
        dgx_ui_paint_t       paint = {.scr = screens[0], .colors = colors, .color_count = 4};
        dgx_ui_paint_stats_t stats = {.clock_us = fake_clock, .per_widget_tree = &home, .per_widget = entries, .per_widget_count = 3};
        dgx_ui_paint_measured(&home, &paint, 0, &stats);
        dgx_ui_redraw(&home, 1);
        dgx_ui_paint_measured(&home, &paint, 100, &stats);
        /* the entries' sums are the whole's: the array covers the tree, no popup open */
        CHECK(entries[0].draws == 1 && entries[1].draws == 2 && entries[2].draws == 1 && entries[1].total_us == 2 * 310 && entries[1].longest_us == 310);
        CHECK(entries[0].draws + entries[1].draws + entries[2].draws == stats.draws &&
              entries[0].total_us + entries[1].total_us + entries[2].total_us == stats.looks_us);
        char text[96];
        dgx_ui_widget_stats_text(&stats, 1, text, sizeof text);
        CHECK(strcmp(text, "widget 1: 2 draws, 310 us mean, 310 us longest") == 0);
        /* a popup over the first widget, closed, and the first widget changed: its place composed and the walk drawing it,
           two draws of one pass in its entry as in the whole */
        {
            static dgx_ui_node_t popup_nodes[1];
            static dgx_ui_t      popup;
            dgx_ui_init(&popup, popup_nodes, 1, 1), dgx_ui_add(&popup, -1, &root_w);
            dgx_ui_popup_open(&home, &popup, 10, 2, 40, 6, 0);
            dgx_ui_paint_measured(&home, &paint, 200, &stats);
            dgx_ui_popup_close(&home);
            dgx_ui_redraw(&home, 1);
            const uint32_t before = entries[1].draws;
            dgx_ui_paint_measured(&home, &paint, 300, &stats);
            CHECK(entries[1].draws == before + 2 && stats.draws_last >= 3);
            dgx_ui_free(&popup);
        }
        /* another widget at the index of the other: its entry begun anew */
        dgx_ui_remove(&home, 2);
        static const dgx_ui_widget_t new_w = {.kind = DGX_UI_LABEL, .want_h = 10, .look = &look_100};
        CHECK(dgx_ui_add(&home, 0, &new_w) == 2);
        dgx_ui_paint_measured(&home, &paint, 400, &stats);
        CHECK(entries[2].widget == &new_w && entries[2].draws == 1 && entries[2].total_us == 110);
        /* a sum near 32 bits: the draws and the sum halved first, the mean kept */
        entries[1] = (dgx_ui_widget_stats_t){.widget = &first_w, .draws = 10000, .total_us = 4294967000u, .longest_us = 310}; /* 310 more would pass 32 bits */
        const uint32_t mean = entries[1].total_us / entries[1].draws;
        dgx_ui_redraw(&home, 1);
        dgx_ui_paint_measured(&home, &paint, 500, &stats);
        const uint32_t mean_after = entries[1].total_us / entries[1].draws;
        CHECK(entries[1].draws == 5001 && entries[1].total_us == 4294967000u / 2 + 310 && mean_after + mean_after / 100 >= mean && mean_after <= mean + mean / 100);
        /* begun anew: the counts and the entries zero, the clock and the naming kept */
        dgx_ui_paint_stats_restart(&stats);
        CHECK(stats.passes == 0 && stats.draws == 0 && entries[1].draws == 0 && entries[1].widget == NULL && stats.clock_us == fake_clock &&
              stats.per_widget == entries && stats.per_widget_tree == &home && stats.per_widget_count == 3);
        dgx_ui_paint_free(&paint), dgx_ui_free(&home);
    }

    /* an array smaller than the tree: three widgets in an array of nodes of two, the third in a block, an array of two
       entries; the third not counted each, counted in the whole */
    {
        static dgx_ui_node_t         nodes[2];
        static dgx_ui_t              home;
        static dgx_ui_widget_stats_t entries[2];
        make(&home, nodes, 2);
        dgx_ui_paint_t       paint = {.scr = screens[0], .colors = colors, .color_count = 4};
        dgx_ui_paint_stats_t stats = {.clock_us = fake_clock, .per_widget_tree = &home, .per_widget = entries, .per_widget_count = 2};
        dgx_ui_paint_measured(&home, &paint, 0, &stats);
        CHECK(home.block_count == 1 && stats.draws == 3 && entries[0].draws + entries[1].draws == 2);
        dgx_ui_paint_free(&paint), dgx_ui_free(&home);
    }

    /* a page named: its widgets counted while it is drawn over the home; closed and freed, nothing counted, the entries
       kept */
    {
        static dgx_ui_node_t         home_nodes[3], page_nodes[3];
        static dgx_ui_t              home, page;
        static dgx_ui_widget_stats_t entries[3];
        make(&home, home_nodes, 3), make(&page, page_nodes, 3);
        dgx_ui_paint_t       paint = {.scr = screens[0], .colors = colors, .color_count = 4};
        dgx_ui_paint_stats_t stats = {.clock_us = fake_clock, .per_widget_tree = &page, .per_widget = entries, .per_widget_count = 3};
        dgx_ui_paint_measured(&home, &paint, 0, &stats);
        CHECK(entries[0].draws == 0 && entries[1].draws == 0);
        dgx_ui_page_open(&home, &page);
        dgx_ui_paint_measured(&home, &paint, 100, &stats);
        CHECK(entries[0].draws == 1 && entries[1].draws == 1 && entries[2].draws == 1 && entries[1].total_us == 310);
        dgx_ui_page_back(&home);
        dgx_ui_free(&page);
        memset(page_nodes, 0, sizeof page_nodes);
        dgx_ui_paint_measured(&home, &paint, 200, &stats);
        CHECK(entries[0].draws == 1 && entries[1].draws == 1 && entries[2].draws == 1 && stats.draws_last >= 1);
        dgx_ui_paint_free(&paint), dgx_ui_free(&home);
    }

    for (int k = 0; k < 2; ++k) dgx_screen_destroy(&screens[k]), dgx_screen_destroy(&frames[k]);
    CHECK_DONE();
}
