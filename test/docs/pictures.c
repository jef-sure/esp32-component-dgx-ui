/* The pictures of the documentation, drawn by dgx-ui itself on a virtual screen of DGX; the code of the pages is here. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/TerminusTTFMedium12.h"

static void shot(dgx_screen_t *scr, const char *name)
{
    static uint32_t px[320 * 240];
    dgx_vscreen_to_argb8888(scr, 0, 0, scr->width, scr->height, NULL, px, scr->width);
    FILE *f = fopen(name, "wb");
    fprintf(f, "P6\n%d %d\n255\n", scr->width, scr->height);
    for (int i = 0; i < scr->width * scr->height; ++i) fputc(px[i] >> 16 & 255, f), fputc(px[i] >> 8 & 255, f), fputc(px[i] & 255, f);
    fclose(f);
}

/* ---------------------------------------------------------------- the colours and the fonts of every scene */
enum { PAPER, INK, FILL, BORDER, ACCENT, ACCENT_DOWN, SHADOW, DIM, COLORS };
static uint32_t colors[COLORS];
static void make_colors(dgx_screen_t *scr)
{
    colors[PAPER]       = dgx_screen_rgb(scr, 16, 20, 28);
    colors[INK]         = dgx_screen_rgb(scr, 230, 230, 220);
    colors[FILL]        = dgx_screen_rgb(scr, 32, 48, 72);
    colors[BORDER]      = dgx_screen_rgb(scr, 120, 160, 220);
    colors[ACCENT]      = dgx_screen_rgb(scr, 40, 110, 200);
    colors[ACCENT_DOWN] = dgx_screen_rgb(scr, 90, 160, 240);
    colors[SHADOW]      = dgx_screen_rgb(scr, 0, 0, 0);
    colors[DIM]         = dgx_screen_rgb(scr, 90, 96, 110);
}
static dgx_ui_font_variant_t big[2], small[1];
static dgx_ui_text_style_t   st_big, st_left, st_right, st_centre, st_important, st_plain;
static void make_styles(void)
{
    big[0]   = (dgx_ui_font_variant_t){.font = DejaVuSansBook32()};
    big[1]   = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    small[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    st_big       = (dgx_ui_text_style_t){.variants = big, .count = 2, .align = DGX_FONT_ALIGN_CENTER, .flags = DGX_UI_TEXT_MIDDLE};
    st_left      = (dgx_ui_text_style_t){.variants = small, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    st_right     = (dgx_ui_text_style_t){.variants = small, .count = 1, .align = DGX_FONT_ALIGN_RIGHT};
    st_centre    = (dgx_ui_text_style_t){.variants = small, .count = 1, .align = DGX_FONT_ALIGN_CENTER};
    st_important = (dgx_ui_text_style_t){.variants = small, .count = 1, .flags = DGX_UI_TEXT_IMPORTANT, .scroll_ms = 2000};
    st_plain     = (dgx_ui_text_style_t){.variants = small, .count = 1};
}

/* ---------------------------------------------------------------- scene 1: the first program */
enum { MSG_CO2 = 1, MSG_SECOND = 2 };
typedef struct { int16_t ppm; } co2_t;
DGX_UI_DECODER(co2, MSG_CO2, co2_t)

static char             co2_buf[24], clock_buf[16], note_buf[24];
static dgx_ui_textbox_t co2_box, clock_box, note_box;
static uint32_t         fake_seconds;

static bool co2_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    co2_t c;
    if (msg_decode_co2(msg, &c) == ESP_OK) dgx_ui_text_printf(ui, i, "CO2 %d ppm", c.ppm);
    return false; /* read, not claimed: others may want it too */
}
static bool clock_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what != MSG_SECOND) return false;
    const uint32_t s = fake_seconds;
    dgx_ui_text_printf(ui, i, "%02u:%02u:%02u", (unsigned)(s / 3600 % 24), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
    return true; /* nobody else wants the second */
}
static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4, .gap = 4};
static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .want_h = 40};
static const dgx_ui_widget_t co2_w   = {.kind = DGX_UI_LABEL, .text = &co2_box, .want_h = 34, .take = co2_take};
static const dgx_ui_widget_t row_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 10};
static const dgx_ui_widget_t clock_w = {.kind = DGX_UI_LABEL, .text = &clock_box, .want_w = 50, .take = clock_take};
static const dgx_ui_widget_t note_w  = {.kind = DGX_UI_LABEL, .flags = DGX_UI_REST, .text = &note_box};

static void scene_first(dgx_screen_t *scr, const char *out)
{
    static dgx_ui_node_t nodes[8];
    static dgx_ui_t      ui;
    dgx_ui_textbox_init(&co2_box, co2_buf, sizeof co2_buf, 1, &st_big);
    dgx_ui_textbox_init(&clock_box, clock_buf, sizeof clock_buf, 1, &st_left);
    dgx_ui_textbox_init(&note_box, note_buf, sizeof note_buf, 1, &st_right);
    dgx_ui_init(&ui, nodes, 8, 2); /* a point of the sizes is 2 pixels */
    dgx_ui_add(&ui, -1, &root_w);
    const int frame = dgx_ui_add(&ui, 0, &frame_w);
    dgx_ui_add(&ui, frame, &co2_w);
    const int row = dgx_ui_add(&ui, 0, &row_w);
    dgx_ui_add(&ui, row, &clock_w);
    const int note = dgx_ui_add(&ui, row, &note_w);
    dgx_ui_text_set(&ui, note, "living room");
    /* the messages a sensor task and a ticker would send */
    dgx_ui_msg_t msg;
    const co2_t  c = {.ppm = 612};
    dgx_ui_msg_value(&msg, MSG_CO2, &c, sizeof c), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    fake_seconds = 9 * 3600 + 41 * 60 + 7;
    dgx_ui_msg_value(&msg, MSG_SECOND, NULL, 0), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = COLORS, .background = PAPER};
    dgx_ui_paint(&ui, &paint, 0);
    shot(scr, out);
}

/* ---------------------------------------------------------------- scene 2: controls, and a popup over them */
/* a signal icon of 16x16, 1 bit, four levels: rows from a new byte, the leftmost pixel in the high bit */
#define ROW4(a, b) a, b, a, b, a, b, a, b
static const uint8_t bars1[32] = {ROW4(0x00, 0x00), ROW4(0x00, 0x00), ROW4(0x00, 0x00), ROW4(0xC0, 0x00)};
static const uint8_t bars2[32] = {ROW4(0x00, 0x00), ROW4(0x00, 0x00), ROW4(0x0C, 0x00), ROW4(0xCC, 0x00)};
static const uint8_t bars3[32] = {ROW4(0x00, 0x00), ROW4(0x00, 0xC0), ROW4(0x0C, 0xC0), ROW4(0xCC, 0xC0)};
static const uint8_t bars4[32] = {ROW4(0x00, 0x0C), ROW4(0x00, 0xCC), ROW4(0x0C, 0xCC), ROW4(0xCC, 0xCC)};
static const dgx_image_t signal_im[4] = {
    {.width = 16, .height = 16, .format = DGX_IMAGE_1BPP, .data = bars1},
    {.width = 16, .height = 16, .format = DGX_IMAGE_1BPP, .data = bars2},
    {.width = 16, .height = 16, .format = DGX_IMAGE_1BPP, .data = bars3},
    {.width = 16, .height = 16, .format = DGX_IMAGE_1BPP, .data = bars4},
};
static const dgx_ui_picture_t signal_pic[4] = {{.image = &signal_im[0]}, {.image = &signal_im[1]}, {.image = &signal_im[2]}, {.image = &signal_im[3]}};

/* looks in the program's own colours: the library's drawing, parameters of its own, constants */
static const dgx_ui_button_look_t my_button = {
    .face = ACCENT, .pressed = ACCENT_DOWN, .focused = ACCENT_DOWN, .disabled = FILL, .text = INK, .text_disabled = DIM, .shadow = SHADOW, .depth = 1};
static const dgx_ui_slider_look_t my_slider = {.track = FILL, .fill = ACCENT, .knob = INK, .focused = BORDER, .disabled = DIM, .track_h = 2};
static const dgx_ui_switch_look_t my_switch = {.off = FILL, .on = ACCENT, .knob = INK, .focused = BORDER, .disabled = DIM};
static const dgx_ui_look_t look_button = {.draw = dgx_ui_draw_button, .params = &my_button, .fill = -1};
static const dgx_ui_look_t look_slider = {.draw = dgx_ui_draw_slider, .params = &my_slider, .fill = -1};
static const dgx_ui_look_t look_switch = {.draw = dgx_ui_draw_switch, .params = &my_switch, .fill = -1};

static char title_buf[16], light_buf[16], wifi_buf[16], sound_buf[16], ok_buf[8], cancel_buf[8];
static dgx_ui_textbox_t title_box, light_box, wifi_box, sound_box, ok_box, cancel_box;
static int  saved;
static void on_ok(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED) saved = 1;
}
enum { TAG_LIGHT = 1, TAG_WIFI, TAG_SOUND, TAG_SIGNAL, TAG_OK, TAG_CANCEL };
static const dgx_ui_widget_t set_root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 6, .gap = 5};
static const dgx_ui_widget_t title_w     = {.kind = DGX_UI_LABEL, .text = &title_box, .want_h = 18};
static const dgx_ui_widget_t line_w      = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 12, .gap = 4};
static const dgx_ui_widget_t light_lbl_w = {.kind = DGX_UI_LABEL, .text = &light_box, .want_w = 50, .at_y = 2};
static const dgx_ui_widget_t light_w     = {.kind = DGX_UI_SLIDER, .min = 0, .max = 100, .step = 5, .want_w = 94, .tag = TAG_LIGHT, .state = {.value = 70}, .look = &look_slider};
static const dgx_ui_widget_t wifi_lbl_w  = {.kind = DGX_UI_LABEL, .text = &wifi_box, .want_w = 50, .at_y = 2};
static const dgx_ui_widget_t wifi_w      = {.kind = DGX_UI_SWITCH, .max = 1, .want_w = 26, .tag = TAG_WIFI, .state = {.value = 1}, .look = &look_switch};
static const dgx_ui_widget_t signal_w    = {.kind = DGX_UI_ICON, .picture = signal_pic, .min = 1, .max = 4, .want_w = 12, .tag = TAG_SIGNAL, .state = {.value = 3}, .look = &dgx_ui_look_picture};
static const dgx_ui_widget_t sound_lbl_w = {.kind = DGX_UI_LABEL, .text = &sound_box, .want_w = 50, .at_y = 2};
static const dgx_ui_widget_t sound_w     = {.kind = DGX_UI_SWITCH, .max = 1, .want_w = 26, .tag = TAG_SOUND, .look = &look_switch};
static const dgx_ui_widget_t buttons_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 18, .gap = 6};
static const dgx_ui_widget_t ok_w        = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_STRETCH, .text = &ok_box, .want_w = 60, .tag = TAG_OK, .look = &look_button, .on = on_ok};
static const dgx_ui_widget_t cancel_w    = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_STRETCH, .text = &cancel_box, .want_w = 60, .tag = TAG_CANCEL, .state = {.bits = DGX_UI_DISABLED}, .look = &look_button};

static dgx_ui_node_t set_nodes[20];
static dgx_ui_t      set_ui;
static void make_settings(void)
{
    dgx_ui_textbox_init(&title_box, title_buf, sizeof title_buf, 1, &st_big);
    dgx_ui_textbox_init(&light_box, light_buf, sizeof light_buf, 1, &st_left);
    dgx_ui_textbox_init(&wifi_box, wifi_buf, sizeof wifi_buf, 1, &st_left);
    dgx_ui_textbox_init(&sound_box, sound_buf, sizeof sound_buf, 1, &st_left);
    dgx_ui_textbox_init(&ok_box, ok_buf, sizeof ok_buf, 1, &st_centre);
    dgx_ui_textbox_init(&cancel_box, cancel_buf, sizeof cancel_buf, 1, &st_centre);
    dgx_ui_init(&set_ui, set_nodes, 20, 2);
    const int root = dgx_ui_add(&set_ui, -1, &set_root_w);
    dgx_ui_text_set(&set_ui, dgx_ui_add(&set_ui, root, &title_w), "Settings");
    int line = dgx_ui_add(&set_ui, root, &line_w);
    dgx_ui_text_set(&set_ui, dgx_ui_add(&set_ui, line, &light_lbl_w), "Backlight");
    dgx_ui_add(&set_ui, line, &light_w);
    line = dgx_ui_add(&set_ui, root, &line_w);
    dgx_ui_text_set(&set_ui, dgx_ui_add(&set_ui, line, &wifi_lbl_w), "Wi-Fi");
    dgx_ui_add(&set_ui, line, &wifi_w);
    dgx_ui_add(&set_ui, line, &signal_w);
    line = dgx_ui_add(&set_ui, root, &line_w);
    dgx_ui_text_set(&set_ui, dgx_ui_add(&set_ui, line, &sound_lbl_w), "Sound");
    dgx_ui_add(&set_ui, line, &sound_w);
    line = dgx_ui_add(&set_ui, root, &buttons_w);
    dgx_ui_text_set(&set_ui, dgx_ui_add(&set_ui, line, &ok_w), "Save");
    dgx_ui_text_set(&set_ui, dgx_ui_add(&set_ui, line, &cancel_w), "Undo");
}

/* the popup: a tree of its own */
static char ask_buf[32], yes_buf[8], no_buf[8];
static dgx_ui_textbox_t ask_box, yes_box, no_box;
static const dgx_ui_widget_t pop_root_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 5, .gap = 5};
static const dgx_ui_widget_t ask_w      = {.kind = DGX_UI_LABEL, .text = &ask_box, .want_h = 12};
static const dgx_ui_widget_t pop_row_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 18, .gap = 6};
static const dgx_ui_widget_t yes_w      = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_STRETCH | DGX_UI_REST, .text = &yes_box, .look = &look_button};
static const dgx_ui_widget_t no_w       = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_STRETCH | DGX_UI_REST, .text = &no_box, .look = &look_button};
static dgx_ui_node_t pop_nodes[6];
static dgx_ui_t      pop_ui;
static void make_popup(void)
{
    dgx_ui_textbox_init(&ask_box, ask_buf, sizeof ask_buf, 1, &st_centre);
    dgx_ui_textbox_init(&yes_box, yes_buf, sizeof yes_buf, 1, &st_centre);
    dgx_ui_textbox_init(&no_box, no_buf, sizeof no_buf, 1, &st_centre);
    dgx_ui_init(&pop_ui, pop_nodes, 6, 2);
    const int root = dgx_ui_add(&pop_ui, -1, &pop_root_w);
    dgx_ui_text_set(&pop_ui, dgx_ui_add(&pop_ui, root, &ask_w), "Save the settings?");
    const int row = dgx_ui_add(&pop_ui, root, &pop_row_w);
    dgx_ui_text_set(&pop_ui, dgx_ui_add(&pop_ui, row, &yes_w), "Yes");
    dgx_ui_text_set(&pop_ui, dgx_ui_add(&pop_ui, row, &no_w), "No");
}

static void scene_settings(dgx_screen_t *scr, const char *out, const char *out_popup)
{
    make_settings();
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = COLORS, .background = PAPER};
    dgx_ui_focus(&set_ui, dgx_ui_find(&set_ui, -1, TAG_OK));
    dgx_ui_paint(&set_ui, &paint, 0);
    shot(scr, out);
    make_popup();
    dgx_ui_popup_open(&set_ui, &pop_ui, 50, 70, 220, 92, DGX_UI_POPUP_MODAL);
    dgx_ui_key(&set_ui, DGX_UI_KEY_NEXT); /* the focus on the popup's first button */
    dgx_ui_paint(&set_ui, &paint, 0);
    shot(scr, out_popup);
    dgx_ui_popup_close(&set_ui);
    dgx_ui_paint(&set_ui, &paint, 0);
    dgx_ui_paint_free(&paint);
}

/* ---------------------------------------------------------------- scene 3: a text of several lines */
static char news_buf[200], hint_buf[120], head1_buf[24], head2_buf[24];
static dgx_ui_textbox_t news_box, hint_box, head1_box, head2_box;
static const dgx_ui_widget_t txt_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 6, .gap = 3};
static const dgx_ui_widget_t head1_w    = {.kind = DGX_UI_LABEL, .text = &head1_box};
static const dgx_ui_widget_t news_fr_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .want_h = 26};
static const dgx_ui_widget_t news_w     = {.kind = DGX_UI_LABEL, .text = &news_box, .look = &dgx_ui_look_lines};
static const dgx_ui_widget_t head2_w    = {.kind = DGX_UI_LABEL, .text = &head2_box};
static const dgx_ui_widget_t hint_fr_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .want_h = 21};
static const dgx_ui_widget_t hint_w     = {.kind = DGX_UI_LABEL, .text = &hint_box, .look = &dgx_ui_look_lines};

static void scene_lines(dgx_screen_t *scr, const char *out)
{
    static dgx_ui_node_t nodes[8];
    static dgx_ui_t      ui;
    dgx_ui_textbox_init(&head1_box, head1_buf, sizeof head1_buf, 1, &st_left);
    dgx_ui_textbox_init(&head2_box, head2_buf, sizeof head2_buf, 1, &st_left);
    dgx_ui_textbox_init(&news_box, news_buf, sizeof news_buf, 3, &st_important);
    dgx_ui_textbox_init(&hint_box, hint_buf, sizeof hint_buf, 2, &st_plain);
    dgx_ui_init(&ui, nodes, 8, 2);
    const int root = dgx_ui_add(&ui, -1, &txt_root_w);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, root, &head1_w), "important: scrolls");
    const int f1   = dgx_ui_add(&ui, root, &news_fr_w);
    const int news = dgx_ui_add(&ui, f1, &news_w);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, root, &head2_w), "not important: cut");
    const int f2   = dgx_ui_add(&ui, root, &hint_fr_w);
    const int hint = dgx_ui_add(&ui, f2, &hint_w);
    dgx_ui_text_set(&ui, news, "The filter has worked for 2000 hours. Replace it and hold the button for three seconds to start the count again. "
                               "Until then the fan runs at half its speed.");
    dgx_ui_text_set(&ui, hint, "Swipe from the left edge to open the menu, from the top edge to see the last warnings, and hold a row to change it.");
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = COLORS, .background = PAPER};
    bool more = dgx_ui_paint(&ui, &paint, 0);
    more      = dgx_ui_paint(&ui, &paint, 2100); /* one line up after scroll_ms */
    printf("lines: asks for more time %d, top %d of %d lines\n", more, news_box.top, news_box.total_lines);
    shot(scr, out);
}

/* ---------------------------------------------------------------- scene 4: a list, and a dropdown with its list open */
static char              mode_buf[32], fan_buf[32], mode_l_buf[8], fan_l_buf[8];
static dgx_ui_textbox_t  mode_box, fan_box, mode_l_box, fan_l_box;
static dgx_ui_dropdown_t fan_list;
static const dgx_ui_widget_t pick_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 8, .gap = 12};
static const dgx_ui_widget_t pick_col_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .gap = 3, .want_w = 66};
static const dgx_ui_widget_t mode_l_w    = {.kind = DGX_UI_LABEL, .text = &mode_l_box};
static const dgx_ui_widget_t fan_l_w     = {.kind = DGX_UI_LABEL, .text = &fan_l_box};
static const dgx_ui_widget_t mode_w = {.kind = DGX_UI_LIST, .text = &mode_box, .min = 0, .max = 3, .want_h = 56, .look = &dgx_ui_look_list};
static const dgx_ui_widget_t fan_w  = {.kind = DGX_UI_DROPDOWN, .text = &fan_box, .min = 0, .max = 2, .want_h = 16, .look = &dgx_ui_look_dropdown};

static void scene_pick(dgx_screen_t *scr, const char *out_list, const char *out_dropdown)
{
    static dgx_ui_node_t nodes[8];
    static dgx_ui_t      ui;
    /* the colours of the looks' defaults: 0 the paper, 1 the ink, 2 a fill, 3 a border */
    const uint32_t four[4] = {colors[PAPER], colors[INK], colors[FILL], colors[BORDER]};
    dgx_ui_textbox_init(&mode_box, mode_buf, sizeof mode_buf, 1, &st_left);
    dgx_ui_textbox_init(&fan_box, fan_buf, sizeof fan_buf, 1, &st_left);
    dgx_ui_textbox_init(&mode_l_box, mode_l_buf, sizeof mode_l_buf, 1, &st_left);
    dgx_ui_textbox_init(&fan_l_box, fan_l_buf, sizeof fan_l_buf, 1, &st_left);
    dgx_ui_init(&ui, nodes, 8, 2);
    const int root = dgx_ui_add(&ui, -1, &pick_root_w);
    const int left = dgx_ui_add(&ui, root, &pick_col_w), right = dgx_ui_add(&ui, root, &pick_col_w);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, left, &mode_l_w), "Mode");
    const int mode = dgx_ui_add(&ui, left, &mode_w);
    dgx_ui_text_set(&ui, mode, "Auto\nHeat\nCool\nOff");
    dgx_ui_set_value(&ui, mode, 1);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, right, &fan_l_w), "Fan");
    const int fan = dgx_ui_add(&ui, right, &fan_w);
    dgx_ui_text_set(&ui, fan, "Low\nMedium\nHigh");
    dgx_ui_paint_t paint = {.scr = scr, .colors = four, .color_count = 4, .background = 0};
    dgx_ui_paint(&ui, &paint, 0);
    shot(scr, out_list);
    /* the dropdown clicked: its list under it, the pick moved by a key to the second option */
    const bool opened = dgx_ui_dropdown_open(&ui, fan, &fan_list, NULL, NULL);
    dgx_ui_key(&ui, DGX_UI_KEY_NEXT);
    dgx_ui_paint(&ui, &paint, 0);
    shot(scr, out_dropdown);
    const dgx_ui_node_t *fn = dgx_ui_node(&ui, fan);
    printf("pick: the dropdown at %d,%d %dx%d; its list opened %d at %d,%d %dx%d\n", fn->x, fn->y, fn->w, fn->h, opened, fan_list.tree.root_rect.x,
           fan_list.tree.root_rect.y, fan_list.tree.root_rect.w, fan_list.tree.root_rect.h);
    dgx_ui_popup_close(&ui);
    dgx_ui_paint_free(&paint);
}

/* ---------------------------------------------------------------- scene 5: a picture under the widgets */
static dgx_screen_t *map_screen; /* the picture: hills and a sun, drawn here with DGX */
static void map_draw(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    dgx_vscreen_region_to_screen(paint->scr, x - paint->ox, y - paint->oy, ctx, x, y, w, h, 0);
}
static char             wind_buf[16], rain_buf[16], temp_buf[12], more_buf[8], place_buf[20];
static dgx_ui_textbox_t wind_box, rain_box, temp_box, more_box, place_box;
static const dgx_ui_widget_t wx_root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 8, .gap = 4};
static const dgx_ui_widget_t place_w    = {.kind = DGX_UI_LABEL, .text = &place_box};
static const dgx_ui_widget_t wind_w     = {.kind = DGX_UI_LABEL, .text = &wind_box};
static const dgx_ui_widget_t rain_w     = {.kind = DGX_UI_LABEL, .text = &rain_box};
static const dgx_ui_widget_t wx_space_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_REST};
static const dgx_ui_widget_t wx_row_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 8, .want_h = 22};
static const dgx_ui_widget_t temp_fr_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .want_w = 60, .want_h = 22};
static const dgx_ui_widget_t temp_w     = {.kind = DGX_UI_LABEL, .text = &temp_box, .want_h = 18};
static const dgx_ui_widget_t wx_gap_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_REST};
static const dgx_ui_widget_t more_w     = {.kind = DGX_UI_BUTTON, .text = &more_box, .want_w = 40, .want_h = 18, .at_y = 2, .look = &dgx_ui_look_button};

static void scene_background(dgx_screen_t *scr, const char *out)
{
    static dgx_ui_node_t nodes[12];
    static dgx_ui_t      ui;
    /* the picture: a sky in bands, a sun, two hills */
    map_screen = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
    for (int y = 0; y < 240; ++y) dgx_fill_rectangle(map_screen, 0, y, 320, 1, dgx_screen_rgb(map_screen, (uint8_t)(20 + y / 6), (uint8_t)(40 + y / 3), (uint8_t)(90 + y / 4)));
    dgx_fill_circle(map_screen, 250, 60, 26, dgx_screen_rgb(map_screen, 250, 210, 90));
    dgx_fill_circle(map_screen, 90, 330, 170, dgx_screen_rgb(map_screen, 30, 90, 60));
    dgx_fill_circle(map_screen, 270, 360, 180, dgx_screen_rgb(map_screen, 24, 70, 50));
    dgx_ui_textbox_init(&place_box, place_buf, sizeof place_buf, 1, &st_left);
    dgx_ui_textbox_init(&wind_box, wind_buf, sizeof wind_buf, 1, &st_left);
    dgx_ui_textbox_init(&rain_box, rain_buf, sizeof rain_buf, 1, &st_left);
    dgx_ui_textbox_init(&temp_box, temp_buf, sizeof temp_buf, 1, &st_big);
    dgx_ui_textbox_init(&more_box, more_buf, sizeof more_buf, 1, &st_centre);
    dgx_ui_init(&ui, nodes, 12, 2);
    const int root = dgx_ui_add(&ui, -1, &wx_root_w);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, root, &place_w), "Tallinn");
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, root, &wind_w), "wind 6 m/s");
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, root, &rain_w), "rain 20 %");
    dgx_ui_add(&ui, root, &wx_space_w);
    const int row = dgx_ui_add(&ui, root, &wx_row_w), fr = dgx_ui_add(&ui, row, &temp_fr_w);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, fr, &temp_w), "+4 C");
    dgx_ui_add(&ui, row, &wx_gap_w);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, row, &more_w), "More");
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = COLORS, .background = PAPER};
    dgx_ui_background_set(&paint, map_draw, map_screen);
    dgx_ui_paint(&ui, &paint, 0);
    shot(scr, out);
    dgx_ui_paint_free(&paint);
}

int main(int argc, char **argv)
{
    const char   *dir = argc > 1 ? argv[1] : ".";
    char          a[512], b[512];
    dgx_screen_t *scr = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
    make_colors(scr), make_styles();
    snprintf(a, sizeof a, "%s/first.ppm", dir), scene_first(scr, a);
    dgx_fill_rectangle(scr, 0, 0, 320, 240, colors[PAPER]);
    snprintf(a, sizeof a, "%s/settings.ppm", dir), snprintf(b, sizeof b, "%s/popup.ppm", dir), scene_settings(scr, a, b);
    dgx_fill_rectangle(scr, 0, 0, 320, 240, colors[PAPER]);
    snprintf(a, sizeof a, "%s/lines.ppm", dir), scene_lines(scr, a);
    dgx_fill_rectangle(scr, 0, 0, 320, 240, colors[PAPER]);
    snprintf(a, sizeof a, "%s/list.ppm", dir), snprintf(b, sizeof b, "%s/dropdown.ppm", dir), scene_pick(scr, a, b);
    dgx_fill_rectangle(scr, 0, 0, 320, 240, colors[PAPER]);
    snprintf(a, sizeof a, "%s/background.ppm", dir), scene_background(scr, a);
    printf("sizes on this machine: widget %zu, node %zu, msg %zu, saved %d\n", sizeof(dgx_ui_widget_t), sizeof(dgx_ui_node_t), sizeof(dgx_ui_msg_t), saved);
    return 0;
}
