/*
 * dgx_ui_json.h
 *
 *  A reader of JSON that knows nothing of what it reads: a text of a length, never read past it (it need not end in 0),
 *  and the byte the reader is at; a string with its escapes and surrogate pairs decoded and its UTF-8 checked whole (no
 *  sequence longer than its code point needs, no surrogate, nothing past U+10FFFF), an integer within bounds, the words
 *  true, false and null, any value passed over, its numbers of any kind; each past white space before it. The first
 *  error kept, its code and its byte. The loader's (dgx_ui_load.c), not a public header.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** What went wrong, dgx_ui_json_t::error. */
enum {
    DGX_UI_JSON_OK,
    DGX_UI_JSON_SYNTAX, ///< not JSON, or not the value asked for
    DGX_UI_JSON_RANGE,  ///< an integer out of its bounds, a fraction, an exponent
    DGX_UI_JSON_DEPTH,  ///< a value passed over nested deeper than allowed
};

/** A text being read: set text and len, the rest 0. */
typedef struct dgx_ui_json_t {
    const char *text;
    size_t      len;
    size_t      pos;      ///< the byte the reader is at; the reader's user may set it back to a byte it was at
    int         error;    ///< DGX_UI_JSON_OK, or the first error
    size_t      error_at; ///< the byte of the first error: where the token it is in begins
} dgx_ui_json_t;

/** The error kept, when it is the first; false always. */
bool dgx_ui_json_fail(dgx_ui_json_t *json, int error, size_t at);

/** The next byte past white space, the reader there; -1 at the end of the text. */
int dgx_ui_json_peek(dgx_ui_json_t *json);

/** The byte c next, past white space, and the reader past it. */
bool dgx_ui_json_expect(dgx_ui_json_t *json, int c);

/** After a member of a list or an object: *more when a ',' follows, not *more at the closing byte close. */
bool dgx_ui_json_next(dgx_ui_json_t *json, int close, bool *more);

/**
 * A string, its UTF-8 written to out as far as cap (out may be NULL) and its whole length to *length: escapes and
 * surrogate pairs decoded, sequences of UTF-8 checked whole, no control character and no \u0000.
 */
bool dgx_ui_json_string(dgx_ui_json_t *json, char *out, size_t cap, size_t *length);

/** An integer within low..high: no fraction, no exponent, no leading zero. */
bool dgx_ui_json_integer(dgx_ui_json_t *json, int32_t low, int32_t high, int32_t *value);

/**
 * An integer within low..high, as dgx_ui_json_integer() reads it, or a percent: a string of an optional "-", digits with
 * no leading zero but a lone one, an optional "." with one or two digits, and "%", its value 8.8 fixed point,
 * round(hundredths * 256 / 100), the hundredths within hundredths_low..hundredths_high. *percent says which it was. A
 * string of another form is SYNTAX; more than two decimals, or a percent out of its bounds, RANGE.
 */
bool dgx_ui_json_number_or_percent(dgx_ui_json_t *json, int32_t low, int32_t high, int32_t hundredths_low, int32_t hundredths_high, int32_t *value,
                                   bool *percent);

/** The word true, false or null, as said. */
bool dgx_ui_json_word(dgx_ui_json_t *json, const char *word);

/** The most levels dgx_ui_json_skip() allows: more asked for are so many. */
enum { DGX_UI_JSON_LEVELS = 63 };

/**
 * Any value of JSON passed over, its syntax checked, nested at most levels deep (a scalar 0, [1] 1): strings as
 * dgx_ui_json_string() reads them, numbers of any kind and size. By a count and not by recursion: the stack the same
 * however deep the text.
 */
bool dgx_ui_json_skip(dgx_ui_json_t *json, int levels);
