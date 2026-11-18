#pragma once
/*
 * dgx_ui.h
 *
 *  dgx-ui: a light layer of widgets on DGX. Widgets stand in rows and columns, their values come as messages from
 *  other tasks, and they animate only while a widget asks for time.
 *
 *  The core: the tree of widgets, its layout, the state of each widget, the pointer, the keys and the focus. It
 *  draws nothing and calls no function of DGX, so it runs in host tests without a display; drawing is
 *  dgx_ui_paint.h. One task owns a tree, as one task owns a screen of DGX: no call here may come from another.
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
    DGX_UI_HIDDEN   = 1u << 0, ///< not shown, takes no place in its row or column, not hit, not focused
    DGX_UI_DISABLED = 1u << 1, ///< shown as its look draws it so, not hit, not focused
    DGX_UI_FOCUSED  = 1u << 2, ///< the keys go to it
    DGX_UI_PRESSED  = 1u << 3, ///< pressed: by a finger on it, or by the program
};

/**
 * @brief The state of a widget: its bits and its value.
 *
 * A widget keeps it twice: as it is (state_value) and as it was last drawn (state_drawn). Its look draws what
 * differs between the two.
 */
typedef struct dgx_ui_state_t {
    uint8_t bits;  ///< DGX_UI_HIDDEN, DGX_UI_DISABLED, DGX_UI_FOCUSED, DGX_UI_PRESSED
    int16_t value; ///< the value of a control, within its min and max
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
    DGX_UI_ICON,     ///< a picture of an array, state_value.value picks it; outside min..max, none
    DGX_UI_LIST,     ///< rows, one picked: the value, min the first row
    DGX_UI_SWITCH,   ///< value 0 or 1
    DGX_UI_SLIDER,   ///< value min..max by step
    DGX_UI_BUTTON,   ///< pressed and let go inside: activated
    DGX_UI_INPUT,    ///< a field typed into
    DGX_UI_DROPDOWN, ///< a button showing the value picked; its list opens as a popup
    DGX_UI_OWN,      ///< drawn by a look of the program, its data in own
};

/** How a widget is built, dgx_ui_widget_t::flags: set by the program. */
enum {
    DGX_UI_COLUMN  = 1u << 0, ///< a box: children one under another
    DGX_UI_ROW     = 1u << 1, ///< a box: children side by side
    DGX_UI_STRETCH = 1u << 2, ///< across the stacking as wide (in a column) or as tall (in a row) as the box
    DGX_UI_SWIPE   = 1u << 3, ///< a slider moved by the way the finger goes from where it went down
    DGX_UI_DRAG    = 1u << 4, ///< a button dragged up or down as well as pressed (a scroll bar's knob)
    DGX_UI_CANVAS  = 1u << 5, ///< the widget has a canvas of its own, made after the layout
    DGX_UI_REST    = 1u << 6, ///< along the stacking of a column or a row, all the box has left after the others, the
                              ///< gaps and the pads; several share it, the last taking the odd pixels; never below 0
};

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
};

/** What a widget's on() is told: what the user did, never what the program set. */
enum {
    DGX_UI_ACTIVATED, ///< a click of a button, a list's row or a dropdown; a field's "done"
    DGX_UI_CHANGED,   ///< the value changed by the finger or a key
    DGX_UI_FOCUS,     ///< the focus came to it
    DGX_UI_BACK,      ///< a field's "cancel"
    DGX_UI_NO_MEMORY, ///< its canvas could not be had, told once for that size after the pass; tried again whenever its look asks
    DGX_UI_EDIT,      ///< a field clicked, or DGX_UI_KEY_ACTIVATE while it has the focus: the program opens its editor
};

typedef struct dgx_ui_t        dgx_ui_t;
typedef struct dgx_ui_msg_t    dgx_ui_msg_t;
typedef struct dgx_ui_look_t   dgx_ui_look_t;
typedef struct dgx_ui_node_t   dgx_ui_node_t;

/**
 * @brief A picture of a widget: a picture of DGX of 1, 4 or 8 bits, or one of RGB565.
 */
typedef struct dgx_ui_picture_t {
    const dgx_image_t *image;  ///< the picture of 1, 4 or 8 bits; NULL for one of RGB565
    const uint16_t    *rgb565; ///< or rows of RGB565, high byte first, as dgx_vscreen_from_array() takes them
    const uint8_t     *map;    ///< 4 or 8 bits: the colors of its values, indices of the painter's table
    int16_t            width, height; ///< the size of an RGB565 picture; of a dgx_image_t its own is taken
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

/** @brief The height a text box asks for: its lines times the line of its first font. */
int dgx_ui_textbox_height(const dgx_ui_textbox_t *box);

/**
 * @brief A widget: what the program says of it. Constant: the library never writes it, so it may lie in flash.
 *
 * Filled by its members' names; zero is the default of every member. Sizes and places are in points of the style:
 * dgx_ui_init()'s unit pixels each. What changes as the program runs, its place and its state, is the tree's: a
 * dgx_ui_node_t of it.
 */
typedef struct dgx_ui_widget_t {
    uint8_t        kind;           ///< DGX_UI_BOX ... DGX_UI_OWN
    uint8_t        flags;          ///< DGX_UI_COLUMN, ROW, STRETCH, SWIPE, DRAG, CANVAS
    uint8_t        pad, gap;       ///< inside a box: around its children and between them, in points
    int16_t        at_x, at_y;     ///< asked: from the parent's inside edge, in points; negative from the far edge
    int16_t        want_w, want_h; ///< asked, in points; 0: the content's (a picture's size, a text's lines; a text has no width of its own, nor a box any size), or across the stacking all of the box
    int16_t        min, max, step; ///< the range of a control; a list's first row and its last
    int16_t        tag;            ///< the program's: found by dgx_ui_find(), kept and nothing more
    dgx_ui_state_t state;          ///< the state it starts with: HIDDEN, DISABLED, the first value
    const dgx_ui_picture_t *picture; ///< PICTURE: one; ICON: an array of min..max
    dgx_ui_textbox_t       *text;    ///< LABEL, BUTTON, LIST, INPUT, DROPDOWN: the text box, in RAM
    const dgx_ui_look_t    *look;    ///< how it is drawn; NULL: the look of its kind
    bool (*take)(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg); ///< a message offered; true: claimed
    /** told what the user did, DGX_UI_ACTIVATED ...; node: the widget's node as it is now, its value set by the finger
        or a key in node->state_value.value (a slider moved, a list's row taken); valid while the widget is in the tree */
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
    dgx_ui_state_t         state_value;   ///< the state as it is
    dgx_ui_state_t         state_drawn;   ///< the state as it was last drawn: the painter's
    uint8_t                marks;         ///< the library's: DGX_UI_MARK_*
};

#if UINTPTR_MAX == 0xFFFFFFFFu
#ifdef __cplusplus
static_assert(sizeof(dgx_ui_widget_t) == 52, "the constant part of a widget is 52 bytes on a 32-bit chip");
static_assert(sizeof(dgx_ui_node_t) == 28, "the node of a widget is 28 bytes of RAM on a 32-bit chip");
#else
_Static_assert(sizeof(dgx_ui_widget_t) == 52, "the constant part of a widget is 52 bytes on a 32-bit chip");
_Static_assert(sizeof(dgx_ui_node_t) == 28, "the node of a widget is 28 bytes of RAM on a 32-bit chip");
#endif
#endif

/** Widgets past the program's array come in blocks of so many. */
enum { DGX_UI_BLOCK = 64 };

/** The keys of a dialog: dgx_ui_key(). */
enum {
    DGX_UI_KEY_NEXT,     ///< the focus on (in a list: the next row first)
    DGX_UI_KEY_PREV,     ///< the focus back (in a list: the row before first)
    DGX_UI_KEY_ACTIVATE, ///< the focused one pressed
    DGX_UI_KEY_BACK,     ///< the program's: no widget takes it
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
    int16_t           press_x, press_y, press_value, move_x, move_y;
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
    dgx_ui_t         *popup;       ///< the popup open over the tree, a tree of its own, maybe with its own; NULL none
    uint8_t           level;       ///< how many popups lie under it: 0 for a tree that is no popup
    uint8_t           popup_flags; ///< DGX_UI_POPUP_*
    dgx_ui_rect_t     popup_was;   ///< where a popup closed since the last pass of the painter: composed again there
};

/** How a popup takes the pointer and the keys. */
enum {
    DGX_UI_POPUP_MODAL         = 1u << 0, ///< it takes every pointer and key that reaches it: nothing goes to the tree under it
    DGX_UI_POPUP_CLOSE_OUTSIDE = 1u << 1, ///< a finger that goes down outside it closes it (a dropdown's list)
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

/** @brief The blocks given back, every widget removed; the program's array is the program's. */
void dgx_ui_free(dgx_ui_t *ui);

/** @brief The node of a number, free or not; NULL past the tree. */
dgx_ui_node_t *dgx_ui_node(const dgx_ui_t *ui, int i);

/** @brief The widget of a number: what the program said of it; NULL for a number that holds none. */
const dgx_ui_widget_t *dgx_ui_widget(const dgx_ui_t *ui, int i);

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
 * slots are taken again by dgx_ui_add().
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

/** @brief The value of a control set, within its min and max when min < max; an ICON's is not bound: outside its range it shows none. */
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
 * @return true when a widget took it: it went down on one, and holds it till it goes up.
 */
bool dgx_ui_pointer(dgx_ui_t *ui, const dgx_ui_pointer_t *pointer);

/**
 * @brief Whether the pointer is held by a widget that is dragged: a slider, a list, a button with DGX_UI_DRAG.
 *
 * Such a widget owns the finger: a swipe from an edge that begins on it is not the program's.
 */
bool dgx_ui_dragging(const dgx_ui_t *ui);

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
 * @brief The time step of the logic: a held key repeats when its time comes.
 * @return true while a key is held that repeats: the loop asks for time.
 */
bool dgx_ui_time(dgx_ui_t *ui, uint32_t now_ms);

/** @brief The focus put on a widget (-1: none); its on() told DGX_UI_FOCUS. */
void dgx_ui_focus(dgx_ui_t *ui, int i);

/*
 * A popup: the only widgets that lie over others. It is a tree of its own, its root a FRAME, laid in its rectangle;
 * opaque, drawn straight on the screen. A popup may have a popup of its own, up to DGX_UI_POPUP_DEPTH over a tree:
 * a dialog, a list opened from it that hangs out of it, a question over the list. Each tree is drawn around every
 * popup over it; the pointer and the keys go to the top one first, and messages go to every tree, the bottom first.
 */

/**
 * @brief A popup opened over a tree, in a rectangle of the screen; one already open over that tree is closed first,
 * with all over it. The tree may be a popup itself.
 *
 * A finger that goes down inside the popup, or inside one over it, is theirs or nobody's: a widget they hide is never
 * pressed. Its rectangle is in pixels of the screen as it stands: on a turn of the screen the program closes it or
 * opens it again in the new pixels.
 * @param flags DGX_UI_POPUP_MODAL, DGX_UI_POPUP_CLOSE_OUTSIDE.
 * @return false, nothing opened: the popup is the tree itself, or is open as a popup already (over another tree), or
 * has a popup open over it (it lies under the tree already), or the tree has DGX_UI_POPUP_DEPTH popups under it.
 */
bool dgx_ui_popup_open(dgx_ui_t *ui, dgx_ui_t *popup, int x, int y, int w, int h, unsigned flags);

/**
 * @brief The popup of a tree closed, and every popup over it: the trees under them are drawn again where they lay, at
 * the next pass of the painter.
 */
void dgx_ui_popup_close(dgx_ui_t *ui);

#ifdef __cplusplus
}
#endif
