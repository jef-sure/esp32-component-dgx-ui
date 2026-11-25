/*
 * The files dgx_ui_c made of descriptions, against the loader: each description loaded by dgx_ui_load() on the
 * program's tables and added by the function dgx_ui_c made of it, the two trees the same widget by widget and field by
 * field, their ids the same, and drawn the same pixel by pixel; a made function that finds no room for a widget
 * removes what it added and writes no id; added again after the program wrote a longer text in a box, the text is the
 * description's again. The descriptions dgx_ui_c refused are given to the loader, and what it says of each is printed
 * for test_generate.py to hold against what dgx_ui_c said. The loader's tables dgx_ui_c made of the names file
 * (program_names, by -t) are held against the tables written here entry by entry, and every description is loaded on
 * them too, with the same verdict and the same tree; the cases of a names file of a few tables (partial_names) are
 * loaded on its tables alone. The cases are test_generate.py's, in cases.h of its directory.
 */
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
#include "generate_program.h"

/* the allocations refused on demand: a block of nodes that cannot be had (dgx_ui_add() takes it by calloc()) */
void       *__real_calloc(size_t count, size_t size);
static bool refuse_calloc;
void       *__wrap_calloc(size_t count, size_t size)
{
    return refuse_calloc ? NULL : __real_calloc(count, size);
}

/* the program's objects, as generate_program.h declares them */
int32_t                       volume, choice = 2, ticked, picked, other;
static const uint8_t          dot_bits[5] = {0x70, 0xf8, 0xf8, 0xf8, 0x70};
static const dgx_image_t      dot_image   = {.width = 5, .height = 5, .format = DGX_IMAGE_1BPP, .data = dot_bits};
const dgx_ui_picture_t        dot[1]      = {{.image = &dot_image}}, dots[2] = {{.image = &dot_image}, {.image = &dot_image}};
static dgx_ui_font_variant_t  variant[1];
dgx_ui_text_style_t           text, small;
static const dgx_ui_input_look_t mine_colours = {.ink = 1, .caret = 3, .paper = 2};
const dgx_ui_look_t           mine        = {.draw = dgx_ui_draw_input, .params = &mine_colours, .fill = -1};
dgx_ui_t                      settings;
int                           title_index = -1, slider_index = -1, less_index = -1, go_index = -1;

void saved(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx) {}
void step(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx) {}
bool take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    return false;
}

/* the program's tables: the same names to the same objects as the names file test_generate.py writes */
static const dgx_ui_look_name_t looks[] = {
    DGX_UI_LOOK_NAME(button), DGX_UI_LOOK_NAME(slider),   DGX_UI_LOOK_NAME(switch),         DGX_UI_LOOK_NAME(list),  DGX_UI_LOOK_NAME(dropdown),
    DGX_UI_LOOK_NAME(input),  DGX_UI_LOOK_NAME(radio),    DGX_UI_LOOK_NAME(picture),        DGX_UI_LOOK_NAME(ring),  DGX_UI_LOOK_NAME(checkbox),
    DGX_UI_LOOK_NAME(level),  DGX_UI_LOOK_NAME(text),     DGX_UI_LOOK_NAME(picture_button), DGX_UI_LOOK_NAME(box),   DGX_UI_LOOK_NAME(frame),
    DGX_UI_LOOK_NAME(scroll), {"mine", &mine}};
static const dgx_ui_value_name_t   values[]   = {{"volume", &volume}, {"choice", &choice}, {"ticked", &ticked}, {"picked", &picked}, {"loudness", &volume}};
static const dgx_ui_picture_name_t pictures[] = {{"dot", dot, 1}, {"dots", dots, 2}, {"pip", dot, 1}};
static const dgx_ui_style_name_t   styles[]   = {{"text", &text}, {"small", &small}, {"big", &small}};
static const dgx_ui_on_event_name_t      handlers[] = {DGX_UI_ACTION_NAME(back), DGX_UI_ACTION_NAME(page), {"saved", saved, false}, {"step", step, true}, {"tap", saved, false}};
static const dgx_ui_take_name_t    takes[]    = {{"take", take}};
static const dgx_ui_context_name_t contexts[] = {{"settings", &settings, true}, {"other", &other, false}};
static const dgx_ui_id_name_t           ids[]      = {{"title", &title_index}, {"slider", &slider_index}, {"less", &less_index}, {"go", &go_index}};
enum { ID_COUNT = sizeof ids / sizeof *ids };
#define COUNT(table) (uint16_t)(sizeof table / sizeof *table)
static const dgx_ui_load_names_t names = {.looks = looks, .look_count = COUNT(looks), .values = values, .value_count = COUNT(values), .pictures = pictures,
                                          .picture_count = COUNT(pictures), .styles = styles, .style_count = COUNT(styles), .handlers = handlers,
                                          .handler_count = COUNT(handlers), .takes = takes, .take_count = COUNT(takes), .contexts = contexts,
                                          .context_count = COUNT(contexts), .ids = ids, .id_count = COUNT(ids)};

/* a description dgx_ui_c made a file of: the function it made, the style its rule gave a text that names none, the
   ints its rule made for the ids, in the order of the ids' table, and the made tables it was made by, NULL: the
   program's names file, whose tables are those above */
typedef struct generate_case_t {
    const char                *name, *json;
    int                        (*add)(dgx_ui_t *ui, int parent);
    dgx_ui_text_style_t       *default_style;
    int                       *rule_ids[ID_COUNT];
    const dgx_ui_load_names_t *names;
} generate_case_t;
/* a description dgx_ui_c refused, and the made tables of the names file it was refused by; NULL as above */
typedef struct generate_refusal_t {
    const char                *name, *json;
    const dgx_ui_load_names_t *names;
} generate_refusal_t;

#include "cases.h"

/* the tables dgx_ui_c made of the names file and those written here: the same names, in the same order, leading to
   the same places */
static bool same_tables(const dgx_ui_load_names_t *a, const dgx_ui_load_names_t *b)
{
    bool same = a->look_count == b->look_count && a->value_count == b->value_count && a->picture_count == b->picture_count &&
                a->style_count == b->style_count && a->handler_count == b->handler_count && a->take_count == b->take_count &&
                a->context_count == b->context_count && a->id_count == b->id_count;
    for (int k = 0; same && k < a->look_count; ++k) same = !strcmp(a->looks[k].name, b->looks[k].name) && a->looks[k].look == b->looks[k].look;
    for (int k = 0; same && k < a->value_count; ++k) same = !strcmp(a->values[k].name, b->values[k].name) && a->values[k].value == b->values[k].value;
    for (int k = 0; same && k < a->picture_count; ++k)
        same = !strcmp(a->pictures[k].name, b->pictures[k].name) && a->pictures[k].picture == b->pictures[k].picture && a->pictures[k].count == b->pictures[k].count;
    for (int k = 0; same && k < a->style_count; ++k) same = !strcmp(a->styles[k].name, b->styles[k].name) && a->styles[k].style == b->styles[k].style;
    for (int k = 0; same && k < a->handler_count; ++k)
        same = !strcmp(a->handlers[k].name, b->handlers[k].name) && a->handlers[k].on_event == b->handlers[k].on_event && a->handlers[k].ctx_kind == b->handlers[k].ctx_kind;
    for (int k = 0; same && k < a->take_count; ++k) same = !strcmp(a->takes[k].name, b->takes[k].name) && a->takes[k].take == b->takes[k].take;
    for (int k = 0; same && k < a->context_count; ++k)
        same = !strcmp(a->contexts[k].name, b->contexts[k].name) && a->contexts[k].ctx == b->contexts[k].ctx && a->contexts[k].page == b->contexts[k].page;
    for (int k = 0; same && k < a->id_count; ++k) same = !strcmp(a->ids[k].name, b->ids[k].name) && a->ids[k].index == b->ids[k].index;
    return same;
}

/* two widgets the same: every field, the text boxes by their strings, sizes, lines and styles; the style the rule gave
   a text that names none is the program's first */
/* a place of the program's table: not one a scrolling box of none named was given of its own */
static bool named_place(const int32_t *place)
{
    for (size_t k = 0; k < sizeof values / sizeof *values; ++k)
        if (values[k].value == place) return true;
    return false;
}

static bool same_widget(const dgx_ui_widget_t *loaded, const dgx_ui_widget_t *made, const dgx_ui_text_style_t *default_style)
{
    const dgx_ui_widget_t *a = loaded, *b = made;
    /* a scrolling box's own offset: in the load's memory, in the made array; each 0, neither of the table */
    const bool own_offsets = (a->flags & DGX_UI_SCROLL) && a->value && b->value && !named_place(a->value) && !named_place(b->value) && !*a->value && !*b->value;
    if (a->kind != b->kind || a->flags != b->flags || a->pad != b->pad || a->gap != b->gap || a->at_x != b->at_x || a->at_y != b->at_y || a->want_w != b->want_w ||
        a->want_h != b->want_h || a->min != b->min || a->max != b->max || a->step != b->step || a->tag != b->tag || a->state.bits != b->state.bits || (a->value != b->value && !own_offsets) ||
        a->picture != b->picture || a->look != b->look || a->take != b->take || a->on_event != b->on_event || a->ctx != b->ctx || a->own != b->own || !a->text != !b->text)
        return false;
    if (!a->text) return true;
    const bool same_style = a->text->style == b->text->style || (default_style && b->text->style == default_style && a->text->style == styles[0].style);
    return strcmp(a->text->buf, b->text->buf) == 0 && a->text->size == b->text->size && a->text->lines == b->text->lines && same_style;
}

/* the nodes of a tree in its order, parents before children: the widget and the parent's place in the order */
static int order_of(const dgx_ui_t *ui, int i, const dgx_ui_widget_t **out, int *parent, int at, int up)
{
    out[at] = dgx_ui_node(ui, i)->widget, parent[at] = up;
    const int self = at++;
    for (int c = dgx_ui_node(ui, i)->child; c >= 0; c = dgx_ui_node(ui, c)->next) at = order_of(ui, c, out, parent, at, self);
    return at;
}

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

static int *const id_places[ID_COUNT] = {&title_index, &slider_index, &less_index, &go_index};

/* the tree of a description loaded on the program's made tables: the same need and the same widgets as on the tables
   written here */
static bool same_on_made_tables(const generate_case_t *c, const dgx_ui_load_need_t *need, const dgx_ui_t *loaded)
{
    static dgx_ui_node_t again_nodes[128];
    dgx_ui_t             again;
    dgx_ui_load_need_t   made_need;
    dgx_ui_load_error_t  err;
    if (!dgx_ui_load_need(c->json, strlen(c->json), &program_names, &made_need, &err) || made_need.bytes != need->bytes || made_need.nodes != need->nodes ||
        made_need.depth != need->depth || strcmp(made_need.name, need->name) != 0)
        return false;
    void *memory = malloc(need->bytes ? need->bytes : 1);
    dgx_ui_init(&again, again_nodes, 128, 1);
    bool                   same = dgx_ui_load(&again, -1, c->json, strlen(c->json), &program_names, memory, need->bytes, &err) == 0;
    const dgx_ui_widget_t *a[128], *b[128];
    int                    pa[128], pb[128];
    const int              count = same ? order_of(loaded, 0, a, pa, 0, -1) : 0;
    same                         = same && count == order_of(&again, 0, b, pb, 0, -1);
    for (int k = 0; same && k < count; ++k) same = same_widget(a[k], b[k], NULL) && pa[k] == pb[k];
    dgx_ui_free(&again), free(memory);
    return same;
}

/* one case: the loaded tree and the made one, the same */
static bool same_trees(const generate_case_t *c)
{
    static dgx_ui_node_t       loaded_nodes[128], made_nodes[128];
    dgx_ui_t                   loaded, made;
    dgx_ui_load_need_t         need;
    dgx_ui_load_error_t        err;
    const dgx_ui_load_names_t *table = c->names ? c->names : &names;
    if (c->default_style) *c->default_style = text;
    for (int k = 0; k < ID_COUNT; ++k) *id_places[k] = -1;
    if (!dgx_ui_load_need(c->json, strlen(c->json), table, &need, &err)) {
        fprintf(stderr, "  %s: the loader refused what dgx_ui_c took: code %d at %d \"%s\"\n", c->name, err.code, err.offset, err.name);
        return false;
    }
    void *memory = malloc(need.bytes ? need.bytes : 1);
    dgx_ui_init(&loaded, loaded_nodes, 128, 1);
    const int loaded_root = dgx_ui_load(&loaded, -1, c->json, strlen(c->json), table, memory, need.bytes, &err);
    int       loaded_ids[ID_COUNT];
    for (int k = 0; k < ID_COUNT; ++k) loaded_ids[k] = *id_places[k], *id_places[k] = -1;
    dgx_ui_init(&made, made_nodes, 128, 1);
    const int made_root = c->add(&made, -1);
    bool      same      = loaded_root == 0 && made_root == 0;
    /* the widgets, field by field, in their order */
    const dgx_ui_widget_t *a[128], *b[128];
    int                    pa[128], pb[128];
    const int              count = same ? order_of(&loaded, 0, a, pa, 0, -1) : 0;
    same                         = same && count == order_of(&made, 0, b, pb, 0, -1);
    for (int k = 0; same && k < count; ++k) same = same_widget(a[k], b[k], c->default_style) && pa[k] == pb[k];
    /* the ids: written alike */
    for (int k = 0; same && k < ID_COUNT; ++k) {
        const int written = c->rule_ids[k] ? *c->rule_ids[k] : *id_places[k];
        same              = written == loaded_ids[k];
    }
    /* drawn the same */
    if (same) {
        dgx_screen_t *drawn_loaded = draw(&loaded), *drawn_made = draw(&made);
        same = pixels_off(drawn_loaded, drawn_made) == 0;
        dgx_screen_destroy(&drawn_loaded), dgx_screen_destroy(&drawn_made);
    }
    if (!same) fprintf(stderr, "  %s: the made tree is not the loaded one\n", c->name);
    if (same && !c->names && !same_on_made_tables(c, &need, &loaded)) {
        fprintf(stderr, "  %s: loaded on the made tables, not the tree loaded on the tables written here\n", c->name);
        same = false;
    }
    dgx_ui_free(&loaded), dgx_ui_free(&made), free(memory);
    return same;
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    text       = (dgx_ui_text_style_t){.fonts = variant, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    small      = (dgx_ui_text_style_t){.fonts = variant, .count = 1, .align = DGX_FONT_ALIGN_CENTER, .flags = DGX_UI_STYLE_MIDDLE};
    dgx_ui_init(&settings, NULL, 0, 1);
    CHECK(same_tables(&program_names, &names));
    int off = 0;
    for (size_t k = 0; k < sizeof cases / sizeof *cases; ++k) off += !same_trees(&cases[k]);
    CHECK(off == 0);
    /* the cases of every_names alone, the first: no room for a widget midway, what was added removed, no id written */
    const bool every_first = strcmp(cases[0].name, "every_names") == 0;
    CHECK(every_first);
    if (every_first) {
        static dgx_ui_node_t few[4];
        dgx_ui_t             ui;
        dgx_ui_init(&ui, few, 4, 1);
        const int root = dgx_ui_add(&ui, -1, &(dgx_ui_widget_t){.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN});
        for (int k = 0; k < ID_COUNT; ++k) *id_places[k] = -1;
        refuse_calloc = true;
        CHECK(cases[0].add(&ui, root) < 0);
        refuse_calloc = false;
        CHECK(ui.count == 1 && dgx_ui_node(&ui, root)->child < 0 && title_index < 0 && slider_index < 0 && less_index < 0 && go_index < 0);
        dgx_ui_free(&ui);
    }
    /* added again after the program wrote a longer text in a box of its own: the text as the description gives it */
    if (every_first) {
        static dgx_ui_node_t again[128];
        dgx_ui_t             ui;
        dgx_ui_init(&ui, again, 128, 1);
        CHECK(cases[0].add(&ui, -1) == 0);
        dgx_ui_text_set(&ui, title_index, "a text longer than the description's");
        dgx_ui_free(&ui);
        dgx_ui_init(&ui, again, 128, 1);
        CHECK(cases[0].add(&ui, -1) == 0 && strcmp(dgx_ui_widget(&ui, title_index)->text->buf, "Volume \xc3\xa9\xf0\x9f\x98\x80") == 0);
        dgx_ui_free(&ui);
    }
    /* what the loader says of the descriptions dgx_ui_c refused, for test_generate.py */
    for (size_t k = 0; k < sizeof refusals / sizeof *refusals; ++k) {
        const generate_refusal_t *r = &refusals[k];
        dgx_ui_load_need_t        need;
        dgx_ui_load_error_t       err, made_err;
        const bool                taken = dgx_ui_load_need(r->json, strlen(r->json), r->names ? r->names : &names, &need, &err);
        if (taken) printf("REFUSAL %s taken\n", r->name);
        else printf("REFUSAL %s %d %d \"%s\"\n", r->name, err.code, err.offset, err.name);
        /* on the program's made tables, the same verdict */
        if (!r->names)
            CHECK(dgx_ui_load_need(r->json, strlen(r->json), &program_names, &need, &made_err) == taken &&
                  (taken || (made_err.code == err.code && made_err.offset == err.offset && strcmp(made_err.name, err.name) == 0)));
    }
    printf("cases %zu, refusals %zu\n", sizeof cases / sizeof *cases, sizeof refusals / sizeof *refusals);
    CHECK_DONE();
}
