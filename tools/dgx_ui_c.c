/*
 * dgx_ui_c: a description of dgx_ui_load.h made C. It writes the constants of the very tree dgx_ui_load() builds of the
 * description and a function that adds them, so that a program shows the tree with no loader and no tables of names.
 *
 *     dgx_ui_c [-n <names file>] [-i <header>]... [-d <directory>] [-p <prefix>] [-f <files' name>] <description>
 *     dgx_ui_c -n <names file> [-i <header>]... [-d <directory>] [-f <files' name>] -t <prefix>
 *
 * -f names the files made when they are not to be named by the prefix: dgx_ui_tree() of project_include.cmake names
 * them by the description's file, which CMake knows with no reading of it.
 *
 * writes <prefix>.c and <prefix>.h into the directory ('.' when none is said); the prefix, a name of C, begins all they
 * declare. It is the tree's name, "name" of the description, unless -p gives another: -p is needed when the description
 * has no name, or one that is not a name of C, or one that begins with dgx_, as the libraries' names do. A file of that
 * name already there is written over only when the tool made it; another is refused, and neither file is written. The
 * description is read by the loader itself, on tables of stand-ins made of the names it uses: every check and every
 * default is the loader's, and the tree it lays is printed, each stand-in turned back into the name it stands for. A
 * description the loader refuses is refused with the loader's error, its byte and its name.
 *
 * A name becomes C by a names file, a line a name, '#' to the end of a line a comment:
 *
 *     <table> <name> <C expression> [<count> | number]
 *
 * the table one of look, value, picture, style, on, take, ctx, id; a picture's count of pictures (1 when none is
 * said), "number" for a handler whose ctx is a number; an expression is written with no spaces. A look of the
 * library's and the actions back and page may go with no expression: "look button" is &dgx_ui_look_button, "on back"
 * dgx_ui_load_on_back. The expression of an id is an int of the program's the add function writes the widget's index
 * to; the others are constant expressions, as the widgets are constants. With a names file every table is the
 * file's, a table it does not name an empty one, as for the loader on the tables made of the file: the verdict on a
 * description is the loader's on those tables, and the tree made the one it loads. A table may have at most 65535
 * names, the ids DGX_UI_LOAD_IDS.
 *
 * With -t the names file alone is made C: <prefix>.c and <prefix>.h, the loader's tables of its names, in its order,
 * as const dgx_ui_load_names_t <prefix>_names, its arrays <prefix>_looks, _values, _pictures, _styles, _handlers,
 * _takes, _contexts and _ids; so one file gives a program both its loader's tables and the trees made C.
 *
 * With no names file every table is made of the names the description uses, by the rule:
 *
 *     look     a word of the library's own looks &dgx_ui_look_<word>, any other name &<name>
 *     value    &<name>, so "settings.volume" is &settings.volume
 *     picture  <name>, of as many pictures as the widget reads
 *     style    &<name>; a text that names none &<prefix>_style, declared by the header, where the program's table would
 *              give its own first
 *     on       back and page the library's actions, dgx_ui_load_on_back and _page; any other <name>, its ctx a number
 *              when the description gives one
 *     take     <name>
 *     ctx      &<name>; an integer (void *)(intptr_t)<n>
 *     id       int <prefix>_<id>, declared by the header, written by the add function
 *
 * A name the rule cannot make C, one with '-', is refused with the name. So are the names the tool makes itself: an id
 * by the rule may not be add, style, widgets, parents, texts or buffers, and a name by the rule may not begin with
 * <prefix>_widgets, _parents, _texts or _buffers, the arrays of the file made. The file made says at its top what was
 * not checked for want of the program's tables.
 *
 * A file of the program's is not written over, nor a file the tool made of the other kind: the tables of a names file
 * for a tree, or a tree for tables.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#define _GNU_SOURCE /* vasprintf() */
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_ui.h"
#include "dgx_ui_json.h"
#include "dgx_ui_load.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"

/* the format this tool reads, as dgx_ui_load.h has it */
enum { FORMAT = 1 };

/* the library's own looks, by their words: made at the tool's build from the declarations of include/ */
static const char *const library_looks[] = {
#define LOOK(word) #word,
#include "library_looks.h"
#undef LOOK
};
enum { LIBRARY_LOOK_COUNT = sizeof library_looks / sizeof *library_looks };

/* ---- the tables ---- */

enum { T_LOOK, T_VALUE, T_PICTURE, T_STYLE, T_ON, T_TAKE, T_CTX, T_ID, TABLES };
static const char *const table_words[TABLES] = {"look", "value", "picture", "style", "on", "take", "ctx", "id"};
/* the keys of a widget that name an entry of a table, in the order of the tables */
static const char *const table_keys[TABLES] = {"look", "value", "picture", "style", "on", "take", "ctx", "id"};

typedef struct entry_t {
    char     name[24];
    char    *c;       /* the C expression it stands for */
    long     offset;  /* the byte of the description that first names it; -1: of the names file or the library */
    unsigned count;   /* a picture's pictures */
    bool     number;  /* a handler that takes a number in its ctx */
    bool     ruled;   /* made C by the rule: its name must be of C */
} entry_t;

typedef struct table_t {
    entry_t *entries;
    int      count, room;
    bool     from_file; /* the names file gave it: the whole truth for it */
} table_t;

static table_t     tables[TABLES], used[TABLES];
static const char  DEFAULT_STYLE[] = "the default"; /* a space: no description names it */
static const char *description_path, *prefix; /* with -t the names file's path, and the prefix -t gives */
static bool        tables_mode;                /* -t: the names file alone made C, the loader's tables */
static const char *files_name;                 /* the made files' name, -f; the prefix when none is given */
/* the words of the first comment: what a file was made of, and, last in its first paragraph, what to change */
static const char TREE_WORDS[] = ", a description of format ", TABLES_WORDS[] = ", a names file: ", EDIT_WORDS[] = "Not to be edited: change the ";
static char        tree_name[24]; /* the description's "name", as the loader told it; empty: none */

/* the names the tool makes, each <prefix>_<word>: the add function, the default style, the arrays of the file */
static const char *const made_words[] = {"add", "style", "widgets", "parents", "texts", "buffers"};
enum { MADE_WORD_COUNT = sizeof made_words / sizeof *made_words, FIRST_ARRAY_WORD = 2 };

static void fail_out(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fputs("dgx_ui_c: ", stderr);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    exit(1);
}

static char *copy_of(const char *text)
{
    char *made = strdup(text);
    if (!made) fail_out("no memory");
    return made;
}

static char *printed(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *made = NULL;
    if (vasprintf(&made, fmt, args) < 0) fail_out("no memory");
    va_end(args);
    return made;
}

static entry_t *entry_of(table_t *table, const char *name)
{
    for (int k = 0; k < table->count; ++k)
        if (strcmp(table->entries[k].name, name) == 0) return &table->entries[k];
    return NULL;
}

static entry_t *entry_add(table_t *table, const char *name)
{
    if (table->count == table->room) {
        table->room    = table->room ? 2 * table->room : 16;
        table->entries = realloc(table->entries, (size_t)table->room * sizeof *table->entries);
        if (!table->entries) fail_out("no memory");
    }
    entry_t *entry = &table->entries[table->count++];
    *entry         = (entry_t){.offset = -1};
    snprintf(entry->name, sizeof entry->name, "%s", name);
    return entry;
}

/* a name the loader takes: letters, digits, '_', '.' and '-', 1 to 23 */
static bool loader_name(const char *name)
{
    const size_t length = strlen(name);
    if (length == 0 || length > 23) return false;
    for (size_t k = 0; k < length; ++k) {
        const char c = name[k];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) return false;
    }
    return true;
}

/* a name of C: identifiers, joined by '.' when members are allowed */
static bool c_name(const char *name, bool members)
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

/* a name for the made files: letters, digits, '_', '-' and '.', neither '-' nor '.' first, so that no directory is named
   and no option read */
static bool file_name_of_files(const char *name)
{
    if (!name[0] || name[0] == '.' || name[0] == '-' || strlen(name) > 200) return false;
    for (const char *p = name; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.')) return false;
    return true;
}

/* ---- the names file ---- */

/* the C of a word of the library's: a look of its own, or the action back or page; NULL when it is none */
static char *library_c(int table, const char *word)
{
    if (table == T_ON && (strcmp(word, "back") == 0 || strcmp(word, "page") == 0)) return printed("dgx_ui_load_on_%s", word);
    for (int k = 0; table == T_LOOK && k < LIBRARY_LOOK_COUNT; ++k)
        if (strcmp(library_looks[k], word) == 0) return printed("&dgx_ui_look_%s", word);
    return NULL;
}

static void names_file_read(const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file) fail_out("%s: %s", path, strerror(errno));
    char line[1024];
    for (int number = 1; fgets(line, sizeof line, file); ++number) {
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char *words[5];
        int   count = 0;
        for (char *word = strtok(line, " \t\r\n"); word && count < 5; word = strtok(NULL, " \t\r\n")) words[count++] = word;
        if (count == 0) continue;
        int table = 0;
        while (table < TABLES && strcmp(table_words[table], words[0]) != 0) ++table;
        if (table == TABLES) fail_out("%s:%d: no table \"%s\": look, value, picture, style, on, take, ctx or id", path, number, words[0]);
        if (count < 2) fail_out("%s:%d: a %s with no name", path, number, words[0]);
        if (!loader_name(words[1])) fail_out("%s:%d: \"%s\" is not a name a description may have", path, number, words[1]);
        if (entry_of(&tables[table], words[1])) fail_out("%s:%d: the %s \"%s\" twice", path, number, words[0], words[1]);
        if (tables[table].count == 65535) fail_out("%s:%d: a %s past the 65535 a table may have", path, number, words[0]);
        if (table == T_ID && tables[table].count == DGX_UI_LOAD_IDS) fail_out("%s:%d: an id past the %d a table may have", path, number, DGX_UI_LOAD_IDS);
        char *library = library_c(table, words[1]);
        if (count == 2 && !library)
            fail_out("%s:%d: the %s \"%s\" with no C expression: only a look of the library's and the actions back and page go without one", path,
                     number, words[0], words[1]);
        entry_t *entry          = entry_add(&tables[table], words[1]);
        entry->c                = count == 2 ? library : copy_of(words[2]);
        entry->count            = 1;
        tables[table].from_file = true;
        if (count == 2) continue;
        free(library);
        if (count == 3) continue;
        if (count > 4) fail_out("%s:%d: more than a table, a name, an expression and a word: an expression is written with no spaces", path, number);
        if (table == T_PICTURE) {
            char *end;
            const long pictures = strtol(words[3], &end, 10);
            if (*end || pictures < 1 || pictures > 65535) fail_out("%s:%d: \"%s\" is not a count of pictures, 1 to 65535", path, number, words[3]);
            entry->count = (unsigned)pictures;
        } else if (table == T_ON && strcmp(words[3], "number") == 0) {
            entry->number = true;
        } else {
            fail_out("%s:%d: \"%s\" means nothing for a %s: an expression is written with no spaces", path, number, words[3], words[0]);
        }
    }
    fclose(file);
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

static void collect(const char *text, size_t length)
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

/* the tables the file did not give, made of the names used by the rule */
static void rule_tables(void)
{
    for (int table = 0; table < TABLES; ++table) {
        if (tables[table].from_file) continue;
        if (table == T_LOOK)
            for (int k = 0; k < LIBRARY_LOOK_COUNT; ++k) entry_add(&tables[T_LOOK], library_looks[k])->c = library_c(T_LOOK, library_looks[k]);
        /* the style of a text that names none, the table's first: of a name no description can have; its C, of the
           prefix, made by prefix_tables() */
        if (table == T_STYLE) entry_add(&tables[T_STYLE], DEFAULT_STYLE);
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
            case T_ID: break; /* of the prefix: prefix_tables() */
            default: entry->c = printed("&%s", use->name); break;
            }
        }
    }
}

/* the C of the rule that is of the prefix, known once the loader has told the tree's name */
static void prefix_tables(void)
{
    if (!tables[T_STYLE].from_file) tables[T_STYLE].entries[0].c = printed("&%s_style", prefix);
    for (int k = 0; k < tables[T_ID].count; ++k)
        if (tables[T_ID].entries[k].ruled) tables[T_ID].entries[k].c = printed("%s_%s", prefix, tables[T_ID].entries[k].name);
}

/* a name the tool makes, by its word, of the prefix: the add function, the style, an array */
static int made_word_of(const char *word, size_t length)
{
    for (int k = 0; k < MADE_WORD_COUNT; ++k)
        if (strlen(made_words[k]) == length && strncmp(made_words[k], word, length) == 0) return k;
    return -1;
}

/* the names by the rule that would be the tool's own: an id <prefix>_<word>, or a name leading with an array of the file */
static void refuse_made_names(void)
{
    const size_t prefix_length = strlen(prefix);
    for (int table = 0; table < TABLES; ++table)
        for (int k = 0; k < tables[table].count; ++k) {
            const entry_t *entry = &tables[table].entries[k];
            if (!entry->ruled) continue;
            if (table == T_ID) {
                if (made_word_of(entry->name, strlen(entry->name)) >= 0)
                    fail_out("%s: byte %ld: the id \"%s\" would be %s_%s, a name dgx_ui_c makes: give the widget another id", description_path, entry->offset,
                             entry->name, prefix, entry->name);
                continue;
            }
            if (strncmp(entry->name, prefix, prefix_length) != 0 || entry->name[prefix_length] != '_') continue;
            const char *rest = entry->name + prefix_length + 1;
            const char *member = strchr(rest, '.');
            if (made_word_of(rest, member ? (size_t)(member - rest) : strlen(rest)) >= FIRST_ARRAY_WORD)
                fail_out("%s: byte %ld: the %s \"%s\" leads with an array of %s.c that dgx_ui_c makes: give it its expression in a names file, or the "
                         "tree another prefix",
                         description_path, entry->offset, table_words[table], entry->name, prefix);
        }
}

/* ---- the stand-ins: distinct places the loader keeps, each found again by its table and entry ---- */

static dgx_ui_look_t       *look_stand;
static int32_t             *value_stand;
static dgx_ui_picture_t    *picture_stand;
static dgx_ui_text_style_t *style_stand;
static uint8_t             *on_stand, *take_stand, *ctx_stand;
static int                 *id_stand;

static void *zeroed(int count, size_t size)
{
    void *made = calloc((size_t)(count > 0 ? count : 1), size);
    if (!made) fail_out("no memory");
    return made;
}

typedef void (*on_fn)(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
typedef bool (*take_fn)(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg);

static dgx_ui_load_names_t stand_in_names(void)
{
    for (int table = 0; table < TABLES; ++table)
        if (tables[table].count > 65535) fail_out("%s: more than the 65535 %s names a table may have", description_path, table_words[table]);
    look_stand    = zeroed(tables[T_LOOK].count, sizeof *look_stand);
    value_stand   = zeroed(tables[T_VALUE].count, sizeof *value_stand);
    picture_stand = zeroed(tables[T_PICTURE].count, sizeof *picture_stand);
    style_stand   = zeroed(tables[T_STYLE].count, sizeof *style_stand);
    on_stand      = zeroed(tables[T_ON].count, 1);
    take_stand    = zeroed(tables[T_TAKE].count, 1);
    ctx_stand     = zeroed(tables[T_CTX].count, 1);
    id_stand      = zeroed(tables[T_ID].count, sizeof *id_stand);
    /* an integer ctx is told from a context by its place: the contexts lie past every integer */
    if ((uintptr_t)ctx_stand <= (uintptr_t)INT32_MAX) fail_out("the stand-ins of the contexts lie among the integers of a ctx on this computer");
    dgx_ui_look_name_t    *looks    = zeroed(tables[T_LOOK].count, sizeof *looks);
    dgx_ui_value_name_t   *values   = zeroed(tables[T_VALUE].count, sizeof *values);
    dgx_ui_picture_name_t *pictures = zeroed(tables[T_PICTURE].count, sizeof *pictures);
    dgx_ui_style_name_t   *styles   = zeroed(tables[T_STYLE].count, sizeof *styles);
    dgx_ui_on_name_t      *handlers = zeroed(tables[T_ON].count, sizeof *handlers);
    dgx_ui_take_name_t    *takes    = zeroed(tables[T_TAKE].count, sizeof *takes);
    dgx_ui_context_name_t *contexts = zeroed(tables[T_CTX].count, sizeof *contexts);
    dgx_ui_id_t           *ids      = zeroed(tables[T_ID].count, sizeof *ids);
    for (int k = 0; k < tables[T_LOOK].count; ++k) looks[k] = (dgx_ui_look_name_t){tables[T_LOOK].entries[k].name, &look_stand[k]};
    for (int k = 0; k < tables[T_VALUE].count; ++k) values[k] = (dgx_ui_value_name_t){tables[T_VALUE].entries[k].name, &value_stand[k]};
    for (int k = 0; k < tables[T_PICTURE].count; ++k)
        pictures[k] = (dgx_ui_picture_name_t){tables[T_PICTURE].entries[k].name, &picture_stand[k], (uint16_t)tables[T_PICTURE].entries[k].count};
    for (int k = 0; k < tables[T_STYLE].count; ++k) styles[k] = (dgx_ui_style_name_t){tables[T_STYLE].entries[k].name, &style_stand[k]};
    for (int k = 0; k < tables[T_ON].count; ++k)
        handlers[k] = (dgx_ui_on_name_t){tables[T_ON].entries[k].name, (on_fn)(uintptr_t)&on_stand[k], tables[T_ON].entries[k].number};
    for (int k = 0; k < tables[T_TAKE].count; ++k) takes[k] = (dgx_ui_take_name_t){tables[T_TAKE].entries[k].name, (take_fn)(uintptr_t)&take_stand[k]};
    for (int k = 0; k < tables[T_CTX].count; ++k) contexts[k] = (dgx_ui_context_name_t){tables[T_CTX].entries[k].name, &ctx_stand[k]};
    for (int k = 0; k < tables[T_ID].count; ++k) ids[k] = (dgx_ui_id_t){tables[T_ID].entries[k].name, &id_stand[k]}, id_stand[k] = -1;
    return (dgx_ui_load_names_t){.looks = looks, .values = values, .pictures = pictures, .styles = styles, .handlers = handlers, .takes = takes,
                                 .contexts = contexts, .ids = ids, .look_count = (uint16_t)tables[T_LOOK].count, .value_count = (uint16_t)tables[T_VALUE].count,
                                 .picture_count = (uint16_t)tables[T_PICTURE].count, .style_count = (uint16_t)tables[T_STYLE].count,
                                 .handler_count = (uint16_t)tables[T_ON].count, .take_count = (uint16_t)tables[T_TAKE].count,
                                 .context_count = (uint16_t)tables[T_CTX].count, .id_count = (uint16_t)tables[T_ID].count};
}

/* the expression a stand-in stands for: its entry found by its place in its table's stand-ins */
static const char *c_of(int table, uintptr_t place, uintptr_t first, size_t size)
{
    if (place < first) fail_out("a %s the stand-ins do not have", table_words[table]);
    const size_t k = (place - first) / size;
    if ((int)k >= tables[table].count || (place - first) % size) fail_out("a %s the stand-ins do not have", table_words[table]);
    return tables[table].entries[k].c;
}

/* ---- the C ---- */

typedef struct out_t {
    char  *text;
    size_t length;
    FILE  *file;
} out_t;

static void out(out_t *o, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(o->file, fmt, args);
    va_end(args);
}

/* " *" and the lead of a line of the first comment: its column after them */
static int out_lead(out_t *o, const char *lead)
{
    out(o, " *%s%s", lead[0] ? " " : "", lead);
    return 2 + (lead[0] ? 1 + (int)strlen(lead) : 0);
}

/* a string of C: the quote, the backslash, a control character and a '?' after a '?' escaped, UTF-8 as it is */
static void out_string(out_t *o, const char *text)
{
    fputc('"', o->file);
    for (const char *p = text; *p; ++p) {
        const unsigned char c = (unsigned char)*p;
        switch (c) {
        case '"': fputs("\\\"", o->file); break;
        case '\\': fputs("\\\\", o->file); break;
        case '\n': fputs("\\n", o->file); break;
        case '\t': fputs("\\t", o->file); break;
        case '\r': fputs("\\r", o->file); break;
        case '?': fputs(p > text && p[-1] == '?' ? "\\?" : "?", o->file); break;
        default:
            if (c < 0x20 || c == 0x7f) fprintf(o->file, "\\%03o", c);
            else fputc(c, o->file);
        }
    }
    fputc('"', o->file);
}

static const struct {
    unsigned    bit;
    const char *word;
} flag_words[] = {{DGX_UI_COLUMN, "DGX_UI_COLUMN"}, {DGX_UI_ROW, "DGX_UI_ROW"},   {DGX_UI_STRETCH, "DGX_UI_STRETCH"}, {DGX_UI_SWIPE, "DGX_UI_SWIPE"},
                  {DGX_UI_DRAG, "DGX_UI_DRAG"},     {DGX_UI_CANVAS, "DGX_UI_CANVAS"}, {DGX_UI_REST, "DGX_UI_REST"},       {DGX_UI_REPEAT, "DGX_UI_REPEAT"}};
static const char *const kind_words[] = {
    [DGX_UI_BOX] = "DGX_UI_BOX",       [DGX_UI_FRAME] = "DGX_UI_FRAME",   [DGX_UI_LABEL] = "DGX_UI_LABEL",   [DGX_UI_PICTURE] = "DGX_UI_PICTURE",
    [DGX_UI_ICON] = "DGX_UI_ICON",     [DGX_UI_LIST] = "DGX_UI_LIST",     [DGX_UI_SWITCH] = "DGX_UI_SWITCH", [DGX_UI_SLIDER] = "DGX_UI_SLIDER",
    [DGX_UI_BUTTON] = "DGX_UI_BUTTON", [DGX_UI_INPUT] = "DGX_UI_INPUT",   [DGX_UI_DROPDOWN] = "DGX_UI_DROPDOWN", [DGX_UI_OWN] = "DGX_UI_OWN",
    [DGX_UI_RADIO] = "DGX_UI_RADIO"};
static const char *const error_words[] = {
    [DGX_UI_LOAD_SYNTAX] = "DGX_UI_LOAD_SYNTAX",       [DGX_UI_LOAD_VERSION] = "DGX_UI_LOAD_VERSION", [DGX_UI_LOAD_KEY] = "DGX_UI_LOAD_KEY",
    [DGX_UI_LOAD_KIND] = "DGX_UI_LOAD_KIND",           [DGX_UI_LOAD_FIELD] = "DGX_UI_LOAD_FIELD",     [DGX_UI_LOAD_RANGE] = "DGX_UI_LOAD_RANGE",
    [DGX_UI_LOAD_NAME_BAD] = "DGX_UI_LOAD_NAME_BAD",   [DGX_UI_LOAD_UNKNOWN] = "DGX_UI_LOAD_UNKNOWN", [DGX_UI_LOAD_NO_LOOK] = "DGX_UI_LOAD_NO_LOOK",
    [DGX_UI_LOAD_NO_STYLE] = "DGX_UI_LOAD_NO_STYLE",   [DGX_UI_LOAD_DEPTH_MAX] = "DGX_UI_LOAD_DEPTH_MAX", [DGX_UI_LOAD_MEMORY] = "DGX_UI_LOAD_MEMORY",
    [DGX_UI_LOAD_NODES] = "DGX_UI_LOAD_NODES",         [DGX_UI_LOAD_ID] = "DGX_UI_LOAD_ID"};

/* the widgets in the order they were added: parents before children, the parent's place in the order */
static int order_of(const dgx_ui_t *ui, int i, int *order, int *parent, int at, int up)
{
    order[at] = i, parent[at] = up;
    const int self = at++;
    for (int c = dgx_ui_node(ui, i)->child; c >= 0; c = dgx_ui_node(ui, c)->next) at = order_of(ui, c, order, parent, at, self);
    return at;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* a paragraph of the first comment, its words wrapped at 120 columns after "lead" */
static void out_paragraph(out_t *o, const char *lead, const char *text)
{
    int column = out_lead(o, lead);
    for (const char *word = text; *word;) {
        const char *end = strchr(word, ' ');
        const int   length = end ? (int)(end - word) : (int)strlen(word);
        if (column + 1 + length > 120) out(o, "\n"), column = out_lead(o, lead[0] == '-' ? " " : "");
        out(o, " %.*s", length, word), column += 1 + length;
        word += length;
        while (*word == ' ') ++word;
    }
    out(o, "\n");
}

/* the first comment of both files: what they were made from, and what was not checked */
static void out_head(out_t *o, const char *file_name)
{
    out(o, "/*\n");
    if (tables_mode) {
        char *made = printed("%s: made by dgx_ui_c from %s%sthe tables of names dgx_ui_load() takes. %snames file and make it again.", file_name,
                             base_name(description_path), TABLES_WORDS, EDIT_WORDS);
        out_paragraph(o, "", made), free(made);
        out(o, " */\n");
        return;
    }
    char *named = tree_name[0] && strcmp(tree_name, prefix) != 0 ? printed(", its tree \"%s\" named %s in C", tree_name, prefix) : copy_of("");
    char *made  = printed("%s: made by dgx_ui_c from %s%s%d of dgx_ui_load.h%s. %sdescription and make it again.", file_name,
                          base_name(description_path), TREE_WORDS, FORMAT, named, EDIT_WORDS);
    free(named);
    out_paragraph(o, "", made), free(made);
    char ruled[160] = "";
    for (int table = 0; table < TABLES; ++table)
        if (!tables[table].from_file) snprintf(ruled + strlen(ruled), sizeof ruled - strlen(ruled), "%s%s", ruled[0] ? ", " : "", table_words[table]);
    if (ruled[0]) {
        made = printed("Made C by the rule of dgx_ui_c, as no names file gave them: %s. A name the program has not is the C compiler's to "
                       "find.%s",
                       ruled, tables[T_PICTURE].from_file && tables[T_ON].from_file && tables[T_LOOK].from_file && tables[T_STYLE].from_file ? "" :
                                                                                                                                       " Not checked for want of the program's tables:");
        out(o, " *\n"), out_paragraph(o, "", made), free(made);
        if (!tables[T_PICTURE].from_file) out_paragraph(o, "-", "the pictures' counts: an icon's range, a switch's or a radio's two;");
        if (!tables[T_ON].from_file) out_paragraph(o, "-", "which handlers take a number in their ctx;");
        if (!tables[T_LOOK].from_file) out_paragraph(o, "-", "which looks the program has: a name not of the library's looks is taken as the program's own;");
        if (!tables[T_STYLE].from_file) {
            made = printed("the style of a text that names none, which the loader takes from the first of the program's styles: "
                           "here %s_style, the program's to define.",
                           prefix);
            out_paragraph(o, "-", made), free(made);
        }
    }
    out(o, " */\n");
}

/* a file the tool is to write: none there yet, or one it made of the same kind, its first comment its own; another is
   the program's, and one of the other kind is the tool's for another use */
static void refuse_others_file(const char *directory, const char *suffix)
{
    char *path = printed("%s/%s%s", directory, files_name, suffix);
    FILE *file = fopen(path, "rb");
    if (!file) {
        free(path);
        return;
    }
    char        *own    = printed("/*\n * %s%s: made by dgx_ui_c from ", files_name, suffix);
    const size_t length = strlen(own);
    char         head[2048];
    const size_t got = fread(head, 1, sizeof head - 1, file);
    fclose(file);
    head[got]       = 0;
    /* past its own beginning, the first comment's words, its lines joined; its source's name comes before the last
       words of its first paragraph, which say what it was made of */
    const bool begun = got >= length && memcmp(head, own, length) == 0;
    char      *words = head + (begun ? length : got), *end = strstr(words, "*/");
    if (end) *end = 0;
    for (char *line; (line = strstr(words, "\n * ")) != NULL;) memmove(line + 1, line + 4, strlen(line + 4) + 1), *line = ' ';
    const char *last = NULL;
    for (const char *at = words; (at = strstr(at, EDIT_WORDS)) != NULL; ++at) last = at;
    const bool  made        = begun && last;
    const bool  made_tables = made && strncmp(last + strlen(EDIT_WORDS), "names file", 10) == 0;
    const char *options     = tables_mode ? "give the tables another prefix with -t" : "give the tree another prefix with -p";
    if (!made) fail_out("%s: there, and not made by dgx_ui_c: not written over; %s, or the files another directory with -d", path, options);
    if (made_tables != tables_mode)
        fail_out("%s: made by dgx_ui_c of a %s, not of a %s: not written over; %s, or the files another directory with -d", path,
                 made_tables ? "names file" : "description", made_tables ? "description" : "names file", options);
    free(path), free(own);
}

static FILE *open_out(const char *directory, const char *suffix, char **path)
{
    *path      = printed("%s/%s%s", directory, files_name, suffix);
    FILE *file = fopen(*path, "w");
    if (!file) fail_out("%s: %s", *path, strerror(errno));
    return file;
}

static void write_c(const dgx_ui_t *ui, int root, const char *directory, const char *const *headers, int header_count)
{
    const int count  = ui->count;
    int      *order  = zeroed(count, sizeof *order);
    int      *parent = zeroed(count, sizeof *parent);
    const int laid   = order_of(ui, root, order, parent, 0, -1);
    char     *c_path, *h_path;
    out_t     c_file = {.file = open_out(directory, ".c", &c_path)}, h_file = {.file = open_out(directory, ".h", &h_path)};

    /* the header: the add function and the ids the rule made */
    out_head(&h_file, base_name(h_path));
    out(&h_file, "#pragma once\n\n#include \"dgx_ui.h\"\n#include \"dgx_ui_text.h\"\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");
    out(&h_file, "/**\n * @brief The tree of %s added under parent (-1: as the tree's root): its widgets, constants, and its text\n"
            " * boxes as the description gives them.\n * @return The index of its top widget; -1 when the tree had no room for one, what was added removed.\n */\n",
        base_name(description_path));
    out(&h_file, "int %s_add(dgx_ui_t *ui, int parent);\n", prefix);
    if (!tables[T_STYLE].from_file) {
        bool defaulted = false;
        for (int at = 0; at < laid; ++at) {
            const dgx_ui_textbox_t *box = dgx_ui_node(ui, order[at])->widget->text;
            defaulted |= box && box->style == &style_stand[0];
        }
        if (defaulted)
            out(&h_file, "\n/** The style of the texts that name none: the program's, filled before the tree is drawn. */\n"
                    "extern dgx_ui_text_style_t %s_style;\n",
                prefix);
    }
    bool ids_declared = false;
    for (int k = 0; k < tables[T_ID].count; ++k) {
        if (tables[T_ID].from_file || id_stand[k] < 0) continue;
        if (!ids_declared) out(&h_file, "\n/** The widgets the description names: their indices once added, -1 before. */\n"), ids_declared = true;
        out(&h_file, "extern int %s;\n", tables[T_ID].entries[k].c);
    }
    out(&h_file, "\n#ifdef __cplusplus\n}\n#endif\n");

    /* the C: the text boxes, the widgets, the parents, the add function */
    out_head(&c_file, base_name(c_path));
    out(&c_file, "#include <stdint.h>\n#include <string.h>\n\n#include \"dgx_ui.h\"\n#include \"dgx_ui_load.h\"\n#include \"dgx_ui_paint.h\"\n#include \"dgx_ui_text.h\"\n");
    for (int k = 0; k < header_count; ++k) out(&c_file, "#include \"%s\"\n", headers[k]);
    out(&c_file, "#include \"%s\"\n\n", base_name(h_path));
    for (int k = 0; k < tables[T_ID].count; ++k)
        if (!tables[T_ID].from_file && id_stand[k] >= 0) out(&c_file, "int %s = -1;\n", tables[T_ID].entries[k].c);
    /* the text boxes in one array in the order of their widgets; the buffers of those of a "text_size" in another, one
       after another; neither when there is nothing in it */
    int    *text_of   = zeroed(laid, sizeof *text_of);   /* a widget's text box in the array; -1: none */
    size_t *buffer_at = zeroed(laid, sizeof *buffer_at); /* its buffer's offset in the buffers */
    int     text_count = 0, constant_count = 0;
    size_t  buffer_bytes = 0;
    for (int at = 0; at < laid; ++at) {
        const dgx_ui_textbox_t *box = dgx_ui_node(ui, order[at])->widget->text;
        text_of[at]                 = box ? text_count++ : -1;
        if (box && box->size) buffer_at[at] = buffer_bytes, buffer_bytes += box->size;
        else constant_count += box != NULL;
    }
    if (buffer_bytes) out(&c_file, "static char             %s_buffers[%zu];\n", prefix, buffer_bytes);
    if (text_count) {
        out(&c_file, "static dgx_ui_textbox_t %s_texts[%d]%s\n", prefix, text_count, constant_count ? " = {" : ";");
        for (int at = 0; at < laid && constant_count; ++at) {
            const dgx_ui_textbox_t *box = dgx_ui_node(ui, order[at])->widget->text;
            if (!box || box->size) continue;
            out(&c_file, "    [%d] = DGX_UI_TEXT_CONST(", text_of[at]);
            out_string(&c_file, box->buf);
            out(&c_file, ", %u, %s),\n", (unsigned)box->lines, c_of(T_STYLE, (uintptr_t)box->style, (uintptr_t)style_stand, sizeof *style_stand));
        }
        if (constant_count) out(&c_file, "};\n");
    }
    out(&c_file, "\nstatic const dgx_ui_widget_t %s_widgets[%d] = {\n", prefix, laid);
    for (int at = 0; at < laid; ++at) {
        const dgx_ui_widget_t *w = dgx_ui_node(ui, order[at])->widget;
        out(&c_file, "    {.kind = %s", kind_words[w->kind]);
        if (w->flags) {
            out(&c_file, ", .flags = ");
            bool first = true;
            for (size_t k = 0; k < sizeof flag_words / sizeof *flag_words; ++k)
                if (w->flags & flag_words[k].bit) out(&c_file, "%s%s", first ? "" : " | ", flag_words[k].word), first = false;
        }
        if (w->state.bits)
            out(&c_file, ", .state = {%s%s%s}", w->state.bits & DGX_UI_HIDDEN ? "DGX_UI_HIDDEN" : "",
                (w->state.bits & DGX_UI_HIDDEN) && (w->state.bits & DGX_UI_DISABLED) ? " | " : "", w->state.bits & DGX_UI_DISABLED ? "DGX_UI_DISABLED" : "");
        const struct {
            const char *field;
            int         value;
        } numbers[] = {{"pad", w->pad},       {"gap", w->gap}, {"at_x", w->at_x}, {"at_y", w->at_y}, {"want_w", w->want_w}, {"want_h", w->want_h},
                       {"min", w->min},       {"max", w->max}, {"step", w->step}, {"tag", w->tag}};
        for (size_t k = 0; k < sizeof numbers / sizeof *numbers; ++k)
            if (numbers[k].value) out(&c_file, ", .%s = %d", numbers[k].field, numbers[k].value);
        if (w->value) out(&c_file, ", .value = %s", c_of(T_VALUE, (uintptr_t)w->value, (uintptr_t)value_stand, sizeof *value_stand));
        if (w->picture) out(&c_file, ", .picture = %s", c_of(T_PICTURE, (uintptr_t)w->picture, (uintptr_t)picture_stand, sizeof *picture_stand));
        if (w->text) out(&c_file, ", .text = &%s_texts[%d]", prefix, text_of[at]);
        if (w->look) out(&c_file, ", .look = %s", c_of(T_LOOK, (uintptr_t)w->look, (uintptr_t)look_stand, sizeof *look_stand));
        if (w->take) out(&c_file, ", .take = %s", c_of(T_TAKE, (uintptr_t)w->take, (uintptr_t)take_stand, 1));
        if (w->on) out(&c_file, ", .on = %s", c_of(T_ON, (uintptr_t)w->on, (uintptr_t)on_stand, 1));
        if (w->ctx) {
            const uintptr_t place = (uintptr_t)w->ctx;
            if (place >= (uintptr_t)ctx_stand && place < (uintptr_t)ctx_stand + (uintptr_t)tables[T_CTX].count)
                out(&c_file, ", .ctx = %s", c_of(T_CTX, place, (uintptr_t)ctx_stand, 1));
            else out(&c_file, ", .ctx = (void *)(intptr_t)%d", (int)(intptr_t)w->ctx);
        }
        out(&c_file, "},\n");
    }
    out(&c_file, "};\nstatic const int16_t %s_parents[%d] = {", prefix, laid);
    for (int at = 0; at < laid; ++at) out(&c_file, "%s%d", at ? ", " : "", parent[at]);
    out(&c_file, "};\n\nint %s_add(dgx_ui_t *ui, int parent)\n{\n", prefix);
    for (int at = 0; at < laid; ++at) {
        const dgx_ui_textbox_t *box = dgx_ui_node(ui, order[at])->widget->text;
        if (!box || !box->size) continue;
        const char *style  = c_of(T_STYLE, (uintptr_t)box->style, (uintptr_t)style_stand, sizeof *style_stand);
        char       *buffer = buffer_at[at] ? printed("%s_buffers + %zu", prefix, buffer_at[at]) : printed("%s_buffers", prefix);
        out(&c_file, "    dgx_ui_textbox_init(&%s_texts[%d], %s, %u, %u, %s);\n", prefix, text_of[at], buffer, (unsigned)box->size, (unsigned)box->lines, style);
        if (box->buf[0]) {
            out(&c_file, "    memcpy(%s, ", buffer);
            out_string(&c_file, box->buf);
            out(&c_file, ", %zu);\n", strlen(box->buf) + 1);
        }
        free(buffer);
    }
    out(&c_file, "    int16_t at[%d];\n", laid);
    out(&c_file, "    for (int k = 0; k < %d; ++k) {\n", laid);
    out(&c_file, "        const int added = dgx_ui_add(ui, k ? at[%s_parents[k]] : parent, &%s_widgets[k]);\n", prefix, prefix);
    out(&c_file, "        if (added < 0) {\n            if (k) dgx_ui_remove(ui, at[0]);\n            return -1;\n        }\n");
    out(&c_file, "        at[k] = (int16_t)added;\n    }\n");
    for (int k = 0; k < tables[T_ID].count; ++k) {
        if (id_stand[k] < 0) continue;
        int at = 0;
        while (at < laid && order[at] != id_stand[k]) ++at;
        out(&c_file, "    %s = at[%d];\n", tables[T_ID].entries[k].c, at);
    }
    out(&c_file, "    return at[0];\n}\n");
    if (fclose(c_file.file) || fclose(h_file.file)) fail_out("%s: %s", c_path, strerror(errno));
    free(order), free(parent), free(text_of), free(buffer_at);
}

/* the loader's tables of the names file: the types, the fields of dgx_ui_load_names_t, in the order of the tables */
static const char *const table_types[TABLES]  = {"dgx_ui_look_name_t",  "dgx_ui_value_name_t", "dgx_ui_picture_name_t", "dgx_ui_style_name_t",
                                                 "dgx_ui_on_name_t",    "dgx_ui_take_name_t",  "dgx_ui_context_name_t", "dgx_ui_id_t"};
static const char *const table_fields[TABLES] = {"looks", "values", "pictures", "styles", "handlers", "takes", "contexts", "ids"};
static const char *const count_fields[TABLES] = {"look_count", "value_count", "picture_count", "style_count", "handler_count", "take_count", "context_count", "id_count"};

static void write_tables(const char *directory, const char *const *headers, int header_count)
{
    char *c_path, *h_path;
    out_t c_file = {.file = open_out(directory, ".c", &c_path)}, h_file = {.file = open_out(directory, ".h", &h_path)};

    out_head(&h_file, base_name(h_path));
    out(&h_file, "#pragma once\n\n#include \"dgx_ui_load.h\"\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");
    out(&h_file, "/** The names of %s for dgx_ui_load_need() and dgx_ui_load(): what a description may name, and what each leads to. */\n",
        base_name(description_path));
    out(&h_file, "extern const dgx_ui_load_names_t %s_names;\n\n#ifdef __cplusplus\n}\n#endif\n", prefix);

    out_head(&c_file, base_name(c_path));
    out(&c_file, "#include <stdbool.h>\n#include <stddef.h>\n\n#include \"dgx_ui_load.h\"\n#include \"dgx_ui_paint.h\"\n");
    for (int k = 0; k < header_count; ++k) out(&c_file, "#include \"%s\"\n", headers[k]);
    out(&c_file, "#include \"%s\"\n", base_name(h_path));
    for (int table = 0; table < TABLES; ++table) {
        if (!tables[table].count) continue;
        out(&c_file, "\nstatic const %s %s_%s[%d] = {\n", table_types[table], prefix, table_fields[table], tables[table].count);
        for (int k = 0; k < tables[table].count; ++k) {
            const entry_t *entry = &tables[table].entries[k];
            switch (table) {
            case T_PICTURE: out(&c_file, "    {\"%s\", %s, %u},\n", entry->name, entry->c, entry->count); break;
            case T_ON: out(&c_file, "    {\"%s\", %s, %s},\n", entry->name, entry->c, entry->number ? "true" : "false"); break;
            case T_ID: out(&c_file, "    {\"%s\", &(%s)},\n", entry->name, entry->c); break;
            default: out(&c_file, "    {\"%s\", %s},\n", entry->name, entry->c); break;
            }
        }
        out(&c_file, "};\n");
    }
    out(&c_file, "\nconst dgx_ui_load_names_t %s_names = {\n", prefix);
    for (int table = 0; table < TABLES; ++table)
        if (tables[table].count) out(&c_file, "    .%s = %s_%s,\n", table_fields[table], prefix, table_fields[table]);
        else out(&c_file, "    .%s = NULL,\n", table_fields[table]);
    for (int table = 0; table < TABLES; ++table) out(&c_file, "    .%s = %d,\n", count_fields[table], tables[table].count);
    out(&c_file, "};\n");
    if (fclose(c_file.file) || fclose(h_file.file)) fail_out("%s: %s", c_path, strerror(errno));
}

static char *read_all(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if (!file) fail_out("%s: %s", path, strerror(errno));
    size_t room = 4096, have = 0;
    char  *text = malloc(room);
    for (size_t got; text && (got = fread(text + have, 1, room - have, file)) > 0;) {
        have += got;
        if (have == room) text = realloc(text, room *= 2);
    }
    if (!text) fail_out("no memory");
    fclose(file);
    *length = have;
    return text;
}

int main(int argc, char **argv)
{
    const char *names_path = NULL, *directory = ".", *tables_prefix = NULL;
    const char *headers[16];
    int         header_count = 0, arg = 1;
    for (; arg < argc && argv[arg][0] == '-'; arg += 2) {
        if (arg + 1 >= argc) fail_out("%s wants a value", argv[arg]);
        if (strcmp(argv[arg], "-n") == 0) names_path = argv[arg + 1];
        else if (strcmp(argv[arg], "-d") == 0) directory = argv[arg + 1];
        else if (strcmp(argv[arg], "-p") == 0) prefix = argv[arg + 1];
        else if (strcmp(argv[arg], "-t") == 0) tables_prefix = argv[arg + 1];
        else if (strcmp(argv[arg], "-f") == 0) files_name = argv[arg + 1];
        else if (strcmp(argv[arg], "-i") == 0 && header_count < 16) headers[header_count++] = argv[arg + 1];
        else fail_out("%s: not an option; -n <names file>, -i <header>, -d <directory>, -p <prefix>, -t <prefix>, -f <files' name>", argv[arg]);
    }
    if (files_name && !file_name_of_files(files_name)) fail_out("-f \"%s\" is not a name for the files: letters, digits, '_', '-' and '.', neither '-' nor '.' first", files_name);
    /* -t: the names file alone, made the loader's tables */
    if (tables_prefix) {
        if (argc - arg != 0) fail_out("-t makes the tables of a names file alone: no description with it");
        if (prefix) fail_out("-t names the tables' prefix: no -p with it");
        if (!names_path) fail_out("-t wants the names file, -n <names file>");
        if (!c_name(tables_prefix, false)) fail_out("-t \"%s\" is not a name of C", tables_prefix);
        if (strncmp(tables_prefix, "dgx_", 4) == 0) fail_out("-t \"%s\" begins with dgx_, as the libraries' names and headers do", tables_prefix);
        tables_mode = true, prefix = tables_prefix, description_path = names_path;
        if (!files_name) files_name = prefix;
        names_file_read(names_path);
        refuse_others_file(directory, ".c");
        refuse_others_file(directory, ".h");
        write_tables(directory, headers, header_count);
        return 0;
    }
    if (argc - arg != 1)
        fail_out("dgx_ui_c [-n <names file>] [-i <header>]... [-d <directory>] [-p <prefix>] [-f <files' name>] <description>, or -t <prefix> for the "
                 "tables of the names file");
    description_path = argv[arg];
    if (prefix && !c_name(prefix, false)) fail_out("-p \"%s\" is not a name of C", prefix);
    if (prefix && strncmp(prefix, "dgx_", 4) == 0) fail_out("-p \"%s\" begins with dgx_, as the libraries' names and headers do", prefix);

    size_t length;
    char  *text = read_all(description_path, &length);
    /* with a names file every table is the file's, an empty one where it names none, as for the loader on its tables */
    if (names_path) {
        names_file_read(names_path);
        for (int table = 0; table < TABLES; ++table) tables[table].from_file = true;
    }
    collect(text, length);
    rule_tables();

    /* the loader on the stand-ins: its checks, its defaults, its tree */
    dgx_ui_load_names_t names = stand_in_names();
    dgx_ui_load_need_t  need;
    dgx_ui_load_error_t err;
    if (!dgx_ui_load_need(text, length, &names, &need, &err)) {
        fprintf(stderr, "dgx_ui_c: %s: byte %d: %s", description_path, err.offset, error_words[err.code]);
        if (err.name[0]) fprintf(stderr, " \"%s\"", err.name);
        fputc('\n', stderr);
        return 1;
    }
    /* the prefix: the command's, else the tree's name, which must then be of C */
    memcpy(tree_name, need.name, sizeof tree_name);
    if (!prefix && !tree_name[0])
        fail_out("%s: the description has no \"name\": give the tree one there, or a prefix with -p (PREFIX of dgx_ui_tree())", description_path);
    if (!prefix && !c_name(tree_name, false))
        fail_out("%s: the name \"%s\" is not a name of C: give the tree a prefix of C with -p (PREFIX of dgx_ui_tree())", description_path, tree_name);
    if (!prefix && strncmp(tree_name, "dgx_", 4) == 0)
        fail_out("%s: the name \"%s\" begins with dgx_, as the libraries' names and headers do: give the tree a prefix with -p (PREFIX of dgx_ui_tree())",
                 description_path, tree_name);
    if (!prefix) prefix = tree_name;
    if (!files_name) files_name = prefix;
    /* a name the rule made must be of C */
    for (int table = 0; table < TABLES; ++table)
        for (int k = 0; k < tables[table].count; ++k) {
            const entry_t *entry = &tables[table].entries[k];
            if (entry->ruled && !c_name(entry->name, table != T_ID))
                fail_out("%s: byte %ld: the %s \"%s\" is not a name of C: give it its expression in a names file", description_path, entry->offset, table_words[table], entry->name);
        }
    refuse_made_names();
    prefix_tables();
    dgx_ui_node_t *nodes  = zeroed(need.nodes, sizeof *nodes);
    void          *memory = zeroed(1, need.bytes ? need.bytes : 1);
    dgx_ui_t       ui;
    dgx_ui_init(&ui, nodes, need.nodes, 1);
    const int root = dgx_ui_load(&ui, -1, text, length, &names, memory, need.bytes, &err);
    if (root < 0) fail_out("%s: byte %d: %s, after its need was told", description_path, err.offset, error_words[err.code]);
    /* no file of the program's written over: both looked at before either is written */
    refuse_others_file(directory, ".c");
    refuse_others_file(directory, ".h");
    write_c(&ui, root, directory, headers, header_count);
    return 0;
}
