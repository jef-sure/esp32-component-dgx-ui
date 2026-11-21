/*
 * A tree from a description in JSON, dgx_ui_load(). Every kind and every key, the widgets laid against C constants of
 * the same tree field by field, and the two trees drawn the same, pixel by pixel; keys in any order, "children" first
 * too; a look not said, the kind's own from the table, none for a box, a frame, a label, and its absence an error;
 * "ctx" a name, or a number for a handler marked to take one; ids found by the program, the others left as they were;
 * the actions back and page by a tap.
 * Each error with its code, its offset and its name, the tree as it was: a name unknown or malformed or too long, a
 * kind, a key twice, a key of no meaning for the kind, a fraction, a number out of range, a bad escape, a lone
 * surrogate, a version above 1, the depth past its bound, the memory a byte short and not aligned, the nodes one short
 * (an add refused midway, what was added removed); a number for "ctx" with no handler or one that takes a pointer, a
 * switch or a radio of one picture, an icon's range past its pictures, an id of the table twice, a table of too many
 * ids. The need's bytes exactly what the load uses. A valid description cut
 * at every byte and with every byte changed: never a fault, an error inside the text or a tree, nothing added on an
 * error. The text overwritten after the load, the tree drawn the same. Trees made by chance as C constants, printed to
 * JSON with their keys shuffled, loaded and compared field by field.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_load.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"

/* the allocations refused on demand: a block of nodes that cannot be had (dgx_ui_add() takes it by calloc()) */
void       *__real_calloc(size_t count, size_t size);
static bool refuse_malloc;
void       *__wrap_calloc(size_t count, size_t size)
{
    return refuse_malloc ? NULL : __real_calloc(count, size);
}

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style = {.variants = variant, .count = 1}, style2 = {.variants = variant, .count = 1};

static int32_t                volume, choice = 2, ticked, picked;
static const uint8_t          dot_bits[5] = {0x70, 0xf8, 0xf8, 0xf8, 0x70};
static const dgx_image_t      dot_im      = {.width = 5, .height = 5, .format = DGX_IMAGE_1BPP, .data = dot_bits};
static const dgx_ui_picture_t dots[2]     = {{.image = &dot_im}, {.image = &dot_im}};

static int  saved, stepped, taken;
static void on_saved(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    saved += what == DGX_UI_ACTIVATED;
}
static void on_step(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) stepped += (int)(intptr_t)ctx;
}
static bool on_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    return ++taken, false;
}

static dgx_ui_node_t page_nodes[4];
static dgx_ui_t      page;
static int           title = -77, unused_id = -55, slider_id = -1, less_id = -1, back_id = -1, go_id = -1;

static const dgx_ui_look_name_t    looks[]    = {DGX_UI_LOOK_NAME(button), DGX_UI_LOOK_NAME(slider), DGX_UI_LOOK_NAME(switch), DGX_UI_LOOK_NAME(list),
                                                 DGX_UI_LOOK_NAME(dropdown), DGX_UI_LOOK_NAME(input), DGX_UI_LOOK_NAME(radio), DGX_UI_LOOK_NAME(picture),
                                                 DGX_UI_LOOK_NAME(checkbox), DGX_UI_LOOK_NAME(ring), DGX_UI_LOOK_NAME(picture_button), {"icon", &dgx_ui_look_picture}};
static const dgx_ui_value_name_t   values[]   = {{"volume", &volume}, {"choice", &choice}, {"ticked", &ticked}, {"picked", &picked}};
static const dgx_ui_picture_name_t pictures[] = {{"dot", &dots[0], 1}, {"dots", dots, 2}, {.name = "one", .picture = &dots[1]}};
static const dgx_ui_style_name_t   styles[]   = {{"text", &style}, {"small", &style2}};
static const dgx_ui_on_name_t      handlers[] = {DGX_UI_ACTION_NAME(back), DGX_UI_ACTION_NAME(page), {"saved", on_saved, false}, {"step", on_step, true}};
static const dgx_ui_take_name_t    takes[]    = {{"take", on_take}};
static const dgx_ui_context_name_t contexts[] = {{"settings", &page}, {"other", (void *)&volume}};
static const dgx_ui_id_t           ids[]      = {{"title", &title}, {"unused", &unused_id}, {"slider", &slider_id}, {"less", &less_id}, {"back", &back_id}, {"go", &go_id}};
static const dgx_ui_load_names_t   names      = {.looks = looks, .look_count = 12, .values = values, .value_count = 4, .pictures = pictures, .picture_count = 3,
                                                 .styles = styles, .style_count = 2, .handlers = handlers, .handler_count = 4, .takes = takes, .take_count = 1,
                                                 .contexts = contexts, .context_count = 2, .ids = ids, .id_count = 6};

/* every kind and every key, and the same tree as C constants */
static const char every[] =
    "{\"tree\": {\"children\": ["
    "  {\"kind\": \"label\", \"text\": \"Volume \\u00e9\\ud83d\\ude00\", \"id\": \"title\", \"lines\": 2, \"style\": \"small\", \"text_size\": 40},"
    "  {\"kind\": \"slider\", \"value\": \"volume\", \"min\": -5, \"max\": 100, \"step\": 5, \"want\": [0, 12], \"id\": \"slider\", \"tag\": 7},"
    "  {\"kind\": \"button\", \"text\": \"Less\", \"on\": \"step\", \"flags\": [\"repeat\"], \"ctx\": -5, \"id\": \"less\"},"
    "  {\"kind\": \"frame\", \"flags\": [\"row\", \"stretch\"], \"pad\": 3, \"gap\": 4, \"at\": [2, -3], \"want\": [80, 20], \"take\": \"take\", \"children\": ["
    "     {\"kind\": \"switch\", \"value\": \"ticked\", \"text\": \"Tick\", \"look\": \"checkbox\"},"
    "     {\"kind\": \"radio\", \"value\": \"choice\", \"step\": 2, \"text\": \"Two\", \"state\": [\"disabled\"]}]},"
    "  {\"kind\": \"list\", \"value\": \"picked\", \"max\": 2, \"text\": \"a\\nb\\nc\", \"want\": [0, 30]},"
    "  {\"kind\": \"dropdown\", \"value\": \"picked\", \"max\": 2, \"text\": \"a\\nb\\nc\"},"
    "  {\"kind\": \"input\", \"text_size\": 8},"
    "  {\"kind\": \"picture\", \"picture\": \"dot\"},"
    "  {\"kind\": \"icon\", \"picture\": \"dots\", \"value\": \"picked\", \"max\": 1},"
    "  {\"kind\": \"own\", \"look\": \"ring\", \"value\": \"volume\", \"max\": 100, \"want\": [30, 30], \"pad\": 5, \"children\": [{\"kind\": \"box\", \"look\": null}]},"
    "  {\"kind\": \"button\", \"picture\": \"dot\", \"look\": \"picture_button\", \"on\": \"page\", \"ctx\": \"settings\", \"id\": \"go\", \"state\": [\"hidden\"]},"
    "  {\"kind\": \"button\", \"text\": \"Save\", \"on\": \"saved\", \"ctx\": \"other\"}"
    "], \"kind\": \"box\", \"flags\": [\"column\"], \"pad\": 2, \"gap\": 2}, \"dgx-ui\": 1}";

static char label_text[40] = "Volume \xc3\xa9\xf0\x9f\x98\x80";
static char input_text[8];
static dgx_ui_textbox_t label_box, less_box = DGX_UI_TEXT_CONST("Less", 1, &style), tick_box = DGX_UI_TEXT_CONST("Tick", 1, &style);
static dgx_ui_textbox_t two_box = DGX_UI_TEXT_CONST("Two", 1, &style), list_box = DGX_UI_TEXT_CONST("a\nb\nc", 1, &style), drop_box = DGX_UI_TEXT_CONST("a\nb\nc", 1, &style);
static dgx_ui_textbox_t input_box, save_box = DGX_UI_TEXT_CONST("Save", 1, &style);
static dgx_ui_widget_t  expected[16];
static int              expected_parent[16], expected_count;

static void make_expected(void)
{
    dgx_ui_textbox_init(&label_box, label_text, 40, 2, &style2), strcpy(label_text, "Volume \xc3\xa9\xf0\x9f\x98\x80");
    dgx_ui_textbox_init(&input_box, input_text, 8, 1, &style);
    const dgx_ui_widget_t w[] = {
        {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2},
        {.kind = DGX_UI_LABEL, .text = &label_box},
        {.kind = DGX_UI_SLIDER, .value = &volume, .min = -5, .max = 100, .step = 5, .want_h = 12, .tag = 7, .look = &dgx_ui_look_slider},
        {.kind = DGX_UI_BUTTON, .text = &less_box, .on = on_step, .flags = DGX_UI_REPEAT, .ctx = (void *)(intptr_t)-5, .look = &dgx_ui_look_button},
        {.kind = DGX_UI_FRAME, .flags = DGX_UI_ROW | DGX_UI_STRETCH, .pad = 3, .gap = 4, .at_x = 2, .at_y = -3, .want_w = 80, .want_h = 20, .take = on_take},
        {.kind = DGX_UI_SWITCH, .value = &ticked, .text = &tick_box, .look = &dgx_ui_look_checkbox},
        {.kind = DGX_UI_RADIO, .value = &choice, .step = 2, .text = &two_box, .state = {DGX_UI_DISABLED}, .look = &dgx_ui_look_radio},
        {.kind = DGX_UI_LIST, .value = &picked, .max = 2, .text = &list_box, .want_h = 30, .look = &dgx_ui_look_list},
        {.kind = DGX_UI_DROPDOWN, .value = &picked, .max = 2, .text = &drop_box, .look = &dgx_ui_look_dropdown},
        {.kind = DGX_UI_INPUT, .text = &input_box, .look = &dgx_ui_look_input},
        {.kind = DGX_UI_PICTURE, .picture = &dots[0], .look = &dgx_ui_look_picture},
        {.kind = DGX_UI_ICON, .picture = dots, .value = &picked, .max = 1, .look = &dgx_ui_look_picture},
        {.kind = DGX_UI_OWN, .look = &dgx_ui_look_ring, .value = &volume, .max = 100, .want_w = 30, .want_h = 30, .pad = 5},
        {.kind = DGX_UI_BOX},
        {.kind = DGX_UI_BUTTON, .picture = &dots[0], .look = &dgx_ui_look_picture_button, .on = dgx_ui_load_on_page, .ctx = &page, .state = {DGX_UI_HIDDEN}},
        {.kind = DGX_UI_BUTTON, .text = &save_box, .on = on_saved, .ctx = (void *)&volume, .look = &dgx_ui_look_button},
    };
    const int parents[] = {-1, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 12, 0, 0};
    expected_count      = 16;
    for (int k = 0; k < 16; ++k) expected[k] = w[k], expected_parent[k] = parents[k];
}

/* two widgets the same: every field, the text boxes by their strings, sizes, lines and styles */
static bool same_widget(const dgx_ui_widget_t *a, const dgx_ui_widget_t *b)
{
    if (a->kind != b->kind || a->flags != b->flags || a->pad != b->pad || a->gap != b->gap || a->at_x != b->at_x || a->at_y != b->at_y || a->want_w != b->want_w ||
        a->want_h != b->want_h || a->min != b->min || a->max != b->max || a->step != b->step || a->tag != b->tag || a->state.bits != b->state.bits || a->value != b->value ||
        a->picture != b->picture || a->look != b->look || a->take != b->take || a->on != b->on || a->ctx != b->ctx || a->own != b->own || !a->text != !b->text)
        return false;
    if (!a->text) return true;
    return strcmp(a->text->buf, b->text->buf) == 0 && a->text->size == b->text->size && a->text->lines == b->text->lines && a->text->style == b->text->style;
}

/* the nodes of a tree in its order, parents before children: the widget and the parent's place in the order */
static int order_of(const dgx_ui_t *ui, int i, const dgx_ui_widget_t **out, int *parent, int at, int up)
{
    out[at] = dgx_ui_node(ui, i)->widget, parent[at] = up;
    const int self = at++;
    for (int c = dgx_ui_node(ui, i)->child; c >= 0; c = dgx_ui_node(ui, c)->next) at = order_of(ui, c, out, parent, at, self);
    return at;
}

/* the tree loaded the same as the constants: in the same order, every field */
static int off_expected(const dgx_ui_t *ui, int root)
{
    const dgx_ui_widget_t *got[64];
    int                    parent[64];
    const int              count = order_of(ui, root, got, parent, 0, -1);
    int                    off   = count != expected_count;
    for (int k = 0; k < count && k < expected_count; ++k) off += !same_widget(got[k], &expected[k]) || parent[k] != expected_parent[k];
    return off;
}

/* a tree drawn on a virtual screen of its own */
static dgx_screen_t *draw(dgx_ui_t *ui)
{
    static const uint32_t colors[4] = {0x1082, 0xe73c, 0x29e8, 0xf507};
    dgx_screen_t         *scr       = dgx_vscreen_init(160, 240, 16, DgxScreenRGB);
    dgx_ui_paint_t        paint     = {.scr = scr, .colors = colors, .color_count = 4};
    dgx_ui_paint(ui, &paint, 0);
    dgx_ui_paint_free(&paint);
    return scr;
}
static int pixels_off(dgx_screen_t *a, dgx_screen_t *b)
{
    int off = 0;
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 160; ++x) off += dgx_get_pixel(a, x, y) != dgx_get_pixel(b, x, y);
    return off;
}

/* an error expected of a description */
static bool fails_with(const char *json, int code, int offset, const char *name)
{
    dgx_ui_load_need_t  need;
    dgx_ui_load_error_t err;
    const bool          ok = dgx_ui_load_need(json, strlen(json), &names, &need, &err);
    const bool          right = !ok && err.code == code && (offset < 0 || err.offset == offset) && (!name || strcmp(err.name, name) == 0);
    if (!right) fprintf(stderr, "  %s: code %d offset %d name \"%s\", expected %d %d \"%s\"\n", json, ok ? 0 : err.code, err.offset, err.name, code, offset, name ? name : "");
    return right;
}

/* ---- trees by chance, printed to JSON with their keys shuffled ---- */

static uint32_t seed = 12345;
static int      chance(int below)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 8) % (uint32_t)below);
}

typedef struct printer_t {
    char  *out;
    size_t n, cap;
} printer_t;
static void print(printer_t *p, const char *f, ...)
{
    va_list a;
    va_start(a, f);
    p->n += (size_t)vsnprintf(p->out + p->n, p->n < p->cap ? p->cap - p->n : 0, f, a);
    va_end(a);
}

static dgx_ui_widget_t  chance_w[64];
static dgx_ui_textbox_t chance_box[64];
static char             chance_text[64][16];
static int              chance_parent[64], chance_count;

/* a widget made by chance, its keys printed in a shuffled order, its children after it in the order */
static void chance_widget(printer_t *p, int parent, int depth)
{
    const int        self = chance_count++;
    dgx_ui_widget_t *w    = &chance_w[self];
    *w = (dgx_ui_widget_t){0}, chance_parent[self] = parent;
    static const int holders[] = {DGX_UI_BOX, DGX_UI_FRAME}, leaves[] = {DGX_UI_LABEL, DGX_UI_SLIDER, DGX_UI_BUTTON, DGX_UI_SWITCH, DGX_UI_LIST};
    static const char *const words[] = {"box", "frame", "label", "picture", "icon", "list", "switch", "slider", "button", "input", "dropdown", "own", "radio"};
    const bool holds = depth < 4 && chance_count < 50 && chance(3) == 0;
    w->kind          = (uint8_t)(holds ? holders[chance(2)] : leaves[chance(5)]);
    char keys[12][96];
    int  key_count = 0;
    snprintf(keys[key_count++], 96, "\"kind\": \"%s\"", words[w->kind]);
    if (chance(2)) w->tag = (int16_t)(chance(200) - 100), snprintf(keys[key_count++], 96, "\"tag\": %d", w->tag);
    if (chance(2)) w->want_w = (int16_t)chance(60), w->want_h = (int16_t)chance(30), snprintf(keys[key_count++], 96, "\"want\": [%d, %d]", w->want_w, w->want_h);
    if (chance(3) == 0) w->at_x = (int16_t)(chance(20) - 10), w->at_y = (int16_t)chance(20), snprintf(keys[key_count++], 96, "\"at\": [%d, %d]", w->at_x, w->at_y);
    if (chance(4) == 0) w->state.bits = DGX_UI_DISABLED, snprintf(keys[key_count++], 96, "\"state\": [\"disabled\"]");
    if (holds) {
        w->pad = (uint8_t)chance(5), w->flags = chance(2) ? DGX_UI_COLUMN : DGX_UI_ROW;
        snprintf(keys[key_count++], 96, "\"pad\": %d", w->pad);
        snprintf(keys[key_count++], 96, "\"flags\": [\"%s\"]", w->flags == DGX_UI_COLUMN ? "column" : "row");
    }
    if (w->kind == DGX_UI_SLIDER || w->kind == DGX_UI_SWITCH || w->kind == DGX_UI_LIST) {
        w->value = &volume, w->max = (int16_t)(1 + chance(100));
        snprintf(keys[key_count++], 96, "\"value\": \"volume\"");
        snprintf(keys[key_count++], 96, "\"max\": %d", w->max);
    }
    if (w->kind == DGX_UI_LABEL || w->kind == DGX_UI_BUTTON || w->kind == DGX_UI_LIST) {
        snprintf(chance_text[self], 16, "t%d%c", self, 'a' + chance(26));
        chance_box[self] = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST(chance_text[self], 1, &style);
        w->text          = &chance_box[self];
        snprintf(keys[key_count++], 96, "\"text\": \"%s\"", chance_text[self]);
    }
    if (w->kind == DGX_UI_BUTTON && chance(2)) w->on = on_step, w->ctx = (void *)(intptr_t)(chance(11) - 5), snprintf(keys[key_count++], 96, "\"on\": \"step\""), snprintf(keys[key_count++], 96, "\"ctx\": %d", (int)(intptr_t)w->ctx);
    const dgx_ui_look_t *own = w->kind == DGX_UI_SLIDER ? &dgx_ui_look_slider : w->kind == DGX_UI_BUTTON ? &dgx_ui_look_button : w->kind == DGX_UI_SWITCH ? &dgx_ui_look_switch : w->kind == DGX_UI_LIST ? &dgx_ui_look_list : NULL;
    w->look = own;
    /* the keys shuffled, the children one of them, printed in its turn */
    const int child_count = holds ? 1 + chance(4) : 0;
    if (holds) strcpy(keys[key_count++], "");
    for (int k = key_count - 1; k > 0; --k) {
        const int j = chance(k + 1);
        char      t[96];
        memcpy(t, keys[k], 96), memcpy(keys[k], keys[j], 96), memcpy(keys[j], t, 96);
    }
    print(p, "{");
    for (int k = 0; k < key_count; ++k) {
        if (k) print(p, ", ");
        if (keys[k][0]) {
            print(p, "%s", keys[k]);
            continue;
        }
        print(p, "\"children\": [");
        for (int c = 0; c < child_count; ++c) {
            if (c) print(p, ", ");
            chance_widget(p, self, depth + 1);
        }
        print(p, "]");
    }
    print(p, "}");
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    dgx_ui_init(&page, page_nodes, 4, 1);
    dgx_ui_add(&page, -1, &(dgx_ui_widget_t){.kind = DGX_UI_FRAME});

    /* every kind and key: the widgets as the constants, the ids found and the one not in it left as it was, the need
       the exact bytes, drawn the same as the constant tree */
    make_expected();
    dgx_ui_load_need_t  need;
    dgx_ui_load_error_t err;
    CHECK(dgx_ui_load_need(every, strlen(every), &names, &need, &err) && need.nodes == 16 && need.depth == 3 && need.name[0] == 0);
    static dgx_ui_node_t nodes[24], constant_nodes[24];
    dgx_ui_t             ui, constant;
    void                *memory = malloc(need.bytes);
    dgx_ui_init(&ui, nodes, 24, 1);
    CHECK(dgx_ui_load(&ui, -1, every, strlen(every), &names, memory, need.bytes - 1, &err) < 0 && err.code == DGX_UI_LOAD_MEMORY && ui.count == 0);
    CHECK(dgx_ui_load(&ui, -1, every, strlen(every), &names, (uint8_t *)memory + 1, need.bytes, &err) < 0 && err.code == DGX_UI_LOAD_MEMORY && ui.count == 0);
    const int root = dgx_ui_load(&ui, -1, every, strlen(every), &names, memory, need.bytes, &err);
    CHECK(root == 0 && off_expected(&ui, root) == 0);
    CHECK(title == 1 && slider_id == 2 && less_id == 3 && go_id == 14 && unused_id == -55 && back_id == -1);
    dgx_ui_init(&constant, constant_nodes, 24, 1);
    int at[16];
    for (int k = 0; k < 16; ++k) at[k] = dgx_ui_add(&constant, expected_parent[k] < 0 ? -1 : at[expected_parent[k]], &expected[k]);
    dgx_screen_t *a = draw(&ui), *b = draw(&constant);
    CHECK(pixels_off(a, b) == 0);
    dgx_screen_destroy(&a), dgx_screen_destroy(&b);
    /* the program sets the label it found */
    dgx_ui_text_set(&ui, title, "Sound");
    CHECK(strcmp(dgx_ui_widget(&ui, title)->text->buf, "Sound") == 0);
    dgx_ui_free(&ui), dgx_ui_free(&constant), free(memory);

    /* the text overwritten after the load: the tree as it was, drawn the same */
    {
        char *text = strdup(every);
        make_expected();
        CHECK(dgx_ui_load_need(text, strlen(text), &names, &need, &err));
        memory = malloc(need.bytes);
        dgx_ui_init(&ui, nodes, 24, 1);
        CHECK(dgx_ui_load(&ui, -1, text, strlen(text), &names, memory, need.bytes, &err) == 0);
        memset(text, 'x', strlen(text)), free(text);
        CHECK(off_expected(&ui, 0) == 0);
        dgx_ui_init(&constant, constant_nodes, 24, 1);
        for (int k = 0; k < 16; ++k) at[k] = dgx_ui_add(&constant, expected_parent[k] < 0 ? -1 : at[expected_parent[k]], &expected[k]);
        a = draw(&ui), b = draw(&constant);
        CHECK(pixels_off(a, b) == 0);
        dgx_screen_destroy(&a), dgx_screen_destroy(&b);
        dgx_ui_free(&ui), dgx_ui_free(&constant), free(memory);
    }

    /* the actions: a tap on the picture button opens the page named, the page's back button goes back */
    {
        static const char app[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"children\": ["
                                  "{\"kind\": \"button\", \"text\": \"Go\", \"on\": \"page\", \"ctx\": \"settings\", \"want\": [40, 16], \"id\": \"go\"}]}}";
        static const char in_page[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"button\", \"text\": \"Back\", \"on\": \"back\", \"want\": [40, 16], \"id\": \"back\"}}";
        static uint8_t    app_memory[1024] __attribute__((aligned(8))), page_memory[512] __attribute__((aligned(8)));
        dgx_ui_init(&ui, nodes, 24, 1);
        dgx_ui_init(&page, page_nodes, 4, 1);
        CHECK(dgx_ui_load(&ui, -1, app, strlen(app), &names, app_memory, sizeof app_memory, &err) == 0);
        CHECK(dgx_ui_load(&page, -1, in_page, strlen(in_page), &names, page_memory, sizeof page_memory, &err) == 0);
        dgx_screen_t *scr = draw(&ui);
        const dgx_ui_node_t *g = dgx_ui_node(&ui, go_id);
        dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(g->x + 5), .y = (int16_t)(g->y + 5), .down = true, .pressed = true, .ms = 10});
        dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(g->x + 5), .y = (int16_t)(g->y + 5), .released = true, .ms = 40});
        CHECK(ui.popup == &page);
        dgx_screen_destroy(&scr);
        scr = draw(&ui);
        const dgx_ui_node_t *k = dgx_ui_node(&page, back_id);
        dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(k->x + 5), .y = (int16_t)(k->y + 5), .down = true, .pressed = true, .ms = 100});
        dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(k->x + 5), .y = (int16_t)(k->y + 5), .released = true, .ms = 130});
        CHECK(ui.popup == NULL);
        dgx_screen_destroy(&scr);
        dgx_ui_free(&page), dgx_ui_free(&ui);
    }

    /* the errors, each with its code, offset and name; the tree as it was */
    CHECK(fails_with("{\"dgx-ui\": 2, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 0, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\"}, \"fonts\": 1}", DGX_UI_LOAD_KEY, 39, "fonts"));

    /* the tree's name: before, between and after the other two keys, written with escapes, given out by the need; the
       tree the same as with none */
    {
        static const char *const named[] = {
            "{\"name\": \"settings\", \"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"pad\": 3}}",
            "{\"dgx-ui\": 1, \"name\": \"settings\", \"tree\": {\"kind\": \"box\", \"pad\": 3}}",
            "{\"tree\": {\"kind\": \"box\", \"pad\": 3}, \"dgx-ui\": 1, \"name\": \"settings\"}",
            "{\"dgx-ui\": 1, \"\\u006eame\": \"s\\u0065ttings\", \"tree\": {\"kind\": \"box\", \"pad\": 3}}",
        };
        for (size_t k = 0; k < sizeof named / sizeof *named; ++k) {
            static uint8_t named_memory[256] __attribute__((aligned(8)));
            static dgx_ui_node_t named_nodes[2];
            dgx_ui_t             named_ui;
            dgx_ui_init(&named_ui, named_nodes, 2, 1);
            CHECK(dgx_ui_load_need(named[k], strlen(named[k]), &names, &need, &err) && strcmp(need.name, "settings") == 0 && need.nodes == 1);
            const int top = dgx_ui_load(&named_ui, -1, named[k], strlen(named[k]), &names, named_memory, sizeof named_memory, &err);
            CHECK(top == 0 && dgx_ui_node(&named_ui, 0)->widget->kind == DGX_UI_BOX && dgx_ui_node(&named_ui, 0)->widget->pad == 3);
            dgx_ui_free(&named_ui);
        }
        static const char longest[] = "{\"dgx-ui\": 1, \"name\": \"a2345678901234567890123\", \"tree\": {\"kind\": \"box\"}}";
        CHECK(dgx_ui_load_need(longest, strlen(longest), &names, &need, &err) && strcmp(need.name, "a2345678901234567890123") == 0);
        CHECK(fails_with("{\"dgx-ui\": 1, \"name\": \"a23456789012345678901234\", \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_NAME_BAD, 22, "a2345678901234567890123"));
        CHECK(fails_with("{\"dgx-ui\": 1, \"name\": \"\", \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_NAME_BAD, 22, ""));
        CHECK(fails_with("{\"dgx-ui\": 1, \"name\": \"a/b\", \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_NAME_BAD, 22, "a/b"));
        CHECK(fails_with("{\"dgx-ui\": 1, \"name\": null, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_SYNTAX, 22, NULL));
        CHECK(fails_with("{\"dgx-ui\": 1, \"name\": 5, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_SYNTAX, 22, NULL));
        CHECK(fails_with("{\"dgx-ui\": 1, \"name\": \"a\", \"name\": \"b\", \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 27, "name"));
        CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"name\": \"x\"}}", DGX_UI_LOAD_KEY, 38, "name"));
    }

    /* a later version told before a key of the top this one does not know, whatever their order; the first such key told
       when the version is this one */
    CHECK(fails_with("{\"fonts\": {\"a\": [1, {\"b\": 2}]}, \"dgx-ui\": 2, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 32, NULL));
    CHECK(fails_with("{\"dgx-ui\": 2, \"pages\": [], \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"x\": 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 0, NULL));
    CHECK(fails_with("{\"x\": 1, \"dgx-ui\": 1, \"y\": 2, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 1, "x"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"dgx-ui\": 2, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 14, "dgx-ui"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"boxx\"}}", DGX_UI_LOAD_KIND, 31, "boxx"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"pad\": 1}}", DGX_UI_LOAD_KIND, 22, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 38, "kind"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"label\", \"value\": \"volume\"}}", DGX_UI_LOAD_FIELD, 40, "value"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"colour\": 1}}", DGX_UI_LOAD_KEY, 38, "colour"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"pad\": 1.5}}", DGX_UI_LOAD_RANGE, 45, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"pad\": 256}}", DGX_UI_LOAD_RANGE, 45, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"pad\": 1e2}}", DGX_UI_LOAD_RANGE, 45, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"slider\", \"value\": \"loudness\"}}", DGX_UI_LOAD_UNKNOWN, 50, "loudness"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"slider\", \"value\": \"vol ume\"}}", DGX_UI_LOAD_NAME_BAD, 50, "vol ume"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"slider\", \"value\": \"aaaaaaaaaaaaaaaaaaaaaaaab\"}}", DGX_UI_LOAD_NAME_BAD, 50, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"label\", \"text\": \"\\ud83d\"}}", DGX_UI_LOAD_SYNTAX, 49, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"label\", \"text\": \"\\q\"}}", DGX_UI_LOAD_SYNTAX, 49, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"label\", \"text\": \"\\udc00\"}}", DGX_UI_LOAD_SYNTAX, 49, NULL)); /* a low surrogate alone */
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\"}} x", DGX_UI_LOAD_SYNTAX, 39, NULL));                       /* text after the description */
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"label\", \"text\": \"\\u0000\"}}", DGX_UI_LOAD_SYNTAX, 49, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"label\", \"text\": \"abc\", \"text_size\": 3}}", DGX_UI_LOAD_RANGE, 55, "text_size"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"own\"}}", DGX_UI_LOAD_NO_LOOK, 22, "own"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"label\", \"lines\": 2}}", DGX_UI_LOAD_FIELD, 40, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\"} x}", DGX_UI_LOAD_SYNTAX, 38, NULL));
    /* a description names only what the program listed: a number for ctx to a handler marked to take one alone; a
       switch's or a radio's two pictures, an icon's range, within the count of the pictures; an id of the table once */
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"button\", \"on\": \"page\", \"ctx\": 5}}", DGX_UI_LOAD_FIELD, 55, "ctx"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"button\", \"ctx\": 5}}", DGX_UI_LOAD_FIELD, 41, "ctx"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"button\", \"ctx\": 5, \"on\": \"saved\"}}", DGX_UI_LOAD_FIELD, 41, "ctx"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"switch\", \"picture\": \"dot\", \"value\": \"ticked\", \"look\": \"picture_button\"}}", DGX_UI_LOAD_RANGE, 41, "dot"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"radio\", \"value\": \"choice\", \"picture\": \"dot\"}}", DGX_UI_LOAD_RANGE, 59, "dot"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"icon\", \"picture\": \"dots\", \"value\": \"picked\", \"max\": 2}}", DGX_UI_LOAD_RANGE, 39, "dots"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"icon\", \"min\": 3, \"max\": 4, \"value\": \"picked\", \"picture\": \"dot\"}}", DGX_UI_LOAD_RANGE, 78, "dot"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"children\": [{\"kind\": \"label\", \"id\": \"title\"}, {\"kind\": \"box\", \"children\": [{\"kind\": \"label\", \"id\": \"title\"}]}]}}", DGX_UI_LOAD_ID, 132, "title"));
    {
        static const char *loads[] = {
            "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"button\", \"ctx\": 5, \"on\": \"step\"}}",
            "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"switch\", \"picture\": \"dots\", \"value\": \"ticked\", \"look\": \"picture_button\"}}",
            "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"icon\", \"picture\": \"dots\", \"value\": \"picked\", \"min\": 3, \"max\": 4}}",
            "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"icon\", \"picture\": \"dot\", \"value\": \"picked\"}}",
            "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"picture\", \"picture\": \"one\"}}", /* a count of 0: one picture */
            "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"children\": [{\"kind\": \"label\", \"id\": \"free\"}, {\"kind\": \"label\", \"id\": \"free\"}]}}",
        };
        static uint8_t small_memory[512] __attribute__((aligned(8)));
        for (size_t k = 0; k < sizeof loads / sizeof *loads; ++k) {
            dgx_ui_init(&ui, nodes, 24, 1);
            const int r = dgx_ui_load(&ui, -1, loads[k], strlen(loads[k]), &names, small_memory, sizeof small_memory, &err);
            if (r != 0) fprintf(stderr, "  %s: code %d offset %d name \"%s\"\n", loads[k], err.code, err.offset, err.name);
            CHECK(r == 0);
            if (k == 0) CHECK(dgx_ui_widget(&ui, 0)->on == on_step && dgx_ui_widget(&ui, 0)->ctx == (void *)(intptr_t)5);
            dgx_ui_free(&ui);
        }
        /* the id twice: no id written, nothing added */
        static const char twice[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"children\": [{\"kind\": \"label\", \"id\": \"go\"}, {\"kind\": \"label\", \"id\": \"go\"}]}}";
        go_id = -9;
        dgx_ui_init(&ui, nodes, 24, 1);
        CHECK(dgx_ui_load(&ui, -1, twice, strlen(twice), &names, small_memory, sizeof small_memory, &err) < 0 && err.code == DGX_UI_LOAD_ID && ui.count == 0 && go_id == -9);
        dgx_ui_free(&ui);
        /* a table of more ids than DGX_UI_LOAD_IDS refused, its count at the bound taken */
        static dgx_ui_id_t   many[DGX_UI_LOAD_IDS + 1];
        static int           many_index[DGX_UI_LOAD_IDS + 1];
        static char          many_name[DGX_UI_LOAD_IDS + 1][8];
        for (int k = 0; k <= DGX_UI_LOAD_IDS; ++k) snprintf(many_name[k], 8, "i%d", k), many[k] = (dgx_ui_id_t){many_name[k], &many_index[k]}, many_index[k] = -1;
        dgx_ui_load_names_t more = names;
        more.ids = many, more.id_count = DGX_UI_LOAD_IDS + 1;
        static const char last[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"label\", \"id\": \"i255\"}}";
        CHECK(!dgx_ui_load_need(last, strlen(last), &more, &need, &err) && err.code == DGX_UI_LOAD_ID);
        more.id_count = DGX_UI_LOAD_IDS;
        CHECK(dgx_ui_load_need(last, strlen(last), &more, &need, &err));
        memory = malloc(need.bytes);
        dgx_ui_init(&ui, nodes, 24, 1);
        CHECK(dgx_ui_load(&ui, -1, last, strlen(last), &more, memory, need.bytes, &err) == 0 && many_index[255] == 0 && many_index[256] == -1);
        dgx_ui_free(&ui), free(memory);
    }
    {
        /* the depth: DGX_UI_LOAD_DEPTH boxes in one another loaded, one more refused */
        char deep[2048] = "{\"dgx-ui\": 1, \"tree\": ";
        for (int k = 0; k < DGX_UI_LOAD_DEPTH; ++k) strcat(deep, k ? ", \"children\": [{\"kind\": \"box\"" : "{\"kind\": \"box\"");
        for (int k = 0; k < DGX_UI_LOAD_DEPTH; ++k) strcat(deep, k < DGX_UI_LOAD_DEPTH - 1 ? "}]" : "}");
        strcat(deep, "}");
        CHECK(dgx_ui_load_need(deep, strlen(deep), &names, &need, &err) && need.depth == DGX_UI_LOAD_DEPTH && need.nodes == DGX_UI_LOAD_DEPTH);
        char deeper[2048] = "{\"dgx-ui\": 1, \"tree\": ";
        for (int k = 0; k <= DGX_UI_LOAD_DEPTH; ++k) strcat(deeper, k ? ", \"children\": [{\"kind\": \"box\"" : "{\"kind\": \"box\"");
        for (int k = 0; k <= DGX_UI_LOAD_DEPTH; ++k) strcat(deeper, k < DGX_UI_LOAD_DEPTH ? "}]" : "}");
        strcat(deeper, "}");
        CHECK(!dgx_ui_load_need(deeper, strlen(deeper), &names, &need, &err) && err.code == DGX_UI_LOAD_DEPTH_MAX);
        /* lists in one another past the levels a description may have, in a key the walk has not yet refused */
        char lists[256] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"x\": ";
        for (int k = 0; k < 60; ++k) strcat(lists, "[");
        for (int k = 0; k < 60; ++k) strcat(lists, "]");
        strcat(lists, "}}");
        CHECK(fails_with(lists, DGX_UI_LOAD_DEPTH_MAX, 92, NULL));
    }

    /* the nodes one short: an add refused midway, what was added removed, the ids as they were */
    {
        static dgx_ui_node_t few[4];
        dgx_ui_init(&ui, few, 4, 1);
        dgx_ui_add(&ui, -1, &(dgx_ui_widget_t){.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN});
        CHECK(dgx_ui_load_need(every, strlen(every), &names, &need, &err));
        memory = malloc(need.bytes);
        title = -77, slider_id = -1;
        refuse_malloc = true;
        CHECK(dgx_ui_load(&ui, 0, every, strlen(every), &names, memory, need.bytes, &err) < 0);
        refuse_malloc = false;
        CHECK(err.code == DGX_UI_LOAD_NODES && ui.count == 1 && dgx_ui_node(&ui, 0)->child < 0 && title == -77 && slider_id == -1);
        dgx_ui_free(&ui), free(memory);
    }

    /* cut at every byte, and every byte changed: never a fault; an error inside the text and nothing added, or a tree */
    {
        const size_t full = strlen(every);
        char        *copy = malloc(full);
        memory            = malloc(16384);
        int errors = 0, loads = 0, outside = 0, added = 0;
        for (size_t cut = 0; cut < full; ++cut) {
            memcpy(copy, every, full);
            dgx_ui_init(&ui, nodes, 24, 1);
            const int r = dgx_ui_load(&ui, -1, copy, cut, &names, memory, 16384, &err);
            errors += r < 0, outside += r < 0 && (err.offset < 0 || (size_t)err.offset > cut), added += r < 0 && ui.count != 0;
            dgx_ui_free(&ui);
        }
        static const char bytes[] = {'"', '{', '}', '[', ']', ',', ':', '\\', '0', '-', 'x', ' ', '\0', (char)0xc3, (char)0xff};
        for (size_t k = 0; k < full; ++k)
            for (size_t b = 0; b < sizeof bytes; ++b) {
                memcpy(copy, every, full);
                copy[k] = bytes[b];
                dgx_ui_init(&ui, nodes, 24, 1);
                const int r = dgx_ui_load(&ui, -1, copy, full, &names, memory, 16384, &err);
                if (r < 0) ++errors, outside += err.offset < 0 || (size_t)err.offset > full, added += ui.count != 0;
                else ++loads;
                dgx_ui_free(&ui);
            }
        CHECK(errors > 0 && outside == 0 && added == 0);
        free(copy), free(memory);
    }

    /* trees by chance, printed with their keys shuffled: loaded the same, field by field */
    {
        static char text[8192];
        memory  = malloc(32768);
        int off = 0, made = 0;
        for (int round = 0; round < 400; ++round) {
            printer_t  p             = {.out = text, .cap = sizeof text};
            const bool version_first = chance(2);
            const int  name_place    = chance(4); /* the name before the two keys, between them, after them; 3: none */
            chance_count             = 0;
            print(&p, "{");
            for (int place = 0; place < 3; ++place) {
                if (place == name_place) print(&p, "%s\"name\": \"round%d\"", place ? ", " : "", round);
                if (place == 2) break;
                if (place || name_place == 0) print(&p, ", ");
                if ((place == 0) == version_first) print(&p, "\"dgx-ui\": 1");
                else print(&p, "\"tree\": "), chance_widget(&p, -1, 1);
            }
            print(&p, "}");
            if (p.n >= sizeof text) continue;
            {
                char said[24] = "";
                if (name_place < 3) snprintf(said, sizeof said, "round%d", round);
                off += !dgx_ui_load_need(text, p.n, &names, &need, &err) || strcmp(need.name, said) != 0;
            }
            static dgx_ui_node_t chance_nodes[64];
            dgx_ui_init(&ui, chance_nodes, 64, 1);
            const int r = dgx_ui_load(&ui, -1, text, p.n, &names, memory, 32768, &err);
            if (r != 0) {
                ++off;
                fprintf(stderr, "  round %d: code %d at %d \"%s\" in %.*s\n", round, err.code, err.offset, err.name, (int)p.n, text);
                dgx_ui_free(&ui);
                continue;
            }
            const dgx_ui_widget_t *got[64];
            int                    parent[64];
            const int              count = order_of(&ui, 0, got, parent, 0, -1);
            off += count != chance_count;
            for (int k = 0; k < count && k < chance_count; ++k) off += !same_widget(got[k], &chance_w[k]) || parent[k] != chance_parent[k];
            ++made;
            dgx_ui_free(&ui);
        }
        CHECK(off == 0 && made > 300);
        free(memory);
    }

    dgx_ui_free(&page);
    CHECK_DONE();
}
