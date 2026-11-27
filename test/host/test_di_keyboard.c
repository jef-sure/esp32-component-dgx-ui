/*
 * DGX-DI's keyboard for the fields of a tree (dgx_ui_di_keyboard_*, dgx_ui_di.h), on a virtual screen of 16 bits as
 * the display, its fingers from a device of the test through DGX-DI's pointer: a field clicked opens it on the next
 * pass, away from the field, its labels in the field's font, the paint's cover its band; its keys type into the field,
 * erase erases; done closes it with DGX_UI_EVENT_ACTIVATED told, nothing typed after it, cancel with DGX_UI_EVENT_BACK;
 * the focus moved, the field hidden, disabled or removed, a popup over the tree, the field's page closed: each closes
 * it, nothing told; another field edited moves it there; a layout that cannot open cancels the edit; the ready
 * on_event; closed by the program, the cover cleared, before its page is freed; a finger on its band is its own, the
 * tree gets none of it.
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_di.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"

enum { DISPLAY_W = 240, DISPLAY_H = 320, GLASS_W_MM = 43, GLASS_H_MM = 58 };

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static dgx_ui_di_keyboard_t  keys;
static int                   done_told, back_told;

/* the fields' own on_event: the edit given to the helper, done and cancel counted */
static void on_field(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_EDIT) dgx_ui_di_keyboard_edit(&keys, ui, i);
    if (what == DGX_UI_EVENT_ACTIVATED) ++done_told;
    if (what == DGX_UI_EVENT_BACK) ++back_told;
}

static char             first_buf[16], second_buf[16], page_buf[16];
static dgx_ui_textbox_t first_box, second_box, page_box;
static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4, .gap = 4};
static const dgx_ui_widget_t first_w  = {.kind = DGX_UI_INPUT, .text = &first_box, .want_h = 16, .look = &dgx_ui_look_input, .on_event = on_field};
static const dgx_ui_widget_t second_w = {.kind = DGX_UI_INPUT, .text = &second_box, .want_h = 16, .look = &dgx_ui_look_input, .on_event = on_field};
static const dgx_ui_widget_t page_w   = {.kind = DGX_UI_INPUT, .text = &page_box, .want_h = 16, .look = &dgx_ui_look_input,
                                         .on_event = dgx_ui_di_keyboard_on_event, .ctx = &keys};
static const dgx_ui_widget_t pop_w    = {.kind = DGX_UI_FRAME};

static dgx_ui_node_t    nodes[4], page_nodes[2], pop_nodes[1];
static dgx_ui_t         ui, page, pop;
static dgx_ui_paint_t   paint;
static dgx_di_pointer_t pointer;
static dgx_di_touch_t   touch;
static bool             tree_took; /* a finger went down on a widget of the tree */
static uint32_t         now = 1000;

/* a device of the test: one finger or none, in pixels of the display */
typedef struct {
    dgx_di_device_t device;
    int             count;
    dgx_di_point_t  point;
} test_device_t;
static int test_device_read(dgx_di_device_t *device, dgx_di_point_t *points, int max)
{
    const test_device_t *test = (const test_device_t *)device;
    if (test->count && max) points[0] = test->point;
    return test->count && max ? 1 : 0;
}
static test_device_t device = {.device = {.read = test_device_read, .fingers = 1, .x_min = 0, .x_max = DISPLAY_W - 1, .y_min = 0, .y_max = DISPLAY_H - 1}};

/* a pass of the program's loop: the pointer read, the keyboard's fingers first, the tree's after, the tree painted */
static void pass(int count, int x, int y)
{
    now += 50;
    device.count = count, device.point = (dgx_di_point_t){.x = (int16_t)x, .y = (int16_t)y, .id = 1};
    dgx_di_pointer_read(&pointer, now);
    dgx_ui_di_keyboard_pass(&keys, &pointer, now);
    dgx_ui_t *top = &ui;
    while (top->popup) top = top->popup;
    dgx_ui_di_gestures_t g;
    dgx_ui_di_pointer(top, &pointer, &touch, now, &g);
    tree_took |= g.taken;
    dgx_ui_paint(&ui, &paint, now);
}
static void tap(int x, int y)
{
    pass(1, x, y), pass(0, x, y), pass(0, x, y);
}
static void tap_widget(dgx_ui_t *tree, int i)
{
    const dgx_ui_node_t *n = dgx_ui_node(tree, i);
    tap(n->x + n->w / 2, n->y + n->h / 2);
}
/* a key of the digits' layout: 0..8 are 1..9, 9 erase, 10 is 0, 11 done */
static void tap_key(int key)
{
    const dgx_di_rect_t *r = &keys.kb.keys[key];
    tap(r->x + r->w / 2, r->y + r->h / 2);
}
static bool covered(void)
{
    return keys.open && paint.cover.x == keys.kb.x && paint.cover.y == keys.kb.y && paint.cover.w == keys.kb.w && paint.cover.h == keys.kb.h;
}
static bool uncovered(void)
{
    return !keys.open && paint.cover.w == 0 && paint.cover.h == 0;
}
/* the code point a key of the test types, put in the keyboard's queue as a key would */
static void typed_by_keyboard(uint32_t code)
{
    keys.kb.queue[(keys.kb.head + keys.kb.queued++) % DGX_DI_KEYBOARD_QUEUE] = code;
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
    dgx_ui_textbox_init(&first_box, first_buf, sizeof first_buf, 1, &style);
    dgx_ui_textbox_init(&second_box, second_buf, sizeof second_buf, 1, &style);
    dgx_ui_textbox_init(&page_box, page_buf, sizeof page_buf, 1, &style);
    dgx_screen_t  *display  = dgx_vscreen_init(DISPLAY_W, DISPLAY_H, 16, DgxScreenRGB);
    static const uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};
    paint = (dgx_ui_paint_t){.scr = display, .colors = colors, .color_count = 4};
    dgx_di_pointer_init(&pointer, &device.device, 0, display);
    dgx_di_pointer_debounce(&pointer, 0, 0); /* a read down and one up: a tap */
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_screen(&ui, DISPLAY_W, DISPLAY_H, 0);
    dgx_ui_add(&ui, -1, &root_w);
    const int first = dgx_ui_add(&ui, 0, &first_w);
    const int second = dgx_ui_add(&ui, 0, &second_w);
    dgx_ui_di_keyboard_init(&keys, &paint, GLASS_W_MM, GLASS_H_MM, dgx_di_keyboard_digits, dgx_di_keyboard_digits_size);
    pass(0, 0, 0);
    CHECK(!keys.open && keys.field == -1 && uncovered());

    /* a click on the field: edited, opened by the next pass, away from the field, in its font, its band the cover */
    tap_widget(&ui, first);
    {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, first);
        CHECK(keys.open && keys.ui == &ui && keys.field == first && covered() && keys.kb.font == &TerminusTTFMedium12);
        CHECK(keys.kb.y >= n->y + n->h || keys.kb.y + keys.kb.h <= n->y); /* the band off the field */
    }
    /* its keys type into the field, erase erases; a finger on its band is its own */
    tree_took = false;
    tap_key(0), tap_key(1), tap_key(9);
    CHECK(strcmp(first_buf, "1") == 0 && keys.open && !tree_took);
    tap_key(10);
    CHECK(strcmp(first_buf, "10") == 0);
    /* done: ACTIVATED told, closed, the cover cleared */
    done_told = 0;
    tap_key(11);
    CHECK(done_told == 1 && uncovered() && keys.field == -1);
    /* a letter after done in one take: not typed, the field holds what ACTIVATED was told of */
    tap_widget(&ui, first);
    done_told = 0;
    typed_by_keyboard('1'), typed_by_keyboard(DGX_DI_KEY_DONE), typed_by_keyboard('2');
    pass(0, 0, 0);
    CHECK(done_told == 1 && strcmp(first_buf, "101") == 0 && uncovered());
    /* cancel: BACK told, closed */
    tap_widget(&ui, first);
    back_told = 0;
    typed_by_keyboard(DGX_DI_KEY_CANCEL);
    pass(0, 0, 0);
    CHECK(keys.open == false && back_told == 1 && uncovered());

    /* no more edited, nothing told: the focus moved by a key, the field hidden, disabled; a popup over the tree */
    done_told = back_told = 0;
    tap_widget(&ui, first);
    CHECK(keys.open);
    dgx_ui_key(&ui, DGX_UI_KEY_NEXT);
    pass(0, 0, 0);
    CHECK(uncovered() && ui.focus != first);
    tap_widget(&ui, first);
    CHECK(keys.open);
    dgx_ui_show(&ui, first, false);
    pass(0, 0, 0);
    CHECK(uncovered());
    dgx_ui_show(&ui, first, true);
    pass(0, 0, 0);
    tap_widget(&ui, first);
    CHECK(keys.open);
    dgx_ui_set_state(&ui, first, DGX_UI_DISABLED, true);
    pass(0, 0, 0);
    CHECK(uncovered());
    dgx_ui_set_state(&ui, first, DGX_UI_DISABLED, false);
    tap_widget(&ui, first);
    CHECK(keys.open);
    dgx_ui_init(&pop, pop_nodes, 1, 1), dgx_ui_add(&pop, -1, &pop_w);
    dgx_ui_popup_open(&ui, &pop, 0, 200, DISPLAY_W, 40, 0);
    pass(0, 0, 0);
    CHECK(uncovered());
    dgx_ui_popup_close(&ui);
    pass(0, 0, 0);
    CHECK(done_told == 0 && back_told == 0);

    /* another field edited while it is open: moved there on the next pass, in its place */
    tap_widget(&ui, first);
    CHECK(keys.open && keys.field == first);
    tap_widget(&ui, second);
    CHECK(keys.open && keys.field == second && covered());
    tap_key(4);
    CHECK(strcmp(second_buf, "5") == 0 && strcmp(first_buf, "101") == 0);

    /* the field removed: closed */
    dgx_ui_remove(&ui, second);
    pass(0, 0, 0);
    CHECK(uncovered());

    /* a field of a page, the ready on_event with ctx the struct; the page closed: closed. Then the program's own close
       before the page is freed, the documented order */
    dgx_ui_init(&page, page_nodes, 2, 1);
    dgx_ui_add(&page, -1, &(dgx_ui_widget_t){.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4});
    const int on_page = dgx_ui_add(&page, 0, &page_w);
    CHECK(dgx_ui_page_open(&ui, &page));
    pass(0, 0, 0);
    tap_widget(&page, on_page);
    CHECK(keys.open && keys.ui == &page && keys.field == on_page && covered());
    tap_key(6);
    CHECK(strcmp(page_buf, "7") == 0);
    dgx_ui_page_back(&ui);
    pass(0, 0, 0);
    CHECK(uncovered());
    CHECK(dgx_ui_page_open(&ui, &page));
    pass(0, 0, 0);
    tap_widget(&page, on_page);
    CHECK(keys.open);
    dgx_ui_di_keyboard_close(&keys);
    CHECK(uncovered() && keys.ui == NULL);
    dgx_ui_page_back(&ui);
    dgx_ui_free(&page);
    pass(0, 0, 0);
    CHECK(uncovered());

    /* a layout that cannot open: the edit cancelled, BACK told, nothing open, no cover */
    dgx_ui_di_keyboard_init(&keys, &paint, GLASS_W_MM, GLASS_H_MM, dgx_di_keyboard_digits, 0);
    back_told = 0;
    tap_widget(&ui, first);
    CHECK(back_told == 1 && uncovered());

    dgx_ui_free(&ui);
    dgx_screen_destroy(&display);
    CHECK_DONE();
}
