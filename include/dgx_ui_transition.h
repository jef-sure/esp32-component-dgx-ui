/*
 * dgx_ui_transition.h
 *
 *  Pages that change by a transition: a slide or a crossfade, made from two pictures of the whole screen that the
 *  program gives, for a board with the memory for them. A program that names none of this carries none of it; a page
 *  without the pictures changes at once.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dgx_ui.h"
#include "dgx_ui_paint.h"

#ifdef __cplusplus
// @formatter:off
extern "C" {
// @formatter:on
#endif

/** @brief How a page comes: the new screen comes in from the right, from the left, from below, from above, or the two
 * screens fade into each other. */
enum { DGX_UI_SLIDE_LEFT, DGX_UI_SLIDE_RIGHT, DGX_UI_SLIDE_UP, DGX_UI_SLIDE_DOWN, DGX_UI_CROSSFADE };

/**
 * @brief A transition between two screens of pages: the program's two pictures of the whole screen and how long it
 * takes; the rest the module's.
 *
 * The pictures are virtual screens as large as the display and of its depth, in the program's memory (PSRAM on a
 * board that has it): 2.5 MB of RGB565 for both on 1024 by 600, 1,228,800 bytes each. The frames are made from the pictures alone, so the
 * trees are not drawn while they run:
 *
 *     static dgx_ui_transition_t slide = {.ms = 250};       // from and to made at the start, in PSRAM
 *     ...
 *     slide.from = dgx_vscreen_from_array(heap_caps_malloc(1024 * 600 * 2, MALLOC_CAP_SPIRAM), 1024, 600, 16, DgxScreenRGB);
 *     slide.to   = dgx_vscreen_from_array(heap_caps_malloc(1024 * 600 * 2, MALLOC_CAP_SPIRAM), 1024, 600, 16, DgxScreenRGB);
 *     ...
 *     // the home's button: dgx_ui_page_open_by(&home, &settings, &paint, &slide, DGX_UI_SLIDE_LEFT);
 *     // the settings' root, on(DGX_UI_BACK): dgx_ui_page_back_by(&home, &paint, &slide, DGX_UI_SLIDE_RIGHT);
 *     ...
 *     if (!slide.running) dgx_ui_pointer(&home, &finger);    // no input while it runs: spent
 *     busy = slide.running ? dgx_ui_transition_frame(&slide, &paint, now) : dgx_ui_paint(&home, &paint, now);
 *
 * The loop calls dgx_ui_transition_frame() in place of dgx_ui_paint() while it runs: a pass would draw over the
 * frames. It gives the trees no finger and no key meanwhile; a finger that went down during the frames and is down
 * after them comes without its press, and is nobody's till it goes up. What changes in the trees meanwhile is drawn by
 * the first pass after the last frame, and a page under the top one when it shows again.
 *
 * A page changes at once, and that is no error: with no pictures or ms 0, as the program chose; with a line in the log,
 * a picture whose size or depth is not the screen's (the screen turned since they were made), the keyboard's band open,
 * as the pictures do not hold what lies under it, a crossfade on a display that cannot be read with no buffer in the
 * paint of a row of it.
 * A slide is two transfers a frame, at an offset, on any display. A crossfade is mixed in the frame buffer of a display
 * that reads it (DSI), as DGX_VSCREEN_ALPHA does there, and on one that cannot be read strip by strip in the paint's
 * buffer; on a display of 24 bits DGX mixes pixel by pixel, which is slow.
 */
typedef struct dgx_ui_transition_t {
    dgx_screen_t *from, *to; ///< the program's: two pictures of the whole screen, of the display's depth; NULL: at once
    uint16_t      ms;        ///< how long; 0: at once
    /* the module's */
    uint8_t  kind;     ///< DGX_UI_SLIDE_LEFT ... DGX_UI_CROSSFADE, as the call gave it
    bool     running;  ///< frames to come: the loop calls dgx_ui_transition_frame() in place of dgx_ui_paint()
    bool          started;  ///< its first frame drawn, start_ms set
    uint32_t      start_ms; ///< the time of the first frame
    dgx_ui_rect_t area;     ///< where the frames move: the screen, or the place of a page in home's place
    dgx_ui_rect_t outside;  ///< what else changed, outside the area: written by the last frame alone
} dgx_ui_transition_t;

/**
 * @brief dgx_ui_page_open() by a transition: the screen as it stands into `from`, the page opened, the trees marked
 * drawn by dgx_ui_changed() with nobody told, the screen as it will be into `to`; the frames then run. Called from an
 * on(), at the time of the last pass. Asked for while one runs, a new one starts from the trees as they stand.
 * @return dgx_ui_page_open()'s.
 */
bool dgx_ui_page_open_by(dgx_ui_t *home, dgx_ui_t *page, dgx_ui_paint_t *paint, dgx_ui_transition_t *tr, int kind);

/**
 * @brief dgx_ui_page_back() by a transition: `from` composed with the top page still open, then the page closed. Call
 * it from the page's root's on(DGX_UI_BACK): then the key back, a "back" button that calls dgx_ui_page_back() and one
 * that calls this all go by one transition; called from a button and from that on() as well, it runs once.
 * @return dgx_ui_page_back()'s.
 */
bool dgx_ui_page_back_by(dgx_ui_t *home, dgx_ui_paint_t *paint, dgx_ui_transition_t *tr, int kind);

/**
 * @brief The next frame of a transition on the paint's screen; the first sets its start. The last puts `to` whole,
 * and the next dgx_ui_paint() draws what changed meanwhile. The next frame is asked at once (paint's wake_ms 0): a
 * frame of a whole screen takes a display long enough to be its own pace.
 * @return true while it runs, the last frame included: the loop paints once more after it.
 */
bool dgx_ui_transition_frame(dgx_ui_transition_t *tr, dgx_ui_paint_t *paint, uint32_t now_ms);

#ifdef __cplusplus
// @formatter:off
}
// @formatter:on
#endif
