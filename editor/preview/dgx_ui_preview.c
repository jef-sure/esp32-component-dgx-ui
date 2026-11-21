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
#include "dgx_image.h"
#include "dgx_screen_functions.h"
#include "dgx_screen_with_bus.h"
#include "dgx_ui.h"
#include "dgx_ui_appearance.h"
#include "dgx_ui_load.h"
#include "dgx_ui_names.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/DroidSansRegular9.h"
#include "fonts/LiberationSansRegular12.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"

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

typedef struct said_t {
    char   text[4096];
    size_t used;
} said_t;

static said_t events, notes;

static void say(said_t *to, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void say(said_t *to, const char *fmt, ...)
{
    if (to->used >= sizeof to->text - 1) return;
    va_list args;
    va_start(args, fmt);
    const int wrote = vsnprintf(to->text + to->used, sizeof to->text - to->used, fmt, args);
    va_end(args);
    if (wrote > 0) to->used = to->used + (size_t)wrote < sizeof to->text - 1 ? to->used + (size_t)wrote : sizeof to->text - 1;
}

static void said_clear(said_t *said)
{
    said->used = 0, said->text[0] = 0;
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

/* ---- the stand-ins that are the same for every page ---- */

/* a style of the program's own */
static const dgx_ui_font_variant_t stand_variant[1] = {{.font = &TerminusTTFMedium12}};
static const dgx_ui_text_style_t   stand_style      = {.variants = stand_variant, .count = 1};

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
    char                    (*names)[24]; /* the names of its draws, fonts and hands, kept */
} appearance_load_t;

static appearance_load_t appearance_load;
static char             *names_text;
static size_t            names_length;
static bool              names_given;

static void names_free_all(void)
{
    table_t *every[] = {&tables[0], &tables[1], &tables[2], &tables[3], &tables[4], &tables[5], &tables[6], &tables[7],
                        &used[0], &used[1], &used[2], &used[3], &used[4], &used[5], &used[6], &used[7],
                        &appearance_tables[0], &appearance_tables[1], &appearance_tables[2]};
    _Static_assert(TABLES == 8 && APPEARANCE_TABLES == 3, "every table above");
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

static bool draw_named(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms);

static void appearance_free(void)
{
    appearance_load_t *a = &appearance_load;
    free(a->memory), free(a->draws), free(a->fonts), free(a->hands), free(a->names);
    a->memory = NULL, a->draws = NULL, a->fonts = NULL, a->hands = NULL, a->names = NULL, a->loaded = false;
}

/* the appearance loaded on its names, as dgx_ui_c -a reads it: no look or style of the program's after its own */
static void appearance_read(void)
{
    appearance_load_t *a = &appearance_load;
    appearance_free();
    a->verdict = (preview_verdict_t){0};
    if (!a->text) return;
    names_free_all();
    if (names_given) {
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
    const int      count    = draws_of->count + fonts_of->count + hands_of->count;
    a->names                = zeroed((size_t)count, sizeof *a->names);
    a->draws                = zeroed((size_t)draws_of->count, sizeof *a->draws);
    a->fonts                = zeroed((size_t)fonts_of->count, sizeof *a->fonts);
    a->hands                = zeroed((size_t)hands_of->count, sizeof *a->hands);
    int named               = 0;
    for (int k = 0; k < draws_of->count; ++k) {
        const entry_t            *draw    = &draws_of->entries[k];
        const dgx_ui_draw_name_t *library = library_draw_function_of(draw->c);
        memcpy(a->names[named], draw->name, 24);
        a->draws[k] = library ? (dgx_ui_draw_name_t){a->names[named], library->draw, library->params} : (dgx_ui_draw_name_t){a->names[named], draw_named, NULL};
        if (!library) say(&notes, "draw %s: the program's own, a frame with the look's name\n", draw->name);
        ++named;
    }
    for (int k = 0; k < fonts_of->count; ++k) {
        const entry_t    *font  = &fonts_of->entries[k];
        const dgx_font_t *built = built_in_font(font->c);
        memcpy(a->names[named], font->name, 24);
        a->fonts[k] = (dgx_ui_font_name_t){a->names[named], built ? built : font->handwritten ? &font0ss : &TerminusTTFMedium12};
        if (!built) say(&notes, "font %s: %s stands in, its measures not the board's\n", font->name, font->handwritten ? "font0ss" : "TerminusTTFMedium12");
        ++named;
    }
    for (int k = 0; k < hands_of->count; ++k) {
        const entry_t *hand = &hands_of->entries[k];
        memcpy(a->names[named], hand->name, 24);
        a->hands[k] = (dgx_ui_hand_name_t){a->names[named], strcmp(hand->c, "&dgx_ui_handwriting_canvas") == 0 ? &dgx_ui_handwriting_canvas : &dgx_ui_handwriting};
        ++named;
    }
    const dgx_ui_appearance_names_t names = {.draws = a->draws, .fonts = a->fonts, .hands = a->hands, .draw_count = (uint16_t)draws_of->count,
                                             .font_count = (uint16_t)fonts_of->count, .hand_count = (uint16_t)hands_of->count};
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

static const char *tree_name_of(const dgx_ui_t *ui)
{
    for (int k = 0; k < PAGES; ++k)
        if (pages[k].loaded && &pages[k].ui == ui) return pages[k].name;
    return "home";
}

static const char *const what_words[] = {"ACTIVATED", "CHANGED", "FOCUS", "BACK", "NO_MEMORY", "EDIT", "SHOWN", "COVERED"};
_Static_assert(DGX_UI_ACTIVATED == 0 && DGX_UI_COVERED == 7, "the words of what a handler is told, in its order");

static void tell(const dgx_ui_t *ui, int i, int what)
{
    say(&events, "%s %d %s\n", tree_name_of(ui), i, what >= 0 && what <= DGX_UI_COVERED ? what_words[what] : "?");
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
        say(&events, "page %s: no room for more than %d pages\n", name, PAGES);
        return;
    }
    if (!page->loaded) {
        char        *text   = NULL;
        const size_t length = preview_page_text(name, NULL, 0);
        if (length) text = zeroed(1, length), preview_page_text(name, text, length);
        preview_verdict_t verdict;
        const bool        taken = text && tree_load(page, text, length, &verdict);
        free(text);
        if (!taken) {
            if (!length) say(&events, "page %s: no text\n", name);
            else say(&events, "page %s: refused, %d at byte %d \"%s\"\n", name, (int)verdict.code, (int)verdict.offset, verdict.name);
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
            say(&notes, "look %s: the program's own, a frame with its name\n", name);
        }
    }
    tree->values               = zeroed((size_t)tables[T_VALUE].count, sizeof *tree->values);
    dgx_ui_value_name_t *values = zeroed((size_t)tables[T_VALUE].count, sizeof *values);
    for (int k = 0; k < tables[T_VALUE].count; ++k) values[k] = (dgx_ui_value_name_t){NAME_OF(T_VALUE, k), &tree->values[k]};
    dgx_ui_picture_name_t *pictures = zeroed((size_t)tables[T_PICTURE].count, sizeof *pictures);
    for (int k = 0; k < tables[T_PICTURE].count; ++k) {
        pictures[k] = (dgx_ui_picture_name_t){NAME_OF(T_PICTURE, k), crossed_picture(), (uint16_t)tables[T_PICTURE].entries[k].count};
        say(&notes, "picture %s: a crossed square stands in\n", pictures[k].name);
    }
    dgx_ui_style_name_t *styles = zeroed((size_t)tables[T_STYLE].count, sizeof *styles);
    for (int k = 0, appearance_k = 0; k < tables[T_STYLE].count; ++k) {
        const entry_t *entry = &tables[T_STYLE].entries[k];
        const char    *name  = NAME_OF(T_STYLE, k);
        styles[k] = (dgx_ui_style_name_t){name, entry->from_appearance ? a->appearance.styles[appearance_k++].style : &stand_style};
        if (!entry->from_appearance) say(&notes, "style %s: the program's own, TerminusTTFMedium12 stands in\n", strcmp(name, DEFAULT_STYLE) == 0 ? "of a text that names none" : name);
    }
    dgx_ui_on_name_t *handlers = zeroed((size_t)tables[T_ON].count, sizeof *handlers);
    for (int k = 0; k < tables[T_ON].count; ++k) {
        const entry_t *entry = &tables[T_ON].entries[k];
        const char    *c     = entry->c ? entry->c : "";
        handlers[k] = (dgx_ui_on_name_t){NAME_OF(T_ON, k), strcmp(c, "dgx_ui_load_on_back") == 0 ? on_back : strcmp(c, "dgx_ui_load_on_page") == 0 ? on_page : on_told,
                                         entry->number};
    }
    dgx_ui_take_name_t *takes = zeroed((size_t)tables[T_TAKE].count, sizeof *takes);
    for (int k = 0; k < tables[T_TAKE].count; ++k) takes[k] = (dgx_ui_take_name_t){NAME_OF(T_TAKE, k), take_nothing};
    tree->contexts                  = zeroed((size_t)tables[T_CTX].count, 1);
    dgx_ui_context_name_t *contexts = zeroed((size_t)tables[T_CTX].count, sizeof *contexts);
    for (int k = 0; k < tables[T_CTX].count; ++k) contexts[k] = (dgx_ui_context_name_t){NAME_OF(T_CTX, k), &tree->contexts[k]};
    tree->ids        = zeroed((size_t)tables[T_ID].count, sizeof *tree->ids);
    dgx_ui_id_t *ids = zeroed((size_t)tables[T_ID].count, sizeof *ids);
    for (int k = 0; k < tables[T_ID].count; ++k) ids[k] = (dgx_ui_id_t){NAME_OF(T_ID, k), &tree->ids[k]}, tree->ids[k] = -1;
#undef NAME_OF
    tree->names = (dgx_ui_load_names_t){.looks = looks, .values = values, .pictures = pictures, .styles = styles, .handlers = handlers, .takes = takes,
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
    dgx_ui_init(&tree->ui, tree->nodes, need.nodes, 1);
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
    return "{\"dgx_ui\": \"" PREVIEW_DGX_UI_VERSION "\", \"dgx\": \"" PREVIEW_DGX_VERSION "\", \"toolchain\": \"" PREVIEW_TOOLCHAIN "\", \"sources\": \"" PREVIEW_SOURCES
           "\"}";
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

int preview_screen(int width, int height, int depth)
{
    panel_free();
    if (width < 1 || height < 1 || width > 4096 || height > 4096 || (depth != 16 && depth != 18 && depth != 24)) return PREVIEW_NO_SCREEN;
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
    const dgx_ui_pointer_t pointer = {.x = (int16_t)x, .y = (int16_t)y, .down = down != 0, .pressed = down && !was_down, .released = !down && was_down, .ms = now_ms};
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

int preview_count(void)
{
    return home.loaded ? home.ui.count : 0;
}

const preview_widget_t *preview_widget(int i)
{
    if (!home.loaded || i < 0 || i >= home.ui.count) return NULL;
    const dgx_ui_node_t   *node   = dgx_ui_node(&home.ui, i);
    const dgx_ui_widget_t *widget = node->widget;
    if (!widget) return NULL;
    /* shown: no hidden one on the way to the root, and a box left inside each parent's */
    int  left = node->x, top = node->y, right = node->x + node->w, bottom = node->y + node->h;
    bool shown = !(node->state.bits & DGX_UI_HIDDEN);
    for (int up = node->parent; up >= 0 && shown; up = dgx_ui_node(&home.ui, up)->parent) {
        const dgx_ui_node_t *parent = dgx_ui_node(&home.ui, up);
        if (parent->state.bits & DGX_UI_HIDDEN) shown = false;
        if (parent->x > left) left = parent->x;
        if (parent->y > top) top = parent->y;
        if (parent->x + parent->w < right) right = parent->x + parent->w;
        if (parent->y + parent->h < bottom) bottom = parent->y + parent->h;
    }
    widget_read = (preview_widget_t){.kind = widget->kind, .parent = node->parent, .x = node->x, .y = node->y, .w = node->w, .h = node->h,
                                     .shown = shown && right > left && bottom > top, .bits = node->state.bits, .value = dgx_ui_value(&home.ui, i),
                                     .text = widget->text ? widget->text->buf : NULL};
    return &widget_read;
}

int preview_widget_at(int x, int y)
{
    return home.loaded ? dgx_ui_hit(&home.ui, x, y) : -1;
}

const char *preview_events(void)
{
    static char out[sizeof events.text];
    memcpy(out, events.text, events.used + 1);
    said_clear(&events);
    return out;
}

const char *preview_notes(void)
{
    return notes.text;
}
