/*
 * dgx_ui_load.c
 *
 *  A tree of widgets from a description in JSON (dgx_ui_load.h): the text read by dgx_ui_json.h, which reads nothing
 *  past its length, and one walk of the description made twice, first to check it whole and count what it takes, then
 *  to lay the widgets in the program's memory and add them. A widget's own keys are read before its children, whatever
 *  their order in the object. The walk is a loop that keeps its places in the load, on the caller's stack, not a
 *  recursion: the stack is the same however deep the text, and two tasks may load at once. A program that loads
 *  nothing carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_load.h"

#include "dgx_ui_load_tables.h"
#include "dgx_ui_read.h"

#include <string.h>

/* a load: the reading of its text and its first error; the tables; what the walk counts, or lays */
typedef struct load_t {
    dgx_ui_read_t              read;
    const dgx_ui_load_names_t *names;
    bool                       build;
    size_t                     bytes; /* counted, or laid so far */
    int                        nodes, depth;
    dgx_ui_t                  *ui;
    uint8_t                   *memory;
    int                       *ids;  /* the index of each id of the table, written to the table once all is added */
    int                        top;  /* the first widget added: removing it removes all the load added */
    uint8_t                    ids_met[DGX_UI_LOAD_MAX_IDS / 8]; /* the table's ids met by the walk: a second is an error */
    char                       tree_name[24];                /* the description's "name"; empty: none */
    struct {
        size_t end;  /* where its object ends: the reader goes on there after its children */
        int    self; /* its index, the parent of its children; -1 counting */
    } open[DGX_UI_LOAD_MAX_DEPTH]; /* the widgets whose children are walked, the root's first */
} load_t;

enum { SKIP_LEVELS = DGX_UI_READ_LEVELS };
enum { MOST_NODES = 32767 }; /* as many widgets as a tree holds: dgx_ui_add() */
/* the flags the numbers set, "50%": none is a word of "flags" */
enum { PERCENTS = DGX_UI_PERCENT_X | DGX_UI_PERCENT_Y | DGX_UI_PERCENT_W | DGX_UI_PERCENT_H | DGX_UI_PERCENT_PAD | DGX_UI_PERCENT_GAP };

/* the first error kept; false always */
static bool fail(load_t *L, int code, size_t at, const char *name)
{
    return dgx_ui_read_fail(&L->read, code, at, name);
}

/* a name, into name[24] */
static bool name_of(load_t *L, char name[24])
{
    return dgx_ui_read_name(&L->read, name);
}

/* a name at the reader found in a table: its entry, or an error */
static int find(load_t *L, const void *table, int count, size_t stride)
{
    return dgx_ui_read_find(&L->read, table, count, stride);
}

/* ---- the widgets ---- */

/* a list of words into bits */
static bool word_list(load_t *L, const char *words, uint32_t *out)
{
    return dgx_ui_read_words(&L->read, words, out);
}

/* a number of points, low_points..INT16_MAX, or a percent, "50%", its flag then set in *flags: -128 % to 127.99 % where
   the points take negatives, 0 % to 127.99 % where they do not */
static bool points_or_percent(load_t *L, int32_t low_points, int16_t *out, uint16_t flag, uint16_t *flags)
{
    int32_t v;
    bool    percent;
    if (!dgx_ui_json_number_or_percent(&L->read.json, low_points, INT16_MAX, low_points < 0 ? -12800 : 0, 12799, &v, &percent)) return false;
    *out = (int16_t)v;
    if (percent) *flags |= flag;
    return true;
}

/* two numbers [a, b] of int16, each of points or a percent */
static bool pair(load_t *L, int16_t *a, int16_t *b, uint16_t flag_a, uint16_t flag_b, uint16_t *flags)
{
    return dgx_ui_json_expect(&L->read.json, '[') && points_or_percent(L, INT16_MIN, a, flag_a, flags) && dgx_ui_json_expect(&L->read.json, ',') &&
           points_or_percent(L, INT16_MIN, b, flag_b, flags) && dgx_ui_json_expect(&L->read.json, ']');
}

/* bytes of the memory taken, aligned; laid when building. A sum past what a size_t holds stays at its most, which no
   memory given is: the load refuses it */
static void *take_bytes(load_t *L, size_t n, size_t align)
{
    if (L->bytes > SIZE_MAX - n - align) {
        L->bytes = SIZE_MAX;
        return NULL;
    }
    L->bytes = (L->bytes + align - 1) / align * align;
    void *at = L->build ? L->memory + L->bytes : NULL;
    L->bytes += n;
    return at;
}

/* a widget's own keys at the reader, under parent, at depth: checked, laid and added when building, the reader past its
   object; its node in *self (-1 counting), the place of its children in *children (0: none) */
static bool widget(load_t *L, int parent, int depth, int *self_out, size_t *children_out)
{
    if (depth > DGX_UI_LOAD_MAX_DEPTH) return fail(L, DGX_UI_LOAD_TOO_DEEP, L->read.json.pos, NULL);
    if (depth > L->depth) L->depth = depth;
    const size_t at = (dgx_ui_json_peek(&L->read.json), L->read.json.pos);
    if (!dgx_ui_json_expect(&L->read.json, '{')) return false;
    dgx_ui_widget_t w = {0};
    uint32_t        seen = 0;
    size_t          key_at[KEY_COUNT] = {0}, text_at = 0, children_at = 0, text_len = 0;
    int32_t         v, text_size = 0, lines = 1;
    uint32_t        bits;
    int             kind = -1, look = -2, style = -1, id = -1, on = -1, picture = -1, context = -1; /* look -2: not said, -1: null */
    bool            ctx_number = false;
    char            id_name[24] = "";
    if (dgx_ui_json_peek(&L->read.json) == '}') ++L->read.json.pos;
    else
        for (bool more = true; more;) {
            char name[24];
            dgx_ui_json_peek(&L->read.json);
            const size_t here = L->read.json.pos;
            if (!name_of(L, name) || !dgx_ui_json_expect(&L->read.json, ':')) return false;
            int k = 0;
            while (k < KEY_COUNT && strcmp(keys[k], name) != 0) ++k;
            if (k == KEY_COUNT || (seen & 1u << k)) return fail(L, DGX_UI_LOAD_KEY, here, name);
            seen |= 1u << k, key_at[k] = here;
            bool ok = true;
            int  e;
            switch (k) {
            case K_KIND: {
                char         word_[24];
                const size_t wat = (dgx_ui_json_peek(&L->read.json), L->read.json.pos);
                ok               = name_of(L, word_);
                if (ok) {
                    kind = 0;
                    while (kind < KIND_COUNT && strcmp(kinds[kind], word_) != 0) ++kind;
                    if (kind == KIND_COUNT) return fail(L, DGX_UI_LOAD_KIND, wat, word_);
                }
                break;
            }
            /* the words of "flags" beside the percents' bits, which the numbers set, whichever is read first */
            case K_FLAGS:
                ok = word_list(L, flag_words, &bits), bits = (bits & ~(uint32_t)FLAG_SCROLL) | (bits & FLAG_SCROLL ? DGX_UI_SCROLL : 0);
                w.flags = (uint16_t)((w.flags & PERCENTS) | bits);
                break;
            case K_STATE: ok = word_list(L, state_words, &bits), w.state.bits = (uint8_t)bits; break;
            case K_PAD: ok = points_or_percent(L, 0, &w.pad, DGX_UI_PERCENT_PAD, &w.flags); break;
            case K_GAP: ok = points_or_percent(L, 0, &w.gap, DGX_UI_PERCENT_GAP, &w.flags); break;
            case K_AT: ok = pair(L, &w.at_x, &w.at_y, DGX_UI_PERCENT_X, DGX_UI_PERCENT_Y, &w.flags); break;
            case K_WANT: ok = pair(L, &w.want_w, &w.want_h, DGX_UI_PERCENT_W, DGX_UI_PERCENT_H, &w.flags); break;
            case K_MIN: ok = dgx_ui_json_integer(&L->read.json, INT16_MIN, INT16_MAX, &v), w.min = (int16_t)v; break;
            case K_MAX: ok = dgx_ui_json_integer(&L->read.json, INT16_MIN, INT16_MAX, &v), w.max = (int16_t)v; break;
            case K_STEP: ok = dgx_ui_json_integer(&L->read.json, INT16_MIN, INT16_MAX, &v), w.step = (int16_t)v; break;
            case K_TAG: ok = dgx_ui_json_integer(&L->read.json, INT16_MIN, INT16_MAX, &v), w.tag = (int16_t)v; break;
            case K_LOOK:
                if (dgx_ui_json_peek(&L->read.json) == 'n') ok = dgx_ui_json_word(&L->read.json, "null"), look = -1;
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
            case K_ON_EVENT:
                ok = (on = find(L, L->names->handlers, L->names->handler_count, sizeof *L->names->handlers)) >= 0;
                if (ok) w.on_event = L->names->handlers[on].on_event;
                break;
            case K_TAKE:
                ok = (e = find(L, L->names->takes, L->names->take_count, sizeof *L->names->takes)) >= 0;
                if (ok) w.take = L->names->takes[e].take;
                break;
            case K_CTX:
                if (dgx_ui_json_peek(&L->read.json) == '"') {
                    ok = (context = find(L, L->names->contexts, L->names->context_count, sizeof *L->names->contexts)) >= 0;
                    if (ok) w.ctx = L->names->contexts[context].ctx;
                } else {
                    ok = dgx_ui_json_integer(&L->read.json, INT32_MIN, INT32_MAX, &v), w.ctx = (void *)(intptr_t)v, ctx_number = true;
                }
                break;
            case K_TEXT: ok = (dgx_ui_json_peek(&L->read.json), text_at = L->read.json.pos, dgx_ui_json_string(&L->read.json, NULL, 0, &text_len)); break;
            case K_TEXT_SIZE: ok = dgx_ui_json_integer(&L->read.json, 1, UINT16_MAX, &text_size); break;
            case K_LINES: ok = dgx_ui_json_integer(&L->read.json, 1, 255, &lines); break;
            case K_ID: ok = name_of(L, id_name), id = 0; break;
            /* the children found and passed over, the editor's object passed over, by the levels the first pass gave */
            case K_CHILDREN: ok = (dgx_ui_json_peek(&L->read.json) == '[' ? (children_at = L->read.json.pos, dgx_ui_json_skip(&L->read.json, SKIP_LEVELS - 2 * depth)) : fail(L, DGX_UI_LOAD_SYNTAX, L->read.json.pos, NULL)); break;
            case K_EDITOR: ok = dgx_ui_json_peek(&L->read.json) == '{' ? dgx_ui_json_skip(&L->read.json, SKIP_LEVELS - 2 * depth) : fail(L, DGX_UI_LOAD_SYNTAX, L->read.json.pos, NULL); break;
            }
            if (!ok || !dgx_ui_json_next(&L->read.json, '}', &more)) return false;
        }
    /* the kind, and each key of a meaning for it */
    if (kind < 0) return fail(L, DGX_UI_LOAD_KIND, at, NULL);
    w.kind = (uint8_t)kind;
    for (int k = 0; k < KEY_COUNT; ++k)
        if ((seen & 1u << k) && !(key_kinds[k] & 1u << kind)) return fail(L, DGX_UI_LOAD_FIELD, key_at[k], keys[k]);
    /* a number for ctx only to a handler that takes one, the library's page action only a page's tree or none;
       pictures no fewer than the kind reads */
    const int takes = on >= 0 ? L->names->handlers[on].ctx_kind : DGX_UI_CTX_OWN;
    if (ctx_number && takes != DGX_UI_CTX_NUMBER) return fail(L, DGX_UI_LOAD_FIELD, key_at[K_CTX], keys[K_CTX]);
    if (takes == DGX_UI_CTX_PAGE && (seen & 1u << K_CTX) && (ctx_number || !L->names->contexts[context].page))
        return fail(L, DGX_UI_LOAD_FIELD, key_at[K_CTX], keys[K_CTX]);
    if (picture >= 0) {
        const int count = L->names->pictures[picture].count ? L->names->pictures[picture].count : 1;
        const int reads = kind == DGX_UI_ICON ? (w.max >= w.min ? w.max - w.min + 1 : 0) : kind == DGX_UI_SWITCH || kind == DGX_UI_RADIO ? 2 : 1;
        if (reads > count) return fail(L, DGX_UI_LOAD_RANGE, key_at[K_PICTURE], L->names->pictures[picture].name);
    }
    /* a scrolling box: a box of a column; its offset and its step a scrolling box's alone */
    const bool scrolls = (w.flags & DGX_UI_SCROLL) != 0;
    if (scrolls && (kind != DGX_UI_BOX || !(w.flags & DGX_UI_COLUMN))) return fail(L, DGX_UI_LOAD_FIELD, key_at[K_FLAGS], "scroll");
    if (kind == DGX_UI_BOX && !scrolls && (seen & (1u << K_VALUE | 1u << K_STEP)))
        return fail(L, DGX_UI_LOAD_FIELD, key_at[seen & 1u << K_VALUE ? K_VALUE : K_STEP], keys[seen & 1u << K_VALUE ? K_VALUE : K_STEP]);
    const bool texted = (seen & (1u << K_TEXT | 1u << K_TEXT_SIZE)) != 0;
    if (!texted && (seen & (1u << K_LINES | 1u << K_STYLE))) return fail(L, DGX_UI_LOAD_FIELD, key_at[seen & 1u << K_LINES ? K_LINES : K_STYLE], NULL);
    if (text_size && text_len + 1 > (size_t)text_size) return fail(L, DGX_UI_LOAD_RANGE, key_at[K_TEXT_SIZE], keys[K_TEXT_SIZE]);
    if (texted && style < 0) {
        if (!L->names->style_count) return fail(L, DGX_UI_LOAD_NO_STYLE, key_at[K_TEXT], NULL);
        style = 0;
    }
    /* the look: the one said, none, or the one of the kind's word; a scrolling box's of the word "scroll" */
    if (look == -2 && (scrolls || !(OWN_LOOKS & 1u << kind))) {
        const char *word = scrolls ? "scroll" : kinds[kind];
        look             = dgx_ui_read_lookup(L->names->looks, L->names->look_count, sizeof *L->names->looks, word);
        if (look < 0) return fail(L, DGX_UI_LOAD_NO_LOOK, at, word);
    }
    if (look >= 0) w.look = L->names->looks[look].look;
    /* an id of the table, met once */
    const int id_entry = id == 0 ? dgx_ui_read_lookup(L->names->ids, L->names->id_count, sizeof *L->names->ids, id_name) : -1;
    if (id_entry >= 0) {
        if (L->ids_met[id_entry / 8] & 1u << id_entry % 8) return fail(L, DGX_UI_LOAD_ID, key_at[K_ID], id_name);
        L->ids_met[id_entry / 8] |= (uint8_t)(1u << id_entry % 8);
    }
    /* laid and added, or counted: no more than a tree holds, refused before any is added */
    if (L->nodes >= MOST_NODES) return fail(L, DGX_UI_LOAD_NODES, at, NULL);
    dgx_ui_widget_t  *laid = take_bytes(L, sizeof w, _Alignof(dgx_ui_widget_t));
    int32_t          *own_offset = scrolls && !w.value ? take_bytes(L, sizeof *own_offset, _Alignof(int32_t)) : NULL; /* its offset, of no place named */
    dgx_ui_textbox_t *box  = texted ? take_bytes(L, sizeof *box, _Alignof(dgx_ui_textbox_t)) : NULL;
    char             *buf  = texted ? take_bytes(L, text_size ? (size_t)text_size : text_len + 1, 1) : NULL;
    ++L->nodes;
    int self = -1;
    if (L->build) {
        if (own_offset) *own_offset = 0, w.value = own_offset;
        if (texted) {
            const dgx_ui_text_style_t *s = L->names->styles[style].style;
            if (text_size) dgx_ui_textbox_init(box, buf, text_size, lines, s);
            else *box = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST(buf, lines, s);
            const size_t end = L->read.json.pos;
            size_t       n   = 0;
            L->read.json.pos      = text_at;
            if (text_at) dgx_ui_json_string(&L->read.json, buf, text_len, &n);
            buf[text_at ? text_len : 0] = 0;
            L->read.json.pos                 = end;
            w.text                      = box;
        }
        *laid = w;
        self  = dgx_ui_add(L->ui, parent, laid);
        if (self < 0) return fail(L, DGX_UI_LOAD_NODES, at, NULL);
        if (L->top < 0) L->top = self;
        if (id_entry >= 0) L->ids[id_entry] = self;
    }
    *self_out = self, *children_out = children_at;
    return true;
}

/* the tree at the reader, widget by widget: a widget's own keys, then its children, then the widgets after it. The
   widgets whose children are walked are kept in L->open, not on the stack: the stack is the same however deep the tree */
static bool tree(load_t *L, int parent, int *index)
{
    int depth = 1;
    for (;;) {
        int    self        = -1; /* widget() gives both where it returns true; given here too, for the compiler */
        size_t children_at = 0;
        if (!widget(L, depth == 1 ? parent : L->open[depth - 2].self, depth, &self, &children_at)) return false;
        if (depth == 1) *index = self;
        if (children_at) { /* its children next, where its object ends kept */
            const size_t end = L->read.json.pos;
            L->read.json.pos      = children_at;
            if (!dgx_ui_json_expect(&L->read.json, '[')) return false;
            if (dgx_ui_json_peek(&L->read.json) != ']') {
                L->open[depth - 1].end = end, L->open[depth - 1].self = self;
                ++depth;
                continue;
            }
            L->read.json.pos = end;
        }
        /* a widget done: the next of its parent's children, or its parent done too */
        for (;;) {
            if (depth == 1) return true;
            bool more;
            if (!dgx_ui_json_next(&L->read.json, ']', &more)) return false;
            if (more) break;
            --depth;
            L->read.json.pos = L->open[depth - 1].end;
        }
    }
}

/* the description: {"dgx-ui": 1, "tree": a widget, "name": the tree's name, "editor": the editor's object}, nothing
   after it, judged in one order (dgx_ui_read_top()): a text that is not JSON; then a version that is not the integer 1;
   then a key of the top unknown or twice, and the tree missing; then the values of the keys known, the name, the
   editor's, the tree. So that a description of a later version is refused as one, whatever it holds */
static bool document(load_t *L, int parent, int *index)
{
    static const char *const top_keys[] = {"name", "editor", "tree"};
    size_t                   at[3];
    if (!dgx_ui_read_top(&L->read, top_keys, 3, at)) return false;
    if (!at[2]) return fail(L, DGX_UI_LOAD_KEY, 0, "tree");
    if (at[0] && (L->read.json.pos = at[0], !name_of(L, L->tree_name))) return false;
    if (at[1] && L->read.json.text[at[1]] != '{') return fail(L, DGX_UI_LOAD_SYNTAX, at[1], NULL);
    if (L->read.json.text[at[2]] != '{') return fail(L, DGX_UI_LOAD_SYNTAX, at[2], NULL);
    L->read.json.pos = at[2];
    return tree(L, parent, index);
}

/* the walk of a load: first the place of the ids' indices, then the widgets */
static bool walk(load_t *L, int parent, int *index)
{
    if (L->names->id_count > DGX_UI_LOAD_MAX_IDS) return fail(L, DGX_UI_LOAD_ID, 0, NULL);
    L->ids = take_bytes(L, (size_t)L->names->id_count * sizeof(int), _Alignof(int));
    for (int k = 0; L->build && k < L->names->id_count; ++k) L->ids[k] = -1;
    if (document(L, parent, index)) return true;
    dgx_ui_read_reader_error(&L->read);
    return false;
}

/* a load begun: its text, its tables, no error */
static void begin(load_t *L, const char *json, size_t len, const dgx_ui_load_names_t *names, dgx_ui_load_error_t *err)
{
    memset(L, 0, sizeof *L);
    dgx_ui_read_begin(&L->read, json, len, err);
    L->names = names, L->top = -1;
}

bool dgx_ui_load_need(const char *json, size_t len, const dgx_ui_load_names_t *names, dgx_ui_load_need_t *need, dgx_ui_load_error_t *err)
{
    load_t L;
    int    index;
    begin(&L, json, len, names, err);
    if (!walk(&L, -1, &index)) return false;
    need->bytes = L.bytes, need->nodes = L.nodes, need->depth = L.depth;
    memcpy(need->name, L.tree_name, sizeof need->name);
    return true;
}

int dgx_ui_load(dgx_ui_t *ui, int parent, const char *json, size_t len, const dgx_ui_load_names_t *names, void *memory, size_t size, dgx_ui_load_error_t *err)
{
    /* the description checked whole and counted, then walked again to lay and add it: one load, so one place on the stack */
    load_t L;
    int    index = -1;
    begin(&L, json, len, names, err);
    if (!walk(&L, -1, &index)) return -1;
    if (size < L.bytes || (uintptr_t)memory % _Alignof(dgx_ui_widget_t)) return fail(&L, DGX_UI_LOAD_MEMORY, 0, NULL), -1;
    begin(&L, json, len, names, err);
    L.build = true, L.ui = ui, L.memory = memory;
    if (!walk(&L, parent, &index)) { /* an add refused midway: what was added removed, the ids as they were */
        if (L.top >= 0) dgx_ui_remove(ui, L.top);
        return -1;
    }
    for (int k = 0; k < names->id_count; ++k)
        if (L.ids[k] >= 0) *names->ids[k].index = L.ids[k];
    return index;
}
