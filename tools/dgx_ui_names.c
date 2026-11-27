/*
 * dgx_ui_names.c
 *
 *  The names a description is read on, as dgx_ui_c reads them, in a source of its own to be shared (dgx_ui_names.h).
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#define _GNU_SOURCE /* vasprintf() */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_image_file.h"
#include "dgx_ui_load.h"
#include "dgx_ui_names.h"

/* the library's own looks, by their words: made at the build from the declarations of include/ */
const char *const library_looks[] = {
#define LOOK(word) #word,
#include "library_looks.h"
#undef LOOK
};
const int library_look_count = sizeof library_looks / sizeof *library_looks;

/* the library's draws, by their words, with the type of their parameters and their description: made at the build
   from src/dgx_ui_params.c */
#define DRAW(word, type) {#word, #type, &dgx_ui_params_##word},
#define COUNT(type, member, count)
const library_draw_t library_draws[] = {
#include "library_draws.h"
};
#undef DRAW
#undef COUNT
const int         library_draw_count = sizeof library_draws / sizeof *library_draws;
const char *const library_hands[2]   = {"handwriting", "handwriting_canvas"};

/* ---- the tables ---- */

const char *const table_words[TABLES]                 = {"look", "value", "picture", "style", "on_event", "take", "ctx", "id"};
const char *const table_keys[TABLES]                  = {"look", "value", "picture", "style", "on_event", "take", "ctx", "id"};
table_t           tables[TABLES], used[TABLES];
const char *const appearance_words[APPEARANCE_TABLES] = {"draw", "font", "hand", "picture"};
table_t           appearance_tables[APPEARANCE_TABLES];
const char        DEFAULT_STYLE[] = "the default";

char *copy_of(const char *text)
{
    char *made = strdup(text);
    if (!made) names_no_memory();
    return made;
}

char *printed(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *made = NULL;
    if (vasprintf(&made, fmt, args) < 0) names_no_memory();
    va_end(args);
    return made;
}

entry_t *entry_of(table_t *table, const char *name)
{
    for (int k = 0; k < table->count; ++k)
        if (strcmp(table->entries[k].name, name) == 0) return &table->entries[k];
    return NULL;
}

entry_t *entry_add(table_t *table, const char *name)
{
    if (table->count == table->room) {
        table->room    = table->room ? 2 * table->room : 16;
        table->entries = realloc(table->entries, (size_t)table->room * sizeof *table->entries);
        if (!table->entries) names_no_memory();
    }
    entry_t *entry = &table->entries[table->count++];
    *entry         = (entry_t){.offset = -1, .library = -1};
    snprintf(entry->name, sizeof entry->name, "%s", name);
    return entry;
}

bool loader_name(const char *name)
{
    const size_t length = strlen(name);
    if (length == 0 || length > 23) return false;
    for (size_t k = 0; k < length; ++k) {
        const char c = name[k];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) return false;
    }
    return true;
}

bool c_name(const char *name, bool members)
{
    bool start = true;
    for (const char *p = name; *p; ++p) {
        const char c = *p;
        if (c == '.' && members && !start) {
            start = true;
            continue;
        }
        const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        if (!letter && !(c >= '0' && c <= '9' && !start)) return false;
        start = false;
    }
    return !start;
}

/* ---- the names file ---- */

char *library_c(int table, const char *word)
{
    if (table == T_ON_EVENT && (strcmp(word, "back") == 0 || strcmp(word, "page") == 0 || strcmp(word, "command") == 0))
        return printed("dgx_ui_action_%s", word);
    for (int k = 0; table == T_LOOK && k < library_look_count; ++k)
        if (strcmp(library_looks[k], word) == 0) return printed("&dgx_ui_look_%s", word);
    return NULL;
}

int ctx_kind_of(const entry_t *handler)
{
    if (handler->c && strcmp(handler->c, "dgx_ui_action_page") == 0) return DGX_UI_CTX_PAGE;
    if (handler->c && strcmp(handler->c, "dgx_ui_action_command") == 0) return DGX_UI_CTX_NUMBER_ONLY;
    return handler->number ? DGX_UI_CTX_NUMBER : DGX_UI_CTX_OWN;
}

int library_draw_of(const char *word)
{
    for (int k = 0; k < library_draw_count; ++k)
        if (strcmp(library_draws[k].word, word) == 0) return k;
    return -1;
}

bool library_hand(const char *word)
{
    return strcmp(word, library_hands[0]) == 0 || strcmp(word, library_hands[1]) == 0;
}

/* a line of the names file for an appearance: a draw, a font, a hand */
static char *appearance_line(const char *path, int number, int table, char **words, int count)
{
    if (count < 2) return printed("%s:%d: a %s with no name", path, number, words[0]);
    if (!loader_name(words[1])) return printed("%s:%d: \"%s\" is not a name a description may have", path, number, words[1]);
    if (entry_of(&appearance_tables[table], words[1])) return printed("%s:%d: the %s \"%s\" twice", path, number, words[0], words[1]);
    if (appearance_tables[table].count == 65535) return printed("%s:%d: a %s past the 65535 dgx_ui_c takes", path, number, words[0]);
    entry_t *entry                     = entry_add(&appearance_tables[table], words[1]);
    appearance_tables[table].from_file = true;
    if (table == A_DRAW) {
        entry->library = count == 2 ? library_draw_of(words[1]) : -1;
        if (count == 2 && entry->library < 0)
            return printed("%s:%d: the draw \"%s\" with no C function: only a draw of the library's goes without one", path, number, words[1]);
        if (count > 4) return printed("%s:%d: more than a table, a name, a function and a description: an expression is written with no spaces", path, number);
        entry->c         = count == 2 ? printed("dgx_ui_draw_%s", words[1]) : copy_of(words[2]);
        entry->described = count == 2 ? printed("&dgx_ui_params_%s", words[1]) : count == 4 ? copy_of(words[3]) : NULL;
    } else if (table == A_FONT) {
        entry->handwritten = count >= 3 && strcmp(words[count - 1], "handwritten") == 0;
        const int given    = count - entry->handwritten;
        if (given > 3) return printed("%s:%d: \"%s\" means nothing for a font: an expression is written with no spaces", path, number, words[3]);
        if (given == 2 && !c_name(words[1], false)) return printed("%s:%d: the font \"%s\" with no constant of C, and dgx_font_%s is not a name of C", path, number, words[1], words[1]);
        entry->c = given == 3 ? printed("&%s", words[2]) : printed("&dgx_font_%s", words[1]);
    } else {
        if (count > 2 || !library_hand(words[1]))
            return printed("%s:%d: the hand \"%s\": the library's alone, handwriting or handwriting_canvas, with no expression", path, number, words[1]);
        entry->c = printed("&dgx_ui_%s", words[1]);
    }
    return NULL;
}

/* a picture line of an asset: picture <name> @<asset> [<count>] [scale <n>] [map=<C array>] */
static char *asset_line(const char *path, int number, entry_t *entry, char **words, int count)
{
    const char *asset = words[2] + 1;
    if (!c_name(asset, false))
        return printed("%s:%d: \"%s\": an asset of DGX is @ and its name, a name of C, as dgx_image_need() gave it", path, number, words[2]);
    free(entry->c);
    entry->asset = copy_of(asset), entry->line = number;
    entry->c     = printed("pictures_of_%s", entry->name); /* the table the made C keeps and fills, one a line */
    for (char *at = entry->c; *at; ++at)
        if (*at == '.' || *at == '-') *at = '_'; /* a name of the loader's made one of C */
    for (int k = 3; k < count; ++k) {
        char *end;
        if (strcmp(words[k], "scale") == 0 && k + 1 < count) {
            const long scale = strtol(words[++k], &end, 10);
            if (*end || scale < 0 || scale > 255) return printed("%s:%d: \"%s\" is not a scale, 0 to 255", path, number, words[k]);
            entry->scale = (unsigned)scale;
        } else if (strncmp(words[k], "map=", 4) == 0) {
            if (!c_name(words[k] + 4, true)) return printed("%s:%d: \"%s\": map= takes the name of the program's array of C", path, number, words[k]);
            entry->map = copy_of(words[k] + 4);
        } else {
            const long pictures = strtol(words[k], &end, 10);
            if (*end || pictures < 1 || pictures > 65535 || entry->count_given)
                return printed("%s:%d: \"%s\" means nothing for a picture of a file: a count, scale <n>, or map=<array>", path, number, words[k]);
            entry->count = (unsigned)pictures, entry->count_given = true;
        }
    }
    return NULL;
}

int nearest_color(const uint32_t *rgb, int count, unsigned r, unsigned g, unsigned b)
{
    int      best = 0;
    uint32_t least = UINT32_MAX;
    for (int k = 0; k < count; ++k) {
        const int      dr = (int)(rgb[k] >> 16 & 255) - (int)r, dg = (int)(rgb[k] >> 8 & 255) - (int)g, db = (int)(rgb[k] & 255) - (int)b;
        const uint32_t far = (uint32_t)(2 * dr * dr + 4 * dg * dg + 3 * db * db);
        if (far < least) least = far, best = k;
    }
    return best;
}

char *picture_file_refused(const entry_t *entry, const void *opened)
{
    const dgx_image_file_t *file  = opened;
    bool                    alpha = false;
    const unsigned          format = file->bytes[4] | file->bytes[5] << 8u; /* of the header, read as DGX reads it */
    if (format > 2)
        return printed("picture %s: @%s is of the format %u of a later DGX: dgx-ui reads the formats 1 and 2, pictures of 1, 4 and 8 bits indexed, masks "
                       "of 4 and 8 bits and 16 bits of pixels", entry->name, entry->asset, format);
    for (unsigned j = 0; j < file->count; ++j) {
        dgx_image_file_picture_t one;
        alpha |= dgx_image_file_picture(file, j, &one) == ESP_OK && one.alpha;
    }
    for (unsigned j = 0; file->kind == DGX_IMAGE_FILE_INDEXED && j < file->colors; ++j) alpha |= file->palette[4 * j + 3] != 255;
    const bool mask = file->kind == DGX_IMAGE_FILE_MASK && (file->bits == 4 || file->bits == 8);
    if (!(file->kind == DGX_IMAGE_FILE_PIXELS && file->bits == 16) && !(file->kind == DGX_IMAGE_FILE_INDEXED && file->bits <= 8) && !mask) {
        char kind[40];
        snprintf(kind, sizeof kind, file->kind == DGX_IMAGE_FILE_PIXELS ? "pixels" : file->kind == DGX_IMAGE_FILE_INDEXED ? "an indexed palette"
                                    : file->kind == DGX_IMAGE_FILE_MASK ? "a mask" : "kind %u",
                 (unsigned)file->kind);
        return printed("picture %s: @%s is of %u bits of %s: dgx-ui draws 1, 4 and 8 bits indexed, masks of 4 and 8 bits and 16 bits of pixels", entry->name,
                       entry->asset, (unsigned)file->bits, kind);
    }
    if (mask && entry->map) return printed("picture %s: @%s is a mask, drawn in one colour, the look's ink: map= says nothing of it", entry->name, entry->asset);
    if (alpha && file->kind == DGX_IMAGE_FILE_INDEXED)
        return printed("picture %s: @%s keeps the opacity of its PNG in its palette (ALPHA), which dgx-ui does not mix: its need without ALPHA, or BITS 16 "
                       "for soft edges", entry->name, entry->asset);
    if (entry->count_given && entry->count != file->count)
        return printed("picture %s: the line says %u pictures, @%s holds %u", entry->name, entry->count, entry->asset, (unsigned)file->count);
    return NULL;
}

void picture_file_map(const void *opened, const uint32_t *rgb, int count, uint8_t map[255], uint8_t *solid, uint8_t *zero)
{
    const dgx_image_file_t *file = opened;
    memset(map, 0, 255);
    *solid = *zero = 0;
    for (unsigned j = 0; j < file->colors; ++j) {
        const uint8_t *c     = file->palette + 4 * j;
        const int      value = file->first + (int)j, index = nearest_color(rgb, count, c[0], c[1], c[2]);
        if (value == 0) *solid = 1, *zero = (uint8_t)index;
        else if (value <= 255) map[value - 1] = (uint8_t)index;
    }
}

char *names_read(const char *path, const char *text, size_t length, bool appearance_alone)
{
    char line[1024];
    int  number = 0;
    /* a line as fgets() gives it: to its '\n' or 1023 bytes, whichever comes first */
    for (size_t at = 0, taken; at < length; at += taken) {
        ++number;
        const char *newline = memchr(text + at, '\n', length - at);
        taken               = newline ? (size_t)(newline - (text + at)) + 1 : length - at;
        if (taken > sizeof line - 1) taken = sizeof line - 1;
        memcpy(line, text + at, taken), line[taken] = 0;
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char *words[8];
        int   count = 0;
        for (char *word = strtok(line, " \t\r\n"); word && count < 8; word = strtok(NULL, " \t\r\n")) words[count++] = word;
        if (count == 0) continue;
        int kind = 0;
        while (kind < A_PICTURE && strcmp(appearance_words[kind], words[0]) != 0) ++kind;
        if (kind < A_PICTURE) {
            char *said = appearance_line(path, number, kind, words, count);
            if (said) return said;
            continue;
        }
        /* of an appearance's names alone, its pictures are read too, into its own table; else into the page's, and
           given to the appearance's after the file */
        const bool picture = strcmp(words[0], table_words[T_PICTURE]) == 0;
        if (appearance_alone && !picture) continue;
        int table = 0;
        while (table < TABLES && strcmp(table_words[table], words[0]) != 0) ++table;
        if (table == TABLES) return printed("%s:%d: no table \"%s\": look, value, picture, style, on_event, take, ctx, id, draw, font or hand", path, number, words[0]);
        table_t *into = appearance_alone ? &appearance_tables[A_PICTURE] : &tables[table];
        if (count < 2) return printed("%s:%d: a %s with no name", path, number, words[0]);
        if (!loader_name(words[1])) return printed("%s:%d: \"%s\" is not a name a description may have", path, number, words[1]);
        if (entry_of(into, words[1])) return printed("%s:%d: the %s \"%s\" twice", path, number, words[0], words[1]);
        if (into->count == 65535) return printed("%s:%d: a %s past the 65535 a table may have", path, number, words[0]);
        if (table == T_ID && into->count == DGX_UI_LOAD_MAX_IDS) return printed("%s:%d: an id past the %d a table may have", path, number, DGX_UI_LOAD_MAX_IDS);
        char *library = library_c(table, words[1]);
        if (count == 2 && !library)
            return printed("%s:%d: the %s \"%s\" with no C expression: only a look of the library's and the actions back and page go without one", path,
                     number, words[0], words[1]);
        entry_t *entry  = entry_add(into, words[1]);
        entry->c        = count == 2 ? library : copy_of(words[2]);
        entry->count    = 1;
        into->from_file = true;
        if (table == T_PICTURE && count > 2 && words[2][0] == '@') { /* the pictures of a file of DGX, read where it lies */
            free(library);
            char *said = asset_line(path, number, entry, words, count);
            if (said) return said;
            continue;
        }
        if (count == 2) continue;
        free(library);
        if (count == 3) continue;
        if (count > 4) return printed("%s:%d: more than a table, a name, an expression and a word: an expression is written with no spaces", path, number);
        if (table == T_PICTURE) {
            char *end;
            const long pictures = strtol(words[3], &end, 10);
            if (*end || pictures < 1 || pictures > 65535) return printed("%s:%d: \"%s\" is not a count of pictures, 1 to 65535", path, number, words[3]);
            entry->count = (unsigned)pictures;
        } else if (table == T_ON_EVENT && strcmp(words[3], "number") == 0) {
            entry->number = true;
        } else if (table == T_CTX && strcmp(words[3], "page") == 0) {
            entry->page = true;
        } else {
            return printed("%s:%d: \"%s\" means nothing for a %s: an expression is written with no spaces", path, number, words[3], words[0]);
        }
    }
    /* the page's pictures given to the appearance too: one line of the file serves both */
    for (int k = 0; !appearance_alone && k < tables[T_PICTURE].count; ++k) {
        const entry_t *page = &tables[T_PICTURE].entries[k];
        if (entry_of(&appearance_tables[A_PICTURE], page->name)) continue;
        entry_t *entry = entry_add(&appearance_tables[A_PICTURE], page->name);
        entry->c = copy_of(page->c), entry->count = page->count;
        entry->asset = page->asset ? copy_of(page->asset) : NULL, entry->map = page->map ? copy_of(page->map) : NULL;
        entry->scale = page->scale, entry->count_given = page->count_given;
        appearance_tables[A_PICTURE].from_file = true;
    }
    return NULL;
}

/* ---- the names the description uses, by the reader of the loader ---- */

static void used_add(int table, const char *name, size_t offset)
{
    if (!entry_of(&used[table], name)) entry_add(&used[table], name)->offset = (long)offset;
}

/* a widget's object: the names of its keys, then its children; stopped at what is not as it should be, which the
   loader then refuses */
static void collect_widget(dgx_ui_json_t *json, int depth)
{
    if (depth > DGX_UI_LOAD_MAX_DEPTH || dgx_ui_json_peek(json) != '{') return;
    ++json->pos;
    if (dgx_ui_json_peek(json) == '}') {
        ++json->pos;
        return;
    }
    for (bool more = true; more;) {
        char   key[24], name[24];
        size_t length;
        if (!dgx_ui_json_string(json, key, 23, &length) || !dgx_ui_json_expect(json, ':')) return;
        key[length < 23 ? length : 23] = 0;
        int table = 0;
        while (table < TABLES && strcmp(table_keys[table], key) != 0) ++table;
        if (table < TABLES && dgx_ui_json_peek(json) == '"') {
            const size_t at = json->pos;
            if (!dgx_ui_json_string(json, name, 23, &length)) return;
            if (length <= 23) name[length] = 0, used_add(table, name, at);
        } else if (strcmp(key, "children") == 0 && dgx_ui_json_peek(json) == '[') {
            ++json->pos;
            if (dgx_ui_json_peek(json) == ']') ++json->pos;
            else
                for (bool child = true; child;) {
                    collect_widget(json, depth + 1);
                    if (json->error || !dgx_ui_json_next(json, ']', &child)) return;
                }
        } else if (!dgx_ui_json_skip(json, 2 * DGX_UI_LOAD_MAX_DEPTH)) {
            return;
        }
        if (!dgx_ui_json_next(json, '}', &more)) return;
    }
}

void names_collect(const char *text, size_t length)
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
        if (strcmp(key, "tree") == 0) collect_widget(&json, 1);
        else if (!dgx_ui_json_skip(&json, 2 * DGX_UI_LOAD_MAX_DEPTH + 1)) return; /* as the loader passes a value of the top */
        if (json.error || !dgx_ui_json_next(&json, '}', &more)) return;
    }
}

void names_rule(bool default_style)
{
    for (int table = 0; table < TABLES; ++table) {
        if (tables[table].from_file) continue;
        if (table == T_LOOK)
            for (int k = 0; k < library_look_count; ++k) entry_add(&tables[T_LOOK], library_looks[k])->c = library_c(T_LOOK, library_looks[k]);
        /* the style of a text that names none, the table's first: of a name no description can have; its C, of the
           prefix, made by prefix_tables() */
        if (table == T_STYLE && default_style) entry_add(&tables[T_STYLE], DEFAULT_STYLE);
        for (int k = 0; k < used[table].count; ++k) {
            const entry_t *use = &used[table].entries[k];
            if (entry_of(&tables[table], use->name)) continue;
            entry_t *entry = entry_add(&tables[table], use->name);
            entry->offset  = use->offset;
            entry->ruled   = true;
            switch (table) {
            case T_PICTURE: entry->c = copy_of(use->name), entry->count = 65535; break;
            case T_ON_EVENT:
                if ((entry->c = library_c(T_ON_EVENT, use->name))) entry->ruled = false;
                else entry->c = copy_of(use->name), entry->number = true;
                break;
            case T_TAKE: entry->c = copy_of(use->name); break;
            case T_CTX: entry->c = printed("&%s", use->name), entry->page = true; break; /* any may be a page's, as any handler a number */
            case T_ID: break; /* of the prefix: prefix_tables() */
            default: entry->c = printed("&%s", use->name); break;
            }
        }
    }
}

bool next_member(dgx_ui_json_t *json, bool *first, char key[24])
{
    size_t length;
    if (*first) {
        *first = false;
        if (dgx_ui_json_peek(json) != '{') return false;
        ++json->pos;
        if (dgx_ui_json_peek(json) == '}') return ++json->pos, false;
    } else {
        bool more;
        if (!dgx_ui_json_next(json, '}', &more) || !more) return false;
    }
    if (!dgx_ui_json_string(json, key, 23, &length) || !dgx_ui_json_expect(json, ':')) return false;
    key[length < 23 ? length : 23] = 0;
    return true;
}

bool top_value(dgx_ui_json_t *json, const char *wanted)
{
    char key[24];
    json->pos = 0;
    for (bool first = true; next_member(json, &first, key);) {
        if (strcmp(key, wanted) == 0) return true;
        if (!dgx_ui_json_skip(json, DGX_UI_LOAD_MAX_DEPTH * 2 + 1)) return false;
    }
    return false;
}

/* by the rule, the pictures the looks of the library's draws name in their fields of pictures: <name>, of 65535 there, as
   a page's picture by the rule (names_rule()); stopped at what is not as it should be, which the loader then refuses */
static void collect_pictures(const char *text, size_t text_length)
{
    dgx_ui_json_t json = {.text = text, .len = text_length};
    char          key[24], name[24], draw[24];
    size_t        length;
    if (!top_value(&json, "looks")) return;
    for (bool look = true; next_member(&json, &look, key);) {
        if (dgx_ui_json_peek(&json) != '{') {
            if (!dgx_ui_json_skip(&json, 8)) return;
            continue;
        }
        /* its draw first, wherever it stands in the look; then its parameters again */
        const size_t start = json.pos;
        draw[0]            = 0;
        for (bool field = true; next_member(&json, &field, key);) {
            if (strcmp(key, "draw") == 0 && dgx_ui_json_peek(&json) == '"') {
                if (!dgx_ui_json_string(&json, draw, 23, &length)) return;
                draw[length < 23 ? length : 23] = 0;
            } else if (!dgx_ui_json_skip(&json, 8)) {
                return;
            }
        }
        if (json.error) return;
        const size_t end = json.pos;
        const int    k   = library_draw_of(draw);
        if (k < 0 || !library_draws[k].params) continue;
        const dgx_ui_params_t *described = library_draws[k].params;
        json.pos                         = start;
        for (bool field = true; next_member(&json, &field, key);) {
            if (strcmp(key, "params") != 0 || dgx_ui_json_peek(&json) != '{') {
                if (!dgx_ui_json_skip(&json, 8)) return;
                continue;
            }
            for (bool param = true; next_member(&json, &param, key);) {
                const dgx_ui_field_t *of = NULL;
                for (int f = 0; f < described->field_count; ++f)
                    if (strcmp(described->fields[f].name, key) == 0) of = &described->fields[f];
                const size_t at = (dgx_ui_json_peek(&json), json.pos);
                if (of && of->kind == DGX_UI_FIELD_PICTURE && dgx_ui_json_peek(&json) == '"') {
                    if (!dgx_ui_json_string(&json, name, 23, &length)) return;
                    if (length <= 23 && !entry_of(&appearance_tables[A_PICTURE], (name[length] = 0, name))) {
                        entry_t *picture = entry_add(&appearance_tables[A_PICTURE], name);
                        picture->offset = (long)at, picture->ruled = true, picture->c = copy_of(name), picture->count = 65535;
                    }
                } else if (!dgx_ui_json_skip(&json, 8)) {
                    return;
                }
            }
            if (json.error) return;
        }
        json.pos = end;
    }
}

/* of each variant its name, or its object's "font"; stopped at what is not as it should be, which the loader then refuses;
   and the pictures of the looks */
void names_collect_fonts(const char *text, size_t text_length)
{
    collect_pictures(text, text_length);
    dgx_ui_json_t json = {.text = text, .len = text_length};
    char          key[24], name[24];
    size_t        length;
    if (!top_value(&json, "styles")) return;
    for (bool style = true; next_member(&json, &style, key);) {
        for (bool field = true; next_member(&json, &field, key);) {
            if (strcmp(key, "fonts") != 0 || dgx_ui_json_peek(&json) != '[') {
                if (!dgx_ui_json_skip(&json, 8)) return;
                continue;
            }
            ++json.pos;
            if (dgx_ui_json_peek(&json) == ']') {
                ++json.pos;
                continue;
            }
            for (bool more = true; more;) {
                const size_t at = (dgx_ui_json_peek(&json), json.pos);
                if (dgx_ui_json_peek(&json) == '"') {
                    if (!dgx_ui_json_string(&json, name, 23, &length)) return;
                    if (length <= 23 && !entry_of(&appearance_tables[A_FONT], (name[length] = 0, name))) {
                        entry_t *font = entry_add(&appearance_tables[A_FONT], name);
                        font->offset = (long)at, font->ruled = true, font->c = printed("&dgx_font_%s", name);
                    }
                } else {
                    for (bool inner = true; next_member(&json, &inner, key);) {
                        const size_t value = (dgx_ui_json_peek(&json), json.pos);
                        if (strcmp(key, "font") == 0 && dgx_ui_json_peek(&json) == '"') {
                            if (!dgx_ui_json_string(&json, name, 23, &length)) return;
                            if (length <= 23 && !entry_of(&appearance_tables[A_FONT], (name[length] = 0, name))) {
                                entry_t *font = entry_add(&appearance_tables[A_FONT], name);
                                font->offset = (long)value, font->ruled = true, font->c = printed("&dgx_font_%s", name);
                            }
                        } else if (!dgx_ui_json_skip(&json, 8)) {
                            return;
                        }
                    }
                    if (json.error) return;
                }
                if (!dgx_ui_json_next(&json, ']', &more)) return;
            }
        }
        if (json.error) return;
    }
}
