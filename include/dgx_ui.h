#pragma once
/*
 * dgx_ui.h
 *
 *  dgx-ui: a light layer of widgets on DGX. Widgets stand in rows and columns, their values come as messages from
 *  other tasks, and they animate only while a widget asks for time.
 *
 *  The core: the tree of widgets, its layout, the state of each widget, the pointer, the keys and the focus. It
 *  draws nothing and calls no function of DGX, so it runs in host tests without a display; drawing is
 *  dgx_ui_paint.h. One task owns a tree, as one task owns a screen of DGX: no call here may come from another. The
 *  library keeps nothing of a tree outside the tree and its paint, so two tasks, each with its own trees and paints,
 *  do not meet.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dgx_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Rectangles.
 */

/**
 * @brief A rectangle of a screen in pixels: the top left corner and the size.
 *
 * A rectangle of no width or no height holds no pixel; such a rectangle is empty wherever it lies.
 */
typedef struct dgx_ui_rect_t {
    int16_t x, y;
    int16_t w, h;
} dgx_ui_rect_t;

/** @brief A rectangle made of its corner and size. */
static inline dgx_ui_rect_t dgx_ui_rect(int x, int y, int w, int h)
{
    dgx_ui_rect_t r = {(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h};
    return r;
}

/** @brief Whether a rectangle holds no pixel. */
static inline bool dgx_ui_rect_empty(const dgx_ui_rect_t *r)
{
    return r->w <= 0 || r->h <= 0;
}

/**
 * @brief The pixels two rectangles have in common.
 *
 * @param a,b The rectangles.
 * @param out Where the common part goes; all zeros when there is none. May be a or b.
 * @return false when they have no pixel in common.
 */
bool dgx_ui_rect_intersect(const dgx_ui_rect_t *a, const dgx_ui_rect_t *b, dgx_ui_rect_t *out);

/**
 * @brief The pixels of a box that lie outside a cut: at most four rectangles that do not overlap.
 *
 * The band above the cut and the band below it are as wide as the box; the bands left and right of the cut lie
 * between them. A widget partly under an open popup is drawn in these parts, the clip set to each in turn.
 *
 * @param box The box; its right and bottom, x + w and y + h, do not pass 32767 (no screen is that large).
 * @param cut What is taken out of it.
 * @param out Room for four rectangles; the parts go there, the band above first.
 * @return How many parts: 0 when the box is empty or the cut covers it, 1 with the box itself when they do
 *         not meet or the cut is empty, else 1 to 4.
 */
int dgx_ui_rect_subtract(const dgx_ui_rect_t *box, const dgx_ui_rect_t *cut, dgx_ui_rect_t out[4]);

/*
 * The state of a widget.
 */

/** Bits of a widget's state, dgx_ui_state_t::bits. */
enum {
    DGX_UI_HIDDEN   = 1u << 0,     ///< not shown, takes no place in its row or column, not hit, not focused
    DGX_UI_DISABLED = 1u << 1,     ///< shown as its look draws it so, not hit, not focused
    DGX_UI_FOCUSED  = 1u << 2,     ///< the keys go to it
    DGX_UI_PRESSED  = 1u << 3,     ///< pressed: by a finger on it, or by the program
};

/**
 * @brief The bits of a widget's state: as it starts (a widget's .state) and as it is (a node's state).
 *
 * A widget's value is not here. It lives in the program's memory, where the widget's `value` points; a node keeps
 * only the value it last drew (state_drawn), and a look draws what differs between that and the memory.
 */
typedef struct dgx_ui_bits_t {
    uint8_t bits; ///< DGX_UI_HIDDEN, DGX_UI_DISABLED, DGX_UI_FOCUSED, DGX_UI_PRESSED
} dgx_ui_bits_t;

/** @brief The state of a widget as it was last drawn: its bits and its value. */
typedef struct dgx_ui_state_t {
    uint8_t bits;  ///< DGX_UI_HIDDEN, DGX_UI_DISABLED, DGX_UI_FOCUSED, DGX_UI_PRESSED
    int32_t value; ///< the value of a control as it was last drawn
} dgx_ui_state_t;

/*
 * The widget.
 */

/** Kinds of widgets, dgx_ui_widget_t::kind. */
enum {
    DGX_UI_BOX,      ///< holds children: COLUMN or ROW stacks them, else they sit where they ask
    DGX_UI_FRAME,    ///< a box with a fill and a border
    DGX_UI_LABEL,     ///< a text box
    DGX_UI_PICTURE,  ///< one picture
    DGX_UI_ICON,     ///< a picture of an array, its value picks it; outside min..max, none
    DGX_UI_LIST,     ///< rows, one picked: the value, min the first row; more rows than its box takes, a window of them that scrolls
    DGX_UI_SWITCH,   ///< value 0 or 1
    DGX_UI_SLIDER,   ///< value min..max by step
    DGX_UI_BUTTON,   ///< pressed and let go within DGX_UI_CLICK_MS, not swiped: activated, wherever it is let go
    DGX_UI_INPUT,    ///< a field typed into
    DGX_UI_DROPDOWN, ///< a button showing the value picked; its list opens as a popup
    DGX_UI_OWN,      ///< drawn by a look of the program, or of the library's (the level bar, the ring scale), its data in own
    /** one of a group: the buttons of a group name one place (.value), each its own number in step, and the one whose
        number the place holds is chosen; one of no place never is. Its value is the group's: dgx_ui_value() of any
        button is the number chosen, the same for every button of the group, and dgx_ui_set_value() on any writes the
        group's place; a button is chosen when the value is its step (dgx_ui_radio_chosen()). Not bound by min and max.
        A click, or DGX_UI_KEY_ACTIVATE while it has the focus, writes its own number: on() told DGX_UI_CHANGED when
        the place changed, the button that was chosen told nothing */
    DGX_UI_RADIO,
};

/** How a widget is built, dgx_ui_widget_t::flags: set by the program. */
enum {
    DGX_UI_COLUMN      = 1u << 0,  ///< a box: children one under another
    DGX_UI_ROW         = 1u << 1,  ///< a box: children side by side
    DGX_UI_STRETCH     = 1u << 2,  ///< across the stacking as wide (in a column) or as tall (in a row) as the box
    DGX_UI_SWIPE       = 1u << 3,  ///< a slider moved by the way the finger goes from where it went down
    DGX_UI_DRAG        = 1u << 4,  ///< a button dragged up or down as well as pressed (a scroll bar's knob)
    DGX_UI_CANVAS      = 1u << 5,  ///< the widget has a canvas of its own, made after the layout
    DGX_UI_REST        = 1u << 6,  ///< along the stacking of a column or a row, all the box has left after the others, the
                                   ///< gaps and the pads; several share it, the last taking the odd pixels; never below 0
    DGX_UI_REPEAT      = 1u << 7,  ///< a button told DGX_UI_ACTIVATED again and again while it is held (below): a
                                   ///< stepper's "+"
    DGX_UI_PERCENT_X   = 1u << 8,  ///< at_x in percent (DGX_UI_PERCENT()), not in points
    DGX_UI_PERCENT_Y   = 1u << 9,  ///< at_y in percent
    DGX_UI_PERCENT_W   = 1u << 10, ///< want_w in percent
    DGX_UI_PERCENT_H   = 1u << 11, ///< want_h in percent
    DGX_UI_PERCENT_PAD = 1u << 12, ///< a box's pad in percent
    DGX_UI_PERCENT_GAP = 1u << 13, ///< a box's gap in percent
};

/*
 * Sizes, places, pads and gaps in percent: each of want_w, want_h, at_x, at_y, pad and gap is in percent when its flag
 * DGX_UI_PERCENT_* is set, else in points. A percent is of pixels, the unit does not touch it:
 *  - a box's pad: of the box's own width, on all four sides; the inside is the box less it;
 *  - a box's gap: of the inside along its stacking, the width in a row, the height in a column;
 *  - along a row's or a column's stacking, a child's size: of the room the children share, the inside less every gap
 *    between the children shown, never below 0; the percents of the children taken by running totals in their order,
 *    so that they add up with no pixel lost or twice: two of 50 % with a gap fill a row; a total past 255.99 % holds
 *    there. A child of points takes its points, one of DGX_UI_REST what is left after the points and the percents;
 *    neither takes from the percents. A hidden child is out of it all: no gap, no part of the room, no step of the
 *    totals;
 *  - across the stacking, and both ways in a box of neither, a size and a place: of the inside; a negative place from
 *    the far edge, as in points;
 *  - 0 with its flag is 0 %, no size, where 0 in points is the content's size.
 * The root's children are of the root, laid on the screen or on the rect given.
 */

/**
 * @brief A percent as a widget holds it, 8.8 fixed point in an int16_t: DGX_UI_PERCENT(50) is 12800, DGX_UI_PERCENT(33.33)
 * 8532, a step 1/256 %. A constant expression. Past what an int16_t holds it holds the edge: 127.998 and above give
 * 32767 (127.996 %), -128 and below -32768 (-128 %).
 */
#define DGX_UI_PERCENT(p)                                                                                                   \
    ((p) * 256 >= 32767.5 ? (int16_t)32767 : (p) * 256 <= -32768.5 ? (int16_t)-32768 : (int16_t)((p) * 256 + ((p) < 0 ? -0.5 : 0.5)))

/*
 * DGX_UI_REPEAT: a button told DGX_UI_ACTIVATED when the finger goes down on it, and again every DGX_UI_REPEAT_MS from
 * DGX_UI_REPEAT_FIRST_MS on while the finger holds it, wherever the finger goes, as it stays pressed and looks so till
 * the finger goes up; nothing told when it goes up, so a click is one ACTIVATED. The key ACTIVATE held on it while it
 * has the focus (dgx_ui_buttons()) repeats the same way. The repeats come from dgx_ui_time(): a loop with such a button
 * calls it at every turn and waits by dgx_ui_wake(); without it the button is told once, at the press, and no more. It
 * ends when the finger goes up, or the press is spent: a swipe told on it, the button hidden, disabled or removed, a
 * popup opened over its tree. One repeats at a time, the last begun: a key held while the finger repeats takes over,
 * and the other, still held, does not begin again when that one ends. DGX_UI_PRESSED set by the program
 * (dgx_ui_set_state()) shows it pressed and repeats nothing; a button also of DGX_UI_DRAG is the drag's and does not
 * repeat. The flags are all taken with this one.
 */

/** Marks of a widget, dgx_ui_widget_t::marks: set by the library only. */
enum {
    DGX_UI_MARK_CHANGED   = 1u << 0, ///< its content changed (a text, a picture)
    DGX_UI_MARK_WHOLE     = 1u << 1, ///< to be drawn whole: nothing of it is on the screen as it should be
    DGX_UI_MARK_FIT       = 1u << 2, ///< its text to be fitted to its field again
    DGX_UI_MARK_HARD_SIZE = 1u << 3, ///< its canvas keeps its size: a larger one could not be had
    DGX_UI_MARK_TIME      = 1u << 4, ///< its look asked for time: called again at the next pass, to draw what time changed
    DGX_UI_MARK_AROUND    = 1u << 5, ///< a child of it moved, came or went: a box or a frame of a column or a row fills
                                     ///< around its children, who are drawn only when marked themselves
    DGX_UI_MARK_BELOW     = 1u << 6, ///< a widget under it is to be drawn: the painter goes down here, and skips the rest
    DGX_UI_MARK_WINDOW    = 1u << 7, ///< a list's window to be brought to its pick when it is drawn next: its value set, its height changed
};

/** What a widget's on() is told: what the user did, never what the program set; and to a tree's root, where the tree
 * is, whoever opened or closed the popups (DGX_UI_SHOWN, DGX_UI_COVERED: see the popups below). */
enum {
    DGX_UI_ACTIVATED, ///< a click of a button, a list's row, a dropdown or a widget of the program's own; a field's "done"
    DGX_UI_CHANGED,   ///< the value changed by the finger or a key
    DGX_UI_FOCUS,     ///< the focus came to it
    DGX_UI_BACK,      ///< a field's "cancel"; to a popup's root, a page's or a popup's closed by the key back, told while it is still open
    DGX_UI_NO_MEMORY, ///< its canvas could not be had, told once for that size after the pass; tried again whenever its look asks
    DGX_UI_EDIT,      ///< a field clicked, or DGX_UI_KEY_ACTIVATE while it has the focus: the program opens its editor
    DGX_UI_SHOWN,     ///< to a tree's root: the tree came to the screen, no page over it any more, or it opened as a popup with
                      ///< none over it; never to home at the start, nor for dgx_ui_show() of a widget
    DGX_UI_COVERED,   ///< to a tree's root: the tree left the screen, a page came over it, or it closed while it was shown
};

typedef struct dgx_ui_t        dgx_ui_t;
typedef struct dgx_ui_msg_t    dgx_ui_msg_t;
typedef struct dgx_ui_look_t   dgx_ui_look_t;
typedef struct dgx_ui_node_t   dgx_ui_node_t;

typedef struct dgx_ui_paint_t  dgx_ui_paint_t;

/**
 * @brief How a widget of the program's own (DGX_UI_OWN) behaves under the finger and the keys, named by its look
 * (dgx_ui_look_t::behavior): the core does what is common to every kind (the hit, DGX_UI_PRESSED, the focus, the rule
 * of a click, a swipe from an edge, hidden and disabled) and calls these for what is the widget's own. Every function
 * may be NULL; x and y are in the widget's own pixels, from the top left of its box.
 *
 *     // a dial 0..100: the angle of the finger round its middle, set as the user set it
 *     static void dial_down(dgx_ui_t *ui, int i, int x, int y) { dgx_ui_user_value(ui, i, angle_of(ui, i, x, y)); }
 *     static bool dial_move(dgx_ui_t *ui, int i, int x, int y) { dial_down(ui, i, x, y); return true; } // a drag
 *     static const dgx_ui_behavior_t dial_behavior = {.flags = DGX_UI_BEHAVIOR_FOCUS, .down = dial_down, .move = dial_move};
 *     static const dgx_ui_look_t     dial_look     = {.draw = draw_dial, .fill = -1, .behavior = &dial_behavior};
 *     static const dgx_ui_widget_t   dial_w        = {.kind = DGX_UI_OWN, .max = 100, .want_w = 40, .want_h = 40, .look = &dial_look};
 *
 * move() returning true takes the move as a drag: no click when let go, and a swipe from an edge that began on it is
 * not the program's (dgx_ui_dragging()); a widget that is only pressed returns false, or has no move(), and keeps its
 * click. Let go: up() first, with click by the rule of a click (not dragged, within DGX_UI_CLICK_MS), then on a click
 * the core tells DGX_UI_ACTIVATED, so a value up() sets is the one the program hears of. A key while it has the focus
 * goes to key() first; what it does not take (false) the core does as for any widget: NEXT and PREV move the focus on,
 * ACTIVATE is told as DGX_UI_ACTIVATED, as a click (up() is the finger's and is not called), BACK is the program's. A
 * widget of no DGX_UI_BEHAVIOR_FOCUS takes no focus: a press on it leaves the focus where it was, and dgx_ui_focus() on
 * it leaves nobody focused, as on any widget that cannot take it. The widget's on() stays the program's: the behaviour tells through it, by dgx_ui_user_value()
 * and dgx_ui_tell(). Not in it yet: the code points typed go to a DGX_UI_INPUT only, and the hit is the widget's
 * rectangle, so a round dial is hit in its corners too.
 */
typedef struct dgx_ui_behavior_t {
    uint8_t flags;                                             ///< DGX_UI_BEHAVIOR_FOCUS
    void (*down)(dgx_ui_t *ui, int i, int x, int y);           ///< the finger went down on it
    bool (*move)(dgx_ui_t *ui, int i, int x, int y);           ///< the finger moved while it holds it, anywhere; true: a drag
    void (*up)(dgx_ui_t *ui, int i, int x, int y, bool click); ///< the finger let go; click: by the rule of a click
    bool (*key)(dgx_ui_t *ui, int i, int key);                 ///< a key while it has the focus; true: taken
} dgx_ui_behavior_t;

/** Flags of a dgx_ui_behavior_t. */
enum {
    DGX_UI_BEHAVIOR_FOCUS = 1u << 0, ///< it takes the focus, and the keys with it; without it the finger alone
};

/**
 * @brief How a kind of widget looks: a function that draws a widget in its state, and its parameters; and for a widget
 * of the program's own, how it behaves (the painter's rules of a look: dgx_ui_paint.h).
 */
struct dgx_ui_look_t {
    bool (*draw)(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms); ///< true: more time
    const void              *params;   ///< the look's own type
    int16_t                  fill;     ///< the index of the color it leaves under its children; -1: what lies under it
    const dgx_ui_behavior_t *behavior; ///< a widget of DGX_UI_OWN drawn by it: how it behaves; NULL, it is not worked (every look of the library)
};

/**
 * @brief A picture of a widget: a picture of DGX of 1, 4 or 8 bits, or one of RGB565.
 */
typedef struct dgx_ui_picture_t {
    const dgx_image_t *image;  ///< the picture of 1, 4 or 8 bits; NULL for one of RGB565
    const uint16_t    *rgb565; ///< or rows of RGB565, high byte first, as dgx_vscreen_from_array() takes them
    const uint8_t     *map;    ///< 4 or 8 bits: the colors of its values, indices of the painter's table
    int16_t            width, height; ///< the size of an RGB565 picture; of a dgx_image_t its own is taken
    uint8_t            scale;  ///< 0: a pixel of it a pixel of the screen; 1: a point, unit x unit pixels; more: so many points; 255 pixels at most
} dgx_ui_picture_t;

/** @brief The size of a picture: its own for a dgx_image_t, the given one for RGB565. */
static inline void dgx_ui_picture_size(const dgx_ui_picture_t *p, int *w, int *h)
{
    *w = p->image ? p->image->width : p->width;
    *h = p->image ? p->image->height : p->height;
}

/**
 * @brief A text box: the string of a widget and how it is laid. Made by dgx_ui_text.h; the core reads its height.
 */
typedef struct dgx_ui_textbox_t dgx_ui_textbox_t;

/** @brief The height a text box asks for in a tree: its lines times the line of its first font (dgx_ui_variant_line()). */
int dgx_ui_textbox_height(const dgx_ui_t *ui, const dgx_ui_textbox_t *box);

/**
 * @brief A widget: what the program says of it. Constant: the library never writes it, so it may lie in flash.
 *
 * Filled by its members' names; zero is the default of every member. Sizes and places are in points of the style:
 * dgx_ui_init()'s unit pixels each. What changes as the program runs, its place and its state, is the tree's: a
 * dgx_ui_node_t of it.
 */
typedef struct dgx_ui_widget_t {
    uint8_t        kind;           ///< DGX_UI_BOX ... DGX_UI_RADIO
    dgx_ui_bits_t  state;          ///< the state it starts with: HIDDEN, DISABLED
    uint16_t       flags;          ///< DGX_UI_COLUMN, ROW, STRETCH, SWIPE, DRAG, CANVAS, REST, REPEAT, PERCENT_*
    int16_t        pad, gap;       ///< inside a box: around its children and between them, in points, or in percent (above)
    int16_t        at_x, at_y;     ///< asked: from the parent's inside edge, in points or percent; negative from the far edge
    int16_t        want_w, want_h; ///< asked, in points or percent (0 % no size); 0 points: the content's (a picture's size, a text's lines: of any widget with a text box; a button's, a
                                   ///< dropdown's, a radio's and a checkbox's a point more above and below, the room of the frame of the focus; a
                                   ///< BUTTON's, a SWITCH's or a RADIO's with a picture, whatever look draws it, its first picture's and 5 points more
                                   ///< across and down, a point of face and the frame's point each side and one of a shadow; a text has no width of
                                   ///< its own, nor a box any size), or across the stacking all of the box
    int16_t        min, max, step; ///< the range of a control; a list's first row and its last, and its step the least height of a row in points (0: a line of its text);
                                   ///< a RADIO's step the number it stands for
    int16_t        tag;            ///< the program's: found by dgx_ui_find(), kept and nothing more
    int32_t       *value;          ///< where its value lives, the program's memory; NULL: it has none (shows 0, nothing changes it)
    const dgx_ui_picture_t *picture; ///< PICTURE: one; ICON: an array of min..max; drawn by dgx_ui_look_picture_button, a BUTTON's one,
                                     ///< a SWITCH's or a RADIO's two: off and on, not chosen and chosen
    dgx_ui_textbox_t       *text;    ///< LABEL, BUTTON, LIST, INPUT, DROPDOWN, RADIO, a SWITCH drawn as a checkbox: the text box, in RAM
    const dgx_ui_look_t    *look;    ///< how it is drawn; NULL: the look of its kind
    bool (*take)(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg); ///< a message offered; true: claimed
    /** told what the user did, DGX_UI_ACTIVATED ...; node: the widget's node as it is now, its value set by the finger
        or a key in the widget's place, dgx_ui_value_of(node) (a slider moved, a list's row taken); valid while the widget is in the tree */
    void (*on)(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
    void *ctx;                     ///< for on()
    void *own;                     ///< OWN: its data
} dgx_ui_widget_t;

/**
 * @brief A widget in a tree: its place among the others, its place on the screen and its state. In RAM, the tree's.
 */
struct dgx_ui_node_t {
    const dgx_ui_widget_t *widget;        ///< what the program said of it; NULL: a slot that holds no widget
    int16_t                parent;        ///< -1 for the root
    int16_t                child, next;   ///< the first child and the next sibling, -1 none
    int16_t                x, y, w, h;    ///< laid: pixels of the screen
    dgx_ui_bits_t          state;   ///< its bits as they are; its value lives where the widget says
    uint8_t                marks;         ///< the library's: DGX_UI_MARK_*
    dgx_ui_state_t         state_drawn;   ///< the bits and the value as they were last drawn: the painter's; a RADIO's value
                                          ///< whether it was chosen, 1 or 0
};

#if UINTPTR_MAX == 0xFFFFFFFFu
#ifdef __cplusplus
static_assert(sizeof(dgx_ui_widget_t) == 56, "the constant part of a widget is 56 bytes on a 32-bit chip");
static_assert(sizeof(dgx_ui_picture_t) == 20, "a picture is 20 bytes on a 32-bit chip");
static_assert(sizeof(dgx_ui_node_t) == 28, "the node of a widget is 28 bytes of RAM on a 32-bit chip");
#else
_Static_assert(sizeof(dgx_ui_widget_t) == 56, "the constant part of a widget is 56 bytes on a 32-bit chip");
_Static_assert(sizeof(dgx_ui_picture_t) == 20, "a picture is 20 bytes on a 32-bit chip");
_Static_assert(sizeof(dgx_ui_node_t) == 28, "the node of a widget is 28 bytes of RAM on a 32-bit chip");
#endif
#endif

/**
 * @brief The value of a widget's node: read from the place its widget names, and kept within min and max when
 * min < max, a list's or a dropdown's when min <= max, one row too (an ICON's and a RADIO's are not bound). 0 for a
 * widget of no place. The memory itself is the program's and is not
 * corrected: it may have come from flash.
 *
 * The place is written by the program, by the user through the core, or by dgx_ui_set_value(), all in the task that
 * owns the tree: a constant with a place belongs to the trees of one task, and another task sends a message, as for
 * any value. The painter looks at every place at the start of a pass and draws the widgets whose place holds other
 * than they drew. A place written after a pass asks for no pass of its own: the task that writes it wakes its own
 * loop (the message that made it write does), and the next pass draws it. A control of no place shows 0 and nothing
 * changes it; its look tells it in the log, once a look.
 */
int32_t dgx_ui_value_of(const dgx_ui_node_t *node);

/** @brief Whether a RADIO's node is chosen: its place holds its number, step. One of no place is not. */
static inline bool dgx_ui_radio_chosen(const dgx_ui_node_t *node)
{
    const dgx_ui_widget_t *w = node->widget;
    return w && w->value && *w->value == w->step;
}

/** Widgets past the program's array come in blocks of so many. */
enum { DGX_UI_BLOCK = 64 };

/** The keys of a dialog: dgx_ui_key(). */
enum {
    DGX_UI_KEY_NEXT,     ///< the focus on (in a list: the next row first)
    DGX_UI_KEY_PREV,     ///< the focus back (in a list: the row before first)
    DGX_UI_KEY_ACTIVATE, ///< the focused one pressed
    DGX_UI_KEY_BACK,     ///< the program's: no widget of the core takes it (one of the program's own may, in its key())
    DGX_UI_KEY_MORE,     ///< the focused slider or switch up
    DGX_UI_KEY_LESS,     ///< the focused slider or switch down
    DGX_UI_KEYS
};

/** The buttons of dgx_ui_buttons(): the bits of a game pad as DGX-GE's dgx_ge_input.h and DGX-DI's pad have them. */
enum {
    DGX_UI_PAD_LEFT  = 1u << 0,
    DGX_UI_PAD_RIGHT = 1u << 1,
    DGX_UI_PAD_UP    = 1u << 2,
    DGX_UI_PAD_DOWN  = 1u << 3,
    DGX_UI_PAD_A     = 1u << 4,
    DGX_UI_PAD_START = 1u << 5,
    DGX_UI_PAD_BACK  = 1u << 6,
    DGX_UI_PAD_B     = 1u << 7,
};

/** Held keys repeat from so long after they went down, every so often: as DGX-DI's keyboard repeats erase. */
enum { DGX_UI_REPEAT_FIRST_MS = 500, DGX_UI_REPEAT_MS = 100 };

/** The knob of a slider, in points of the style: where the finger takes it and where its look draws it. */
enum { DGX_UI_KNOB = 3 };

/** Of a list's row, its top and its bottom 1/n belong to no row: a finger between two rows takes neither. */
enum { DGX_UI_ROW_GAP = 12 };

/**
 * A click: a widget pressed and let go within so long, and not swiped (DGX-DI's long press begins there). Wherever the
 * finger is let go: it went down on the widget, the widget was meant.
 */
enum { DGX_UI_CLICK_MS = 500 };

/**
 * @brief A pointer as it is now: where, in pixels of the screen; down now, went down, went up since the last look;
 * when, in milliseconds of the program's clock (that of dgx_ui_time()).
 */
typedef struct dgx_ui_pointer_t {
    int16_t  x, y;
    bool     down, pressed, released;
    bool     swipe; ///< a swipe told now (DGX-DI's, from an edge too): the widget held stays pressed, no click when let go
    uint32_t ms;
} dgx_ui_pointer_t;

/**
 * @brief A tree of widgets, and what the pointer and the keys are doing to it.
 *
 * Fill it by dgx_ui_init(); the members are read by the painter and the adapters, and changed by the calls only.
 */
struct dgx_ui_t {
    dgx_ui_node_t    *first;       ///< the program's array of nodes
    int16_t           first_size;  ///< how many it holds
    int16_t           count;       ///< slots in use: the highest index + 1
    dgx_ui_node_t   **blocks;      ///< the blocks of DGX_UI_BLOCK nodes past the array, allocated by dgx_ui_add()
    int16_t           block_count;
    uint8_t           unit;        ///< pixels a point of the style
    bool              needs_layout;
    bool              root_given;  ///< the program gave the root's rectangle; else the painter gives the screen's
    dgx_ui_rect_t     root_rect;
    int16_t           screen_w, screen_h; ///< the screen the tree was laid for
    uint8_t           screen_turn;        ///< its orientation as the painter tells it
    bool              turned;             ///< the screen changed since the last layout: all to be drawn whole
    int16_t           focus;       ///< the focused widget, -1 none
    int16_t           pressed;     ///< the widget the pointer went down on, -1 none
    int16_t           press_x, press_y, move_x, move_y;
    int32_t           press_value;
    uint32_t          press_ms;    ///< when it went down
    bool              dragged;     ///< moved since it went down: no click when let go
    bool              swiped;      ///< swiped since it went down: no click when let go
    bool              between;     ///< a list pressed between its rows: no row taken
    uint32_t          held;        ///< the buttons held, DGX_UI_PAD_*
    int8_t            repeat_key;  ///< the key repeating while its button is held, -1 none
    uint32_t          repeat_at;   ///< when it repeats next
    const uint8_t    *button_keys; ///< 8 entries: the key of each button, 0xff none; NULL the default
    dgx_ui_msg_t     *offered;     ///< the message dgx_ui_take() offers now, for dgx_ui_msg_keep(); NULL between
    bool              told_width;  ///< the warning of a text laid with no width given once
    bool              told_look;   ///< the warning of a widget shown with no look given once
    bool              leaving;     ///< a popup whose root is told DGX_UI_BACK now: a back for it from that on() closes it, untold again
    bool              setting_up;  ///< a transition is being set up over it (dgx_ui_page_open_by(), _back_by()): one asked meanwhile, from
                                   ///< an on() told within it, changes the pages at once, with no pictures of its own
    dgx_ui_t         *popup;       ///< the popup open over the tree, a tree of its own, maybe with its own; NULL none
    uint8_t           level;       ///< how many popups lie under it: 0 for a tree that is no popup
    uint8_t           popup_flags; ///< DGX_UI_POPUP_*
    dgx_ui_rect_t     popup_was;   ///< where a popup closed since the last pass of the painter: composed again there
    dgx_ui_t         *under;       ///< the library's: the tree this one lies over as a popup; NULL none
    bool              shown_told;  ///< the library's, not to be read: a program keeps a flag of its own from DGX_UI_SHOWN and _COVERED
    uint8_t           veiled_bits; ///< the library's: DGX_UI_FOCUSED while a modal popup lies over the tree, the bits its looks do
                                   ///< not show; read by dgx_ui_bits_shown()
    int16_t           page_place;  ///< the library's: home's widget its pages lie over, + 1 (dgx_ui_page_place()); 0 the screen
};
/**
 * @brief The pixels of the screen a pixel of a picture takes, across and down, in a tree: 1 for scale 0, else scale
 * times the tree's unit, at most 255. A picture of a widget is so many times its size (dgx_ui_picture_size()).
 */
static inline int dgx_ui_picture_scale(const dgx_ui_t *ui, const dgx_ui_picture_t *picture)
{
    const int by = picture->scale * (ui ? ui->unit : 1);
    return !picture->scale ? 1 : by > 255 ? 255 : by;
}

/** How a popup takes the pointer and the keys. */
enum {
    DGX_UI_POPUP_MODAL         = 1u << 0, ///< it takes every pointer and key that reaches it: nothing goes to the tree under it,
                                          ///< and the trees under it show no focus
    DGX_UI_POPUP_CLOSE_OUTSIDE = 1u << 1, ///< a finger that goes down outside it closes it (a dropdown's list)
    DGX_UI_POPUP_BACK          = 1u << 2, ///< the key back that nothing in it took closes it, its root told DGX_UI_BACK first
    DGX_UI_POPUP_SCREEN        = 1u << 3, ///< it lies on the whole screen of the tree, laid again when the screen turns; x, y, w, h not read
    DGX_UI_POPUP_PLACE         = 1u << 4, ///< it lies on the box of home's widget named by dgx_ui_page_place(), laid again when that box
                                          ///< changes, the screen while none is named; x, y, w, h not read. With MODAL a finger outside
                                          ///< it still goes to the trees under it, unless a popup over it of MODAL alone takes it
};

/** Popups stand one over another over a tree, so many at most: the painter needs no memory for their covers. */
enum { DGX_UI_POPUP_DEPTH = 8 };

/**
 * @brief A tree in the program's array of nodes.
 *
 * @param ui         The tree.
 * @param first      The program's nodes; NULL with first_size 0 when all are to come in blocks.
 * @param first_size How many the array holds.
 * @param unit       Pixels a point of the style; 0 is the same as 1.
 */
void dgx_ui_init(dgx_ui_t *ui, dgx_ui_node_t *first, int first_size, int unit);

/** @brief The blocks given back, every widget removed; the program's array is the program's. A tree with popups open
 * over it, or open as a popup itself, has them closed first: a tree being freed is told DGX_UI_COVERED if it was
 * shown, never DGX_UI_SHOWN, and the chain it lay in is told after the tree is wiped. A tree is built by dgx_ui_init()
 * only while it is not open. */
void dgx_ui_free(dgx_ui_t *ui);

/** @brief The node of a number, free or not; NULL past the tree. */
dgx_ui_node_t *dgx_ui_node(const dgx_ui_t *ui, int i);

/** @brief The value of widget i, as dgx_ui_value_of() reads it; 0 for a number that holds no widget. */
int32_t dgx_ui_value(const dgx_ui_t *ui, int i);

/** @brief The widget of a number: what the program said of it; NULL for a number that holds none. */
const dgx_ui_widget_t *dgx_ui_widget(const dgx_ui_t *ui, int i);

/**
 * @brief The tree written to the log (ESP_LOGI), and each popup over it: a line a widget, indented by its depth, of
 * its number, kind, place and size, value, state, marks, tag and the start of its text; then how many nodes it uses.
 *
 *     I dgx_ui: #0 BOX 0,0 240x320 v=0 bits=- marks=- tag=0
 *     I dgx_ui:   #1 LABEL 2,2 236x13 v=0 bits=- marks=- tag=0 "21.5 C"
 *     I dgx_ui:   #2 LIST 2,17 236x52 v=5 bits=FOCUSED marks=- tag=0 "Jan\nFeb\nMar\nApr..."
 *     I dgx_ui: nodes: 3 in use, 3 of the array of 8, blocks: 0
 *
 * A tree that outgrew its array, its nodes past it in blocks of 64 from the heap (1792 bytes each on an ESP32), is
 * told what its array should hold: "give the array 70 nodes and no block is needed". Nothing in the tree is changed.
 * A program that does not call it does not carry it.
 */
void dgx_ui_dump(const dgx_ui_t *ui);

/**
 * @brief A widget added as the last child of a parent; the first one added is the root.
 *
 * The tree keeps the pointer, not a copy: the widget must live as long as it is in the tree. Give a static const
 * object or the program's own memory, never a compound literal made inside a function, which dies with it. Its
 * state is copied into the node.
 *
 * The slot of a widget removed is taken again: a number kept by the program may then name another widget, so a
 * widget that comes and goes is found by its tag, dgx_ui_find().
 *
 * @return Its number; -1 for no such parent, or no memory for a new block (logged).
 */
int dgx_ui_add(dgx_ui_t *ui, int parent, const dgx_ui_widget_t *w);

/** @brief The same, put before the child `before` of the parent (-1: as the last). */
int dgx_ui_insert(dgx_ui_t *ui, int parent, int before, const dgx_ui_widget_t *w);

/**
 * @brief A widget removed with everything under it.
 *
 * A focus or a press inside it is dropped: no ACTIVATED comes, and the widgets removed are told nothing. Their
 * slots are taken again by dgx_ui_add(). The widget that home's pages lie over removed, or one above it
 * (dgx_ui_page_place()), the pages take the whole screen, and the trees covered or shown by that are told so here,
 * from within this call, by the rules of DGX_UI_SHOWN and DGX_UI_COVERED.
 */
void dgx_ui_remove(dgx_ui_t *ui, int i);

/**
 * @brief The first widget of a tag under a parent, not the parent itself, in the order of the tree; -1 none.
 *
 * Parent -1 looks in all of the tree, the root too. A tag of 0 is every widget's that has none: give tags from 1.
 */
int dgx_ui_find(const dgx_ui_t *ui, int parent, int tag);

/** @brief The rectangle the root is laid in, in pixels of the screen; else the painter lays it in all of the screen. */
void dgx_ui_set_root(dgx_ui_t *ui, int x, int y, int w, int h);

/**
 * @brief The tree laid again before it is next drawn or hit: after the program changed sizes, places or pictures of
 * widgets it keeps in its own RAM. A widget changed so is drawn whole by dgx_ui_redraw().
 */
void dgx_ui_relayout(dgx_ui_t *ui);

/**
 * @brief The screen the tree is shown on, told by the painter before it draws.
 *
 * A screen of another size or orientation than the last makes the tree be laid again and drawn whole; its
 * rectangle is the root's unless the program gave one.
 *
 * @param turn The orientation of the screen in any form that is equal for equal orientations.
 */
void dgx_ui_screen(dgx_ui_t *ui, int width, int height, int turn);

/** @brief The tree laid now if it has to be. The painter and the pointer call it; true when it was laid. */
bool dgx_ui_layout(dgx_ui_t *ui);

/*
 * The state, changed by the program. None of these calls on(): it tells only what the user did.
 */

/**
 * @brief The value of a control written into its place, within its min and max when min < max; an ICON's is not
 * bound: outside its range it shows none. A widget of no place: nothing. The program may as well write its memory
 * itself, in the task that owns the tree: the painter looks at every place at the start of a pass.
 */
void dgx_ui_set_value(dgx_ui_t *ui, int i, int value);

/**
 * @brief Bits of a widget's state set (on) or cleared: DGX_UI_DISABLED, DGX_UI_PRESSED; HIDDEN by dgx_ui_show().
 *
 * Disabled, a widget loses the focus, and a press of the finger on it ends there, told nothing.
 */
void dgx_ui_set_state(dgx_ui_t *ui, int i, unsigned bits, bool on);

/** @brief A widget shown or hidden: its row or column is laid again; hidden, it loses the focus and a press of the finger. */
void dgx_ui_show(dgx_ui_t *ui, int i, bool shown);

/** @brief A widget to be drawn whole when it is next drawn. */
void dgx_ui_redraw(dgx_ui_t *ui, int i);

/** @brief Whether a widget is hidden, itself or by one above it. */
bool dgx_ui_hidden(const dgx_ui_t *ui, int i);

/*
 * The pointer, the keys and the focus.
 */

/**
 * @brief The deepest widget under a pixel that answers to a pointer; -1 none. A widget is seen and touched only within
 * its parent's box: what of it lies outside, laid larger than the box or put out of it by at_x and at_y, is cut off.
 */
int dgx_ui_hit(dgx_ui_t *ui, int x, int y);

/**
 * @brief The pointer as it is now: what it is on gets it in its own coordinates.
 *
 * The widget it went down on is pressed (DGX_UI_PRESSED) till it goes up, wherever it goes; let go within
 * DGX_UI_CLICK_MS, not swiped nor dragged, it is a click: a button, a list's row, a dropdown activated, a switch
 * turned, a field told DGX_UI_EDIT. A swipe that begins on a field opens no editor.
 *
 * A list whose rows all show picks the row under the finger as it moves. A list that scrolls (more rows than its box
 * takes, dgx_ui_text_scroll()) picks nothing when the finger goes down: moved up or down half a row's height, the
 * finger moves the pick a row for each row's height it went, up going on down the list, from the pick it found, and
 * the window follows the pick, as DGX-GE's list does; let go without having moved, a click picks the row it went
 * down on.
 *
 * @return true when a widget took it: it went down on one, and holds it till it goes up; or a popup of
 * DGX_UI_POPUP_MODAL over the tree did: every pointer that reaches it is taken, down, moving or up, inside it or out,
 * but one of DGX_UI_POPUP_PLACE lets a finger outside it go to the trees under it.
 */
bool dgx_ui_pointer(dgx_ui_t *ui, const dgx_ui_pointer_t *pointer);

/**
 * @brief Whether the pointer is held by a widget that is dragged: a slider, a list, a button with DGX_UI_DRAG.
 *
 * Such a widget owns the finger: a swipe from an edge that begins on it is not the program's.
 */
bool dgx_ui_dragging(const dgx_ui_t *ui);

/**
 * @brief The value of widget i set as the user sets it, as the core sets a slider's: within min..max, the widget
 * marked, DGX_UI_CHANGED told to its on() when it changed, nothing when it is the same. For the behaviour of a widget
 * of the program's own (dgx_ui_behavior_t), and for any widget: a program's test acts as the user by it.
 */
void dgx_ui_user_value(dgx_ui_t *ui, int i, int value);

/**
 * @brief An event told to widget i's on(), DGX_UI_CHANGED, DGX_UI_ACTIVATED ..., the node as it is now: for the
 * behaviour of a widget of the program's own (a value of its own in `own` changed), and for any widget.
 */
void dgx_ui_tell(dgx_ui_t *ui, int i, int what);

/** @brief Whether the pointer is held by a widget of the tree or of its popup: it went down on one and is not up. */
bool dgx_ui_holding(const dgx_ui_t *ui);

/**
 * @brief A key of a dialog, DGX_UI_KEY_*: the focus moves, the focused one is worked; true when a widget took it.
 * DGX_UI_KEY_ACTIVATE on a field tells it DGX_UI_EDIT: the walk of the keys passes the fields and opens no editor.
 */
bool dgx_ui_key(dgx_ui_t *ui, int key);

/**
 * @brief The buttons held now, DGX_UI_PAD_*: each that went down is its key; NEXT, PREV, MORE and LESS held repeat.
 *
 * @return true when a widget took a key; BACK and the buttons of no key are the program's.
 */
bool dgx_ui_buttons(dgx_ui_t *ui, uint32_t held, uint32_t now_ms);

/**
 * @brief A table of the program's: which key of a dialog each button of the game pad gives, in place of the default.
 *
 * dgx_ui_buttons() turns each button of the pad into a key of a dialog, DGX_UI_KEY_*, by a table. By default up and
 * down move the focus (PREV, NEXT), left and right change the value of the focused widget (LESS, MORE), A activates
 * it, BACK and B are BACK, START gives no key.
 *
 * A case where that does not fit: a dialog of three buttons in a row, "Back", "OK", "Next". The user stands on "Back"
 * and presses right to get to "OK". By default right is MORE; a button has no value to change, so nothing happens:
 * the focus moves by up and down only. A table of the program's makes right NEXT and left PREV:
 *
 *     static const uint8_t row_keys[8] = {
 *         DGX_UI_KEY_PREV,     // DGX_UI_PAD_LEFT:  to the widget before
 *         DGX_UI_KEY_NEXT,     // DGX_UI_PAD_RIGHT: to the widget after
 *         DGX_UI_KEY_PREV,     // DGX_UI_PAD_UP
 *         DGX_UI_KEY_NEXT,     // DGX_UI_PAD_DOWN
 *         DGX_UI_KEY_ACTIVATE, // DGX_UI_PAD_A
 *         0xff,                // DGX_UI_PAD_START: no key, the program's
 *         DGX_UI_KEY_BACK,     // DGX_UI_PAD_BACK
 *         DGX_UI_KEY_BACK,     // DGX_UI_PAD_B
 *     };
 *     dgx_ui_button_keys(&ui, row_keys);
 *
 * The table has 8 entries, one a button, in the order of the bits of DGX_UI_PAD_* (LEFT first, B last); an entry is a
 * DGX_UI_KEY_*, or 0xff: the button gives no key and is the program's. The tree keeps the pointer, not a copy: give a
 * constant. It may be given again at any time, as when a dialog of a column gives way to one of a row. It is the table
 * of the tree given to dgx_ui_buttons(); the keys it gives go to the popups over that tree as any key does.
 *
 * @param keys The table; NULL: the default again.
 */
void dgx_ui_button_keys(dgx_ui_t *ui, const uint8_t *keys);

/**
 * @brief The time step of the logic: a held key repeats when its time comes, and a button of DGX_UI_REPEAT held by the
 * finger (in home or in a popup over it) is told again.
 * @return true while a key or such a button is held that repeats: the loop asks for time.
 */
bool dgx_ui_time(dgx_ui_t *ui, uint32_t now_ms);

/**
 * @brief The user's idle, one for the program: nothing of the user for after_ms, no finger on the screen, no button of
 * the pad held, gone down or let go, no key and no code point, in any tree of the program; a finger on the screen and a
 * button held as the last report of each has it (dgx_ui_pointer(), dgx_ui_buttons()), so that a source that reports
 * changes alone keeps the idle away under them, and dgx_ui_pointer() reporting no finger is not the user. When it
 * comes, home's trees are offered a message of the program's number, `what`, its one byte 1, as a ticker's are
 * (dgx_ui_take()); when the user wakes it, the same with 0. Both are told by dgx_ui_time() of home alone, in home's
 * task, a wake by another tree's input at home's next step. A widget hears of it by its take(), the loop by reading
 * `idle` after dgx_ui_time(), or the time itself by dgx_ui_idle_for(): to dim the screen, turn it off, show a saver
 * page, lock, go back to the first page; the backlight is the program's.
 *
 * The input that wakes it is spent while wake_acts is false, as it is read at that moment: the finger's whole gesture
 * from its press to its release goes to no tree (nothing pressed, nothing told), the key and the code points of that
 * call go nowhere, the pad's buttons held then give no key till they are let go; each such call answers true, taken,
 * dgx_ui_buttons() in the call that spends them. A new press, or a report of no finger, ends a gesture whose release
 * never came. With wake_acts the waking input acts as any: the program sets it while
 * the screen is only dimmed and its buttons show, and clears it once the screen is dark.
 *
 * A key and code points carry no time: they are counted at the next dgx_ui_time(), so the loop calls the inputs first
 * and the time after. dgx_ui_wake() gives the moment the idle comes, as a repeat's, and dgx_ui_time() asks for no time
 * for it: a loop that does not wait by dgx_ui_wake() hears of the idle when it next turns. A program that names no idle
 * carries a test of a pointer in each of those calls.
 */
typedef struct dgx_ui_idle_t {
    uint32_t after_ms;  ///< the program's: idle after so long with nothing of the user; 0, never
    int      what;      ///< the program's: the number of the message told to home, its byte 1 when the idle comes, 0 when woken
    bool     wake_acts; ///< the program's, read at the wake: the waking input acts as any; false, it is spent
    bool     idle;      ///< the program's to read: idle now, from the message's 1 till the input that wakes it
    /* the library's alone */
    bool      woken, told, spending, unstamped, finger_down, buttons_held;
    uint8_t   spent_buttons;
    dgx_ui_t *home;
    uint32_t  last_ms;
} dgx_ui_idle_t;

#if UINTPTR_MAX == 0xFFFFFFFFu
#ifdef __cplusplus
static_assert(sizeof(dgx_ui_t) == 104, "a tree is 104 bytes on a 32-bit chip");
static_assert(sizeof(dgx_ui_idle_t) == 28, "the user's idle is 28 bytes on a 32-bit chip");
#else
_Static_assert(sizeof(dgx_ui_t) == 104, "a tree is 104 bytes on a 32-bit chip");
_Static_assert(sizeof(dgx_ui_idle_t) == 28, "the user's idle is 28 bytes on a 32-bit chip");
#endif
#endif

/**
 * @brief The program's idle named, counted from now; home's dgx_ui_time() tells it. The same named again is counted
 * anew from now, and told 0 at home's next step if it was told 1; another named in its place drops it, telling it
 * nothing more. NULL: no idle, nothing more told or spent. The one dropped reads `idle` false from then on.
 */
void dgx_ui_idle(dgx_ui_t *home, dgx_ui_idle_t *idle, uint32_t now_ms);

/** @brief The milliseconds since the user's last input; 0 while one of no time waits for the next dgx_ui_time(). */
uint32_t dgx_ui_idle_for(const dgx_ui_idle_t *idle, uint32_t now_ms);

/** @brief The focus put on a widget (-1: none); its on() told DGX_UI_FOCUS. */
void dgx_ui_focus(dgx_ui_t *ui, int i);

/*
 * A popup: the only widgets that lie over others. It is a tree of its own, its root a FRAME, laid in its rectangle;
 * opaque, drawn straight on the screen. A popup may have a popup of its own, up to DGX_UI_POPUP_DEPTH over a tree:
 * a dialog, a list opened from it that hangs out of it, a question over the list. Each tree is drawn around every
 * popup over it; the pointer and the keys go to the top one first, and messages go to every tree, the bottom first.
 *
 * Where a tree is, told to its root: a tree is shown while no popup of DGX_UI_POPUP_SCREEN (a page) lies over it,
 * anywhere above it; a dialog, a dropdown's list or any popup not of the screen or of the place covers nothing. A page
 * in home's place (DGX_UI_POPUP_PLACE, dgx_ui_page_place()) covers the trees of the place alone: the lowest popup of
 * the place in the chain and those above it. Home under it stays shown, and so does a dialog opened over home before
 * the pages; a popup of the place while home names no place covers as one of the screen. Its root is told
 * DGX_UI_SHOWN when it comes to the screen (a popup opened, a page over it closed) and DGX_UI_COVERED when it leaves it
 * (a page opened over it, or it closed while shown), in pairs: a page keeps a ticker or a sensor's poll from SHOWN to
 * COVERED and never has one left running. Told by the calls that open and close (the key back's too), at the call and
 * not at a pass, once the chain of popups stands: first the trees closed that were shown, then the trees covered now,
 * from the top down, then those come to the screen, from the bottom up; the chain is read again after each, as an on()
 * may open or close in its turn. A popup opened again elsewhere stays as it was told. Home is shown from the start and
 * told nothing then; a program that wants one path for all its screens tells it itself,
 * `dgx_ui_tell(&home, 0, DGX_UI_SHOWN)`. A covered tree still takes messages. An on() told these two may close or
 * free its own tree; it never opens, closes or frees another tree closed by the same call, which the library tells
 * next, nor opens a tree being freed.
 *
 * The focus under a popup: while a popup of DGX_UI_POPUP_MODAL lies over a tree, anywhere above it, the tree's looks
 * show no focus (dgx_ui_bits_shown()); its focus stays where it is, the widget's DGX_UI_FOCUSED and ui->focus too, and
 * the mark is back where the focus is then when the popup closes, with no DGX_UI_FOCUS told. Under a popup that is not
 * modal the mark stays: the keys go to that popup first all the same when it has widgets that take the focus, and a
 * program whose popup should take the mark off the tree under it opens it DGX_UI_POPUP_MODAL.
 */

/**
 * @brief A popup opened over a tree, in a rectangle of the screen; one already open over that tree is closed first,
 * with all over it. The tree may be a popup itself.
 *
 * A finger that goes down inside the popup, or inside one over it, is theirs or nobody's: a widget they hide is never
 * pressed. Its rectangle is in pixels of the screen as it stands: on a turn of the screen the program closes it or
 * opens it again in the new pixels, unless it lies on the whole screen (DGX_UI_POPUP_SCREEN), which follows the turn.
 * @param flags DGX_UI_POPUP_MODAL, DGX_UI_POPUP_CLOSE_OUTSIDE, DGX_UI_POPUP_BACK, DGX_UI_POPUP_SCREEN.
 * @return false, nothing opened: the popup is the tree itself, or is open as a popup already (over another tree), or
 * has a popup open over it (it lies under the tree already), or the tree has DGX_UI_POPUP_DEPTH popups under it.
 */
bool dgx_ui_popup_open(dgx_ui_t *ui, dgx_ui_t *popup, int x, int y, int w, int h, unsigned flags);

/**
 * @brief The popup of a tree closed, and every popup over it: the trees under them are drawn again where they lay, at
 * the next pass of the painter.
 */
void dgx_ui_popup_close(dgx_ui_t *ui);

/**
 * @brief The popup of a tree closed as the key back closes one of DGX_UI_POPUP_BACK: its root told DGX_UI_BACK first,
 * while it is open, then it and every popup over it closed, unless that on() closed it, opened another in its place
 * or opened one over it: the program's then. That is how a page asks before it goes ("discard the changes?"): its
 * on(DGX_UI_BACK) opens the question over itself, and the page stays under it.
 * Called from that on() for the same popup, it closes it without telling again.
 * @return false: no popup is open over the tree.
 */
bool dgx_ui_popup_back(dgx_ui_t *ui);

/*
 * A page: a popup of DGX_UI_POPUP_MODAL | DGX_UI_POPUP_BACK | DGX_UI_POPUP_SCREEN, a screen of its own over the one
 * under it, closed by the key back. Pages stand one over another, up to DGX_UI_POPUP_DEPTH with the popups between
 * them. A program may open one by dgx_ui_popup_open() itself; the two calls below are the common case, in a file of
 * their own:
 *
 *     static void on_settings(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
 *     {
 *         if (what == DGX_UI_ACTIVATED) dgx_ui_page_open(&home, &settings);    // a button of the home
 *     }
 *     static void on_back(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
 *     {
 *         if (what == DGX_UI_ACTIVATED) dgx_ui_page_back(&home);             // the page's own "back"
 *     }
 *     static void on_settings_root(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
 *     {
 *         if (what == DGX_UI_BACK) save(&settings);                           // closed: its widgets read still
 *     }
 *
 * The key back closes the top page as its "back" button does; the page's root is told DGX_UI_BACK first either way.
 */

/**
 * @brief A page opened over the top page of home, or over home: first the popups over that page that a finger
 * elsewhere would close (DGX_UI_POPUP_CLOSE_OUTSIDE, a dropdown's list) are closed, from the top down to the first that
 * is not; a dialog that waits for an answer stays under the new page.
 * @return dgx_ui_popup_open()'s.
 */
bool dgx_ui_page_open(dgx_ui_t *home, dgx_ui_t *page);

/**
 * @brief The top page of home closed, with all over it; its root told DGX_UI_BACK first, while it is open.
 * @return false: no page is open over home.
 */
bool dgx_ui_page_back(dgx_ui_t *home);

/**
 * @brief Home's pages put over the box of its widget i, not over the whole screen: a bar shared by the pages, a status
 * line with a clock, a "back" button, tabs.
 *
 * The pages opened from then on are of DGX_UI_POPUP_PLACE, and those open move there at once: each lies on the box of
 * widget i and follows it when home is laid again (a turn, a widget shown or hidden), at the layout, so that a finger
 * and a composing meet it where it is. The rest of home stays on the screen and alive: its widgets take the finger,
 * a ticker of it keeps running, as home is not covered (DGX_UI_COVERED). The keys still go to the top page alone, so
 * that while a page is open the bar is of the finger alone: a program of keys only reaches what the bar does another
 * way, by the key back or a button of the page. Widget i hidden, the pages over it are hidden too, told nothing, and
 * the keys still go to the top page; removed, or one above it, the pages take the screen (dgx_ui_remove()).
 *
 * What a widget of the bar opens goes over the top of the chain, not over home: a popup opened over a tree closes the
 * one it had, the pages with it. dgx_ui_dropdown_open() does so for a dropdown of the bar itself; a program's own
 * finds the top as `dgx_ui_t *top = &home; while (top->popup) top = top->popup;` and opens over it.
 *
 * @param home The bottom of the chain, no popup itself.
 * @param i    A widget of home; -1: the whole screen again, as the pages are by default.
 * @return false: home lies over a tree as a popup, or i is no widget of it.
 */
bool dgx_ui_page_place(dgx_ui_t *home, int i);

#ifdef __cplusplus
}
#endif
