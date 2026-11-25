#pragma once
/*
 * dgx_ui_stats.h
 *
 *  The statistics of painting: a pass of dgx_ui_paint() or of dgx_ui_paint_changes() measured, how long it took and what it
 *  drew, counted into a struct of the program's; each widget of a tree the program names counted apart if it gives an
 *  array for them; the frames of a transition in a struct of their own. A program that names none of it carries none
 *  of it, and each call only what it names.
 *
 *      static dgx_ui_paint_stats_t  stats;                // zeros: the clock is esp_timer_get_time()
 *      static dgx_ui_widget_stats_t home_widgets[16];     // optional: a count for each widget of the home tree
 *      stats.per_widget_tree = &home, stats.per_widget = home_widgets, stats.per_widget_count = 16;
 *      dgx_ui_paint_measured(&ui, &paint, now, &stats);   // in the place of dgx_ui_paint()
 *      if (now - told_ms >= 10000) dgx_ui_paint_stats_log(&stats, "home"), dgx_ui_widget_stats_log(&stats, "home"), told_ms = now;
 *      dgx_ui_paint_stats_restart(&stats);                // begun anew: the clock and the array named kept
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
 *  A struct is one tree's, with its popups, and one way's: two trees drawn in turn by one paint take a struct each, and
 *  so do a tree's passes by dgx_ui_paint() and by dgx_ui_paint_changes() if a program had both. It is the program's, one
 *  call's at a time; the library keeps nothing of it between calls. The measured calls are the painter's own: they are
 *  not made from a look, nor from a function told by dgx_ui_paint_changes().
 *
 *  A pass by dgx_ui_paint_changes_measured() draws nothing itself: a rectangle told is its draw and its part, the program's
 *  function told it (the composing) its drawing's time, and the slowest draw the slowest rectangle's, "a rectangle
 *  told". The looks it calls under an empty clip are the core's time there. Its rectangles are not counted for each
 *  widget.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stddef.h>
#include <stdint.h>

#include "dgx_ui_paint.h"
#include "dgx_ui_transition.h"

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

/**
 * @brief The count of one widget, an entry of the program's array (dgx_ui_paint_stats_t::per_widget) by the widget's
 * node index. The entry keeps the widget counted at its index, only compared: another widget there begins the entry
 * anew (a run-time widget freed and another made at its address continues it). A sum that would pass 32 bits halves
 * the entry's draws and sum first: the mean is kept.
 */
typedef struct dgx_ui_widget_stats_t {
    const dgx_ui_widget_t *widget;     ///< the widget counted at this index; NULL none yet
    uint32_t               draws;      ///< its draws
    uint32_t               total_us;   ///< their time: the mean is total_us / draws
    uint32_t               longest_us; ///< the longest
} dgx_ui_widget_stats_t;

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
    uint32_t               looks_last_us;      ///< the drawing's time at the last pass: the sum of the parts the looks were called
                                               ///< for; of a pass by dgx_ui_paint_changes_measured(), the program's function told
    uint64_t               looks_us;           ///< at all of them
    uint16_t               draws_last;   ///< draws of widgets at the last pass
    uint16_t               parts_last;   ///< parts a look was called for at the last pass
    uint32_t               draws, parts; ///< at all of them
    uint16_t               widgets;      ///< at the last pass, the tree and its popups: the widgets in use, as dgx_ui_dump() says "in use"
    uint16_t               room;         ///< and the room of their arrays and blocks of nodes
    dgx_ui_paint_slowest_t slowest_last; ///< the slowest draw of the last pass
    dgx_ui_paint_slowest_t slowest;      ///< of all of them
    const dgx_ui_t        *per_widget_tree;  ///< the tree whose widgets are counted each: the measured one or a popup over it
                                             ///< (a page); only compared, never read: a tree freed while named and another at
                                             ///< its address would be counted; set NULL when it goes
    dgx_ui_widget_stats_t *per_widget;       ///< the program's array of them by node index, one struct's; NULL: none (these
                                             ///< three members are 12 bytes of every struct, no more)
    uint16_t               per_widget_count; ///< its entries: a widget at an index past them (a block of nodes) is not counted
} dgx_ui_paint_stats_t;

#if UINTPTR_MAX == 0xFFFFFFFFu
#ifdef __cplusplus
static_assert(sizeof(dgx_ui_paint_stats_t) == 104, "the statistics of a pass are 104 bytes on a 32-bit chip");
static_assert(sizeof(dgx_ui_widget_stats_t) == 16, "an entry of a widget's statistics is 16 bytes on a 32-bit chip");
#else
_Static_assert(sizeof(dgx_ui_paint_stats_t) == 104, "the statistics of a pass are 104 bytes on a 32-bit chip");
_Static_assert(sizeof(dgx_ui_widget_stats_t) == 16, "an entry of a widget's statistics is 16 bytes on a 32-bit chip");
#endif
#endif

/**
 * @brief dgx_ui_paint(), measured: the same pass, its time and what it drew counted into stats. The paint's strips the
 * program named still draw, each part offered them as before; the member is the paint's own again after the call.
 * @return As dgx_ui_paint().
 */
bool dgx_ui_paint_measured(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms, dgx_ui_paint_stats_t *stats);

/**
 * @brief dgx_ui_paint_changes(), measured: the same pass, telling the same rectangles to tell, each timed; counted into stats
 * as above. tell NULL: dgx_ui_paint_changes() is given NULL, as asked, and only the call is timed.
 * @return As dgx_ui_paint_changes().
 */
bool dgx_ui_paint_changes_measured(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms, dgx_ui_paint_changes_t tell, void *ctx,
                                   dgx_ui_paint_stats_t *stats);

/** @brief The counts to zero, and the entries of the array named; the clock and the naming of the array kept. */
void dgx_ui_paint_stats_restart(dgx_ui_paint_stats_t *stats);

/**
 * @brief The statistics in words, one line, as snprintf: the passes and those that drew, the mean of a pass that drew
 * and of one that did not, the last and the longest, the drawing's share, the draws and parts, the widgets, and the
 * slowest draw of all and of the last pass. In microseconds, integers. name: the tree's, for the reader.
 * @return The length of the whole line.
 */
int dgx_ui_paint_stats_text(const dgx_ui_paint_stats_t *stats, const char *name, char *text, size_t size);

/** @brief That line in the log, with the library's tag: 480 bytes of the caller's stack, the line of the largest counts
 * and a name of 80 letters. */
void dgx_ui_paint_stats_log(const dgx_ui_paint_stats_t *stats, const char *name);

/** @brief The count of widget i of the array named, one line, as snprintf: "widget 3: 120 draws, 210 us mean, 900 us
 * longest", or "widget 3: not drawn". */
int dgx_ui_widget_stats_text(const dgx_ui_paint_stats_t *stats, int i, char *text, size_t size);

/** @brief A line in the log for each widget of the array named that was drawn. */
void dgx_ui_widget_stats_log(const dgx_ui_paint_stats_t *stats, const char *name);

/**
 * @brief The frames of a transition (dgx_ui_transition.h), a struct of their own, one transition's: what a frame
 * costs, and what was shown. A frame's time is the clock around dgx_ui_transition_frame(); the rate shown is that of
 * the transitions measured from their first frame to their end, the periods between the starts of their frames over
 * the span from the start of the first to the start of the last, the loop's waits in it. A frame of 30 ms on a loop
 * that waits a tick of 10 ms between frames is shown at 25 a second, not 33: the two numbers disagreeing tell a loop
 * that waits too long. A transition begun again before its end drops the run before it from the rate, its frames kept
 * in the cost; one never run to its end, or whose first frame this struct did not measure (begun anew in the middle
 * of it, or its first frames drawn by dgx_ui_transition_frame()), is in the cost only. The start (the two pictures
 * composed by dgx_ui_page_open_by()) is not a frame.
 */
typedef struct dgx_ui_frame_stats_t {
    int64_t (*clock_us)(void); ///< the program's clock in microseconds; NULL: esp_timer_get_time() on a chip
    uint32_t frames;           ///< the frames measured
    uint64_t frames_us;        ///< and their time: what they cost
    uint32_t last_us;          ///< the last frame's
    uint32_t longest_us;       ///< and the longest
    uint32_t transitions;      ///< the transitions measured from their first frame to their end
    uint32_t shown_periods;    ///< the periods between the starts of their frames: a frame fewer each
    uint64_t shown_us;         ///< from the start of the first frame of each to the start of its last: the rate shown
    int64_t  run_start_us;     ///< the library's: when the first frame of the transition running began
    uint32_t run_frames;       ///< the library's: its frames so far
    bool     run_seen;         ///< the library's: the first frame of the transition running was measured here
} dgx_ui_frame_stats_t;

/**
 * @brief dgx_ui_transition_frame(), measured into stats. With no transition running it is called and nothing is
 * counted.
 * @return As dgx_ui_transition_frame().
 */
bool dgx_ui_transition_frame_measured(dgx_ui_transition_t *tr, dgx_ui_paint_t *paint, uint32_t now_ms, dgx_ui_frame_stats_t *stats);

/** @brief The frames in words, one line, as snprintf: "frames slide: 30 frames, transitions run to their end: 2; a frame
 * 29700 us mean, 28700 last, 34000 longest; 25.0 frames a second shown". */
int dgx_ui_frame_stats_text(const dgx_ui_frame_stats_t *stats, const char *name, char *text, size_t size);

/** @brief That line in the log, with the library's tag. */
void dgx_ui_frame_stats_log(const dgx_ui_frame_stats_t *stats, const char *name);

#ifdef __cplusplus
}
#endif
