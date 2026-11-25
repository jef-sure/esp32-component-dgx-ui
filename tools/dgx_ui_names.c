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

const char *const table_words[TABLES]                 = {"look", "value", "picture", "style", "on", "take", "ctx", "id"};
const char *const table_keys[TABLES]                  = {"look", "value", "picture", "style", "on", "take", "ctx", "id"};
table_t           tables[TABLES], used[TABLES];
const char *const appearance_words[APPEARANCE_TABLES] = {"draw", "font", "hand"};
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
    if (table == T_ON && (strcmp(word, "back") == 0 || strcmp(word, "page") == 0)) return printed("dgx_ui_load_on_%s", word);
    for (int k = 0; table == T_LOOK && k < library_look_count; ++k)
        if (strcmp(library_looks[k], word) == 0) return printed("&dgx_ui_look_%s", word);
    return NULL;
}

int takes_of(const entry_t *handler)
{
    if (handler->c && strcmp(handler->c, "dgx_ui_load_on_page") == 0) return DGX_UI_TAKES_PAGE;
    return handler->number ? DGX_UI_TAKES_NUMBER : DGX_UI_TAKES_OWN;
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
        char *words[5];
        int   count = 0;
        for (char *word = strtok(line, " \t\r\n"); word && count < 5; word = strtok(NULL, " \t\r\n")) words[count++] = word;
        if (count == 0) continue;
        int kind = 0;
        while (kind < APPEARANCE_TABLES && strcmp(appearance_words[kind], words[0]) != 0) ++kind;
        if (kind < APPEARANCE_TABLES) {
            char *said = appearance_line(path, number, kind, words, count);
            if (said) return said;
            continue;
        }
        if (appearance_alone) continue;
        int table = 0;
        while (table < TABLES && strcmp(table_words[table], words[0]) != 0) ++table;
        if (table == TABLES) return printed("%s:%d: no table \"%s\": look, value, picture, style, on, take, ctx, id, draw, font or hand", path, number, words[0]);
        if (count < 2) return printed("%s:%d: a %s with no name", path, number, words[0]);
        if (!loader_name(words[1])) return printed("%s:%d: \"%s\" is not a name a description may have", path, number, words[1]);
        if (entry_of(&tables[table], words[1])) return printed("%s:%d: the %s \"%s\" twice", path, number, words[0], words[1]);
        if (tables[table].count == 65535) return printed("%s:%d: a %s past the 65535 a table may have", path, number, words[0]);
        if (table == T_ID && tables[table].count == DGX_UI_LOAD_IDS) return printed("%s:%d: an id past the %d a table may have", path, number, DGX_UI_LOAD_IDS);
        char *library = library_c(table, words[1]);
        if (count == 2 && !library)
            return printed("%s:%d: the %s \"%s\" with no C expression: only a look of the library's and the actions back and page go without one", path,
                     number, words[0], words[1]);
        entry_t *entry          = entry_add(&tables[table], words[1]);
        entry->c                = count == 2 ? library : copy_of(words[2]);
        entry->count            = 1;
        tables[table].from_file = true;
        if (count == 2) continue;
        free(library);
        if (count == 3) continue;
        if (count > 4) return printed("%s:%d: more than a table, a name, an expression and a word: an expression is written with no spaces", path, number);
        if (table == T_PICTURE) {
            char *end;
            const long pictures = strtol(words[3], &end, 10);
            if (*end || pictures < 1 || pictures > 65535) return printed("%s:%d: \"%s\" is not a count of pictures, 1 to 65535", path, number, words[3]);
            entry->count = (unsigned)pictures;
        } else if (table == T_ON && strcmp(words[3], "number") == 0) {
            entry->number = true;
        } else if (table == T_CTX && strcmp(words[3], "page") == 0) {
            entry->page = true;
        } else {
            return printed("%s:%d: \"%s\" means nothing for a %s: an expression is written with no spaces", path, number, words[3], words[0]);
        }
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
    if (depth > DGX_UI_LOAD_DEPTH || dgx_ui_json_peek(json) != '{') return;
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
        } else if (!dgx_ui_json_skip(json, 2 * DGX_UI_LOAD_DEPTH)) {
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
        else if (!dgx_ui_json_skip(&json, 2 * DGX_UI_LOAD_DEPTH + 1)) return; /* as the loader passes a value of the top */
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
            case T_ON:
                if ((entry->c = library_c(T_ON, use->name))) entry->ruled = false;
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
        if (!dgx_ui_json_skip(json, DGX_UI_LOAD_DEPTH * 2 + 1)) return false;
    }
    return false;
}

/* of each variant its name, or its object's "font"; stopped at what is not as it should be, which the loader then refuses */
void names_collect_fonts(const char *text, size_t text_length)
{
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
