/*
 * hook: the tree of pages/a.json loaded at run time on the tables dgx_ui_names() made of vocab.txt, and the same tree
 * added by the function dgx_ui_tree() made of it: the same widgets, the same id. Then the trees made of b.json and of
 * the second component's description, made on its appearance; then framed.json, made on that appearance in this
 * component, against the loader on the tables made with it. "hook: ok" and 0 when all holds.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a.h"
#include "framed.h"
#include "look.h"
#include "themed.h"
#include "dgx_ui_load.h"
#include "other.h"
#include "headers/program.h"
#include "second.h"
#include "third.h"
#include "vocab.h"

int32_t             volume = 40, muted;
dgx_ui_text_style_t text_style;
int                 title_index = -1;

void on_saved(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx) {}

static void fail(const char *what)
{
    printf("hook: %s\n", what);
    exit(1);
}

static char *read_page(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if (!file) fail("no page to read");
    static char text[4096];
    *length = fread(text, 1, sizeof text, file);
    fclose(file);
    return text;
}

/* two trees the same, node by node: the widget's every field, the text by its string, the parent */
static bool same(const dgx_ui_t *a, const dgx_ui_t *b)
{
    if (a->count != b->count) return false;
    for (int i = 0; i < a->count; ++i) {
        const dgx_ui_widget_t *x = dgx_ui_node(a, i)->widget, *y = dgx_ui_node(b, i)->widget;
        if (dgx_ui_node(a, i)->parent != dgx_ui_node(b, i)->parent || x->kind != y->kind || x->flags != y->flags || x->pad != y->pad ||
            x->gap != y->gap || x->want_w != y->want_w || x->want_h != y->want_h || x->min != y->min || x->max != y->max || x->step != y->step ||
            x->value != y->value || x->look != y->look || x->on != y->on || x->ctx != y->ctx || !x->text != !y->text)
            return false;
        if (x->text && (strcmp(x->text->buf, y->text->buf) != 0 || x->text->size != y->text->size || x->text->style != y->text->style)) return false;
    }
    return true;
}

void app_main(void)
{
    static dgx_ui_node_t loaded_nodes[16], made_nodes[16];
    dgx_ui_t             loaded, made;
    size_t               length;
    const char          *json = read_page(PAGES_DIR "/a.json", &length);
    dgx_ui_load_need_t   need;
    dgx_ui_load_error_t  err;
    if (!dgx_ui_load_need(json, length, &vocab_names, &need, &err)) fail("a.json refused by the loader");
    void *memory = malloc(need.bytes);
    dgx_ui_init(&loaded, loaded_nodes, 16, 1);
    if (dgx_ui_load(&loaded, -1, json, length, &vocab_names, memory, need.bytes, &err) != 0) fail("a.json not loaded");
    const int loaded_title = title_index;
    title_index            = -1;
    dgx_ui_init(&made, made_nodes, 16, 1);
    if (sound_add(&made, -1) != 0) fail("the made tree not added");
    if (!same(&loaded, &made) || title_index != loaded_title || title_index < 0) fail("the made tree is not the loaded one");
    printf("hook: the tree \"%s\", %d widgets, made and loaded the same\n", need.name, made.count);
    dgx_ui_free(&made);
    dgx_ui_init(&made, made_nodes, 16, 1);
    if (other_add(&made, -1) != 0 || dgx_ui_node(&made, 0)->widget->on != on_saved) fail("b.json's tree not as made");
    if (second_count() != 2) fail("the second component's tree not as made");
    if (third_count() != 3) fail("the third component's tree not as made");
    /* on the second component's appearance: the made tree against the loader on the tables made with it */
    dgx_ui_free(&made);
    dgx_ui_init(&made, made_nodes, 16, 1);
    dgx_ui_init(&loaded, loaded_nodes, 16, 1);
    json = read_page(PAGES_DIR "/../framed.json", &length);
    if (!dgx_ui_load_need(json, length, &themed_names, &need, &err)) fail("framed.json refused by the loader");
    void *framed_memory = malloc(need.bytes);
    if (dgx_ui_load(&loaded, -1, json, length, &themed_names, framed_memory, need.bytes, &err) != 0) fail("framed.json not loaded");
    if (framed_add(&made, -1) != 0 || !same(&loaded, &made)) fail("the tree made on the appearance is not the loaded one");
    if (dgx_ui_node(&made, 0)->widget->look != &theme_look[0] || dgx_ui_node(&made, 1)->widget->look != &theme_look[1] ||
        dgx_ui_node(&made, 2)->widget->text->style != &theme_style[0])
        fail("the tree made on the appearance has not its looks and style");
    printf("hook: ok\n");
    exit(0);
}
