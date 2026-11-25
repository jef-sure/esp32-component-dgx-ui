/*
 * dgx_ui_read.c
 *
 *  What the readers of descriptions share (dgx_ui_read.h).
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_read.h"

#include <string.h>

void dgx_ui_read_begin(dgx_ui_read_t *R, const char *json, size_t len, dgx_ui_load_error_t *err)
{
    R->json = (dgx_ui_json_t){.text = json, .len = json ? len : 0}, R->err = err;
    err->code = DGX_UI_LOAD_OK, err->offset = 0, err->name[0] = 0;
}

bool dgx_ui_read_fail(dgx_ui_read_t *R, int code, size_t at, const char *name)
{
    if (R->err->code != DGX_UI_LOAD_OK || R->json.error != DGX_UI_JSON_OK) return false;
    R->err->code = code, R->err->offset = (int)at, R->err->name[0] = 0;
    if (name) {
        size_t k = 0;
        for (; name[k] && k < sizeof R->err->name - 1; ++k) R->err->name[k] = name[k];
        /* a name cut at its 23rd byte ends on a whole character of UTF-8: one it cut is left out */
        size_t last = k; /* the first byte of the last character */
        while (last > 0 && ((uint8_t)name[--last] & 0xc0) == 0x80) {}
        const unsigned lead = (uint8_t)name[last];
        if (k - last < (lead >= 0xf0 ? 4u : lead >= 0xe0 ? 3u : lead >= 0xc0 ? 2u : 1u)) k = last;
        R->err->name[k] = 0;
    }
    return false;
}

bool dgx_ui_read_failed(const dgx_ui_read_t *R)
{
    return R->err->code != DGX_UI_LOAD_OK || R->json.error != DGX_UI_JSON_OK;
}

void dgx_ui_read_reader_error(dgx_ui_read_t *R)
{
    if (R->json.error == DGX_UI_JSON_OK || R->err->code != DGX_UI_LOAD_OK) return;
    R->err->code   = R->json.error == DGX_UI_JSON_RANGE ? DGX_UI_LOAD_RANGE : R->json.error == DGX_UI_JSON_DEPTH ? DGX_UI_LOAD_TOO_DEEP : DGX_UI_LOAD_SYNTAX;
    R->err->offset = (int)R->json.error_at;
}

bool dgx_ui_read_name(dgx_ui_read_t *R, char name[24])
{
    const size_t at = (dgx_ui_json_peek(&R->json), R->json.pos);
    size_t       n;
    if (!dgx_ui_json_string(&R->json, name, 23, &n)) return false;
    name[n < 23 ? n : 23] = 0;
    if (n > 23 || n == 0) return dgx_ui_read_fail(R, DGX_UI_LOAD_NAME_BAD, at, name);
    for (size_t k = 0; k < n; ++k) {
        const char c = name[k];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-'))
            return dgx_ui_read_fail(R, DGX_UI_LOAD_NAME_BAD, at, name);
    }
    return true;
}

int dgx_ui_read_lookup(const void *table, int count, size_t stride, const char *name)
{
    for (int k = 0; table && k < count; ++k)
        if (strcmp(*(const char *const *)((const uint8_t *)table + k * stride), name) == 0) return k;
    return -1;
}

int dgx_ui_read_find(dgx_ui_read_t *R, const void *table, int count, size_t stride)
{
    char         name[24];
    const size_t at = (dgx_ui_json_peek(&R->json), R->json.pos);
    if (!dgx_ui_read_name(R, name)) return -1;
    const int k = dgx_ui_read_lookup(table, count, stride, name);
    if (k < 0) dgx_ui_read_fail(R, DGX_UI_LOAD_UNKNOWN, at, name);
    return k;
}

bool dgx_ui_read_words(dgx_ui_read_t *R, const char *words, uint32_t *bits)
{
    *bits = 0;
    if (!dgx_ui_json_expect(&R->json, '[')) return false;
    if (dgx_ui_json_peek(&R->json) == ']') return ++R->json.pos, true;
    for (bool more = true; more;) {
        char         name[24];
        const size_t at = (dgx_ui_json_peek(&R->json), R->json.pos);
        if (!dgx_ui_read_name(R, name)) return false;
        int         place = 0;
        const char *word  = words;
        while (*word && strcmp(word, name) != 0) word += strlen(word) + 1, ++place;
        if (!*word) return dgx_ui_read_fail(R, DGX_UI_LOAD_UNKNOWN, at, name);
        *bits |= 1u << place;
        if (!dgx_ui_json_next(&R->json, ']', &more)) return false;
    }
    return true;
}

bool dgx_ui_read_top(dgx_ui_read_t *R, const char *const *keys, int count, size_t *at)
{
    size_t version_at = 0, version_from = 0, version_to = 0, unknown_at = 0;
    char   unknown[24];
    for (int k = 0; k < count; ++k) at[k] = 0;
    if (!dgx_ui_json_expect(&R->json, '{')) return false;
    if (dgx_ui_json_peek(&R->json) != '}')
        for (bool more = true; more;) {
            /* a key: any string, one that is no name a key not known, told by its first 23 bytes */
            char   key[24];
            size_t key_length;
            dgx_ui_json_peek(&R->json);
            const size_t here = R->json.pos;
            if (!dgx_ui_json_string(&R->json, key, 23, &key_length) || !dgx_ui_json_expect(&R->json, ':')) return false;
            key[key_length < 23 ? key_length : 23] = 0;
            const size_t value   = (dgx_ui_json_peek(&R->json), R->json.pos);
            const bool   version = strcmp(key, "dgx-ui") == 0 && !version_at;
            int          k       = 0;
            while (k < count && strcmp(keys[k], key) != 0) ++k;
            if (version) version_at = here, version_from = value;
            else if (k < count && !at[k]) at[k] = value;
            else if (!unknown_at) unknown_at = here, strcpy(unknown, key);
            if (!dgx_ui_json_skip(&R->json, DGX_UI_READ_LEVELS - 1)) return false;
            if (version) version_to = R->json.pos;
            if (!dgx_ui_json_next(&R->json, '}', &more)) return false;
        }
    else
        ++R->json.pos;
    if (dgx_ui_json_peek(&R->json) != -1) return dgx_ui_read_fail(R, DGX_UI_LOAD_SYNTAX, R->json.pos, NULL);
    if (!version_at || version_to - version_from != 1 || R->json.text[version_from] != '1') return dgx_ui_read_fail(R, DGX_UI_LOAD_VERSION, version_at, NULL);
    if (unknown_at) return dgx_ui_read_fail(R, DGX_UI_LOAD_KEY, unknown_at, unknown);
    return true;
}
