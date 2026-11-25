/*
 * dgx_ui_appearance.c
 *
 *  An appearance from a description in JSON (dgx_ui_appearance.h), read as a page is (dgx_ui_read.h) and walked twice,
 *  first to check it whole and count what it takes, then to lay it in the program's memory. Its colours are read before
 *  its styles and looks, which name them, wherever they stand in the text; a colour's index is found by its name in the
 *  colours' object of the text again, so that the first walk needs no memory. A look's parameters, a style and a
 *  variant of a font are read by one reader of fields, by their descriptions (dgx_ui_params.c). No recursion: the stack
 *  is the same whatever the text. A program that loads no appearance carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_appearance.h"

#include "dgx_ui_read.h"

#include <string.h>

/* a load of an appearance: the reading of its text; the tables; what the walk counts, or lays */
typedef struct appearance_load_t {
    dgx_ui_read_t                    read;
    const dgx_ui_appearance_names_t *names;
    bool                             build;
    uint8_t                         *memory;
    size_t                           bytes;                          /* counted, or laid so far */
    size_t                           colors_at, styles_at, looks_at; /* where their objects are; 0: none */
    int                              colors, styles, looks;          /* the appearance's own */
    char                             name[24];                       /* its "name"; empty: none */
    dgx_ui_appearance_t              laid;
} appearance_load_t;

static bool fail(appearance_load_t *A, int code, size_t at, const char *name)
{
    return dgx_ui_read_fail(&A->read, code, at, name);
}

static bool failed(const appearance_load_t *A)
{
    return dgx_ui_read_failed(&A->read);
}

/* bytes of the memory taken, aligned; laid when building */
static void *take(appearance_load_t *A, size_t n, size_t align)
{
    A->bytes = (A->bytes + align - 1) / align * align;
    void *at = A->build ? A->memory + A->bytes : NULL;
    A->bytes += n;
    return at;
}

/* a name laid in the memory, so that the text may be freed */
static const char *laid_name(appearance_load_t *A, const char *name)
{
    char *at = take(A, strlen(name) + 1, 1);
    if (at) strcpy(at, name);
    return at;
}

/* the members of an object, one a call: its key in key, where it is in *at, the reader at its value; false at the
   object's end or on an error */
static bool member(appearance_load_t *A, bool *first, char key[24], size_t *at)
{
    dgx_ui_json_t *json = &A->read.json;
    if (*first) {
        *first = false;
        if (!dgx_ui_json_expect(json, '{')) return false;
        if (dgx_ui_json_peek(json) == '}') return ++json->pos, false;
    } else {
        bool more;
        if (!dgx_ui_json_next(json, '}', &more) || !more) return false;
    }
    *at = (dgx_ui_json_peek(json), json->pos);
    return dgx_ui_read_name(&A->read, key) && dgx_ui_json_expect(json, ':');
}

/* the members of the object at a place, counted, the reader where it was */
static int members(appearance_load_t *A, size_t object)
{
    const size_t keep  = A->read.json.pos;
    bool         first = true;
    char         key[24];
    size_t       at;
    int          count = 0;
    A->read.json.pos   = object;
    while (member(A, &first, key, &at) && dgx_ui_json_skip(&A->read.json, DGX_UI_READ_LEVELS)) ++count;
    A->read.json.pos = keep;
    return count;
}

/* ---- the colours ---- */

static const char *const fixed_colors[] = {"paper", "ink", "fill", "border"};

static int hex_digit(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* the index a colour's name has in the colours' object of the text: the four fixed first, the others in their order;
   -1 none. The object is read again, the reader where it was */
static int color_index(appearance_load_t *A, const char *name)
{
    const dgx_ui_json_t keep  = A->read.json;
    bool                first = true;
    char                key[24];
    size_t              at;
    int                 others = 4, found = -1;
    A->read.json.pos           = A->colors_at;
    while (found < 0 && member(A, &first, key, &at)) {
        const int fixed = dgx_ui_read_lookup(fixed_colors, 4, sizeof *fixed_colors, key);
        if (strcmp(key, name) == 0) found = fixed >= 0 ? fixed : others;
        else if (fixed < 0) ++others;
        if (!dgx_ui_json_skip(&A->read.json, 0)) break;
    }
    A->read.json = keep;
    return found;
}

/* a colour's name at the reader into its index; null too, as DGX_UI_PAPER, when the field may take what lies under */
static bool color_of(appearance_load_t *A, bool under, int *index)
{
    char         name[24];
    const size_t at = (dgx_ui_json_peek(&A->read.json), A->read.json.pos);
    if (under && dgx_ui_json_peek(&A->read.json) == 'n') return *index = DGX_UI_PAPER, dgx_ui_json_word(&A->read.json, "null");
    if (!dgx_ui_read_name(&A->read, name)) return false;
    *index = color_index(A, name);
    return *index >= 0 || fail(A, DGX_UI_LOAD_UNKNOWN, at, name);
}

/* "#rrggbb" at the reader, of the colour key */
static bool rgb_of(appearance_load_t *A, const char *key, uint32_t *rgb)
{
    char         text[8];
    size_t       length;
    const size_t at = (dgx_ui_json_peek(&A->read.json), A->read.json.pos);
    if (!dgx_ui_json_string(&A->read.json, text, 7, &length)) return false;
    bool good = length == 7 && text[0] == '#';
    *rgb      = 0;
    for (int k = 1; good && k < 7; ++k) good = hex_digit(text[k]) >= 0, *rgb = *rgb << 4 | (uint32_t)hex_digit(text[k]);
    return good || fail(A, DGX_UI_LOAD_SYNTAX, at, key);
}

/* the colours: checked and counted, then laid, their values and names by index */
static bool colors(appearance_load_t *A)
{
    char     key[24];
    size_t   at;
    uint32_t rgb;
    int      others = 4, seen_fixed = 0;
    A->read.json.pos = A->colors_at;
    for (bool first = true; member(A, &first, key, &at);) {
        if (!rgb_of(A, key, &rgb)) return false;
        const int fixed = dgx_ui_read_lookup(fixed_colors, 4, sizeof *fixed_colors, key);
        if (fixed >= 0 ? (seen_fixed & 1 << fixed) != 0 : color_index(A, key) != others) return fail(A, DGX_UI_LOAD_KEY, at, key); /* said before */
        if (fixed >= 0) seen_fixed |= 1 << fixed;
        else if (++others > 255) return fail(A, DGX_UI_LOAD_RANGE, at, key);
    }
    if (failed(A)) return false;
    for (int k = 0; k < 4; ++k)
        if (!(seen_fixed & 1 << k)) return fail(A, DGX_UI_LOAD_MISSING, A->colors_at, fixed_colors[k]);
    A->colors          = others;
    uint32_t    *value = take(A, sizeof *value * (size_t)others, _Alignof(uint32_t));
    const char **names = take(A, sizeof *names * (size_t)others, _Alignof(const char *));
    others             = 4;
    A->read.json.pos   = A->colors_at;
    for (bool first = true; member(A, &first, key, &at);) {
        rgb_of(A, key, &rgb);
        const int   fixed = dgx_ui_read_lookup(fixed_colors, 4, sizeof *fixed_colors, key);
        const int   index = fixed >= 0 ? fixed : others++;
        const char *name  = laid_name(A, key);
        if (A->build) value[index] = rgb, names[index] = name;
    }
    A->laid.rgb = value, A->laid.color_names = names, A->laid.color_count = (uint16_t)A->colors;
    return !failed(A);
}

/* ---- the fields, of a look's parameters, a style, a variant of a font ---- */

static void put_number(uint8_t *base, const dgx_ui_field_t *field, int32_t value)
{
    if (field->size == 1) base[field->offset] = (uint8_t)value;
    else if (field->size == 2) {
        const int16_t narrow = (int16_t)value;
        memcpy(base + field->offset, &narrow, 2);
    } else {
        memcpy(base + field->offset, &value, 4);
    }
}

static int32_t number_at(const uint8_t *base, const dgx_ui_field_t *field)
{
    int32_t value = 0;
    if (field->size == 1) value = base[field->offset];
    else if (field->size == 2) {
        int16_t narrow;
        memcpy(&narrow, base + field->offset, 2), value = narrow;
    } else {
        memcpy(&value, base + field->offset, 4);
    }
    return value;
}

static bool read_object(appearance_load_t *A, const dgx_ui_params_t *described, uint8_t *base, size_t object);

/* a word of words at the reader: its place */
static bool word_of(appearance_load_t *A, const char *words, int32_t *place)
{
    char         name[24];
    const size_t at = (dgx_ui_json_peek(&A->read.json), A->read.json.pos);
    if (!dgx_ui_read_name(&A->read, name)) return false;
    for (*place = 0; *words; words += strlen(words) + 1, ++*place)
        if (strcmp(words, name) == 0) return true;
    return fail(A, DGX_UI_LOAD_UNKNOWN, at, name);
}

/* a list at the reader, of [below, colour] bands or of variants of a font: checked and counted, then laid, its pointer
   and count written to base */
static bool list_of(appearance_load_t *A, const dgx_ui_field_t *field, uint8_t *base)
{
    dgx_ui_json_t *json  = &A->read.json;
    const bool     bands = field->kind == DGX_UI_FIELD_BANDS;
    const size_t   list  = (dgx_ui_json_peek(json), json->pos);
    int            count = 0, index;
    int32_t        below;
    for (int pass = 0; pass < 2; ++pass) {
        json->pos = list;
        if (!dgx_ui_json_expect(json, '[')) return false;
        dgx_ui_band_t         *band    = pass && bands ? take(A, sizeof *band * (size_t)count, _Alignof(dgx_ui_band_t)) : NULL;
        dgx_ui_font_variant_t *variant = pass && !bands ? take(A, sizeof *variant * (size_t)count, _Alignof(dgx_ui_font_variant_t)) : NULL;
        if (pass && base) {
            void *const laid = bands ? (void *)band : (void *)variant;
            memcpy(base + field->offset, &laid, sizeof laid), base[field->count_offset] = (uint8_t)count;
        }
        if (dgx_ui_json_peek(json) == ']') ++json->pos;
        else
            for (int k = 0, more = 1; more; ++k) {
                bool next;
                if (bands) {
                    if (!dgx_ui_json_expect(json, '[') || !dgx_ui_json_integer(json, INT32_MIN, INT32_MAX, &below) || !dgx_ui_json_expect(json, ',') ||
                        !color_of(A, false, &index) || !dgx_ui_json_expect(json, ']'))
                        return false;
                    if (band) band[k] = (dgx_ui_band_t){.below = below, .color = (uint8_t)index};
                } else {
                    /* a variant: a font's name, or an object of a handwritten one; read into one of its own here, so that
                       the first walk sees its font too */
                    dgx_ui_font_variant_t made = {0};
                    const size_t          at   = (dgx_ui_json_peek(json), json->pos);
                    if (dgx_ui_json_peek(json) == '"') {
                        const int font = dgx_ui_read_find(&A->read, A->names->fonts, A->names->font_count, sizeof *A->names->fonts);
                        if (font < 0) return false;
                        made.font = A->names->fonts[font].font;
                    } else if (!read_object(A, &dgx_ui_params_font_variant, (uint8_t *)&made, at)) {
                        return false;
                    }
                    if (made.font && made.font->f_type == DGX_FONT_HW && !made.hand) return fail(A, DGX_UI_LOAD_MISSING, at, "hand");
                    if (made.points && made.hand) return fail(A, DGX_UI_LOAD_FIELD, at, "points"); /* a handwritten font's pixels are its own */
                    if (variant) variant[k] = made;
                }
                if (!pass && ++count > field->high) return fail(A, DGX_UI_LOAD_RANGE, list, field->name);
                if (!dgx_ui_json_next(json, ']', &next)) return false;
                more = next;
            }
        if (!pass && count < field->low) return fail(A, DGX_UI_LOAD_RANGE, list, field->name);
    }
    return true;
}

/* a field's value at the reader, written to base when there is one; a bit kept in *bits, set after the object */
static bool field_value(appearance_load_t *A, const dgx_ui_field_t *field, uint8_t *base, int32_t *bits)
{
    dgx_ui_json_t *json  = &A->read.json;
    int32_t        value = 0;
    int            index;
    switch (field->kind) {
    case DGX_UI_FIELD_COLOR:
    case DGX_UI_FIELD_COLOR_OR_UNDER:
        if (!color_of(A, field->kind == DGX_UI_FIELD_COLOR_OR_UNDER, &index)) return false;
        value = index;
        break;
    case DGX_UI_FIELD_NUMBER:
        if (!dgx_ui_json_integer(json, field->low, field->high, &value)) return false;
        break;
    case DGX_UI_FIELD_BOOL:
    case DGX_UI_FIELD_BIT:
        value = dgx_ui_json_peek(json) == 't';
        if (!dgx_ui_json_word(json, value ? "true" : "false")) return false;
        if (field->kind == DGX_UI_FIELD_BIT) return *bits |= value ? field->high : 0, true;
        break;
    case DGX_UI_FIELD_WORD:
        if (!word_of(A, field->words, &value)) return false;
        break;
    case DGX_UI_FIELD_FLAGS: {
        uint32_t flags;
        if (!dgx_ui_read_words(&A->read, field->words, &flags)) return false;
        value = (int32_t)flags;
        break;
    }
    case DGX_UI_FIELD_FONT:
    case DGX_UI_FIELD_HAND: {
        const bool  font  = field->kind == DGX_UI_FIELD_FONT;
        const int   entry = font ? dgx_ui_read_find(&A->read, A->names->fonts, A->names->font_count, sizeof *A->names->fonts)
                                 : dgx_ui_read_find(&A->read, A->names->hands, A->names->hand_count, sizeof *A->names->hands);
        const void *laid  = entry < 0 ? NULL : font ? (const void *)A->names->fonts[entry].font : (const void *)A->names->hands[entry].hand;
        if (entry < 0) return false;
        if (base) memcpy(base + field->offset, &laid, sizeof laid);
        return true;
    }
    case DGX_UI_FIELD_PICTURE: {
        /* a name of the program's pictures, of at least `low` there: a count of 0 or 1 is one picture */
        const size_t at    = (dgx_ui_json_peek(json), json->pos);
        const int    entry = dgx_ui_read_find(&A->read, A->names->pictures, A->names->picture_count, sizeof *A->names->pictures);
        if (entry < 0) return false;
        const dgx_ui_picture_name_t *named = &A->names->pictures[entry];
        if ((named->count > 1 ? named->count : 1) < field->low) return fail(A, DGX_UI_LOAD_RANGE, at, field->name);
        if (base) memcpy(base + field->offset, &named->picture, sizeof named->picture);
        return true;
    }
    default: /* bands and variants */
        return list_of(A, field, base);
    }
    if (base) put_number(base, field, value);
    return true;
}

/* the fields that must be said, said, and the bits set: told at the object's place, or the look's when it has none */
static bool fields_said(appearance_load_t *A, const dgx_ui_params_t *described, uint8_t *base, size_t object, uint32_t said, int32_t bits)
{
    const int count = described ? described->field_count : 0;
    for (int k = 0; k < count; ++k) {
        const dgx_ui_field_t *field = &described->fields[k];
        /* a colour, a font, a style's variants and pictures must be said: one forgotten would draw nothing, with no word */
        if (!(said & 1u << k) && (field->kind <= DGX_UI_FIELD_COLOR_OR_UNDER || field->kind == DGX_UI_FIELD_FONT || field->kind == DGX_UI_FIELD_VARIANTS ||
                                  field->kind == DGX_UI_FIELD_PICTURE))
            return fail(A, DGX_UI_LOAD_MISSING, object, field->name);
        if (field->kind == DGX_UI_FIELD_BIT && base) put_number(base, field, number_at(base, field) | (bits & field->high));
    }
    return true;
}

/* an object of fields at a place, by their description: each field once, and those that must be said said */
static bool read_object(appearance_load_t *A, const dgx_ui_params_t *described, uint8_t *base, size_t object)
{
    const int count = described ? described->field_count : 0;
    uint32_t  said  = 0;
    int32_t   bits  = 0;
    bool      first = true;
    char      key[24];
    size_t    at;
    A->read.json.pos = object;
    while (member(A, &first, key, &at)) {
        int k = 0;
        while (k < count && strcmp(described->fields[k].name, key) != 0) ++k;
        if (k == count || (said & 1u << k)) return fail(A, DGX_UI_LOAD_KEY, at, key);
        said |= 1u << k;
        if (!field_value(A, &described->fields[k], base, &bits)) return false;
    }
    return !failed(A) && fields_said(A, described, base, object, said, bits);
}

/* ---- the styles and the looks ---- */

static bool styles(appearance_load_t *A)
{
    const int            own   = A->names->style_count;
    char                 key[24];
    size_t               at;
    A->styles                  = A->styles_at ? members(A, A->styles_at) : 0;
    dgx_ui_style_name_t *table = take(A, sizeof *table * (size_t)(A->styles + own), _Alignof(dgx_ui_style_name_t));
    int                  count = 0;
    A->read.json.pos           = A->styles_at;
    for (bool first = true; A->styles_at && member(A, &first, key, &at); ++count) {
        dgx_ui_text_style_t *style = take(A, sizeof *style, _Alignof(dgx_ui_text_style_t));
        dgx_ui_text_style_t  made  = {0};
        const char          *name  = laid_name(A, key);
        const size_t         value = (dgx_ui_json_peek(&A->read.json), A->read.json.pos);
        if (!read_object(A, &dgx_ui_params_text_style, (uint8_t *)&made, value)) return false;
        if (style) *style = made, table[count] = (dgx_ui_style_name_t){name, style};
    }
    if (failed(A)) return false;
    for (int k = 0; A->build && k < own; ++k) table[count + k] = A->names->styles[k]; /* after the appearance's, which hide them */
    A->laid.styles = table, A->laid.style_count = (uint16_t)(count + own);
    return true;
}

static bool looks(appearance_load_t *A)
{
    static const char *const look_keys[] = {"draw", "fill", "params"};
    const int                own         = A->names->look_count;
    char                     key[24], field[24];
    size_t                   at, field_at;
    A->looks                  = A->looks_at ? members(A, A->looks_at) : 0;
    dgx_ui_look_name_t *table = take(A, sizeof *table * (size_t)(A->looks + own), _Alignof(dgx_ui_look_name_t));
    int                 count = 0;
    A->read.json.pos          = A->looks_at;
    for (bool first = true; A->looks_at && member(A, &first, key, &at); ++count) {
        /* a look as dgx_ui_look_t is: its draw, its fill, its draw's parameters in an object of their own */
        size_t place[3] = {0, 0, 0};
        for (bool inner = true; member(A, &inner, field, &field_at);) {
            const int k = dgx_ui_read_lookup(look_keys, 3, sizeof *look_keys, field);
            if (k < 0 || place[k]) return fail(A, DGX_UI_LOAD_KEY, field_at, field);
            place[k] = (dgx_ui_json_peek(&A->read.json), A->read.json.pos);
            if (!dgx_ui_json_skip(&A->read.json, DGX_UI_READ_LEVELS)) return false;
        }
        if (failed(A)) return false;
        const size_t end = A->read.json.pos;
        int          fill = DGX_UI_PAPER;
        if (!place[0]) return fail(A, DGX_UI_LOAD_MISSING, at, "draw");
        A->read.json.pos = place[0];
        const int draw   = dgx_ui_read_find(&A->read, A->names->draws, A->names->draw_count, sizeof *A->names->draws);
        if (draw < 0) return false;
        if (place[1] && (A->read.json.pos = place[1], !color_of(A, true, &fill))) return false;
        const dgx_ui_params_t *described = A->names->draws[draw].params;
        dgx_ui_look_t         *look      = take(A, sizeof *look, _Alignof(dgx_ui_look_t));
        uint8_t *params = described && described->size ? take(A, described->size, described->align) : NULL;
        const char *name = laid_name(A, key);
        if (params) memset(params, 0, described->size);
        if (place[2] ? !read_object(A, described, params, place[2]) : !fields_said(A, described, params, at, 0, 0)) return false;
        A->read.json.pos = end;
        if (look) {
            *look        = (dgx_ui_look_t){.draw = A->names->draws[draw].draw, .params = params, .fill = (uint8_t)fill,
                                          .behavior = described ? described->behavior : NULL};
            table[count] = (dgx_ui_look_name_t){name, look};
        }
    }
    if (failed(A)) return false;
    for (int k = 0; A->build && k < own; ++k) table[count + k] = A->names->looks[k];
    A->laid.looks = table, A->laid.look_count = (uint16_t)(count + own);
    return true;
}

/* ---- the document ---- */

/* {"dgx-ui": 1, "colors": ..., "styles": ..., "looks": ..., "name": ..., "editor": {...}}, judged in the order of a page's
   (dgx_ui_read_top()); then the name and the editor's object, then the colours, the styles, the looks */
static bool document(appearance_load_t *A)
{
    static const char *const top_keys[] = {"name", "editor", "colors", "styles", "looks"};
    size_t                   place[5];
    if (!dgx_ui_read_top(&A->read, top_keys, 5, place)) return false;
    if (!place[2]) return fail(A, DGX_UI_LOAD_KEY, 0, "colors");
    if (place[0] && (A->read.json.pos = place[0], !dgx_ui_read_name(&A->read, A->name))) return false;
    if (place[1] && A->read.json.text[place[1]] != '{') return fail(A, DGX_UI_LOAD_SYNTAX, place[1], NULL);
    A->colors_at = place[2], A->styles_at = place[3], A->looks_at = place[4];
    return colors(A) && styles(A) && looks(A);
}

static void begin(appearance_load_t *A, const char *json, size_t len, const dgx_ui_appearance_names_t *names, dgx_ui_load_error_t *err)
{
    memset(A, 0, sizeof *A);
    dgx_ui_read_begin(&A->read, json, len, err);
    A->names = names;
}

static bool walk(appearance_load_t *A)
{
    if (document(A)) return true;
    dgx_ui_read_reader_error(&A->read);
    return false;
}

bool dgx_ui_appearance_need(const char *json, size_t len, const dgx_ui_appearance_names_t *names, dgx_ui_appearance_need_t *need,
                            dgx_ui_load_error_t *err)
{
    appearance_load_t A;
    begin(&A, json, len, names, err);
    if (!walk(&A)) return false;
    need->bytes = A.bytes, need->colors = A.colors, need->looks = A.looks, need->styles = A.styles;
    memcpy(need->name, A.name, sizeof need->name);
    return true;
}

bool dgx_ui_appearance_load(const char *json, size_t len, const dgx_ui_appearance_names_t *names, void *memory, size_t size,
                            dgx_ui_appearance_t *out, dgx_ui_load_error_t *err)
{
    /* checked whole and counted, then walked again to lay it: one load, one place on the stack */
    appearance_load_t A;
    begin(&A, json, len, names, err);
    if (!walk(&A)) return false;
    if (size < A.bytes || (uintptr_t)memory % _Alignof(dgx_ui_look_t)) return fail(&A, DGX_UI_LOAD_MEMORY, 0, NULL);
    begin(&A, json, len, names, err);
    A.build = true, A.memory = memory;
    if (!walk(&A)) return false;
    *out = A.laid;
    return true;
}

void dgx_ui_appearance_pack(const dgx_ui_appearance_t *appearance, uint32_t (*pack)(void *ctx, uint8_t r, uint8_t g, uint8_t b), void *ctx,
                            uint32_t *colors)
{
    for (int k = 0; k < appearance->color_count; ++k) {
        const uint32_t rgb = appearance->rgb[k];
        colors[k]          = pack(ctx, (uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb);
    }
}

uint32_t dgx_ui_pack_screen(void *ctx, uint8_t r, uint8_t g, uint8_t b)
{
    return dgx_screen_rgb(ctx, r, g, b);
}

int dgx_ui_appearance_color(const dgx_ui_appearance_t *appearance, const char *name)
{
    return dgx_ui_read_lookup(appearance->color_names, appearance->color_count, sizeof *appearance->color_names, name);
}
