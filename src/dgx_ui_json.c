/*
 * dgx_ui_json.c
 *
 *  The reader of JSON of dgx_ui_json.h: nothing read past the text's length, the first error kept.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_json.h"

#include <string.h>

bool dgx_ui_json_fail(dgx_ui_json_t *json, int error, size_t at)
{
    if (json->error == DGX_UI_JSON_OK) json->error = error, json->error_at = at;
    return false;
}

int dgx_ui_json_peek(dgx_ui_json_t *json)
{
    const char *text = json->text;
    while (json->pos < json->len && (text[json->pos] == ' ' || text[json->pos] == '\t' || text[json->pos] == '\n' || text[json->pos] == '\r')) ++json->pos;
    return json->pos < json->len ? (unsigned char)text[json->pos] : -1;
}

bool dgx_ui_json_expect(dgx_ui_json_t *json, int c)
{
    if (dgx_ui_json_peek(json) != c) return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, json->pos);
    ++json->pos;
    return true;
}

bool dgx_ui_json_next(dgx_ui_json_t *json, int close, bool *more)
{
    const int c = dgx_ui_json_peek(json);
    if (c == ',') return ++json->pos, *more = true, true;
    if (c == close) return ++json->pos, *more = false, true;
    return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, json->pos);
}

/* four hex digits of \u at the reader */
static bool hex4(dgx_ui_json_t *json, uint32_t *value)
{
    *value = 0;
    for (int digit = 0; digit < 4; ++digit, ++json->pos) {
        if (json->pos >= json->len) return false;
        const char c = json->text[json->pos];
        const int  v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (v < 0) return false;
        *value = *value << 4 | (uint32_t)v;
    }
    return true;
}

/* a byte of the string out, as far as cap; counted always */
static void put(char *out, size_t cap, size_t *length, uint8_t c)
{
    if (out && *length < cap) out[*length] = (char)c;
    ++*length;
}

bool dgx_ui_json_string(dgx_ui_json_t *json, char *out, size_t cap, size_t *length)
{
    if (dgx_ui_json_peek(json) != '"') return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, json->pos);
    const char  *text = json->text;
    const size_t at   = json->pos++;
    size_t       made = 0;
    for (;;) {
        if (json->pos >= json->len) return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, at);
        const size_t here = json->pos;
        uint32_t     c    = (unsigned char)text[json->pos++];
        if (c == '"') break;
        if (c < 0x20) return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, here);
        if (c == '\\') {
            if (json->pos >= json->len) return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, here);
            switch (text[json->pos++]) {
            case '"': c = '"'; break;
            case '\\': c = '\\'; break;
            case '/': c = '/'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'u': {
                if (!hex4(json, &c) || (c >= 0xdc00 && c <= 0xdfff)) return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, here);
                if (c >= 0xd800 && c <= 0xdbff) { /* a high surrogate: its low one follows */
                    uint32_t low;
                    if (json->pos + 1 >= json->len || text[json->pos] != '\\' || text[json->pos + 1] != 'u') return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, here);
                    json->pos += 2;
                    if (!hex4(json, &low) || low < 0xdc00 || low > 0xdfff) return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, here);
                    c = 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
                }
                if (c == 0) return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, here); /* the end of a C string: not within one */
                break;
            }
            default: return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, here);
            }
            if (c < 0x80) put(out, cap, &made, (uint8_t)c);
            else if (c < 0x800) put(out, cap, &made, (uint8_t)(0xc0 | c >> 6)), put(out, cap, &made, (uint8_t)(0x80 | (c & 0x3f)));
            else if (c < 0x10000) put(out, cap, &made, (uint8_t)(0xe0 | c >> 12)), put(out, cap, &made, (uint8_t)(0x80 | (c >> 6 & 0x3f))), put(out, cap, &made, (uint8_t)(0x80 | (c & 0x3f)));
            else
                put(out, cap, &made, (uint8_t)(0xf0 | c >> 18)), put(out, cap, &made, (uint8_t)(0x80 | (c >> 12 & 0x3f))), put(out, cap, &made, (uint8_t)(0x80 | (c >> 6 & 0x3f))),
                    put(out, cap, &made, (uint8_t)(0x80 | (c & 0x3f)));
            continue;
        }
        if (c < 0x80) {
            put(out, cap, &made, (uint8_t)c);
            continue;
        }
        /* a sequence of UTF-8: its lead and its continuations, 80..bf; the first narrower after e0, ed, f0 and f4, so that
           no code point is written longer than it needs, none is a surrogate and none is past U+10FFFF */
        const int more = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : -1;
        if (more < 0) return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, here);
        const unsigned first_low = c == 0xe0 ? 0xa0 : c == 0xf0 ? 0x90 : 0x80, first_high = c == 0xed ? 0x9f : c == 0xf4 ? 0x8f : 0xbf;
        put(out, cap, &made, (uint8_t)c);
        for (int follow = 0; follow < more; ++follow, ++json->pos) {
            if (json->pos >= json->len) return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, here);
            const unsigned byte = (unsigned char)text[json->pos];
            if (byte < (follow ? 0x80 : first_low) || byte > (follow ? 0xbf : first_high)) return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, here);
            put(out, cap, &made, (uint8_t)byte);
        }
    }
    *length = made;
    return true;
}

bool dgx_ui_json_integer(dgx_ui_json_t *json, int32_t low, int32_t high, int32_t *value)
{
    dgx_ui_json_peek(json);
    const char  *text     = json->text;
    const size_t at       = json->pos;
    const bool   negative = json->pos < json->len && text[json->pos] == '-';
    if (negative) ++json->pos;
    if (json->pos >= json->len || text[json->pos] < '0' || text[json->pos] > '9') return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, at);
    if (text[json->pos] == '0' && json->pos + 1 < json->len && text[json->pos + 1] >= '0' && text[json->pos + 1] <= '9') return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, at);
    int64_t number = 0;
    while (json->pos < json->len && text[json->pos] >= '0' && text[json->pos] <= '9') {
        number = number * 10 + (text[json->pos++] - '0');
        if (number > (int64_t)INT32_MAX + 1) return dgx_ui_json_fail(json, DGX_UI_JSON_RANGE, at);
    }
    if (json->pos < json->len && (text[json->pos] == '.' || text[json->pos] == 'e' || text[json->pos] == 'E')) return dgx_ui_json_fail(json, DGX_UI_JSON_RANGE, at);
    if (negative) number = -number;
    if (number < low || number > high) return dgx_ui_json_fail(json, DGX_UI_JSON_RANGE, at);
    *value = (int32_t)number;
    return true;
}

bool dgx_ui_json_number_or_percent(dgx_ui_json_t *json, int32_t low, int32_t high, int32_t hundredths_low, int32_t hundredths_high, int32_t *value,
                                   bool *percent)
{
    *percent = dgx_ui_json_peek(json) == '"';
    if (!*percent) return dgx_ui_json_integer(json, low, high, value);
    enum { MOST = 12 }; /* "-127.99%" and room past it: a longer string is of no percent */
    char         text[MOST];
    size_t       length;
    const size_t at = json->pos;
    if (!dgx_ui_json_string(json, text, sizeof text, &length)) return false;
    const char *c = text, *end = text + (length < sizeof text ? length : 0);
    const bool  negative = c < end && *c == '-';
    c += negative;
    /* digits, no leading zero but a lone one; the whole held at 1000, past every percent; one or two decimals */
    const char *digits = c;
    int32_t     whole = 0, hundredths = 0, decimals = 0;
    for (; c < end && *c >= '0' && *c <= '9'; ++c) whole = whole >= 1000 ? 1000 : whole * 10 + (*c - '0');
    bool form = c > digits && (*digits != '0' || c == digits + 1);
    if (form && c < end && *c == '.') {
        const char *point = c++;
        for (; c < end && *c >= '0' && *c <= '9'; ++c, ++decimals)
            if (decimals < 2) hundredths = hundredths * 10 + (*c - '0');
        form = c > point + 1;
        if (decimals == 1) hundredths *= 10;
    }
    if (!form || c + 1 != end || *c != '%') return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, at);
    const int32_t all = negative ? -(whole * 100 + hundredths) : whole * 100 + hundredths;
    if (decimals > 2 || all < hundredths_low || all > hundredths_high) return dgx_ui_json_fail(json, DGX_UI_JSON_RANGE, at);
    /* never a tie: hundredths * 64 / 25 is never a half */
    *value = all >= 0 ? (all * 256 + 50) / 100 : -((-all * 256 + 50) / 100);
    return true;
}

bool dgx_ui_json_word(dgx_ui_json_t *json, const char *word)
{
    const size_t length = strlen(word);
    if ((dgx_ui_json_peek(json), json->len - json->pos < length) || memcmp(json->text + json->pos, word, length) != 0)
        return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, json->pos);
    json->pos += length;
    return true;
}

/* the digits at the reader passed over: how many */
static size_t digits(dgx_ui_json_t *json)
{
    const size_t from = json->pos;
    while (json->pos < json->len && json->text[json->pos] >= '0' && json->text[json->pos] <= '9') ++json->pos;
    return json->pos - from;
}

/* the byte at the reader passed over when it is the one wanted or the other */
static bool pass_either(dgx_ui_json_t *json, char wanted, char other)
{
    if (json->pos >= json->len || (json->text[json->pos] != wanted && json->text[json->pos] != other)) return false;
    return ++json->pos, true;
}

/* a number by JSON's grammar, of any kind and size, passed over: its error at its first byte */
static bool number(dgx_ui_json_t *json)
{
    const size_t at = json->pos;
    pass_either(json, '-', '-');
    const size_t first = json->pos, whole = digits(json);
    if (!whole || (whole > 1 && json->text[first] == '0') || (pass_either(json, '.', '.') && !digits(json)) ||
        (pass_either(json, 'e', 'E') && (pass_either(json, '+', '-'), !digits(json))))
        return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, at);
    return true;
}

/* a value that is not a list or an object */
static bool scalar(dgx_ui_json_t *json, int first_byte)
{
    size_t length;
    if (first_byte == '"') return dgx_ui_json_string(json, NULL, 0, &length);
    if (first_byte == '-' || (first_byte >= '0' && first_byte <= '9')) return number(json);
    if (first_byte == 't') return dgx_ui_json_word(json, "true");
    if (first_byte == 'f') return dgx_ui_json_word(json, "false");
    if (first_byte == 'n') return dgx_ui_json_word(json, "null");
    return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, json->pos);
}

/* the key of a member and its colon */
static bool key(dgx_ui_json_t *json)
{
    size_t length;
    return dgx_ui_json_string(json, NULL, 0, &length) && dgx_ui_json_expect(json, ':');
}

bool dgx_ui_json_skip(dgx_ui_json_t *json, int levels)
{
    if (levels > DGX_UI_JSON_LEVELS) levels = DGX_UI_JSON_LEVELS;
    uint64_t objects = 0; /* bit k: the bracket open at k is an object's */
    int      open    = 0; /* the brackets open: the value at the reader is nested so deep */
    for (;;) {
        /* a value */
        if (open > levels) return dgx_ui_json_fail(json, DGX_UI_JSON_DEPTH, json->pos);
        const int first_byte = dgx_ui_json_peek(json);
        if (first_byte == '{' || first_byte == '[') {
            ++json->pos;
            if (dgx_ui_json_peek(json) != (first_byte == '{' ? '}' : ']')) { /* its first member next */
                objects = first_byte == '{' ? objects | (uint64_t)1 << open : objects & ~((uint64_t)1 << open);
                ++open;
                if (first_byte == '{' && !key(json)) return false;
                continue;
            }
            ++json->pos;
        } else if (!scalar(json, first_byte)) {
            return false;
        }
        /* a value passed over: the brackets it closes, then the next member */
        for (;;) {
            if (open == 0) return true;
            const bool object = objects >> (open - 1) & 1;
            bool       more;
            if (!dgx_ui_json_next(json, object ? '}' : ']', &more)) return false;
            if (!more) {
                --open;
                continue;
            }
            if (object && !key(json)) return false;
            break;
        }
    }
}
