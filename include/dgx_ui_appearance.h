#pragma once
/*
 * dgx_ui_appearance.h
 *
 *  The appearance of a program's pages as data: its colours by name, its text styles and its looks, in one document
 *  of JSON for all the pages, the format of dgx_ui_load.h with other keys at the top, loaded into the program's
 *  memory, or made C by tools/dgx_ui_c as a tree is.
 *
 *      {"dgx-ui": 1, "name": "day",
 *       "colors": {"paper": "#12161e", "ink": "#e8e8de", "fill": "#2c3c56", "border": "#44556a", "accent": "#f0a03c"},
 *       "styles": {"body": {"fonts": ["body", "small"], "align": "left", "flags": ["middle"]}},
 *       "looks":  {"slider": {"draw": "slider", "params": {"track": "border", "fill": "accent", "knob": "ink",
 *                                                         "focused": "ink", "disabled": "border", "track_h": 3}}}}
 *
 *  - "colors": a name and "#rrggbb" each. "paper", "ink", "fill" and "border" are the indices 0, 1, 2 and 3 wherever
 *    they stand, as the library's own looks take them, and each must be there; the others follow from 4 in the order
 *    written, 255 at most. A program's own look that wants a colour of the appearance takes its index by its name
 *    (dgx_ui_appearance_color(), or the constant the made C gives) and fills its parameters at the start.
 *  - "styles": a name and a style: "fonts", a list of its variants, the best first, each the name of a font of the
 *    program's table, or an object of a handwritten one, {"font", "hand", "height", "narrow", "joined", "pen",
 *    "soft"}, or of a bitmap one whose pixels are points, {"font", "points": true}, "points" beside "hand" refused as a
 *    field of no meaning; "align": "left", "center" or "right"; "flags": a list of "middle" and "important";
 *    "scroll_ms".
 *  - "looks": a name and a look as dgx_ui_look_t is: "draw", a draw of the program's table; "fill", the colour it
 *    leaves under its children, or null, what lies under it; "params", the parameters of its draw by their names, as
 *    its description below gives them.
 *  - "name": the appearance's name; "editor": an object the tools that edit it keep their own things in, at the top
 *    only, passed over.
 *
 *  What is not said: "fill" of a look is null, -1, what lies under it; a number, a word or a list of flags is 0, as a
 *  member of C left out; a colour of a look's parameters must be said, null where the field may take it (the colour
 *  under the widget), else the look is refused with DGX_UI_LOAD_MISSING and the field's name, so that no colour is
 *  forgotten into paper on paper; so must a style's "fonts", a variant's "font", and the "hand" of a handwritten font,
 *  which the fit would skip with none.
 *
 *  The appearance's looks and styles stand first in the tables it lays, the program's own after them: a name in both
 *  is the appearance's, so a look "slider" restyles every slider that takes its look by its kind's word, and the
 *  style a text takes when it names none is the appearance's first. A page loads on those tables:
 *
 *      dgx_ui_appearance_need_t need;  dgx_ui_load_error_t err;  dgx_ui_appearance_t appearance;
 *      if (dgx_ui_appearance_need(json, len, &names, &need, &err)) {
 *          void *memory = malloc(need.bytes);
 *          if (dgx_ui_appearance_load(json, len, &names, memory, need.bytes, &appearance, &err)) {
 *              colors = malloc(need.colors * sizeof *colors);              // one table for each screen
 *              dgx_ui_appearance_pack(&appearance, dgx_ui_pack_screen, scr, colors);
 *              page_names.looks = appearance.looks, page_names.look_count = appearance.look_count;
 *              page_names.styles = appearance.styles, page_names.style_count = appearance.style_count;
 *          }
 *      }
 *
 *  An appearance may only name what the program listed: its draws, its fonts and its hands are the program's tables,
 *  a draw's parameters are laid by the draw's description of them, every colour is an index below the count, every
 *  number within its field. The text may be freed after the load; the memory may not, while a tree takes its looks.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dgx_ui_load.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"

#ifdef __cplusplus
// @formatter:off
extern "C" {
// @formatter:on
#endif

/** What a field of a draw's parameters holds, and how a description writes it. */
enum {
    DGX_UI_FIELD_COLOR,          ///< a colour's name: its index, a uint8_t
    DGX_UI_FIELD_COLOR_OR_UNDER, ///< the same, or null: DGX_UI_PAPER, what lies under the widget
    DGX_UI_FIELD_NUMBER,         ///< an integer within low..high, the member of 1, 2 or 4 bytes
    DGX_UI_FIELD_BOOL,           ///< true or false, a bool
    DGX_UI_FIELD_WORD,           ///< one of the words, its place the value, the member of 1, 2 or 4 bytes
    DGX_UI_FIELD_FLAGS,          ///< a list of the words, each its place's bit, a uint8_t
    DGX_UI_FIELD_BIT,            ///< true or false: the bits `high` set in the member, or not (a pen's DGX_PEN_SOFT)
    DGX_UI_FIELD_BANDS,          ///< a list of [below, colour]: a pointer to dgx_ui_band_t, its count the uint8_t at count_offset
    DGX_UI_FIELD_FONT,           ///< a font's name, of the program's fonts
    DGX_UI_FIELD_HAND,           ///< a hand's name, of the program's hands
    DGX_UI_FIELD_VARIANTS,       ///< a list of variants of a font: a pointer, the count the uint8_t at count_offset
    DGX_UI_FIELD_PICTURE,        ///< a picture's name, of the program's pictures: a pointer to them, at least `low` there
    DGX_UI_FIELD_PART,           ///< true or false: a pointer to the field's part, or NULL (a ring's soft edges)
};

/** A field of a draw's parameters: its name in a description, where it lies and what it holds. */
typedef struct dgx_ui_field_t {
    const char *name;
    uint16_t    offset;       ///< of the member
    uint16_t    count_offset; ///< bands and variants: of their count
    uint8_t     kind;         ///< DGX_UI_FIELD_*
    uint8_t     size;         ///< of the member, in bytes
    int32_t     low, high;    ///< a number's bounds; the bits of DGX_UI_FIELD_BIT in high
    union {
        const char *words; ///< a word's or flags' words, each ended by 0, an empty one last
        const void *part;  ///< DGX_UI_FIELD_PART: what true names
    };
} dgx_ui_field_t;

/** A draw's parameters described: their size and alignment, the kinds of widget it draws, their fields. */
typedef struct dgx_ui_params_t {
    uint16_t              size, align;
    uint16_t              kinds;       ///< 1 << DGX_UI_BOX ...: what a builder offers it for; not checked by a load
    uint8_t               field_count;
    const dgx_ui_field_t *fields;
    const dgx_ui_behavior_t *behavior; ///< what a look of the draw carries as its behavior: a scrolling box's; NULL none
} dgx_ui_params_t;

/**
 * A field described by its member, for the description of the parameters of a program's own draw as of the library's:
 *     typedef struct { uint8_t needle, face; uint8_t width; } dial_params_t;
 *     static const dgx_ui_field_t  dial_fields[] = {DGX_UI_COLOR_FIELD(dial_params_t, needle), DGX_UI_COLOR_FIELD(dial_params_t, face),
 *                                                   DGX_UI_NUMBER_FIELD(dial_params_t, width, 1, 9)};
 *     static const dgx_ui_params_t dial_described = DGX_UI_PARAMS(dial_params_t, 1u << DGX_UI_OWN, dial_fields);
 *     static const dgx_ui_draw_name_t draws[]     = {DGX_UI_DRAW_NAME(slider), {"dial", draw_dial, &dial_described}};
 * The field's name in a description is the member's.
 */
#define DGX_UI_FIELD_OF(type, member, field_kind, lowest, highest, field_words)                                                     \
    {.name = #member, .offset = offsetof(type, member), .count_offset = 0, .kind = (field_kind), .size = sizeof(((type *)0)->member), \
     .low = (lowest), .high = (highest), .words = (field_words)}
#define DGX_UI_COLOR_FIELD(type, member)               DGX_UI_FIELD_OF(type, member, DGX_UI_FIELD_COLOR, 0, 0, NULL)
#define DGX_UI_COLOR_OR_UNDER_FIELD(type, member)      DGX_UI_FIELD_OF(type, member, DGX_UI_FIELD_COLOR_OR_UNDER, 0, 0, NULL)
#define DGX_UI_NUMBER_FIELD(type, member, lowest, highest) DGX_UI_FIELD_OF(type, member, DGX_UI_FIELD_NUMBER, lowest, highest, NULL)
#define DGX_UI_BOOL_FIELD(type, member)                DGX_UI_FIELD_OF(type, member, DGX_UI_FIELD_BOOL, 0, 1, NULL)
#define DGX_UI_WORD_FIELD(type, member, field_words)   DGX_UI_FIELD_OF(type, member, DGX_UI_FIELD_WORD, 0, 0, field_words)
#define DGX_UI_BANDS_FIELD(type, member, count_member)                                                                           \
    {.name = #member, .offset = offsetof(type, member), .count_offset = offsetof(type, count_member), .kind = DGX_UI_FIELD_BANDS, \
     .size = sizeof(((type *)0)->member), .low = 0, .high = 255, .words = NULL}
/** A field of true or false that names a part of the library by a pointer: true, `named`; false, NULL. */
#define DGX_UI_PART_FIELD(type, member, named)                                                                              \
    {.name = #member, .offset = offsetof(type, member), .count_offset = 0, .kind = DGX_UI_FIELD_PART, .size = sizeof(((type *)0)->member), \
     .low = 0, .high = 1, .part = (named)}
/** A field of pictures: a name of the program's pictures (dgx_ui_appearance_names_t::pictures) of at least `need` there. */
#define DGX_UI_PICTURE_FIELD(type, member, need) DGX_UI_FIELD_OF(type, member, DGX_UI_FIELD_PICTURE, need, need, NULL)
#define DGX_UI_PARAMS(type, widget_kinds, field_table)                                                                       \
    {.size = sizeof(type), .align = __alignof__(type), .kinds = (widget_kinds), .field_count = sizeof(field_table) / sizeof *(field_table), \
     .fields = (field_table)}
/** The same, of a draw whose looks carry a behaviour: dgx_ui_params_scroll's. */
#define DGX_UI_PARAMS_WITH_BEHAVIOR(type, widget_kinds, field_table, behaving)                                                    \
    {.size = sizeof(type), .align = __alignof__(type), .kinds = (widget_kinds), .field_count = sizeof(field_table) / sizeof *(field_table), \
     .fields = (field_table), .behavior = (behaving)}

/** The parameters of the library's draws, of its text styles and of a variant of a font, described. */
extern const dgx_ui_params_t dgx_ui_params_box, dgx_ui_params_frame, dgx_ui_params_text, dgx_ui_params_lines, dgx_ui_params_hand_lines,
    dgx_ui_params_button, dgx_ui_params_picture_button, dgx_ui_params_input, dgx_ui_params_switch, dgx_ui_params_slider,
    dgx_ui_params_list, dgx_ui_params_dropdown, dgx_ui_params_picture, dgx_ui_params_rgb565, dgx_ui_params_indexed,
    dgx_ui_params_radio, dgx_ui_params_checkbox, dgx_ui_params_level, dgx_ui_params_ring, dgx_ui_params_nine, dgx_ui_params_scroll;
extern const dgx_ui_params_t dgx_ui_params_text_style, dgx_ui_params_font_variant;

/** A draw by its word, and its parameters described: DGX_UI_DRAW_NAME(slider) is the library's. */
typedef struct dgx_ui_draw_name_t {
    const char *name;
    bool (*draw)(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);
    const dgx_ui_params_t *params;
} dgx_ui_draw_name_t;
#define DGX_UI_DRAW_NAME(word) {#word, dgx_ui_draw_##word, &dgx_ui_params_##word}

/** A font of DGX by its name: {"body", &dgx_font_body}, or every need of dgx_fonts.h at once,
    `#define FONT(need) {#need, &dgx_font_##need},` and `{DGX_FONT_NEEDS(FONT)}`. */
typedef struct dgx_ui_font_name_t {
    const char       *name;
    const dgx_font_t *font;
} dgx_ui_font_name_t;

/** A hand of the library's by its word: DGX_UI_HAND_NAME(handwriting), DGX_UI_HAND_NAME(handwriting_canvas). */
typedef struct dgx_ui_hand_name_t {
    const char          *name;
    const dgx_ui_hand_t *hand;
} dgx_ui_hand_name_t;
#define DGX_UI_HAND_NAME(word) {#word, &dgx_ui_##word}

/** The program's tables an appearance is read on: what it may name, and the looks and styles that come after its own. */
typedef struct dgx_ui_appearance_names_t {
    const dgx_ui_draw_name_t    *draws;
    const dgx_ui_font_name_t    *fonts;
    const dgx_ui_hand_name_t    *hands;
    const dgx_ui_picture_name_t *pictures; ///< the pictures a look's field may name, as a page's table has them
    const dgx_ui_look_name_t    *looks;
    const dgx_ui_style_name_t   *styles;
    uint16_t draw_count, font_count, hand_count, picture_count, look_count, style_count;
    /** Called by dgx_ui_appearance_need() and dgx_ui_appearance_load() before the appearance is read, as
        dgx_ui_load_names_t::start: the pictures of files of DGX read into their table. NULL: nothing to make ready. */
    void (*start)(void);
} dgx_ui_appearance_names_t;

#if UINTPTR_MAX == 0xFFFFFFFFu
#ifdef __cplusplus
static_assert(sizeof(dgx_ui_appearance_names_t) == 40, "the tables an appearance is read on are 40 bytes on a 32-bit chip");
#else
_Static_assert(sizeof(dgx_ui_appearance_names_t) == 40, "the tables an appearance is read on are 40 bytes on a 32-bit chip");
#endif
#endif

/** What an appearance takes: the bytes of the memory, aligned as a pointer; its own colours, looks and styles; its name. */
typedef struct dgx_ui_appearance_need_t {
    size_t bytes;
    int    colors, looks, styles;
    char   name[24];
} dgx_ui_appearance_need_t;

/** An appearance laid, or made C: its colours by index, 0xRRGGBB, and their names; the tables of looks and styles, its
    own first and the program's after. */
typedef struct dgx_ui_appearance_t {
    const uint32_t            *rgb;
    const char *const         *color_names;
    const dgx_ui_look_name_t  *looks;
    const dgx_ui_style_name_t *styles;
    uint16_t                   color_count, look_count, style_count;
} dgx_ui_appearance_t;

/** @brief The appearance checked whole, and what it takes. False: an error, in err. */
bool dgx_ui_appearance_need(const char *json, size_t len, const dgx_ui_appearance_names_t *names, dgx_ui_appearance_need_t *need,
                            dgx_ui_load_error_t *err);

/** @brief The appearance laid in memory of size bytes into *out. False: an error, in err, *out as it was. */
bool dgx_ui_appearance_load(const char *json, size_t len, const dgx_ui_appearance_names_t *names, void *memory, size_t size,
                            dgx_ui_appearance_t *out, dgx_ui_load_error_t *err);

/**
 * @brief Its colours packed by a function of the program's into colors[color_count], the painter's table: for a screen
 * of DGX dgx_ui_pack_screen with the screen as ctx; a table for each screen of another depth.
 */
void dgx_ui_appearance_pack(const dgx_ui_appearance_t *appearance, uint32_t (*pack)(void *ctx, uint8_t r, uint8_t g, uint8_t b), void *ctx,
                            uint32_t *colors);

/** @brief A colour packed for the screen ctx, dgx_screen_rgb(). */
uint32_t dgx_ui_pack_screen(void *ctx, uint8_t r, uint8_t g, uint8_t b);

/** @brief The index of a colour by its name; -1 when the appearance has none of it. */
int dgx_ui_appearance_color(const dgx_ui_appearance_t *appearance, const char *name);

#ifdef __cplusplus
// @formatter:off
}
// @formatter:on
#endif
