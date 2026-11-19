#pragma once
/*
 * dgx_ui_stats.h
 *
 *  The statistics of painting: a pass of dgx_ui_paint() measured, how long it took and what it drew, counted into a
 *  struct of the program's. A program that names none of it carries none of it.
 *
 *      static dgx_ui_paint_stats_t stats;                // zeros: the clock is esp_timer_get_time()
 *      dgx_ui_paint_measured(&ui, &paint, now, &stats);  // in the place of dgx_ui_paint()
 *      if (now - told_ms >= 10000) dgx_ui_paint_stats_log(&stats, "home"), told_ms = now;
 *      stats = (dgx_ui_paint_stats_t){.clock_us = stats.clock_us}; // begun anew, the clock kept
 *
 *  A pass drew when a look was called for a part of a widget. A loop that wakes for a finger or a key passes often and
 *  draws seldom, so the passes that drew and those that did not are counted apart, each with its time: the mean of a
 *  pass that drew is not that of the empty ones, and the mean of an empty one is what the core costs when nothing
 *  changed.
 *
 *  What the time holds: the whole call of dgx_ui_paint() as the clock reads it around the call: the layout, the places
 *  of a keyboard's band gone and of popups closed composed again, the walk and the looks. The looks' time is the sum of
 *  the parts a look was called for, the program's strips with it. Not held: the count of the widgets after the pass. A
 *  part with no look, where the painter lays the paper, is the core's time, and a pass that only laid paper did not
 *  draw.
 *
 *  A struct is one tree's, with its popups: two trees drawn in turn by one paint take a struct each. It is the
 *  program's, one call's at a time; the library keeps nothing of it between calls. The measured call is the painter's
 *  own pass: it is not made from a look, nor from a function told by dgx_ui_changed(). dgx_ui_changed() and the frames
 *  of a transition are not counted.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stddef.h>
#include <stdint.h>

#include "dgx_ui_paint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A draw of a widget: where the widget lies, the level of its tree in the chain of popups (0 the tree measured,
 * 1 the popup over it, and so on) and its index there, as dgx_ui_dump() numbers them; how long the draw took. Whether
 * a draw was counted the counts say, not this: a struct of zeros has zeros here, the tree's root, until a pass draws
 * (draws above 0); after a pass that drew nothing slowest_last is -1, -1, 0 (draws_last 0).
 */
typedef struct dgx_ui_paint_slowest_t {
    int8_t   level;
    int16_t  widget;
    uint32_t us;
} dgx_ui_paint_slowest_t;

/** @brief The statistics of the passes of a tree: the program's, zeros to begin. */
typedef struct dgx_ui_paint_stats_t {
    int64_t (*clock_us)(void);           ///< the program's clock in microseconds; NULL: esp_timer_get_time() on a chip
    uint32_t               passes;       ///< the passes measured
    uint64_t               passes_us;    ///< and their time
    uint32_t               last_us;      ///< the time of the last pass
    uint32_t               drawing_passes;     ///< the passes that drew: a look was called for a part
    uint64_t               drawing_us;         ///< and their time
    uint32_t               last_drawing_us;    ///< the time of the last pass that drew
    uint32_t               longest_drawing_us; ///< and of the longest
    uint32_t               looks_last_us;      ///< the looks' time at the last pass: the sum of the parts they were called for
    uint64_t               looks_us;           ///< at all of them
    uint16_t               draws_last;   ///< draws of widgets at the last pass: a widget drawn by a composing and by the walk of one pass is two
    uint16_t               parts_last;   ///< parts a look was called for at the last pass
    uint32_t               draws, parts; ///< at all of them
    uint16_t               widgets;      ///< at the last pass, the tree and its popups: the widgets in use, as dgx_ui_dump() says "in use"
    uint16_t               room;         ///< and the room of their arrays and blocks of nodes
    dgx_ui_paint_slowest_t slowest_last; ///< the slowest draw of the last pass
    dgx_ui_paint_slowest_t slowest;      ///< of all of them
} dgx_ui_paint_stats_t;

/**
 * @brief dgx_ui_paint(), measured: the same pass, its time and what it drew counted into stats. The paint's strips the
 * program named still draw, each part offered them as before; the member is the paint's own again after the call.
 * @return As dgx_ui_paint().
 */
bool dgx_ui_paint_measured(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms, dgx_ui_paint_stats_t *stats);

/**
 * @brief The statistics in words, one line, as snprintf: the passes and those that drew, the mean of a pass that drew
 * and of one that did not, the last and the longest, the looks' share, the draws and parts, the widgets, and the
 * slowest draw of all and of the last pass. In microseconds, integers. name: the tree's, for the reader.
 * @return The length of the whole line.
 */
int dgx_ui_paint_stats_text(const dgx_ui_paint_stats_t *stats, const char *name, char *text, size_t size);

/** @brief That line in the log, with the library's tag: 480 bytes of the caller's stack, the line of the largest counts
 * and a name of 80 letters. */
void dgx_ui_paint_stats_log(const dgx_ui_paint_stats_t *stats, const char *name);

#ifdef __cplusplus
}
#endif
