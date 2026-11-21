/*
 * dgx_ui_load.h
 *
 *  A tree of widgets made from a description in JSON, the format a builder writes: every pointer of C is a name there,
 *  found in tables the program gives by kind, and the widgets, their text boxes and strings are laid in the program's
 *  memory. A program on C constants names none of it and carries none of it.
 *
 *      static const dgx_ui_look_name_t   looks[]    = {DGX_UI_LOOK_NAME(button), DGX_UI_LOOK_NAME(slider)};
 *      static const dgx_ui_value_name_t  values[]   = {{"volume", &settings.volume}};
 *      static const dgx_ui_on_name_t     handlers[] = {DGX_UI_ACTION_NAME(back), {"saved", on_saved, false}};
 *      static const dgx_ui_style_name_t  styles[]   = {{"text", &style}};
 *      static int                        title;      // the label the program sets
 *      static const dgx_ui_id_t          ids[]      = {{"title", &title}};
 *      static const dgx_ui_load_names_t  names = {.looks = looks, .look_count = 2, .values = values, .value_count = 1,
 *                                                 .handlers = handlers, .handler_count = 2, .styles = styles,
 *                                                 .style_count = 1, .ids = ids, .id_count = 1};
 *      ...
 *      dgx_ui_load_need_t need; dgx_ui_load_error_t err;
 *      if (dgx_ui_load_need(json, len, &names, &need, &err)) {       // the bytes, the nodes, the depth
 *          void *memory = malloc(need.bytes);                           // aligned as malloc() aligns
 *          int   root   = dgx_ui_load(&page, -1, json, len, &names, memory, need.bytes, &err);
 *          if (root >= 0) dgx_ui_text_set(&page, title, "Sound");
 *      }
 *
 *  The format, version 1: one object of the keys "dgx-ui": 1, "tree": a widget, and two that may be left out: "name",
 *  the tree's name (dgx_ui_load_need_t::name), and "editor", an object the tools that edit a description keep their
 *  own things in, which the library passes over: any JSON in it, its numbers of any kind and size, its strings checked
 *  as every string is. A widget is an object of these keys, in any order, each at most once:
 *  - "kind": "box", "frame", "label", "picture", "icon", "list", "switch", "slider", "button", "input", "dropdown",
 *    "own" or "radio", as DGX_UI_BOX ... DGX_UI_RADIO; a column or a row is a box of the flag "column" or "row";
 *  - "flags": a list of "column", "row", "stretch", "swipe", "drag", "canvas", "rest", "repeat"; "state": of "hidden",
 *    "disabled";
 *  - "pad", "gap" (0..255), "at": [x, y], "want": [w, h], "min", "max", "step", "tag" (int16): integers alone;
 *  - names of the program's tables: "look" (or null, none), "value", "picture", "style", "on", "take", and "ctx", a
 *    name of the contexts, or an integer for a handler the table says takes a number (a stepper's +5 and -5);
 *  - "text": a string, the widget's text box over it, a constant one, or of "text_size" bytes the program may write
 *    (dgx_ui_text_set()), of "lines" lines (1 by default), in the style named, the first of the table when none is;
 *  - "id": a name the program finds the widget by: the index of the widget is written to the id's place in the table,
 *    an id the table has not left as it is, so that one table serves several pages; an id of the table on two widgets
 *    is an error, one the table has not may repeat;
 *  - "children": a list of widgets, added after the widget whatever its place in the object;
 *  - "editor": an object of the editor's, as at the top, for a widget of any kind.
 *  A widget's look, when "look" is not said: none for a box, a frame and a label (the core's own), else the look the
 *  looks' table names by the kind's own word ("button" for a button), its absence an error. A key unknown, a key twice,
 *  a field of no meaning for the kind, a number out of its field or with a fraction or an exponent, a name of another
 *  character than a letter, a digit, '_', '.' or '-', or longer than 23, is an error. The top is judged in one order:
 *  a text that is not JSON; then a version that is not the integer 1; then a key of the top unknown or twice, or no
 *  "tree" (a key of the top that is no name is one unknown); then the values of the keys known. So a description of a
 *  later version is refused as one, whatever it holds, and a number of no field is told by the fault of its place. A
 *  value stands inside at most 2 * DGX_UI_LOAD_DEPTH + 2 brackets, the top's own one of them, in an editor's object as
 *  anywhere. The keys "styles", "looks", "fonts" and "pages" of the top are kept for a later version, refused in this
 *  one.
 *
 *  Not in a description: a look's parameters and colours, fonts and text styles, which are C the program names; the
 *  tree once loaded, which the program changes by the calls of dgx_ui.h as any tree.
 *
 *  A description may only name what the program listed: no number of it becomes a pointer, or an index into the
 *  program's memory, that the program did not give. So an integer "ctx" goes only to a handler that takes a number, an
 *  icon's range and a switch's or a radio's two pictures stay within the count the pictures' table gives, and the
 *  tables say what may be reached. The text may come from a file or over the air: nothing is read past len (it need
 *  not end in 0), the depth is at most DGX_UI_LOAD_DEPTH, and the whole is checked before the first widget is added: on
 *  an error the tree is as it was. The text may be freed after the load; the memory may not, while the widgets are in
 *  the tree.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dgx_ui.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"

#ifdef __cplusplus
// @formatter:off
extern "C" {
// @formatter:on
#endif

/** The deepest a description nests its widgets: the root at 1; the most ids a table may hold. */
enum { DGX_UI_LOAD_DEPTH = 24, DGX_UI_LOAD_IDS = 256 };

/** A name of a table and what it stands for. Each table is the program's, a constant. */
typedef struct dgx_ui_look_name_t {
    const char          *name;
    const dgx_ui_look_t *look;
} dgx_ui_look_name_t;
typedef struct dgx_ui_value_name_t {
    const char *name;
    int32_t    *value;
} dgx_ui_value_name_t;
typedef struct dgx_ui_picture_name_t {
    const char             *name;
    const dgx_ui_picture_t *picture; ///< one, or the array an icon, a switch or a radio takes
    uint16_t                count;   ///< the pictures there; 0 or 1: one. An icon of more in its range, a switch or a radio of fewer than two, refused
} dgx_ui_picture_name_t;
typedef struct dgx_ui_style_name_t {
    const char                *name;
    const dgx_ui_text_style_t *style;
} dgx_ui_style_name_t;
typedef struct dgx_ui_on_name_t {
    const char *name;
    void (*on)(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
    bool takes_number; ///< its ctx a number, never read as a pointer: a widget's "ctx" may be an integer with it alone
} dgx_ui_on_name_t;
typedef struct dgx_ui_take_name_t {
    const char *name;
    bool (*take)(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg);
} dgx_ui_take_name_t;
typedef struct dgx_ui_context_name_t {
    const char *name;
    void       *ctx; ///< a page's tree for DGX_UI_ACTION_NAME(page), or anything of the program's
} dgx_ui_context_name_t;
/** A widget the program finds: its index written to *index by the load, when the description has the id. */
typedef struct dgx_ui_id_t {
    const char *name;
    int        *index;
} dgx_ui_id_t;

/** A look of the library by its own word: DGX_UI_LOOK_NAME(slider) is {"slider", &dgx_ui_look_slider}. */
#define DGX_UI_LOOK_NAME(word) {#word, &dgx_ui_look_##word}

/**
 * An action of a button with no code of the program's, listed among the handlers as any: DGX_UI_ACTION_NAME(back)
 * goes back a page (dgx_ui_page_back() of the tree's home), DGX_UI_ACTION_NAME(page) opens the page that is the
 * button's "ctx" (a tree of the contexts' table). A program that lists neither carries neither.
 */
#define DGX_UI_ACTION_NAME(word) {#word, dgx_ui_load_on_##word, false}
void dgx_ui_load_on_back(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
void dgx_ui_load_on_page(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);

/** The tables of a load; a table may be NULL with its count 0. */
typedef struct dgx_ui_load_names_t {
    const dgx_ui_look_name_t    *looks;
    const dgx_ui_value_name_t   *values;
    const dgx_ui_picture_name_t *pictures;
    const dgx_ui_style_name_t   *styles;
    const dgx_ui_on_name_t      *handlers;
    const dgx_ui_take_name_t    *takes;
    const dgx_ui_context_name_t *contexts;
    const dgx_ui_id_t           *ids;
    uint16_t look_count, value_count, picture_count, style_count, handler_count, take_count, context_count, id_count;
} dgx_ui_load_names_t;

/** What a description takes: the bytes of the memory, aligned as a pointer; the nodes of the tree; its depth. And the
    tree's name, the description's "name", empty when it has none. */
typedef struct dgx_ui_load_need_t {
    size_t bytes;
    int    nodes;
    int    depth;
    char   name[24];
} dgx_ui_load_need_t;

/** What went wrong, dgx_ui_load_error_t::code. */
enum {
    DGX_UI_LOAD_OK,
    DGX_UI_LOAD_SYNTAX,    ///< not JSON, a string not well-formed UTF-8, or a value of the wrong type for its key
    DGX_UI_LOAD_VERSION,   ///< no "dgx-ui", or one that is not the integer 1
    DGX_UI_LOAD_KEY,       ///< a key unknown, or twice in one object (name: the key)
    DGX_UI_LOAD_KIND,      ///< no kind, or one unknown (name: the word)
    DGX_UI_LOAD_FIELD,     ///< a key of no meaning for the kind, a number for "ctx" its handler takes not (name: the key)
    DGX_UI_LOAD_RANGE,     ///< a number out of its field, a fraction, an exponent; pictures fewer than the widget reads (name: the picture)
    DGX_UI_LOAD_NAME_BAD,  ///< a name of a character not allowed, or longer than 23
    DGX_UI_LOAD_UNKNOWN,   ///< a name its table has not (name: the name)
    DGX_UI_LOAD_NO_LOOK,   ///< a widget with no look said, and the looks' table names none for its kind (name: the kind)
    DGX_UI_LOAD_NO_STYLE,  ///< a text and the styles' table empty
    DGX_UI_LOAD_DEPTH_MAX, ///< nested deeper than DGX_UI_LOAD_DEPTH
    DGX_UI_LOAD_MEMORY,    ///< the memory smaller than the need, or not aligned as a pointer
    DGX_UI_LOAD_NODES,     ///< dgx_ui_add() refused (no memory for a block of nodes): what was begun removed
    DGX_UI_LOAD_ID,        ///< an id of the table on a second widget, or a table of more than DGX_UI_LOAD_IDS (name: the id)
};

/** An error: its code, where in the text (the byte), and the name or the key it is about, when there is one. */
typedef struct dgx_ui_load_error_t {
    int  code;
    int  offset;
    char name[24];
} dgx_ui_load_error_t;

/**
 * @brief The description checked whole, and what it takes. False: an error, in err.
 */
bool dgx_ui_load_need(const char *json, size_t len, const dgx_ui_load_names_t *names, dgx_ui_load_need_t *need, dgx_ui_load_error_t *err);

/**
 * @brief The description's tree added under parent (-1: as the tree's root), its widgets laid in memory of size bytes.
 * @return The index of its widget at the top; -1 on an error, in err, the tree as it was.
 */
int dgx_ui_load(dgx_ui_t *ui, int parent, const char *json, size_t len, const dgx_ui_load_names_t *names, void *memory, size_t size, dgx_ui_load_error_t *err);

#ifdef __cplusplus
// @formatter:off
}
// @formatter:on
#endif
