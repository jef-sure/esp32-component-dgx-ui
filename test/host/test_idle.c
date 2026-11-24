/*
 * The user's idle, dgx_ui_idle(): told 1 to home's trees once, at the step that crosses after_ms, and 0 at the step
 * after the input that wakes it, by home's dgx_ui_time() alone; dgx_ui_wake() gives its moment, the lesser of it and a
 * look's, and a loop that sleeps exactly that is woken on it; dgx_ui_time() asks no time for it. First, before any pad
 * and any press: a tree with an idle named, stepped. The waking input spent: a finger's whole gesture on a button, a
 * repeating one, a slider, a field and a dropdown (nothing pressed, told or changed, the pass quiet), a key, code
 * points, the pad's button till it is let go (dgx_ui_buttons() answering taken); a new press, and a report of no
 * finger, end a gesture whose release never came; a finger and a pad's button held with no report after keep it
 * away; with wake_acts the
 * waking touch acts, and cleared after, the next wake is spent. A finger held keeps it away; an empty pointer does not.
 * An input on a dialog over home and on a second home wakes it, told at home's step. The message reaches a widget of
 * home and one of a popup, is kept by a take(), and a root's take() opens a saver page at 1 and closes it at 0. The pad
 * read every turn, the finger's repeat and a held key across the moment: the moment kept and met. after_ms changed while
 * it runs; the same idle named again while told; named while a finger is down; NULL; the clock over its end.
 */
#define PANEL_W 160
#define PANEL_H 120

#include <stdint.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

enum { IDLE_WHAT = 77 };

static uint32_t       colors[4];
static panel_t       *panel;
static uint8_t        buffer[PANEL_W * 2 * 8];
static dgx_ui_paint_t paint;

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style = {.fonts = variant, .count = 1};
static dgx_ui_textbox_t      go_box = DGX_UI_TEXT_CONST("Go", 1, &style), plus_box = DGX_UI_TEXT_CONST("+", 1, &style);
static dgx_ui_textbox_t      options_box = DGX_UI_TEXT_CONST("A\nB", 1, &style);
static char                  field_text[8] = "1";
static dgx_ui_textbox_t      field_box;

/* what was told: the idle's messages in order, the buttons' ACTIVATED, a field's EDIT */
static char         told_idle[64];
static int          activated, plus_told, edits, popup_seen;
static int32_t      level, option;
static bool         saver_mode;     /* the root's take() opens the saver page at 1, closes it at 0 */
static dgx_ui_msg_t kept;           /* the message kept by the root's take() */
static bool         keep_it;
static dgx_ui_t     ui, dialog, other, saver;

static bool root_take(dgx_ui_t *tree, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what != IDLE_WHAT) return false;
    char word[3] = {(char)('0' + msg->bytes[0]), ' ', 0};
    strncat(told_idle, word, sizeof told_idle - strlen(told_idle) - 1);
    if (keep_it) dgx_ui_msg_keep(tree, &kept), keep_it = false;
    if (saver_mode && msg->bytes[0] == 1) dgx_ui_page_open(&ui, &saver);
    if (saver_mode && msg->bytes[0] == 0) dgx_ui_page_back(&ui);
    return false; /* not claimed: the popups hear of it too */
}
static bool popup_take(dgx_ui_t *tree, int i, const dgx_ui_msg_t *msg)
{
    popup_seen += msg->what == IDLE_WHAT;
    return false;
}
static void on_button(dgx_ui_t *tree, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) ++*(int *)ctx;
    if (what == DGX_UI_EDIT) ++edits;
}

static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2, .take = root_take};
static const dgx_ui_widget_t button_w = {.kind = DGX_UI_BUTTON, .want_w = 40, .want_h = 14, .text = &go_box, .look = &dgx_ui_look_button, .on = on_button, .ctx = &activated};
static const dgx_ui_widget_t plus_w   = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_REPEAT, .want_w = 40, .want_h = 14, .text = &plus_box, .look = &dgx_ui_look_button, .on = on_button, .ctx = &plus_told};
static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .value = &level, .max = 10, .want_w = 80, .want_h = 10, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t field_w  = {.kind = DGX_UI_INPUT, .want_w = 60, .want_h = 14, .text = &field_box, .look = &dgx_ui_look_input, .on = on_button, .ctx = &activated};
static const dgx_ui_widget_t drop_w   = {.kind = DGX_UI_DROPDOWN, .value = &option, .max = 1, .want_w = 50, .want_h = 14, .text = &options_box, .look = &dgx_ui_look_dropdown};
static const dgx_ui_widget_t frame_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .look = &dgx_ui_look_frame, .take = popup_take};
static const dgx_ui_widget_t inner_w  = {.kind = DGX_UI_BUTTON, .want_w = 30, .want_h = 14, .text = &go_box, .look = &dgx_ui_look_button, .on = on_button, .ctx = &activated};
static const dgx_ui_widget_t other_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};

static dgx_ui_node_t nodes[10], dialog_nodes[2], other_nodes[2], saver_nodes[2];
static int           button, plus, slider, field, drop, inner, other_button;
static dgx_ui_idle_t idle;

static void make(void)
{
    dgx_ui_textbox_init(&field_box, field_text, sizeof field_text, 1, &style);
    strcpy(field_text, "1");
    dgx_ui_init(&ui, nodes, 10, 1);
    dgx_ui_add(&ui, -1, &root_w);
    button = dgx_ui_add(&ui, 0, &button_w), plus = dgx_ui_add(&ui, 0, &plus_w), slider = dgx_ui_add(&ui, 0, &slider_w);
    field = dgx_ui_add(&ui, 0, &field_w), drop = dgx_ui_add(&ui, 0, &drop_w);
    dgx_ui_init(&dialog, dialog_nodes, 2, 1);
    dgx_ui_add(&dialog, -1, &frame_w);
    inner = dgx_ui_add(&dialog, 0, &inner_w);
    dgx_ui_init(&other, other_nodes, 2, 1);
    dgx_ui_add(&other, -1, &other_w);
    other_button = dgx_ui_add(&other, 0, &button_w);
    dgx_ui_init(&saver, saver_nodes, 2, 1);
    dgx_ui_add(&saver, -1, &frame_w);
    told_idle[0] = 0, activated = plus_told = edits = popup_seen = 0, level = 5, option = 0, saver_mode = keep_it = false;
    dgx_ui_paint_free(&paint);
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
    paint.strips = &dgx_ui_strips;
#endif
    dgx_ui_paint(&ui, &paint, 0);
}

static bool told(const char *expected)
{
    const bool same = strcmp(told_idle, expected) == 0;
    if (!same) fprintf(stderr, "  told \"%s\", expected \"%s\"\n", told_idle, expected);
    told_idle[0] = 0;
    return same;
}

/* a pass that writes nothing */
static bool quiet(uint32_t now_ms)
{
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &paint, now_ms);
    return panel_written(panel) == 0;
}

static void finger_on(dgx_ui_t *home, dgx_ui_t *tree, int i, bool pressed, bool down, bool released, uint32_t ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(tree, i);
    dgx_ui_pointer(home, &(dgx_ui_pointer_t){.x = (int16_t)(n->x + n->w / 2), .y = (int16_t)(n->y + n->h / 2), .down = down, .pressed = pressed, .released = released, .ms = ms});
}
static void tap(dgx_ui_t *home, dgx_ui_t *tree, int i, uint32_t ms)
{
    finger_on(home, tree, i, true, true, false, ms);
    finger_on(home, tree, i, false, false, true, ms + 30);
}
static void no_finger(uint32_t ms)
{
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.ms = ms});
}

/* the time stepped every 10 ms, the loop reading the touch with no finger on it */
static void loop_through(uint32_t from, uint32_t to)
{
    for (uint32_t t = from; (int32_t)(t - to) <= 0; t += 10) no_finger(t), dgx_ui_time(&ui, t);
}

/* idle at `at`: after_ms of 1000 from a naming at at - 1000 */
static void idle_at(uint32_t at)
{
    idle = (dgx_ui_idle_t){.after_ms = 1000, .what = IDLE_WHAT}; /* anew: nothing of a case before told */
    dgx_ui_idle(&ui, &idle, at - 1000);
    loop_through(at - 990, at);
}

int main(void)
{
    panel = panel_make();
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};

    /* before any pad and any press of the program: named, stepped, the moment given; the time asks for none; told 1
       once at the step that crosses it; a loop that sleeps exactly what dgx_ui_wake() says is woken on it */
    make();
    idle = (dgx_ui_idle_t){.after_ms = 1000, .what = IDLE_WHAT};
    dgx_ui_idle(&ui, &idle, 0);
    CHECK(!dgx_ui_time(&ui, 0) && dgx_ui_wake(NULL, &ui, 0) == 1000 && dgx_ui_wake(NULL, &ui, 400) == 600 && told(""));
    uint32_t t = 0, turns = 0;
    while (!idle.idle && turns < 10) {
        dgx_ui_time(&ui, t);
        const uint32_t wait = dgx_ui_wake(NULL, &ui, t);
        if (idle.idle || wait == DGX_UI_WAKE_NONE) break;
        t += wait, ++turns;
    }
    CHECK(idle.idle && t == 1000 && turns == 1 && told("1 ") && !dgx_ui_time(&ui, 1010) && told(""));
    CHECK(dgx_ui_wake(NULL, &ui, 1010) == DGX_UI_WAKE_NONE && dgx_ui_idle_for(&idle, 1500) == 1500);

    /* a tap on a button while idle: woken, the whole gesture spent, nothing drawn; told 0 at the step; the next tap acts */
    tap(&ui, &ui, button, 2000);
    CHECK(!idle.idle && told("") && activated == 0 && quiet(2040));
    dgx_ui_time(&ui, 2050);
    CHECK(told("0 ") && dgx_ui_wake(NULL, &ui, 2050) == 1000 - 20); /* the moment from the release at 2030 */
    tap(&ui, &ui, button, 2100);
    CHECK(activated == 1 && told(""));

    /* the same on a repeating button, a slider dragged, a field and a dropdown: nothing pressed, changed or opened */
    const int on[4] = {plus, slider, field, drop};
    for (int k = 0; k < 4; ++k) {
        make();
        idle_at(10000);
        CHECK(idle.idle && told("1 "));
        finger_on(&ui, &ui, on[k], true, true, false, 10100);
        finger_on(&ui, &ui, on[k], false, true, false, 10200); /* moved, held */
        dgx_ui_time(&ui, 10700);
        finger_on(&ui, &ui, on[k], false, false, true, 10800);
        dgx_ui_time(&ui, 10810);
        CHECK(told("0 ") && plus_told == 0 && level == 5 && edits == 0 && activated == 0 && ui.popup == NULL && quiet(10820));
    }

    /* a key while idle: spent, the focus not moved; code points: spent, the field as it was */
    make();
    dgx_ui_focus(&ui, field);
    dgx_ui_paint(&ui, &paint, 5);
    idle_at(20000);
    CHECK(told("1 ") && dgx_ui_key(&ui, DGX_UI_KEY_NEXT) && ui.focus == field && dgx_ui_idle_for(&idle, 20050) == 0);
    dgx_ui_time(&ui, 20060);
    CHECK(told("0 ") && dgx_ui_idle_for(&idle, 20100) == 40);
    loop_through(20070, 21060);
    CHECK(told("1 ") && dgx_ui_typed(&ui, (const uint32_t[]){'7'}, 1) && strcmp(field_text, "1") == 0);
    dgx_ui_time(&ui, 21070);
    CHECK(told("0 ") && dgx_ui_typed(&ui, (const uint32_t[]){'7'}, 1) && strcmp(field_text, "17") == 0);

    /* the pad: A held at the wake gives nothing till let go; the next press is ACTIVATE */
    make();
    dgx_ui_focus(&ui, button);
    idle_at(30000);
    CHECK(told("1 ") && dgx_ui_buttons(&ui, DGX_UI_PAD_A, 30100) && activated == 0); /* spent: taken */
    dgx_ui_buttons(&ui, DGX_UI_PAD_A, 30200), dgx_ui_time(&ui, 30200);
    CHECK(activated == 0 && told("0 "));
    dgx_ui_buttons(&ui, 0, 30300), dgx_ui_buttons(&ui, DGX_UI_PAD_A, 30400);
    CHECK(activated == 1);
    dgx_ui_buttons(&ui, 0, 30500);

    /* a new press ends a gesture whose release never came; with wake_acts the waking tap acts, cleared, spent again */
    make();
    idle_at(40000);
    told_idle[0] = 0;
    finger_on(&ui, &ui, button, true, true, false, 40100); /* woken, spent; its release never reported */
    tap(&ui, &ui, button, 40200);
    CHECK(activated == 1);
    idle_at(42000);
    told_idle[0] = 0;
    idle.wake_acts = true;
    tap(&ui, &ui, button, 42100);
    CHECK(activated == 2);
    dgx_ui_time(&ui, 42200);
    CHECK(told("0 "));
    idle.wake_acts = false;
    loop_through(42210, 43200);
    CHECK(told("1 "));
    tap(&ui, &ui, button, 43300);
    CHECK(activated == 2);

    /* the waking press never let go, then reports of no finger: the gesture ended, not the user, the idle comes again */
    make();
    idle_at(45000);
    told_idle[0] = 0;
    finger_on(&ui, &ui, button, true, true, false, 45100); /* woken, spent; no release ever */
    idle.after_ms = 300;
    loop_through(45110, 46100);
    CHECK(told("0 1 ") && idle.idle && dgx_ui_idle_for(&idle, 46100) >= 300 && activated == 0);

    /* a source that reports changes alone: a finger down, then no report for a second; no idle under it, and its
       release acts and lets the button go */
    make();
    idle = (dgx_ui_idle_t){.after_ms = 300, .what = IDLE_WHAT};
    dgx_ui_idle(&ui, &idle, 47000);
    finger_on(&ui, &ui, button, true, true, false, 47000);
    for (t = 47010; t <= 48000; t += 10) dgx_ui_time(&ui, t);
    CHECK(!idle.idle && told("") && dgx_ui_holding(&ui));
    finger_on(&ui, &ui, button, false, false, true, 48010);
    CHECK(activated == 0 && !dgx_ui_holding(&ui)); /* held past a click: let go, nothing told, not spent */

    /* a button of the pad held and not read again: no idle under it, the key's repeats all its own */
    make();
    dgx_ui_focus(&ui, slider);
    idle = (dgx_ui_idle_t){.after_ms = 300, .what = IDLE_WHAT};
    dgx_ui_idle(&ui, &idle, 49000);
    level = 0;
    dgx_ui_buttons(&ui, DGX_UI_PAD_RIGHT, 49000); /* MORE: 1 now, then from 49500 every 100 ms */
    for (t = 49010; t <= 50000; t += 10) dgx_ui_time(&ui, t);
    CHECK(!idle.idle && told("") && level == 7);
    dgx_ui_buttons(&ui, 0, 50010);

    /* dgx_ui_buttons() answers taken in the call that spends the pad's buttons */
    make();
    idle_at(51000);
    told_idle[0] = 0;
    CHECK(dgx_ui_buttons(&ui, DGX_UI_PAD_BACK, 51100));
    CHECK(!dgx_ui_buttons(&ui, 0, 51200) && dgx_ui_idle_for(&idle, 51200) == 0 && dgx_ui_wake(NULL, &ui, 51200) == 1000); /* its let-go the user's */
    CHECK(!dgx_ui_buttons(&ui, DGX_UI_PAD_BACK, 51300)); /* again: BACK, the program's */
    dgx_ui_buttons(&ui, 0, 51400);

    /* a finger held keeps it away; an empty pointer does not */
    make();
    idle = (dgx_ui_idle_t){.after_ms = 1000, .what = IDLE_WHAT};
    dgx_ui_idle(&ui, &idle, 50000);
    finger_on(&ui, &ui, button, true, true, false, 50000);
    for (t = 50010; t <= 52000; t += 10) finger_on(&ui, &ui, button, false, true, false, t), dgx_ui_time(&ui, t);
    CHECK(!idle.idle && told(""));
    finger_on(&ui, &ui, button, false, false, true, 52010);
    loop_through(52020, 53010);
    CHECK(idle.idle && told("1 "));

    /* an input on a dialog over home, and on a second home: woken, told at home's step alone; the message heard by a
       widget of the popup too, and kept by the root's take() */
    make();
    CHECK(dgx_ui_popup_open(&ui, &dialog, 80, 60, 60, 30, DGX_UI_POPUP_MODAL));
    dgx_ui_paint(&ui, &paint, 5);
    keep_it = true;
    idle_at(60000);
    CHECK(told("1 ") && popup_seen == 0 && kept.what == IDLE_WHAT && kept.count == 1 && kept.bytes[0] == 1); /* kept: claimed */
    tap(&ui, &dialog, inner, 60100);
    CHECK(activated == 0 && told(""));
    dgx_ui_time(&ui, 60200);
    CHECK(told("0 ") && popup_seen == 1); /* not kept: the popup's widget hears it */
    loop_through(60210, 61200);
    CHECK(told("1 "));
    dgx_ui_screen(&other, PANEL_W, PANEL_H, 0);
    dgx_ui_layout(&other);
    tap(&other, &other, other_button, 61300); /* a second home: woken, its tap spent */
    dgx_ui_time(&other, 61400);
    CHECK(told("") && activated == 0);
    dgx_ui_time(&ui, 61500);
    CHECK(told("0 "));
    dgx_ui_popup_close(&ui);

    /* a root's take() opens a saver page at 1 and closes it at 0; the waking tap spent, the next presses home's button */
    make();
    saver_mode = true;
    idle_at(70000);
    CHECK(told("1 ") && ui.popup == &saver);
    tap(&ui, &saver, 0, 70100);
    dgx_ui_time(&ui, 70200);
    CHECK(told("0 ") && ui.popup == NULL && activated == 0);
    tap(&ui, &ui, button, 70300);
    CHECK(activated == 1);
    saver_mode = false;

    /* the pad read every turn, the finger's repeat and a held key across the moment: the moment kept and met */
    make();
    idle = (dgx_ui_idle_t){.after_ms = 1000, .what = IDLE_WHAT};
    dgx_ui_idle(&ui, &idle, 80000);
    for (t = 80010; t <= 80500; t += 10) dgx_ui_buttons(&ui, 0, t), dgx_ui_time(&ui, t);
    CHECK(dgx_ui_wake(NULL, &ui, 80500) == 500);
    finger_on(&ui, &ui, plus, true, true, false, 80600);
    for (t = 80610; t <= 81800; t += 10) finger_on(&ui, &ui, plus, false, true, false, t), dgx_ui_time(&ui, t);
    CHECK(!idle.idle && plus_told > 1);
    finger_on(&ui, &ui, plus, false, false, true, 81810);
    CHECK(dgx_ui_wake(NULL, &ui, 81810) == 1000); /* back at once, before any step */
    dgx_ui_focus(&ui, slider);
    dgx_ui_buttons(&ui, DGX_UI_PAD_RIGHT, 81900);
    for (t = 81910; t <= 83000; t += 10) dgx_ui_buttons(&ui, DGX_UI_PAD_RIGHT, t), dgx_ui_time(&ui, t);
    CHECK(!idle.idle);
    dgx_ui_buttons(&ui, 0, 83010);
    CHECK(dgx_ui_wake(NULL, &ui, 83010) == 1000);
    t = 83010;
    for (int k = 0; k < 4 && !idle.idle; ++k) {
        dgx_ui_buttons(&ui, 0, t), dgx_ui_time(&ui, t);
        const uint32_t wait = dgx_ui_wake(NULL, &ui, t);
        if (idle.idle || wait == DGX_UI_WAKE_NONE) break;
        t += wait;
    }
    CHECK(idle.idle && t == 84010);
    told_idle[0] = 0;

    /* after_ms changed while it runs; the same named again while told; named while a finger is down; NULL */
    make();
    idle = (dgx_ui_idle_t){.after_ms = 1000, .what = IDLE_WHAT};
    dgx_ui_idle(&ui, &idle, 90000);
    dgx_ui_time(&ui, 90100);
    idle.after_ms = 300;
    dgx_ui_time(&ui, 90200);
    CHECK(dgx_ui_wake(NULL, &ui, 90200) == 100 && !idle.idle);
    dgx_ui_time(&ui, 90300);
    CHECK(idle.idle && told("1 "));
    dgx_ui_idle(&ui, &idle, 90400);
    CHECK(!idle.idle);
    dgx_ui_time(&ui, 90410);
    CHECK(told("0 ") && dgx_ui_wake(NULL, &ui, 90410) == 290);
    finger_on(&ui, &ui, button, true, true, false, 90500);
    dgx_ui_idle(&ui, &idle, 90600);
    finger_on(&ui, &ui, button, false, false, true, 90610);
    CHECK(activated == 1); /* its release acts: the idle never came between */
    loop_through(90620, 90910); /* idle again at 90910 */
    CHECK(idle.idle && told("1 "));
    dgx_ui_idle(&ui, NULL, 90920);
    CHECK(!idle.idle); /* dropped: no longer idle to whoever reads it */
    loop_through(90930, 92000);
    CHECK(told("") && dgx_ui_wake(NULL, &ui, 92000) == DGX_UI_WAKE_NONE);
    tap(&ui, &ui, button, 92100);
    CHECK(activated == 2);

    /* the clock over its end while idle comes and is woken */
    make();
    const uint32_t near_end = UINT32_MAX - 400;
    idle = (dgx_ui_idle_t){.after_ms = 1000, .what = IDLE_WHAT};
    dgx_ui_idle(&ui, &idle, near_end);
    CHECK(dgx_ui_wake(NULL, &ui, near_end) == 1000);
    loop_through(near_end + 10, near_end + 990);
    CHECK(!idle.idle && told(""));
    loop_through(near_end + 1000, near_end + 1000);
    CHECK(idle.idle && told("1 ") && dgx_ui_idle_for(&idle, near_end + 1500) == 1500);
    tap(&ui, &ui, button, near_end + 1600);
    dgx_ui_time(&ui, near_end + 1700);
    CHECK(told("0 ") && activated == 0);
    dgx_ui_idle(&ui, NULL, 0);

    dgx_ui_free(&saver), dgx_ui_free(&other), dgx_ui_free(&dialog), dgx_ui_free(&ui);
    dgx_ui_paint_free(&paint);
    CHECK_DONE();
}
