/*
 * dgx_ui_stats.c
 *
 *  The statistics of painting (dgx_ui_stats.h). A measured pass lends the paint a counter in the member of the strips,
 *  as dgx_ui_paint_changes() lends its teller, and gives the member back after. The painter offers the counter every part a
 *  look is to draw (cut() of dgx_ui_paint.c); the counter reads the clock, hands the part to the program's strips when
 *  it named them, else calls the look itself as the painter would, and reads the clock again. A draw of a widget
 *  begins at its part 0. A told pass gives dgx_ui_paint_changes() a telling function of its own around the program's, the
 *  teller still the only holder of the member; the frames of a transition are timed around their call. Nothing is
 *  kept here: the counters live on the calls' stacks, the counts in the program's structs. A program that names none
 *  of it carries none of it, and each call only what it names.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <inttypes.h>
#include <stdio.h>

#include "dgx_ui_paint_priv.h"
#include "dgx_ui_stats.h"
#include "esp_log.h"

#if __has_include("esp_timer.h")
#include "esp_timer.h"
static int64_t platform_us(void)
{
    return esp_timer_get_time();
}
#else
/* a computer built by make, without ESP-IDF */
#include <time.h>
static int64_t platform_us(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
}
#endif

static const char TAG[] = "dgx_ui";

/* what a pass counted, of either way, before it goes into the program's struct */
typedef struct {
    uint32_t               looks_us; /* the drawing's time at this pass */
    uint16_t               draws, parts;
    dgx_ui_paint_slowest_t slowest;  /* the slowest draw of this pass */
} pass_t;

/* lent to the paint for the call: the strips' hook first, so the paint's pointer to its strips is the counter */
typedef struct {
    dgx_ui_strips_t        hook;
    const dgx_ui_strips_t *passed_on; /* the program's strips, offered each part after it; NULL none */
    dgx_ui_paint_stats_t  *stats;
    const dgx_ui_t        *measured;  /* the tree of the call: a popup's level counted from it */
    pass_t                 pass;
    dgx_ui_paint_slowest_t drawing;   /* the draw going on: its widget and its time so far */
    dgx_ui_widget_stats_t *entry;     /* its entry of the program's array; NULL none */
} counter_t;

static int64_t clock_of(int64_t (*clock_us)(void))
{
    return clock_us ? clock_us() : platform_us();
}

/* the level of a tree in the chain of popups over the measured one; -1 not in it */
static int level_of(const dgx_ui_t *measured, const dgx_ui_t *tree)
{
    int level = 0;
    for (const dgx_ui_t *in_chain = measured; in_chain; in_chain = in_chain->popup, ++level)
        if (in_chain == tree) return level;
    return -1;
}

/* a part the painter offers: counted and timed, drawn by the program's strips or by the look straight, as cut() would */
static bool counted_part(dgx_ui_paint_t *paint, const dgx_ui_look_t *look, dgx_ui_t *ui, int i, uint32_t now_ms, dgx_ui_rect_t r, bool *more)
{
    counter_t            *counter = (counter_t *)(uintptr_t)paint->strips;
    dgx_ui_paint_stats_t *stats   = counter->stats;
    if (paint->part == 0) {
        /* a draw begins: the widget's first part; its entry, when the program counts this tree's widgets each, begun
           anew for another widget at the index */
        ++counter->pass.draws;
        counter->drawing = (dgx_ui_paint_slowest_t){.level = (int8_t)level_of(counter->measured, ui), .widget = (int16_t)i, .us = 0};
        counter->entry   = stats->per_widget && ui == stats->per_widget_tree && i < stats->per_widget_count ? &stats->per_widget[i] : NULL;
        if (counter->entry) {
            const dgx_ui_widget_t *widget = dgx_ui_widget(ui, i);
            if (counter->entry->widget != widget) *counter->entry = (dgx_ui_widget_stats_t){.widget = widget};
            ++counter->entry->draws;
        }
    }
    ++counter->pass.parts;
    const int64_t before = clock_of(stats->clock_us);
    if (!counter->passed_on || !counter->passed_on->draw(paint, look, ui, i, now_ms, r, more)) *more = look->draw(look, paint, ui, i, now_ms);
    const uint32_t took = (uint32_t)(clock_of(stats->clock_us) - before);
    counter->pass.looks_us += took, counter->drawing.us += took;
    if (counter->drawing.us >= counter->pass.slowest.us) counter->pass.slowest = counter->drawing;
    dgx_ui_widget_stats_t *entry = counter->entry;
    if (entry) {
        if (entry->total_us > UINT32_MAX - took) entry->total_us /= 2, entry->draws = (entry->draws + 1) / 2; /* the mean kept */
        entry->total_us += took;
        if (counter->drawing.us > entry->longest_us) entry->longest_us = counter->drawing.us;
    }
    return true;
}

/* the widgets in use and the room of the arrays and blocks of a tree and the popups over it, as dgx_ui_dump() counts */
static void count_widgets(const dgx_ui_t *ui, uint16_t *widgets, uint16_t *room)
{
    uint32_t in_use = 0, all = 0;
    for (const dgx_ui_t *tree = ui; tree; tree = tree->popup) {
        for (int i = 0; i < tree->count; ++i) in_use += dgx_ui_widget(tree, i) != NULL;
        all += (uint32_t)tree->first_size + (uint32_t)tree->block_count * DGX_UI_BLOCK;
    }
    *widgets = (uint16_t)(in_use > 0xFFFF ? 0xFFFF : in_use), *room = (uint16_t)(all > 0xFFFF ? 0xFFFF : all);
}

/* a pass of either way into the program's struct: every pass, and apart those that drew */
static void count_pass(dgx_ui_paint_stats_t *stats, const dgx_ui_t *ui, uint32_t took, const pass_t *pass)
{
    ++stats->passes, stats->passes_us += took, stats->last_us = took;
    if (pass->parts) {
        ++stats->drawing_passes, stats->drawing_us += took, stats->last_drawing_us = took;
        if (took > stats->longest_drawing_us) stats->longest_drawing_us = took;
        if (!stats->draws || pass->slowest.us > stats->slowest.us) stats->slowest = pass->slowest;
    }
    stats->looks_last_us = pass->looks_us, stats->looks_us += pass->looks_us;
    stats->draws_last = pass->draws, stats->parts_last = pass->parts;
    stats->draws += pass->draws, stats->parts += pass->parts;
    stats->slowest_last = pass->slowest;
    count_widgets(ui, &stats->widgets, &stats->room);
}

bool dgx_ui_paint_measured(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms, dgx_ui_paint_stats_t *stats)
{
    counter_t counter = {.hook      = {.draw = counted_part},
                         .passed_on = paint->strips,
                         .stats     = stats,
                         .measured  = ui,
                         .pass      = {.slowest = {.level = -1, .widget = -1}}};
    paint->strips          = &counter.hook;
    const int64_t  start   = clock_of(stats->clock_us);
    const bool     more    = dgx_ui_paint(ui, paint, now_ms);
    const uint32_t took    = (uint32_t)(clock_of(stats->clock_us) - start);
    paint->strips          = counter.passed_on;
    count_pass(stats, ui, took, &counter.pass);
    return more;
}

/* the told pass's own telling function around the program's: each rectangle a draw and a part, timed */
typedef struct {
    dgx_ui_paint_changes_t tell;
    void             *ctx;
    int64_t (*clock_us)(void);
    pass_t            pass;
} telling_t;

static void told_measured(void *arg, dgx_ui_rect_t rect)
{
    telling_t    *telling = arg;
    const int64_t before  = clock_of(telling->clock_us);
    telling->tell(telling->ctx, rect);
    const uint32_t took = (uint32_t)(clock_of(telling->clock_us) - before);
    ++telling->pass.draws, ++telling->pass.parts, telling->pass.looks_us += took;
    if (took >= telling->pass.slowest.us) telling->pass.slowest.us = took;
}

bool dgx_ui_paint_changes_measured(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms, dgx_ui_paint_changes_t tell, void *ctx,
                                   dgx_ui_paint_stats_t *stats)
{
    telling_t      telling = {.tell = tell, .ctx = ctx, .clock_us = stats->clock_us, .pass = {.slowest = {.level = -1, .widget = -1}}};
    const int64_t  start   = clock_of(stats->clock_us);
    const bool     more    = tell ? dgx_ui_paint_changes(ui, paint, now_ms, told_measured, &telling)
                                  : dgx_ui_paint_changes(ui, paint, now_ms, NULL, ctx);
    const uint32_t took    = (uint32_t)(clock_of(stats->clock_us) - start);
    count_pass(stats, ui, took, &telling.pass);
    return more;
}

void dgx_ui_paint_stats_restart(dgx_ui_paint_stats_t *stats)
{
    const dgx_ui_paint_stats_t kept = *stats;
    *stats = (dgx_ui_paint_stats_t){.clock_us = kept.clock_us, .per_widget_tree = kept.per_widget_tree, .per_widget = kept.per_widget, .per_widget_count = kept.per_widget_count};
    for (int i = 0; kept.per_widget && i < kept.per_widget_count; ++i) kept.per_widget[i] = (dgx_ui_widget_stats_t){0};
}

bool dgx_ui_transition_frame_measured(dgx_ui_transition_t *tr, dgx_ui_paint_t *paint, uint32_t now_ms, dgx_ui_frame_stats_t *stats)
{
    if (!tr->running) return dgx_ui_transition_frame(tr, paint, now_ms); /* no frame: nothing counted */
    const int64_t before = clock_of(stats->clock_us);
    /* the first frame of a run: one begun again drops the run before; a run whose first frame this struct did not see
       is in the cost only */
    if (!tr->started) stats->run_start_us = before, stats->run_frames = 0, stats->run_seen = true;
    const bool     more  = dgx_ui_transition_frame(tr, paint, now_ms);
    const uint32_t took  = (uint32_t)(clock_of(stats->clock_us) - before);
    ++stats->frames, stats->frames_us += took, stats->last_us = took, ++stats->run_frames;
    if (took > stats->longest_us) stats->longest_us = took;
    if (!tr->running) {
        if (stats->run_seen) ++stats->transitions, stats->shown_periods += stats->run_frames - 1, stats->shown_us += (uint64_t)(before - stats->run_start_us);
        stats->run_seen = false;
    }
    return more;
}

/* a draw in words, where it lay and how long it took: "widget 3 of the tree, 310 us", "widget 5 of popup 1, 910 us",
   "a rectangle told, 900 us"; "none" when no draw was counted, which the counts tell: a struct of zeros has zeros there
   too */
static const char *draw_in_words(const dgx_ui_paint_slowest_t *draw, bool counted, char *text, size_t size)
{
    if (!counted) snprintf(text, size, "none");
    else if (draw->widget < 0) snprintf(text, size, "a rectangle told, %" PRIu32 " us", draw->us);
    else if (draw->level == 0) snprintf(text, size, "widget %d of the tree, %" PRIu32 " us", draw->widget, draw->us);
    else if (draw->level > 0) snprintf(text, size, "widget %d of popup %d, %" PRIu32 " us", draw->widget, draw->level, draw->us);
    else snprintf(text, size, "widget %d of a tree out of the chain, %" PRIu32 " us", draw->widget, draw->us);
    return text;
}

int dgx_ui_paint_stats_text(const dgx_ui_paint_stats_t *stats, const char *name, char *text, size_t size)
{
    const uint32_t drew = stats->drawing_passes, idle = stats->passes - stats->drawing_passes;
    const uint32_t drew_mean  = drew ? (uint32_t)(stats->drawing_us / drew) : 0, idle_mean = idle ? (uint32_t)((stats->passes_us - stats->drawing_us) / idle) : 0;
    const uint32_t looks_mean = drew ? (uint32_t)(stats->looks_us / drew) : 0;
    char           slowest[64], slowest_last[64];
    /* in microseconds: no floating point, and no division but for the means */
    return snprintf(text, size,
                    "stats %s: %" PRIu32 " passes, %" PRIu32 " drew; a pass that drew %" PRIu32 " us mean, %" PRIu32 " last, %" PRIu32
                    " longest, its drawing %" PRIu32 " us; one that did not %" PRIu32 " us; %" PRIu32 " draws and %" PRIu32
                    " parts in all, %u and %u at the last pass; %u widgets in use, room for %u; the slowest draw %s; at the last pass %s",
                    name ? name : "", stats->passes, drew, drew_mean, stats->last_drawing_us, stats->longest_drawing_us, looks_mean, idle_mean,
                    stats->draws, stats->parts, (unsigned)stats->draws_last, (unsigned)stats->parts_last, (unsigned)stats->widgets, (unsigned)stats->room,
                    draw_in_words(&stats->slowest, stats->draws > 0, slowest, sizeof slowest),
                    draw_in_words(&stats->slowest_last, stats->draws_last > 0, slowest_last, sizeof slowest_last));
}

void dgx_ui_paint_stats_log(const dgx_ui_paint_stats_t *stats, const char *name)
{
    char text[480]; /* the line of the largest counts and a name of 80 letters */
    dgx_ui_paint_stats_text(stats, name, text, sizeof text);
    ESP_LOGI(TAG, "%s", text);
}

int dgx_ui_widget_stats_text(const dgx_ui_paint_stats_t *stats, int i, char *text, size_t size)
{
    const dgx_ui_widget_stats_t *entry = stats->per_widget && i >= 0 && i < stats->per_widget_count ? &stats->per_widget[i] : NULL;
    if (!entry || !entry->draws) return snprintf(text, size, "widget %d: not drawn", i);
    return snprintf(text, size, "widget %d: %" PRIu32 " draws, %" PRIu32 " us mean, %" PRIu32 " us longest", i, entry->draws, entry->total_us / entry->draws,
                    entry->longest_us);
}

void dgx_ui_widget_stats_log(const dgx_ui_paint_stats_t *stats, const char *name)
{
    for (int i = 0; stats->per_widget && i < stats->per_widget_count; ++i) {
        if (!stats->per_widget[i].draws) continue;
        char text[96];
        dgx_ui_widget_stats_text(stats, i, text, sizeof text);
        ESP_LOGI(TAG, "stats %s, %s", name ? name : "", text);
    }
}

int dgx_ui_frame_stats_text(const dgx_ui_frame_stats_t *stats, const char *name, char *text, size_t size)
{
    const uint32_t mean       = stats->frames ? (uint32_t)(stats->frames_us / stats->frames) : 0;
    const uint32_t tenths     = stats->shown_us ? (uint32_t)((uint64_t)stats->shown_periods * 10000000u / stats->shown_us) : 0; /* frames in 10 s */
    char           shown[48];
    if (stats->transitions) snprintf(shown, sizeof shown, "%" PRIu32 ".%" PRIu32 " frames a second shown", tenths / 10, tenths % 10);
    else snprintf(shown, sizeof shown, "no transition run to its end");
    return snprintf(text, size,
                    "frames %s: %" PRIu32 " frames, transitions run to their end: %" PRIu32 "; a frame %" PRIu32 " us mean, %" PRIu32 " last, %" PRIu32
                    " longest; %s",
                    name ? name : "", stats->frames, stats->transitions, mean, stats->last_us, stats->longest_us, shown);
}

void dgx_ui_frame_stats_log(const dgx_ui_frame_stats_t *stats, const char *name)
{
    char text[240];
    dgx_ui_frame_stats_text(stats, name, text, sizeof text);
    ESP_LOGI(TAG, "%s", text);
}
