/* The recipes of the cookbook run: its C blocks, as the checker joined them, with the program's own functions stubbed. */
#include "docs_en_cookbook.md.c" /* written by check_docs.py into its build directory: give it with -I */
#include <stdio.h>
#include "dgx_vscreen.h"

static int saves, backs, menus, backlight = -1, wifi = -1;
void settings_save(void) { ++saves; }
void backlight_set(int percent) { backlight = percent; }
void wifi_enable(bool on) { wifi = on; }
void menu_back(void) { ++backs; }
void menu_open(void) { ++menus; }
void wifi_connect(const char *password) { (void)password; }
const uint16_t logo_pixels[64 * 32];
int64_t    esp_timer_get_time(void) { return 0; }
void       vTaskDelay(TickType_t t) { (void)t; }
BaseType_t xQueueSend(QueueHandle_t q, const void *i, TickType_t w) { return pdTRUE; }
bool dgx_ui_post(QueueHandle_t q, int what, const void *data, size_t size) { return true; }
bool dgx_ui_post_ref(QueueHandle_t q, int what, void *data, size_t size, void (*done)(void *, size_t)) { return true; }
bool dgx_ui_post_from_isr(QueueHandle_t q, int what, const void *data, size_t size, BaseType_t *woken) { return true; }
bool dgx_ui_ticker_start(dgx_ui_ticker_t *t, QueueHandle_t q, int what, uint32_t ms) { return true; }
int  dgx_di_pointer_read(dgx_di_pointer_t *p, uint32_t now) { return 0; }
bool dgx_di_pointer_first(const dgx_di_pointer_t *p, int *x, int *y) { *x = *y = 0; return false; }
void dgx_di_touch_take(dgx_di_touch_t *t, int x, int y, bool down, uint32_t now) {}
int  dgx_di_keyboard_take(dgx_di_keyboard_t *kb, dgx_di_pointer_t *p, uint32_t now, uint32_t *typed, int max) { return 0; }
/* the keyboard of the strip's recipe: its band the lower part of the display, from 36 pixels down, as on a CYD */
const uint8_t keypad[1];
const size_t  keypad_size = sizeof keypad;
bool dgx_di_keyboard_open(dgx_di_keyboard_t *kb, dgx_screen_t *d, int mm_w, int mm_h, const uint8_t *layout, size_t size, dgx_font_t *font,
                          const dgx_di_rect_t *field)
{
    kb->open = true, kb->x = 0, kb->y = 36, kb->w = (int16_t)d->width, kb->h = (int16_t)(d->height - 36);
    return true;
}
void dgx_di_keyboard_close(dgx_di_keyboard_t *kb) { kb->open = false; }

static uint32_t table[COLORS];
static const dgx_ui_widget_t col_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};

/* a picture of a screen as PPM */
static void shot(dgx_screen_t *scr, const char *dir, const char *name)
{
    static uint32_t px[320 * 240];
    char            path[600];
    if (!dir) return;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    dgx_vscreen_to_argb8888(scr, 0, 0, scr->width, scr->height, NULL, px, scr->width);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", scr->width, scr->height);
    for (int i = 0; i < scr->width * scr->height; ++i) fputc(px[i] >> 16 & 255, f), fputc(px[i] >> 8 & 255, f), fputc(px[i] & 255, f);
    fclose(f);
}

/* a rectangle outlined in thin dashes: three pixels on, three off */
static void dashes(dgx_screen_t *scr, int x, int y, int w, int h, uint32_t color)
{
    for (int k = 0; k < w; ++k)
        if (k % 6 < 3) dgx_set_pixel(scr, x + k, y, color), dgx_set_pixel(scr, x + k, y + h - 1, color);
    for (int k = 0; k < h; ++k)
        if (k % 6 < 3) dgx_set_pixel(scr, x, y + k, color), dgx_set_pixel(scr, x + w - 1, y + k, color);
}

/* the recipe of the ring scale on a screen of 240: its picture, and the same with the boxes of the layout in dashes */
static void ring_pictures(const char *dir)
{
    static dgx_ui_node_t nodes[8];
    dgx_ui_t             ui;
    dgx_screen_t        *scr = dgx_vscreen_init(240, 240, 16, DgxScreenRGB);
    static uint32_t      table[COLORS];
    table[PAPER] = dgx_screen_rgb(scr, 12, 14, 20), table[INK] = dgx_screen_rgb(scr, 235, 235, 225);
    dgx_ui_paint_t paint = {.scr = scr, .colors = table, .color_count = COLORS, .background = PAPER};
    dgx_ui_init(&ui, nodes, 8, 1);
    const int ring = ring_add(&ui, scr);
    dgx_ui_msg_t    msg;
    const ppm_t     v = {.ppm = 1340};
    const climate_t c = {.t10 = 215, .rh = 41};
    dgx_ui_msg_value(&msg, MSG_PPM, &v, sizeof v), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_msg_value(&msg, MSG_CLIMATE, &c, sizeof c), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_paint(&ui, &paint, 0);
    shot(scr, dir, "ring.ppm");
    /* a second value: only the steps that changed and the number's field */
    const ppm_t v2 = {.ppm = 1350};
    dgx_ui_msg_value(&msg, MSG_PPM, &v2, sizeof v2), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_paint(&ui, &paint, 1000);
    const ppm_t v3 = {.ppm = 1340};
    dgx_ui_msg_value(&msg, MSG_PPM, &v3, sizeof v3), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_paint(&ui, &paint, 2000);
    /* the layout in dashes: the square the pad leaves, then the box of every widget */
    const dgx_ui_node_t *r   = dgx_ui_node(&ui, ring);
    const int            pad = r->widget->pad * ui.unit;
    dashes(scr, r->x + pad, r->y + pad, r->w - 2 * pad, r->h - 2 * pad, dgx_screen_rgb(scr, 90, 140, 255));
    for (int i = 0; i < ui.count; ++i) {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, i);
        if (n->widget && n->w > 0 && n->h > 0) dashes(scr, n->x, n->y, n->w, n->h, dgx_screen_rgb(scr, 255, 170, 60));
    }
    shot(scr, dir, "ring-layout.ppm");
    for (int i = 0; i < ui.count; ++i) {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, i);
        printf("ring: widget %d at %d,%d %dx%d\n", i, n->x, n->y, n->w, n->h);
    }
}

int main(int argc, char **argv)
{
    dgx_screen_t  *scr = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
    for (int k = 0; k < COLORS; ++k) table[k] = dgx_screen_rgb(scr, 30 * k, 30 * k, 255 - 30 * k);
    dgx_ui_paint_t paint = {.scr = scr, .colors = table, .color_count = COLORS, .background = PAPER};
    styles_init(), city_style_init();

    interface_test(); /* the message and the tap of the last recipe */
    printf("test recipe: the label got its text, the tap saved %d time(s)\n", saved);

    static dgx_ui_node_t nodes[32];
    dgx_ui_t             ui;
    dgx_ui_init(&ui, nodes, 32, 2);
    const int root = dgx_ui_add(&ui, -1, &col_w);

    /* the level look: 40 of 100 */
    const int level = dgx_ui_add(&ui, root, &level_w);
    dgx_ui_set_value(&ui, level, 40);
    dgx_ui_paint(&ui, &paint, 0);
    const dgx_ui_node_t *n = dgx_ui_node(&ui, level);
    printf("level look: box %d,%d %dx%d; a pixel at 20%% is the bar %d, at 80%% the rest %d\n", n->x, n->y, n->w, n->h,
           dgx_get_pixel(scr, n->x + n->w / 5, n->y + 1) == table[ACCENT], dgx_get_pixel(scr, n->x + n->w * 4 / 5, n->y + 1) == table[FILL]);

    /* the icon: a message of 3 bars, then of none */
    const int   sig = dgx_ui_add(&ui, root, &signal_w);
    dgx_ui_msg_t msg;
    signal_t     s = {.bars = 3};
    dgx_ui_msg_value(&msg, MSG_SIGNAL, &s, sizeof s), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    const int v3 = dgx_ui_node(&ui, sig)->state_value.value;
    s.bars = 0;
    dgx_ui_msg_value(&msg, MSG_SIGNAL, &s, sizeof s), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    printf("icon: value %d after 3 bars, %d after none (outside 1..4: no picture); its box %dx%d\n", v3, dgx_ui_node(&ui, sig)->state_value.value,
           dgx_ui_node(&ui, sig)->w, dgx_ui_node(&ui, sig)->h);

    /* the slider and the switch by keys */
    const int sl = dgx_ui_add(&ui, root, &backlight_w), sw = dgx_ui_add(&ui, root, &wifi_w);
    dgx_ui_focus(&ui, sl), dgx_ui_key(&ui, DGX_UI_KEY_MORE);
    dgx_ui_focus(&ui, sw), dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE);
    backlight_show(&ui, sl, 30);
    printf("slider: started at 70, MORE told backlight_set(%d); set to 30 by the program, on() not told again (%d); switch told wifi_enable(%d)\n", backlight,
           backlight, wifi);

    /* the button, and the alarm that comes and goes */
    const int save = save_add(&ui, root);
    dgx_ui_focus(&ui, save), dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE);
    alarm_set(&ui, root, "filter!");
    const int a1 = dgx_ui_find(&ui, root, TAG_ALARM), first = dgx_ui_node(&ui, root)->child;
    alarm_set(&ui, root, NULL);
    printf("button: activated by the key, saves %d; alarm: found %d, it is the first child %d, after removal found %d\n", saves, a1 >= 0, a1 == first,
           dgx_ui_find(&ui, root, TAG_ALARM));

    /* the texts of lines, the buttons of the board, the colours */
    texts_add(&ui, root);
    bool more = dgx_ui_paint(&ui, &paint, 0);
    printf("lines: asks for time while the letters come: %d\n", more);
    buttons_t b = {.held = DGX_UI_PAD_DOWN};
    dgx_ui_msg_value(&msg, MSG_BUTTONS, &b, sizeof b), buttons_message(&ui, &msg, 100);
    const int f1 = ui.focus;
    b.held       = DGX_UI_PAD_BACK;
    dgx_ui_msg_value(&msg, MSG_BUTTONS, &b, sizeof b), buttons_message(&ui, &msg, 200);
    printf("buttons: DOWN moved the focus from %d to %d; BACK was the program's: menu_back %d time(s)\n", save, f1, backs);
    colors_set(&ui, &paint, table);

    /* the popup: opened, Yes activated by keys */
    ask_open(&ui);
    dgx_ui_paint(&ui, &paint, 300);
    const int saves0 = saves;
    dgx_ui_key(&ui, DGX_UI_KEY_NEXT), dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE);
    printf("popup: open %d before; Yes by keys saved %d more time(s), open after: %d\n", 1, saves - saves0, ui.popup != NULL);
    dgx_ui_paint(&ui, &paint, 400);
    dgx_ui_paint_free(&paint);
    /* the row of three buttons: right does nothing by default, and moves the focus with the program's table */
    {
        static const dgx_ui_widget_t  line_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 20};
        static const dgx_ui_widget_t  b_w    = {.kind = DGX_UI_BUTTON, .want_w = 40};
        static dgx_ui_node_t          rn[6];
        dgx_ui_t                      r;
        dgx_ui_init(&r, rn, 6, 1);
        const int line = dgx_ui_add(&r, -1, &line_w), back = dgx_ui_add(&r, line, &b_w), ok = dgx_ui_add(&r, line, &b_w);
        dgx_ui_add(&r, line, &b_w);
        dgx_ui_screen(&r, 200, 100, 0);
        dgx_ui_focus(&r, back);
        const bool took_default = dgx_ui_buttons(&r, DGX_UI_PAD_RIGHT, 0);
        const int  f_default    = r.focus;
        dgx_ui_buttons(&r, 0, 10);
        dialog_keys(&r, true);
        const bool took_row = dgx_ui_buttons(&r, DGX_UI_PAD_RIGHT, 20);
        printf("row of buttons: by default right is taken %d and the focus stays on Back %d; with row_keys taken %d and the focus is on OK %d\n", took_default,
               f_default == back, took_row, r.focus == ok);
    }
    /* the strip over the keypad: a code of 123 touched, 9 typed and cancelled, then typed and done; a PIN in stars */
    {
        static dgx_ui_node_t fn[4];
        static dgx_ui_t      f;
        dgx_screen_t        *scr = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
        dgx_ui_paint_t       pp  = {.scr = scr, .colors = table, .color_count = COLORS, .background = PAPER};
        dgx_ui_textbox_init(&code_box, code_buf, sizeof code_buf, 1, &st_left);
        dgx_ui_textbox_init(&pin_box, pin_buf, sizeof pin_buf, 1, &st_left);
        dgx_ui_init(&f, fn, 4, 1);
        const int top = dgx_ui_add(&f, -1, &col_w), code = dgx_ui_add(&f, top, &code_w), pin = dgx_ui_add(&f, top, &pin_w);
        dgx_ui_screen(&f, 320, 240, 0);
        dgx_ui_text_set(&f, code, "123");
        edit_ui = &f, edit_scr = scr, edit_mm_w = 58, edit_mm_h = 43;
        dgx_ui_layout(&f);
        const dgx_ui_node_t *cn = dgx_ui_node(&f, code), *pn = dgx_ui_node(&f, pin);
        const dgx_ui_pointer_t down = {.x = (int16_t)(cn->x + 5), .y = (int16_t)(cn->y + 5), .down = true, .pressed = true, .ms = 10};
        const dgx_ui_pointer_t up   = {.x = down.x, .y = down.y, .released = true, .ms = 60};
        dgx_ui_pointer(&f, &down), dgx_ui_pointer(&f, &up);
        keyboard_cover(&pp, &keypad_kb), dgx_ui_paint(&f, &pp, 100);
        const bool     opened = f.popup == &strip_ui && keypad_kb.open, pressed = cn->state_value.bits & DGX_UI_PRESSED;
        const uint32_t nine[] = {'9'}, cancel[] = {DGX_UI_CP_CANCEL}, done[] = {DGX_UI_CP_DONE};
        dgx_ui_typed(&f, nine, 1);
        printf("strip: clicked, open %d, the field pressed %d; 9 typed: the strip \"%s\", the field \"%s\"\n", opened, pressed, edit_buf, code_buf);
        dgx_ui_typed(&f, cancel, 1);
        keyboard_cover(&pp, &keypad_kb), dgx_ui_paint(&f, &pp, 200);
        printf("strip: cancel: open %d, the field \"%s\", the focus on it %d\n", f.popup != NULL || keypad_kb.open, code_buf, f.focus == code);
        dgx_ui_pointer(&f, &down), dgx_ui_pointer(&f, &up);
        dgx_ui_typed(&f, nine, 1), dgx_ui_typed(&f, done, 1);
        printf("strip: clicked again, 9 and done: open %d, the field \"%s\"\n", f.popup != NULL || keypad_kb.open, code_buf);
        /* a swipe that begins on the field opens nothing */
        const dgx_ui_pointer_t swiped = {.x = (int16_t)(cn->x + 60), .y = down.y, .down = true, .swipe = true, .ms = 80};
        dgx_ui_pointer(&f, &down), dgx_ui_pointer(&f, &swiped), dgx_ui_pointer(&f, &(dgx_ui_pointer_t){.x = swiped.x, .y = swiped.y, .released = true, .ms = 120});
        printf("strip: a swipe begun on the field: open %d\n", f.popup != NULL || keypad_kb.open);
        /* the keys: the walk goes on from the code to the PIN and opens nothing; the key that activates opens the strip */
        dgx_ui_buttons(&f, DGX_UI_PAD_DOWN, 1000), dgx_ui_buttons(&f, 0, 1010);
        const bool walked = f.focus == pin && !f.popup;
        dgx_ui_buttons(&f, DGX_UI_PAD_A, 1020), dgx_ui_buttons(&f, 0, 1030);
        printf("strip: DOWN from the code: the focus on the PIN and nothing open %d; A on it: open %d\n", walked, f.popup == &strip_ui);
        /* a button of the board that means done: after dgx_ui_buttons() got it, the strip stays closed at the next tick */
        dgx_ui_buttons(&f, DGX_UI_PAD_A, 1100), edit_close(true);
        dgx_ui_buttons(&f, DGX_UI_PAD_A, 1115);
        const bool stays = !f.popup;
        dgx_ui_buttons(&f, 0, 1130);
        /* and instead of it: the button is a new press at the next tick, and the strip opens again */
        dgx_ui_buttons(&f, DGX_UI_PAD_A, 1200), dgx_ui_buttons(&f, 0, 1210); /* open */
        edit_close(true);                                                    /* the program's own A, dgx_ui_buttons() not called */
        dgx_ui_buttons(&f, DGX_UI_PAD_A, 1315);
        printf("strip: done by a button after dgx_ui_buttons(): stays closed %d; instead of it: open again %d\n", stays, f.popup == &strip_ui);
        dgx_ui_buttons(&f, 0, 1330), edit_close(false);
        /* the PIN: the strip in the secret look, the letters kept */
        const dgx_ui_pointer_t pd = {.x = (int16_t)(pn->x + 5), .y = (int16_t)(pn->y + 5), .down = true, .pressed = true, .ms = 2000};
        dgx_ui_pointer(&f, &pd), dgx_ui_pointer(&f, &(dgx_ui_pointer_t){.x = pd.x, .y = pd.y, .released = true, .ms = 2050});
        const bool     secret = f.popup == &strip_ui && dgx_ui_widget(&strip_ui, 1)->look == &look_pin;
        const uint32_t four[] = {'4', '2', DGX_UI_CP_DONE};
        dgx_ui_typed(&f, four, 3);
        keyboard_cover(&pp, &keypad_kb), dgx_ui_paint(&f, &pp, 400);
        printf("strip: the PIN's strip is secret %d; 4, 2 and done: the PIN's text box holds \"%s\"\n", secret, pin_buf);
        dgx_ui_paint_free(&pp), dgx_ui_free(&f);
        scr->destroy(&scr);
    }
    ring_pictures(argc > 1 ? argv[1] : NULL);
    puts("cookbook: ran to the end");
    return 0;
}
