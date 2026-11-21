/*
 * An appearance from a description in JSON, dgx_ui_appearance_load(): its colours, the four fixed wherever they stand
 * and the others in their order; a look of every draw of the library, its parameters laid by their description; styles
 * of bitmap and handwritten variants; the tables its own first, the program's after, a page loaded on them taking the
 * appearance's look and style where both have the name and the program's where only it has; the need's bytes exactly
 * what the load uses; the text overwritten after the load, the appearance as it was; two packers on one appearance;
 * a button of the appearance drawn as the same button of C constants, pixel by pixel; the whole appearance written as
 * dgx_ui_c is to make it, constants and its styles' fonts filled at the start, the same field by field, and a page of
 * all its looks drawn on its tables as on the loaded ones, pixel by pixel. Each refusal with its code, its
 * byte and its name. Appearances made by chance from the descriptions themselves, every kind of field, their keys
 * shuffled, loaded and compared byte by byte; the same spoiled at one byte: never a fault, an error inside the text.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_appearance.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"

#define DRAW(word) DGX_UI_DRAW_NAME(word)
static const dgx_ui_draw_name_t draws[] = {DRAW(box),      DRAW(frame),         DRAW(text),  DRAW(lines),  DRAW(hand_lines), DRAW(button),  DRAW(picture_button),
                                           DRAW(input),    DRAW(switch),        DRAW(slider), DRAW(list),  DRAW(dropdown),   DRAW(picture), DRAW(rgb565),
                                           DRAW(indexed),  DRAW(radio),         DRAW(checkbox), DRAW(level), DRAW(ring)};
enum { DRAW_COUNT = sizeof draws / sizeof *draws };
static const dgx_ui_font_name_t        fonts[]      = {{"small", TerminusTTFMedium12}, {"hand", font0ss}};
static const dgx_ui_hand_name_t        hands[]      = {DGX_UI_HAND_NAME(handwriting), DGX_UI_HAND_NAME(handwriting_canvas)};
static const dgx_ui_look_name_t        own_looks[]  = {DGX_UI_LOOK_NAME(slider), DGX_UI_LOOK_NAME(switch)};
static dgx_ui_text_style_t             plain;
static const dgx_ui_style_name_t       own_styles[] = {{"plain", &plain}};
static const dgx_ui_appearance_names_t names        = {.draws = draws, .draw_count = DRAW_COUNT, .fonts = fonts, .font_count = 2, .hands = hands, .hand_count = 2,
                                                       .looks = own_looks, .look_count = 2, .styles = own_styles, .style_count = 1};

static bool refused(const char *json, int code, int offset, const char *name)
{
    dgx_ui_appearance_need_t need;
    dgx_ui_load_error_t      err;
    const bool               taken = dgx_ui_appearance_need(json, strlen(json), &names, &need, &err);
    const bool right = !taken && err.code == code && (offset < 0 || err.offset == offset) && (!name || strcmp(err.name, name) == 0);
    if (!right) fprintf(stderr, "  %.200s: code %d offset %d name \"%s\", expected %d %d \"%s\"\n", json, taken ? 0 : err.code, err.offset, err.name, code, offset, name ? name : "");
    return right;
}

static uint32_t as_is(void *ctx, uint8_t r, uint8_t g, uint8_t b)
{
    return (uint32_t)r << 16 | (uint32_t)g << 8 | b;
}

static dgx_screen_t *draw_tree(dgx_ui_t *ui, const uint32_t *colors, int count)
{
    dgx_screen_t  *scr   = dgx_vscreen_init(120, 80, 16, DgxScreenRGB);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = (uint16_t)count};
    dgx_ui_paint(ui, &paint, 0);
    dgx_ui_paint_free(&paint);
    return scr;
}

/* ---- the full appearance as dgx_ui_c is to make it: constants, the styles' fonts filled at the start ---- */

static const uint32_t             made_rgb[]         = {0x12161e, 0xe8e8de, 0x2c3c56, 0x44556a, 0xf0a03c, 0xff4000};
static const char *const          made_color_names[] = {"paper", "ink", "fill", "border", "accent", "warn"};
static const dgx_ui_button_look_t made_params_0      = {.face = 2, .pressed = 4, .focused = DGX_UI_PAPER, .disabled = 0, .text = 1, .text_disabled = 3, .shadow = 0, .depth = 2};
static const dgx_ui_slider_look_t made_params_1      = {.track = 3, .fill = 4, .knob = 1, .focused = 1, .disabled = 3, .track_h = 3};
static const dgx_ui_band_t        made_bands_2[]     = {{.below = 20, .color = 5}, {.below = 80, .color = 1}};
static const dgx_ui_level_look_t  made_params_2      = {.bar = 4, .rest = DGX_UI_PAPER, .from = DGX_UI_FROM_BOTTOM, .band_count = 2, .bands_by = DGX_UI_BANDS_BY_PLACE, .bands = made_bands_2};
static const dgx_ui_frame_look_t  made_params_3      = {.border = 4, .width = 2};
static const dgx_ui_look_t        made_look[4]       = {{.draw = dgx_ui_draw_button, .params = &made_params_0, .fill = -1},
                                                        {.draw = dgx_ui_draw_slider, .params = &made_params_1, .fill = -1},
                                                        {.draw = dgx_ui_draw_level, .params = &made_params_2, .fill = -1},
                                                        {.draw = dgx_ui_draw_frame, .params = &made_params_3, .fill = 2}};
static dgx_ui_font_variant_t      made_variants_0[2] = {{.font = NULL}, {.font = NULL, .height = 24, .narrow = 70, .joined = true, .pen = 2 | DGX_PEN_SOFT, .hand = &dgx_ui_handwriting}};
static dgx_ui_text_style_t        made_style[1]      = {{.variants = made_variants_0, .count = 2, .flags = DGX_UI_TEXT_MIDDLE | DGX_UI_TEXT_IMPORTANT, .align = DGX_FONT_ALIGN_CENTER, .scroll_ms = 900}};
static const dgx_ui_look_name_t   made_looks[]       = {{"big", &made_look[0]}, {"slider", &made_look[1]}, {"tank", &made_look[2]}, {"pane", &made_look[3]},
                                                        DGX_UI_LOOK_NAME(slider), DGX_UI_LOOK_NAME(switch)};
static const dgx_ui_style_name_t  made_styles[]      = {{"body", &made_style[0]}, {"plain", &plain}};
static const dgx_ui_appearance_t  made_appearance    = {.rgb = made_rgb, .color_names = made_color_names, .looks = made_looks, .styles = made_styles,
                                                        .color_count = 6, .look_count = 6, .style_count = 2};
/* the fonts are functions of DGX until 0.12: filled at the start */
static void made_start(void)
{
    made_variants_0[0].font = TerminusTTFMedium12(), made_variants_0[1].font = font0ss();
}

static bool same_variant(const dgx_ui_font_variant_t *a, const dgx_ui_font_variant_t *b);

/* two looks the same: their draw, their fill, their parameters byte by byte, their bands */
static bool same_look(const dgx_ui_look_t *a, const dgx_ui_look_t *b, const dgx_ui_params_t *described)
{
    if (a->draw != b->draw || a->fill != b->fill || !a->params != !b->params) return false;
    if (!a->params) return true;
    uint8_t first[64], second[64];
    memcpy(first, a->params, described->size), memcpy(second, b->params, described->size);
    for (int f = 0; f < described->field_count; ++f)
        if (described->fields[f].kind == DGX_UI_FIELD_BANDS) {
            const dgx_ui_band_t *bands_a, *bands_b;
            memcpy(&bands_a, first + described->fields[f].offset, sizeof bands_a), memcpy(&bands_b, second + described->fields[f].offset, sizeof bands_b);
            const int count = first[described->fields[f].count_offset];
            for (int k = 0; k < count; ++k)
                if (bands_a[k].below != bands_b[k].below || bands_a[k].color != bands_b[k].color) return false;
            memset(first + described->fields[f].offset, 0, sizeof bands_a), memset(second + described->fields[f].offset, 0, sizeof bands_b);
        }
    return memcmp(first, second, described->size) == 0;
}

/* ---- appearances by chance, from the descriptions ---- */

static uint32_t seed = 4242;
static int      chance(int below)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 8) % (uint32_t)below);
}

typedef struct printer_t {
    char  *out;
    size_t n, cap;
} printer_t;
static void print(printer_t *p, const char *format, ...)
{
    va_list a;
    va_start(a, format);
    p->n += (size_t)vsnprintf(p->out + p->n, p->n < p->cap ? p->cap - p->n : 0, format, a);
    va_end(a);
}

/* what a look by chance is expected to be */
typedef struct expected_look_t {
    char          name[24];
    int           draw, fill;
    uint8_t       params[64];
    dgx_ui_band_t bands[4];
    int           band_count;
} expected_look_t;

static char     color_names[12][24];
static int      color_count; /* the indices: 0..3 the fixed, then in the order written */
static uint32_t color_rgb[12];

static int a_color(bool under) /* a colour's index, or DGX_UI_PAPER for null where the field may take it */
{
    return under && chance(4) == 0 ? DGX_UI_PAPER : chance(color_count);
}
static void print_color(printer_t *p, int index)
{
    if (index == DGX_UI_PAPER) print(p, "null");
    else print(p, "\"%s\"", color_names[index]);
}
static void put(uint8_t *base, const dgx_ui_field_t *field, int32_t value)
{
    if (field->size == 1) base[field->offset] = (uint8_t)value;
    else if (field->size == 2) {
        const int16_t narrow = (int16_t)value;
        memcpy(base + field->offset, &narrow, 2);
    } else {
        memcpy(base + field->offset, &value, 4);
    }
}

/* the params of a look by chance, printed and expected: every colour, the others now and then */
static void chance_params(printer_t *p, const dgx_ui_params_t *described, expected_look_t *e)
{
    int first = 1;
    print(p, "{");
    int order[16];
    for (int k = 0; k < described->field_count; ++k) order[k] = k;
    for (int k = described->field_count - 1; k > 0; --k) {
        const int j = chance(k + 1), t = order[k];
        order[k] = order[j], order[j] = t;
    }
    for (int o = 0; o < described->field_count; ++o) {
        const dgx_ui_field_t *field = &described->fields[order[o]];
        const bool            color = field->kind <= DGX_UI_FIELD_COLOR_OR_UNDER;
        if (!color && chance(3) == 0) continue; /* not said: 0 */
        print(p, "%s\"%s\": ", first ? "" : ", ", field->name), first = 0;
        int32_t value = 0;
        switch (field->kind) {
        case DGX_UI_FIELD_COLOR:
        case DGX_UI_FIELD_COLOR_OR_UNDER: value = a_color(field->kind == DGX_UI_FIELD_COLOR_OR_UNDER), print_color(p, value); break;
        case DGX_UI_FIELD_NUMBER: value = field->low + chance(field->high - field->low + 1), print(p, "%d", (int)value); break;
        case DGX_UI_FIELD_BOOL: value = chance(2), print(p, value ? "true" : "false"); break;
        case DGX_UI_FIELD_WORD: {
            int count = 0;
            for (const char *w = field->words; *w; w += strlen(w) + 1) ++count;
            value          = chance(count);
            const char *w  = field->words;
            for (int k = 0; k < value; ++k) w += strlen(w) + 1;
            print(p, "\"%s\"", w);
            break;
        }
        case DGX_UI_FIELD_BANDS: {
            e->band_count = chance(5);
            print(p, "[");
            for (int b = 0; b < e->band_count; ++b) {
                e->bands[b] = (dgx_ui_band_t){.below = chance(2000) - 1000, .color = (uint8_t)a_color(false)};
                print(p, "%s[%d, ", b ? ", " : "", (int)e->bands[b].below), print_color(p, e->bands[b].color), print(p, "]");
            }
            print(p, "]");
            e->params[field->count_offset] = (uint8_t)e->band_count;
            continue;
        }
        default: break;
        }
        put(e->params, field, value);
    }
    print(p, "}");
}

/* a variant by chance: a font's name, or an object; printed and expected */
static void chance_variant(printer_t *p, dgx_ui_font_variant_t *v)
{
    *v = (dgx_ui_font_variant_t){0};
    if (chance(2)) {
        v->font = TerminusTTFMedium12();
        print(p, "\"small\"");
        return;
    }
    const bool hand = chance(2);
    v->font         = hand ? font0ss() : TerminusTTFMedium12();
    print(p, "{\"font\": \"%s\"", hand ? "hand" : "small");
    if (hand || chance(3) == 0) v->hand = chance(2) ? &dgx_ui_handwriting : &dgx_ui_handwriting_canvas, print(p, ", \"hand\": \"%s\"", v->hand == &dgx_ui_handwriting ? "handwriting" : "handwriting_canvas");
    if (chance(2)) v->height = (int16_t)chance(40), print(p, ", \"height\": %d", v->height);
    if (chance(2)) v->narrow = (uint8_t)chance(101), print(p, ", \"narrow\": %d", v->narrow);
    if (chance(2)) v->joined = chance(2), print(p, ", \"joined\": %s", v->joined ? "true" : "false");
    const bool soft = chance(2);
    if (chance(2)) v->pen = (uint32_t)chance(6), print(p, ", \"pen\": %u", (unsigned)v->pen);
    if (soft) v->pen |= DGX_PEN_SOFT, print(p, ", \"soft\": true");
    print(p, "}");
}

static bool same_variant(const dgx_ui_font_variant_t *a, const dgx_ui_font_variant_t *b)
{
    return a->font == b->font && a->height == b->height && a->narrow == b->narrow && a->joined == b->joined && a->pen == b->pen && a->hand == b->hand;
}

int main(void)
{
    dgx_ui_appearance_need_t need;
    dgx_ui_load_error_t      err;
    dgx_ui_appearance_t      appearance;

    /* an appearance: the fixed colours among the others, a look of four draws, a style of two variants */
    static const char day[] =
        "{\"looks\": {"
        "\"big\": {\"params\": {\"face\": \"fill\", \"pressed\": \"accent\", \"focused\": null, \"disabled\": \"paper\", \"text\": \"ink\", \"text_disabled\": \"border\", \"shadow\": \"paper\", \"depth\": 2}, \"draw\": \"button\"},"
        "\"slider\": {\"draw\": \"slider\", \"fill\": null, \"params\": {\"track\": \"border\", \"fill\": \"accent\", \"knob\": \"ink\", \"focused\": \"ink\", \"disabled\": \"border\", \"track_h\": 3}},"
        "\"tank\": {\"draw\": \"level\", \"params\": {\"bar\": \"accent\", \"rest\": null, \"from\": \"bottom\", \"bands_by\": \"place\", \"bands\": [[20, \"warn\"], [80, \"ink\"]]}},"
        "\"pane\": {\"draw\": \"frame\", \"fill\": \"fill\", \"params\": {\"border\": \"accent\", \"width\": 2}}},"
        "\"name\": \"day\", \"editor\": {\"zoom\": 1.5},"
        "\"colors\": {\"ink\": \"#e8e8de\", \"accent\": \"#f0a03c\", \"paper\": \"#12161e\", \"fill\": \"#2c3c56\", \"warn\": \"#ff4000\", \"border\": \"#44556a\"},"
        "\"styles\": {\"body\": {\"fonts\": [\"small\", {\"font\": \"hand\", \"hand\": \"handwriting\", \"height\": 24, \"narrow\": 70, \"joined\": true, \"pen\": 2, \"soft\": true}], \"align\": \"center\", \"flags\": [\"middle\", \"important\"], \"scroll_ms\": 900}},"
        "\"dgx-ui\": 1}";
    CHECK(dgx_ui_appearance_need(day, strlen(day), &names, &need, &err) && need.colors == 6 && need.looks == 4 && need.styles == 1 && strcmp(need.name, "day") == 0);
    uint8_t *memory = malloc(need.bytes);
    CHECK(!dgx_ui_appearance_load(day, strlen(day), &names, memory, need.bytes - 1, &appearance, &err) && err.code == DGX_UI_LOAD_MEMORY);
    CHECK(!dgx_ui_appearance_load(day, strlen(day), &names, memory + 1, need.bytes, &appearance, &err) && err.code == DGX_UI_LOAD_MEMORY);
    char *text = strdup(day);
    CHECK(dgx_ui_appearance_load(text, strlen(text), &names, memory, need.bytes, &appearance, &err));
    memset(text, 'x', strlen(text)), free(text); /* the text may go: all is laid */
    CHECK(appearance.color_count == 6 && appearance.look_count == 6 && appearance.style_count == 2);
    static const char *const order[]   = {"paper", "ink", "fill", "border", "accent", "warn"};
    static const uint32_t    expect[6] = {0x12161e, 0xe8e8de, 0x2c3c56, 0x44556a, 0xf0a03c, 0xff4000};
    for (int k = 0; k < 6; ++k) CHECK(strcmp(appearance.color_names[k], order[k]) == 0 && appearance.rgb[k] == expect[k] && dgx_ui_appearance_color(&appearance, order[k]) == k);
    CHECK(dgx_ui_appearance_color(&appearance, "red") == -1);
    const dgx_ui_button_look_t big = {.face = 2, .pressed = 4, .focused = DGX_UI_PAPER, .disabled = 0, .text = 1, .text_disabled = 3, .shadow = 0, .depth = 2};
    CHECK(strcmp(appearance.looks[0].name, "big") == 0 && appearance.looks[0].look->draw == dgx_ui_draw_button && appearance.looks[0].look->fill == -1 &&
          memcmp(appearance.looks[0].look->params, &big, sizeof big) == 0);
    const dgx_ui_slider_look_t slider = {.track = 3, .fill = 4, .knob = 1, .focused = 1, .disabled = 3, .track_h = 3};
    CHECK(strcmp(appearance.looks[1].name, "slider") == 0 && memcmp(appearance.looks[1].look->params, &slider, sizeof slider) == 0);
    const dgx_ui_level_look_t *tank = appearance.looks[2].look->params;
    CHECK(tank->bar == 4 && tank->rest == DGX_UI_PAPER && tank->from == DGX_UI_FROM_BOTTOM && tank->bands_by == DGX_UI_BANDS_BY_PLACE && tank->band_count == 2 &&
          tank->bands[0].below == 20 && tank->bands[0].color == 5 && tank->bands[1].below == 80 && tank->bands[1].color == 1);
    const dgx_ui_frame_look_t *pane = appearance.looks[3].look->params;
    CHECK(appearance.looks[3].look->fill == 2 && pane->border == 4 && pane->width == 2 && appearance.looks[3].look->draw == dgx_ui_draw_frame);
    CHECK(appearance.looks[4].look == &dgx_ui_look_slider && appearance.looks[5].look == &dgx_ui_look_switch); /* the program's after */
    const dgx_ui_text_style_t *body = appearance.styles[0].style;
    CHECK(strcmp(appearance.styles[0].name, "body") == 0 && body->count == 2 && body->align == DGX_FONT_ALIGN_CENTER &&
          body->flags == (DGX_UI_TEXT_MIDDLE | DGX_UI_TEXT_IMPORTANT) && body->scroll_ms == 900);
    CHECK(body->variants[0].font == TerminusTTFMedium12() && body->variants[0].hand == NULL && body->variants[1].font == font0ss() &&
          body->variants[1].hand == &dgx_ui_handwriting && body->variants[1].height == 24 && body->variants[1].narrow == 70 && body->variants[1].joined &&
          body->variants[1].pen == (2 | DGX_PEN_SOFT));
    CHECK(appearance.styles[1].style == &plain);

    /* two packers on one appearance: a table for each */
    uint32_t packed[6], screen_packed[6];
    dgx_ui_appearance_pack(&appearance, as_is, NULL, packed);
    dgx_screen_t *scr = dgx_vscreen_init(8, 8, 16, DgxScreenRGB);
    dgx_ui_appearance_pack(&appearance, dgx_ui_pack_screen, scr, screen_packed);
    for (int k = 0; k < 6; ++k)
        CHECK(packed[k] == expect[k] && screen_packed[k] == dgx_screen_rgb(scr, (uint8_t)(expect[k] >> 16), (uint8_t)(expect[k] >> 8), (uint8_t)expect[k]));
    dgx_screen_destroy(&scr);

    /* a page on its tables: the slider takes the appearance's look, which hides the program's; the switch the program's,
       which the appearance has not; a text with no style the appearance's first, one that names "plain" the program's */
    {
        static int32_t                   value;
        static const dgx_ui_value_name_t values[] = {{"v", &value}};
        const dgx_ui_load_names_t        tables   = {.looks = appearance.looks, .look_count = appearance.look_count, .styles = appearance.styles,
                                                     .style_count = appearance.style_count, .values = values, .value_count = 1};
        static const char page[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"children\": [{\"kind\": \"slider\", \"value\": \"v\", \"want\": [0, 10]},"
                                   "{\"kind\": \"switch\", \"value\": \"v\", \"want\": [0, 10]}, {\"kind\": \"label\", \"text\": \"hi\"},"
                                   "{\"kind\": \"label\", \"text\": \"so\", \"style\": \"plain\"}, {\"kind\": \"button\", \"look\": \"big\", \"text\": \"Go\", \"want\": [0, 16]}]}}";
        static dgx_ui_node_t nodes[8];
        static uint8_t       page_memory[2048] __attribute__((aligned(16)));
        dgx_ui_t             ui;
        dgx_ui_init(&ui, nodes, 8, 1);
        CHECK(dgx_ui_load(&ui, -1, page, strlen(page), &tables, page_memory, sizeof page_memory, &err) == 0);
        CHECK(dgx_ui_widget(&ui, 1)->look == appearance.looks[1].look && dgx_ui_widget(&ui, 2)->look == &dgx_ui_look_switch);
        CHECK(dgx_ui_widget(&ui, 3)->text->style == body && dgx_ui_widget(&ui, 4)->text->style == &plain && dgx_ui_widget(&ui, 5)->look == appearance.looks[0].look);
        /* the button of the appearance drawn as one of C constants */
        static dgx_ui_textbox_t   go = DGX_UI_TEXT_CONST("Go", 1, NULL);
        static dgx_ui_look_t      constant_big;
        static dgx_ui_node_t      constant_nodes[2], loaded_nodes[2];
        static dgx_ui_font_variant_t small[1];
        static dgx_ui_text_style_t small_style;
        small[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()}, small_style = (dgx_ui_text_style_t){.variants = small, .count = 1};
        go.style     = &small_style;
        constant_big = (dgx_ui_look_t){.draw = dgx_ui_draw_button, .params = &big, .fill = -1};
        const dgx_ui_widget_t made_w = {.kind = DGX_UI_BUTTON, .text = &go, .look = &constant_big}, loaded_w = {.kind = DGX_UI_BUTTON, .text = &go, .look = appearance.looks[0].look};
        dgx_ui_t made_ui, loaded_ui;
        dgx_ui_init(&made_ui, constant_nodes, 2, 1), dgx_ui_init(&loaded_ui, loaded_nodes, 2, 1);
        dgx_ui_add(&made_ui, -1, &made_w), dgx_ui_add(&loaded_ui, -1, &loaded_w);
        dgx_screen_t *a = draw_tree(&made_ui, screen_packed, 6), *b = draw_tree(&loaded_ui, screen_packed, 6);
        int           off = 0;
        for (int y = 0; y < 80; ++y)
            for (int x = 0; x < 120; ++x) off += dgx_get_pixel(a, x, y) != dgx_get_pixel(b, x, y);
        CHECK(off == 0);
        dgx_screen_destroy(&a), dgx_screen_destroy(&b);
        dgx_ui_free(&made_ui), dgx_ui_free(&loaded_ui), dgx_ui_free(&ui);
    }
    /* the appearance made C, by hand as dgx_ui_c is to make it: every field the loaded one's, a page on its tables drawn
       the same, pixel by pixel */
    {
        made_start();
        CHECK(made_appearance.color_count == appearance.color_count && made_appearance.look_count == appearance.look_count &&
              made_appearance.style_count == appearance.style_count);
        for (int k = 0; k < appearance.color_count; ++k) CHECK(made_appearance.rgb[k] == appearance.rgb[k] && strcmp(made_appearance.color_names[k], appearance.color_names[k]) == 0);
        static const dgx_ui_params_t *const described[] = {&dgx_ui_params_button, &dgx_ui_params_slider, &dgx_ui_params_level, &dgx_ui_params_frame};
        for (int k = 0; k < appearance.look_count; ++k) {
            CHECK(strcmp(made_appearance.looks[k].name, appearance.looks[k].name) == 0);
            if (k < 4) CHECK(same_look(made_appearance.looks[k].look, appearance.looks[k].look, described[k]));
            else CHECK(made_appearance.looks[k].look == appearance.looks[k].look);
        }
        for (int k = 0; k < appearance.style_count; ++k) {
            const dgx_ui_text_style_t *made = made_appearance.styles[k].style, *loaded = appearance.styles[k].style;
            CHECK(strcmp(made_appearance.styles[k].name, appearance.styles[k].name) == 0 && made->count == loaded->count && made->flags == loaded->flags &&
                  made->align == loaded->align && made->scroll_ms == loaded->scroll_ms);
            for (int v = 0; v < made->count && v < loaded->count; ++v) CHECK(same_variant(&made->variants[v], &loaded->variants[v]));
        }
        uint32_t made_packed[6];
        dgx_ui_appearance_pack(&made_appearance, as_is, NULL, made_packed);
        CHECK(memcmp(made_packed, packed, sizeof packed) == 0);
        /* a page of every look and style of it on each, drawn the same */
        static int32_t                   value = 40;
        static const dgx_ui_value_name_t values[] = {{"v", &value}};
        static const char page[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"frame\", \"look\": \"pane\", \"flags\": [\"column\"], \"pad\": 2, \"children\": ["
                                   "{\"kind\": \"slider\", \"value\": \"v\", \"max\": 100, \"want\": [0, 9]}, {\"kind\": \"own\", \"look\": \"tank\", \"value\": \"v\", \"max\": 100, \"want\": [0, 8]},"
                                   "{\"kind\": \"switch\", \"value\": \"v\", \"want\": [0, 9]}, {\"kind\": \"label\", \"text\": \"hi\"}, {\"kind\": \"label\", \"text\": \"so\", \"style\": \"plain\"},"
                                   "{\"kind\": \"button\", \"look\": \"big\", \"text\": \"Go\", \"want\": [0, 16]}]}}";
        static dgx_ui_node_t made_nodes[8], loaded_nodes[8];
        static uint8_t       made_memory[2048] __attribute__((aligned(16))), loaded_memory[2048] __attribute__((aligned(16)));
        static dgx_ui_font_variant_t plain_variant[1];
        plain_variant[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()}, plain = (dgx_ui_text_style_t){.variants = plain_variant, .count = 1};
        const dgx_ui_load_names_t made_tables   = {.looks = made_appearance.looks, .look_count = made_appearance.look_count, .styles = made_appearance.styles,
                                                   .style_count = made_appearance.style_count, .values = values, .value_count = 1};
        const dgx_ui_load_names_t loaded_tables = {.looks = appearance.looks, .look_count = appearance.look_count, .styles = appearance.styles,
                                                   .style_count = appearance.style_count, .values = values, .value_count = 1};
        dgx_ui_t made_ui, loaded_ui;
        dgx_ui_init(&made_ui, made_nodes, 8, 1), dgx_ui_init(&loaded_ui, loaded_nodes, 8, 1);
        CHECK(dgx_ui_load(&made_ui, -1, page, strlen(page), &made_tables, made_memory, sizeof made_memory, &err) == 0);
        CHECK(dgx_ui_load(&loaded_ui, -1, page, strlen(page), &loaded_tables, loaded_memory, sizeof loaded_memory, &err) == 0);
        dgx_screen_t *a = draw_tree(&made_ui, made_packed, 6), *b = draw_tree(&loaded_ui, packed, 6);
        int           off = 0, inked = 0;
        for (int y = 0; y < 80; ++y)
            for (int x = 0; x < 120; ++x) off += dgx_get_pixel(a, x, y) != dgx_get_pixel(b, x, y), inked += dgx_get_pixel(a, x, y) != dgx_get_pixel(a, 0, 0);
        CHECK(off == 0 && inked > 200);
        dgx_screen_destroy(&a), dgx_screen_destroy(&b);
        dgx_ui_free(&made_ui), dgx_ui_free(&loaded_ui);
    }
    free(memory);

    /* the refusals, each with its code, byte and name */
#define FOUR "\"paper\": \"#000000\", \"ink\": \"#ffffff\", \"fill\": \"#111111\", \"border\": \"#222222\""
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {\"paper\": \"#000000\", \"ink\": \"#ffffff\", \"fill\": \"#111111\"}}", DGX_UI_LOAD_MISSING, 24, "border"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR ", \"paper\": \"#000000\"}}", DGX_UI_LOAD_KEY, 103, "paper"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR ", \"a\": \"#000000\", \"a\": \"#000000\"}}", DGX_UI_LOAD_KEY, 119, "a"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR ", \"a\": \"#00000g\"}}", DGX_UI_LOAD_SYNTAX, 108, "a"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR ", \"a\": \"#000\"}}", DGX_UI_LOAD_SYNTAX, 108, "a"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR ", \"a\": 5}}", DGX_UI_LOAD_SYNTAX, 108, NULL));
    CHECK(refused("{\"dgx-ui\": 1}", DGX_UI_LOAD_KEY, 0, "colors"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"tree\": {}}", DGX_UI_LOAD_KEY, 104, "tree"));
    CHECK(refused("{\"dgx-ui\": 2, \"colors\": {\"paper\": 1.5}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"editor\": 5}", DGX_UI_LOAD_SYNTAX, 114, NULL));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"name\": \"a b\"}", DGX_UI_LOAD_NAME_BAD, 112, "a b"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"params\": {}}}}", DGX_UI_LOAD_MISSING, 114, "draw"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"dial\"}}}", DGX_UI_LOAD_UNKNOWN, 128, "dial"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"frame\"}}}", DGX_UI_LOAD_MISSING, 114, "border"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"frame\", \"params\": {\"width\": 1}}}}", DGX_UI_LOAD_MISSING, 147, "border"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"frame\", \"params\": {\"border\": \"red\"}}}}", DGX_UI_LOAD_UNKNOWN, 158, "red"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"frame\", \"params\": {\"border\": null}}}}", DGX_UI_LOAD_SYNTAX, 158, NULL));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"frame\", \"params\": {\"border\": \"ink\", \"width\": 300}}}}", DGX_UI_LOAD_RANGE, 174, NULL));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"frame\", \"params\": {\"border\": \"ink\", \"colour\": 1}}}}", DGX_UI_LOAD_KEY, 165, "colour"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"frame\", \"draw\": \"frame\"}}}", DGX_UI_LOAD_KEY, 137, "draw"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"frame\", \"face\": \"ink\"}}}", DGX_UI_LOAD_KEY, 137, "face"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"level\", \"params\": {\"bar\": \"ink\", \"rest\": null, \"from\": \"up\"}}}}", DGX_UI_LOAD_UNKNOWN, 184, "up"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"looks\": {\"x\": {\"draw\": \"level\", \"params\": {\"bar\": \"ink\", \"rest\": null, \"bands\": [[1, \"red\"]]}}}}", DGX_UI_LOAD_UNKNOWN, 190, "red"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"styles\": {\"s\": {\"fonts\": [\"big\"]}}}", DGX_UI_LOAD_UNKNOWN, 131, "big"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"styles\": {\"s\": {\"align\": \"left\"}}}", DGX_UI_LOAD_MISSING, 120, "fonts"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"styles\": {\"s\": {\"fonts\": []}}}", DGX_UI_LOAD_RANGE, 130, "fonts"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"styles\": {\"s\": {\"fonts\": [\"small\"], \"flags\": [\"bold\"]}}}", DGX_UI_LOAD_UNKNOWN, 151, "bold"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"styles\": {\"s\": {\"fonts\": [\"small\"], \"middle\": true}}}", DGX_UI_LOAD_KEY, 141, "middle"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"styles\": {\"s\": {\"fonts\": [{\"font\": \"hand\", \"height\": 20}]}}}", DGX_UI_LOAD_MISSING, 131, "hand"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"styles\": {\"s\": {\"fonts\": [{\"height\": 20}]}}}", DGX_UI_LOAD_MISSING, 131, "font"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"styles\": {\"s\": {\"fonts\": [{\"font\": \"small\", \"hand\": \"pencil\"}]}}}", DGX_UI_LOAD_UNKNOWN, 157, "pencil"));
    CHECK(refused("{\"dgx-ui\": 1, \"colors\": {" FOUR "}, \"styles\": {\"s\": {\"fonts\": [{\"font\": \"small\", \"narrow\": 101}]}}}", DGX_UI_LOAD_RANGE, 159, NULL));
    {
        /* 255 colours at most: the fixed four and 251 more */
        static char many[8192];
        char       *at = many + sprintf(many, "{\"dgx-ui\": 1, \"colors\": {" FOUR);
        for (int k = 0; k < 251; ++k) at += sprintf(at, ", \"c%d\": \"#010203\"", k);
        sprintf(at, "}}");
        CHECK(dgx_ui_appearance_need(many, strlen(many), &names, &need, &err) && need.colors == 255);
        at = many + strlen(many) - 2;
        sprintf(at, ", \"c999\": \"#010203\"}}");
        CHECK(refused(many, DGX_UI_LOAD_RANGE, (int)(strstr(many, "\"c999") - many), "c999"));
    }
    /* a page refuses an appearance's keys as keys it does not know */
    {
        dgx_ui_load_need_t page_need;
        static const char  page[] = "{\"dgx-ui\": 1, \"colors\": {}, \"tree\": {\"kind\": \"box\"}}";
        CHECK(!dgx_ui_load_need(page, strlen(page), &(dgx_ui_load_names_t){0}, &page_need, &err) && err.code == DGX_UI_LOAD_KEY && strcmp(err.name, "colors") == 0);
    }

    /* appearances by chance from the descriptions: loaded and compared byte by byte; each spoiled at one byte */
    {
        static char     text_by_chance[16384];
        static uint8_t  laid[16384] __attribute__((aligned(16)));
        expected_look_t expected[8];
        int             off = 0, made = 0, spoiled_off = 0, spoiled = 0;
        for (int round = 0; round < 300; ++round) {
            printer_t p = {.out = text_by_chance, .cap = sizeof text_by_chance};
            /* the colours: the fixed four and some more, in a shuffled order */
            color_count = 4 + chance(8);
            int written[12];
            for (int k = 0; k < color_count; ++k) written[k] = k;
            for (int k = color_count - 1; k > 0; --k) {
                const int j = chance(k + 1), t = written[k];
                written[k] = written[j], written[j] = t;
            }
            static const char *const fixed[] = {"paper", "ink", "fill", "border"};
            int                      next    = 4;
            for (int k = 0; k < color_count; ++k) {
                const int index = written[k] < 4 ? written[k] : next++;
                if (written[k] < 4) strcpy(color_names[index], fixed[written[k]]);
                else snprintf(color_names[index], 24, "c%d.%d", round, k);
                color_rgb[index] = (uint32_t)chance(1 << 24);
                written[k]       = index;
            }
            print(&p, "{\"dgx-ui\": 1, \"colors\": {");
            for (int k = 0; k < color_count; ++k) print(&p, "%s\"%s\": \"#%06x\"", k ? ", " : "", color_names[written[k]], (unsigned)color_rgb[written[k]]);
            print(&p, "}");
            if (chance(3) == 0) print(&p, ", \"editor\": {\"zoom\": %d.5, \"guides\": [1e3, -2]}", chance(9));
            if (chance(2)) print(&p, ", \"name\": \"round%d\"", round);
            /* the looks */
            const int look_count = chance(7);
            print(&p, ", \"looks\": {");
            for (int k = 0; k < look_count; ++k) {
                expected_look_t *e = &expected[k];
                memset(e, 0, sizeof *e);
                e->draw = chance(DRAW_COUNT), e->fill = -1;
                snprintf(e->name, sizeof e->name, k == 0 && chance(3) == 0 ? "slider" : "l%d", k);
                print(&p, "%s\"%s\": {\"draw\": \"%s\"", k ? ", " : "", e->name, draws[e->draw].name);
                if (chance(2)) {
                    const int fill = a_color(true);
                    e->fill        = fill == DGX_UI_PAPER ? -1 : fill;
                    print(&p, ", \"fill\": "), print_color(&p, fill);
                }
                if (draws[e->draw].params->field_count || chance(2)) print(&p, ", \"params\": "), chance_params(&p, draws[e->draw].params, e);
                print(&p, "}");
            }
            print(&p, "}");
            /* the styles */
            const int             style_count = chance(4);
            dgx_ui_text_style_t   styles_expected[4];
            dgx_ui_font_variant_t variants_expected[4][3];
            print(&p, ", \"styles\": {");
            for (int k = 0; k < style_count; ++k) {
                dgx_ui_text_style_t *s = &styles_expected[k];
                *s                     = (dgx_ui_text_style_t){.count = (uint8_t)(1 + chance(3))};
                print(&p, "%s\"s%d\": {\"fonts\": [", k ? ", " : "", k);
                for (int v = 0; v < s->count; ++v) print(&p, v ? ", " : ""), chance_variant(&p, &variants_expected[k][v]);
                print(&p, "]");
                if (chance(2)) s->align = (dgx_font_align_t)chance(3), print(&p, ", \"align\": \"%s\"", s->align == 0 ? "left" : s->align == 1 ? "center" : "right");
                if (chance(2)) s->flags = (uint8_t)chance(4), print(&p, ", \"flags\": [%s%s%s]", s->flags & 1 ? "\"middle\"" : "", s->flags == 3 ? ", " : "", s->flags & 2 ? "\"important\"" : "");
                if (chance(2)) s->scroll_ms = (uint16_t)chance(65536), print(&p, ", \"scroll_ms\": %u", s->scroll_ms);
                print(&p, "}");
            }
            print(&p, "}}");
            if (p.n >= sizeof text_by_chance) continue;
            if (!dgx_ui_appearance_need(text_by_chance, p.n, &names, &need, &err) || need.bytes > sizeof laid ||
                !dgx_ui_appearance_load(text_by_chance, p.n, &names, laid, need.bytes, &appearance, &err)) {
                ++off;
                fprintf(stderr, "  round %d: code %d at %d \"%s\" in %.*s\n", round, err.code, err.offset, err.name, (int)p.n, text_by_chance);
                continue;
            }
            ++made;
            off += appearance.color_count != color_count || appearance.look_count != look_count + 2 || appearance.style_count != style_count + 1;
            for (int k = 0; k < color_count; ++k) off += appearance.rgb[k] != color_rgb[k] || strcmp(appearance.color_names[k], color_names[k]) != 0;
            for (int k = 0; k < look_count && k < appearance.look_count; ++k) {
                const expected_look_t *e         = &expected[k];
                const dgx_ui_look_t   *look      = appearance.looks[k].look;
                const dgx_ui_params_t *described = draws[e->draw].params;
                uint8_t                params[64];
                memcpy(params, look->params ? look->params : (const void *)params, described->size);
                for (int f = 0; f < described->field_count; ++f) /* a pointer laid where it is: its bands compared below */
                    if (described->fields[f].kind == DGX_UI_FIELD_BANDS) {
                        const dgx_ui_band_t *bands;
                        memcpy(&bands, params + described->fields[f].offset, sizeof bands);
                        for (int b = 0; b < e->band_count; ++b) off += bands[b].below != e->bands[b].below || bands[b].color != e->bands[b].color;
                        memset(params + described->fields[f].offset, 0, sizeof bands);
                    }
                off += strcmp(appearance.looks[k].name, e->name) != 0 || look->draw != draws[e->draw].draw || look->fill != e->fill ||
                       memcmp(params, e->params, described->size) != 0;
            }
            for (int k = 0; k < style_count; ++k) {
                const dgx_ui_text_style_t *s = appearance.styles[k].style, *e = &styles_expected[k];
                off += s->count != e->count || s->align != e->align || s->flags != e->flags || s->scroll_ms != e->scroll_ms;
                for (int v = 0; v < s->count && v < e->count; ++v) off += !same_variant(&s->variants[v], &variants_expected[k][v]);
            }
            off += appearance.looks[look_count].look != &dgx_ui_look_slider || appearance.styles[style_count].style != &plain;
            /* spoiled at a byte: a refusal inside the text, or an appearance laid in what its need said */
            static const char bytes[] = {'"', '{', '}', '[', ']', ',', ':', '#', '0', 'x', ' ', '\0', (char)0xc3};
            for (int s = 0; s < 4; ++s) {
                char *copy = malloc(p.n);
                memcpy(copy, text_by_chance, p.n);
                copy[chance((int)p.n)] = bytes[chance(sizeof bytes)];
                ++spoiled;
                if (dgx_ui_appearance_need(copy, p.n, &names, &need, &err)) spoiled_off += need.bytes > sizeof laid || !dgx_ui_appearance_load(copy, p.n, &names, laid, need.bytes, &appearance, &err);
                else spoiled_off += err.offset < 0 || (size_t)err.offset > p.n || err.code == DGX_UI_LOAD_OK;
                free(copy);
            }
        }
        CHECK(off == 0 && made > 250 && spoiled_off == 0 && spoiled > 1000);
    }
    CHECK_DONE();
}
