/*
 * dgx_ui_load.c
 *
 *  A tree of widgets from a description in JSON (dgx_ui_load.h): the text read by dgx_ui_json.h, which reads nothing
 *  past its length, and one walk of the description made twice, first to check it whole and count what
 *  it takes, then to lay the widgets in the program's memory and add them. A widget's own keys are read before its
 *  children, whatever their order in the object. A program that loads nothing carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_load.h"

#include "dgx_ui_json.h"

#include <string.h>

/* a load: the reader of its text; the tables; the first error; what the walk counts, or lays */
typedef struct load_t {
    dgx_ui_json_t              json;
    const dgx_ui_load_names_t *names;
    dgx_ui_load_error_t       *err;
    bool                       build;
    size_t                     bytes; /* counted, or laid so far */
    int                        nodes, depth;
    dgx_ui_t                  *ui;
    uint8_t                   *memory;
    int                       *ids;  /* the index of each id of the table, written to the table once all is added */
    int                        top;  /* the first widget added: removing it removes all the load added */
    uint8_t                    ids_met[DGX_UI_LOAD_IDS / 8]; /* the table's ids met by the walk: a second is an error */
    char                       tree_name[24];                /* the description's "name"; empty: none */
} load_t;

/* the most levels a value passed over goes in: a widget's object and its list of children two each */
enum { SKIP_LEVELS = 2 * DGX_UI_LOAD_DEPTH + 2 };

/* the first error kept, the reader's or the load's; false always */
static bool fail(load_t *L, int code, size_t at, const char *name)
{
    if (L->err->code != DGX_UI_LOAD_OK || L->json.error != DGX_UI_JSON_OK) return false;
    L->err->code = code, L->err->offset = (int)at, L->err->name[0] = 0;
    if (name) {
        size_t k = 0;
        for (; name[k] && k < sizeof L->err->name - 1; ++k) L->err->name[k] = name[k];
        L->err->name[k] = 0;
    }
    return false;
}

/* ---- the names ---- */

/* a name: a string of letters, digits, '_', '.' and '-', at most 23, into name[24] */
static bool name_of(load_t *L, char name[24])
{
    const size_t at = (dgx_ui_json_peek(&L->json), L->json.pos);
    size_t       n;
    if (!dgx_ui_json_string(&L->json, name, 23, &n)) return false;
    name[n < 23 ? n : 23] = 0;
    if (n > 23 || n == 0) return fail(L, DGX_UI_LOAD_NAME_BAD, at, name);
    for (size_t k = 0; k < n; ++k) {
        const char c = name[k];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-'))
            return fail(L, DGX_UI_LOAD_NAME_BAD, at, name);
    }
    return true;
}

/* the entry of a table of `count` entries of `stride` bytes, each beginning with its name; -1 none */
static int lookup(const void *table, int count, size_t stride, const char *name)
{
    for (int k = 0; table && k < count; ++k)
        if (strcmp(*(const char *const *)((const uint8_t *)table + k * stride), name) == 0) return k;
    return -1;
}

/* a name at the reader found in a table: its entry, or an error */
static int find(load_t *L, const void *table, int count, size_t stride)
{
    char         name[24];
    const size_t at = (dgx_ui_json_peek(&L->json), L->json.pos);
    if (!name_of(L, name)) return -1;
    const int k = lookup(table, count, stride, name);
    if (k < 0) fail(L, DGX_UI_LOAD_UNKNOWN, at, name);
    return k;
}

/* ---- the widgets ---- */

static const char *const kinds[] = {"box", "frame", "label", "picture", "icon", "list", "switch", "slider", "button", "input", "dropdown", "own", "radio"};
enum { KIND_COUNT = sizeof kinds / sizeof *kinds };

static const char *const flag_words[] = {"column", "row", "stretch", "swipe", "drag", "canvas", "rest", "repeat"};
static const uint8_t     flag_bits[]  = {DGX_UI_COLUMN, DGX_UI_ROW, DGX_UI_STRETCH, DGX_UI_SWIPE, DGX_UI_DRAG, DGX_UI_CANVAS, DGX_UI_REST, DGX_UI_REPEAT};

/* the keys of a widget, and the kinds each means something for */
enum { K_KIND, K_FLAGS, K_STATE, K_PAD, K_GAP, K_AT, K_WANT, K_MIN, K_MAX, K_STEP, K_TAG, K_LOOK, K_VALUE, K_PICTURE, K_STYLE, K_ON, K_TAKE, K_CTX, K_TEXT, K_TEXT_SIZE, K_LINES, K_ID, K_CHILDREN, KEY_COUNT };
static const char *const keys[KEY_COUNT] = {"kind", "flags", "state", "pad", "gap", "at", "want", "min", "max", "step", "tag", "look", "value", "picture", "style", "on", "take", "ctx", "text", "text_size", "lines", "id", "children"};
enum {
    ALL       = 0x1fff,
    VALUED    = 1u << DGX_UI_LIST | 1u << DGX_UI_SWITCH | 1u << DGX_UI_SLIDER | 1u << DGX_UI_DROPDOWN | 1u << DGX_UI_OWN | 1u << DGX_UI_RADIO | 1u << DGX_UI_ICON,
    TEXTED    = 1u << DGX_UI_LABEL | 1u << DGX_UI_BUTTON | 1u << DGX_UI_LIST | 1u << DGX_UI_INPUT | 1u << DGX_UI_DROPDOWN | 1u << DGX_UI_RADIO | 1u << DGX_UI_SWITCH,
    PICTURED  = 1u << DGX_UI_PICTURE | 1u << DGX_UI_ICON | 1u << DGX_UI_BUTTON | 1u << DGX_UI_SWITCH | 1u << DGX_UI_RADIO,
    HOLDING   = 1u << DGX_UI_BOX | 1u << DGX_UI_FRAME | 1u << DGX_UI_OWN,
    OWN_LOOKS = 1u << DGX_UI_BOX | 1u << DGX_UI_FRAME | 1u << DGX_UI_LABEL, /* the core's own look when none is said */
};
static const uint16_t key_kinds[KEY_COUNT] = {ALL,    ALL,    ALL,    HOLDING, HOLDING,  ALL,    ALL,    VALUED, VALUED, VALUED, ALL, ALL,
                                              VALUED, PICTURED, TEXTED, ALL,   ALL,      ALL,    TEXTED, TEXTED, TEXTED, ALL, HOLDING};

/* a list of words of a table of words into bits */
static bool word_list(load_t *L, const char *const *words, const uint8_t *bits, int count, uint8_t *out)
{
    *out = 0;
    if (!dgx_ui_json_expect(&L->json, '[')) return false;
    if (dgx_ui_json_peek(&L->json) == ']') return ++L->json.pos, true;
    for (bool more = true; more;) {
        char         name[24];
        const size_t at = (dgx_ui_json_peek(&L->json), L->json.pos);
        if (!name_of(L, name)) return false;
        int k = 0;
        while (k < count && strcmp(words[k], name) != 0) ++k;
        if (k == count) return fail(L, DGX_UI_LOAD_UNKNOWN, at, name);
        *out |= bits[k];
        if (!dgx_ui_json_next(&L->json, ']', &more)) return false;
    }
    return true;
}

/* two integers [a, b] of int16 */
static bool pair(load_t *L, int16_t *a, int16_t *b)
{
    int32_t x, y;
    if (!dgx_ui_json_expect(&L->json, '[') || !dgx_ui_json_integer(&L->json, INT16_MIN, INT16_MAX, &x) || !dgx_ui_json_expect(&L->json, ',') || !dgx_ui_json_integer(&L->json, INT16_MIN, INT16_MAX, &y) || !dgx_ui_json_expect(&L->json, ']')) return false;
    *a = (int16_t)x, *b = (int16_t)y;
    return true;
}

/* bytes of the memory taken, aligned; laid when building */
static void *take_bytes(load_t *L, size_t n, size_t align)
{
    L->bytes = (L->bytes + align - 1) / align * align;
    void *at = L->build ? L->memory + L->bytes : NULL;
    L->bytes += n;
    return at;
}

/* a widget at the reader, under parent, at depth: its own keys first, then its children; *index its node, building */
static bool widget(load_t *L, int parent, int depth, int *index)
{
    if (depth > DGX_UI_LOAD_DEPTH) return fail(L, DGX_UI_LOAD_DEPTH_MAX, L->json.pos, NULL);
    if (depth > L->depth) L->depth = depth;
    const size_t at = (dgx_ui_json_peek(&L->json), L->json.pos);
    if (!dgx_ui_json_expect(&L->json, '{')) return false;
    dgx_ui_widget_t w = {0};
    uint32_t        seen = 0;
    size_t          key_at[KEY_COUNT] = {0}, text_at = 0, children_at = 0, text_len = 0;
    int32_t         v, text_size = 0, lines = 1;
    int             kind = -1, look = -2, style = -1, id = -1, on = -1, picture = -1; /* look -2: not said, -1: null */
    bool            ctx_number = false;
    char            id_name[24] = "";
    if (dgx_ui_json_peek(&L->json) == '}') ++L->json.pos;
    else
        for (bool more = true; more;) {
            char name[24];
            dgx_ui_json_peek(&L->json);
            const size_t here = L->json.pos;
            if (!name_of(L, name) || !dgx_ui_json_expect(&L->json, ':')) return false;
            int k = 0;
            while (k < KEY_COUNT && strcmp(keys[k], name) != 0) ++k;
            if (k == KEY_COUNT || (seen & 1u << k)) return fail(L, DGX_UI_LOAD_KEY, here, name);
            seen |= 1u << k, key_at[k] = here;
            bool ok = true;
            int  e;
            switch (k) {
            case K_KIND: {
                char         word_[24];
                const size_t wat = (dgx_ui_json_peek(&L->json), L->json.pos);
                ok               = name_of(L, word_);
                if (ok) {
                    kind = 0;
                    while (kind < KIND_COUNT && strcmp(kinds[kind], word_) != 0) ++kind;
                    if (kind == KIND_COUNT) return fail(L, DGX_UI_LOAD_KIND, wat, word_);
                }
                break;
            }
            case K_FLAGS: ok = word_list(L, flag_words, flag_bits, 8, &w.flags); break;
            case K_STATE: {
                static const char *const state_words[] = {"hidden", "disabled"};
                static const uint8_t     state_bits[]  = {DGX_UI_HIDDEN, DGX_UI_DISABLED};
                ok                                    = word_list(L, state_words, state_bits, 2, &w.state.bits);
                break;
            }
            case K_PAD: ok = dgx_ui_json_integer(&L->json, 0, 255, &v), w.pad = (uint8_t)v; break;
            case K_GAP: ok = dgx_ui_json_integer(&L->json, 0, 255, &v), w.gap = (uint8_t)v; break;
            case K_AT: ok = pair(L, &w.at_x, &w.at_y); break;
            case K_WANT: ok = pair(L, &w.want_w, &w.want_h); break;
            case K_MIN: ok = dgx_ui_json_integer(&L->json, INT16_MIN, INT16_MAX, &v), w.min = (int16_t)v; break;
            case K_MAX: ok = dgx_ui_json_integer(&L->json, INT16_MIN, INT16_MAX, &v), w.max = (int16_t)v; break;
            case K_STEP: ok = dgx_ui_json_integer(&L->json, INT16_MIN, INT16_MAX, &v), w.step = (int16_t)v; break;
            case K_TAG: ok = dgx_ui_json_integer(&L->json, INT16_MIN, INT16_MAX, &v), w.tag = (int16_t)v; break;
            case K_LOOK:
                if (dgx_ui_json_peek(&L->json) == 'n') ok = dgx_ui_json_word(&L->json, "null"), look = -1;
                else ok = (look = find(L, L->names->looks, L->names->look_count, sizeof *L->names->looks)) >= 0;
                break;
            case K_VALUE:
                ok = (e = find(L, L->names->values, L->names->value_count, sizeof *L->names->values)) >= 0;
                if (ok) w.value = L->names->values[e].value;
                break;
            case K_PICTURE:
                ok = (picture = find(L, L->names->pictures, L->names->picture_count, sizeof *L->names->pictures)) >= 0;
                if (ok) w.picture = L->names->pictures[picture].picture;
                break;
            case K_STYLE: ok = (style = find(L, L->names->styles, L->names->style_count, sizeof *L->names->styles)) >= 0; break;
            case K_ON:
                ok = (on = find(L, L->names->handlers, L->names->handler_count, sizeof *L->names->handlers)) >= 0;
                if (ok) w.on = L->names->handlers[on].on;
                break;
            case K_TAKE:
                ok = (e = find(L, L->names->takes, L->names->take_count, sizeof *L->names->takes)) >= 0;
                if (ok) w.take = L->names->takes[e].take;
                break;
            case K_CTX:
                if (dgx_ui_json_peek(&L->json) == '"') {
                    ok = (e = find(L, L->names->contexts, L->names->context_count, sizeof *L->names->contexts)) >= 0;
                    if (ok) w.ctx = L->names->contexts[e].ctx;
                } else {
                    ok = dgx_ui_json_integer(&L->json, INT32_MIN, INT32_MAX, &v), w.ctx = (void *)(intptr_t)v, ctx_number = true;
                }
                break;
            case K_TEXT: ok = (dgx_ui_json_peek(&L->json), text_at = L->json.pos, dgx_ui_json_string(&L->json, NULL, 0, &text_len)); break;
            case K_TEXT_SIZE: ok = dgx_ui_json_integer(&L->json, 1, UINT16_MAX, &text_size); break;
            case K_LINES: ok = dgx_ui_json_integer(&L->json, 1, 255, &lines); break;
            case K_ID: ok = name_of(L, id_name), id = 0; break;
            case K_CHILDREN: ok = (dgx_ui_json_peek(&L->json) == '[' ? (children_at = L->json.pos, dgx_ui_json_skip(&L->json, SKIP_LEVELS - depth)) : fail(L, DGX_UI_LOAD_SYNTAX, L->json.pos, NULL)); break;
            }
            if (!ok || !dgx_ui_json_next(&L->json, '}', &more)) return false;
        }
    /* the kind, and each key of a meaning for it */
    if (kind < 0) return fail(L, DGX_UI_LOAD_KIND, at, NULL);
    w.kind = (uint8_t)kind;
    for (int k = 0; k < KEY_COUNT; ++k)
        if ((seen & 1u << k) && !(key_kinds[k] & 1u << kind)) return fail(L, DGX_UI_LOAD_FIELD, key_at[k], keys[k]);
    /* a number for ctx only to a handler that takes one; pictures no fewer than the kind reads */
    if (ctx_number && (on < 0 || !L->names->handlers[on].takes_number)) return fail(L, DGX_UI_LOAD_FIELD, key_at[K_CTX], keys[K_CTX]);
    if (picture >= 0) {
        const int count = L->names->pictures[picture].count ? L->names->pictures[picture].count : 1;
        const int reads = kind == DGX_UI_ICON ? (w.max >= w.min ? w.max - w.min + 1 : 0) : kind == DGX_UI_SWITCH || kind == DGX_UI_RADIO ? 2 : 1;
        if (reads > count) return fail(L, DGX_UI_LOAD_RANGE, key_at[K_PICTURE], L->names->pictures[picture].name);
    }
    const bool texted = (seen & (1u << K_TEXT | 1u << K_TEXT_SIZE)) != 0;
    if (!texted && (seen & (1u << K_LINES | 1u << K_STYLE))) return fail(L, DGX_UI_LOAD_FIELD, key_at[seen & 1u << K_LINES ? K_LINES : K_STYLE], NULL);
    if (text_size && text_len + 1 > (size_t)text_size) return fail(L, DGX_UI_LOAD_RANGE, key_at[K_TEXT_SIZE], keys[K_TEXT_SIZE]);
    if (texted && style < 0) {
        if (!L->names->style_count) return fail(L, DGX_UI_LOAD_NO_STYLE, key_at[K_TEXT], NULL);
        style = 0;
    }
    /* the look: the one said, none, or the one of the kind's word */
    if (look == -2 && !(OWN_LOOKS & 1u << kind)) {
        look = lookup(L->names->looks, L->names->look_count, sizeof *L->names->looks, kinds[kind]);
        if (look < 0) return fail(L, DGX_UI_LOAD_NO_LOOK, at, kinds[kind]);
    }
    if (look >= 0) w.look = L->names->looks[look].look;
    /* an id of the table, met once */
    const int id_entry = id == 0 ? lookup(L->names->ids, L->names->id_count, sizeof *L->names->ids, id_name) : -1;
    if (id_entry >= 0) {
        if (L->ids_met[id_entry / 8] & 1u << id_entry % 8) return fail(L, DGX_UI_LOAD_ID, key_at[K_ID], id_name);
        L->ids_met[id_entry / 8] |= (uint8_t)(1u << id_entry % 8);
    }
    /* laid and added, or counted */
    dgx_ui_widget_t  *laid = take_bytes(L, sizeof w, _Alignof(dgx_ui_widget_t));
    dgx_ui_textbox_t *box  = texted ? take_bytes(L, sizeof *box, _Alignof(dgx_ui_textbox_t)) : NULL;
    char             *buf  = texted ? take_bytes(L, text_size ? (size_t)text_size : text_len + 1, 1) : NULL;
    ++L->nodes;
    int self = -1;
    if (L->build) {
        if (texted) {
            const dgx_ui_text_style_t *s = L->names->styles[style].style;
            if (text_size) dgx_ui_textbox_init(box, buf, text_size, lines, s);
            else *box = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST(buf, lines, s);
            const size_t end = L->json.pos;
            size_t       n   = 0;
            L->json.pos      = text_at;
            if (text_at) dgx_ui_json_string(&L->json, buf, text_len, &n);
            buf[text_at ? text_len : 0] = 0;
            L->json.pos                 = end;
            w.text                      = box;
        }
        *laid = w;
        self  = dgx_ui_add(L->ui, parent, laid);
        if (self < 0) return fail(L, DGX_UI_LOAD_NODES, at, NULL);
        if (L->top < 0) L->top = self;
        if (id_entry >= 0) L->ids[id_entry] = self;
        *index = self;
    }
    /* the children, after the widget */
    if (children_at) {
        const size_t end = L->json.pos;
        L->json.pos      = children_at;
        if (!dgx_ui_json_expect(&L->json, '[')) return false;
        if (dgx_ui_json_peek(&L->json) == ']') ++L->json.pos;
        else
            for (bool more = true; more;) {
                int child;
                if (!widget(L, self, depth + 1, &child) || !dgx_ui_json_next(&L->json, ']', &more)) return false;
            }
        L->json.pos = end;
    }
    return true;
}

/* the description: {"dgx-ui": 1, "tree": a widget, "name": the tree's name}, nothing after it. A key of the top this
   version does not know is told after the version, so that a description of a later version is refused as one */
static bool document(load_t *L, int parent, int *index)
{
    size_t  tree_at = 0, version_at = 0, name_at = 0, unknown_at = 0;
    char    unknown[24];
    int32_t version = 0;
    if (!dgx_ui_json_expect(&L->json, '{')) return false;
    if (dgx_ui_json_peek(&L->json) != '}')
        for (bool more = true; more;) {
            char key[24];
            dgx_ui_json_peek(&L->json);
            const size_t here = L->json.pos;
            if (!name_of(L, key) || !dgx_ui_json_expect(&L->json, ':')) return false;
            if (strcmp(key, "dgx-ui") == 0 && !version_at) {
                version_at = here;
                if (!dgx_ui_json_integer(&L->json, INT32_MIN, INT32_MAX, &version)) return false;
            } else if (strcmp(key, "tree") == 0 && !tree_at) {
                if (dgx_ui_json_peek(&L->json) != '{') return fail(L, DGX_UI_LOAD_SYNTAX, L->json.pos, NULL);
                tree_at = L->json.pos;
                if (!dgx_ui_json_skip(&L->json, SKIP_LEVELS - 1)) return false;
            } else if (strcmp(key, "name") == 0 && !name_at) {
                name_at = here;
                if (!name_of(L, L->tree_name)) return false;
            } else {
                if (!unknown_at) unknown_at = here, strcpy(unknown, key);
                if (!dgx_ui_json_skip(&L->json, SKIP_LEVELS - 1)) return false;
            }
            if (!dgx_ui_json_next(&L->json, '}', &more)) return false;
        }
    else
        ++L->json.pos;
    if (dgx_ui_json_peek(&L->json) != -1) return fail(L, DGX_UI_LOAD_SYNTAX, L->json.pos, NULL);
    if (!version_at || version != 1) return fail(L, DGX_UI_LOAD_VERSION, version_at, NULL);
    if (unknown_at) return fail(L, DGX_UI_LOAD_KEY, unknown_at, unknown);
    if (!tree_at) return fail(L, DGX_UI_LOAD_KEY, 0, "tree");
    L->json.pos = tree_at;
    return widget(L, parent, 1, index);
}

/* the walk of a load: first the place of the ids' indices, then the widgets */
static bool walk(load_t *L, int parent, int *index)
{
    if (L->names->id_count > DGX_UI_LOAD_IDS) return fail(L, DGX_UI_LOAD_ID, 0, NULL);
    L->ids = take_bytes(L, (size_t)L->names->id_count * sizeof(int), _Alignof(int));
    for (int k = 0; L->build && k < L->names->id_count; ++k) L->ids[k] = -1;
    if (document(L, parent, index)) return true;
    if (L->json.error != DGX_UI_JSON_OK && L->err->code == DGX_UI_LOAD_OK) { /* the reader's error, in the load's words */
        const int code = L->json.error == DGX_UI_JSON_RANGE ? DGX_UI_LOAD_RANGE : L->json.error == DGX_UI_JSON_DEPTH ? DGX_UI_LOAD_DEPTH_MAX : DGX_UI_LOAD_SYNTAX;
        L->err->code = code, L->err->offset = (int)L->json.error_at;
    }
    return false;
}

bool dgx_ui_load_need(const char *json, size_t len, const dgx_ui_load_names_t *names, dgx_ui_load_need_t *need, dgx_ui_load_error_t *err)
{
    load_t L = {.json = {.text = json, .len = json ? len : 0}, .names = names, .err = err, .top = -1};
    int    index;
    err->code = DGX_UI_LOAD_OK, err->offset = 0, err->name[0] = 0;
    if (!walk(&L, -1, &index)) return false;
    need->bytes = L.bytes, need->nodes = L.nodes, need->depth = L.depth;
    memcpy(need->name, L.tree_name, sizeof need->name);
    return true;
}

int dgx_ui_load(dgx_ui_t *ui, int parent, const char *json, size_t len, const dgx_ui_load_names_t *names, void *memory, size_t size, dgx_ui_load_error_t *err)
{
    dgx_ui_load_need_t need;
    if (!dgx_ui_load_need(json, len, names, &need, err)) return -1;
    if (size < need.bytes || (uintptr_t)memory % _Alignof(dgx_ui_widget_t)) return fail(&(load_t){.err = err}, DGX_UI_LOAD_MEMORY, 0, NULL), -1;
    load_t L     = {.json = {.text = json, .len = len}, .names = names, .err = err, .build = true, .ui = ui, .memory = memory, .top = -1};
    int    index = -1;
    if (!walk(&L, parent, &index)) { /* an add refused midway: what was added removed, the ids as they were */
        if (L.top >= 0) dgx_ui_remove(ui, L.top);
        return -1;
    }
    for (int k = 0; k < names->id_count; ++k)
        if (L.ids[k] >= 0) *names->ids[k].index = L.ids[k];
    return index;
}
