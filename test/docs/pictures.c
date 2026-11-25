/* The pictures of the documentation, drawn by dgx-ui itself on a virtual screen of DGX; the code of the pages is here. Their
   letters are the fonts of the cookbook's needs, soft-edged, made by DGX from test/docs/fonts.needs (run.mk); the
   picture of the getting-started page is picture_start.c's. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_fonts.h"
#include "dgx_vscreen.h"

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
/* the fonts of the needs, 4 bits a pixel */
static void make_styles(void)
{
    big[0]   = (dgx_ui_font_variant_t){.font = &dgx_font_large};
    big[1]   = (dgx_ui_font_variant_t){.font = &dgx_font_body};
    small[0] = (dgx_ui_font_variant_t){.font = &dgx_font_body};
    st_big       = (dgx_ui_text_style_t){.fonts = big, .count = 2, .align = DGX_FONT_ALIGN_CENTER, .flags = DGX_UI_STYLE_MIDDLE};
    st_left      = (dgx_ui_text_style_t){.fonts = small, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    st_right     = (dgx_ui_text_style_t){.fonts = small, .count = 1, .align = DGX_FONT_ALIGN_RIGHT};
    st_centre    = (dgx_ui_text_style_t){.fonts = small, .count = 1, .align = DGX_FONT_ALIGN_CENTER};
    st_important = (dgx_ui_text_style_t){.fonts = small, .count = 1, .flags = DGX_UI_STYLE_IMPORTANT, .scroll_ms = 2000};
    st_plain     = (dgx_ui_text_style_t){.fonts = small, .count = 1};
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
    .face = ACCENT, .pressed = ACCENT_DOWN, .focused = INK, .disabled = FILL, .ink = INK, .ink_disabled = DIM, .shadow = SHADOW, .depth = 1};
static const dgx_ui_slider_look_t my_slider = {.track = FILL, .fill = ACCENT, .knob = INK, .focused = INK, .disabled = DIM, .track_h = 2};
static const dgx_ui_switch_look_t my_switch = {.track = FILL, .fill = ACCENT, .knob = INK, .focused = INK, .disabled = DIM};
static const dgx_ui_look_t look_button = {.draw = dgx_ui_draw_button, .params = &my_button, .fill = -1};
static const dgx_ui_look_t look_slider = {.draw = dgx_ui_draw_slider, .params = &my_slider, .fill = -1};
static const dgx_ui_look_t look_switch = {.draw = dgx_ui_draw_switch, .params = &my_switch, .fill = -1};

static char title_buf[16], light_buf[16], wifi_buf[16], sound_buf[16], ok_buf[8], cancel_buf[8];
static dgx_ui_textbox_t title_box, light_box, wifi_box, sound_box, ok_box, cancel_box;
static int  saved;
static void on_ok(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_ACTIVATED) saved = 1;
}
enum { TAG_LIGHT = 1, TAG_WIFI, TAG_SOUND, TAG_SIGNAL, TAG_OK, TAG_CANCEL };
static const dgx_ui_widget_t set_root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 6, .gap = 5};
static const dgx_ui_widget_t title_w     = {.kind = DGX_UI_LABEL, .text = &title_box, .want_h = 18};
static const dgx_ui_widget_t line_w      = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 12, .gap = 4};
static const dgx_ui_widget_t light_lbl_w = {.kind = DGX_UI_LABEL, .text = &light_box, .want_w = 50, .at_y = 2};
static int32_t               light_value = 70, wifi_value = 1, signal_value = 3, sound_value; /* the places of the controls' values */
static const dgx_ui_widget_t light_w     = {.kind = DGX_UI_SLIDER, .min = 0, .max = 100, .step = 5, .want_w = 94, .tag = TAG_LIGHT, .value = &light_value, .look = &look_slider};
static const dgx_ui_widget_t wifi_lbl_w  = {.kind = DGX_UI_LABEL, .text = &wifi_box, .want_w = 50, .at_y = 2};
static const dgx_ui_widget_t wifi_w      = {.kind = DGX_UI_SWITCH, .max = 1, .want_w = 26, .tag = TAG_WIFI, .value = &wifi_value, .look = &look_switch};
static const dgx_ui_widget_t signal_w    = {.kind = DGX_UI_ICON, .picture = signal_pic, .min = 1, .max = 4, .want_w = 12, .tag = TAG_SIGNAL, .value = &signal_value, .look = &dgx_ui_look_picture};
static const dgx_ui_widget_t sound_lbl_w = {.kind = DGX_UI_LABEL, .text = &sound_box, .want_w = 50, .at_y = 2};
static const dgx_ui_widget_t sound_w     = {.kind = DGX_UI_SWITCH, .max = 1, .want_w = 26, .tag = TAG_SOUND, .value = &sound_value, .look = &look_switch};
static const dgx_ui_widget_t buttons_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 18, .gap = 6};
static const dgx_ui_widget_t ok_w        = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_STRETCH, .text = &ok_box, .want_w = 60, .tag = TAG_OK, .look = &look_button, .on_event = on_ok};
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
static int32_t               mode_value, fan_value;
static const dgx_ui_widget_t mode_w = {.kind = DGX_UI_LIST, .text = &mode_box, .min = 0, .max = 3, .want_h = 56, .value = &mode_value, .look = &dgx_ui_look_list};
static const dgx_ui_widget_t fan_w  = {.kind = DGX_UI_DROPDOWN, .text = &fan_box, .min = 0, .max = 2, .want_h = 16, .value = &fan_value, .look = &dgx_ui_look_dropdown};

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

/* the states of the library's own looks by their default colours: a value in the accent; the focus a frame in the ink,
   or a hollow knob; a finger on the widget. A cell is a tree of one widget drawn on a screen of its own and put into the
   picture */
static void scene_states(dgx_screen_t *scr, const char *out)
{
    enum { CELL_W = 80, CELL_H = 34, NAMES_W = 74, HEADS_H = 18, KINDS = 6, STATES = 3 };
    const uint32_t           four[4] = {colors[PAPER], colors[INK], colors[FILL], colors[BORDER]};
    static const char *const names[KINDS]  = {"button", "dropdown", "switch", "slider", "radio", "checkbox"};
    static const char *const heads[STATES] = {"plain", "the focus", "a finger"};
    /* the names of the rows and the heads of the columns: labels placed over the whole picture */
    static const dgx_ui_widget_t free_w = {.kind = DGX_UI_BOX};
    static dgx_ui_node_t         name_nodes[KINDS + STATES + 1];
    static dgx_ui_textbox_t      name_box[KINDS + STATES];
    static dgx_ui_widget_t       name_w[KINDS + STATES];
    dgx_ui_t                     names_tree;
    dgx_ui_paint_t               names_paint = {.scr = scr, .colors = four, .color_count = 4, .background = 0};
    dgx_ui_init(&names_tree, name_nodes, KINDS + STATES + 1, 1);
    dgx_ui_add(&names_tree, -1, &free_w);
    const int line = small[0].font->y_advance; /* a label is as high as a line of its font */
    for (int k = 0; k < KINDS + STATES; ++k) {
        const bool head = k >= KINDS;
        name_box[k]     = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST(head ? heads[k - KINDS] : names[k], 1, &st_left);
        name_w[k]       = (dgx_ui_widget_t){.kind   = DGX_UI_LABEL,
                                            .text   = &name_box[k],
                                            .at_x   = (int16_t)(head ? NAMES_W + (k - KINDS) * (CELL_W + 2) + 6 : 4),
                                            .at_y   = (int16_t)(head ? (HEADS_H - line) / 2 : HEADS_H + k * (CELL_H + 2) + (CELL_H - line) / 2),
                                            .want_w = (int16_t)(head ? CELL_W - 6 : NAMES_W - 4)};
        dgx_ui_add(&names_tree, 0, &name_w[k]);
    }
    dgx_ui_paint(&names_tree, &names_paint, 0);
    dgx_ui_paint_free(&names_paint), dgx_ui_free(&names_tree);
    /* the cells */
    dgx_screen_t *cell = dgx_vscreen_init(CELL_W, CELL_H, 16, DgxScreenRGB);
    for (int kind = 0; kind < KINDS; ++kind)
        for (int state = 0; state < STATES; ++state) {
            static int32_t          value;
            static dgx_ui_textbox_t caption;
            static dgx_ui_widget_t  widget;
            static dgx_ui_node_t    cell_nodes[2];
            dgx_ui_t                tree;
            dgx_ui_paint_t          paint = {.scr = cell, .colors = four, .color_count = 4, .background = 0};
            switch (kind) {
            case 0:
                caption = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST("OK", 1, &st_centre);
                widget  = (dgx_ui_widget_t){.kind = DGX_UI_BUTTON, .text = &caption, .at_x = 3, .at_y = 2, .want_w = 34, .want_h = 12, .look = &dgx_ui_look_button};
                break;
            case 1:
                caption = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST("Auto\nDay", 1, &st_left), value = 0;
                widget  = (dgx_ui_widget_t){.kind = DGX_UI_DROPDOWN, .text = &caption, .max = 1, .value = &value, .at_x = 3, .at_y = 2, .want_w = 34, .want_h = 12,
                                            .look = &dgx_ui_look_dropdown};
                break;
            case 2:
                value  = 1; /* on: its track in the accent */
                widget = (dgx_ui_widget_t){.kind = DGX_UI_SWITCH, .max = 1, .value = &value, .at_x = 3, .at_y = 5, .want_w = 16, .want_h = 7, .look = &dgx_ui_look_switch};
                break;
            case 3:
                value  = 60;
                widget = (dgx_ui_widget_t){.kind = DGX_UI_SLIDER, .max = 100, .value = &value, .at_x = 3, .at_y = 5, .want_w = 34, .want_h = 7, .look = &dgx_ui_look_slider};
                break;
            case 4:
                caption = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST("Auto", 1, &st_left), value = 1; /* chosen */
                widget  = (dgx_ui_widget_t){.kind = DGX_UI_RADIO, .step = 1, .text = &caption, .value = &value, .at_x = 3, .at_y = 4, .want_w = 34, .look = &dgx_ui_look_radio};
                break;
            default:
                caption = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST("Beep", 1, &st_left), value = 1; /* ticked */
                widget  = (dgx_ui_widget_t){.kind = DGX_UI_SWITCH, .max = 1, .text = &caption, .value = &value, .at_x = 3, .at_y = 4, .want_w = 34,
                                            .look = &dgx_ui_look_checkbox};
                break;
            }
            dgx_ui_init(&tree, cell_nodes, 2, 2);
            dgx_ui_add(&tree, -1, &free_w);
            const int shown = dgx_ui_add(&tree, 0, &widget);
            dgx_fill_rectangle(cell, 0, 0, CELL_W, CELL_H, four[0]);
            dgx_ui_paint(&tree, &paint, 0);
            const dgx_ui_node_t *node = dgx_ui_node(&tree, shown);
            if (state == 1) dgx_ui_focus(&tree, shown);
            if (state == 2) /* a finger put on it: pressed, and the focus with it */
                dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(node->x + node->w * 3 / 5), .y = (int16_t)(node->y + node->h / 2), .down = true, .pressed = true, .ms = 10});
            dgx_ui_paint(&tree, &paint, 20);
            dgx_vscreen_region_to_screen(scr, NAMES_W + state * (CELL_W + 2), HEADS_H + kind * (CELL_H + 2), cell, 0, 0, CELL_W, CELL_H, 0);
            dgx_ui_paint_free(&paint), dgx_ui_free(&tree);
        }
    dgx_screen_destroy(&cell);
    shot(scr, out);
}

/* ---------------------------------------------------------------- the cookbook's frame of nine pictures */
/* the nine pieces of a frame with rounded corners, 4 bits a pixel, a hex digit a pixel: 1 the border, 2 its light
   inner line, 0 the fill. The corners are 4 by 4, the top and bottom edges 1 wide, the side edges 1 high */
static const uint8_t nine_tl_px[4 * 2] = {0x01, 0x11, 0x12, 0x22, 0x12, 0x00, 0x12, 0x00};
static const uint8_t nine_tr_px[4 * 2] = {0x11, 0x10, 0x22, 0x21, 0x00, 0x21, 0x00, 0x21};
static const uint8_t nine_bl_px[4 * 2] = {0x12, 0x00, 0x12, 0x00, 0x12, 0x22, 0x01, 0x11};
static const uint8_t nine_br_px[4 * 2] = {0x00, 0x21, 0x00, 0x21, 0x22, 0x21, 0x11, 0x10};
static const uint8_t nine_t_px[4]      = {0x10, 0x20, 0x00, 0x00};
static const uint8_t nine_b_px[4]      = {0x00, 0x00, 0x20, 0x10};
static const uint8_t nine_l_px[2]      = {0x12, 0x00};
static const uint8_t nine_r_px[2]      = {0x00, 0x21};
static const uint8_t nine_m_px[1]      = {0x00};
#define NINE_IMAGE(px, w, h) {.width = (w), .height = (h), .format = DGX_IMAGE_4BPP, .data = (px)}
static const dgx_image_t nine_images[9] = {
    NINE_IMAGE(nine_tl_px, 4, 4), NINE_IMAGE(nine_t_px, 1, 4), NINE_IMAGE(nine_tr_px, 4, 4),
    NINE_IMAGE(nine_l_px, 4, 1),  NINE_IMAGE(nine_m_px, 1, 1), NINE_IMAGE(nine_r_px, 4, 1),
    NINE_IMAGE(nine_bl_px, 4, 4), NINE_IMAGE(nine_b_px, 1, 4), NINE_IMAGE(nine_br_px, 4, 4),
};
static const uint8_t nine_map[15] = {BORDER, ACCENT}; /* the values 1 and 2 */
/* row after row: the top-left corner, the top edge, the top-right corner, the left edge, the middle, ... */
static const dgx_ui_picture_t nine_pieces[9] = {
    {.image = &nine_images[0], .map = nine_map, .scale = 1}, {.image = &nine_images[1], .map = nine_map, .scale = 1},
    {.image = &nine_images[2], .map = nine_map, .scale = 1}, {.image = &nine_images[3], .map = nine_map, .scale = 1},
    {.image = &nine_images[4], .map = nine_map, .scale = 1}, {.image = &nine_images[5], .map = nine_map, .scale = 1},
    {.image = &nine_images[6], .map = nine_map, .scale = 1}, {.image = &nine_images[7], .map = nine_map, .scale = 1},
    {.image = &nine_images[8], .map = nine_map, .scale = 1},
};
static const dgx_ui_nine_look_t nine_params = {.pieces = nine_pieces, .fill = FILL, .ink = INK};
static const dgx_ui_look_t      nine_look   = {.draw = dgx_ui_draw_nine, .params = &nine_params, .fill = FILL};
static const dgx_ui_widget_t    card_w      = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 6, .gap = 2,
                                               .want_w = 68, .want_h = 44, .look = &nine_look};

static char             card_title_buf[12], card_value_buf[12], card_name_buf[8], card_state_buf[8];
static dgx_ui_textbox_t card_title_box, card_value_box, card_name_box, card_state_box;
static const dgx_ui_widget_t cards_w      = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 8, .gap = 8};
static const dgx_ui_widget_t card_title_w = {.kind = DGX_UI_LABEL, .text = &card_title_box};
static const dgx_ui_widget_t card_value_w = {.kind = DGX_UI_LABEL, .text = &card_value_box};
static const dgx_ui_widget_t card_name_w  = {.kind = DGX_UI_LABEL, .text = &card_name_box};
static const dgx_ui_widget_t card_state_w = {.kind = DGX_UI_LABEL, .text = &card_state_box};

static void scene_nine(dgx_screen_t *scr, const char *out)
{
    static dgx_ui_node_t nodes[8];
    static dgx_ui_t      ui;
    static uint8_t       buffer[320 * 8];
    dgx_ui_textbox_init(&card_title_box, card_title_buf, sizeof card_title_buf, 1, &st_left);
    dgx_ui_textbox_init(&card_value_box, card_value_buf, sizeof card_value_buf, 1, &st_big);
    dgx_ui_textbox_init(&card_name_box, card_name_buf, sizeof card_name_buf, 1, &st_left);
    dgx_ui_textbox_init(&card_state_box, card_state_buf, sizeof card_state_buf, 1, &st_left);
    dgx_ui_init(&ui, nodes, 8, 2);
    const int root = dgx_ui_add(&ui, -1, &cards_w);
    const int one = dgx_ui_add(&ui, root, &card_w), two = dgx_ui_add(&ui, root, &card_w);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, one, &card_title_w), "Kitchen");
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, one, &card_value_w), "21.5 C");
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, two, &card_name_w), "Fan");
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, two, &card_state_w), "Auto");
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = COLORS, .buffer = buffer, .buffer_size = sizeof buffer};
    dgx_ui_paint(&ui, &paint, 0);
    shot(scr, out);
    dgx_ui_paint_free(&paint);
}

/* ---------------------------------------------------------------- the cookbook's page that scrolls */
enum { LONG_ROWS = 12 };
static int32_t          long_offset; /* the rows moved up so many pixels; 0: the first row at the top */
static char             long_buf[LONG_ROWS][20];
static dgx_ui_textbox_t long_box[LONG_ROWS];
static dgx_ui_widget_t  long_row_w[LONG_ROWS]; /* the program's RAM: a label each, filled before it is added */
static const dgx_ui_widget_t long_page_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN | DGX_UI_SCROLL | DGX_UI_REST,
                                            .pad = 4, .gap = 2, .step = 12, .value = &long_offset, .look = &dgx_ui_look_scroll};

/* the page under parent, a column the rest of whose height it takes, its rows added; its index */
static int long_page_add(dgx_ui_t *ui, int parent)
{
    const int page = dgx_ui_add(ui, parent, &long_page_w);
    for (int k = 0; k < LONG_ROWS; ++k) {
        dgx_ui_textbox_init(&long_box[k], long_buf[k], sizeof long_buf[k], 1, &st_left);
        long_row_w[k] = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &long_box[k]};
        dgx_ui_text_printf(ui, dgx_ui_add(ui, page, &long_row_w[k]), "Setting %d", k + 1);
    }
    return page;
}

static char                  long_title_buf[12];
static dgx_ui_textbox_t      long_title_box;
static const dgx_ui_widget_t long_root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 4, .gap = 2};
static const dgx_ui_widget_t long_title_w = {.kind = DGX_UI_LABEL, .text = &long_title_box};

static void scene_scroll(dgx_screen_t *scr, const char *out)
{
    static dgx_ui_node_t nodes[16];
    static dgx_ui_t      ui;
    dgx_ui_textbox_init(&long_title_box, long_title_buf, sizeof long_title_buf, 1, &st_big);
    dgx_ui_init(&ui, nodes, 16, 2);
    const int root = dgx_ui_add(&ui, -1, &long_root_w);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, root, &long_title_w), "Settings");
    long_page_add(&ui, root);
    long_offset = 52; /* scrolled part of the way down */
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = COLORS};
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
    dgx_fill_rectangle(scr, 0, 0, 320, 240, colors[PAPER]);
    snprintf(a, sizeof a, "%s/states.ppm", dir), scene_states(scr, a);
    dgx_fill_rectangle(scr, 0, 0, 320, 240, colors[PAPER]);
    snprintf(a, sizeof a, "%s/nine.ppm", dir), scene_nine(scr, a);
    dgx_fill_rectangle(scr, 0, 0, 320, 240, colors[PAPER]);
    snprintf(a, sizeof a, "%s/scroll.ppm", dir), scene_scroll(scr, a);
    printf("sizes on this machine: widget %zu, node %zu, msg %zu, saved %d\n", sizeof(dgx_ui_widget_t), sizeof(dgx_ui_node_t), sizeof(dgx_ui_msg_t), saved);
    return 0;
}
