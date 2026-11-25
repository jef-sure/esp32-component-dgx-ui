/*
 * dgx_ui_preview.c
 *
 *  A description drawn as the board draws it, for a page that edits descriptions (dgx_ui_preview.h). A page is read
 *  as dgx_ui_c reads a page on an appearance (-a): the appearance loaded on its own names, the names file's or the
 *  rule's, its looks and styles first in the page's tables, then the page's names, the file's or the rule's; then the
 *  loader, on stand-ins that work where dgx_ui_c's only mark a place.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#include "dgx_ui_preview.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_draw.h"
#include "dgx_font.h"
#include "dgx_font_file.h"
#include "dgx_image.h"
#include "dgx_screen_functions.h"
#include "dgx_screen_with_bus.h"
#include "dgx_ui.h"
#include "dgx_ui_appearance.h"
#include "dgx_ui_load.h"
#include "dgx_ui_json.h"
#include "dgx_ui_load_tables.h"
#include "dgx_ui_names.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/DroidSansRegular9.h"
#include "fonts/LiberationSansRegular12.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"

/* the versions it is built of and the hash of its sources, written by the module's build (Makefile) */
#if defined(__has_include)
#if __has_include("preview_version.h")
#include "preview_version.h"
#endif
#endif
#ifndef PREVIEW_DGX_UI_VERSION
#define PREVIEW_DGX_UI_VERSION "unknown"
#endif
#ifndef PREVIEW_DGX_VERSION
#define PREVIEW_DGX_VERSION "unknown"
#endif
#ifndef PREVIEW_TOOLCHAIN
#define PREVIEW_TOOLCHAIN "unknown"
#endif
#ifndef PREVIEW_SOURCES
#define PREVIEW_SOURCES "unknown"
#endif

/* no memory in the reading of the names: the module stops, as a fault the page tells */
_Noreturn void names_no_memory(void)
{
    abort();
}

static void *zeroed(size_t count, size_t size)
{
    void *made = calloc(count ? count : 1, size);
    if (!made) names_no_memory();
    return made;
}

/* ---- the panel: a display behind a bus in memory, its pixels kept as the page shows them ---- */

typedef struct panel_t {
    dgx_screen_with_bus_t sbus; /* first: the screen is the panel */
    dgx_bus_protocols_t   bus;
    uint8_t               bus_buffer[256];
    int                   bytes; /* of a pixel on the bus: 2, or 3 for 18 and 24 bits */
    uint8_t              *rgba;
    int                   left, right, top, bottom, x, y;
    int                   changed_left, changed_top, changed_right, changed_bottom; /* right < left: nothing */
    uint32_t              written;                                                  /* pixels, ever */
} panel_t;

static panel_t *panel;

static void panel_nothing_changed(panel_t *p)
{
    p->changed_left = p->changed_top = INT16_MAX, p->changed_right = p->changed_bottom = -1;
}

static void panel_set_area(dgx_screen_t *scr, uint16_t left, uint16_t right, uint16_t top, uint16_t bottom)
{
    panel_t *p = (panel_t *)scr;
    if (left > right) DGX_INT_SWAP(left, right);
    if (top > bottom) DGX_INT_SWAP(top, bottom);
    p->left = left, p->right = right, p->top = top, p->bottom = bottom, p->x = left, p->y = top;
}

static void panel_write_area(dgx_screen_t *scr, uint8_t *data, uint32_t lenbits)
{
    panel_t  *p      = (panel_t *)scr;
    const int width  = scr->width, height = scr->height;
    const int pixels = (int)(lenbits / (8u * (unsigned)p->bytes));
    for (int k = 0; k < pixels; ++k) {
        if (p->x < width && p->y < height) {
            const uint8_t *in  = data + p->bytes * k;
            uint8_t       *out = p->rgba + 4 * ((size_t)p->y * (size_t)width + (size_t)p->x);
            if (p->bytes == 2) {
                const unsigned pixel = (unsigned)in[0] << 8 | in[1];
                const unsigned red = pixel >> 11, green = (pixel >> 5) & 0x3f, blue = pixel & 0x1f;
                out[0] = (uint8_t)(red << 3 | red >> 2), out[1] = (uint8_t)(green << 2 | green >> 4), out[2] = (uint8_t)(blue << 3 | blue >> 2);
            } else if (scr->color_bits == 18) {
                for (int c = 0; c < 3; ++c) out[c] = (uint8_t)((in[c] & 0xfc) | in[c] >> 6);
            } else {
                out[0] = in[0], out[1] = in[1], out[2] = in[2];
            }
            out[3] = 255;
            ++p->written;
            if (p->x < p->changed_left) p->changed_left = p->x;
            if (p->x > p->changed_right) p->changed_right = p->x;
            if (p->y < p->changed_top) p->changed_top = p->y;
            if (p->y > p->changed_bottom) p->changed_bottom = p->y;
        }
        if (++p->x > p->right) {
            p->x = p->left;
            if (++p->y > p->bottom) p->y = p->top;
        }
    }
}

static void panel_wait(dgx_screen_t *scr)
{
    (void)scr;
}

static void panel_update(dgx_screen_t *scr, int left, int right, int top, int bottom)
{
    (void)scr, (void)left, (void)right, (void)top, (void)bottom;
}

static void panel_free(void)
{
    if (!panel) return;
    free(panel->rgba), free(panel);
    panel = NULL;
}

/* ---- what is said: the events, the notes, the verdicts ---- */

/* lines kept to be read: whole lines alone, those that find no room counted, the count said as the last line read */
enum { SAID_ROOM = 4096, SAID_LOST_ROOM = 64 };
typedef struct said_t {
    char     text[SAID_ROOM];
    size_t   used;
    unsigned lost;
} said_t;

static said_t events, notes;

static void say(said_t *to, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void say(said_t *to, const char *fmt, ...)
{
    char    line[256];
    va_list args;
    va_start(args, fmt);
    const int wrote = vsnprintf(line, sizeof line, fmt, args);
    va_end(args);
    if (wrote < 0) return;
    const size_t length = (size_t)wrote < sizeof line ? (size_t)wrote : sizeof line - 1;
    if (to->used + length >= SAID_ROOM - SAID_LOST_ROOM) {
        ++to->lost;
        return;
    }
    memcpy(to->text + to->used, line, length + 1);
    to->used += length;
}

static void said_clear(said_t *said)
{
    said->used = 0, said->lost = 0, said->text[0] = 0;
}

/* the lines, and how many were lost when there were */
static const char *said_read(const said_t *said, char out[SAID_ROOM])
{
    memcpy(out, said->text, said->used + 1);
    if (said->lost) snprintf(out + said->used, SAID_ROOM - said->used, "lost %u\n", said->lost);
    return out;
}

static void verdict_of(preview_verdict_t *verdict, const dgx_ui_load_error_t *err)
{
    *verdict = (preview_verdict_t){.code = err->code, .offset = err->offset};
    memcpy(verdict->name, err->name, sizeof verdict->name);
}

/* ---- the library's looks and draws by their words, the fonts built in ---- */

static const dgx_ui_look_name_t library_look_objects[] = {
#define LOOK(word) DGX_UI_LOOK_NAME(word),
#include "library_looks.h"
#undef LOOK
};
static const dgx_ui_draw_name_t library_draw_functions[] = {
#define DRAW(word, type) DGX_UI_DRAW_NAME(word),
#define COUNT(type, member, count)
#include "library_draws.h"
#undef DRAW
#undef COUNT
};

/* the library's look a C names, "&dgx_ui_look_<word>"; NULL else */
static const dgx_ui_look_t *library_look_of(const char *c)
{
    static const char lead[] = "&dgx_ui_look_";
    if (!c || strncmp(c, lead, sizeof lead - 1) != 0) return NULL;
    for (size_t k = 0; k < sizeof library_look_objects / sizeof *library_look_objects; ++k)
        if (strcmp(library_look_objects[k].name, c + sizeof lead - 1) == 0) return library_look_objects[k].look;
    return NULL;
}

/* the library's draw a C names, "dgx_ui_draw_<word>"; NULL else */
static const dgx_ui_draw_name_t *library_draw_function_of(const char *c)
{
    static const char lead[] = "dgx_ui_draw_";
    if (!c || strncmp(c, lead, sizeof lead - 1) != 0) return NULL;
    for (size_t k = 0; k < sizeof library_draw_functions / sizeof *library_draw_functions; ++k)
        if (strcmp(library_draw_functions[k].name, c + sizeof lead - 1) == 0) return &library_draw_functions[k];
    return NULL;
}

static const struct {
    const char       *name;
    const dgx_font_t *font;
} built_in_fonts[] = {
    {"TerminusTTFMedium12", &TerminusTTFMedium12}, {"DejaVuSansBook32", &DejaVuSansBook32}, {"LiberationSansRegular12", &LiberationSansRegular12},
    {"DroidSansRegular9", &DroidSansRegular9},     {"font0ss", &font0ss},
};
enum { BUILT_IN_FONTS = sizeof built_in_fonts / sizeof *built_in_fonts };

/* the font a names file's C names, "&<name>", when it is one built in; NULL else */
static const dgx_font_t *built_in_font(const char *c)
{
    if (!c || c[0] != '&') return NULL;
    for (int k = 0; k < BUILT_IN_FONTS; ++k)
        if (strcmp(built_in_fonts[k].name, c + 1) == 0) return built_in_fonts[k].font;
    return NULL;
}

/* ---- the files of fonts, by the names of C they stand for ---- */

typedef struct font_file_t {
    char              name[PREVIEW_FONT_NAME_MOST + 1];
    void             *bytes;  /* a copy of the file, aligned as malloc() gives it: the tables read in place */
    void             *memory; /* what dgx_font_file_load() lays */
    const dgx_font_t *font;
} font_file_t;

static font_file_t *font_files;
static int          font_file_count, font_file_room;

/* the font of a file given for a C, "&<name>"; NULL none */
static const dgx_font_t *font_file_of(const char *c)
{
    for (int k = 0; c && c[0] == '&' && k < font_file_count; ++k)
        if (strcmp(font_files[k].name, c + 1) == 0) return font_files[k].font;
    return NULL;
}

/* ---- the stand-ins that are the same for every page ---- */

/* a style of the program's own */
static const dgx_ui_font_variant_t stand_variant[1] = {{.font = &TerminusTTFMedium12}};
static const dgx_ui_text_style_t   stand_style      = {.fonts = stand_variant, .count = 1};

/* a picture: a crossed square of 16 pixels, as many as a table may say */
static const uint8_t crossed_bits[32] = {0xff, 0xff, 0xc0, 0x03, 0xa0, 0x05, 0x90, 0x09, 0x88, 0x11, 0x84, 0x21, 0x82, 0x41, 0x81, 0x81,
                                         0x81, 0x81, 0x82, 0x41, 0x84, 0x21, 0x88, 0x11, 0x90, 0x09, 0xa0, 0x05, 0xc0, 0x03, 0xff, 0xff};
static const dgx_image_t crossed       = {.width = 16, .height = 16, .format = DGX_IMAGE_1BPP, .data = crossed_bits};
static dgx_ui_picture_t *crossed_pictures; /* 65535 of them, made at the first need */

static const dgx_ui_picture_t *crossed_picture(void)
{
    if (!crossed_pictures) {
        crossed_pictures = zeroed(65535, sizeof *crossed_pictures);
        for (int k = 0; k < 65535; ++k) crossed_pictures[k].image = &crossed;
    }
    return crossed_pictures;
}

/* ---- the appearance: loaded on its own names, its looks and styles first in each page's tables ---- */

typedef struct appearance_load_t {
    char                     *text;
    size_t                    length;
    preview_verdict_t         verdict;
    bool                      loaded;
    dgx_ui_appearance_t       appearance;
    dgx_ui_appearance_need_t  need;
    void                     *memory;
    dgx_ui_draw_name_t       *draws;
    dgx_ui_font_name_t       *fonts;
    dgx_ui_hand_name_t       *hands;
    dgx_ui_picture_name_t    *pictures;
    char                    (*names)[24]; /* the names of its draws, fonts, hands and pictures, kept */
    int                       draw_count, font_count, hand_count, picture_count;
} appearance_load_t;

static appearance_load_t appearance_load;
static char             *names_text;
static size_t            names_length;
static bool              names_given;
/* the names file of the appearance's draws, fonts and hands: the page's, its own, or the rule */
static int               appearance_on = PREVIEW_ON_PAGE;
static char             *appearance_names_text;
static size_t            appearance_names_length;

static void names_free_all(void)
{
    table_t *every[] = {&tables[0], &tables[1], &tables[2], &tables[3], &tables[4], &tables[5], &tables[6], &tables[7],
                        &used[0], &used[1], &used[2], &used[3], &used[4], &used[5], &used[6], &used[7],
                        &appearance_tables[0], &appearance_tables[1], &appearance_tables[2], &appearance_tables[3]};
    _Static_assert(TABLES == 8 && APPEARANCE_TABLES == 4, "every table above");
    for (size_t t = 0; t < sizeof every / sizeof *every; ++t) {
        for (int k = 0; k < every[t]->count; ++k) free(every[t]->entries[k].c), free(every[t]->entries[k].described);
        free(every[t]->entries);
        *every[t] = (table_t){0};
    }
}

/* the names file's lines, or of an appearance's alone; the file was judged when it was given */
static void names_file_lines(bool appearance_alone)
{
    if (!names_given) return;
    char *said = names_read("names", names_text, names_length, appearance_alone);
    free(said);
}

/* whether the appearance is read on a names file, its own or the page's, and not by the rule */
static bool appearance_named(void)
{
    return appearance_on == PREVIEW_ON_OWN || (appearance_on == PREVIEW_ON_PAGE && names_given);
}

static bool draw_named(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);

static void appearance_free(void)
{
    appearance_load_t *a = &appearance_load;
    free(a->memory), free(a->draws), free(a->fonts), free(a->hands), free(a->pictures), free(a->names);
    a->memory = NULL, a->draws = NULL, a->fonts = NULL, a->hands = NULL, a->pictures = NULL, a->names = NULL, a->loaded = false;
    a->draw_count = a->font_count = a->hand_count = a->picture_count = 0;
}

/* the appearance loaded on its names, as dgx_ui_c -a reads it: no look or style of the program's after its own */
static void appearance_read(void)
{
    appearance_load_t *a = &appearance_load;
    appearance_free();
    a->verdict = (preview_verdict_t){0};
    if (!a->text) return;
    names_free_all();
    if (appearance_on == PREVIEW_ON_OWN) {
        free(names_read("appearance's names", appearance_names_text, appearance_names_length, true));
    } else if (appearance_named()) {
        names_file_lines(true);
    } else {
        for (int k = 0; k < library_draw_count; ++k) {
            entry_t *draw = entry_add(&appearance_tables[A_DRAW], library_draws[k].word);
            draw->library = k, draw->c = printed("dgx_ui_draw_%s", library_draws[k].word);
        }
        for (int k = 0; k < 2; ++k) entry_add(&appearance_tables[A_HAND], library_hands[k])->c = printed("&dgx_ui_%s", library_hands[k]);
        names_collect_fonts(a->text, a->length);
    }
    const table_t *draws_of = &appearance_tables[A_DRAW], *fonts_of = &appearance_tables[A_FONT], *hands_of = &appearance_tables[A_HAND];
    const table_t *pictures_of = &appearance_tables[A_PICTURE];
    const int      count    = draws_of->count + fonts_of->count + hands_of->count + pictures_of->count;
    a->names                = zeroed((size_t)count, sizeof *a->names);
    a->pictures             = zeroed((size_t)pictures_of->count, sizeof *a->pictures);
    a->picture_count        = pictures_of->count;
    a->draws                = zeroed((size_t)draws_of->count, sizeof *a->draws);
    a->fonts                = zeroed((size_t)fonts_of->count, sizeof *a->fonts);
    a->hands                = zeroed((size_t)hands_of->count, sizeof *a->hands);
    a->draw_count = draws_of->count, a->font_count = fonts_of->count, a->hand_count = hands_of->count;
    int named               = 0;
    for (int k = 0; k < draws_of->count; ++k) {
        const entry_t            *draw    = &draws_of->entries[k];
        const dgx_ui_draw_name_t *library = library_draw_function_of(draw->c);
        memcpy(a->names[named], draw->name, 24);
        a->draws[k] = library ? (dgx_ui_draw_name_t){a->names[named], library->draw, library->params}
                              : (dgx_ui_draw_name_t){a->names[named], draw_named, NULL};
        if (!library) say(&notes, "draw %s: the program's own, a frame with the look's name\n", draw->name);
        ++named;
    }
    /* a font: its file, which wins over a font built in of its name; else the one built in; else a stand-in */
    for (int k = 0; k < fonts_of->count; ++k) {
        const entry_t    *font  = &fonts_of->entries[k];
        const dgx_font_t *filed = font_file_of(font->c), *built = filed ? NULL : built_in_font(font->c);
        const char       *c     = font->c && font->c[0] == '&' ? font->c + 1 : font->c ? font->c : "";
        memcpy(a->names[named], font->name, 24);
        a->fonts[k] = (dgx_ui_font_name_t){a->names[named], filed ? filed : built ? built : font->handwritten ? &font0ss : &TerminusTTFMedium12};
        if (!filed && !built)
            say(&notes, "font %s: no file for %s, %s stands in, its measures not the board's\n", font->name, c,
                font->handwritten ? "font0ss" : "TerminusTTFMedium12");
        /* the kind the names say, which dgx_ui_c follows, against the file's, which the loader and the board follow */
        const bool written = filed && filed->f_type == DGX_FONT_HW;
        if (filed && written != font->handwritten)
            say(&notes, "font %s: %s, its file is %s\n", font->name,
                font->handwritten    ? "the names file calls it handwritten"
                : appearance_named() ? "the names file does not call it handwritten"
                                     : "taken for a bitmap font by the rule",
                written ? "a handwritten font" : "a bitmap font");
        ++named;
    }
    for (int k = 0; k < hands_of->count; ++k) {
        const entry_t *hand = &hands_of->entries[k];
        memcpy(a->names[named], hand->name, 24);
        const bool canvas = strcmp(hand->c, "&dgx_ui_handwriting_canvas") == 0;
        a->hands[k]       = (dgx_ui_hand_name_t){a->names[named], canvas ? &dgx_ui_handwriting_canvas : &dgx_ui_handwriting};
        ++named;
    }
    /* a picture: the crossed square stands in, as for a page's, so many as the names give */
    for (int k = 0; k < pictures_of->count; ++k) {
        const entry_t *picture = &pictures_of->entries[k];
        memcpy(a->names[named], picture->name, 24);
        a->pictures[k] = (dgx_ui_picture_name_t){a->names[named], crossed_picture(), (uint16_t)picture->count};
        say(&notes, "picture %s: a crossed square of 16 pixels stands in\n", picture->name);
        ++named;
    }
    const dgx_ui_appearance_names_t names = {.draws = a->draws, .fonts = a->fonts, .hands = a->hands, .pictures = a->pictures,
                                             .draw_count = (uint16_t)draws_of->count, .font_count = (uint16_t)fonts_of->count,
                                             .hand_count = (uint16_t)hands_of->count, .picture_count = (uint16_t)pictures_of->count};
    names_free_all();
    dgx_ui_load_error_t err;
    if (!dgx_ui_appearance_need(a->text, a->length, &names, &a->need, &err)) {
        verdict_of(&a->verdict, &err);
        return;
    }
    a->memory = zeroed(1, a->need.bytes);
    if (!dgx_ui_appearance_load(a->text, a->length, &names, a->memory, a->need.bytes, &a->appearance, &err)) {
        verdict_of(&a->verdict, &err);
        return;
    }
    a->loaded = true;
}

/* ---- a tree: the home page, or a page an action opened, each with its tables and stand-ins ---- */

enum { PAGES = 8 };

typedef struct tree_t {
    char                name[24]; /* "" the home page; else the name of the context that opened it */
    bool                loaded;
    dgx_ui_t            ui;
    dgx_ui_node_t      *nodes;
    void               *memory;
    dgx_ui_load_names_t names;
    char              (*names_of)[24]; /* the names of its tables, kept */
    dgx_ui_look_t      *own_looks;     /* the program's own: a frame with the name */
    int                 own_look_count;
    int32_t            *values;
    uint8_t            *contexts;
    int                *ids;
} tree_t;

static int           unit = 1;       /* pixels a point, as the board's program gives dgx_ui_init() */
static char          note_lead[40]; /* "" for the home page's notes, "page <name>: " for a page's */
static tree_t        home;
static tree_t        pages[PAGES];
static dgx_ui_paint_t paint;
static uint32_t     *colors;
static uint16_t      color_count;
static preview_verdict_t page_verdict;

static void tree_free(tree_t *tree)
{
    if (tree->loaded) dgx_ui_free(&tree->ui);
    free(tree->nodes), free(tree->memory), free(tree->names_of), free(tree->own_looks), free(tree->values), free(tree->contexts), free(tree->ids);
    free((void *)tree->names.looks), free((void *)tree->names.values), free((void *)tree->names.pictures), free((void *)tree->names.styles);
    free((void *)tree->names.handlers), free((void *)tree->names.takes), free((void *)tree->names.contexts), free((void *)tree->names.ids);
    *tree = (tree_t){0};
}

/* the name of an opened page's tree; NULL for home */
static const char *tree_name_of(const dgx_ui_t *ui)
{
    for (int k = 0; k < PAGES; ++k)
        if (pages[k].loaded && &pages[k].ui == ui) return pages[k].name;
    return NULL;
}

typedef void (*on_fn)(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);

static const char *const what_words[] = {"ACTIVATED", "CHANGED", "FOCUS", "BACK", "NO_MEMORY", "EDIT", "SHOWN", "COVERED"};
_Static_assert(DGX_UI_ACTIVATED == 0 && DGX_UI_COVERED == 7, "the words of what a handler is told, in its order");

static void tell(const dgx_ui_t *ui, int i, int what)
{
    const char *page = tree_name_of(ui);
    say(&events, "told %d %s%s%s\n", i, what >= 0 && what <= DGX_UI_COVERED ? what_words[what] : "?", page ? " page " : "", page ? page : "");
}

static void on_told(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    (void)node, (void)ctx;
    tell(ui, i, what);
}

static void on_back(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    tell(ui, i, what);
    dgx_ui_load_on_back(ui, i, node, what, ctx);
}

static bool take_nothing(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    (void)ui, (void)i, (void)msg;
    return false;
}

static bool tree_load(tree_t *tree, const char *text, size_t length, preview_verdict_t *verdict);

/* the action page: the page named by the button's context, loaded once and kept, as a program keeps its pages */
static void on_page(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    (void)node;
    tell(ui, i, what);
    if (what != DGX_UI_ACTIVATED || !ctx) return;
    const tree_t *of = &home;
    for (int k = 0; k < PAGES; ++k)
        if (pages[k].loaded && &pages[k].ui == ui) of = &pages[k];
    const uint8_t *place = ctx;
    if (place < of->contexts || place >= of->contexts + of->names.context_count) return;
    const char *name = of->names.contexts[place - of->contexts].name;
    tree_t     *page = NULL;
    for (int k = 0; k < PAGES && !page; ++k)
        if (pages[k].loaded && strcmp(pages[k].name, name) == 0) page = &pages[k];
    for (int k = 0; k < PAGES && !page; ++k)
        if (!pages[k].loaded) page = &pages[k];
    if (!page) {
        say(&events, "unopened %s no-room %d\n", name, PAGES);
        return;
    }
    if (!page->loaded) {
        char        *text   = NULL;
        const size_t length = preview_page_text(name, NULL, 0);
        if (length) text = zeroed(1, length), preview_page_text(name, text, length);
        preview_verdict_t verdict;
        snprintf(note_lead, sizeof note_lead, "page %s: ", name);
        const bool taken = text && tree_load(page, text, length, &verdict);
        note_lead[0]     = 0;
        free(text);
        if (!taken) {
            if (!length) say(&events, "unopened %s no-text\n", name);
            else say(&events, "unopened %s refused %d %d \"%s\"\n", name, (int)verdict.code, (int)verdict.offset, verdict.name);
            return;
        }
        snprintf(page->name, sizeof page->name, "%s", name);
    }
    dgx_ui_page_open(&home.ui, &page->ui);
}

/* the tables of a page, as dgx_ui_c -a makes them: the appearance's looks and styles first, then the names file's
   or the rule's; each entry its stand-in */
static void tree_names(tree_t *tree, const char *text, size_t length)
{
    names_free_all();
    if (names_given) {
        names_file_lines(false);
        for (int table = 0; table < TABLES; ++table) tables[table].from_file = true;
    }
    const appearance_load_t *a = &appearance_load;
    const int own_looks = a->loaded ? a->need.looks : 0, own_styles = a->loaded ? a->need.styles : 0;
    for (int table = T_LOOK; table <= T_STYLE; table += T_STYLE - T_LOOK) {
        const int own   = table == T_LOOK ? own_looks : own_styles;
        table_t   moved = tables[table];
        tables[table]   = (table_t){.from_file = moved.from_file};
        for (int k = 0; k < own; ++k)
            entry_add(&tables[table], table == T_LOOK ? a->appearance.looks[k].name : a->appearance.styles[k].name)->from_appearance = true;
        for (int k = 0; k < moved.count; ++k) *entry_add(&tables[table], moved.entries[k].name) = moved.entries[k];
        free(moved.entries);
    }
    names_collect(text, length);
    names_rule(!(a->loaded && a->need.styles));

    int count = 0;
    for (int table = 0; table < TABLES; ++table) count += tables[table].count;
    tree->names_of = zeroed((size_t)count, sizeof *tree->names_of);
    int named      = 0;
#define NAME_OF(table, k) (memcpy(tree->names_of[named], tables[table].entries[k].name, 24), tree->names_of[named++])

    dgx_ui_look_name_t *looks = zeroed((size_t)tables[T_LOOK].count, sizeof *looks);
    tree->own_looks           = zeroed((size_t)tables[T_LOOK].count, sizeof *tree->own_looks);
    for (int k = 0, appearance_k = 0; k < tables[T_LOOK].count; ++k) {
        const entry_t       *entry   = &tables[T_LOOK].entries[k];
        const char          *name    = NAME_OF(T_LOOK, k);
        const dgx_ui_look_t *library = library_look_of(entry->c);
        if (entry->from_appearance) {
            looks[k] = (dgx_ui_look_name_t){name, a->appearance.looks[appearance_k++].look};
        } else if (library) {
            looks[k] = (dgx_ui_look_name_t){name, library};
        } else {
            dgx_ui_look_t *own = &tree->own_looks[tree->own_look_count++];
            *own               = (dgx_ui_look_t){.draw = draw_named, .params = name, .fill = -1};
            looks[k]           = (dgx_ui_look_name_t){name, own};
            say(&notes, "%slook %s: the program's own, a frame with its name\n", note_lead, name);
        }
    }
    tree->values               = zeroed((size_t)tables[T_VALUE].count, sizeof *tree->values);
    dgx_ui_value_name_t *values = zeroed((size_t)tables[T_VALUE].count, sizeof *values);
    for (int k = 0; k < tables[T_VALUE].count; ++k) values[k] = (dgx_ui_value_name_t){NAME_OF(T_VALUE, k), &tree->values[k]};
    dgx_ui_picture_name_t *pictures = zeroed((size_t)tables[T_PICTURE].count, sizeof *pictures);
    for (int k = 0; k < tables[T_PICTURE].count; ++k) {
        pictures[k] = (dgx_ui_picture_name_t){NAME_OF(T_PICTURE, k), crossed_picture(), (uint16_t)tables[T_PICTURE].entries[k].count};
        say(&notes, "%spicture %s: a crossed square stands in\n", note_lead, pictures[k].name);
    }
    dgx_ui_style_name_t *styles = zeroed((size_t)tables[T_STYLE].count, sizeof *styles);
    for (int k = 0, appearance_k = 0; k < tables[T_STYLE].count; ++k) {
        const entry_t *entry = &tables[T_STYLE].entries[k];
        const char    *name  = NAME_OF(T_STYLE, k);
        styles[k] = (dgx_ui_style_name_t){name, entry->from_appearance ? a->appearance.styles[appearance_k++].style : &stand_style};
        if (!entry->from_appearance)
            say(&notes, "%sstyle %s: the program's own, TerminusTTFMedium12 stands in\n", note_lead,
                strcmp(name, DEFAULT_STYLE) == 0 ? "of a text that names none" : name);
    }
    dgx_ui_on_name_t *handlers = zeroed((size_t)tables[T_ON].count, sizeof *handlers);
    for (int k = 0; k < tables[T_ON].count; ++k) {
        const entry_t *entry = &tables[T_ON].entries[k];
        const char    *c     = entry->c ? entry->c : "";
        const on_fn    on    = strcmp(c, "dgx_ui_load_on_back") == 0 ? on_back : strcmp(c, "dgx_ui_load_on_page") == 0 ? on_page : on_told;
        handlers[k]          = (dgx_ui_on_name_t){NAME_OF(T_ON, k), on, (uint8_t)takes_of(entry)};
    }
    dgx_ui_take_name_t *takes = zeroed((size_t)tables[T_TAKE].count, sizeof *takes);
    for (int k = 0; k < tables[T_TAKE].count; ++k) takes[k] = (dgx_ui_take_name_t){NAME_OF(T_TAKE, k), take_nothing};
    tree->contexts                  = zeroed((size_t)tables[T_CTX].count, 1);
    dgx_ui_context_name_t *contexts = zeroed((size_t)tables[T_CTX].count, sizeof *contexts);
    for (int k = 0; k < tables[T_CTX].count; ++k) contexts[k] = (dgx_ui_context_name_t){NAME_OF(T_CTX, k), &tree->contexts[k], tables[T_CTX].entries[k].page};
    tree->ids        = zeroed((size_t)tables[T_ID].count, sizeof *tree->ids);
    dgx_ui_id_t *ids = zeroed((size_t)tables[T_ID].count, sizeof *ids);
    for (int k = 0; k < tables[T_ID].count; ++k) ids[k] = (dgx_ui_id_t){NAME_OF(T_ID, k), &tree->ids[k]}, tree->ids[k] = -1;
#undef NAME_OF
    tree->names = (dgx_ui_load_names_t){.looks = looks, .values = values, .pictures = pictures, .styles = styles, .handlers = handlers,
                                        .takes = takes,
                                        .contexts = contexts, .ids = ids, .look_count = (uint16_t)tables[T_LOOK].count,
                                        .value_count = (uint16_t)tables[T_VALUE].count, .picture_count = (uint16_t)tables[T_PICTURE].count,
                                        .style_count = (uint16_t)tables[T_STYLE].count, .handler_count = (uint16_t)tables[T_ON].count,
                                        .take_count = (uint16_t)tables[T_TAKE].count, .context_count = (uint16_t)tables[T_CTX].count,
                                        .id_count = (uint16_t)tables[T_ID].count};
    names_free_all();
}

static bool tree_load(tree_t *tree, const char *text, size_t length, preview_verdict_t *verdict)
{
    tree_free(tree);
    *verdict = (preview_verdict_t){0};
    tree_names(tree, text, length);
    dgx_ui_load_need_t  need;
    dgx_ui_load_error_t err;
    if (!dgx_ui_load_need(text, length, &tree->names, &need, &err)) {
        verdict_of(verdict, &err);
        return false;
    }
    tree->nodes  = zeroed((size_t)need.nodes, sizeof *tree->nodes);
    tree->memory = zeroed(1, need.bytes);
    dgx_ui_init(&tree->ui, tree->nodes, need.nodes, unit);
    if (dgx_ui_load(&tree->ui, -1, text, length, &tree->names, tree->memory, need.bytes, &err) < 0) {
        verdict_of(verdict, &err);
        dgx_ui_free(&tree->ui);
        return false;
    }
    tree->loaded = true;
    return true;
}

/* the name of a look a frame stands in for: the program's own, or the appearance's of a draw of the program's */
static const char *name_of_look(const dgx_ui_look_t *look)
{
    const tree_t *trees[PAGES + 1] = {&home};
    for (int k = 0; k < PAGES; ++k) trees[k + 1] = &pages[k];
    for (int t = 0; t <= PAGES; ++t)
        if (trees[t]->own_looks && look >= trees[t]->own_looks && look < trees[t]->own_looks + trees[t]->own_look_count) return look->params;
    const appearance_load_t *a = &appearance_load;
    for (int k = 0; a->loaded && k < a->appearance.look_count; ++k)
        if (a->appearance.looks[k].look == look) return a->appearance.looks[k].name;
    return "?";
}

/* a look of the program's own: a frame, and its name in it where there is room */
static bool draw_named(const dgx_ui_look_t *look, dgx_ui_paint_t *on, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    (void)now_ms;
    const dgx_ui_node_t *node = dgx_ui_node(ui, i);
    const int            x = node->x + on->ox, y = node->y + on->oy, w = node->w, h = node->h;
    if (w <= 0 || h <= 0) return false;
    const uint32_t ink = dgx_ui_color(on, 1), paper = dgx_ui_color(on, on->paper == DGX_UI_PAPER ? 0 : on->paper);
    dgx_fill_rectangle(on->scr, x, y, w, h, paper);
    dgx_draw_rectangle(on->scr, x, y, w, h, ink);
    const dgx_font_t *font = &TerminusTTFMedium12;
    if (w > 4 && h >= font->y_advance + 2) {
        const dgx_font_context_t text = {.scr = on->scr, .font = font, .color = ink, .bg = paper, .field = w - 4, .align = DGX_FONT_ALIGN_CENTER};
        dgx_font_draw(&text, x + 2, y + (h + font->y_advance) / 2 - 2, name_of_look(look));
    }
    return false;
}

/* ---- the format, as the loader's tables have it, for a page that edits descriptions ---- */

/* a key's value: what it is, its range for a number, the table a name of it is found in */
typedef struct key_type_t {
    const char *type;     /* kind, words, int, pair, name, text, id, children, editor */
    const char *words;    /* words: of the loader's */
    int32_t     min, max; /* int, pair: each number's; ctx: of its number, which a handler that takes one takes */
    const char *table;    /* name: the table of the names, as preview_tables() names it */
    bool        null;     /* name: null may be said, look none */
    bool        percent;  /* int, pair: a percent may be said too, "50%", from percent_min % to 127.99 % */
    int32_t     percent_min;
} key_type_t;

static const key_type_t key_types[KEY_COUNT] = {
    [K_KIND] = {.type = "kind"},
    [K_FLAGS] = {.type = "words", .words = flag_words},
    [K_STATE] = {.type = "words", .words = state_words},
    [K_PAD] = {.type = "int", .max = INT16_MAX, .percent = true},
    [K_GAP] = {.type = "int", .max = INT16_MAX, .percent = true},
    [K_AT] = {.type = "pair", .min = INT16_MIN, .max = INT16_MAX, .percent = true, .percent_min = -128},
    [K_WANT] = {.type = "pair", .min = INT16_MIN, .max = INT16_MAX, .percent = true, .percent_min = -128},
    [K_MIN] = {.type = "int", .min = INT16_MIN, .max = INT16_MAX},
    [K_MAX] = {.type = "int", .min = INT16_MIN, .max = INT16_MAX},
    [K_STEP] = {.type = "int", .min = INT16_MIN, .max = INT16_MAX},
    [K_TAG] = {.type = "int", .min = INT16_MIN, .max = INT16_MAX},
    [K_LOOK] = {.type = "name", .table = "look", .null = true},
    [K_VALUE] = {.type = "name", .table = "value"},
    [K_PICTURE] = {.type = "name", .table = "picture"},
    [K_STYLE] = {.type = "name", .table = "style"},
    [K_ON] = {.type = "name", .table = "on"},
    [K_TAKE] = {.type = "name", .table = "take"},
    [K_CTX] = {.type = "name", .min = INT32_MIN, .max = INT32_MAX, .table = "ctx"},
    [K_TEXT] = {.type = "text"},
    [K_TEXT_SIZE] = {.type = "int", .min = 1, .max = UINT16_MAX},
    [K_LINES] = {.type = "int", .min = 1, .max = 255},
    [K_ID] = {.type = "id"},
    [K_CHILDREN] = {.type = "children"},
    [K_EDITOR] = {.type = "editor"},
};

/* a text that grows */
typedef struct grown_t {
    char  *text;
    size_t used, room;
} grown_t;

static void grow(grown_t *to, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void grow(grown_t *to, const char *fmt, ...)
{
    for (;;) {
        va_list args;
        va_start(args, fmt);
        const int wrote = vsnprintf(to->text ? to->text + to->used : NULL, to->text ? to->room - to->used : 0, fmt, args);
        va_end(args);
        if (wrote < 0) return;
        if (to->text && to->used + (size_t)wrote < to->room) {
            to->used += (size_t)wrote;
            return;
        }
        to->room = (to->room + (size_t)wrote + 1) * 2;
        to->text = realloc(to->text, to->room);
        if (!to->text) names_no_memory();
    }
}

/* words of the loader's, each ended by a 0, as a list of JSON */
static void grow_words(grown_t *to, const char *words)
{
    grow(to, "[");
    for (const char *word = words; *word; word += strlen(word) + 1) grow(to, "%s\"%s\"", word == words ? "" : ", ", word);
    grow(to, "]");
}

const char *preview_format(void)
{
    static grown_t format;
    if (format.text) return format.text;
    grow(&format, "{\"version\": 1, \"kinds\": [");
    for (int k = 0; k < KIND_COUNT; ++k) grow(&format, "%s\"%s\"", k ? ", " : "", kinds[k]);
    grow(&format, "], \"keys\": [");
    for (int k = 0; k < KEY_COUNT; ++k) {
        const key_type_t *type = &key_types[k];
        grow(&format, "%s\n  {\"key\": \"%s\", \"type\": \"%s\", \"kinds\": [", k ? "," : "", keys[k], type->type);
        for (int kind = 0, first = 1; kind < KIND_COUNT; ++kind)
            if (key_kinds[k] & 1u << kind) grow(&format, "%s\"%s\"", first ? "" : ", ", kinds[kind]), first = 0;
        grow(&format, "]");
        if (type->words) grow(&format, ", \"words\": "), grow_words(&format, type->words);
        if (strcmp(type->type, "int") == 0 || strcmp(type->type, "pair") == 0 || type->max)
            grow(&format, ", \"min\": %ld, \"max\": %ld", (long)type->min, (long)type->max);
        if (type->table) grow(&format, ", \"table\": \"%s\"", type->table);
        if (type->null) grow(&format, ", \"null\": true");
        if (type->percent) grow(&format, ", \"percent\": {\"min\": %ld, \"max\": 127.99}", (long)type->percent_min);
        grow(&format, "}");
    }
    grow(&format, "]}");
    return format.text;
}

int preview_json(const char *text, size_t length)
{
    dgx_ui_json_t json = {.text = text, .len = length};
    if (dgx_ui_json_peek(&json) != '{') return 0;
    if (!dgx_ui_json_skip(&json, DGX_UI_JSON_LEVELS)) return json.error == DGX_UI_JSON_DEPTH ? 2 : 0;
    return dgx_ui_json_peek(&json) == -1 ? 1 : 0;
}

/* the names of the tables the page given is read on, as a JSON object of lists */
const char *preview_tables(void)
{
    static grown_t tables_said;
    tables_said.used = 0;
    grow(&tables_said, "{");
    const dgx_ui_load_names_t *names = &home.names;
    const struct {
        const char *table;
        const void *entries;
        size_t      size;
        int         count;
    } lists[] = {
        {"look", names->looks, sizeof *names->looks, names->look_count},
        {"value", names->values, sizeof *names->values, names->value_count},
        {"picture", names->pictures, sizeof *names->pictures, names->picture_count},
        {"style", names->styles, sizeof *names->styles, names->style_count},
        {"on", names->handlers, sizeof *names->handlers, names->handler_count},
        {"take", names->takes, sizeof *names->takes, names->take_count},
        {"ctx", names->contexts, sizeof *names->contexts, names->context_count},
        {"id", names->ids, sizeof *names->ids, names->id_count},
    };
    for (size_t t = 0; t < sizeof lists / sizeof *lists; ++t) {
        grow(&tables_said, "%s\"%s\": [", t ? ", " : "", lists[t].table);
        for (int k = 0, first = 1; k < lists[t].count; ++k) {
            /* every table's entry begins with its name */
            const char *name = *(const char *const *)((const uint8_t *)lists[t].entries + (size_t)k * lists[t].size);
            if (strcmp(name, DEFAULT_STYLE) == 0) continue; /* no description names the style of a text that names none */
            grow(&tables_said, "%s\"%s\"", first ? "" : ", ", name), first = 0;
        }
        grow(&tables_said, "]");
    }
    grow(&tables_said, "}");
    return tables_said.text;
}

/* ---- what an appearance may say, for its forms ---- */

static const char *const field_types[] = {"color", "color_or_under", "number", "bool", "word", "flags", "bit", "bands", "font", "hand", "variants",
                                          "picture"};
_Static_assert(sizeof field_types / sizeof *field_types == DGX_UI_FIELD_PICTURE + 1, "a type for every field");

/* a member of 1, 2 or 4 bytes as an integer, signed where its bounds go below 0 */
static int32_t member_read(const void *params, const dgx_ui_field_t *field)
{
    const uint8_t *at     = (const uint8_t *)params + field->offset;
    const bool     signed_ = field->low < 0;
    switch (field->size) {
    case 1: return signed_ ? *(const int8_t *)at : *at;
    case 2: return signed_ ? *(const int16_t *)(const void *)at : *(const uint16_t *)(const void *)at;
    default: return *(const int32_t *)(const void *)at;
    }
}

/* a colour of the library's defaults by its name: the painter's four by their place, DGX_UI_PAPER null */
static void grow_color(grown_t *to, unsigned index)
{
    static const char *const four[] = {"paper", "ink", "fill", "border"};
    if (index == DGX_UI_PAPER) grow(to, "null");
    else if (index < 4) grow(to, "\"%s\"", four[index]);
    else grow(to, "%u", index);
}

/* the fields of a description, and their values in params when it is given */
static void grow_fields(grown_t *to, const dgx_ui_params_t *described, const void *params)
{
    grow(to, "[");
    for (int k = 0; described && k < described->field_count; ++k) {
        const dgx_ui_field_t *field = &described->fields[k];
        grow(to, "%s{\"name\": \"%s\", \"type\": \"%s\"", k ? ", " : "", field->name, field_types[field->kind]);
        if (field->kind == DGX_UI_FIELD_NUMBER || field->kind == DGX_UI_FIELD_VARIANTS)
            grow(to, ", \"min\": %ld, \"max\": %ld", (long)field->low, (long)field->high);
        if (field->kind == DGX_UI_FIELD_PICTURE) grow(to, ", \"min\": %ld", (long)field->low); /* the pictures it needs */
        if (field->words) grow(to, ", \"words\": "), grow_words(to, field->words);
        grow(to, "}");
    }
    grow(to, "]");
    if (!params) return;
    grow(to, ", \"defaults\": {");
    for (int k = 0; k < described->field_count; ++k) {
        const dgx_ui_field_t *field = &described->fields[k];
        const int32_t         value = field->kind == DGX_UI_FIELD_BANDS || field->kind == DGX_UI_FIELD_PICTURE ? 0 : member_read(params, field);
        grow(to, "%s\"%s\": ", k ? ", " : "", field->name);
        switch (field->kind) {
        case DGX_UI_FIELD_COLOR:
        case DGX_UI_FIELD_COLOR_OR_UNDER: grow_color(to, (unsigned)value); break;
        case DGX_UI_FIELD_BOOL: grow(to, "%s", value ? "true" : "false"); break;
        case DGX_UI_FIELD_WORD: {
            const char *word = field->words;
            for (int at = 0; at < value && *word; ++at) word += strlen(word) + 1;
            grow(to, "\"%s\"", word);
            break;
        }
        case DGX_UI_FIELD_PICTURE: grow(to, "null"); break; /* the library's look names none: the program's to give */
        case DGX_UI_FIELD_BANDS: {
            const dgx_ui_band_t *bands = *(const dgx_ui_band_t *const *)(const void *)((const uint8_t *)params + field->offset);
            const int            count = *((const uint8_t *)params + field->count_offset);
            grow(to, "[");
            for (int band = 0; band < count; ++band) {
                grow(to, "%s[%ld, ", band ? ", " : "", (long)bands[band].below);
                grow_color(to, bands[band].color);
                grow(to, "]");
            }
            grow(to, "]");
            break;
        }
        default: grow(to, "%ld", (long)value);
        }
    }
    grow(to, "}");
}

/* the library's draw a function is, by its word; NULL else */
static const dgx_ui_draw_name_t *library_draw_by_function(bool (*draw)(const dgx_ui_look_t *, dgx_ui_paint_t *, dgx_ui_t *, int, uint32_t))
{
    for (size_t k = 0; k < sizeof library_draw_functions / sizeof *library_draw_functions; ++k)
        if (library_draw_functions[k].draw == draw) return &library_draw_functions[k];
    return NULL;
}

const char *preview_appearance_format(void)
{
    static grown_t said;
    said.used = 0;
    const appearance_load_t *a        = &appearance_load;
    const bool               laid     = a->draws != NULL;
    const int                draws    = laid ? a->draw_count : (int)(sizeof library_draw_functions / sizeof *library_draw_functions);
    grow(&said, "{\"draws\": [");
    for (int k = 0; k < draws; ++k) {
        const dgx_ui_draw_name_t *draw    = laid ? &a->draws[k] : &library_draw_functions[k];
        const dgx_ui_draw_name_t *library = draw->params ? library_draw_by_function(draw->draw) : NULL;
        const dgx_ui_look_t      *look    = NULL;
        if (library)
            for (size_t l = 0; l < sizeof library_look_objects / sizeof *library_look_objects; ++l)
                if (strcmp(library_look_objects[l].name, library->name) == 0) look = library_look_objects[l].look;
        grow(&said, "%s\n  {\"draw\": \"%s\", \"own\": %s, \"kinds\": [", k ? "," : "", draw->name, library ? "false" : "true");
        for (int kind = 0, first = 1; library && kind < KIND_COUNT; ++kind)
            if (draw->params->kinds & 1u << kind) grow(&said, "%s\"%s\"", first ? "" : ", ", kinds[kind]), first = 0;
        grow(&said, "], \"fields\": ");
        grow_fields(&said, library ? draw->params : NULL, look ? look->params : NULL);
        grow(&said, "}");
    }
    grow(&said, "],\n \"fonts\": [");
    for (int k = 0; laid && k < a->font_count; ++k) grow(&said, "%s\"%s\"", k ? ", " : "", a->fonts[k].name);
    grow(&said, "], \"hands\": [");
    for (int k = 0; k < (laid ? a->hand_count : 2); ++k) grow(&said, "%s\"%s\"", k ? ", " : "", laid ? a->hands[k].name : library_hands[k]);
    grow(&said, "], \"pictures\": [");
    for (int k = 0; laid && k < a->picture_count; ++k)
        grow(&said, "%s{\"name\": \"%s\", \"count\": %u}", k ? ", " : "", a->pictures[k].name, a->pictures[k].count > 1 ? a->pictures[k].count : 1u);
    grow(&said, "], \"by_rule\": %s,\n \"style\": ", appearance_named() ? "false" : "true");
    grow_fields(&said, &dgx_ui_params_text_style, NULL);
    grow(&said, ",\n \"variant\": ");
    grow_fields(&said, &dgx_ui_params_font_variant, NULL);
    grow(&said, "}");
    return said.text;
}

/* ---- the calls ---- */

static char             *text_room;
static size_t            text_room_size;
static preview_verdict_t names_verdict;
static char             *page_text;
static size_t            page_length;
static bool              page_given;
static int32_t           changed[4];
static preview_widget_t  widget_read;

const char *preview_version(void)
{
    return "{\"dgx_ui\": \"" PREVIEW_DGX_UI_VERSION "\", \"dgx\": \"" PREVIEW_DGX_VERSION "\", \"toolchain\": \"" PREVIEW_TOOLCHAIN
           "\", \"sources\": \"" PREVIEW_SOURCES "\"}";
}

char *preview_text(size_t size)
{
    if (size > text_room_size) {
        free(text_room);
        text_room      = zeroed(1, size);
        text_room_size = size;
    }
    return text_room;
}

static char *copy_text(const char *text, size_t length)
{
    char *made = zeroed(1, length + 1);
    memcpy(made, text, length);
    return made;
}

/* the colours: the appearance's, else four of the library's own looks */
static void colors_packed(void)
{
    free(colors);
    colors = NULL, color_count = 0;
    if (!panel) return;
    dgx_screen_t *scr = &panel->sbus.scr;
    if (appearance_load.loaded) {
        color_count = appearance_load.appearance.color_count;
        colors      = zeroed(color_count, sizeof *colors);
        dgx_ui_appearance_pack(&appearance_load.appearance, dgx_ui_pack_screen, scr, colors);
    } else {
        color_count = 4;
        colors      = zeroed(color_count, sizeof *colors);
        colors[0] = dgx_screen_rgb(scr, 16, 20, 28), colors[1] = dgx_screen_rgb(scr, 230, 230, 220), colors[2] = dgx_screen_rgb(scr, 32, 48, 72),
        colors[3] = dgx_screen_rgb(scr, 120, 160, 220);
    }
}

/* everything read again: the appearance, the page, the colours; the screen to be drawn whole */
static void read_again(void)
{
    if (paint.scr) dgx_ui_paint_free(&paint);
    paint = (dgx_ui_paint_t){0};
    for (int k = 0; k < PAGES; ++k) tree_free(&pages[k]);
    tree_free(&home);
    said_clear(&notes);
    appearance_read();
    colors_packed();
    page_verdict = (preview_verdict_t){0};
    if (page_given) tree_load(&home, page_text, page_length, &page_verdict);
    if (panel) {
        paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = color_count, .background = 0};
        memset(panel->rgba, 0, (size_t)panel->sbus.scr.width * (size_t)panel->sbus.scr.height * 4);
    }
}

const preview_verdict_t *preview_font(const char *name, const void *bytes, size_t length)
{
    static preview_verdict_t verdict;
    verdict = (preview_verdict_t){0};
    const size_t name_length = name ? strlen(name) : 0;
    if (name_length == 0 || name_length > PREVIEW_FONT_NAME_MOST) {
        verdict.code = PREVIEW_FONT_REFUSED;
        snprintf(verdict.said, sizeof verdict.said, "a name of C of %zu bytes: 1 to %d are taken", name_length, PREVIEW_FONT_NAME_MOST);
        return &verdict;
    }
    int found = -1;
    for (int k = 0; k < font_file_count && found < 0; ++k)
        if (strcmp(font_files[k].name, name) == 0) found = k;
    font_file_t given = {0};
    if (bytes) {
        given.bytes = zeroed(1, length);
        memcpy(given.bytes, bytes, length);
        size_t    need = 0;
        esp_err_t said = dgx_font_file_memory(given.bytes, length, &need);
        if (said == ESP_OK) {
            given.memory = zeroed(1, need);
            said         = dgx_font_file_load(given.bytes, length, given.memory, need, &given.font);
        }
        if (said != ESP_OK) {
            free(given.bytes), free(given.memory);
            verdict.code = PREVIEW_FONT_REFUSED;
            snprintf(verdict.said, sizeof verdict.said, "%s: %s", name,
                     said == ESP_ERR_INVALID_VERSION ? "ESP_ERR_INVALID_VERSION, a file of a later format"
                     : said == ESP_ERR_NOT_SUPPORTED ? "ESP_ERR_NOT_SUPPORTED, a file of the other byte order"
                                                     : "ESP_ERR_INVALID_ARG, not a font, or spoiled or cut");
            return &verdict;
        }
        memcpy(given.name, name, name_length + 1);
    }
    /* the new file in its place, the trees and the appearance laid on it, and only then the old one freed */
    font_file_t old = found >= 0 ? font_files[found] : (font_file_t){0};
    if (found >= 0 && bytes) {
        font_files[found] = given;
    } else if (found >= 0) {
        font_files[found] = font_files[--font_file_count];
    } else if (bytes) {
        if (font_file_count == font_file_room) {
            font_file_room = font_file_room ? 2 * font_file_room : 8;
            font_files     = realloc(font_files, (size_t)font_file_room * sizeof *font_files);
            if (!font_files) names_no_memory();
        }
        font_files[font_file_count++] = given;
    }
    read_again();
    free(old.bytes), free(old.memory);
    return &verdict;
}

int preview_screen(int width, int height, int depth, int pixels_a_point)
{
    panel_free();
    if (width < 1 || height < 1 || width > 4096 || height > 4096 || (depth != 16 && depth != 18 && depth != 24) || pixels_a_point < 1 ||
        pixels_a_point > 16)
        return PREVIEW_NO_SCREEN;
    unit              = pixels_a_point;
    panel_t *p        = zeroed(1, sizeof *p);
    p->rgba           = zeroed((size_t)width * (size_t)height, 4);
    p->bytes          = depth == 16 ? 2 : 3;
    p->bus.buffer     = p->bus_buffer, p->bus.buffer_len = sizeof p->bus_buffer;
    p->sbus.bus       = &p->bus;
    dgx_screen_t *scr = &p->sbus.scr;
    scr->width = (int16_t)width, scr->height = (int16_t)height, scr->color_bits = (uint8_t)depth, scr->screen_subtype = DgxPhysicalScreenWithBus;
    dgx_scr_init_slow_bus_optimized_funcs(scr);
    scr->set_area = panel_set_area, scr->write_area = panel_write_area, scr->wait = panel_wait, scr->update_screen = panel_update;
    panel_nothing_changed(p);
    panel = p;
    read_again();
    return 0;
}

const preview_verdict_t *preview_names(const char *text, size_t length)
{
    free(names_text);
    names_text = NULL, names_length = 0, names_given = text != NULL;
    names_verdict = (preview_verdict_t){0};
    if (names_given) {
        names_text = copy_text(text, length), names_length = length;
        names_free_all();
        char *said = names_read("names", names_text, names_length, false);
        names_free_all();
        if (said) {
            names_verdict.code = PREVIEW_NAMES_REFUSED;
            snprintf(names_verdict.said, sizeof names_verdict.said, "%s", said);
            free(said);
            free(names_text);
            names_text = NULL, names_length = 0, names_given = false;
        }
    }
    read_again();
    return &names_verdict;
}

const preview_verdict_t *preview_appearance(const char *text, size_t length)
{
    free(appearance_load.text);
    appearance_load.text = text ? copy_text(text, length) : NULL, appearance_load.length = text ? length : 0;
    read_again();
    return &appearance_load.verdict;
}

const preview_verdict_t *preview_appearance_on(int how, const char *text, size_t length)
{
    static preview_verdict_t verdict;
    verdict = (preview_verdict_t){0};
    free(appearance_names_text);
    appearance_names_text = NULL, appearance_names_length = 0;
    appearance_on = how == PREVIEW_ON_OWN ? (text ? PREVIEW_ON_OWN : PREVIEW_ON_RULE) : how == PREVIEW_ON_RULE ? PREVIEW_ON_RULE : PREVIEW_ON_PAGE;
    if (appearance_on == PREVIEW_ON_OWN) {
        appearance_names_text = copy_text(text, length), appearance_names_length = length;
        names_free_all();
        char *said = names_read("appearance's names", appearance_names_text, appearance_names_length, false);
        names_free_all();
        if (said) {
            verdict.code = PREVIEW_NAMES_REFUSED;
            snprintf(verdict.said, sizeof verdict.said, "%s", said);
            free(said);
            free(appearance_names_text);
            appearance_names_text = NULL, appearance_names_length = 0, appearance_on = PREVIEW_ON_RULE;
        }
    }
    read_again();
    return &verdict;
}

const preview_verdict_t *preview_page(const char *text, size_t length)
{
    free(page_text);
    page_text = text ? copy_text(text, length) : NULL, page_length = text ? length : 0, page_given = text != NULL;
    read_again();
    return &page_verdict;
}

void preview_pointer(int x, int y, int down, uint32_t now_ms)
{
    static bool was_down;
    if (!home.loaded) return;
    const dgx_ui_pointer_t pointer = {
        .x = (int16_t)x, .y = (int16_t)y, .down = down != 0, .pressed = down && !was_down, .released = !down && was_down, .ms = now_ms};
    was_down = down != 0;
    dgx_ui_pointer(&home.ui, &pointer);
}

void preview_key(int key)
{
    if (home.loaded) dgx_ui_key(&home.ui, key);
}

void preview_buttons(uint32_t held, uint32_t now_ms)
{
    if (home.loaded) dgx_ui_buttons(&home.ui, held, now_ms);
}

/* the value written where the names keep it, in every tree, as the program writes its own: the painter draws anew
   each widget whose value differs from what it drew */
int preview_value(const char *name, int32_t value)
{
    bool    found            = false;
    tree_t *trees[PAGES + 1] = {&home};
    for (int k = 0; k < PAGES; ++k) trees[k + 1] = &pages[k];
    for (int t = 0; t <= PAGES; ++t)
        for (int k = 0; trees[t]->loaded && k < trees[t]->names.value_count; ++k)
            if (strcmp(trees[t]->names.values[k].name, name) == 0) trees[t]->values[k] = value, found = true;
    return found ? 0 : -1;
}

int preview_paint(uint32_t now_ms)
{
    if (!panel || !home.loaded) return 0;
    dgx_ui_time(&home.ui, now_ms);
    const uint32_t before = panel->written;
    dgx_ui_paint(&home.ui, &paint, now_ms);
    return panel->written != before;
}

uint32_t preview_wake(uint32_t now_ms)
{
    if (!panel || !home.loaded) return UINT32_MAX;
    const uint32_t ms = dgx_ui_wake(&paint, &home.ui, now_ms);
    return ms == DGX_UI_WAKE_NONE ? UINT32_MAX : ms;
}

const uint8_t *preview_frame(void)
{
    return panel ? panel->rgba : NULL;
}

const int32_t *preview_changed(void)
{
    changed[0] = changed[1] = changed[2] = changed[3] = 0;
    if (panel && panel->changed_right >= panel->changed_left) {
        changed[0] = panel->changed_left, changed[1] = panel->changed_top;
        changed[2] = panel->changed_right - panel->changed_left + 1, changed[3] = panel->changed_bottom - panel->changed_top + 1;
    }
    if (panel) panel_nothing_changed(panel);
    return changed;
}

/* the tree on the screen: the topmost of home and the pages in the chain of popups over home; NULL with no page */
static tree_t *shown_tree(void)
{
    tree_t *shown = home.loaded ? &home : NULL;
    for (const dgx_ui_t *over = shown ? home.ui.popup : NULL; over; over = over->popup)
        for (int k = 0; k < PAGES; ++k)
            if (pages[k].loaded && &pages[k].ui == over) shown = &pages[k];
    return shown;
}

const char *preview_shown(void)
{
    const tree_t *shown = shown_tree();
    return shown ? shown->name : NULL;
}

int preview_count(void)
{
    const tree_t *shown = shown_tree();
    return shown ? shown->ui.count : 0;
}

const preview_widget_t *preview_widget(int i)
{
    tree_t *shown = shown_tree();
    if (!shown || i < 0 || i >= shown->ui.count) return NULL;
    dgx_ui_t              *ui     = &shown->ui;
    const dgx_ui_node_t   *node   = dgx_ui_node(ui, i);
    const dgx_ui_widget_t *widget = node->widget;
    if (!widget) return NULL;
    /* shown: no hidden one on the way to the root, and a box left inside each parent's */
    int  left = node->x, top = node->y, right = node->x + node->w, bottom = node->y + node->h;
    bool laid  = !(node->state.bits & DGX_UI_HIDDEN);
    for (int up = node->parent; up >= 0 && laid; up = dgx_ui_node(ui, up)->parent) {
        const dgx_ui_node_t *parent = dgx_ui_node(ui, up);
        if (parent->state.bits & DGX_UI_HIDDEN) laid = false;
        if (parent->x > left) left = parent->x;
        if (parent->y > top) top = parent->y;
        if (parent->x + parent->w < right) right = parent->x + parent->w;
        if (parent->y + parent->h < bottom) bottom = parent->y + parent->h;
    }
    widget_read = (preview_widget_t){.kind = widget->kind, .parent = node->parent, .x = node->x, .y = node->y, .w = node->w, .h = node->h,
                                     .shown = laid && right > left && bottom > top, .bits = node->state.bits, .value = dgx_ui_value(ui, i),
                                     .text = widget->text ? widget->text->buf : NULL};
    return &widget_read;
}

int preview_widget_at(int x, int y)
{
    tree_t *shown = shown_tree();
    return shown ? dgx_ui_hit(&shown->ui, x, y) : -1;
}

const char *preview_events(void)
{
    static char out[SAID_ROOM];
    said_read(&events, out);
    said_clear(&events);
    return out;
}

const char *preview_notes(void)
{
    static char out[SAID_ROOM];
    return said_read(&notes, out);
}

/* ---- the names file, for a page that edits it by forms ---- */

/* a string of JSON of a name or an expression of a names file: no '"', '\\' nor control there, as the reader splits
   them, but each is written escaped all the same */
static void grow_string(grown_t *to, const char *text)
{
    grow(to, "\"");
    for (const unsigned char *at = (const unsigned char *)text; *at; ++at) {
        if (*at == '"' || *at == '\\') grow(to, "\\%c", *at);
        else if (*at < 0x20) grow(to, "\\u%04x", *at);
        else grow(to, "%c", *at);
    }
    grow(to, "\"");
}

const char *preview_names_format(void)
{
    static grown_t said;
    if (said.text) return said.text;
    grow(&said, "{\"tables\": [");
    for (int t = 0; t < TABLES; ++t) grow(&said, "%s\"%s\"", t ? ", " : "", table_words[t]);
    grow(&said, "], \"keys\": [");
    for (int t = 0; t < TABLES; ++t) grow(&said, "%s\"%s\"", t ? ", " : "", table_keys[t]);
    grow(&said, "], \"appearance\": [");
    for (int t = 0; t < A_PICTURE; ++t) grow(&said, "%s\"%s\"", t ? ", " : "", appearance_words[t]); /* pictures: the page's table's lines */
    grow(&said, "],\n \"words\": {\"picture\": \"count\", \"on\": \"number\", \"ctx\": \"page\", \"font\": \"handwritten\"}, \"pictures\": [1, 65535],\n");
    grow(&said, " \"bare\": {\"look\": [");
    for (int k = 0; k < library_look_count; ++k) grow(&said, "%s\"%s\"", k ? ", " : "", library_looks[k]);
    grow(&said, "], \"on\": [");
    static const char *const actions[] = {"back", "page"};
    for (int k = 0, first = 1; k < 2; ++k) {
        char *c = library_c(T_ON, actions[k]);
        if (c) grow(&said, "%s\"%s\"", first ? "" : ", ", actions[k]), first = 0;
        free(c);
    }
    grow(&said, "], \"draw\": [");
    for (int k = 0; k < library_draw_count; ++k) grow(&said, "%s\"%s\"", k ? ", " : "", library_draws[k].word);
    grow(&said, "], \"hand\": [\"%s\", \"%s\"]}}", library_hands[0], library_hands[1]);
    return said.text;
}

/* the looks a page's widgets take by their kind's word, naming none: a widget's object, then its children's; stopped at
   what is not as it should be, which the loader then refuses */
static void collect_kind_looks(dgx_ui_json_t *json, int depth, table_t *out, bool *unstyled)
{
    if (depth > DGX_UI_LOAD_DEPTH || dgx_ui_json_peek(json) != '{') return;
    ++json->pos;
    int  kind = -1;
    bool named = false, texted = false, styled = false;
    if (dgx_ui_json_peek(json) == '}') {
        ++json->pos;
        return;
    }
    for (bool more = true; more;) {
        char   key[24], word[24];
        size_t length;
        if (!dgx_ui_json_string(json, key, 23, &length) || !dgx_ui_json_expect(json, ':')) return;
        key[length < 23 ? length : 23] = 0;
        if (strcmp(key, "kind") == 0 && dgx_ui_json_peek(json) == '"') {
            if (!dgx_ui_json_string(json, word, 23, &length)) return;
            word[length < 23 ? length : 23] = 0;
            for (int k = 0; k < KIND_COUNT; ++k)
                if (strcmp(kinds[k], word) == 0) kind = k;
        } else if (strcmp(key, "children") == 0 && dgx_ui_json_peek(json) == '[') {
            ++json->pos;
            if (dgx_ui_json_peek(json) == ']') ++json->pos;
            else
                for (bool child = true; child;) {
                    collect_kind_looks(json, depth + 1, out, unstyled);
                    if (json->error || !dgx_ui_json_next(json, ']', &child)) return;
                }
        } else {
            named |= strcmp(key, "look") == 0, styled |= strcmp(key, "style") == 0;
            texted |= strcmp(key, "text") == 0 || strcmp(key, "text_size") == 0;
            if (!dgx_ui_json_skip(json, 2 * DGX_UI_LOAD_DEPTH)) return;
        }
        if (!dgx_ui_json_next(json, '}', &more)) return;
    }
    if (kind >= 0 && !named && !(OWN_LOOKS & 1u << kind) && !entry_of(out, kinds[kind])) entry_add(out, kinds[kind]);
    *unstyled |= texted && !styled;
}

static void collect_page_kind_looks(const char *text, size_t length, table_t *out, bool *unstyled)
{
    dgx_ui_json_t json = {.text = text, .len = length};
    if (dgx_ui_json_peek(&json) != '{') return;
    ++json.pos;
    if (dgx_ui_json_peek(&json) == '}') return;
    for (bool more = true; more;) {
        char   key[24];
        size_t key_length;
        if (!dgx_ui_json_string(&json, key, 23, &key_length) || !dgx_ui_json_expect(&json, ':')) return;
        key[key_length < 23 ? key_length : 23] = 0;
        if (strcmp(key, "tree") == 0) collect_kind_looks(&json, 1, out, unstyled);
        else if (!dgx_ui_json_skip(&json, 2 * DGX_UI_LOAD_DEPTH + 1)) return;
        if (json.error || !dgx_ui_json_next(&json, '}', &more)) return;
    }
}

const char *preview_names_missing(const char *text, size_t length)
{
    static grown_t said;
    said.used = 0;
    grow(&said, "[");
    names_free_all();
    free(names_read("names", text ? text : "", text ? length : 0, false));
    if (page_given && page_text) names_collect(page_text, page_length);
    /* a widget that names no look takes the one of its kind's word: wanted of the file as a look the page names */
    table_t by_kind  = {0};
    bool    unstyled = false;
    if (page_given && page_text) collect_page_kind_looks(page_text, page_length, &by_kind, &unstyled);
    for (int k = 0; k < by_kind.count; ++k)
        if (!entry_of(&used[T_LOOK], by_kind.entries[k].name)) entry_add(&used[T_LOOK], by_kind.entries[k].name)->offset = -1;
    free(by_kind.entries);
    /* the appearance's looks and styles stand first in the page's tables: none of them is wanted of the file */
    const dgx_ui_appearance_t *shown = appearance_load.loaded ? &appearance_load.appearance : NULL;
    /* a text that names no style takes the first: with none of the file's nor the appearance's, and the page naming
       none, one is wanted, "body" as the documents name it */
    if (unstyled && !tables[T_STYLE].count && !used[T_STYLE].count && !(shown && shown->style_count)) entry_add(&used[T_STYLE], "body")->offset = -1;
    int first = 1;
    for (int t = 0; t < TABLES; ++t)
        for (int k = 0; k < used[t].count; ++k) {
            const entry_t *use = &used[t].entries[k];
            if (strcmp(use->name, DEFAULT_STYLE) == 0 || entry_of(&tables[t], use->name)) continue;
            bool of_appearance = false;
            for (int a = 0; shown && t == T_LOOK && a < shown->look_count; ++a) of_appearance |= strcmp(shown->looks[a].name, use->name) == 0;
            for (int a = 0; shown && t == T_STYLE && a < shown->style_count; ++a) of_appearance |= strcmp(shown->styles[a].name, use->name) == 0;
            if (of_appearance) continue;
            /* the C the rule gives it, as names_rule() does; none for a name the rule cannot make C */
            char *library = library_c(t, use->name), *c = NULL;
            bool  number = false, page = false;
            unsigned pictures = 0;
            if (library) {
                c = NULL;
            } else if (c_name(use->name, t != T_ID)) {
                switch (t) {
                case T_PICTURE: c = copy_of(use->name), pictures = 65535; break;
                case T_ON: c = copy_of(use->name), number = true; break;
                case T_TAKE: c = copy_of(use->name); break;
                case T_CTX: c = printed("&%s", use->name), page = true; break;
                case T_ID: c = copy_of(use->name); break;
                default: c = printed("&%s", use->name); break;
                }
            }
            grow(&said, "%s\n {\"table\": \"%s\", \"name\": ", first ? "" : ",", table_words[t]), first = 0;
            grow_string(&said, use->name);
            grow(&said, ", \"c\": ");
            if (c) grow_string(&said, c);
            else grow(&said, "null");
            grow(&said, ", \"bare\": %s", library ? "true" : "false");
            if (number) grow(&said, ", \"number\": true");
            if (page) grow(&said, ", \"page\": true");
            if (pictures) grow(&said, ", \"count\": %u", pictures);
            grow(&said, "}");
            free(library), free(c);
        }
    grow(&said, "]");
    names_free_all();
    return said.text;
}
