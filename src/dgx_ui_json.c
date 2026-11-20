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

bool dgx_ui_json_word(dgx_ui_json_t *json, const char *word)
{
    const size_t length = strlen(word);
    if ((dgx_ui_json_peek(json), json->len - json->pos < length) || memcmp(json->text + json->pos, word, length) != 0)
        return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, json->pos);
    json->pos += length;
    return true;
}

bool dgx_ui_json_skip(dgx_ui_json_t *json, int levels)
{
    if (levels < 0) return dgx_ui_json_fail(json, DGX_UI_JSON_DEPTH, json->pos);
    const int c = dgx_ui_json_peek(json);
    size_t    length;
    int32_t   number;
    if (c == '"') return dgx_ui_json_string(json, NULL, 0, &length);
    if (c == '-' || (c >= '0' && c <= '9')) return dgx_ui_json_integer(json, INT32_MIN, INT32_MAX, &number);
    if (c == 't') return dgx_ui_json_word(json, "true");
    if (c == 'f') return dgx_ui_json_word(json, "false");
    if (c == 'n') return dgx_ui_json_word(json, "null");
    if (c != '{' && c != '[') return dgx_ui_json_fail(json, DGX_UI_JSON_SYNTAX, json->pos);
    const int close = c == '{' ? '}' : ']';
    ++json->pos;
    if (dgx_ui_json_peek(json) == close) return ++json->pos, true;
    for (bool more = true; more;) {
        if (c == '{' && (!dgx_ui_json_string(json, NULL, 0, &length) || !dgx_ui_json_expect(json, ':'))) return false;
        if (!dgx_ui_json_skip(json, levels - 1) || !dgx_ui_json_next(json, close, &more)) return false;
    }
    return true;
}
