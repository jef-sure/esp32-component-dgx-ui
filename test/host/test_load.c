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
 * JSON with their keys shuffled, loaded and compared field by field, and printed again with editors' objects of chance
 * content at chance places: the same need and the same tree. The top judged in one order, the version first; a
 * number that is not an integer of int32 where none is read told by the fault of its place; the editor's object at
 * the top and in widgets, twice, not an object, what is not JSON in it, an "editor" that is not a widget's; a value
 * inside at most 50 brackets, an editor's as any. A key of the top that is no name, one unknown. A name cut at its
 * 23rd byte given on a whole character.
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
static dgx_ui_text_style_t   style = {.fonts = variant, .count = 1}, style2 = {.fonts = variant, .count = 1};

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

/* the root widget of a page loaded alone, or NULL refused */
static const dgx_ui_widget_t *loaded_root(const char *json)
{
    static dgx_ui_node_t nodes_one[4];
    static dgx_ui_t      tree;
    static void         *room[256]; /* aligned as a pointer */
    static bool          used;
    if (used) dgx_ui_free(&tree);
    used = true;
    dgx_ui_init(&tree, nodes_one, 4, 1);
    dgx_ui_load_error_t err;
    return dgx_ui_load(&tree, -1, json, strlen(json), &names, room, sizeof room, &err) < 0 ? NULL : dgx_ui_widget(&tree, 0);
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

/* editors' objects by a chance of their own, so that a tree printed with them and without is the same tree */
static bool     editors;
static uint32_t editor_seed = 777;
static int      editor_chance(int below)
{
    editor_seed = editor_seed * 1103515245u + 12345u;
    return (int)((editor_seed >> 8) % (uint32_t)below);
}

static void chance_editor(printer_t *p, int levels);

/* a value of an editor's: numbers of every kind and size, strings, words, lists and objects, levels deep at most */
static void chance_editor_value(printer_t *p, int levels)
{
    static const char *const scalars[] = {"1.5", "-2.25e-3", "1E+9", "123456789012345678901234567890", "0", "-0", "true", "false", "null",
                                          "\"a note\"", "\"\\u00e9\\n\\ud83d\\ude00\"", "0.5", "1763712000000", "-7"};
    const int                kind      = levels > 0 ? editor_chance(4) : 0;
    if (kind < 2) {
        print(p, "%s", scalars[editor_chance(sizeof scalars / sizeof *scalars)]);
    } else if (kind == 2) {
        const int count = editor_chance(4);
        print(p, "[");
        for (int k = 0; k < count; ++k) print(p, "%s", k ? ", " : ""), chance_editor_value(p, levels - 1);
        print(p, "]");
    } else {
        chance_editor(p, levels);
    }
}

/* an editor's object */
static void chance_editor(printer_t *p, int levels)
{
    const int members = editor_chance(4);
    print(p, "{");
    for (int k = 0; k < members; ++k) print(p, "%s\"k%d\": ", k ? ", " : "", k), chance_editor_value(p, levels - 1);
    print(p, "}");
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
    /* an editor's object among the keys, by its own chance */
    const int editor_at = editors && editor_chance(2) ? editor_chance(key_count + 1) : -1;
    print(p, "{");
    for (int k = 0; k <= key_count; ++k) {
        if (k == editor_at) print(p, "%s\"editor\": ", k ? ", " : ""), chance_editor(p, 4);
        if (k == key_count) break;
        if (k || editor_at == 0) print(p, ", ");
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
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
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

    /* the walk's shapes: "children" before the other keys and after them, lists of children empty at every depth with
       keys after them, editors between: each widget under its parent, in its order, its keys all read */
    {
        static const char shapes[] = "{\"dgx-ui\": 1, \"tree\": {\"children\": ["
                                     "{\"kind\": \"box\", \"children\": [], \"gap\": 3, \"editor\": {\"a\": [[], {}]}},"
                                     "{\"pad\": 1, \"children\": [{\"kind\": \"box\", \"children\": [{\"kind\": \"box\", \"children\": [], \"tag\": 7}], \"tag\": 6}], \"kind\": \"frame\", \"tag\": 5},"
                                     "{\"editor\": {}, \"kind\": \"box\", \"tag\": 8, \"children\": []}], \"kind\": \"box\", \"pad\": 2}}";
        static const int expected_parent[] = {-1, 0, 0, 2, 3, 0}, expected_tag[] = {0, 0, 5, 6, 7, 8};
        static dgx_ui_node_t shape_nodes[8];
        static uint8_t       shape_memory[1024] __attribute__((aligned(8)));
        dgx_ui_t             shape_ui;
        CHECK(dgx_ui_load_need(shapes, strlen(shapes), &names, &need, &err) && need.nodes == 6 && need.depth == 4);
        dgx_ui_init(&shape_ui, shape_nodes, 8, 1);
        CHECK(dgx_ui_load(&shape_ui, -1, shapes, strlen(shapes), &names, shape_memory, sizeof shape_memory, &err) == 0);
        const dgx_ui_widget_t *got[8];
        int                    parent[8];
        CHECK(order_of(&shape_ui, 0, got, parent, 0, -1) == 6);
        for (int k = 0; k < 6; ++k) CHECK(parent[k] == expected_parent[k] && got[k]->tag == expected_tag[k]);
        CHECK(got[0]->pad == 2 && got[1]->gap == 3 && got[2]->pad == 1 && got[2]->kind == DGX_UI_FRAME);
        dgx_ui_free(&shape_ui);
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
    /* the top judged in one order: not JSON; the version, the integer 1 as written; a key unknown or twice, no tree; the
       values of the keys known. So a later version is told as one whatever it holds, and a value of no number read is
       told by the fault of its place, not by its number */
    CHECK(fails_with("{\"dgx-ui\": 1.5, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1.0, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1e0, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"dgx-ui\": \"1\", \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"dgx-ui\": 99999999999, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"dgx-ui\": [1], \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"dgx-ui\": 01, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_SYNTAX, 11, NULL));
    CHECK(fails_with("{\"dgx-ui\": 2, \"tree\": {\"kind\": \"box\", \"pad\": 1.5}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"dgx-ui\": 2, \"tree\": {\"kind\": \"box\", \"scale\": 0.5}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"dgx-ui\": 2, \"name\": 5, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"dgx-ui\": 2, \"tree\": [1.5]}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"opacity\": 0.5, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 14, "opacity"));
    /* a key of the top that is no name: one unknown, after the version, by its first 23 bytes; a key that is no string
       is not JSON. A widget's key that is no name stays one */
    CHECK(fails_with("{\"dgx-ui\": 2, \"a b\": 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
    CHECK(fails_with("{\"a b\": 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 0, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"a b\": 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 14, "a b"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"a2345678901234567890123456789\": 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 14, "a2345678901234567890123"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"\": 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 14, ""));
    CHECK(fails_with("{\"dgx-ui\": 1, \"\\u00e9t\\u00e9\": 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 14, "\xc3\xa9t\xc3\xa9"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\"}, \"tree_of_a_later_version_x\": {}}", DGX_UI_LOAD_KEY, 39, "tree_of_a_later_version"));
    CHECK(fails_with("{\"dgx-ui\": 1, 5: 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_SYNTAX, 14, NULL));
    CHECK(fails_with("{\"dgx-ui\": 2, \"a\\u0000\": 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_SYNTAX, 16, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"a b\": 1}}", DGX_UI_LOAD_NAME_BAD, 38, "a b"));
    /* a name cut at its 23rd byte ends on a whole character: one of 2, 3 or 4 bytes across the 23rd left out, one that
       ends on it kept; a top key and a widget's name alike */
    {
        static const struct {
            int         ascii;     /* letters before the character */
            const char *character; /* of 2, 3 or 4 bytes */
            int         kept;      /* the bytes of the name given */
        } cuts[] = {{22, "\xc3\xa9", 22}, {21, "\xc3\xa9", 23}, {21, "\xe2\x82\xac", 21}, {22, "\xe2\x82\xac", 22}, {20, "\xe2\x82\xac", 23},
                    {20, "\xf0\x9f\x98\x80", 20}, {21, "\xf0\x9f\x98\x80", 21}, {22, "\xf0\x9f\x98\x80", 22}, {19, "\xf0\x9f\x98\x80", 23}};
        for (size_t k = 0; k < sizeof cuts / sizeof *cuts; ++k) {
            char name[40], text[160], expected[24];
            memset(name, 'a', (size_t)cuts[k].ascii);
            snprintf(name + cuts[k].ascii, sizeof name - (size_t)cuts[k].ascii, "%sbc", cuts[k].character);
            memcpy(expected, name, (size_t)cuts[k].kept), expected[cuts[k].kept] = 0;
            snprintf(text, sizeof text, "{\"dgx-ui\": 1, \"%s\": 1, \"tree\": {\"kind\": \"box\"}}", name);
            CHECK(fails_with(text, DGX_UI_LOAD_KEY, 14, expected));
            snprintf(text, sizeof text, "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"slider\", \"value\": \"%s\"}}", name);
            CHECK(fails_with(text, DGX_UI_LOAD_NAME_BAD, 50, expected));
        }
        /* not cut: as it was */
        CHECK(fails_with("{\"dgx-ui\": 1, \"\xc3\xa9t\xc3\xa9\": 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 14, "\xc3\xa9t\xc3\xa9"));
        CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"slider\", \"value\": \"vol\xe2\x82\xacme\"}}", DGX_UI_LOAD_NAME_BAD, 50, "vol\xe2\x82\xacme"));
        CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"slider\", \"value\": \"aaaaaaaaaaaaaaaaaaaaaaaab\"}}", DGX_UI_LOAD_NAME_BAD, 50, "aaaaaaaaaaaaaaaaaaaaaaa"));
    }
    CHECK(fails_with("{\"dgx-ui\": 1, \"name\": 5, \"x\": 1, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 25, "x"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": 5, \"x\": 1}", DGX_UI_LOAD_KEY, 25, "x"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"name\": 5}", DGX_UI_LOAD_KEY, 0, "tree"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": 5, \"name\": 5}", DGX_UI_LOAD_SYNTAX, 33, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": 5}", DGX_UI_LOAD_SYNTAX, 22, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"opacity\": 0.5}}", DGX_UI_LOAD_KEY, 38, "opacity"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"opacity\": 12345678901}}", DGX_UI_LOAD_KEY, 38, "opacity"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"label\", \"text\": 1.5}}", DGX_UI_LOAD_SYNTAX, 48, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [0.5]}}", DGX_UI_LOAD_SYNTAX, 48, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"children\": [1.5]}}", DGX_UI_LOAD_SYNTAX, 51, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": 2.5}}", DGX_UI_LOAD_SYNTAX, 31, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"tag\": 1.5}}", DGX_UI_LOAD_RANGE, 45, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"at\": [1, 2.5]}}", DGX_UI_LOAD_RANGE, 48, NULL));

    /* the editor's object, at the top and in a widget of any kind: any JSON in it, numbers of every kind, passed over */
    {
        static const char *const taken[] = {
            "{\"dgx-ui\": 1, \"editor\": {\"zoom\": 1.5, \"big\": 123456789012345678901234567890, \"e\": -2.5e-3, \"E\": 1E+9, \"z\": -0, \"n\": [1, {\"a\": null}]}, \"tree\": {\"kind\": \"box\"}}",
            "{\"editor\": {}, \"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"editor\": {\"note\": \"x\", \"lock\": true, \"at\": [0.5, 1e3]}, \"children\": [{\"kind\": \"label\", \"text\": \"a\", \"editor\": {\"z\": 1.25}}]}}",
            "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"\\u0065ditor\": {\"z\": 1.25}}}",
            "{\"dgx-ui\": 1, \"tree\": {\"editor\": {\"dgx-ui\": {\"time\": 1763712000000}}, \"kind\": \"slider\", \"value\": \"volume\"}}",
        };
        for (size_t k = 0; k < sizeof taken / sizeof *taken; ++k) CHECK(dgx_ui_load_need(taken[k], strlen(taken[k]), &names, &need, &err));
        /* the need and the tree of the same description with no editors */
        static const char with[]    = "{\"dgx-ui\": 1, \"editor\": {\"zoom\": 1.5}, \"name\": \"p\", \"tree\": {\"kind\": \"box\", \"editor\": {\"x\": 0.5}, \"children\": [{\"kind\": \"label\", \"text\": \"abc\", \"editor\": {}}]}}";
        static const char without[] = "{\"dgx-ui\": 1, \"name\": \"p\", \"tree\": {\"kind\": \"box\", \"children\": [{\"kind\": \"label\", \"text\": \"abc\"}]}}";
        dgx_ui_load_need_t with_need, without_need;
        CHECK(dgx_ui_load_need(with, strlen(with), &names, &with_need, &err) && dgx_ui_load_need(without, strlen(without), &names, &without_need, &err));
        CHECK(memcmp(&with_need, &without_need, sizeof with_need) == 0);
        /* not an object: at once at the top, as the tree; in a widget where the widget is read, after the version */
        CHECK(fails_with("{\"dgx-ui\": 1, \"editor\": 5, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_SYNTAX, 24, NULL));
        CHECK(fails_with("{\"dgx-ui\": 2, \"editor\": 5, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_VERSION, 1, NULL));
        CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"editor\": 1.5}}", DGX_UI_LOAD_SYNTAX, 48, NULL));
        CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"editor\": [1.5]}}", DGX_UI_LOAD_SYNTAX, 48, NULL));
        CHECK(fails_with("{\"dgx-ui\": 2, \"tree\": {\"kind\": \"box\", \"editor\": 1.5}}", DGX_UI_LOAD_VERSION, 1, NULL));
        /* twice: a key twice */
        CHECK(fails_with("{\"dgx-ui\": 1, \"editor\": {}, \"tree\": {\"kind\": \"box\"}, \"editor\": {\"a\": 0.5}}", DGX_UI_LOAD_KEY, 53, "editor"));
        CHECK(fails_with("{\"dgx-ui\": 1, \"editor\": {}, \"tree\": {\"kind\": \"box\"}, \"editor\": 0.5}", DGX_UI_LOAD_KEY, 53, "editor"));
        CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"editor\": {}, \"editor\": {\"a\": 0.5}}}", DGX_UI_LOAD_KEY, 52, "editor"));
        CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\"}, \"tree\": {\"kind\": \"box\", \"editor\": {\"a\": 0.5}}}", DGX_UI_LOAD_KEY, 39, "tree"));
        /* what is not JSON inside, at its byte, before the version: a number at its first byte, a string at the byte of
           its fault, by the rule of every string */
        static const struct {
            const char *value;
            int         fault; /* within the value */
        } broken[] = {{"1.", 0}, {"01", 0}, {"-", 0}, {"1e+", 0}, {".5", 0}, {"+1", 0}, {"NaN", 0}, {"\"\\u0000\"", 1}, {"\"\\ud83d\"", 1}, {"\"a\x01\"", 2}};
        for (size_t k = 0; k < sizeof broken / sizeof *broken; ++k) {
            char text[96];
            snprintf(text, sizeof text, "{\"dgx-ui\": 2, \"editor\": {\"a\": %s}, \"tree\": {\"kind\": \"box\"}}", broken[k].value);
            CHECK(fails_with(text, DGX_UI_LOAD_SYNTAX, 30 + broken[k].fault, NULL));
            snprintf(text, sizeof text, "{\"dgx-ui\": 2, \"tree\": {\"kind\": \"box\", \"editor\": {\"a\": %s}}}", broken[k].value);
            CHECK(fails_with(text, DGX_UI_LOAD_SYNTAX, 54 + broken[k].fault, NULL));
        }
        CHECK(fails_with("{\"dgx-ui\": 1, \"editor\": {\"a\": 1.5.3}, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_SYNTAX, 33, NULL));
        /* an "editor" that is not a widget's: a key unknown where it stands */
        CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"foo\": {\"editor\": 1.5}}}", DGX_UI_LOAD_KEY, 38, "foo"));
        CHECK(fails_with("{\"dgx-ui\": 1, \"other\": {\"editor\": {\"a\": 1.5}}, \"tree\": {\"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 14, "other"));
        CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"children\": [[{\"editor\": {\"a\": 1.5}}]]}}", DGX_UI_LOAD_SYNTAX, 51, NULL));
        CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"editors\": {\"a\": 1.5}}}", DGX_UI_LOAD_KEY, 38, "editors"));
        /* the nesting: a value inside at most 50 brackets, the top's own one of them, in an editor's object as anywhere;
           the 51st only an empty list */
        static char deep[4096];
        for (int lists = 49; lists <= 50; ++lists) { /* the top, the editor's object, 48 lists round an empty one; one more refused */
            char *d = deep + sprintf(deep, "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\"}, \"editor\": {\"a\": ");
            for (int k = 0; k < lists; ++k) *d++ = '[';
            const int innermost = (int)(d - deep) - 1;
            for (int k = 0; k < lists; ++k) *d++ = ']';
            sprintf(d, "}}");
            if (lists == 50) CHECK(fails_with(deep, DGX_UI_LOAD_DEPTH_MAX, innermost, NULL));
            else CHECK(dgx_ui_load_need(deep, strlen(deep), &names, &need, &err));
        }
        /* a widget at depth d: its editor has the levels its place leaves, the 51st bracket an empty list at most */
        for (int depth = 1; depth <= DGX_UI_LOAD_DEPTH; depth += DGX_UI_LOAD_DEPTH - 1)
            for (int more = 0; more < 2; ++more) {
                char *d = deep + sprintf(deep, "{\"dgx-ui\": 1, \"tree\": ");
                for (int k = 1; k < depth; ++k) d += sprintf(d, "{\"kind\": \"box\", \"children\": [");
                d += sprintf(d, "{\"kind\": \"box\", \"editor\": {\"a\": ");
                const int lists = 51 - 2 * depth - 1 + more; /* the top, 2 * depth - 1 of the widgets, the editor's own */
                for (int k = 0; k < lists; ++k) *d++ = '[';
                const int innermost = (int)(d - deep) - 1;
                for (int k = 0; k < lists; ++k) *d++ = ']';
                d += sprintf(d, "}}");
                for (int k = 1; k < depth; ++k) d += sprintf(d, "]}");
                sprintf(d, "}");
                if (more) CHECK(fails_with(deep, DGX_UI_LOAD_DEPTH_MAX, innermost, NULL));
                else CHECK(dgx_ui_load_need(deep, strlen(deep), &names, &need, &err));
            }
    }
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"boxx\"}}", DGX_UI_LOAD_KIND, 31, "boxx"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"pad\": 1}}", DGX_UI_LOAD_KIND, 22, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"kind\": \"box\"}}", DGX_UI_LOAD_KEY, 38, "kind"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"label\", \"value\": \"volume\"}}", DGX_UI_LOAD_FIELD, 40, "value"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"colour\": 1}}", DGX_UI_LOAD_KEY, 38, "colour"));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"pad\": 1.5}}", DGX_UI_LOAD_RANGE, 45, NULL));
    CHECK(fails_with("{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"pad\": 32768}}", DGX_UI_LOAD_RANGE, 45, NULL));
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

    /* trees by chance, printed with their keys shuffled: loaded the same, field by field. Each printed again with
       editors' objects at chance places, at the top and in widgets, of chance content: the same need, the same tree */
    {
        static char text[8192], edited[32768];
        memory  = malloc(32768);
        int off = 0, made = 0, editors_off = 0, edited_made = 0;
        for (int round = 0; round < 400; ++round) {
            const uint32_t round_seed = seed;
            size_t         edited_n   = 0;
            printer_t      p;
            for (int pass = 0; pass < 2; ++pass) { /* with editors, then the same seed without */
                seed = round_seed, editors = pass == 0;
                p    = pass == 0 ? (printer_t){.out = edited, .cap = sizeof edited} : (printer_t){.out = text, .cap = sizeof text};
                const bool version_first = chance(2);
                const int  name_place    = chance(4); /* the name before the two keys, between them, after them; 3: none */
                const int  editor_place  = editors ? editor_chance(4) : 3;
                chance_count             = 0;
                print(&p, "{");
                for (int place = 0; place < 3; ++place) {
                    if (place == editor_place) print(&p, "%s\"editor\": ", place ? ", " : ""), chance_editor(&p, 4);
                    if (place == name_place) print(&p, "%s\"name\": \"round%d\"", place || editor_place == 0 ? ", " : "", round);
                    if (place == 2) break;
                    if (place || name_place == 0 || editor_place == 0) print(&p, ", ");
                    if ((place == 0) == version_first) print(&p, "\"dgx-ui\": 1");
                    else print(&p, "\"tree\": "), chance_widget(&p, -1, 1);
                }
                print(&p, "}");
                if (pass == 0) edited_n = p.n < sizeof edited ? p.n : 0;
                if (pass == 1 && p.n < sizeof text) {
                    char said[24] = "";
                    if (name_place < 3) snprintf(said, sizeof said, "round%d", round);
                    off += !dgx_ui_load_need(text, p.n, &names, &need, &err) || strcmp(need.name, said) != 0;
                }
            }
            if (p.n >= sizeof text) continue;
            /* with the editors: the need of the tree without them, and the same tree */
            if (edited_n) {
                dgx_ui_load_need_t edited_need;
                editors_off += !dgx_ui_load_need(edited, edited_n, &names, &edited_need, &err) || edited_need.bytes != need.bytes || edited_need.nodes != need.nodes ||
                               edited_need.depth != need.depth || strcmp(edited_need.name, need.name) != 0;
                if (err.code) fprintf(stderr, "  round %d with editors: code %d at %d \"%s\" in %.*s\n", round, err.code, err.offset, err.name, (int)edited_n, edited);
                static dgx_ui_node_t edited_nodes[64];
                dgx_ui_init(&ui, edited_nodes, 64, 1);
                if (dgx_ui_load(&ui, -1, edited, edited_n, &names, memory, 32768, &err) == 0) {
                    const dgx_ui_widget_t *got[64];
                    int                    parent[64];
                    const int              count = order_of(&ui, 0, got, parent, 0, -1);
                    editors_off += count != chance_count;
                    for (int k = 0; k < count && k < chance_count; ++k) editors_off += !same_widget(got[k], &chance_w[k]) || parent[k] != chance_parent[k];
                    ++edited_made;
                } else {
                    ++editors_off;
                }
                dgx_ui_free(&ui);
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
        CHECK(editors_off == 0 && edited_made > 300);
        CHECK(off == 0 && made > 300);
        free(memory);
    }

    /* sizes, places, pads and gaps in percent: a string of at most two decimals and "%", its flag set by the loader,
       whichever of "flags" and the number comes first */
    {
        static const struct {
            const char *json;
            uint16_t    flags;
            int16_t     pad, gap, at_x, at_y, want_w, want_h;
        } taken[] = {
            {"\"want\": [\"50%\", 24]", DGX_UI_PERCENT_W, 0, 0, 0, 0, 12800, 24},
            {"\"at\": [\"10%\", \"-5%\"]", DGX_UI_PERCENT_X | DGX_UI_PERCENT_Y, 0, 0, 2560, -1280, 0, 0},
            {"\"pad\": \"5%\", \"gap\": \"2.5%\"", DGX_UI_PERCENT_PAD | DGX_UI_PERCENT_GAP, 1280, 640, 0, 0, 0, 0},
            {"\"want\": [\"33.33%\", \"127.99%\"]", DGX_UI_PERCENT_W | DGX_UI_PERCENT_H, 0, 0, 0, 0, 8532, 32765},
            {"\"at\": [\"-128%\", \"0.5%\"]", DGX_UI_PERCENT_X | DGX_UI_PERCENT_Y, 0, 0, -32768, 128, 0, 0},
            {"\"want\": [\"0%\", \"100%\"]", DGX_UI_PERCENT_W | DGX_UI_PERCENT_H, 0, 0, 0, 0, 0, 25600},
            {"\"want\": [\"50%\", 0], \"flags\": [\"column\"]", DGX_UI_PERCENT_W | DGX_UI_COLUMN, 0, 0, 0, 0, 12800, 0},
            {"\"flags\": [\"row\", \"rest\"], \"want\": [1, \"2%\"]", DGX_UI_PERCENT_H | DGX_UI_ROW | DGX_UI_REST, 0, 0, 0, 0, 1, 512},
            {"\"pad\": 32767, \"gap\": 0", 0, 32767, 0, 0, 0, 0, 0},
            {"\"gap\": \"\\u0035%\"", DGX_UI_PERCENT_GAP, 0, 1280, 0, 0, 0, 0}, /* "5%" by an escape: a string as any */
        };
        char json[256];
        for (size_t k = 0; k < sizeof taken / sizeof *taken; ++k) {
            snprintf(json, sizeof json, "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", %s}}", taken[k].json);
            const dgx_ui_widget_t *w = loaded_root(json);
            const bool right = w && w->flags == taken[k].flags && w->pad == taken[k].pad && w->gap == taken[k].gap && w->at_x == taken[k].at_x &&
                               w->at_y == taken[k].at_y && w->want_w == taken[k].want_w && w->want_h == taken[k].want_h;
            if (!right) fprintf(stderr, "  %s: %s\n", json, w ? "other fields" : "refused");
            CHECK(right);
        }
        static const struct {
            const char *json, *value; /* the value refused, its byte the error's */
            int         code;
        } refused[] = {
            {"\"want\": [\"128%\", 0]", "\"128%\"", DGX_UI_LOAD_RANGE},     {"\"want\": [0, \"1.234%\"]", "\"1.234%\"", DGX_UI_LOAD_RANGE},
            {"\"at\": [\"-129%\", 0]", "\"-129%\"", DGX_UI_LOAD_RANGE},     {"\"pad\": \"-1%\"", "\"-1%\"", DGX_UI_LOAD_RANGE},
            {"\"pad\": -1", "-1", DGX_UI_LOAD_RANGE},                         {"\"gap\": \"128%\"", "\"128%\"", DGX_UI_LOAD_RANGE},
            {"\"want\": [\"50\", 0]", "\"50\"", DGX_UI_LOAD_SYNTAX},        {"\"want\": [\"50 %\", 0]", "\"50 %\"", DGX_UI_LOAD_SYNTAX},
            {"\"at\": [\"+5%\", 0]", "\"+5%\"", DGX_UI_LOAD_SYNTAX},        {"\"at\": [\"1e2%\", 0]", "\"1e2%\"", DGX_UI_LOAD_SYNTAX},
            {"\"at\": [\"abc\", 0]", "\"abc\"", DGX_UI_LOAD_SYNTAX},        {"\"at\": [\"05%\", 0]", "\"05%\"", DGX_UI_LOAD_SYNTAX},
            {"\"at\": [\".5%\", 0]", "\".5%\"", DGX_UI_LOAD_SYNTAX},        {"\"at\": [\"5.%\", 0]", "\"5.%\"", DGX_UI_LOAD_SYNTAX},
            {"\"at\": [\"%\", 0]", "\"%\"", DGX_UI_LOAD_SYNTAX},            {"\"at\": [\"\", 0]", "\"\"", DGX_UI_LOAD_SYNTAX},
            {"\"at\": [\"5%%\", 0]", "\"5%%\"", DGX_UI_LOAD_SYNTAX},        {"\"at\": [\"100.0000000%\", 0]", "\"100.0000000%\"", DGX_UI_LOAD_SYNTAX},
            {"\"at\": [\"-%\", 0]", "\"-%\"", DGX_UI_LOAD_SYNTAX},          {"\"pad\": \"5%\", \"pad\": 1", "\"pad\": 1", DGX_UI_LOAD_KEY},
        };
        for (size_t k = 0; k < sizeof refused / sizeof *refused; ++k) {
            snprintf(json, sizeof json, "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", %s}}", refused[k].json);
            const char *value = strstr(json + 30, refused[k].value);
            CHECK(fails_with(json, refused[k].code, (int)(value - json), NULL));
        }
    }

    dgx_ui_free(&page);
    CHECK_DONE();
}
