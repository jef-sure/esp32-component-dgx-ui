/*
 * dgx_ui_read.h
 *
 *  What the readers of descriptions share, a page's (dgx_ui_load.c) and an appearance's (dgx_ui_appearance.c): the
 *  reader of JSON with the first error in the load's words, a name, a name found in a table, a list of words made bits,
 *  and the top of a description judged in one order. The library's, not a public header.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dgx_ui_json.h"
#include "dgx_ui_load.h"

/* the most levels a value passed over goes in: a widget's object and its list of children two each; a value of the top
   is passed over with one less, a value of a widget at depth d with 2 * d less, so that a value stands inside at most
   DGX_UI_READ_LEVELS brackets, the top's own one of them */
enum { DGX_UI_READ_LEVELS = 2 * DGX_UI_LOAD_DEPTH + 2 };
_Static_assert(DGX_UI_READ_LEVELS - 1 <= DGX_UI_JSON_LEVELS, "the reader counts every level a description may take");

/* a description being read: its reader of JSON, the first error */
typedef struct dgx_ui_read_t {
    dgx_ui_json_t        json;
    dgx_ui_load_error_t *err;
} dgx_ui_read_t;

/* a read begun on a text, no error yet */
void dgx_ui_read_begin(dgx_ui_read_t *R, const char *json, size_t len, dgx_ui_load_error_t *err);

/* the first error kept, the reader's or the read's, its name ending on a whole character; false always */
bool dgx_ui_read_fail(dgx_ui_read_t *R, int code, size_t at, const char *name);

/* an error kept, the reader's or the read's */
bool dgx_ui_read_failed(const dgx_ui_read_t *R);

/* the reader's error in the load's words, when the read has none of its own */
void dgx_ui_read_reader_error(dgx_ui_read_t *R);

/* a name: a string of letters, digits, '_', '.' and '-', at most 23, into name[24] */
bool dgx_ui_read_name(dgx_ui_read_t *R, char name[24]);

/* the entry of a table of count entries of stride bytes, each beginning with its name; -1 none */
int dgx_ui_read_lookup(const void *table, int count, size_t stride, const char *name);

/* a name at the reader found in a table: its entry, or an error and -1 */
int dgx_ui_read_find(dgx_ui_read_t *R, const void *table, int count, size_t stride);

/* a list of words of words ("a\0b\0", ended by an empty one) into bits, a word's bit 1 << its place */
bool dgx_ui_read_words(dgx_ui_read_t *R, const char *words, uint32_t *bits);

/*
 * The object of a description's top, judged in one order: a text that is not JSON; then a version that is not the
 * integer 1; then a key unknown, one that is no name among them, or one twice. at[k] is where the value of keys[k] is,
 * 0 when it is not there; the caller judges the values after.
 */
bool dgx_ui_read_top(dgx_ui_read_t *R, const char *const *keys, int count, size_t *at);
