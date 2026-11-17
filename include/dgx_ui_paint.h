#pragma once
/*
 * dgx_ui_paint.h
 *
 *  The painter of dgx-ui: a tree drawn on a screen of DGX, only what changed, in one batch. It sets the clip of the
 *  screen to each widget's box while the widget's look draws, and gives the screen back the clip it had. Colors
 *  are indices of the program's table, packed for the screen (dgx_screen_rgb()): a table changed is the whole
 *  interface in other colors.
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

/** @brief How a tree is drawn on a screen. Filled by the program; the members after `cover` are the painter's. */
typedef struct dgx_ui_paint_t {
    dgx_screen_t   *scr;         ///< where
    const uint32_t *colors;      ///< the table: an index of a look to a color of the screen
    uint16_t        color_count; ///< its size
    uint8_t         background;  ///< the index of the paper under the tree
    dgx_ui_rect_t   cover;       ///< the band of DGX-DI's keyboard while it is open: not drawn there; else empty
    int16_t         ox, oy;      ///< the pixel of the tree's screen at (0, 0) of scr: 0, or a buffer's place when composing
    /* the painter's */
    uint8_t         paper;       ///< while a look draws: the index of the color under the widget, its parent's fill or the background
    bool            whole;       ///< while a look draws: nothing of the widget is on the screen as it should be
    bool            composing;   ///< while a look draws: dgx_ui_paint_area() composes, nothing is to be remembered
    uint8_t         part, parts; ///< while a look draws: the part of the box it is called for, of so many
    const dgx_ui_t *popup_over;  ///< while a tree is drawn: the popup over it, whose rectangle and those of the popups
                                 ///< over it are covers after the keyboard's band; NULL none
    dgx_screen_t   *rgb565;      ///< the look of RGB565: a virtual screen pointed at each picture, made at its first need
    dgx_ui_rect_t   cover_was;   ///< the keyboard's band as the last pass left it: where it goes, the tree is composed again
} dgx_ui_paint_t;

/**
 * @brief How a kind of widget looks: a function that draws a widget in its state, and its parameters.
 *
 * The function draws what differs between the widget's state_drawn and state_value, or all of it when the painter
 * says whole; the clip of the screen is set to the part of the widget's box it may draw, and it may be called for
 * several parts in turn. It should write every pixel once. It draws the widget's box less paint's ox and oy: the
 * screen may be a buffer that holds a part of the tree's screen.
 *
 * A look that remembers what it drew (a variant fitted, a line scrolled to) works it out the same in every part and
 * stores it only in the last part of the painter's own pass: paint->part == paint->parts - 1 and not composing.
 * Stored in the first, it would find nothing changed in the parts after.
 */
struct dgx_ui_look_t {
    bool (*draw)(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms); ///< true: more time
    const void *params; ///< the look's own type
    int16_t     fill;   ///< the index of the color it leaves under its children; -1: what lies under it
};

/** @brief A color of the table, packed for the screen; 0 past its end. */
static inline uint32_t dgx_ui_color(const dgx_ui_paint_t *paint, int index)
{
    return paint->colors && index >= 0 && index < paint->color_count ? paint->colors[index] : 0;
}

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

/** Parameters of a button: a face over a shadow; pressed, the face moves onto the shadow. Indices of the table. */
typedef struct dgx_ui_button_look_t {
    uint8_t face, pressed, focused, disabled; ///< the face in each state
    uint8_t text, text_disabled;              ///< the text, the variant 0 of its style, in the middle of the face
    uint8_t shadow;                           ///< under the face, right and below
    uint8_t depth;                            ///< of the shadow, in points of the style; 0: a flat button
} dgx_ui_button_look_t;

/** Parameters of a switch: a track with a knob at its left end (0) or its right end (1). */
typedef struct dgx_ui_switch_look_t {
    uint8_t off, on, knob, focused; ///< the track off and on, the knob, the knob when focused
    uint8_t disabled;               ///< the knob when disabled
} dgx_ui_switch_look_t;

/** Parameters of a slider: a track in the middle of its height, the part up to the knob filled, a knob of 3 points. */
typedef struct dgx_ui_slider_look_t {
    uint8_t track, fill, knob, focused; ///< the track, its filled part, the knob, the knob when focused
    uint8_t disabled;                   ///< the knob when disabled
    uint8_t track_h;                    ///< the height of the track, in points of the style
} dgx_ui_slider_look_t;

/** Parameters of a picture of 1 bit: its ink and its paper, both written, every pixel of the box once. */
typedef struct dgx_ui_picture_look_t {
    uint8_t ink;   ///< index of the table
    uint8_t paper; ///< index of the table; DGX_UI_PAPER: what lies under the widget
} dgx_ui_picture_look_t;

/** Parameters of a field typed into: its text and, focused, a caret where the next letter goes. */
typedef struct dgx_ui_input_look_t {
    uint8_t ink, caret; ///< indices of the table
    uint8_t paper;      ///< index of the table; DGX_UI_PAPER: what lies under the widget
} dgx_ui_input_look_t;

/**
 * The looks of the controls and pictures, of the colours 0 the paper, 1 the ink, 2 a fill, 3 a border:
 * - dgx_ui_look_button: BUTTON and DROPDOWN; its text from the widget's text box;
 * - dgx_ui_look_input: INPUT, its text in one line and, focused, a caret after it;
 * - dgx_ui_look_switch, dgx_ui_look_slider;
 * - dgx_ui_look_picture: PICTURE and ICON of a dgx_image_t of 1 bit (4 and 8 bits: through the buffer, not yet),
 *   in the middle of the box, the rest of it paper; an ICON of a value outside min..max shows none;
 * - dgx_ui_look_rgb565: PICTURE and ICON of RGB565, by a virtual screen over its pixels kept by the paint
 *   (dgx_ui_paint_free() gives it back): it needs CONFIG_DGX_ENABLE_VSCREEN and a 16-bit screen; on another the box
 *   is paper.
 */
extern const dgx_ui_look_t dgx_ui_look_button, dgx_ui_look_input, dgx_ui_look_switch, dgx_ui_look_slider, dgx_ui_look_picture, dgx_ui_look_rgb565;

/**
 * A look of a text of several lines, named by a widget that wants it (.look = &dgx_ui_look_lines): the text laid
 * word by word into the lines its box has, an important text scrolled a line every scroll_ms, one that is not cut
 * with an ellipsis, letters appearing one by one when asked (dgx_ui_text_appear()). It asks for time while it
 * scrolls or its letters come. A program that names it not does not carry it.
 */
extern const dgx_ui_look_t dgx_ui_look_lines;

/**
 * @brief The draw functions of the looks above, for a look of the program's own colours as a constant:
 * `static const dgx_ui_look_t mine = {.draw = dgx_ui_draw_button, .params = &my_colours, .fill = -1};`. Each takes the
 * parameters of its look's type; one that holds children (a box, a frame) takes dgx_ui_look_t::fill as its fill.
 */
bool dgx_ui_draw_box(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_frame(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_text(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_lines(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_button(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_input(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_switch(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_slider(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_picture(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
bool dgx_ui_draw_rgb565(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);

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

/** @brief What the painter made for itself given back: the screen of the look of RGB565. The paint may be used again after. */
void dgx_ui_paint_free(dgx_ui_paint_t *paint);

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

#ifdef __cplusplus
}
#endif
