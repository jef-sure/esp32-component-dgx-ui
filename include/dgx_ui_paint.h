#pragma once
/*
 * dgx_ui_paint.h
 *
 *  The painter of dgx-ui: a tree drawn on a screen of DGX, only what changed, in one batch. It sets the clip of the
 *  screen to each widget's box while the widget's look draws, and gives the screen back the clip it had. Colors
 *  are indices of the program's table, packed for the screen (dgx_screen_rgb()): a table changed is the whole
 *  interface in other colors.
 *
 *  On a display behind a bus of 18 or 24 bits a field of text is filled before its letters are written, and blinks.
 *  The program that has one names the strips and gives a buffer:
 *
 *      static uint8_t buffer[320 * 3 * 8]; // eight rows of a display 320 points wide, 3 bytes a point
 *      dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 4,
 *                              .buffer = buffer, .buffer_size = sizeof buffer, .strips = &dgx_ui_strips};
 *
 *  Then every widget without children is drawn in strips of a virtual screen of the display's depth over the buffer,
 *  as many rows a strip as the buffer takes, each strip sent once: every pixel once, nothing blinks. What it costs: a
 *  widget changed is sent whole, all of its box, where straight only what changed would be written; a buffer of a
 *  row of the widget at least (its width × 3 bytes), more rows fewer transfers. A widget too wide for a row of the
 *  buffer goes straight, as without one, told once in the log; a widget with children goes straight too, as it fills
 *  around them. Displays of 16 bits draw straight, their text not filled first, but for a widget on a drawn background
 *  (below), which the strips take there too. The strips need the virtual screens of DGX (CONFIG_DGX_ENABLE_VSCREEN); a
 *  program that does not name them does not carry them.
 *
 *  The background under the tree is a colour, `background`, or a picture the program puts there by a function of its
 *  own, `background_draw`: a weather map under a column of labels, the frame of a game under a dialog.
 *
 *      static void map(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
 *      {
 *          // the piece x, y, w, h of the map, a virtual screen of RGB565 as large as the display
 *          dgx_vscreen_region_to_screen(paint->scr, x - paint->ox, y - paint->oy, ctx, x, y, w, h, 0);
 *      }
 *      ...
 *      dgx_ui_background_set(&paint, map, map_screen);
 *
 *  A look whose paper is what lies under it lays the map's piece of its box, then draws its letters and pictures over
 *  it: the label "5 m/s" gets the map under its letters. With the strips named the piece and the letters go into a
 *  strip and leave once, on a display of 16 bits too, and the soft edges of the letters mix with the map. Drawn
 *  straight on a display behind a bus, the label blinks (the map, then the letters), and the edges, which cannot read
 *  the display, mix with the colour `background`: give it the map's main colour. A frame that fills shows its fill,
 *  as always. When the map changes, the program says where: dgx_ui_background_changed(). The laying of a drawn
 *  background is in the library only for a program that calls dgx_ui_background_set().
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dgx_screen.h"
#include "dgx_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The painter's way of drawing through the buffer in strips: dgx_ui_strips, for paint's strips. */
typedef struct dgx_ui_strips_t dgx_ui_strips_t;

/**
 * @brief The program's picture under the tree, paint's background_draw: it puts the rectangle x, y, w, h of the
 * tree's screen on paint->scr at x - paint->ox, y - paint->oy, the clip of the screen set to that rectangle. The
 * screen is the display, or a strip of the painter's over the buffer of the display's depth (18 or 24 bits too).
 * ctx is the program's background_ctx.
 */
struct dgx_ui_paint_t;
typedef void (*dgx_ui_background_draw_t)(struct dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx);

/** @brief The library's laying of a drawn background, named by dgx_ui_background_set(). */
typedef struct dgx_ui_drawn_background_t dgx_ui_drawn_background_t;

/** @brief The canvases a paint made for its widgets of DGX_UI_CANVAS (dgx_ui_canvas()), the painter's own. */
typedef struct dgx_ui_canvases_t dgx_ui_canvases_t;

/** @brief How a tree is drawn on a screen. Filled by the program; the members after `cover` are the painter's. */
struct dgx_ui_paint_t {
    dgx_screen_t                    *scr;             ///< where
    const uint32_t                  *colors;          ///< the table: an index of a look to a color of the screen
    uint16_t                         color_count;     ///< its size
    uint8_t                          background;      ///< the index of the colour under the tree; with background_draw
                                                      ///< the colour a look is told as its paper where it cannot lay
                                                      ///< the picture (the soft edges of letters drawn straight, a look
                                                      ///< of the program's own that does not lay it): the picture's
                                                      ///< main colour
    dgx_ui_background_draw_t         background_draw; ///< the program's picture under the tree (above), set by
                                                      ///< dgx_ui_background_set(); NULL: the colour `background`
    void                            *background_ctx;  ///< the program's, given to background_draw; set by
                                                      ///< dgx_ui_background_set()
    const dgx_ui_drawn_background_t *drawn;           ///< set by dgx_ui_background_set(): without it background_draw is
                                                      ///< not called, the colour `background` is under the tree
    void                            *buffer;          ///< the program's memory the painter draws in: a look that needs
                                                      ///< it (pictures of 4 and 8 bits), and the strips; start it on a
                                                      ///< word boundary, DMA-capable on a chip; NULL none
    size_t                           buffer_size;     ///< its bytes
    const dgx_ui_strips_t           *strips;          ///< &dgx_ui_strips: the painter draws through the buffer in
                                                      ///< strips; name it for a display behind a bus of 18 or 24 bits
                                                      ///< (above); NULL: straight
    dgx_ui_rect_t                    cover;           ///< the band of DGX-DI's keyboard while it is open: not drawn
                                                      ///< there; else empty
    int16_t                          ox, oy;          ///< the pixel of the tree's screen at (0, 0) of scr: 0, or a
                                                      ///< buffer's place when composing
    /* the painter's */
    uint8_t                          paper;           ///< while a look draws: the index of the color under the widget,
                                                      ///< its parent's fill or the background
    bool                             on_background;   ///< while a look draws: what lies under the widget is the
                                                      ///< program's drawn background, no box above it fills;
                                                      ///< dgx_ui_lay_paper() lays it
    bool                             whole;           ///< while a look draws: nothing of the widget is on the screen as
                                                      ///< it should be
    bool                             composing;       ///< while a look draws: dgx_ui_paint_area() composes, nothing is
                                                      ///< to be remembered
    uint8_t                          part, parts;     ///< while a look draws: the part of the box it is called for, of
                                                      ///< so many
    const dgx_ui_t                  *popup_over;      ///< while a tree is drawn: the popup over it, whose rectangle and
                                                      ///< those of the popups over it are covers after the keyboard's
                                                      ///< band; NULL none
    dgx_screen_t                    *rgb565;          ///< the look of RGB565: a virtual screen pointed at each picture,
                                                      ///< made at its first need
    dgx_screen_t                    *indexed;         ///< the look of indexed pictures: an 8-bit virtual screen over
                                                      ///< the buffer, made at its first need
    uint16_t                        *lut;             ///< and the RGB565 of the 256 indices, made at its first need
    dgx_screen_t                    *strip;           ///< the strips: a virtual screen of the display's depth over the
                                                      ///< buffer, made at its first need
    size_t                           buffer_used;     ///< while a strip is drawn: its bytes at the start of the buffer
    uint32_t                         now_ms;          ///< the time of the last dgx_ui_paint(), given to the looks of a
                                                      ///< composing (dgx_ui_paint_area(), a popup closed, the drawn
                                                      ///< background changed); a program that composes with a paint
                                                      ///< dgx_ui_paint() never draws sets it before
    uint32_t                         wake_ms;         ///< after dgx_ui_paint(): no sooner than in so many ms from its now_ms a look asks to be called again, the least they named (dgx_ui_wake_in()); 0 the next pass; DGX_UI_WAKE_NONE none asked
    bool                             wake_named;      ///< while a look draws: it named its moment
    bool                             painted;         ///< it drew a pass: now_ms and wake_ms are those of its last
    dgx_ui_canvases_t               *canvases;        ///< the canvases this paint made (dgx_ui_canvas()) for the tree
                                                      ///< it draws and the popups over it, freed by
                                                      ///< dgx_ui_paint_free(); NULL none
    dgx_ui_rect_t                    cover_was;       ///< the keyboard's band as the last pass left it: where it goes,
                                                      ///< the tree is composed again
}; /* its name, dgx_ui_paint_t: dgx_ui.h */

/*
 * The rules of a look (struct dgx_ui_look_t, dgx_ui.h): what its function draws and when.
 *
 * The function draws what differs between the widget's state_drawn and state, or all of it when the painter
 * says whole; the clip of the screen is set to the part of the widget's box it may draw, and it may be called for
 * several parts in turn. It should write every pixel once. It draws the widget's box less paint's ox and oy: the
 * screen may be a buffer that holds a part of the tree's screen. So it is drawn in strips on a display of 18 or 24
 * bits (above): whole in each strip, composing in all but the last strip of a part.
 *
 * Its paper, where it is what lies under the widget (DGX_UI_PAPER), it lays by dgx_ui_lay_paper(): on a drawn
 * background (paint->on_background) that is the program's picture, and the look draws its letters and pictures over
 * it without paper, with no field, no DGX_FONT_FILL and no DGX_IMAGE_OPAQUE.
 *
 * A look that remembers what it drew (a variant fitted, a line scrolled to) works it out the same in every part and
 * stores it only in the last part of the painter's own pass: paint->part == paint->parts - 1 and not composing.
 * Stored in the first, it would find nothing changed in the parts after.
 */

/** @brief No look asks for time: paint's wake_ms, dgx_ui_wake(). */
#define DGX_UI_WAKE_NONE UINT32_MAX

/**
 * @brief Called by a look that returns true, asking for time: it is to be called again no sooner than in ms. A look
 * that asks for time and names nothing is called at the next pass, as before. Each look that asks names its moment
 * again at every pass it is drawn, so the least of them is never stale after the loop woke early.
 */
static inline void dgx_ui_wake_in(dgx_ui_paint_t *paint, uint32_t ms)
{
    if (ms < paint->wake_ms) paint->wake_ms = ms;
    paint->wake_named = true;
}

/**
 * @brief How long the loop may wait before it calls dgx_ui_paint() and dgx_ui_time() again: the least of the moments
 * the looks named at the last pass (counted from its now_ms) and of the next repeat of a key held (ui; NULL none).
 *
 *     const bool     more = dgx_ui_paint(&ui, &paint, now) | dgx_ui_time(&ui, now);
 *     const uint32_t wait = dgx_ui_wake(&paint, &ui, now);
 *     // wait for a message at most so long: a tick at least; DGX_UI_WAKE_NONE, till a message comes
 *
 * 0 is "the next pass": the wait is a tick at least, a loop that waits nothing for it spins. DGX_UI_WAKE_NONE: nothing
 * asks for time, the loop may wait for a message. A loop that asks dgx_ui_paint()'s true alone works as before.
 *
 * Two trees drawn in turn by one paint (a bar and a page, each its root given) are painted with the same now_ms:
 * dgx_ui_wake() after the last of them gives the least of what all of them asked. A program that takes the time anew
 * for each tree has only the last tree's moments.
 */
static inline uint32_t dgx_ui_wake(const dgx_ui_paint_t *paint, const dgx_ui_t *ui, uint32_t now_ms)
{
    uint32_t wait = DGX_UI_WAKE_NONE;
    if (paint && paint->wake_ms != DGX_UI_WAKE_NONE) {
        const uint32_t gone = now_ms - paint->now_ms; /* by difference: now_ms runs over in 49.7 days */
        wait                = gone >= paint->wake_ms ? 0 : paint->wake_ms - gone;
    }
    if (ui && ui->repeat_key >= 0) {
        const int32_t left = (int32_t)(ui->repeat_at - now_ms);
        if ((uint32_t)(left > 0 ? left : 0) < wait) wait = (uint32_t)(left > 0 ? left : 0);
    }
    return wait;
}

/** @brief A color of the table, packed for the screen; 0 past its end. */
static inline uint32_t dgx_ui_color(const dgx_ui_paint_t *paint, int index)
{
    return paint->colors && index >= 0 && index < paint->color_count ? paint->colors[index] : 0;
}

/**
 * @brief The paper of a look laid on a rectangle of the tree's screen: the colour of `paper`, an index of the table;
 * DGX_UI_PAPER, what lies under the widget: the colour paint->paper, or, on a drawn background (on_background), the
 * program's picture, by background_draw under a clip of the rectangle.
 *
 * Called from a look, under the painter's clip of the part it draws; paint's ox and oy are taken off here. A look of
 * the program's own lays its paper so, and on the background draws its letters and pictures without paper (above).
 */
void dgx_ui_lay_paper(dgx_ui_paint_t *paint, uint8_t paper, int x, int y, int w, int h);

/**
 * @brief An area filled with a colour, less the children of widget i that are drawn: for a look of a widget that
 * holds children, as the looks of a box and a frame do it, so that drawn whole no pixel is written twice.
 *
 * Called from a look, under the painter's clip of the part it draws. The children left out are the shown ones that a
 * look draws (their own or their kind's), each cut to the area; a child with no look covers nothing, so its place is
 * filled. The area is any rectangle in pixels of the tree's screen (paint's ox and oy are taken off here): the whole
 * box, or a part of it, as a frame gives the inside of its border. A widget of neither DGX_UI_COLUMN nor DGX_UI_ROW,
 * whose children sit where they ask and may lie over each other, has the area filled whole, children and all.
 *
 * @param area  The rectangle to fill, in pixels of the tree's screen.
 * @param color The colour packed for the screen: dgx_ui_color(paint, index).
 */
void dgx_ui_fill_around(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, dgx_ui_rect_t area, uint32_t color);

/** Parameters of the frame's look: a border; its fill is the look's own fill, dgx_ui_look_t::fill. */
typedef struct dgx_ui_frame_look_t {
    uint8_t border; ///< index of the table
    uint8_t width;  ///< of the border, in points of the style
} dgx_ui_frame_look_t;

/** Parameters of the text's look. */
typedef struct dgx_ui_text_look_t {
    uint8_t ink;   ///< index of the letters
    uint8_t paper; ///< index under them; DGX_UI_PAPER: what lies under the widget
} dgx_ui_text_look_t;

/** The paper of a look that is not its own: the color under the widget. */
enum { DGX_UI_PAPER = 0xff };

/** The looks a widget of each kind has when its look is NULL: indices 0 the paper, 1 the ink, 2 a fill, 3 a border. */
extern const dgx_ui_look_t dgx_ui_look_box, dgx_ui_look_frame, dgx_ui_look_text;

/*
 * The looks of the controls and the pictures. A control has no look of its kind: it names one, so that a program
 * carries only the looks it uses. Each writes every pixel of the widget's box once, whatever its state; a program
 * makes a look of its own colours by a dgx_ui_look_t of the same draw (dgx_ui_draw_button() and the others below) and
 * its own parameters, a constant that may lie in flash.
 */

/**
 * The focus, in the looks of a button, a dropdown, a radio and a checkbox: a frame of a point round the widget's box,
 * inside it (a button's round its face, moving with it), in the colour `focused`; the inside is drawn as it is without
 * the focus. The layout gives such a widget with a text a point more above and below its lines
 * for it. A switch's and a slider's focus is on the knob: hollow, its border in `focused`.
 *
 * Parameters whose `focused` is DGX_UI_PAPER show no focus, in each of the six looks: no frame, the knob solid. It is for
 * a program of the finger alone, where a touch gives the focus and the last widget touched would keep its mark. A
 * button's or a dropdown's `focused` of the face's colour shows none either, the frame not seen on the face.
 */

/** Parameters of a button: a face over a shadow; pressed, the face moves onto the shadow. Indices of the table. */
typedef struct dgx_ui_button_look_t {
    uint8_t face, pressed, focused, disabled; ///< the face, the face pressed, the frame of the focus (DGX_UI_PAPER: none shown), the face disabled
    uint8_t text, text_disabled;              ///< the text, the variant 0 of its style, in the middle of the face
    uint8_t shadow;                           ///< under the face, right and below
    uint8_t depth;                            ///< of the shadow, in points of the style; 0: a flat button
} dgx_ui_button_look_t;

/** Parameters of a switch: a track with a knob at its left end (0) or its right end (1), a point wider toward the
 * middle while a finger is on it. */
typedef struct dgx_ui_switch_look_t {
    uint8_t off, on, knob, focused; ///< the track off and on, the knob, the knob's border with the focus (the knob hollow then; DGX_UI_PAPER: none shown)
    uint8_t disabled;               ///< the knob when disabled
} dgx_ui_switch_look_t;

/** Parameters of a slider: a track in the middle of its height, the part up to the knob filled, a knob of 3 points,
 * a point wider on each side while a finger is on it. */
typedef struct dgx_ui_slider_look_t {
    uint8_t track, fill, knob, focused; ///< the track, its filled part, the knob, the knob's border with the focus (the knob hollow then; DGX_UI_PAPER: none shown)
    uint8_t disabled;                   ///< the knob when disabled
    uint8_t track_h;                    ///< the height of the track, in points of the style
} dgx_ui_slider_look_t;

/** Parameters of a picture of 1 bit: its ink and its paper, both written, every pixel of the box once. */
typedef struct dgx_ui_picture_look_t {
    uint8_t ink;   ///< index of the table
    uint8_t paper; ///< index of the table; DGX_UI_PAPER: what lies under the widget
} dgx_ui_picture_look_t;

/**
 * Parameters of a field typed into: its text and, focused, a caret where the next letter goes. A secret field (a
 * password, a PIN) shows a star for each letter; the text box keeps the letters themselves.
 */
typedef struct dgx_ui_input_look_t {
    uint8_t ink, caret; ///< indices of the table: the caret is the focus
    uint8_t paper;      ///< index of the table; DGX_UI_PAPER: what lies under the widget
    bool    secret;     ///< a star for each letter
} dgx_ui_input_look_t;

/**
 * Parameters of a list: its rows the lines of its text box, one picked (its value) on a bar.
 */
typedef struct dgx_ui_list_look_t {
    uint8_t ink, paper;     ///< a row's text and paper; paper DGX_UI_PAPER: what lies under the widget
    uint8_t pick, pick_ink; ///< the bar of the row picked and its text
    uint8_t focused;        ///< the bar while the list has the focus, its focus: a list has no frame
    uint8_t disabled;       ///< the text of every row while the list is disabled
    uint8_t bar;            ///< a list that scrolls: the bar on its right edge, two points wide, its length the share of
                            ///< the rows shown and its place theirs
} dgx_ui_list_look_t;

/** Parameters of a dropdown: a flat face with the option chosen and an arrow pointing down. */
typedef struct dgx_ui_dropdown_look_t {
    uint8_t face, pressed, focused, disabled; ///< the face, the face pressed, the frame of the focus (DGX_UI_PAPER: none shown), the face disabled
    uint8_t text, text_disabled;              ///< the option chosen
    uint8_t arrow;                            ///< the arrow; when disabled, text_disabled
} dgx_ui_dropdown_look_t;

/**
 * Parameters of a radio button and of a checkbox: a mark at the left of the box, in the middle of its height (a ring, a
 * square), in it a sign while chosen or on (a dot, a tick), and the widget's text to the right, the first line of its
 * text box, in the middle too. The mark stands two points in from the box's edges, the frame of the focus and then a
 * point of paper: as high as a line of the text where the box leaves that, at most the box's height and its width each
 * less four points; at the box's own height, a line and two points, it is a line less two points. Indices of the
 * table.
 */
typedef struct dgx_ui_check_look_t {
    uint8_t mark, focused, disabled; ///< the ring or the square, the frame of the focus round the box (DGX_UI_PAPER: none shown), the mark disabled
    uint8_t sign;                    ///< the dot or the tick; when disabled, the colour disabled
    uint8_t inside, pressed;         ///< inside the mark, and there while a finger is on the widget; DGX_UI_PAPER: what lies under it
    uint8_t ink, ink_disabled;       ///< the text
    uint8_t paper;                   ///< around them; DGX_UI_PAPER: what lies under the widget
} dgx_ui_check_look_t;

/**
 * A band of colour by value, for a look that colours by it: a value below `below` takes `color`, the first band that
 * holds it, the lowest band first; a value past the last band takes the look's own colour.
 */
typedef struct dgx_ui_band_t {
    int32_t below; ///< the band holds the values below this, and from the band before up
    uint8_t color; ///< index of the table
} dgx_ui_band_t;

/**
 * How the bands colour what is filled: dgx_ui_level_look_t::bands_by, dgx_ui_ring_look_t::bands_by. By the value
 * shown, all of it takes one colour, that of the value (a charge red when low); by place, each part takes the colour of
 * the value its own place stands for (a meter with a red zone at its end).
 */
enum { DGX_UI_BANDS_BY_VALUE, DGX_UI_BANDS_BY_PLACE };

/** Where a level bar fills from: dgx_ui_level_look_t::from. */
enum { DGX_UI_FROM_LEFT, DGX_UI_FROM_BOTTOM, DGX_UI_FROM_RIGHT, DGX_UI_FROM_TOP };

/**
 * Parameters of a level bar: the value of the widget's place, within min..max, as the filled part of its box from one
 * edge; max not above min shows it empty. The filled part's colour is `bar`, or from `bands`, by the value shown or by
 * place (`bands_by`).
 */
typedef struct dgx_ui_level_look_t {
    uint8_t              bar;        ///< the filled part; with bands, a value past the last of them
    uint8_t              rest;       ///< the rest of the box; DGX_UI_PAPER: what lies under the widget
    uint8_t              from;       ///< DGX_UI_FROM_LEFT, _BOTTOM, _RIGHT, _TOP
    uint8_t              band_count; ///< bands; 0: `bar` alone
    uint8_t              bands_by;   ///< DGX_UI_BANDS_BY_VALUE (0), DGX_UI_BANDS_BY_PLACE
    const dgx_ui_band_t *bands;      ///< the filled part's colours, the lowest band first; may lie in flash
} dgx_ui_level_look_t;

/**
 * The look of a level bar, `dgx_ui_look_level`, of the colours 3 filled and 2 the rest, from the left, on a widget of
 * DGX_UI_OWN with no behaviour, which nothing works: `.kind = DGX_UI_OWN, .value = &level, .max = 100, .want_h = 6,
 * .look = &dgx_ui_look_level`. Every pixel of the box once; when only its value changed, only the part between the
 * filled length drawn and the new one is written, or, by the value shown, all of the filled part when its colour
 * changed. A program that names it not carries none of it.
 */
extern const dgx_ui_look_t dgx_ui_look_level;

/**
 * Parameters of a ring scale: the value of the widget's place, within min..max, as an arc of a ring standing in the
 * middle of its box, as large as the box's lesser side. The angles are degrees clockwise from the top, which is not
 * DGX's way (counter-clockwise from the right): DGX's gauge at 225 and 270 is a start of -135 and a sweep of 270
 * here. A negative sweep goes counter-clockwise; a sweep of 0 is no scale, all of the box `paper`. The arc runs from
 * the start to the value, all of the sweep at max, none at min; its edge is a straight line from the middle. What lies
 * outside the sweep, and around the ring and inside it, is `paper`, laid less the widget's children: widgets stand
 * inside the ring as in a box. A program's own look of this draw whose `paper` is an index names the same index as
 * its look's `fill`, so that its children's paper is the ring's.
 */
typedef struct dgx_ui_ring_look_t {
    uint8_t              on, off;    ///< the arc up to the value, with no bands or past the last band; the rest of the ring
    uint8_t              paper;      ///< around the ring and inside it; DGX_UI_PAPER: what lies under the widget
    uint8_t              width;      ///< of the ring, in points; not less than the radius: a disc
    int16_t              start;      ///< the angle the arc starts at, in degrees clockwise from the top
    int16_t              sweep;      ///< all of the scale, in degrees, -360..360: clockwise, or counter-clockwise when negative
    uint8_t              band_count; ///< bands; 0: `on` alone
    uint8_t              bands_by;   ///< DGX_UI_BANDS_BY_PLACE (the arc by the value of each point) or DGX_UI_BANDS_BY_VALUE
    const dgx_ui_band_t *bands;      ///< the arc's colours, the lowest band first; may lie in flash
} dgx_ui_ring_look_t;

/**
 * The look of a ring scale, `dgx_ui_look_ring`: of the colours 3 the arc, 2 the rest of the ring, the paper what lies
 * under the widget, 3 points wide, from -135 over 270, on a widget of DGX_UI_OWN with no behaviour, children inside it
 * as in a column or a row. Every pixel of the box once, its children's too; when only its value changed, only the
 * pixels of the arc whose colour changed are written. A child hidden, shown or moved draws the ring whole with its
 * children. A program that names it not carries none of it.
 */
extern const dgx_ui_look_t dgx_ui_look_ring;

/**
 * The looks of the controls and pictures, of the colours 0 the paper, 1 the ink, 2 a fill, 3 a border:
 * - dgx_ui_look_button: BUTTON and DROPDOWN; its text from the widget's text box;
 * - dgx_ui_look_input: INPUT, its text in one line and, focused, a caret after it;
 * - dgx_ui_look_switch, dgx_ui_look_slider;
 * - dgx_ui_look_radio: RADIO, a ring with a dot while it is chosen; dgx_ui_look_checkbox: SWITCH, a square with a tick
 *   while it is on. Each the mark in colour 1, 2 while disabled, its inside what lies under the widget and colour 2
 *   while a finger is on it, the text of colour 1, 2 while disabled. A program that names neither carries neither;
 * - dgx_ui_look_list: LIST, its rows the lines of its text box ("Apple\nBanana\nCherry"): row r the line r, its
 *   value min + r; the rows shown share the height of the box, each line in the middle of its row, the row picked on
 *   a bar of colour 3, its text of colour 0; while the list has the focus the bar is of colour 1, so a list in a frame
 *   (its fill colour 2) shows its pick either way. More rows than the box takes at a line of the text (or the list's
 *   step): a window of them from its text box's top, the pick kept in it, and a bar on the right edge of colour 3,
 *   its length the window's share of the rows and its place the window's;
 * - dgx_ui_look_dropdown: DROPDOWN, its options the lines of its text box as a list's, the one chosen (value - min)
 *   on a flat face with an arrow pointing down;
 * - dgx_ui_look_picture: PICTURE and ICON of a dgx_image_t of 1 bit (of 4 and 8 bits, dgx_ui_look_indexed below),
 *   in the middle of the box, the rest of it paper; an ICON of a value outside min..max shows none;
 * - dgx_ui_look_rgb565: PICTURE and ICON of RGB565, by a virtual screen over its pixels kept by the paint
 *   (dgx_ui_paint_free() gives it back): it needs CONFIG_DGX_ENABLE_VSCREEN and a 16-bit screen; on another the box
 *   is paper;
 * - dgx_ui_look_indexed: PICTURE and ICON of a dgx_image_t of 4 or 8 bits (one of 1 bit as dgx_ui_look_picture): its
 *   values through the picture's map are indices of the painter's table, its 0 the paper; drawn into an 8-bit virtual
 *   screen over the paint's buffer, a strip of buffer_size / width rows at a time, and sent through a table of the
 *   painter's colours. It needs CONFIG_DGX_ENABLE_VSCREEN, a buffer of a row at least and a 16-bit screen; without
 *   them the box is paper, told once in the log: on a display of 18 or 24 bits such a picture stays paper, as DGX
 *   sends a screen through a table to one of 16 bits only. Only the rows the clip shows are built. The paint keeps
 *   that screen and the table of 512 bytes (dgx_ui_paint_free() gives them back).
 *
 * The focus of a button, a dropdown, a radio and a checkbox a frame of a point in colour 1 round its box (a button's
 * round its face), with a point of paper or face between it and what it frames; a switch's and a slider's its knob
 * hollow, a border of colour 1; a list's its bar in colour 1; a field's its caret in colour 1: the focus never in colour
 * 3, which says a value (on, chosen, filled, picked). A finger on a button or a dropdown: its face in colour 3; on a
 * slider's or a switch's knob: the knob a point wider; on a radio or a checkbox: its mark's inside in colour 2.
 */
extern const dgx_ui_look_t dgx_ui_look_button, dgx_ui_look_input, dgx_ui_look_switch, dgx_ui_look_slider, dgx_ui_look_list,
    dgx_ui_look_dropdown, dgx_ui_look_picture, dgx_ui_look_rgb565, dgx_ui_look_indexed, dgx_ui_look_radio, dgx_ui_look_checkbox;

/**
 * A look of a text of several lines, named by a widget that wants it (.look = &dgx_ui_look_lines): the text laid
 * word by word into the lines its box has, an important text scrolled a line every scroll_ms, one that is not cut
 * with an ellipsis, letters appearing one by one when asked (dgx_ui_text_appear()). It asks for time while it
 * scrolls or its letters come. A program that names it not does not carry it.
 */
extern const dgx_ui_look_t dgx_ui_look_lines;

/**
 * A look of a text of several lines in handwriting (.look = &dgx_ui_look_hand_lines), for a style whose variants name
 * the library's handwriting (dgx_ui_font_variant_t::hand): the text laid word by word at a variant's height, not
 * narrowed, the ladder of the style fitting it; a bitmap variant of the ladder drawn as dgx_ui_look_lines draws it, on
 * the first variant's base line. Each row written by the hand, straight, or into the widget's canvas by
 * &dgx_ui_handwriting_canvas. Letters coming (dgx_ui_text_appear()) are written line after line stroke by stroke, a
 * frame every DGX_UI_HAND_FRAME_MS; an important text too long scrolls a line every scroll_ms once all came; one that
 * is not ends its last row in "..." in the same hand. A program that names it not does not carry it.
 */
extern const dgx_ui_look_t dgx_ui_look_hand_lines;

/**
 * @brief The draw functions of the looks above, for a look of the program's own colours as a constant:
 * `static const dgx_ui_look_t mine = {.draw = dgx_ui_draw_button, .params = &my_colours, .fill = -1};`. Each takes the
 * parameters of its look's type; one that holds children (a box, a frame) takes dgx_ui_look_t::fill as its fill.
 */
bool dgx_ui_draw_box(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_frame(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_text(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_lines(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_hand_lines(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_button(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_input(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_switch(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_slider(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_list(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_dropdown(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_picture(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_rgb565(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_indexed(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_radio(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_checkbox(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_level(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_ring(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);

/**
 * @brief The tree drawn on the screen, and every popup open over it one over another: laid out first if they have to
 * be, then what changed, each tree around the popups over it, in one batch.
 *
 * Called by the task that owns the screen. The screen's clip is set for each widget and given back as it was. Where
 * a popup closed, the trees still open are composed again (dgx_ui_paint_area()).
 * Nothing is drawn under the keyboard's band, `cover`; where a band was and is no more, the tree is composed again
 * whole (dgx_ui_paint_area()), so that a keyboard closed leaves no hole.
 *
 * @return true when a widget asks for more time: the loop calls again soon.
 */
bool dgx_ui_paint(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms);

/**
 * @brief What the painter made for itself given back: the virtual screens of the looks of pictures and of the strips,
 * the table of the indexed pictures, the canvases of the widgets. The paint may be used again after.
 */
void dgx_ui_paint_free(dgx_ui_paint_t *paint);

/**
 * @brief The painter drawing through the buffer in strips, for paint's strips: on a display behind a bus of 18 or 24
 * bits every widget without children, and on one of 16 bits every widget without children on a drawn background, is
 * drawn whole in a strip of the buffer and sent once, so its text does not blink (above). Needs
 * CONFIG_DGX_ENABLE_VSCREEN: named without it, the program does not link.
 */
extern const dgx_ui_strips_t dgx_ui_strips;

/**
 * @brief The canvas of widget i, a widget of DGX_UI_CANVAS: an 8-bit virtual screen as large as its box, kept by the
 * paint from pass to pass, for a look that draws its picture on and on: digits that morph, a text written stroke by
 * stroke. The look of such a clock:
 *
 *     bool          made;
 *     dgx_screen_t *c = dgx_ui_canvas(paint, ui, i, &made);
 *     if (c && made) draw_all(c);         // new, or of another size: nothing in it yet
 *     else if (c) draw_what_moved(c);     // as the last pass left it: only what changed since
 *     dgx_ui_canvas_put(paint, ui, i, 1); // on the screen, amounts of the ink 1 over the paper
 *
 * The look draws in it what the time and the state say, so that drawn twice in a pass it is the same: the painter
 * calls a look for each part of its box and for each strip, and each call puts the canvas whole. Drawn whole (a popup
 * closed over it), a look of a kept canvas puts it again, not draws it again.
 *
 * Made at its first need, after the layout, the size of the box; all zeros. Its box of another size: a new one is
 * made, then the old one freed; none to be had, the old one is kept, the widget marked DGX_UI_MARK_HARD_SIZE, and its
 * canvas never changes again. None at all: NULL, an error in the log, the widget told DGX_UI_NO_MEMORY at the end of
 * the next dgx_ui_paint(), once for that size, and tried again at its next need.
 *
 * It is the paint's: the same tree drawn by another paint has canvases of its own, empty at first. It is freed when
 * its widget leaves the tree, when its tree is not drawn by the paint's dgx_ui_paint() (a popup closed: made again
 * when it opens), and by dgx_ui_paint_free(). So one paint draws one tree and the popups over it: two trees drawn in
 * turn by one paint at every pass (a bar and a page, each its root given) free each other's canvases at every pass,
 * right on the screen but made again each time; such a program gives each tree a paint of its own. It needs the
 * virtual screens of DGX (CONFIG_DGX_ENABLE_VSCREEN).
 *
 * @param made Set: true when the canvas is new and empty.
 * @return The canvas, or NULL: no canvas to be had, or widget i is of no DGX_UI_CANVAS or of no size.
 */
dgx_screen_t *dgx_ui_canvas(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, bool *made);

/**
 * @brief The canvas of widget i put on the paint's screen at the top left of its box, under the painter's clip, only
 * its rows in the clip; the rest of the box its paper, laid (a drawn background too).
 *
 * @param ink -1: the canvas holds indices of the painter's table, 0 the paper: on a drawn background 0 leaves the
 *            picture. An index of the table: the canvas holds amounts of that ink over the paper, 0 none and 255 all,
 *            as a soft pen writes them; on a drawn background they mix with the picture where the screen can be read
 *            (a strip), else with the paper's colour, and 0 leaves the picture.
 * On a screen of other than 16 bits (DGX sends an 8-bit screen to 16 bits only), and with no canvas, the box is paper,
 * told once in the log.
 */
void dgx_ui_canvas_put(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, int ink);

/**
 * @brief A rectangle of the tree's screen composed from the root down: the background, then every widget that meets
 * it, each drawn whole under a clip of its part of the rectangle; then the popups open over it the same way, each
 * tree around the popups over it.
 *
 * Nothing is marked as drawn: the widgets drawn whole only in part are drawn as usual by the next dgx_ui_paint().
 * The rectangle is in pixels of the tree's screen; paint's scr may be a buffer, its (0, 0) at ox, oy of it, so a
 * rectangle may be composed in strips. Pixels of the rectangle may be written more than once: compose into a
 * buffer for a display behind a bus where that would show.
 */
void dgx_ui_paint_area(dgx_ui_t *ui, dgx_ui_paint_t *paint, int x, int y, int w, int h);

/**
 * @brief A rectangle of the tree's screen composed onto another screen, its corner at that screen's (0, 0): a strip of
 * a mirror of the display, a picture of it. The paint's screen, colours and ox, oy are those for the call and given
 * back after; its canvases, buffer and strips are its own, so a look's canvas is put as it is and not made again.
 *
 *     static uint16_t rows[240 * 8];                       // eight rows of RGB565
 *     dgx_screen_t   *band = dgx_vscreen_from_array(rows, 240, 8, 16, DgxScreenRGB);
 *     for (int y = 0; y < 320; y += 8) {
 *         dgx_ui_paint_area_to(&ui, &paint, band, NULL, 0, y, 240, 8);
 *         send(rows, sizeof rows);                         // the program's: to a computer, a file
 *     }
 *
 * @param colors The table packed for scr, as many as paint's color_count; NULL: the paint's, of a screen of the same
 *               depth.
 */
static inline void dgx_ui_paint_area_to(dgx_ui_t *ui, dgx_ui_paint_t *paint, dgx_screen_t *scr, const uint32_t *colors, int x, int y, int w, int h)
{
    dgx_screen_t *const   was_scr = paint->scr;
    const uint32_t *const was_colors = paint->colors;
    const int16_t         was_ox = paint->ox, was_oy = paint->oy;
    paint->scr = scr, paint->ox = (int16_t)x, paint->oy = (int16_t)y;
    if (colors) paint->colors = colors;
    dgx_ui_paint_area(ui, paint, x, y, w, h);
    paint->scr = was_scr, paint->colors = was_colors, paint->ox = was_ox, paint->oy = was_oy;
}

/** @brief A function of the program told a rectangle of the tree's screen, in its pixels: dgx_ui_changed(). */
typedef void (*dgx_ui_changed_fn)(void *ctx, dgx_ui_rect_t rect);

/**
 * @brief What changed, told and not drawn: the tree a layer in another compositor's strips.
 *
 * The pass dgx_ui_paint() would make, over the same marks, with nothing written. Every part of a widget it would draw
 * is told to `tell`, and the look is called all the same under an empty clip, so that it keeps what it keeps: its time
 * and its moment, its canvas, its fitted variant. A widget drawn whole is told once and its children not; where the
 * keyboard's band was and where popups closed is told too. The parts under `cover` are not told, as they are not
 * drawn: the new place of a band is the compositor's own, which draws the band. Then the compositor composes each
 * rectangle in its strips:
 *
 *     static void changed(void *ctx, dgx_ui_rect_t rect) { strips_mark(rect); }  // the compositor's list
 *     ...
 *     busy = dgx_ui_changed(&ui, &paint, now, changed, NULL);
 *     for (int y = 0; y < 240; y += 16)                          // each strip marked: its own picture, the tree over it
 *         if (strips_marked(y)) {
 *             game_draw(strip, y);
 *             dgx_ui_paint_area_to(&ui, &paint, strip, NULL, 0, y, 320, 16);
 *             send(strip, y);
 *         }
 *
 * The function may note the rectangle, as above, or compose it at once, dgx_ui_paint_area_to() into a frame buffer:
 * a composing made in it is drawn on its own screen and not told. `tell` NULL: the tree marked drawn, nobody told, for
 * a compositor that has just composed all of it.
 *
 * A tree is led by dgx_ui_paint() or by dgx_ui_changed(), not by both: after this the marks are gone and nothing is
 * on the paint's screen. That screen is the display's own: the tree is laid for its size and turn and nothing is
 * written on it, so no screen of the display's size is needed. Call it when the loop would paint: after input and
 * messages, and when dgx_ui_wake() says a look's moment came; a look that asks for time is told at every call, its
 * moment come or not. A frame the compositor composes for a reason of its own need not call it first:
 * dgx_ui_paint_area_to() draws the tree as it stands, and what changed meanwhile is told at the next call. In a file
 * of its own: a program that does not call it carries none of it.
 *
 * @return As dgx_ui_paint(): a look asks for time; paint's wake_ms filled the same.
 */
bool dgx_ui_changed(dgx_ui_t *ui, dgx_ui_paint_t *paint, uint32_t now_ms, dgx_ui_changed_fn tell, void *ctx);

/**
 * @brief The program's picture under the tree changed in a rectangle of the tree's screen (dgx_ui_background_set()): laid there now where it
 * shows, and the widgets on it drawn again over it.
 *
 * The picture is laid where the root does not lie and wherever the paper is what lies under a widget; a widget
 * without children that meets the rectangle is drawn whole under a clip of its part of it, through the strips when
 * they are named. Nothing is drawn within a widget whose look fills (a frame and all in it), under a popup over the
 * tree, nor under the keyboard's band; a popup's own root that does not fill shows the picture too, as any root. A
 * game calls it for each piece of its frame that changed, where it sent the piece to the display before; a program of
 * a weather map, when the new map has come. Called by the task that owns the screen; within a batch of DGX the pieces
 * go out with the batch. Without a drawn background it does nothing.
 */
void dgx_ui_background_changed(dgx_ui_t *ui, dgx_ui_paint_t *paint, int x, int y, int w, int h);

/** @brief The library's laying of a drawn background: named by dgx_ui_background_set(), not by the program. */
extern const dgx_ui_drawn_background_t dgx_ui_drawn_background;

/**
 * @brief The program's picture under the tree (above): its function and what it is given, or NULL and NULL for the
 * colour `background` again.
 *
 * It names the library's laying of a drawn background, so that a program that never calls it does not carry it: the
 * function written into background_draw by hand is not called. Set before a pass; a background set or taken away
 * while the tree is on the screen shows where it is drawn next. To see it at once: set, the program calls
 * dgx_ui_background_changed() for all of the screen; taken away, which that call no longer draws, dgx_ui_redraw(ui,
 * 0).
 */
static inline void dgx_ui_background_set(dgx_ui_paint_t *paint, dgx_ui_background_draw_t draw, void *ctx)
{
    paint->background_draw = draw, paint->background_ctx = ctx, paint->drawn = draw ? &dgx_ui_drawn_background : NULL;
}

#ifdef __cplusplus
}
#endif
