/*
 * dgx_ui_stats.c
 *
 *  The statistics of painting (dgx_ui_stats.h): the call lends the paint a counter for its pass in the member of the
 *  strips, as dgx_ui_changed() lends its teller, and gives the member back after. The painter offers the counter every
 *  part a look is to draw (cut() of dgx_ui_paint.c); the counter reads the clock, hands the part to the program's strips
 *  when it named them, else calls the look itself as the painter would, and reads the clock again. A draw of a widget
 *  begins at its part 0. Nothing is kept here: the counter lives on the call's stack, the counts in the program's
 *  struct. A program that names none of it carries none of it.
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

/* lent to the paint for the call: the strips' hook first, so the paint's pointer to its strips is the counter */
typedef struct {
    dgx_ui_strips_t        hook;
    const dgx_ui_strips_t *passed_on; /* the program's strips, offered each part after it; NULL none */
    int64_t (*clock_us)(void);
    const dgx_ui_t        *measured;  /* the tree of the call: a popup's level counted from it */
    uint32_t               looks_us;  /* the looks' time at this pass */
    uint16_t               draws, parts;
    dgx_ui_paint_slowest_t drawing;   /* the draw going on: its widget and its time so far */
    dgx_ui_paint_slowest_t slowest;   /* the slowest draw of this pass */
} counter_t;

static int64_t clock_of(const counter_t *counter)
{
    return counter->clock_us ? counter->clock_us() : platform_us();
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
    counter_t *counter = (counter_t *)(uintptr_t)paint->strips;
    if (paint->part == 0) {
        /* a draw begins: the widget's first part */
        ++counter->draws;
        counter->drawing = (dgx_ui_paint_slowest_t){.level = (int8_t)level_of(counter->measured, ui), .widget = (int16_t)i, .us = 0};
    }
    ++counter->parts;
    const int64_t before = clock_of(counter);
    if (!counter->passed_on || !counter->passed_on->draw(paint, look, ui, i, now_ms, r, more)) *more = look->draw(look, paint, ui, i, now_ms);
    const uint32_t took = (uint32_t)(clock_of(counter) - before);
    counter->looks_us += took, counter->drawing.us += took;
    if (counter->drawing.us >= counter->slowest.us) counter->slowest = counter->drawing;
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

bool dgx_ui_paint_measured(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms, dgx_ui_paint_stats_t *stats)
{
    counter_t counter = {.hook      = {.draw = counted_part},
                         .passed_on = paint->strips,
                         .clock_us  = stats->clock_us,
                         .measured  = ui,
                         .slowest   = {.level = -1, .widget = -1}};
    paint->strips          = &counter.hook;
    const int64_t  start   = clock_of(&counter);
    const bool     more    = dgx_ui_paint(ui, paint, now_ms);
    const uint32_t took    = (uint32_t)(clock_of(&counter) - start);
    paint->strips          = counter.passed_on;
    /* the counts: every pass, and apart those that drew */
    ++stats->passes, stats->passes_us += took, stats->last_us = took;
    if (counter.parts) {
        ++stats->drawing_passes, stats->drawing_us += took, stats->last_drawing_us = took;
        if (took > stats->longest_drawing_us) stats->longest_drawing_us = took;
        if (!stats->draws || counter.slowest.us > stats->slowest.us) stats->slowest = counter.slowest;
    }
    stats->looks_last_us = counter.looks_us, stats->looks_us += counter.looks_us;
    stats->draws_last = counter.draws, stats->parts_last = counter.parts;
    stats->draws += counter.draws, stats->parts += counter.parts;
    stats->slowest_last = counter.slowest;
    count_widgets(ui, &stats->widgets, &stats->room);
    return more;
}

/* a draw in words, where it lay and how long it took: "widget 3 of the tree, 310 us", "widget 5 of popup 1, 910 us";
   "none" when no draw was counted, which the counts tell: a struct of zeros has zeros there too */
static const char *draw_in_words(const dgx_ui_paint_slowest_t *draw, bool counted, char *text, size_t size)
{
    if (!counted) snprintf(text, size, "none");
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
                    " longest, its looks %" PRIu32 " us; one that did not %" PRIu32 " us; %" PRIu32 " draws and %" PRIu32
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
