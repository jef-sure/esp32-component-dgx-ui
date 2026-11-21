/*
 * controls: the controls of dgx-ui worked by a finger and by the keys. A button opens a popup that asks "Really?", a
 * switch turns, a dropdown opens its list of modes, a slider moves, two fields take digits, a code and a PIN shown as stars; a line at the bottom tells
 * what the user did last. A field clicked (DGX_UI_EDIT; Enter on it too) is edited in a strip at the top of the
 * display, a keypad of DGX-DI under it: the strip holds a copy of the field's text, given to the field on done and dropped on cancel, so a field left
 * by cancel (the keypad's, or BACK) keeps what it had. The task that owns the display sleeps on
 * its queue; a ticker wakes it every 15 ms to read the finger, as a touch panel is read.
 *
 * The board is chosen by jef-sure/boards (board_select.py): its display and its touch, calibrated by the board's
 * data, come from it, and on the computer a window of DGX showing the board's display. There the mouse is the finger, the arrows move the
 * focus and change a value, Enter activates, Escape is BACK, which no widget takes: the program closes the popup on it.
 * The keys come as the buttons of a pad, DGX_UI_PAD_*, by the default table of dgx_ui_buttons() (dgx_ui_button_keys()
 * would give another). DGX_HOST_SCRIPT plays them with nobody at the computer: script.txt beside main/ plays the run
 * of every control. On a CYD its ILI9341 lying down and its XPT2046.
 *
 * The keys of DGX-DI's keyboard are as large as a finger, in millimetres of the glass: on the 2.8" glass of a CYD its
 * ready pad of digits takes all of it. This program's keypad (keypad.json) has rows of 7.5 mm instead, so the strip
 * fits above it.
 *
 * The fonts are made at build time from a face of the build machine (main/CMakeLists.txt and fonts.rules): the
 * widgets' of 14 px with soft edges, the keypad's digits of 20 px and one bit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_di.h"
#include "dgx_di_keyboard.h"
#include "dgx_ui_di.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_post.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_text.h"
#include "dgx_ui_ticker.h"
#include "esp_timer.h"
#include "dgx_fonts.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "boards.h"
#include "boards_dgx.h"
#include "boards_dgx_di.h"

#ifdef DGX_HOST_WINDOW
#include "dgx_host_screen.h"
/* the keys of the computer as the buttons of a pad: the arrows, Enter A, Escape BACK (USB HID numbers, as SDL's) */
static uint32_t buttons_held(void)
{
    uint16_t keys[8];
    uint32_t held = 0;
    const int n   = dgx_host_keys(keys, 8);
    for (int k = 0; k < n; ++k) {
        switch (keys[k]) {
        case 79: held |= DGX_UI_PAD_RIGHT; break;
        case 80: held |= DGX_UI_PAD_LEFT; break;
        case 81: held |= DGX_UI_PAD_DOWN; break;
        case 82: held |= DGX_UI_PAD_UP; break;
        case 40: held |= DGX_UI_PAD_A; break;
        case 41: held |= DGX_UI_PAD_BACK; break;
        default: break;
        }
    }
    return held;
}
#else
/* a board's buttons: the CYD has none for a program but BOOT, so none here */
static uint32_t buttons_held(void)
{
    return 0;
}
#endif

/* the colours of the painter: 0 the paper, 1 the ink, 2 a fill, 3 the accent */
enum { MSG_TOUCH = 1 };

static const dgx_ui_font_variant_t variant[1] = {{.font = &dgx_font_text}};
static const dgx_ui_text_style_t   left       = {.variants = variant, .count = 1, .align = DGX_FONT_ALIGN_LEFT, .flags = DGX_UI_TEXT_MIDDLE};
static const dgx_ui_text_style_t   middle     = {.variants = variant, .count = 1, .align = DGX_FONT_ALIGN_CENTER, .flags = DGX_UI_TEXT_MIDDLE};

/* the texts, each in a box of its own */
enum { T_TITLE, T_POPUP, T_MODE, T_SLIDER, T_CODE_LABEL, T_CODE, T_PIN_LABEL, T_PIN, T_STATUS, T_ASK, T_YES, T_NO, T_EDIT, TEXTS };
static char             text_buf[TEXTS][40];
static dgx_ui_textbox_t text_box[TEXTS];

static void on_popup_button(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
static void on_switch(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
static void on_mode(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
static void on_slider(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
static void on_field(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
static void on_strip(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);
static void on_answer(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx);

/* the main tree: a column of a title, a row of the popup's button and the switch, the slider, two fields, the status;
   each of a height of its own, which a line of any face the rules may pick fits (16 to 19 px at 14 px), so that the
   widgets lie the same whatever face the build machine gave */
static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 6, .gap = 8};
static const dgx_ui_widget_t title_w  = {.kind = DGX_UI_LABEL, .text = &text_box[T_TITLE], .want_h = 20};
static const dgx_ui_widget_t row_w    = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 10, .want_h = 28};
static const dgx_ui_widget_t popup_w  = {.kind = DGX_UI_BUTTON, .text = &text_box[T_POPUP], .want_w = 90, .flags = DGX_UI_STRETCH, .look = &dgx_ui_look_button, .on = on_popup_button};
/* the program's values: where the switch, the dropdown and the slider keep theirs, written by the user, read here */
static int32_t switch_on, mode_chosen, slider_percent;
static const dgx_ui_widget_t switch_w = {.kind = DGX_UI_SWITCH, .max = 1, .value = &switch_on, .want_w = 44, .want_h = 20, .at_y = 4, .look = &dgx_ui_look_switch, .on = on_switch};
/* a dropdown of four modes: its options the lines of its text box, its list opened by dgx_ui_dropdown_open() */
static const dgx_ui_widget_t mode_w = {.kind = DGX_UI_DROPDOWN, .text = &text_box[T_MODE], .min = 0, .max = 3, .value = &mode_chosen, .flags = DGX_UI_REST | DGX_UI_STRETCH,
                                       .look = &dgx_ui_look_dropdown, .on = on_mode};
static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .max = 100, .step = 1, .value = &slider_percent, .want_h = 22, .look = &dgx_ui_look_slider, .on = on_slider};
static const dgx_ui_widget_t value_w  = {.kind = DGX_UI_LABEL, .text = &text_box[T_SLIDER], .want_h = 20};
/* the fields, each on a paper of its own so that it shows empty: looks of the program's colours, constants in flash;
   the PIN's a secret one, a star for each digit */
static const dgx_ui_input_look_t field_colours  = {.ink = 1, .caret = 3, .paper = 2};
static const dgx_ui_look_t       field_look     = {.draw = dgx_ui_draw_input, .params = &field_colours, .fill = -1};
static const dgx_ui_input_look_t secret_colours = {.ink = 1, .caret = 3, .paper = 2, .secret = true};
static const dgx_ui_look_t       secret_look    = {.draw = dgx_ui_draw_input, .params = &secret_colours, .fill = -1};
static const dgx_ui_widget_t     field_row      = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 6, .want_h = 22};
static const dgx_ui_widget_t code_l_w = {.kind = DGX_UI_LABEL, .text = &text_box[T_CODE_LABEL], .want_w = 40, .flags = DGX_UI_STRETCH};
static const dgx_ui_widget_t code_w   = {.kind = DGX_UI_INPUT, .text = &text_box[T_CODE], .flags = DGX_UI_REST | DGX_UI_STRETCH, .look = &field_look, .on = on_field};
static const dgx_ui_widget_t pin_l_w  = {.kind = DGX_UI_LABEL, .text = &text_box[T_PIN_LABEL], .want_w = 40, .flags = DGX_UI_STRETCH};
static const dgx_ui_widget_t pin_w    = {.kind = DGX_UI_INPUT, .text = &text_box[T_PIN], .flags = DGX_UI_REST | DGX_UI_STRETCH, .look = &secret_look, .on = on_field};
static const dgx_ui_widget_t status_w = {.kind = DGX_UI_LABEL, .text = &text_box[T_STATUS], .flags = DGX_UI_REST};

/* the strip a field is edited in, a popup at the top of the display with the keypad under it: a copy of the field's
   text, given to the field on done and dropped on cancel, so the field keeps what it had */
static const dgx_ui_widget_t strip_w        = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 4};
static const dgx_ui_widget_t strip_plain_w  = {.kind = DGX_UI_INPUT, .text = &text_box[T_EDIT], .flags = DGX_UI_REST | DGX_UI_STRETCH, .look = &field_look, .on = on_strip};
static const dgx_ui_widget_t strip_secret_w = {.kind = DGX_UI_INPUT, .text = &text_box[T_EDIT], .flags = DGX_UI_REST | DGX_UI_STRETCH, .look = &secret_look, .on = on_strip};

/* the popup: a frame of a question and a row of two buttons sharing it */
static const dgx_ui_widget_t pop_w     = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 8, .gap = 8};
static const dgx_ui_widget_t ask_w     = {.kind = DGX_UI_LABEL, .text = &text_box[T_ASK], .flags = DGX_UI_REST};
static const dgx_ui_widget_t answers_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 8, .want_h = 26};
static const dgx_ui_widget_t yes_w = {.kind = DGX_UI_BUTTON, .text = &text_box[T_YES], .flags = DGX_UI_STRETCH | DGX_UI_REST, .look = &dgx_ui_look_button, .on = on_answer, .ctx = (void *)1};
static const dgx_ui_widget_t no_w  = {.kind = DGX_UI_BUTTON, .text = &text_box[T_NO], .flags = DGX_UI_STRETCH | DGX_UI_REST, .look = &dgx_ui_look_button, .on = on_answer, .ctx = (void *)0};

static dgx_ui_node_t      nodes[20], pop_nodes[6], strip_nodes[2];
static dgx_ui_t           ui, pop, strip;
static int                status, value, editing = -1; /* editing: the field the strip edits, -1 none */
static dgx_ui_dropdown_t  dropdown; /* the popup of a dropdown's list: one for all the dropdowns */
static dgx_screen_t      *scr;
static int                mm_w, mm_h; /* the glass in millimetres: the keyboard's keys and the gestures are sized by it */
static dgx_ui_paint_t     paint;
static dgx_di_keyboard_t  kb;
static QueueHandle_t      queue;
static dgx_ui_ticker_t    ticker;

static void on_popup_button(dgx_ui_t *t, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_ACTIVATED) return;
    dgx_ui_popup_open(t, &pop, 60, 50, 200, 100, DGX_UI_POPUP_MODAL);
    dgx_ui_text_set(t, status, "a popup asks");
}

static void on_answer(dgx_ui_t *t, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_ACTIVATED) return;
    dgx_ui_popup_close(&ui);
    dgx_ui_text_set(&ui, status, ctx ? "the popup said yes" : "the popup said no");
}

static void on_switch(dgx_ui_t *t, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_CHANGED) return;
    dgx_ui_text_printf(t, status, "the switch turned %s", switch_on ? "on" : "off");
}

/* the dropdown: clicked, its list opened; an option chosen, told by CHANGED */
static void on_mode(dgx_ui_t *t, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) dgx_ui_dropdown_open(t, i, &dropdown, NULL, NULL);
    if (what == DGX_UI_CHANGED) {
        static const char *const modes[] = {"Auto", "Heat", "Cool", "Off"};
        dgx_ui_text_printf(t, status, "the mode: %s", modes[mode_chosen & 3]);
    }
}

static void on_slider(dgx_ui_t *t, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_CHANGED) return;
    dgx_ui_text_printf(t, value, "%d %%", (int)slider_percent);
}

/* the keypad of this program: rows of 7.5 mm, so that on the 2.8" glass of a CYD a strip is left above it; its last
   row cancel, 0, erase, done (keypad.json, made a C array by tools/keyboard_blob.py of DGX-DI) */
extern const uint8_t keypad[];
extern const size_t  keypad_size;

/* a field edited: the keypad at the bottom, the strip above it with a copy of the field's text and the caret */
static void edit_open(int field)
{
    if (editing >= 0) return;
    const dgx_di_rect_t top = {.x = 0, .y = 0, .w = (int16_t)scr->width, .h = 1}; /* a field at the top: the band under it */
    if (!dgx_di_keyboard_open(&kb, scr, mm_w, mm_h, keypad, keypad_size, &dgx_font_keys, &top) || kb.y < 16) {
        if (kb.open) dgx_di_keyboard_close(&kb);
        dgx_ui_text_set(&ui, status, "no room for the keypad");
        return;
    }
    paint.cover = dgx_ui_rect(kb.x, kb.y, kb.w, kb.h); /* the keypad's band: the painter leaves it alone */
    const dgx_ui_widget_t *w = dgx_ui_widget(&ui, field);
    dgx_ui_init(&strip, strip_nodes, 2, 1);
    dgx_ui_add(&strip, -1, &strip_w);
    const int in = dgx_ui_add(&strip, 0, w->look == &secret_look ? &strip_secret_w : &strip_plain_w);
    dgx_ui_text_set(&strip, in, w->text->buf);
    dgx_ui_popup_open(&ui, &strip, 0, 0, scr->width, kb.y, DGX_UI_POPUP_MODAL);
    dgx_ui_focus(&strip, in);
    editing = field;
}

/* the strip closed: its text given to the field (done), or dropped (cancel) and the field as it was; the focus stays
   on the field, so the keys go on from it to the next */
static void edit_close(bool keep)
{
    if (editing < 0) return;
    const int field = editing;
    editing         = -1;
    if (keep) dgx_ui_text_set(&ui, field, text_buf[T_EDIT]);
    dgx_di_keyboard_close(&kb);
    paint.cover = dgx_ui_rect(0, 0, 0, 0); /* the painter composes again where it lay */
    dgx_ui_popup_close(&ui);
    if (!keep) dgx_ui_text_set(&ui, status, "cancelled: the field kept its value");
    else if (dgx_ui_widget(&ui, field)->look == &secret_look) dgx_ui_text_printf(&ui, status, "a PIN of %d digits", (int)strlen(text_buf[T_PIN]));
    else dgx_ui_text_printf(&ui, status, "the code: %s", text_buf[T_CODE]);
}

/* a field clicked, or Enter on it: edited in the strip; the walk of the keys passes it, a swipe opens nothing */
static void on_field(dgx_ui_t *t, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EDIT) edit_open(i);
}

/* the strip's done and cancel, from the keypad */
static void on_strip(dgx_ui_t *t, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) edit_close(true);
    if (what == DGX_UI_BACK) edit_close(false);
}

/* the finger and the keys, read at a tick: the keyboard takes its fingers first, then the tree gets the first one */
static dgx_di_pointer_t *pointer; /* the board's, calibrated; NULL: no touch */
static dgx_di_touch_t    touch;

static void read_input(uint32_t now)
{
#ifdef DGX_HOST_WINDOW
    vTaskDelay(0); /* the window of DGX reads the mouse and the keys in the waits of the program: one of no time */
#endif
    if (pointer) {
        dgx_di_pointer_read(pointer, now);
        if (kb.open) {
            uint32_t  typed[16];
            const int n = dgx_di_keyboard_take(&kb, pointer, now, typed, 16);
            if (n > 0) dgx_ui_typed(&ui, typed, n);
        }
        dgx_ui_di_gestures_t g;
        dgx_ui_di_pointer(&ui, pointer, &touch, now, &g);
    }
    /* the buttons, given to the tree at every tick, all those held: one it did not see go down would be a new press at
       the next tick (Enter as done would open the strip again). While the strip is open it takes them, its field told
       DGX_UI_EDIT, which on_strip() leaves; then the program's own: A while editing is done, BACK cancels the strip or
       closes the popup, as no widget takes it */
    static uint32_t was;
    const uint32_t  held = buttons_held(), went = held & ~was;
    const bool      was_editing = editing >= 0;
    was                         = held;
    dgx_ui_buttons(&ui, held, now);
    if (was_editing && (went & (DGX_UI_PAD_BACK | DGX_UI_PAD_A))) {
        edit_close(went & DGX_UI_PAD_A);
        return;
    }
    if ((went & DGX_UI_PAD_BACK) && ui.popup) {
        dgx_ui_popup_close(&ui);
        dgx_ui_text_set(&ui, status, "BACK closed the popup");
    }
}

void app_main(void)
{
    /* the parts of the board, the touch after the display: on an ESP32 its start resets the DMA both SPI hosts share */
    board_display_t display = {0};
    if (!board_display_init(&display) || !(scr = board_dgx_screen(&display))) return;
    mm_w = display.mm_w, mm_h = display.mm_h;
    board_touch_t board_touch;
    if (board_touch_start(&display, &board_touch)) pointer = board_dgx_di_pointer(&board_touch);
    const uint32_t colors[] = {
        dgx_screen_rgb(scr, 18, 22, 30),    /* 0 the paper */
        dgx_screen_rgb(scr, 232, 232, 222), /* 1 the ink */
        dgx_screen_rgb(scr, 44, 60, 86),    /* 2 a fill: a button's face, a track */
        dgx_screen_rgb(scr, 240, 160, 60),  /* 3 the accent: pressed, on, chosen; the focus is a frame of the ink */
    };
    for (int k = 0; k < TEXTS; ++k) {
        const bool centred = k == T_POPUP || k == T_YES || k == T_NO || k == T_ASK;
        if (k == T_EDIT) continue; /* the strip's own, below */
        dgx_ui_textbox_init(&text_box[k], text_buf[k], sizeof text_buf[k], 1, centred ? &middle : &left);
    }
    /* the strip's copy: no longer than the fields it edits, so that what is typed fits them on done */
    dgx_ui_textbox_init(&text_box[T_EDIT], text_buf[T_EDIT], sizeof text_buf[T_CODE], 1, &left);

    dgx_ui_init(&ui, nodes, 20, 1);
    dgx_ui_add(&ui, -1, &root_w);
    const int title = dgx_ui_add(&ui, 0, &title_w);
    const int row   = dgx_ui_add(&ui, 0, &row_w);
    const int pb    = dgx_ui_add(&ui, row, &popup_w);
    dgx_ui_add(&ui, row, &switch_w);
    dgx_ui_add(&ui, row, &mode_w);
    dgx_ui_add(&ui, 0, &slider_w);
    value              = dgx_ui_add(&ui, 0, &value_w);
    const int code_row = dgx_ui_add(&ui, 0, &field_row);
    const int code_l   = dgx_ui_add(&ui, code_row, &code_l_w);
    dgx_ui_add(&ui, code_row, &code_w);
    const int pin_row = dgx_ui_add(&ui, 0, &field_row);
    const int pin_l   = dgx_ui_add(&ui, pin_row, &pin_l_w);
    dgx_ui_add(&ui, pin_row, &pin_w);
    status = dgx_ui_add(&ui, 0, &status_w);
    dgx_ui_text_set(&ui, title, "dgx-ui: controls");
    dgx_ui_text_set(&ui, pb, "Popup...");
    strcpy(text_buf[T_MODE], "Auto\nHeat\nCool\nOff"); /* the options, set once: not through text_set, nothing to mark */
    dgx_ui_text_set(&ui, value, "0 %");
    dgx_ui_text_set(&ui, code_l, "Code");
    dgx_ui_text_set(&ui, pin_l, "PIN");
    dgx_ui_text_set(&ui, status, "touch, or the arrows and Enter");

    dgx_ui_init(&pop, pop_nodes, 6, 1);
    dgx_ui_add(&pop, -1, &pop_w);
    const int ask     = dgx_ui_add(&pop, 0, &ask_w);
    const int answers = dgx_ui_add(&pop, 0, &answers_w);
    const int yes     = dgx_ui_add(&pop, answers, &yes_w);
    const int no      = dgx_ui_add(&pop, answers, &no_w);
    dgx_ui_text_set(&pop, ask, "Really?");
    dgx_ui_text_set(&pop, yes, "Yes");
    dgx_ui_text_set(&pop, no, "No");

    if (pointer) dgx_di_touch_display(&touch, scr, mm_w, mm_h);
    paint = (dgx_ui_paint_t){.scr = scr, .colors = colors, .color_count = 4, .background = 0};
#ifdef DGX_HOST_WINDOW
    if (getenv("CONTROLS_PLACES")) { /* where the widgets lie, for a script of DGX_HOST_SCRIPT to aim at */
        dgx_ui_screen(&ui, scr->width, scr->height, 0), dgx_ui_layout(&ui);
        dgx_ui_dump(&ui);
    }
#endif
    queue = xQueueCreate(8, sizeof(dgx_ui_msg_t));
    dgx_ui_ticker_start(&ticker, queue, MSG_TOUCH, 15);

    TickType_t wait = 0;
    for (;;) {
        dgx_ui_msg_t msg;
        while (xQueueReceive(queue, &msg, wait) == pdTRUE) {
            wait               = 0;
            const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
            if (msg.what == MSG_TOUCH) read_input(now);
            else dgx_ui_take(&ui, &msg);
            dgx_ui_msg_done(&msg); /* once, whatever handled it: the ticker may send the next */
        }
        const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        dgx_ui_time(&ui, now);
        dgx_ui_paint(&ui, &paint, now);
        /* asleep until a message comes, or until the moment a widget or a held key asks for: a tick at least */
        const uint32_t ms = dgx_ui_wake(&paint, &ui, now);
        wait              = ms == DGX_UI_WAKE_NONE ? portMAX_DELAY : pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1;
    }
}
