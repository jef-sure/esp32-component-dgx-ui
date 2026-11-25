/*
 * The names a description is read on, as dgx_ui_c reads them: the lines of a names file, the names a description uses,
 * the tables the rule makes of them where no names file gives one, and the fonts an appearance's styles name. In a source
 * of its own, so that another reader of descriptions shares the one reading: a names file refused with the same words,
 * a description by the rule taking the same names.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "dgx_ui_appearance.h"
#include "dgx_ui_json.h"

/* the library's own looks, by their words, and its draws, with the type of their parameters and their description, and
   its hands: made at the build from the declarations of include/ and from src/dgx_ui_params.c */
extern const char *const library_looks[];
extern const int         library_look_count;
typedef struct library_draw_t {
    const char            *word, *type; /* the type "" for a draw of no parameters */
    const dgx_ui_params_t *params;
} library_draw_t;
extern const library_draw_t library_draws[];
extern const int            library_draw_count;
extern const char *const    library_hands[2];

/* ---- the tables ---- */

enum { T_LOOK, T_VALUE, T_PICTURE, T_STYLE, T_ON_EVENT, T_TAKE, T_CTX, T_ID, TABLES };
extern const char *const table_words[TABLES];
/* the keys of a widget that name an entry of a table, in the order of the tables */
extern const char *const table_keys[TABLES];

typedef struct entry_t {
    char     name[24];
    char    *c;               /* the C expression it stands for */
    long     offset;          /* the byte of the description that first names it; -1: of the names file or the library */
    unsigned count;           /* a picture's pictures */
    bool     number;          /* a handler that takes a number in its ctx */
    bool     page;            /* a context that is a page's tree */
    bool     ruled;           /* made C by the rule: its name must be of C */
    int      library;         /* a draw: its place among the library's; -1 the program's own */
    char    *described;       /* a draw: the C of its parameters' description; NULL none */
    bool     handwritten;     /* a font: a handwritten one, by the names file's word */
    bool     from_appearance; /* a look or a style of the appearance of -a */
} entry_t;

typedef struct table_t {
    entry_t *entries;
    int      count, room;
    bool     from_file; /* the names file gave it: the whole truth for it */
} table_t;

/* the tables of the names, and the names the description uses */
extern table_t tables[TABLES], used[TABLES];

/* the tables of an appearance's names: its draws, its fonts, its hands, of lines of their own words; and its pictures, of
   the names file's picture lines, which a page's table takes too */
enum { A_DRAW, A_FONT, A_HAND, A_PICTURE, APPEARANCE_TABLES };
extern const char *const appearance_words[APPEARANCE_TABLES];
extern table_t           appearance_tables[APPEARANCE_TABLES];
extern const char        DEFAULT_STYLE[]; /* a space: no description names it */

/* no memory: the end of the program that reads, each its own */
_Noreturn void names_no_memory(void);

char    *copy_of(const char *text);
char    *printed(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
entry_t *entry_of(table_t *table, const char *name);
entry_t *entry_add(table_t *table, const char *name);
/* a name the loader takes: letters, digits, '_', '.' and '-', 1 to 23 */
bool loader_name(const char *name);
/* a name of C: identifiers, joined by '.' when members are allowed */
bool c_name(const char *name, bool members);
/* the C of a word of the library's: a look of its own, or the action back or page; NULL when it is none */
char *library_c(int table, const char *word);
/* the kind of ctx a handler of the table of handlers takes: DGX_UI_CTX_* */
int ctx_kind_of(const entry_t *handler);
int   library_draw_of(const char *word);
bool  library_hand(const char *word);

/**
 * The lines of a names file into the tables, from its text: NULL when every line is taken, else the refusal of the first
 * that is not, "<path>:<line>: ...", in memory of its own. With appearance_alone only its draws, fonts and hands.
 */
char *names_read(const char *path, const char *text, size_t length, bool appearance_alone);
/* the names a description uses, into used[]: stopped at what is not as it should be, which the loader then refuses */
void names_collect(const char *text, size_t length);
/* the tables the file did not give, made of the names used by the rule; the style of a text that names none first
   unless an appearance's styles come first */
void names_rule(bool default_style);

/* the members of an object at the reader, its key given out; false at its end or at what is not JSON */
bool next_member(dgx_ui_json_t *json, bool *first, char key[24]);
/* a key of the top of an appearance: the reader at its value, or false */
bool top_value(dgx_ui_json_t *json, const char *wanted);
/* by the rule, the fonts an appearance's styles name, into appearance_tables[A_FONT], and the pictures its looks of the
   library's draws name, into appearance_tables[A_PICTURE]: a picture <name> of 65535 there, as a page's by the rule */
void names_collect_fonts(const char *text, size_t text_length);
