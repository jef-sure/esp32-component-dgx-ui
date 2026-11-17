/* The recipes of the cookbook run: its C blocks, as the checker joined them, with the program's own functions stubbed. */
#include "docs_en_cookbook.md.c" /* written by check_docs.py into its build directory: give it with -I */
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

static uint32_t table[COLORS];
static const dgx_ui_widget_t col_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};

int main(void)
{
    dgx_screen_t  *scr = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
    for (int k = 0; k < COLORS; ++k) table[k] = dgx_screen_rgb(scr, 30 * k, 30 * k, 255 - 30 * k);
    dgx_ui_paint_t paint = {.scr = scr, .colors = table, .color_count = COLORS, .background = PAPER};
    styles_init(), city_style_init(), looks_init();

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
    puts("cookbook: ran to the end");
    return 0;
}
