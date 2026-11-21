/*
 * Before any press, the time asks nothing. A button of DGX_UI_REPEAT: told DGX_UI_ACTIVATED at the press and again every DGX_UI_REPEAT_MS from
 * DGX_UI_REPEAT_FIRST_MS on by dgx_ui_time() while it is held, wherever the finger goes; nothing at the release. The
 * exact count of a hold of 2 s, the press and sixteen more; dgx_ui_wake() the exact wait, and the lesser of it and a
 * look's; a click, one; the key ACTIVATE once, and held on it focused, repeating; the finger far off and back, the
 * count as if it had not moved; a stall, one; the clock running over its end in a hold. It ends at the release, at a
 * swipe, when the button is hidden, disabled or removed, under a modal popup, and when its own on() removes it. A
 * button in a dialog, stepped by home's time; one of the bar over a page in home's place; one drawn by the picture
 * button's look. A key held while the finger repeats takes over, and the reverse, the other not beginning again.
 * DGX_UI_PRESSED set by the program repeats nothing; a button also of DGX_UI_DRAG does not repeat. After the passes no
 * pixel twice and the panel a fresh composing, on 16 bits straight and 18 through the strips (PANEL_BITS).
 */
#define PANEL_W 160
#define PANEL_H 120

#include <stdint.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t       colors[4];
static panel_t       *panel;
static dgx_screen_t  *memory;
static uint8_t        buffer[PANEL_W * 3 * 8];
static dgx_ui_paint_t paint;

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style = {.variants = variant, .count = 1};
static dgx_ui_textbox_t      plus_box = DGX_UI_TEXT_CONST("+", 1, &style), minus_box = DGX_UI_TEXT_CONST("-", 1, &style);
static dgx_ui_textbox_t      go_box = DGX_UI_TEXT_CONST("Go", 1, &style), drag_box = DGX_UI_TEXT_CONST("=", 1, &style);

/* what each button was told; a button whose on() removes it at the n-th time */
enum { PLUS, MINUS, PLAIN, DRAGGED, PICTURED, INNER, BUTTONS };
static int  told[BUTTONS];
static int  remove_at; /* PLUS removed by its own on() when told so many times; 0 never */
static void on_button(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    const int which = (int)(intptr_t)ctx;
    if (what != DGX_UI_ACTIVATED) return;
    ++told[which];
    if (which == PLUS && remove_at && told[PLUS] == remove_at) dgx_ui_remove(ui, i);
}

static const uint8_t          dot_bits[5] = {0x70, 0xf8, 0xf8, 0xf8, 0x70};
static const dgx_image_t      dot_im      = {.width = 5, .height = 5, .format = DGX_IMAGE_1BPP, .data = dot_bits};
static const dgx_ui_picture_t dot         = {.image = &dot_im};

/* a look of the program's own that asks for time in 30 ms */
static bool asking_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *p, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    dgx_ui_lay_paper(p, 2, n->x, n->y, n->w, n->h);
    dgx_ui_wake_in(p, 30);
    return true;
}
static const dgx_ui_look_t asking_look = {.draw = asking_draw, .fill = -1};

static const dgx_ui_widget_t column_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t plus_w   = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_REPEAT, .want_w = 30, .want_h = 14, .text = &plus_box, .look = &dgx_ui_look_button, .on = on_button, .ctx = (void *)(intptr_t)PLUS};
static const dgx_ui_widget_t minus_w  = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_REPEAT, .want_w = 30, .want_h = 14, .text = &minus_box, .look = &dgx_ui_look_button, .on = on_button, .ctx = (void *)(intptr_t)MINUS};
static const dgx_ui_widget_t plain_w  = {.kind = DGX_UI_BUTTON, .want_w = 30, .want_h = 14, .text = &go_box, .look = &dgx_ui_look_button, .on = on_button, .ctx = (void *)(intptr_t)PLAIN};
static const dgx_ui_widget_t drag_w   = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_REPEAT | DGX_UI_DRAG, .want_w = 30, .want_h = 14, .text = &drag_box, .look = &dgx_ui_look_button, .on = on_button, .ctx = (void *)(intptr_t)DRAGGED};
static const dgx_ui_widget_t pic_w    = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_REPEAT, .picture = &dot, .look = &dgx_ui_look_picture_button, .on = on_button, .ctx = (void *)(intptr_t)PICTURED};
static const dgx_ui_widget_t inner_w  = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_REPEAT, .want_w = 30, .want_h = 14, .text = &plus_box, .look = &dgx_ui_look_button, .on = on_button, .ctx = (void *)(intptr_t)INNER};
static const dgx_ui_widget_t asking_w = {.kind = DGX_UI_OWN, .want_w = 10, .want_h = 6, .look = &asking_look};
static const dgx_ui_widget_t frame_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .look = &dgx_ui_look_frame};
static const dgx_ui_widget_t place_w  = {.kind = DGX_UI_BOX, .want_h = 30};

static dgx_ui_node_t nodes[12], dialog_nodes[3], page_nodes[2];
static dgx_ui_t      ui, dialog, page;
static int           plus, minus, plain, dragged, pictured, inner, place, asking;

static void paint_anew(void)
{
    dgx_ui_paint_free(&paint);
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
    paint.strips = &dgx_ui_strips;
#endif
}

static void make(void)
{
    dgx_ui_init(&ui, nodes, 12, 1);
    dgx_ui_add(&ui, -1, &column_w);
    plus = dgx_ui_add(&ui, 0, &plus_w), minus = dgx_ui_add(&ui, 0, &minus_w), plain = dgx_ui_add(&ui, 0, &plain_w);
    dragged = dgx_ui_add(&ui, 0, &drag_w), pictured = dgx_ui_add(&ui, 0, &pic_w), place = dgx_ui_add(&ui, 0, &place_w);
    dgx_ui_init(&dialog, dialog_nodes, 3, 1);
    dgx_ui_add(&dialog, -1, &frame_w);
    inner = dgx_ui_add(&dialog, 0, &inner_w);
    dgx_ui_init(&page, page_nodes, 2, 1);
    dgx_ui_add(&page, -1, &frame_w);
    memset(told, 0, sizeof told), remove_at = 0, asking = -1;
    paint_anew();
}

/* a pass: no pixel twice, the panel a fresh composing */
static int pass(uint32_t now_ms)
{
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &paint, now_ms);
    dgx_ui_paint_t fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = now_ms};
    dgx_fill_rectangle(memory, 0, 0, PANEL_W, PANEL_H, colors[3]);
    dgx_ui_paint_area_to(&ui, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint_free(&fresh);
    int off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) off += panel->mem[y][x] != dgx_get_pixel(memory, x, y);
    return panel_twice(panel) + off;
}

static void finger_at(dgx_ui_t *tree, int i, bool pressed, bool released, int dx, int dy, uint32_t ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(tree, i);
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(n->x + n->w / 2 + dx), .y = (int16_t)(n->y + n->h / 2 + dy), .down = !released, .pressed = pressed, .released = released, .ms = ms});
}
static void down_on(dgx_ui_t *tree, int i, uint32_t ms)
{
    finger_at(tree, i, true, false, 0, 0, ms);
}
static void up_on(dgx_ui_t *tree, int i, uint32_t ms)
{
    finger_at(tree, i, false, true, 0, 0, ms);
}

/* the time stepped every 10 ms from `from` up to `to`, by difference: the clock may run over its end */
static void time_through(uint32_t from, uint32_t to)
{
    for (uint32_t t = from; (int32_t)(t - to) <= 0; t += 10) dgx_ui_time(&ui, t);
}

/* the same as a loop of a pad does it: the buttons read, none held, then the time */
static void loop_through(uint32_t from, uint32_t to)
{
    for (uint32_t t = from; (int32_t)(t - to) <= 0; t += 10) dgx_ui_buttons(&ui, 0, t), dgx_ui_time(&ui, t);
}

int main(void)
{
    panel  = panel_make();
    memory = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};

    /* before any press of the program: the time asks nothing, the pad read with nothing held neither, no wait */
    make();
    CHECK(pass(0) == 0);
    CHECK(!dgx_ui_time(&ui, 10) && !dgx_ui_buttons(&ui, 0, 20) && !dgx_ui_time(&ui, 30) && dgx_ui_wake(NULL, &ui, 30) == DGX_UI_WAKE_NONE);

    /* a hold of 2 s: the press and sixteen more; the exact wait; none at the release, and no time asked after */
    down_on(&ui, plus, 1000);
    CHECK(told[PLUS] == 1 && dgx_ui_wake(NULL, &ui, 1000) == DGX_UI_REPEAT_FIRST_MS && pass(1000) == 0);
    CHECK(dgx_ui_time(&ui, 1499) && told[PLUS] == 1 && dgx_ui_wake(NULL, &ui, 1499) == 1);
    CHECK(dgx_ui_time(&ui, 1500) && told[PLUS] == 2 && dgx_ui_wake(NULL, &ui, 1500) == DGX_UI_REPEAT_MS);
    CHECK(dgx_ui_time(&ui, 1600) && told[PLUS] == 3 && dgx_ui_time(&ui, 1650) && told[PLUS] == 3 && dgx_ui_wake(NULL, &ui, 1650) == 50);
    loop_through(1660, 3000); /* the pad read at every turn, none of its buttons held: the finger's goes on */
    CHECK(told[PLUS] == 17);
    up_on(&ui, plus, 3005);
    CHECK(dgx_ui_wake(NULL, &ui, 3006) == DGX_UI_WAKE_NONE); /* let go: no time asked, before any step */
    CHECK(told[PLUS] == 17 && !dgx_ui_time(&ui, 3100) && told[PLUS] == 17 && dgx_ui_wake(NULL, &ui, 3100) == DGX_UI_WAKE_NONE);
    CHECK(pass(3110) == 0);

    /* a click, one; the key on it focused, one */
    down_on(&ui, plus, 4000), up_on(&ui, plus, 4030);
    CHECK(told[PLUS] == 18 && !dgx_ui_time(&ui, 4600) && told[PLUS] == 18);
    dgx_ui_focus(&ui, plus);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE) && told[PLUS] == 19 && !dgx_ui_time(&ui, 5000));
    CHECK(pass(5010) == 0);

    /* the finger far off and back: the count as if it had not moved; then a stall, one */
    down_on(&ui, plus, 6000);
    finger_at(&ui, plus, false, false, 120, 90, 6200);
    time_through(6010, 6700); /* repeats at 6500, 6600, 6700 */
    finger_at(&ui, plus, false, false, 0, 0, 6710);
    CHECK(told[PLUS] == 19 + 1 + 3 && pass(6720) == 0);
    CHECK(dgx_ui_time(&ui, 9700) && told[PLUS] == 24 && dgx_ui_time(&ui, 9750) && told[PLUS] == 24);
    up_on(&ui, plus, 9760);
    CHECK(!dgx_ui_time(&ui, 9900));

    /* ended by a swipe, by hiding, disabling, removing, a modal popup over it, and by its own on() removing it */
    down_on(&ui, plus, 10000);
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 1, .y = 1, .down = true, .swipe = true, .ms = 10100});
    CHECK(!dgx_ui_time(&ui, 10600) && told[PLUS] == 25);
    up_on(&ui, plus, 10700);
    const int cases[3] = {0, 1, 2};
    for (int k = 0; k < 3; ++k) {
        const int before = told[MINUS];
        down_on(&ui, minus, 11000 + 1000 * k);
        if (cases[k] == 0) dgx_ui_show(&ui, minus, false);
        if (cases[k] == 1) dgx_ui_set_state(&ui, minus, DGX_UI_DISABLED, true);
        if (cases[k] == 2) CHECK(dgx_ui_popup_open(&ui, &dialog, 90, 60, 60, 40, DGX_UI_POPUP_MODAL));
        CHECK(!dgx_ui_time(&ui, 11000 + 1000 * k + 600) && told[MINUS] == before + 1);
        up_on(&ui, minus, 11000 + 1000 * k + 700);
        dgx_ui_show(&ui, minus, true), dgx_ui_set_state(&ui, minus, DGX_UI_DISABLED, false);
        if (ui.popup) dgx_ui_popup_close(&ui);
        CHECK(pass(11000 + 1000 * k + 800) == 0);
    }
    remove_at = told[PLUS] + 3; /* removed by its own on() at its third repeat's tell */
    down_on(&ui, plus, 15000);
    time_through(15010, 15800);
    CHECK(told[PLUS] == remove_at && !dgx_ui_node(&ui, plus)->widget && !dgx_ui_time(&ui, 15900));
    up_on(&ui, minus, 15910); /* a release over nothing held */
    CHECK(pass(15920) == 0);

    /* a button in a dialog, stepped by home's time and woken by home's wake */
    make();
    CHECK(pass(0) == 0 && dgx_ui_popup_open(&ui, &dialog, 90, 60, 60, 40, DGX_UI_POPUP_MODAL) && pass(10) == 0);
    down_on(&dialog, inner, 1000);
    CHECK(told[INNER] == 1 && dgx_ui_wake(NULL, &ui, 1000) == DGX_UI_REPEAT_FIRST_MS);
    time_through(1010, 2000);
    CHECK(told[INNER] == 1 + 6 && pass(2010) == 0);
    up_on(&dialog, inner, 2020);
    CHECK(!dgx_ui_time(&ui, 2200) && told[INNER] == 7);
    dgx_ui_popup_close(&ui);

    /* a key held while the finger repeats takes over; the finger's does not begin again when the key is let go */
    down_on(&ui, plus, 3000);
    time_through(3010, 3600); /* 3500, 3600 */
    CHECK(told[PLUS] == 3);
    dgx_ui_focus(&ui, minus);
    dgx_ui_buttons(&ui, DGX_UI_PAD_A, 3610); /* ACTIVATE on minus, focused, repeating */
    CHECK(told[MINUS] == 1);
    time_through(3620, 4200); /* the key's: 4110 */
    CHECK(told[PLUS] == 3 && told[MINUS] == 2);
    dgx_ui_buttons(&ui, 0, 4210);
    CHECK(!dgx_ui_time(&ui, 4800) && told[PLUS] == 3 && told[MINUS] == 2);
    up_on(&ui, plus, 4900);
    /* the reverse: the key repeating, the finger on another takes over; the key's does not begin again */
    dgx_ui_buttons(&ui, DGX_UI_PAD_A, 5000);
    time_through(5010, 5500);
    CHECK(told[MINUS] == 4);
    down_on(&ui, plus, 5505);
    time_through(5510, 6010); /* the finger's first repeat at 6005 */
    CHECK(told[MINUS] == 4 && told[PLUS] == 4 + 1);
    up_on(&ui, plus, 6015);
    CHECK(!dgx_ui_time(&ui, 6600) && told[MINUS] == 4);
    dgx_ui_buttons(&ui, 0, 6700);
    dgx_ui_focus(&ui, -1);
    CHECK(pass(6710) == 0);

    /* the key ACTIVATE held on a button with the focus: the press and its repeats; a key held on a plain button, once */
    dgx_ui_focus(&ui, minus);
    const int at = told[MINUS];
    dgx_ui_buttons(&ui, DGX_UI_PAD_A, 7000);
    time_through(7010, 9000);
    dgx_ui_buttons(&ui, 0, 9005);
    CHECK(told[MINUS] == at + 17 && !dgx_ui_time(&ui, 9500));
    dgx_ui_focus(&ui, plain);
    dgx_ui_buttons(&ui, DGX_UI_PAD_A, 10000);
    CHECK(!dgx_ui_time(&ui, 10600) && told[PLAIN] == 1);
    dgx_ui_buttons(&ui, 0, 10700);
    dgx_ui_focus(&ui, -1);

    /* PRESSED by the program repeats nothing; one also of DGX_UI_DRAG is the drag's: a click at the release, no repeat */
    dgx_ui_set_state(&ui, plus, DGX_UI_PRESSED, true);
    CHECK(!dgx_ui_time(&ui, 11000) && told[PLUS] == 5 && pass(11010) == 0);
    dgx_ui_set_state(&ui, plus, DGX_UI_PRESSED, false);
    down_on(&ui, dragged, 12000);
    CHECK(told[DRAGGED] == 0 && !dgx_ui_time(&ui, 12600));
    up_on(&ui, dragged, 12610);
    CHECK(told[DRAGGED] == 0); /* held past a click */
    down_on(&ui, dragged, 13000), up_on(&ui, dragged, 13030);
    CHECK(told[DRAGGED] == 1 && !dgx_ui_time(&ui, 13600)); /* a click, at the release, as any button's */

    /* the picture button's look: drawn pressed while held, every pixel once */
    down_on(&ui, pictured, 14000);
    CHECK(told[PICTURED] == 1 && pass(14005) == 0);
    time_through(14010, 14600);
    CHECK(told[PICTURED] == 3);
    up_on(&ui, pictured, 14610);
    CHECK(pass(14620) == 0);

    /* the clock running over its end in a hold */
    const uint32_t near_end = UINT32_MAX - 250;
    down_on(&ui, plus, near_end);
    const int from = told[PLUS];
    time_through(near_end + 10, near_end + 10 + 1000);
    CHECK(told[PLUS] == from + 6); /* at +500, +600 ... +1000: across the end */
    up_on(&ui, plus, near_end + 1020);
    CHECK(!dgx_ui_time(&ui, near_end + 1200));

    /* the wake: the lesser of the repeat's and a look's */
    asking = dgx_ui_add(&ui, 0, &asking_w);
    CHECK(pass(20000) == 0);
    CHECK(dgx_ui_wake(&paint, &ui, 20000) == 30);
    down_on(&ui, plus, 20000);
    CHECK(dgx_ui_wake(&paint, &ui, 20000) == 30 && dgx_ui_wake(&paint, &ui, 20020) == 10);
    CHECK(pass(20480) == 0 && dgx_ui_wake(&paint, &ui, 20490) == 10); /* the repeat at 20500 nearer than the look's 20510 */
    up_on(&ui, plus, 20495);
    dgx_ui_remove(&ui, asking);
    CHECK(pass(20500) == 0);

    /* a button of the bar while a page lies in home's place: home holds the press and the time */
    CHECK(dgx_ui_page_place(&ui, place) && dgx_ui_page_open(&ui, &page) && pass(21000) == 0);
    const int bar_from = told[PLUS];
    down_on(&ui, plus, 22000);
    time_through(22010, 22700);
    CHECK(told[PLUS] == bar_from + 1 + 3 && ui.pressed == plus);
    up_on(&ui, plus, 22710);
    CHECK(!dgx_ui_time(&ui, 23000) && pass(23010) == 0);
    dgx_ui_page_back(&ui);
    dgx_ui_page_place(&ui, -1);
    CHECK(pass(23100) == 0);

    dgx_ui_free(&page), dgx_ui_free(&dialog), dgx_ui_free(&ui);
    dgx_ui_paint_free(&paint);
    CHECK_DONE();
}
