/*
 * dgx_ui_preview.h
 *
 *  A description drawn as the board draws it, for a page that edits descriptions: dgx-ui itself, its loader and its
 *  painter, over a panel in memory of the board's size and depth that takes the pixels as a panel behind a bus takes
 *  them, on the program's names: a names file, or with none every name the description uses, as dgx_ui_c reads them
 *  (tools/dgx_ui_names.c), and the appearance on its own names file where it has one. What is the program's C stands
 *  in:
 *  - a look or a draw of the program's own: a frame with the look's name;
 *  - a picture: a crossed square of 16 pixels;
 *  - a style of the program's own: Terminus 12;
 *  - a font with no file given (preview_font()) and not one of the five built in: Terminus 12, or the handwritten
 *    font0ss for one the names call handwritten, so the measures of the text, and the layout, may differ from the
 *    board's;
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
enum { PREVIEW_NAMES_REFUSED = -1, PREVIEW_NO_SCREEN = -2, PREVIEW_FONT_REFUSED = -3 };
/** The longest name of C preview_font() takes. */
enum { PREVIEW_FONT_NAME_MOST = 63 };

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
 * The format of a page as the loader reads it, from its own tables, for a page that edits descriptions to draw its
 * controls by, while the loader keeps the last word: {"version": 1, "kinds": [...], "keys": [{"key", "type", "kinds",
 * and "words", "min" and "max", "table", "null" where they are said}, ...]}. The types: kind, words, int, pair (two of
 * min to max), name (of the table said; null where "null" is true; ctx a number of min to max too, for a handler that
 * takes one), text, id (a name of the page's own), children, editor.
 */
const char *preview_format(void);
/**
 * The names of the tables the page given is read on, by the names file or, with none, by the rule, the appearance's
 * looks and styles first: {"look": [...], "value": [...], "picture": [...], "style": [...], "on": [...], "take": [...],
 * "ctx": [...], "id": [...]}. Kept from the last preview_page(), whether the page was taken or not.
 */
const char *preview_tables(void);
/**
 * Whether a text is JSON as the loader's reader of JSON reads it, in the first pass that judges a description: an
 * object at the top, nothing but white space after it. 1 JSON; 0 not; 2 deeper than the reader goes, which the
 * loader refuses by its bound of depth, not as syntax. A page's own reader of descriptions is held to it.
 */
int preview_json(const char *text, size_t length);

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
/**
 * The file of a font, a .dgxf of DGX, given for the name of C a names file or the rule takes the font by: the names
 * file's `font small TerminusTTFMedium12` takes "TerminusTTFMedium12", the rule's `small` "dgx_font_small", as
 * dgx_fonts-files.txt of DGX names them. A file wins over a font built in of its name, the project's font being the
 * board's. The bytes are copied; the file lives until it is given again for the name, or let go by NULL; any number
 * live at once, through a change of the names or the appearance. The appearance and the page are read again on it, as
 * by preview_names(). Its verdict: 0; or PREVIEW_FONT_REFUSED, the words in said, the file not taken and one given
 * before for the name kept: a name longer than PREVIEW_FONT_NAME_MOST, a file of a later format, of the other byte
 * order, or not a font, spoiled or cut. A page gives them in the order names, fonts, appearance, page.
 */
const preview_verdict_t *preview_font(const char *name, const void *bytes, size_t length);
/** The appearance, kept and loaded again with each page, on the names; NULL, none. Its verdict on the names alone. */
const preview_verdict_t *preview_appearance(const char *text, size_t length);
/**
 * The names file the appearance takes its draws, fonts and hands from, kept for the appearances given after it:
 * PREVIEW_ON_PAGE, the page's of preview_names(), as at the start; PREVIEW_ON_OWN, a names file of its own, the text
 * given, as dgx_ui_c -a reads an appearance made by dgx_ui_appearance() with NAMES (its -an); PREVIEW_ON_RULE, by the
 * rule, as -a reads one made with no NAMES. PREVIEW_ON_OWN with NULL is by the rule. The page's tables stay the page's
 * names file's. The appearance and the page are read again on it. The verdict of its own names file, judged on its own:
 * 0, or PREVIEW_NAMES_REFUSED in words, the appearance then read by the rule.
 */
enum { PREVIEW_ON_PAGE = 0, PREVIEW_ON_OWN = 1, PREVIEW_ON_RULE = 2 };
const preview_verdict_t *preview_appearance_on(int how, const char *text, size_t length);
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
 * text that names none said so; and a font whose file is of another kind than the names say, which dgx_ui_c follows
 * where the board follows the file. Kept and counted as the events are, "lost <count>" the last line.
 */
const char *preview_notes(void);
