/*
 * The core of dgx-ui: the tree of nodes in the program's array and in blocks, its links, the layout and when it is
 * made again, the state set by the program against what the user does, the pointer, the keys, the focus and the
 * repeat of a held button; the text box's string.
 *
 * Every widget is a static const: on the computer it lies in read-only memory, so a write of the library through
 * its pointer crashes the test; run with ASAN_OPTIONS=detect_stack_use_after_return=1 a widget that died with a
 * function would be caught too.
 */
#include <stdint.h>
#include <string.h>

#include "check.h"
#include "dgx_ui.h"
#include "dgx_ui_text.h"

/* what on() was told, in order */
static int told[64][2], told_count, told_value, told_node_ok;

static void on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (told_count < 64) told[told_count][0] = i, told[told_count][1] = what, ++told_count;
    told_value = node->state_value.value, told_node_ok = node == dgx_ui_node(ui, i); /* the value as the finger set it */
}

static int told_of(int i, int what)
{
    int n = 0;
    for (int k = 0; k < told_count; ++k) n += told[k][0] == i && told[k][1] == what;
    return n;
}

/* the children of a widget in their order, as numbers into out; how many */
static int children(dgx_ui_t *ui, int i, int *out)
{
    int n = 0;
    for (int c = dgx_ui_node(ui, i)->child; c >= 0; c = dgx_ui_node(ui, c)->next) out[n++] = c;
    return n;
}

static const dgx_ui_widget_t column    = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
static const dgx_ui_widget_t tagged[4] = {
    {.kind = DGX_UI_BUTTON, .tag = 10}, {.kind = DGX_UI_BUTTON, .tag = 20}, {.kind = DGX_UI_BUTTON, .tag = 30}, {.kind = DGX_UI_BUTTON, .tag = 40}};
static const dgx_ui_widget_t button50 = {.kind = DGX_UI_BUTTON, .tag = 50};
static dgx_ui_widget_t       many[150]; /* tags 100..249, filled once: the program's own RAM */

static void test_tree(void)
{
    dgx_ui_node_t first[4];
    dgx_ui_t      ui;
    dgx_ui_init(&ui, first, 4, 1);
    const int root = dgx_ui_add(&ui, -1, &column);
    CHECK(root == 0);
    const int      a  = dgx_ui_add(&ui, root, &tagged[0]);
    dgx_ui_node_t *pa = dgx_ui_node(&ui, a);
    const int      b  = dgx_ui_add(&ui, root, &tagged[1]);
    const int      c  = dgx_ui_add(&ui, root, &tagged[2]);
    const int      d  = dgx_ui_insert(&ui, root, b, &tagged[3]);
    CHECK(dgx_ui_widget(&ui, c) == &tagged[2]); /* the pointer kept, not a copy */
    int kids[8];
    CHECK(children(&ui, root, kids) == 4 && kids[0] == a && kids[1] == d && kids[2] == b && kids[3] == c);
    /* past the array: blocks, and a node kept stays the widget's */
    int last = -1;
    for (int k = 0; k < 150; ++k) {
        many[k] = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .tag = (int16_t)(100 + k)};
        last    = dgx_ui_add(&ui, c, &many[k]);
    }
    CHECK(d == 4 && last == 5 + 149 && ui.block_count == 3); /* d took the first slot of the first block */
    CHECK(dgx_ui_node(&ui, a) == pa && dgx_ui_widget(&ui, a)->tag == 10);
    CHECK(dgx_ui_find(&ui, -1, 120) == 5 + 20);
    CHECK(dgx_ui_find(&ui, b, 120) == -1); /* not under b */
    CHECK(dgx_ui_find(&ui, c, 249) == last);
    /* removed with what is under it; the slots taken again */
    dgx_ui_focus(&ui, b);
    CHECK(ui.focus == b);
    dgx_ui_remove(&ui, c);
    CHECK(dgx_ui_find(&ui, -1, 30) == -1 && dgx_ui_find(&ui, -1, 120) == -1);
    CHECK(dgx_ui_widget(&ui, c) == NULL && dgx_ui_widget(&ui, 5) == NULL);
    CHECK(ui.count == 5); /* the free slots past d given up */
    dgx_ui_remove(&ui, b);
    CHECK(ui.focus == -1);
    CHECK(children(&ui, root, kids) == 2 && kids[0] == a && kids[1] == d);
    const int e = dgx_ui_add(&ui, root, &button50);
    CHECK(e == b); /* b's slot */
    CHECK(dgx_ui_add(&ui, 77, &button50) == -1);
    CHECK(dgx_ui_add(&ui, root, NULL) == -1);
    dgx_ui_free(&ui);
    CHECK(ui.count == 0 && ui.blocks == NULL);
    /* all in blocks */
    dgx_ui_init(&ui, NULL, 0, 1);
    CHECK(dgx_ui_add(&ui, -1, &column) == 0 && ui.block_count == 1);
    dgx_ui_free(&ui);
}

static const dgx_ui_widget_t lay_root        = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1, .gap = 1};
static const dgx_ui_widget_t lay_a           = {.kind = DGX_UI_BUTTON, .want_h = 5};
static const dgx_ui_widget_t lay_b           = {.kind = DGX_UI_BUTTON, .want_h = 5, .want_w = 10};
static const dgx_ui_widget_t lay_c           = {.kind = DGX_UI_BUTTON, .want_h = 5, .want_w = 10, .at_x = -1};
static const dgx_ui_widget_t row_box         = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW};
static const dgx_ui_widget_t row_a           = {.kind = DGX_UI_BUTTON, .want_w = 30};
static const dgx_ui_widget_t row_b           = {.kind = DGX_UI_BUTTON, .want_w = 20, .want_h = 8, .at_y = -2};
static const dgx_ui_widget_t hidden_at_start = {.kind = DGX_UI_BUTTON, .want_h = 5, .state = {.bits = DGX_UI_HIDDEN}};

static void test_layout(void)
{
    dgx_ui_node_t first[8];
    dgx_ui_t      ui;
    dgx_ui_init(&ui, first, 8, 2);
    const int root = dgx_ui_add(&ui, -1, &lay_root);
    const int a    = dgx_ui_add(&ui, root, &lay_a);
    const int h    = dgx_ui_add(&ui, root, &hidden_at_start); /* hidden from the start: takes no place */
    const int b    = dgx_ui_add(&ui, root, &lay_b);
    const int c    = dgx_ui_add(&ui, root, &lay_c);
    /* no screen yet and no root given: nothing has a place */
    CHECK(!dgx_ui_layout(&ui) || dgx_ui_node(&ui, root)->w == 0);
    dgx_ui_screen(&ui, 100, 80, 0);
    CHECK(dgx_ui_layout(&ui));
    const dgx_ui_node_t *wa = dgx_ui_node(&ui, a), *wb = dgx_ui_node(&ui, b), *wc = dgx_ui_node(&ui, c);
    CHECK(dgx_ui_hidden(&ui, h));
    CHECK(wa->x == 2 && wa->y == 2 && wa->w == 96 && wa->h == 10); /* across all of the box */
    CHECK(wb->x == 2 && wb->y == 14 && wb->w == 20 && wb->h == 10);
    CHECK(wc->x == 76 && wc->y == 26); /* from the far edge: 100 - 2 - 20 - 2 */
    CHECK(!dgx_ui_layout(&ui));       /* nothing changed: not laid again */
    /* drawn: the marks are the painter's to clear */
    for (int i = 0; i < ui.count; ++i) dgx_ui_node(&ui, i)->marks = 0;
    /* hidden: takes no place, the one after moves up, the box fills around its children, not drawn whole */
    dgx_ui_show(&ui, b, false);
    CHECK(dgx_ui_layout(&ui) && wc->y == 14);
    CHECK((dgx_ui_node(&ui, root)->marks & DGX_UI_MARK_AROUND) && !(dgx_ui_node(&ui, root)->marks & DGX_UI_MARK_WHOLE));
    CHECK(!(wa->marks & DGX_UI_MARK_WHOLE)); /* a did not move */
    CHECK(wc->marks & DGX_UI_MARK_WHOLE);
    dgx_ui_show(&ui, b, true);
    dgx_ui_layout(&ui);
    /* the screen turned: laid again for its size, all drawn whole */
    for (int i = 0; i < ui.count; ++i) dgx_ui_node(&ui, i)->marks = 0;
    dgx_ui_screen(&ui, 100, 80, 3); /* the same size, another orientation (180 degrees) */
    CHECK(dgx_ui_layout(&ui));
    for (int i = 0; i < ui.count; ++i) CHECK(dgx_ui_node(&ui, i)->marks & DGX_UI_MARK_WHOLE);
    dgx_ui_screen(&ui, 80, 100, 1);
    CHECK(dgx_ui_layout(&ui) && wa->w == 76);
    /* a root the program gives wins over the screen */
    dgx_ui_set_root(&ui, 10, 10, 50, 50);
    CHECK(dgx_ui_layout(&ui) && wa->x == 12 && wa->w == 46);
    /* a row: a widget without a height of its own as tall as the box */
    dgx_ui_node_t row[4];
    dgx_ui_t      r;
    dgx_ui_init(&r, row, 4, 1);
    dgx_ui_add(&r, -1, &row_box);
    const int ra = dgx_ui_add(&r, 0, &row_a);
    const int rb = dgx_ui_add(&r, 0, &row_b);
    dgx_ui_set_root(&r, 0, 0, 100, 40);
    dgx_ui_layout(&r);
    CHECK(dgx_ui_node(&r, ra)->x == 0 && dgx_ui_node(&r, ra)->h == 40);
    CHECK(dgx_ui_node(&r, rb)->x == 30 && dgx_ui_node(&r, rb)->y == 30);
    dgx_ui_free(&r);
    dgx_ui_free(&ui);
}

/* a font is read for its advance only: no glyphs */
static dgx_font_t                  font12 = {.yAdvance = 12, .f_type = DGX_FONT_BITMAP_LINES};
static const dgx_ui_font_variant_t v12[]  = {{.font = &font12}};
static const dgx_ui_text_style_t   st12   = {.variants = v12, .count = 1};
static char                        text_buf[8];
static dgx_ui_textbox_t            text_box;
static const dgx_ui_widget_t       text_w = {.kind = DGX_UI_LABEL, .text = &text_box};

static void test_text_height(void)
{
    dgx_ui_textbox_init(&text_box, text_buf, sizeof text_buf, 2, &st12);
    dgx_ui_node_t first[2];
    dgx_ui_t      ui;
    dgx_ui_init(&ui, first, 2, 1);
    dgx_ui_add(&ui, -1, &column);
    const int t = dgx_ui_add(&ui, 0, &text_w);
    dgx_ui_set_root(&ui, 0, 0, 64, 64);
    dgx_ui_layout(&ui);
    CHECK(dgx_ui_node(&ui, t)->h == 24 && dgx_ui_node(&ui, t)->w == 64);
    /* set: kept, compared, marked */
    dgx_ui_node(&ui, t)->marks = 0;
    CHECK(dgx_ui_text_printf(&ui, t, "%d ppm", 412));
    CHECK(strcmp(text_buf, "412 ppm") == 0 && (dgx_ui_node(&ui, t)->marks & DGX_UI_MARK_FIT));
    dgx_ui_node(&ui, t)->marks = 0;
    CHECK(!dgx_ui_text_printf(&ui, t, "%d ppm", 412) && dgx_ui_node(&ui, t)->marks == 0);
    /* cut to the buffer on a whole letter: 7 bytes and a 0 */
    CHECK(dgx_ui_text_set(&ui, t, "abcde\xe2\x82\xac"));
    CHECK(strcmp(text_buf, "abcde") == 0);
    CHECK(dgx_ui_text_set(&ui, t, "abcd\xd0\xb1z"));
    CHECK(strcmp(text_buf, "abcd\xd0\xb1z") == 0); /* 7 bytes: all of it */
    CHECK(dgx_ui_text_set(&ui, t, "abcde\xd0\xb1"));
    CHECK(strcmp(text_buf, "abcde\xd0\xb1") == 0);
    CHECK(dgx_ui_text_set(&ui, t, "abcdef\xd0\xb1"));
    CHECK(strcmp(text_buf, "abcdef") == 0); /* the two bytes of б after six do not go */
    dgx_ui_free(&ui);
}

/* a panel of a column: a button, a slider, a switch, a list of 4 rows, a disabled button, a hidden one */
static const dgx_ui_widget_t p_button = {.kind = DGX_UI_BUTTON, .want_h = 20, .on = on};
static const dgx_ui_widget_t p_slider = {.kind = DGX_UI_SLIDER, .want_h = 20, .max = 100, .step = 1, .on = on};
static const dgx_ui_widget_t p_switch = {.kind = DGX_UI_SWITCH, .want_h = 20, .max = 1, .on = on};
static const dgx_ui_widget_t p_list   = {.kind = DGX_UI_LIST, .want_h = 40, .max = 3, .on = on};
static const dgx_ui_widget_t p_off    = {.kind = DGX_UI_BUTTON, .want_h = 20, .on = on, .state = {.bits = DGX_UI_DISABLED}};
static const dgx_ui_widget_t p_hidden = {.kind = DGX_UI_BUTTON, .want_h = 20, .on = on, .state = {.bits = DGX_UI_HIDDEN}};
static dgx_ui_node_t         pw[8];
static dgx_ui_t              pui;
static int                   bt, sl, sw, li;

static void make_panel(void)
{
    dgx_ui_init(&pui, pw, 8, 1);
    dgx_ui_add(&pui, -1, &column);
    bt = dgx_ui_add(&pui, 0, &p_button);
    sl = dgx_ui_add(&pui, 0, &p_slider);
    sw = dgx_ui_add(&pui, 0, &p_switch);
    li = dgx_ui_add(&pui, 0, &p_list);
    dgx_ui_add(&pui, 0, &p_off);
    dgx_ui_add(&pui, 0, &p_hidden);
    dgx_ui_set_root(&pui, 0, 0, 103, 200);
    told_count = 0;
}

static void test_pointer(void)
{
    make_panel();
    dgx_ui_node_t *b = dgx_ui_node(&pui, bt);
    /* a button: pressed shows; led out onto the slider it stays pressed; let go there in 90 ms, a click of the
       button: it was meant, the slider was smudged */
    CHECK(dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 5, .down = true, .pressed = true, .ms = 1000}));
    CHECK(pui.pressed == bt && (b->state_value.bits & DGX_UI_PRESSED) && (b->state_value.bits & DGX_UI_FOCUSED));
    CHECK(told_of(bt, DGX_UI_FOCUS) == 1);
    CHECK(dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 30, .down = true, .ms = 1050}));
    CHECK(b->state_value.bits & DGX_UI_PRESSED);
    CHECK(dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 30, .released = true, .ms = 1090}));
    CHECK(told_of(bt, DGX_UI_ACTIVATED) == 1 && pui.pressed == -1 && !(b->state_value.bits & DGX_UI_PRESSED));
    CHECK(told_of(sl, DGX_UI_CHANGED) == 0 && told_of(sl, DGX_UI_ACTIVATED) == 0);
    /* let go 499 ms after, a click; 500, none: a long press; the clock runs over zero between */
    const uint32_t t0 = 0xFFFFFF00u;
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 5, .down = true, .pressed = true, .ms = t0});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 11, .y = 6, .released = true, .ms = t0 + 499});
    CHECK(told_of(bt, DGX_UI_ACTIVATED) == 2 && !(b->state_value.bits & DGX_UI_PRESSED));
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 5, .down = true, .pressed = true, .ms = t0});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 11, .y = 6, .released = true, .ms = t0 + 500});
    CHECK(told_of(bt, DGX_UI_ACTIVATED) == 2 && !(b->state_value.bits & DGX_UI_PRESSED));
    /* a swipe told while it is held (the adapter of DGX-DI tells it once): pressed till let go, no click */
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 5, .down = true, .pressed = true, .ms = 3000});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 60, .y = 5, .down = true, .swipe = true, .ms = 3040});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 70, .y = 5, .down = true, .ms = 3050});
    CHECK(b->state_value.bits & DGX_UI_PRESSED);
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 70, .y = 5, .released = true, .ms = 3060});
    CHECK(told_of(bt, DGX_UI_ACTIVATED) == 2 && !(b->state_value.bits & DGX_UI_PRESSED));
    /* the swipe of a finger before is not this one's */
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 5, .down = true, .pressed = true, .ms = 4000});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 5, .released = true, .ms = 4050});
    CHECK(told_of(bt, DGX_UI_ACTIVATED) == 3);
    /* a swipe told with the press itself */
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 5, .down = true, .pressed = true, .swipe = true, .ms = 5000});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 5, .released = true, .ms = 5050});
    CHECK(told_of(bt, DGX_UI_ACTIVATED) == 3);
    /* a slider: the knob at the point, then where it is dragged; it owns the finger */
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 51, .y = 25, .down = true, .pressed = true});
    CHECK(dgx_ui_node(&pui, sl)->state_value.value == 50 && told_of(sl, DGX_UI_CHANGED) == 1);
    CHECK(told_value == 50 && told_node_ok); /* on() has the node: the value without a call */
    CHECK(dgx_ui_dragging(&pui));
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 101, .y = 25, .down = true});
    CHECK(dgx_ui_node(&pui, sl)->state_value.value == 100 && told_of(sl, DGX_UI_CHANGED) == 2);
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 101, .y = 25, .released = true});
    CHECK(!dgx_ui_dragging(&pui));
    /* a switch: a click flips it, held long it does not */
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 45, .down = true, .pressed = true});
    CHECK(!dgx_ui_dragging(&pui));
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 45, .released = true});
    CHECK(dgx_ui_node(&pui, sw)->state_value.value == 1 && told_of(sw, DGX_UI_CHANGED) == 1);
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 45, .down = true, .pressed = true, .ms = 100});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 45, .released = true, .ms = 700});
    CHECK(dgx_ui_node(&pui, sw)->state_value.value == 1 && told_of(sw, DGX_UI_CHANGED) == 1);
    /* a list: the row under the finger (rows of 10 pixels from y 60), activated on letting go soon; held long, the
       row taken, not activated */
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 85, .down = true, .pressed = true});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 85, .released = true});
    CHECK(dgx_ui_node(&pui, li)->state_value.value == 2 && told_of(li, DGX_UI_ACTIVATED) == 1);
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 75, .down = true, .pressed = true, .ms = 100});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 75, .released = true, .ms = 600});
    CHECK(dgx_ui_node(&pui, li)->state_value.value == 1 && told_of(li, DGX_UI_ACTIVATED) == 1);
    /* the finger moved along the list: the row under it picked, as the bar follows it, a change told for each row;
       past the last row, the last; let go, no click */
    const int changed_before = told_of(li, DGX_UI_CHANGED);
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 65, .down = true, .pressed = true, .ms = 1000});
    CHECK(dgx_ui_node(&pui, li)->state_value.value == 0);
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 75, .down = true, .ms = 1010});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 85, .down = true, .ms = 1020});
    CHECK(dgx_ui_node(&pui, li)->state_value.value == 2 && told_of(li, DGX_UI_CHANGED) == changed_before + 3);
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 140, .down = true, .ms = 1030});
    CHECK(dgx_ui_node(&pui, li)->state_value.value == 3);
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 140, .released = true, .ms = 1040});
    CHECK(told_of(li, DGX_UI_ACTIVATED) == 1);
    /* back to the row it went down on: still dragged, no click */
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 65, .down = true, .pressed = true, .ms = 2000});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 75, .down = true, .ms = 2010});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 65, .down = true, .ms = 2020});
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 65, .released = true, .ms = 2030});
    CHECK(dgx_ui_node(&pui, li)->state_value.value == 0 && told_of(li, DGX_UI_ACTIVATED) == 1);
    /* the disabled and the hidden from the start are not hit; the program's own changes tell nothing */
    CHECK(dgx_ui_hit(&pui, 5, 105) == -1 && dgx_ui_hit(&pui, 5, 125) == -1);
    CHECK(!dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 5, .y = 105, .down = true, .pressed = true}));
    const int before = told_count;
    dgx_ui_set_value(&pui, sl, 7);
    dgx_ui_set_state(&pui, bt, DGX_UI_PRESSED, true);
    CHECK(told_count == before && dgx_ui_node(&pui, sl)->state_value.value == 7);
    CHECK(dgx_ui_node(&pui, bt)->state_value.bits & DGX_UI_PRESSED);
    dgx_ui_set_value(&pui, sl, 1000);
    CHECK(dgx_ui_node(&pui, sl)->state_value.value == 100);
    dgx_ui_free(&pui);
}

static void test_keys(void)
{
    make_panel();
    /* the focus round the tree in its order, past the disabled and the hidden */
    CHECK(dgx_ui_key(&pui, DGX_UI_KEY_NEXT) && pui.focus == bt);
    CHECK(dgx_ui_key(&pui, DGX_UI_KEY_NEXT) && pui.focus == sl);
    CHECK(dgx_ui_key(&pui, DGX_UI_KEY_MORE) && dgx_ui_node(&pui, sl)->state_value.value == 1);
    CHECK(dgx_ui_key(&pui, DGX_UI_KEY_NEXT) && pui.focus == sw);
    CHECK(dgx_ui_key(&pui, DGX_UI_KEY_ACTIVATE) && dgx_ui_node(&pui, sw)->state_value.value == 1);
    CHECK(dgx_ui_key(&pui, DGX_UI_KEY_NEXT) && pui.focus == li);
    /* in a list the rows first */
    CHECK(dgx_ui_key(&pui, DGX_UI_KEY_NEXT) && pui.focus == li && dgx_ui_node(&pui, li)->state_value.value == 1);
    dgx_ui_set_value(&pui, li, 3);
    CHECK(dgx_ui_key(&pui, DGX_UI_KEY_NEXT) && pui.focus == bt); /* round */
    CHECK(dgx_ui_key(&pui, DGX_UI_KEY_PREV) && pui.focus == li);
    CHECK(!dgx_ui_key(&pui, DGX_UI_KEY_BACK)); /* the program's */
    /* a held button: its key at once, again from 500 ms, then every 100 ms; the time runs over zero */
    dgx_ui_focus(&pui, bt);
    const uint32_t t0 = 0xFFFFFF00u;
    CHECK(dgx_ui_buttons(&pui, DGX_UI_PAD_DOWN, t0) && pui.focus == sl);
    CHECK(dgx_ui_time(&pui, t0 + 100) && pui.focus == sl); /* the moment of the repeat is past zero, now is not */
    CHECK(dgx_ui_time(&pui, t0 + 499) && pui.focus == sl);
    CHECK(dgx_ui_time(&pui, t0 + 500) && pui.focus == sw);
    CHECK(dgx_ui_time(&pui, t0 + 599) && pui.focus == sw);
    CHECK(dgx_ui_time(&pui, t0 + 600) && pui.focus == li);
    dgx_ui_buttons(&pui, 0, t0 + 650);
    CHECK(!dgx_ui_time(&pui, t0 + 700));
    /* two trees, a key held in one: the other, never given the buttons, has no time and nothing moves in it */
    {
        static dgx_ui_node_t other_nodes[2];
        static dgx_ui_t      other;
        dgx_ui_init(&other, other_nodes, 2, 1);
        dgx_ui_add(&other, -1, &(dgx_ui_widget_t){.kind = DGX_UI_BOX});
        dgx_ui_add(&other, 0, &(dgx_ui_widget_t){.kind = DGX_UI_BUTTON, .want_h = 10});
        dgx_ui_focus(&pui, bt);
        CHECK(dgx_ui_buttons(&pui, DGX_UI_PAD_DOWN, 2000) && pui.focus == sl);
        CHECK(!dgx_ui_time(&other, 2500) && other.focus == -1);
        CHECK(dgx_ui_time(&pui, 2500) && pui.focus == sw);
        dgx_ui_buttons(&pui, 0, 2550);
        dgx_ui_free(&other);
    }
    /* held under a popup: each step goes to the popup first, as a key pressed would. Modal, the steps stay in it, round
       its two buttons; not modal, a popup that takes no key lets them through to the tree */
    {
        static const dgx_ui_widget_t column_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        static const dgx_ui_widget_t small_w  = {.kind = DGX_UI_BUTTON, .want_h = 10};
        static const dgx_ui_widget_t note_w   = {.kind = DGX_UI_LABEL, .want_h = 10};
        static dgx_ui_node_t         popup_nodes[3];
        static dgx_ui_t              popup;
        dgx_ui_init(&popup, popup_nodes, 3, 1);
        dgx_ui_add(&popup, -1, &column_w);
        const int first = dgx_ui_add(&popup, 0, &small_w), second = dgx_ui_add(&popup, 0, &small_w);
        dgx_ui_focus(&pui, bt);
        CHECK(dgx_ui_popup_open(&pui, &popup, 0, 0, 50, 30, DGX_UI_POPUP_MODAL));
        CHECK(dgx_ui_buttons(&pui, DGX_UI_PAD_DOWN, 3000) && popup.focus == first && pui.focus == bt);
        CHECK(dgx_ui_time(&pui, 3500) && popup.focus == second && pui.focus == bt);
        CHECK(dgx_ui_time(&pui, 3600) && popup.focus == first && pui.focus == bt);
        dgx_ui_buttons(&pui, 0, 3650);
        dgx_ui_popup_close(&pui);
        dgx_ui_free(&popup);
        static dgx_ui_node_t note_nodes[2];
        static dgx_ui_t      note;
        dgx_ui_init(&note, note_nodes, 2, 1);
        dgx_ui_add(&note, -1, &column_w);
        dgx_ui_add(&note, 0, &note_w);
        CHECK(dgx_ui_popup_open(&pui, &note, 0, 0, 50, 30, 0));
        CHECK(dgx_ui_buttons(&pui, DGX_UI_PAD_DOWN, 4000) && pui.focus == sl);
        CHECK(dgx_ui_time(&pui, 4500) && pui.focus == sw && note.focus == -1);
        dgx_ui_buttons(&pui, 0, 4550);
        dgx_ui_popup_close(&pui);
        dgx_ui_free(&note);
    }
    /* A activates, BACK is nobody's, a button of no key nothing */
    dgx_ui_focus(&pui, bt);
    told_count = 0;
    dgx_ui_buttons(&pui, DGX_UI_PAD_A, 10);
    CHECK(told_of(bt, DGX_UI_ACTIVATED) == 1);
    dgx_ui_buttons(&pui, 0, 20);
    CHECK(!dgx_ui_buttons(&pui, DGX_UI_PAD_B, 30));
    CHECK(!dgx_ui_buttons(&pui, DGX_UI_PAD_B | DGX_UI_PAD_START, 40));
    CHECK(!dgx_ui_time(&pui, 50));
    /* the program's keys of the buttons: left and right move the focus, as in a row; NULL the default again */
    static const uint8_t row_keys[8] = {DGX_UI_KEY_PREV, DGX_UI_KEY_NEXT, DGX_UI_KEY_PREV, DGX_UI_KEY_NEXT, DGX_UI_KEY_ACTIVATE, 0xff, DGX_UI_KEY_BACK, DGX_UI_KEY_BACK};
    dgx_ui_focus(&pui, bt);
    dgx_ui_button_keys(&pui, row_keys);
    CHECK(dgx_ui_buttons(&pui, DGX_UI_PAD_RIGHT, 60) && pui.focus == sl);
    dgx_ui_buttons(&pui, 0, 70);
    CHECK(dgx_ui_buttons(&pui, DGX_UI_PAD_LEFT, 80) && pui.focus == bt);
    dgx_ui_buttons(&pui, 0, 90);
    dgx_ui_button_keys(&pui, NULL);
    dgx_ui_focus(&pui, sl);
    const int was = dgx_ui_node(&pui, sl)->state_value.value;
    CHECK(dgx_ui_buttons(&pui, DGX_UI_PAD_RIGHT, 100) && pui.focus == sl && dgx_ui_node(&pui, sl)->state_value.value == was + 1);
    dgx_ui_buttons(&pui, 0, 110);
    dgx_ui_focus(&pui, bt);
    /* disabled while focused: the focus leaves it */
    dgx_ui_set_state(&pui, bt, DGX_UI_DISABLED, true);
    CHECK(pui.focus == -1);
    dgx_ui_free(&pui);
}

/* widgets removed by their own on(), values without a range, and the rest of the edge cases */
static void on_remove(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == (int)(intptr_t)ctx) dgx_ui_remove(ui, i);
}
static const dgx_ui_widget_t icon_w      = {.kind = DGX_UI_ICON, .min = 0, .max = 2, .want_h = 10};
static const dgx_ui_widget_t no_range    = {.kind = DGX_UI_BUTTON, .want_h = 10};
static const dgx_ui_widget_t focus_dies  = {.kind = DGX_UI_BUTTON, .want_h = 20, .on = on_remove, .ctx = (void *)(intptr_t)DGX_UI_FOCUS};
static const dgx_ui_widget_t change_dies = {.kind = DGX_UI_SLIDER, .want_h = 20, .max = 100, .on = on_remove, .ctx = (void *)(intptr_t)DGX_UI_CHANGED};
static const dgx_ui_widget_t tagged_box  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .tag = 5, .want_h = 40};

static void test_edges(void)
{
    /* an ICON's value is not bound, so it can show none; nor is a value without a range */
    dgx_ui_node_t n[8];
    dgx_ui_t      ui;
    dgx_ui_init(&ui, n, 8, 1);
    dgx_ui_add(&ui, -1, &column);
    const int icon = dgx_ui_add(&ui, 0, &icon_w), plain = dgx_ui_add(&ui, 0, &no_range);
    dgx_ui_set_value(&ui, icon, 5);
    CHECK(dgx_ui_node(&ui, icon)->state_value.value == 5);
    dgx_ui_set_value(&ui, icon, -1);
    CHECK(dgx_ui_node(&ui, icon)->state_value.value == -1);
    dgx_ui_set_value(&ui, plain, 3);
    CHECK(dgx_ui_node(&ui, plain)->state_value.value == 3);
    /* find looks under the parent, not at it; 0 is the tag of every untagged widget, the root's too */
    const int tb = dgx_ui_add(&ui, 0, &tagged_box);
    CHECK(dgx_ui_find(&ui, tb, 5) == -1 && dgx_ui_find(&ui, -1, 5) == tb && dgx_ui_find(&ui, -1, 0) == 0);
    dgx_ui_free(&ui);

    /* hidden while pressed: the press ends, and shown again it is not drawn pressed */
    make_panel();
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 10, .y = 5, .down = true, .pressed = true});
    dgx_ui_show(&pui, bt, false);
    CHECK(pui.pressed == -1 && !(dgx_ui_node(&pui, bt)->state_value.bits & DGX_UI_PRESSED));
    dgx_ui_show(&pui, bt, true);
    CHECK(!(dgx_ui_node(&pui, bt)->state_value.bits & DGX_UI_PRESSED));
    /* disabled while dragged: the press ends, the next move changes nothing and tells nothing */
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 51, .y = 25, .down = true, .pressed = true});
    const int v = dgx_ui_node(&pui, sl)->state_value.value, before = told_count;
    dgx_ui_set_state(&pui, sl, DGX_UI_DISABLED, true);
    dgx_ui_pointer(&pui, &(dgx_ui_pointer_t){.x = 90, .y = 25, .down = true});
    CHECK(pui.pressed == -1 && dgx_ui_node(&pui, sl)->state_value.value == v && told_count == before);
    CHECK(!dgx_ui_dragging(&pui));
    dgx_ui_free(&pui);

    /* on() removes the widget it is told of: nothing is written through it, the press ends */
    dgx_ui_init(&ui, n, 8, 1);
    dgx_ui_add(&ui, -1, &column);
    const int fd = dgx_ui_add(&ui, 0, &focus_dies);
    const int cd = dgx_ui_add(&ui, 0, &change_dies);
    dgx_ui_set_root(&ui, 0, 0, 100, 100);
    CHECK(dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 5, .y = 5, .down = true, .pressed = true}));
    CHECK(dgx_ui_widget(&ui, fd) == NULL && ui.pressed == -1);
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 5, .y = 5, .released = true});
    dgx_ui_relayout(&ui);
    const int y = dgx_ui_node(&ui, cd)->y + 2; /* moved up: fd is gone */
    dgx_ui_layout(&ui);
    CHECK(dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 50, .y = (int16_t)dgx_ui_node(&ui, cd)->y + 2, .down = true, .pressed = true}));
    CHECK(dgx_ui_widget(&ui, cd) == NULL && ui.pressed == -1);
    (void)y;
    dgx_ui_free(&ui);
}

/* a child that takes what is left: a row of a name and a value at the right edge on any width; a column of a title,
   the content and buttons at the bottom; two that share; nothing left, none negative; in a free box nothing */
static const dgx_ui_widget_t r_row     = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 2, .gap = 4};
static const dgx_ui_widget_t r_name    = {.kind = DGX_UI_LABEL, .text = &text_box, .want_h = 10, .flags = DGX_UI_REST};
static const dgx_ui_widget_t r_value   = {.kind = DGX_UI_LABEL, .text = &text_box, .want_h = 10, .want_w = 60};
static const dgx_ui_widget_t r_column  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 3, .gap = 2};
static const dgx_ui_widget_t r_title   = {.kind = DGX_UI_BOX, .want_h = 10};
static const dgx_ui_widget_t r_content = {.kind = DGX_UI_BOX, .want_h = 99, .flags = DGX_UI_REST};
static const dgx_ui_widget_t r_buttons = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 12};
static const dgx_ui_widget_t r_free    = {.kind = DGX_UI_BOX};
static const dgx_ui_widget_t r_freerest = {.kind = DGX_UI_BOX, .want_w = 7, .want_h = 5, .at_x = 1, .at_y = 1, .flags = DGX_UI_REST};

static void test_rest(void)
{
    dgx_ui_textbox_init(&text_box, text_buf, sizeof text_buf, 1, &st12);
    dgx_ui_node_t first[6];
    dgx_ui_t      ui;
    /* a row on 240, then on 320: the value at the right edge; the name all the rest, told of no width */
    dgx_ui_init(&ui, first, 6, 1);
    dgx_ui_add(&ui, -1, &r_row);
    const int name = dgx_ui_add(&ui, 0, &r_name), value = dgx_ui_add(&ui, 0, &r_value);
    dgx_ui_screen(&ui, 240, 20, 0);
    dgx_ui_layout(&ui);
    const dgx_ui_node_t *n = dgx_ui_node(&ui, name), *v = dgx_ui_node(&ui, value);
    CHECK(v->x + v->w == 238 && v->w == 60 && n->x == 2 && n->w == 240 - 4 - 4 - 60 && !ui.told_width);
    dgx_ui_screen(&ui, 320, 20, 0);
    dgx_ui_layout(&ui);
    CHECK(v->x + v->w == 318 && n->w == 320 - 4 - 4 - 60);
    /* so narrow nothing is left: the name 0 wide, and still no warning of a text with no width */
    dgx_ui_screen(&ui, 60, 20, 0);
    dgx_ui_layout(&ui);
    CHECK(n->w == 0 && !ui.told_width);
    dgx_ui_free(&ui);
    /* a column: the buttons at the bottom, the content all between, its own height not asked */
    dgx_ui_init(&ui, first, 6, 1);
    dgx_ui_add(&ui, -1, &r_column);
    const int title = dgx_ui_add(&ui, 0, &r_title), content = dgx_ui_add(&ui, 0, &r_content), buttons = dgx_ui_add(&ui, 0, &r_buttons);
    dgx_ui_screen(&ui, 100, 80, 0);
    dgx_ui_layout(&ui);
    const dgx_ui_node_t *t = dgx_ui_node(&ui, title), *c = dgx_ui_node(&ui, content), *b = dgx_ui_node(&ui, buttons);
    CHECK(t->y == 3 && c->y == 15 && c->h == 80 - 6 - 10 - 12 - 4 && b->y == 15 + c->h + 2 && b->y + b->h == 77);
    /* two share: 119 inside the pads, less 22 of the others and 6 of the gaps, is 91: 45, and the last 46 */
    const int second = dgx_ui_insert(&ui, 0, buttons, &r_content);
    dgx_ui_screen(&ui, 100, 101 + 12 + 10 + 2, 0);
    dgx_ui_layout(&ui);
    CHECK(c->h == 45 && dgx_ui_node(&ui, second)->h == 46 && dgx_ui_node(&ui, second)->y == c->y + 45 + 2);
    CHECK(b->y + b->h == 101 + 12 + 10 + 2 - 3);
    /* nothing left: 0, never below */
    dgx_ui_screen(&ui, 100, 20, 0);
    dgx_ui_layout(&ui);
    CHECK(c->h == 0 && dgx_ui_node(&ui, second)->h == 0);
    /* hidden, it takes nothing: the other one all */
    dgx_ui_show(&ui, second, false);
    dgx_ui_screen(&ui, 100, 80, 0);
    dgx_ui_layout(&ui);
    CHECK(c->h == 80 - 6 - 10 - 12 - 4 && b->y + b->h == 77);
    dgx_ui_free(&ui);
    /* in a box of neither: where it asks, as without the flag */
    dgx_ui_init(&ui, first, 6, 1);
    dgx_ui_add(&ui, -1, &r_free);
    const int f = dgx_ui_add(&ui, 0, &r_freerest);
    dgx_ui_screen(&ui, 50, 50, 0);
    dgx_ui_layout(&ui);
    CHECK(dgx_ui_node(&ui, f)->x == 1 && dgx_ui_node(&ui, f)->w == 7 && dgx_ui_node(&ui, f)->h == 5);
    dgx_ui_free(&ui);
}

/* a field: DGX_UI_EDIT on a click and on ACTIVATE while it has the focus; not when the finger goes down, nor on a
   long hold or a swipe; again on a second click, the focus staying */
static const dgx_ui_widget_t e_field = {.kind = DGX_UI_INPUT, .text = &text_box, .want_h = 12, .on = on};
static const dgx_ui_widget_t e_button = {.kind = DGX_UI_BUTTON, .want_h = 12, .on = on};

static void test_edit(void)
{
    dgx_ui_textbox_init(&text_box, text_buf, sizeof text_buf, 1, &st12);
    dgx_ui_node_t first[4];
    dgx_ui_t      ui;
    dgx_ui_init(&ui, first, 4, 1);
    dgx_ui_add(&ui, -1, &column);
    const int f = dgx_ui_add(&ui, 0, &e_field), b = dgx_ui_add(&ui, 0, &e_button);
    dgx_ui_set_root(&ui, 0, 0, 60, 40);
    told_count = 0;
    /* down: the focus, no edit yet; up soon: the edit */
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 5, .y = 5, .down = true, .pressed = true, .ms = 100});
    CHECK(told_of(f, DGX_UI_FOCUS) == 1 && told_of(f, DGX_UI_EDIT) == 0);
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 5, .y = 5, .released = true, .ms = 150});
    CHECK(told_of(f, DGX_UI_EDIT) == 1 && ui.focus == f);
    /* clicked again, the focus on it already: the edit again, no focus told */
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 5, .y = 5, .down = true, .pressed = true, .ms = 200});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 5, .y = 5, .released = true, .ms = 250});
    CHECK(told_of(f, DGX_UI_EDIT) == 2 && told_of(f, DGX_UI_FOCUS) == 1);
    /* held long, or swiped: no edit */
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 5, .y = 5, .down = true, .pressed = true, .ms = 300});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 5, .y = 5, .released = true, .ms = 900});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 5, .y = 5, .down = true, .pressed = true, .ms = 1000});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 40, .y = 5, .down = true, .swipe = true, .ms = 1040});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 40, .y = 5, .released = true, .ms = 1060});
    CHECK(told_of(f, DGX_UI_EDIT) == 2);
    /* the keys: NEXT past the field opens nothing; ACTIVATE on it, the edit; on the button, its activation */
    dgx_ui_focus(&ui, -1);
    told_count = 0;
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_NEXT) && ui.focus == f && told_of(f, DGX_UI_EDIT) == 0);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE) && told_of(f, DGX_UI_EDIT) == 1);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_NEXT) && ui.focus == b && told_of(f, DGX_UI_EDIT) == 1);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE) && told_of(b, DGX_UI_ACTIVATED) == 1 && told_of(b, DGX_UI_EDIT) == 0);
    dgx_ui_free(&ui);
}

int main(void)
{
    test_tree();
    test_edit();
    test_layout();
    test_text_height();
    test_rest();
    test_pointer();
    test_keys();
    test_edges();
    CHECK_DONE();
}
