/*
 * The reader of JSON, dgx_ui_json.h, alone: no tree and no table. White space, the closing marks and the commas; every
 * escape, surrogate pairs, UTF-8 of two, three and four bytes as it is; each malformed string with the byte of its
 * error: a lone surrogate, a high one with no low one, \u0000, an escape unknown, four hex digits short, a control
 * character, a lead byte not allowed, a sequence cut or of a wrong continuation, a code point in more bytes than it
 * needs, a surrogate in UTF-8, one past U+10FFFF, no closing quote; every lead and every byte after it, taken exactly
 * when their code point is well formed; a string longer than its room, counted whole. Integers at their bounds and past them, a leading zero, a fraction, an exponent; the
 * words; values passed over, nested to the levels allowed and one past them, and ten thousand deep. The first error
 * kept. Every text in memory of its exact length: a text cut at every byte and with every byte changed is never read
 * past its end, and its error is within it.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_ui_json.h"

/* a text in memory of its exact length, with no 0 after it: a read past its end is the sanitizer's */
static dgx_ui_json_t reader_of(const char *text, size_t length, char **copy)
{
    *copy = malloc(length ? length : 1);
    memcpy(*copy, text, length);
    return (dgx_ui_json_t){.text = *copy, .len = length};
}

/* UTF-8 well formed, by its code points: each sequence of the length its lead says, its continuations 10xxxxxx, the
   point no shorter than its length allows, not a surrogate, not past U+10FFFF */
static bool well_formed(const uint8_t *bytes, size_t count)
{
    static const uint32_t least[4] = {0, 0x80, 0x800, 0x10000};
    for (size_t at = 0; at < count;) {
        const uint8_t lead = bytes[at];
        int           follow;
        uint32_t      point;
        if (lead < 0x80) {
            ++at;
            continue;
        }
        if ((lead & 0xe0) == 0xc0) follow = 1, point = lead & 0x1f;
        else if ((lead & 0xf0) == 0xe0) follow = 2, point = lead & 0x0f;
        else if ((lead & 0xf8) == 0xf0) follow = 3, point = lead & 0x07;
        else return false;
        if (at + follow >= count) return false;
        for (int k = 1; k <= follow; ++k) {
            if ((bytes[at + k] & 0xc0) != 0x80) return false;
            point = point << 6 | (bytes[at + k] & 0x3f);
        }
        if (point < least[follow] || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) return false;
        at += (size_t)follow + 1;
    }
    return true;
}

/* a string read: its bytes as decoded, or its error and the byte of it */
static bool string_is(const char *text, const char *decoded, size_t decoded_length)
{
    char         *copy, out[64];
    dgx_ui_json_t json = reader_of(text, strlen(text), &copy);
    size_t        length, counted;
    bool          right = dgx_ui_json_string(&json, out, sizeof out, &length) && length == decoded_length && memcmp(out, decoded, length) == 0 && json.pos == json.len;
    json.pos = 0;
    right    = right && dgx_ui_json_string(&json, NULL, 0, &counted) && counted == length;
    if (!right) fprintf(stderr, "  %s: error %d at %d\n", text, json.error, (int)json.error_at);
    free(copy);
    return right;
}

static bool string_fails(const char *text, size_t length, int error_at)
{
    char         *copy, out[64];
    dgx_ui_json_t json = reader_of(text, length, &copy);
    size_t        decoded;
    const bool    read  = dgx_ui_json_string(&json, out, sizeof out, &decoded);
    const bool    right = !read && json.error == DGX_UI_JSON_SYNTAX && (int)json.error_at == error_at;
    if (!right) fprintf(stderr, "  string %.*s: read %d error %d at %d, expected at %d\n", (int)length, text, read, json.error, (int)json.error_at, error_at);
    free(copy);
    return right;
}

/* an integer within low..high: its value, or its error and the byte of it */
static bool integer_is(const char *text, int32_t low, int32_t high, int error, int error_at, int32_t value)
{
    char         *copy;
    dgx_ui_json_t json = reader_of(text, strlen(text), &copy);
    int32_t       got  = 0;
    const bool    read = dgx_ui_json_integer(&json, low, high, &got);
    const bool    right =
        error == DGX_UI_JSON_OK ? read && got == value && json.error == DGX_UI_JSON_OK : !read && json.error == error && (int)json.error_at == error_at;
    if (!right) fprintf(stderr, "  integer %s: read %d value %d error %d at %d\n", text, read, (int)got, json.error, (int)json.error_at);
    free(copy);
    return right;
}

/* a value passed over, nested at most levels deep: the reader past it, or its error */
static bool skips(const char *text, int levels, int error, int error_at)
{
    char         *copy;
    dgx_ui_json_t json  = reader_of(text, strlen(text), &copy);
    const bool    read  = dgx_ui_json_skip(&json, levels);
    const bool    right = error == DGX_UI_JSON_OK ? read && dgx_ui_json_peek(&json) == -1 : !read && json.error == error && (error_at < 0 || (int)json.error_at == error_at);
    if (!right) fprintf(stderr, "  skip %.60s: read %d error %d at %d\n", text, read, json.error, (int)json.error_at);
    free(copy);
    return right;
}

int main(void)
{
    char *copy;

    /* white space is a space, a tab, a line feed and a carriage return; the end -1 */
    {
        dgx_ui_json_t json = reader_of(" \t\r\n[\v", 6, &copy);
        CHECK(dgx_ui_json_peek(&json) == '[' && json.pos == 4);
        CHECK(dgx_ui_json_expect(&json, '[') && dgx_ui_json_peek(&json) == '\v');
        json.pos = 6;
        CHECK(dgx_ui_json_peek(&json) == -1 && !dgx_ui_json_expect(&json, ']') && json.error == DGX_UI_JSON_SYNTAX && json.error_at == 6);
        free(copy);
    }
    /* a list: the commas and the closing mark; a member with no comma before it at its byte */
    {
        dgx_ui_json_t json = reader_of("[1 , 2 ]", 8, &copy);
        int32_t       one, two;
        bool          more_one, more_two;
        CHECK(dgx_ui_json_expect(&json, '[') && dgx_ui_json_integer(&json, 0, 9, &one) && dgx_ui_json_next(&json, ']', &more_one) && more_one);
        CHECK(dgx_ui_json_integer(&json, 0, 9, &two) && dgx_ui_json_next(&json, ']', &more_two) && !more_two && one == 1 && two == 2 && json.pos == 8);
        free(copy);
        json = reader_of("[1 2]", 5, &copy);
        CHECK(dgx_ui_json_expect(&json, '[') && dgx_ui_json_integer(&json, 0, 9, &one));
        CHECK(!dgx_ui_json_next(&json, ']', &more_one) && json.error == DGX_UI_JSON_SYNTAX && json.error_at == 3);
        free(copy);
    }

    /* strings: every escape; \u of two and three bytes of UTF-8, a surrogate pair of four, hex digits of either case;
       UTF-8 of the text as it is */
    CHECK(string_is("\"\"", "", 0));
    CHECK(string_is("  \"abc\"", "abc", 3));
    CHECK(string_is("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"", "\"\\/\b\f\n\r\t", 8));
    CHECK(string_is("\"\\u00e9\\u20AC\\ud83d\\uDE00\\u0041\"", "\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80" "A", 10));
    CHECK(string_is("\"\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\xf4\x8f\xbf\xbf\"", "\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\xf4\x8f\xbf\xbf", 13));
    /* the edges of each length: U+0080, U+07FF, U+0800, U+D7FF, U+E000, U+FFFF, U+10000, U+10FFFF */
    CHECK(string_is("\"\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80\xef\xbf\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf\"",
                    "\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80\xef\xbf\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf", 24));
    /* each malformed string at the byte of its error: the escape's backslash, the lead byte, the control character, the
       opening quote when no closing one comes */
    CHECK(string_fails("\"a\\ud83d\"", 9, 2));          /* a high surrogate alone */
    CHECK(string_fails("\"a\\ud83d\\n\"", 11, 2));      /* a high one, an escape not \u after it */
    CHECK(string_fails("\"a\\ud83d\\u0041\"", 15, 2));  /* a high one, no low one after it */
    CHECK(string_fails("\"a\\ude00\"", 9, 2));          /* a low one alone */
    CHECK(string_fails("\"a\\u0000\"", 9, 2));          /* the end of a C string */
    CHECK(string_fails("\"a\\q\"", 5, 2));              /* an escape unknown */
    CHECK(string_fails("\"a\\u12\"", 7, 2));            /* four hex digits short */
    CHECK(string_fails("\"a\\u12g4\"", 9, 2));          /* not a hex digit */
    CHECK(string_fails("\"a\x01\"", 4, 2));             /* a control character */
    CHECK(string_fails("\"a\nb\"", 5, 2));              /* a line feed as it is */
    CHECK(string_fails("\"a\x80\"", 4, 2));             /* a continuation with no lead */
    CHECK(string_fails("\"a\xc0\x80\"", 5, 2));         /* a lead of two bytes too long for what it holds */
    CHECK(string_fails("\"a\xc1\xbf\"", 5, 2));
    CHECK(string_fails("\"a\xf5\x80\x80\x80\"", 7, 2)); /* past U+10FFFF */
    CHECK(string_fails("\"a\xff\"", 4, 2));
    CHECK(string_fails("\"a\xe2\x82\"", 5, 2));         /* a sequence cut by the quote */
    CHECK(string_fails("\"a\xe2\x41\xac\"", 6, 2));     /* a continuation not one */
    CHECK(string_fails("\"a\xe2\xc3\xac\"", 6, 2));     /* a lead where a continuation should be */
    CHECK(string_fails("\"a\xe2\x82", 4, 2));           /* a sequence cut by the end */
    CHECK(string_fails("\"a\xe0\x80\x80\"", 6, 2));     /* three bytes for a point of fewer: U+0000, U+07FF */
    CHECK(string_fails("\"a\xe0\x9f\xbf\"", 6, 2));
    CHECK(string_fails("\"a\xf0\x80\x80\x80\"", 7, 2)); /* four for a point of fewer: U+0000, U+FFFF */
    CHECK(string_fails("\"a\xf0\x8f\xbf\xbf\"", 7, 2));
    CHECK(string_fails("\"a\xed\xa0\x80\"", 6, 2));     /* a surrogate in UTF-8: U+D800, U+DFFF */
    CHECK(string_fails("\"a\xed\xbf\xbf\"", 6, 2));
    CHECK(string_fails("\"a\xf4\x90\x80\x80\"", 7, 2)); /* past U+10FFFF */
    CHECK(string_fails("\"a\xf4\xbf\xbf\xbf\"", 7, 2));
    CHECK(string_fails("\"a\xe0\xa0\xc0\"", 6, 2));     /* a later continuation not one */
    CHECK(string_fails("\"a\xf0\x90\x80\x7f\"", 7, 2));
    CHECK(string_fails("\"abc", 4, 0));                 /* no closing quote */
    CHECK(string_fails("\"a\\", 3, 2));                 /* an escape cut by the end */
    CHECK(string_fails("\"a\\ud83d\\", 9, 2));          /* a pair cut by the end */
    CHECK(string_fails("abc", 3, 0));                   /* not a string */
    CHECK(string_fails("", 0, 0));
    /* a string longer than its room: written as far as the room, counted whole */
    {
        dgx_ui_json_t json = reader_of("\"abcdef\"", 8, &copy);
        char          out[4] = {'x', 'x', 'x', 'x'};
        size_t        length;
        CHECK(dgx_ui_json_string(&json, out, 3, &length) && length == 6 && memcmp(out, "abcx", 4) == 0);
        free(copy);
    }

    /* integers: their bounds, the edges of 32 bits, a leading zero, no digit, a fraction, an exponent; the error at the
       token's first byte */
    CHECK(integer_is("0", INT32_MIN, INT32_MAX, DGX_UI_JSON_OK, 0, 0));
    CHECK(integer_is(" -0", INT32_MIN, INT32_MAX, DGX_UI_JSON_OK, 0, 0));
    CHECK(integer_is("2147483647", INT32_MIN, INT32_MAX, DGX_UI_JSON_OK, 0, INT32_MAX));
    CHECK(integer_is("-2147483648", INT32_MIN, INT32_MAX, DGX_UI_JSON_OK, 0, INT32_MIN));
    CHECK(integer_is("2147483648", INT32_MIN, INT32_MAX, DGX_UI_JSON_RANGE, 0, 0));
    CHECK(integer_is("-2147483649", INT32_MIN, INT32_MAX, DGX_UI_JSON_RANGE, 0, 0));
    CHECK(integer_is("99999999999999999999999", INT32_MIN, INT32_MAX, DGX_UI_JSON_RANGE, 0, 0));
    CHECK(integer_is("255", 0, 255, DGX_UI_JSON_OK, 0, 255));
    CHECK(integer_is("  256", 0, 255, DGX_UI_JSON_RANGE, 2, 0));
    CHECK(integer_is("-1", 0, 255, DGX_UI_JSON_RANGE, 0, 0));
    CHECK(integer_is("01", INT32_MIN, INT32_MAX, DGX_UI_JSON_SYNTAX, 0, 0));
    CHECK(integer_is("-", INT32_MIN, INT32_MAX, DGX_UI_JSON_SYNTAX, 0, 0));
    CHECK(integer_is("-a", INT32_MIN, INT32_MAX, DGX_UI_JSON_SYNTAX, 0, 0));
    CHECK(integer_is("+1", INT32_MIN, INT32_MAX, DGX_UI_JSON_SYNTAX, 0, 0));
    CHECK(integer_is(" 1.5", INT32_MIN, INT32_MAX, DGX_UI_JSON_RANGE, 1, 0));
    CHECK(integer_is("1e2", INT32_MIN, INT32_MAX, DGX_UI_JSON_RANGE, 0, 0));
    CHECK(integer_is("1E2", INT32_MIN, INT32_MAX, DGX_UI_JSON_RANGE, 0, 0));
    CHECK(integer_is("", INT32_MIN, INT32_MAX, DGX_UI_JSON_SYNTAX, 0, 0));
    /* the integer ends where its digits do */
    {
        dgx_ui_json_t json = reader_of("12]", 3, &copy);
        int32_t       value;
        CHECK(dgx_ui_json_integer(&json, 0, 99, &value) && value == 12 && json.pos == 2);
        free(copy);
    }

    /* the words, past white space; one cut by the end */
    {
        dgx_ui_json_t json = reader_of(" true false\nnull", 16, &copy);
        CHECK(dgx_ui_json_word(&json, "true") && dgx_ui_json_word(&json, "false") && dgx_ui_json_word(&json, "null") && json.pos == 16);
        free(copy);
        json = reader_of(" tru", 4, &copy);
        CHECK(!dgx_ui_json_word(&json, "true") && json.error == DGX_UI_JSON_SYNTAX && json.error_at == 1);
        free(copy);
        json = reader_of("nul", 3, &copy);
        CHECK(!dgx_ui_json_word(&json, "null") && json.error == DGX_UI_JSON_SYNTAX && json.error_at == 0);
        free(copy);
    }

    /* values passed over: every kind, the empty ones; the malformed at their byte */
    CHECK(skips("{\"a\": [1, -2, true, false, null, {\"b\": \"c\\u00e9\"}, [], {}], \"d\": \"\"}", 3, DGX_UI_JSON_OK, 0));
    CHECK(skips("[]", 0, DGX_UI_JSON_OK, 0));
    CHECK(skips("\"x\"", 0, DGX_UI_JSON_OK, 0));
    CHECK(skips("[1]", 1, DGX_UI_JSON_OK, 0));
    CHECK(skips("[1]", 0, DGX_UI_JSON_DEPTH, 1));
    CHECK(skips("[[[]]]", 2, DGX_UI_JSON_OK, 0));
    CHECK(skips("[[[]]]", 1, DGX_UI_JSON_DEPTH, 2));
    CHECK(skips("{\"a\" 1}", 3, DGX_UI_JSON_SYNTAX, 5));
    CHECK(skips("{1: 2}", 3, DGX_UI_JSON_SYNTAX, 1));
    CHECK(skips("[1,]", 3, DGX_UI_JSON_SYNTAX, 3));
    CHECK(skips("[1;2]", 3, DGX_UI_JSON_SYNTAX, 2));
    CHECK(skips("{\"a\": 1,}", 3, DGX_UI_JSON_SYNTAX, 8));
    CHECK(skips("[", 3, DGX_UI_JSON_SYNTAX, 1));
    CHECK(skips("x", 3, DGX_UI_JSON_SYNTAX, 0));
    CHECK(skips("", 3, DGX_UI_JSON_SYNTAX, 0));
    CHECK(skips("[1.5]", 3, DGX_UI_JSON_RANGE, 1));
    /* ten thousand lists in one another: refused at the levels allowed, the stack untouched past them */
    {
        static char deep[10001];
        memset(deep, '[', 10000);
        CHECK(skips(deep, 50, DGX_UI_JSON_DEPTH, 51));
    }

    /* the first error kept */
    {
        dgx_ui_json_t json = reader_of("[x", 2, &copy);
        CHECK(!dgx_ui_json_skip(&json, 3) && json.error == DGX_UI_JSON_SYNTAX && json.error_at == 1);
        CHECK(!dgx_ui_json_fail(&json, DGX_UI_JSON_RANGE, 0) && json.error == DGX_UI_JSON_SYNTAX && json.error_at == 1);
        free(copy);
    }

    /* every lead and every byte after it, the continuations its length wants after them: the reader takes the string
       exactly when its code points are well formed (a quote, a backslash or a control character, which JSON gives a
       meaning, left out) */
    {
        int disagree = 0, taken = 0;
        for (int lead = 0x20; lead < 0x100; ++lead)
            for (int second = 0x20; second < 0x100; ++second) {
                if (lead == '"' || lead == '\\' || second == '"' || second == '\\') continue;
                const int pad = (lead & 0xe0) == 0xc0 ? 0 : (lead & 0xf0) == 0xe0 ? 1 : (lead & 0xf8) == 0xf0 ? 2 : 0;
                uint8_t   inner[4] = {(uint8_t)lead, (uint8_t)second, 0x80, 0x80};
                char      text[8]  = {'"'};
                memcpy(text + 1, inner, (size_t)(2 + pad));
                text[3 + pad]      = '"';
                dgx_ui_json_t json = reader_of(text, (size_t)(4 + pad), &copy);
                char          out[8];
                size_t        length;
                const bool    read = dgx_ui_json_string(&json, out, sizeof out, &length) && json.pos == json.len;
                const bool    good = well_formed(inner, (size_t)(2 + pad));
                disagree += read != good || (read && (length != (size_t)(2 + pad) || memcmp(out, inner, length) != 0)), taken += read;
                free(copy);
            }
        CHECK(disagree == 0 && taken > 0);
    }

    /* a text of every kind of value, cut at every byte and with every byte changed: never read past its end (its memory
       its exact length), passed over whole or an error within it; whole, the reader at its end */
    {
        static const char text[] = "{\"s\": \"a\\u00e9\\ud83d\\ude00\\n\\\"\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\", \"n\": [-12, 0, 2147483647, -2147483648],"
                                   " \"u\": \"\xe0\xa0\x80\xed\x9f\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf\","
                                   " \"w\": [true, false, null], \"o\": {\"x\": {}, \"y\": [[]]}, \"e\": []}";
        const size_t      full   = sizeof text - 1;
        int               wrong  = 0, errors = 0, whole = 0;
        for (size_t cut = 0; cut <= full; ++cut) {
            dgx_ui_json_t json = reader_of(text, cut, &copy);
            const bool    read = dgx_ui_json_skip(&json, 4);
            if (read) whole += cut == full && json.pos == full, wrong += cut != full;
            else ++errors, wrong += json.error_at > cut || json.pos > cut;
            free(copy);
        }
        CHECK(whole == 1 && errors == (int)full && wrong == 0);
        static const char bytes[] = {'"', '{', '}', '[', ']', ',', ':', '\\', '0', '-', 'u', 'x', ' ', '\0', '\n', (char)0x80, (char)0x8f, (char)0x90, (char)0x9f, (char)0xa0, (char)0xbf, (char)0xc0, (char)0xc3, (char)0xe0, (char)0xed, (char)0xf0, (char)0xf4, (char)0xf5, (char)0xff};
        for (size_t at = 0; at < full; ++at)
            for (size_t b = 0; b < sizeof bytes; ++b) {
                char changed[sizeof text];
                memcpy(changed, text, full);
                changed[at]        = bytes[b];
                dgx_ui_json_t json = reader_of(changed, full, &copy);
                if (!dgx_ui_json_skip(&json, 4)) ++errors, wrong += json.error_at > full || json.pos > full || json.error == DGX_UI_JSON_OK;
                else wrong += json.pos > full;
                free(copy);
            }
        CHECK(errors > (int)full && wrong == 0);
    }

    CHECK_DONE();
}
