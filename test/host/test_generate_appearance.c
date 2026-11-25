/*
 * The appearances dgx_ui_c made C (-A), against the loader: each appearance loaded by dgx_ui_appearance_load() on the
 * tables dgx_ui_c made of the names file (-t), and the two held equal field by field: the colours and their names, each look's draw, fill, parameters byte by byte by its draw's
 * description and bands, each style and its variants, the program's own looks and styles after; and a page of a widget
 * for each look of it, of a kind its draw draws, drawn on both pixel by pixel. The spoiled appearances are given to
 * the loader on the same tables, and its verdict printed for test_generate_appearance.py to hold against dgx_ui_c's.
 * A page made with -a on an appearance, against the loader on the tables -t -a made of the two. The cases are
 * test_generate_appearance.py's, in appearance_cases.h of its directory.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_appearance.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"

/* the program's objects, as the names file names them */
int32_t                            volume           = 40;
/* the nine pictures of a frame, of 1 bit: a corner a square of 3, an edge and the middle of 2, each its own pattern */
static const uint8_t     card_bits[9][3] = {{0xe0, 0xa0, 0xe0}, {0xc0, 0x00, 0x00}, {0xe0, 0x20, 0xe0}, {0x80, 0x80, 0x00}, {0x40, 0x80, 0x00},
                                            {0x40, 0x40, 0x00}, {0xe0, 0x80, 0xe0}, {0x00, 0xc0, 0x00}, {0xe0, 0x20, 0xe0}};
static const dgx_image_t card_images[9] = {
    {3, 3, 0, 0, DGX_IMAGE_1BPP, card_bits[0]}, {2, 2, 0, 0, DGX_IMAGE_1BPP, card_bits[1]}, {3, 3, 0, 0, DGX_IMAGE_1BPP, card_bits[2]},
    {2, 2, 0, 0, DGX_IMAGE_1BPP, card_bits[3]}, {2, 2, 0, 0, DGX_IMAGE_1BPP, card_bits[4]}, {2, 2, 0, 0, DGX_IMAGE_1BPP, card_bits[5]},
    {3, 3, 0, 0, DGX_IMAGE_1BPP, card_bits[6]}, {2, 2, 0, 0, DGX_IMAGE_1BPP, card_bits[7]}, {3, 3, 0, 0, DGX_IMAGE_1BPP, card_bits[8]}};
const dgx_ui_picture_t card_pieces[9] = {{.image = &card_images[0]}, {.image = &card_images[1]}, {.image = &card_images[2]},
                                         {.image = &card_images[3]}, {.image = &card_images[4]}, {.image = &card_images[5]},
                                         {.image = &card_images[6]}, {.image = &card_images[7]}, {.image = &card_images[8]}};
const dgx_ui_picture_t one_piece[1] = {{.image = &card_images[4]}};
static const dgx_ui_text_look_t    mine_colours     = {.ink = 1, .paper = 0};
const dgx_ui_look_t                mine             = {.draw = dgx_ui_draw_text, .params = &mine_colours, .fill = -1};
static const dgx_ui_font_variant_t plain_variant[1] = {{.font = &TerminusTTFMedium12}};
const dgx_ui_text_style_t          plain            = {.fonts = plain_variant, .count = 1};
bool draw_dial(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *node = dgx_ui_node(ui, i);
    dgx_fill_rectangle(paint->scr, node->x + paint->ox, node->y + paint->oy, node->w, node->h, dgx_ui_color(paint, look->fill < 0 ? 1 : look->fill));
    return false;
}

typedef struct appearance_case_t {
    const char                *name, *text;
    const dgx_ui_appearance_t *made;
} appearance_case_t;
typedef struct appearance_refusal_t {
    const char *name, *text;
    size_t      length;
} appearance_refusal_t;
#include "appearance_cases.h"

/* the description of a draw's parameters, by the draw: of the names file's table */
static const dgx_ui_params_t *described_of(const dgx_ui_look_t *look)
{
    for (int k = 0; k < vocab_appearance_names.draw_count; ++k)
        if (vocab_appearance_names.draws[k].draw == look->draw) return vocab_appearance_names.draws[k].params;
    return NULL;
}

static bool same_look(const dgx_ui_look_t *a, const dgx_ui_look_t *b)
{
    const dgx_ui_params_t *described = described_of(a);
    if (a->draw != b->draw || a->fill != b->fill || a->behavior != b->behavior || !a->params != !b->params) return false;
    if (!a->params || !described) return true;
    uint8_t first[64], second[64];
    memcpy(first, a->params, described->size), memcpy(second, b->params, described->size);
    for (int f = 0; f < described->field_count; ++f)
        if (described->fields[f].kind == DGX_UI_FIELD_BANDS) {
            const dgx_ui_band_t *bands_a, *bands_b;
            memcpy(&bands_a, first + described->fields[f].offset, sizeof bands_a), memcpy(&bands_b, second + described->fields[f].offset, sizeof bands_b);
            for (int k = 0; k < first[described->fields[f].count_offset]; ++k)
                if (bands_a[k].below != bands_b[k].below || bands_a[k].color != bands_b[k].color) return false;
            memset(first + described->fields[f].offset, 0, sizeof bands_a), memset(second + described->fields[f].offset, 0, sizeof bands_b);
        }
    return memcmp(first, second, described->size) == 0;
}

static bool same_style(const dgx_ui_text_style_t *a, const dgx_ui_text_style_t *b)
{
    if (a->count != b->count || a->flags != b->flags || a->align != b->align || a->scroll_ms != b->scroll_ms) return false;
    for (int v = 0; v < a->count; ++v) {
        const dgx_ui_font_variant_t *x = &a->fonts[v], *y = &b->fonts[v];
        if (x->font != y->font || x->height != y->height || x->narrow != y->narrow || x->joined != y->joined || x->pen != y->pen || x->hand != y->hand ||
            x->points != y->points)
            return false;
    }
    return true;
}

/* a page of a widget for each of the appearance's own looks, of a kind its draw draws, its texts in its first style */
enum { PAGE_W = 200, PAGE_H = 320 };
static dgx_screen_t *drawn(const dgx_ui_appearance_t *appearance, int own_looks, int own_styles)
{
    static dgx_ui_widget_t  widgets[64];
    static dgx_ui_textbox_t boxes[64];
    static dgx_ui_node_t    nodes[80];
    static const dgx_ui_widget_t column = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1, .gap = 1};
    const dgx_ui_text_style_t   *style  = own_styles ? appearance->styles[0].style : &plain;
    dgx_ui_t                     ui;
    dgx_ui_init(&ui, nodes, 80, 1);
    const int root = dgx_ui_add(&ui, -1, &column);
    for (int k = 0; k < own_looks && k < 64; ++k) {
        const dgx_ui_look_t   *look      = appearance->looks[k].look;
        const dgx_ui_params_t *described = described_of(look);
        const unsigned         kinds     = described && described->kinds ? described->kinds : 1u << DGX_UI_OWN;
        const int              kind      = __builtin_ctz(kinds);
        const bool texted = kind == DGX_UI_LABEL || kind == DGX_UI_BUTTON || kind == DGX_UI_LIST || kind == DGX_UI_INPUT || kind == DGX_UI_DROPDOWN || kind == DGX_UI_RADIO ||
                            kind == DGX_UI_SWITCH;
        const bool valued = kind == DGX_UI_LIST || kind == DGX_UI_SWITCH || kind == DGX_UI_SLIDER || kind == DGX_UI_DROPDOWN || kind == DGX_UI_OWN || kind == DGX_UI_RADIO;
        boxes[k]          = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST(kind == DGX_UI_LIST || kind == DGX_UI_DROPDOWN ? "Ab\nCd" : "Ab", kind == DGX_UI_LIST ? 2 : 1, style);
        widgets[k] = (dgx_ui_widget_t){.kind = (uint8_t)kind, .look = look, .want_h = kind == DGX_UI_LIST ? 24 : 12, .max = 100, .text = texted ? &boxes[k] : NULL,
                                       .value = valued ? &volume : NULL};
        dgx_ui_add(&ui, root, &widgets[k]);
    }
    uint32_t       colors[256];
    static uint8_t buffer[PAGE_W * 8]; /* a frame of nine pictures draws its pieces through it */
    dgx_screen_t  *scr = dgx_vscreen_init(PAGE_W, PAGE_H, 16, DgxScreenRGB);
    dgx_ui_appearance_pack(appearance, dgx_ui_pack_screen, scr, colors);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = appearance->color_count, .buffer = buffer, .buffer_size = sizeof buffer};
    dgx_ui_paint(&ui, &paint, 0);
    dgx_ui_paint_free(&paint);
    dgx_ui_free(&ui);
    return scr;
}

/* a frame of a look alone, its pieces drawn through a buffer: the appearance's "nine" against the program's own look */
static dgx_screen_t *framed(const dgx_ui_appearance_t *appearance, const dgx_ui_look_t *look)
{
    static dgx_ui_node_t   nodes[2];
    static uint8_t         buffer[64 * 4];
    dgx_ui_widget_t        frame = {.kind = DGX_UI_FRAME, .want_w = 40, .want_h = 30, .look = look};
    const dgx_ui_widget_t  root  = {.kind = DGX_UI_BOX, .pad = 3};
    dgx_ui_t               ui;
    uint32_t               colors[256];
    dgx_ui_init(&ui, nodes, 2, 1);
    dgx_ui_add(&ui, dgx_ui_add(&ui, -1, &root), &frame);
    dgx_screen_t *scr = dgx_vscreen_init(64, 48, 16, DgxScreenRGB);
    dgx_ui_appearance_pack(appearance, dgx_ui_pack_screen, scr, colors);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = appearance->color_count, .buffer = buffer, .buffer_size = sizeof buffer};
    dgx_ui_paint(&ui, &paint, 0);
    dgx_ui_paint_free(&paint);
    dgx_ui_free(&ui);
    return scr;
}

int main(void)
{
    static uint8_t memory[65536] __attribute__((aligned(16)));
    int            held = 0, framed_checked = 0;
    for (size_t c = 0; c < sizeof cases / sizeof *cases; ++c) {
        const appearance_case_t *one = &cases[c];
        const bool               ruled = strcmp(one->name, "ruled") == 0;
        dgx_ui_appearance_names_t names = vocab_appearance_names;
        if (ruled) names.looks = NULL, names.styles = NULL, names.look_count = 0, names.style_count = 0; /* the rule's: none of the program's own */
        dgx_ui_appearance_need_t need;
        dgx_ui_load_error_t      err;
        dgx_ui_appearance_t      loaded;
        if (!dgx_ui_appearance_need(one->text, strlen(one->text), &names, &need, &err) || need.bytes > sizeof memory ||
            !dgx_ui_appearance_load(one->text, strlen(one->text), &names, memory, need.bytes, &loaded, &err)) {
            fprintf(stderr, "  %s: the loader refused: %d at %d \"%s\"\n", one->name, err.code, err.offset, err.name);
            CHECK(false);
            continue;
        }
        const dgx_ui_appearance_t *made = one->made;
        bool same = made->color_count == loaded.color_count && made->look_count == loaded.look_count && made->style_count == loaded.style_count;
        for (int k = 0; same && k < made->color_count; ++k) same = made->rgb[k] == loaded.rgb[k] && strcmp(made->color_names[k], loaded.color_names[k]) == 0;
        for (int k = 0; same && k < made->look_count; ++k)
            same = strcmp(made->looks[k].name, loaded.looks[k].name) == 0 &&
                   (k < need.looks ? same_look(made->looks[k].look, loaded.looks[k].look) : made->looks[k].look == loaded.looks[k].look);
        for (int k = 0; same && k < made->style_count; ++k)
            same = strcmp(made->styles[k].name, loaded.styles[k].name) == 0 &&
                   (k < need.styles ? same_style(made->styles[k].style, loaded.styles[k].style) : made->styles[k].style == loaded.styles[k].style);
        if (!same) fprintf(stderr, "  %s: the made appearance is not the loaded one\n", one->name);
        CHECK(same);
        dgx_screen_t *a = drawn(made, need.looks, need.styles), *b = drawn(&loaded, need.looks, need.styles);
        int           off = 0;
        for (int y = 0; y < PAGE_H; ++y)
            for (int x = 0; x < PAGE_W; ++x) off += dgx_get_pixel(a, x, y) != dgx_get_pixel(b, x, y);
        if (off) fprintf(stderr, "  %s: %d pixels apart\n", one->name, off);
        CHECK(off == 0);
        dgx_screen_destroy(&a), dgx_screen_destroy(&b);
        held += same && !off;
        /* an appearance sets "nine" whole: its look "card", made C and loaded, draws the pixels of the program's own look
           of dgx_ui_draw_nine on the same pieces, the fill 2 and the ink 1 */
        for (int k = 0; ruled && k < need.looks; ++k) {
            if (strcmp(made->looks[k].name, "card") != 0) continue;
            static const dgx_ui_nine_look_t own_params = {.pieces = card_pieces, .fill = 2, .ink = 1};
            const dgx_ui_look_t             own        = {.draw = dgx_ui_draw_nine, .params = &own_params, .fill = -1};
            dgx_screen_t *of_made = framed(made, made->looks[k].look), *of_loaded = framed(&loaded, loaded.looks[k].look), *of_own = framed(made, &own);
            int apart = 0, inked = 0;
            for (int y = 0; y < 48; ++y)
                for (int x = 0; x < 64; ++x) {
                    const uint32_t pixel = dgx_get_pixel(of_own, x, y);
                    apart += dgx_get_pixel(of_made, x, y) != pixel || dgx_get_pixel(of_loaded, x, y) != pixel;
                    inked += pixel == 0xffff;
                }
            if (apart) fprintf(stderr, "  the appearance's nine: %d pixels apart from the program's own look\n", apart);
            CHECK(apart == 0 && inked > 20 && ((const dgx_ui_nine_look_t *)made->looks[k].look->params)->pieces == card_pieces);
            dgx_screen_destroy(&of_made), dgx_screen_destroy(&of_loaded), dgx_screen_destroy(&of_own);
            ++framed_checked;
        }
    }
    CHECK(framed_checked == 1);
    CHECK(held == (int)(sizeof cases / sizeof *cases));

    /* the spoiled: the loader's verdict on the names file's tables */
    for (size_t k = 0; k < sizeof refusals / sizeof *refusals; ++k) {
        dgx_ui_appearance_need_t need;
        dgx_ui_load_error_t      err;
        if (dgx_ui_appearance_need(refusals[k].text, refusals[k].length, &vocab_appearance_names, &need, &err)) printf("VERDICT %s 0 0 \"\"\n", refusals[k].name);
        else printf("VERDICT %s %d %d \"%s\"\n", refusals[k].name, err.code, err.offset, err.name);
    }

    /* the page made with -a, against the loader on the tables -t -a made: the same widgets, their looks and styles */
    {
        static dgx_ui_node_t loaded_nodes[16], made_nodes[16];
        static uint8_t       page_memory[2048] __attribute__((aligned(16)));
        dgx_ui_t             loaded_ui, made_ui;
        dgx_ui_load_error_t  err;
        dgx_ui_init(&loaded_ui, loaded_nodes, 16, 1), dgx_ui_init(&made_ui, made_nodes, 16, 1);
        CHECK(dgx_ui_load(&loaded_ui, -1, page_text, strlen(page_text), &every_tables_names, page_memory, sizeof page_memory, &err) == 0);
        CHECK(page_add(&made_ui, -1) == 0 && made_ui.count == loaded_ui.count);
        for (int k = 0; k < loaded_ui.count && k < made_ui.count; ++k) {
            const dgx_ui_widget_t *a = dgx_ui_widget(&loaded_ui, k), *b = dgx_ui_widget(&made_ui, k);
            CHECK(a->kind == b->kind && a->look == b->look && !a->text == !b->text && (!a->text || a->text->style == b->text->style));
        }
        CHECK(dgx_ui_widget(&made_ui, 3)->text->style == every_appearance.styles[0].style && dgx_ui_widget(&made_ui, 4)->text->style == &plain);
        dgx_ui_free(&loaded_ui), dgx_ui_free(&made_ui);
    }
    CHECK_DONE();
}
