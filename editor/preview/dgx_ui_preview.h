/*
 * dgx_ui_preview.h
 *
 *  A description drawn as the board draws it, for a page that edits descriptions: dgx-ui itself, its loader and its
 *  painter, over a panel in memory of the board's size and depth that takes the pixels as a panel behind a bus takes
 *  them, on the program's names: a names file, or with none every name the description uses, as dgx_ui_c reads them
 *  (tools/dgx_ui_names.c). What is the program's C stands in:
 *  - a look or a draw of the program's own: a frame with the look's name;
 *  - a picture: a crossed square of 16 pixels;
 *  - a style of the program's own, and a font that is not one of the fonts built in: a font built in, the bitmap
 *    Terminus 12 or the handwritten font0ss, so the measures of the text, and the layout, may differ from the board's;
 *  - a handler: told in the events, nothing more done; a take: takes nothing; the actions back and page are the
 *    library's, a page found by its name through preview_page_text();
 *  - a value: a place of its own, 0 until preview_value() sets it.
 *  Each stand-in in use is said in preview_notes(). Nothing runs between the calls: the page drives the time.
 *
 *  Built for the computer, and to WebAssembly for a page, from the same sources (Makefile).
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/** A verdict: 0 taken; the loader's DGX_UI_LOAD_* with its byte and name; or PREVIEW_NAMES_REFUSED, the words in said. */
typedef struct preview_verdict_t {
    int32_t code;
    int32_t offset;
    char    name[24];
    char    said[512];
} preview_verdict_t;
enum { PREVIEW_NAMES_REFUSED = -1, PREVIEW_NO_SCREEN = -2 };

/** A widget of the page as the tree has it after its layout. */
typedef struct preview_widget_t {
    int32_t     kind, parent;
    int32_t     x, y, w, h; ///< its box on the screen
    int32_t     shown;      ///< laid and drawn: not hidden, nor under a hidden parent, and not cut away by a parent
    uint32_t    bits;       ///< DGX_UI_HIDDEN, _DISABLED, _FOCUSED, _PRESSED ... as the node has them
    int32_t     value;      ///< dgx_ui_value()
    const char *text;       ///< its text, NULL none
} preview_widget_t;

/** What it was built of: {"dgx_ui": ..., "dgx": ..., "toolchain": ..., "sources": ...}, as JSON. */
const char *preview_version(void);

/**
 * The panel: the board's width and height in pixels, its depth, 16, 18 or 24, and the pixels a point of the board's
 * program, what it gives dgx_ui_init(), 1 to 16; 0, or PREVIEW_NO_SCREEN. A screen refused leaves none: nothing is drawn
 * and preview_frame() is NULL until one is taken.
 */
int preview_screen(int width, int height, int depth, int pixels_a_point);

/** Memory of the module's for a text the caller writes before handing it to a call: size bytes, until the next one. */
char *preview_text(size_t size);

/** The names file, kept for the pages after; NULL, by the rule. Its verdict, judged on its own. */
const preview_verdict_t *preview_names(const char *text, size_t length);
/** The appearance, kept and loaded again with each page, on the names; NULL, none. Its verdict on the names alone. */
const preview_verdict_t *preview_appearance(const char *text, size_t length);
/** The page: on the appearance's tables first and the names' after; drawn whole at the next paint. Its verdict. NULL, no
    page: no widget, nothing drawn. */
const preview_verdict_t *preview_page(const char *text, size_t length);

/**
 * The text of the page a description's action "page" opens, by the ctx's name, given by whoever runs the preview: a
 * function the page imports into the module, a function of the program on a computer. Its length; written to out only
 * when it is at most size, else asked again with memory enough.
 */
#ifdef __wasm__
__attribute__((import_module("env"), import_name("preview_page_text")))
#endif
size_t preview_page_text(const char *name, char *out, size_t size);

/** The finger, as a touch panel reports it. */
void preview_pointer(int x, int y, int down, uint32_t now_ms);
/** A key of a dialog, DGX_UI_KEY_*. */
void preview_key(int key);
/** The buttons held, as dgx_ui_buttons() takes them. */
void preview_buttons(uint32_t held, uint32_t now_ms);
/** A value of the names written, as the program writes it, its widgets drawn anew at the next paint; 0, or -1: none. */
int preview_value(const char *name, int32_t value);

/** The time given and the page drawn where it changed: 1 drawn, 0 nothing to draw. */
int preview_paint(uint32_t now_ms);
/** The milliseconds after now_ms when a widget asks to be drawn again; UINT32_MAX none. */
uint32_t preview_wake(uint32_t now_ms);
/** The panel's pixels, red, green, blue and 255 each, row after row. */
const uint8_t *preview_frame(void);
/** The rectangle of the pixels written since the last call, x, y, w, h: w 0 when none. */
const int32_t *preview_changed(void);

/**
 * The page on the screen: "" the page given to preview_page(), else the name of the page an action page opened over
 * it; NULL with no page. The three calls after it speak of that page.
 */
const char *preview_shown(void);
/** The widgets of the page on the screen, 0 with no page. */
int preview_count(void);
/** A widget of the page on the screen by its index, NULL past the count. */
const preview_widget_t *preview_widget(int i);
/** The widget of the page on the screen a finger at x, y would take, -1 none. */
int preview_widget_at(int x, int y);

/**
 * What happened since the last call, a line each, of these forms, a name never holding a space:
 *
 *     told <widget> <what>                      a handler of the page given was told, <what> ACTIVATED, CHANGED, FOCUS,
 *     told <widget> <what> page <name>          BACK, NO_MEMORY, EDIT, SHOWN or COVERED; or of the page <name> opened
 *     unopened <name> no-text                   the action page found no text for the context <name>
 *     unopened <name> no-room <pages>           the pages open and kept are <pages> already
 *     unopened <name> refused <code> <byte> "<error's name>"     the loader refused its text, the name to the line's end
 *     lost <count>                              the last line: so many lines more found no room
 *
 * Whole lines alone are kept, up to about 4 KB; those past it are counted.
 */
const char *preview_events(void);
/**
 * The stand-ins the pages in use take, a line each, for a person to read: of the page given "<table> <name>: ...", of
 * a page opened "page <name>: <table> <name>: ...", the tables draw, font, look, picture and style, the style of a
 * text that names none said so; kept and counted as the events are, "lost <count>" the last line.
 */
const char *preview_notes(void);
