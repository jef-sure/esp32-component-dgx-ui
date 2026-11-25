/*
 * The picture button, dgx_ui_look_picture_button, at points of 1, 2 and 3: a button of one picture, a toggle of two
 * (a SWITCH) and three radios of one place with two pictures each. Every pixel of the box against a model of its own:
 * the face over its shadow, pressed onto it; the frame of the focus a point inside the face with face between it and
 * the picture; the picture's ones in the ink exactly where the image has them, in the middle of the face and moving
 * with it; disabled, the face and the ink of disabled. The size of a control with a picture: the picture's and 5 points
 * more. A toggle's picture on and off as the switch look and the checkbox show the same place, written 0, 1, 7 and the
 * ends of int32, and as the switch look shows a place of a range of -1..0; a click on the button tells ACTIVATED, on the toggle flips it, the key on the focused toggle too; a
 * click on a radio draws the two that changed and not the third. A picture larger than the face, cut to it; one of 4
 * bits, and none, the face plain; a text beside the picture not drawn; a focus of DGX_UI_PAPER, none shown; under a
 * modal popup, no frame. The button and the toggle in a bar over a page in home's place, the finger reaching them.
 * After each pass no pixel twice, nothing outside the boxes that changed, the panel a fresh composing; on 16 bits
 * straight and 18 through the strips (PANEL_BITS, by the Makefile), with the paper and over a drawn background.
 */
#define PANEL_W 160
#define PANEL_H 150

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

static uint32_t       colors[4], table[4];
static panel_t       *panel;
static dgx_screen_t  *memory;
static uint8_t        buffer[PANEL_W * 3 * 8];
static dgx_ui_paint_t paint;
static bool           with_bands;

/* pictures of 1 bit, not symmetric, so that a picture turned or moved by a pixel is seen: an arrow, a bell off and on,
   a star outlined and filled; one of 4 bits; one larger than a face */
static const uint8_t     arrow_bits[7] = {0x10, 0x30, 0x7e, 0xfe, 0x7e, 0x30, 0x10};
static const uint8_t     bell_off_bits[6] = {0x30, 0x48, 0x48, 0x84, 0xfc, 0x30};
static const uint8_t     bell_on_bits[6] = {0x30, 0x78, 0x78, 0xfc, 0xfc, 0x30};
static const uint8_t     star_line_bits[5] = {0x20, 0xf8, 0x50, 0x88, 0x00};
static const uint8_t     star_full_bits[5] = {0x20, 0xf8, 0x70, 0xd8, 0x00};
static uint8_t           big_bits[20 * 3]; /* a pattern, made in main() */
static const uint8_t     four_bits[6 * 3] = {0x12, 0x34, 0x56};
static const dgx_image_t arrow_im = {.width = 7, .height = 7, .format = DGX_IMAGE_1BPP, .data = arrow_bits};
static const dgx_image_t bell_off_im = {.width = 6, .height = 6, .format = DGX_IMAGE_1BPP, .data = bell_off_bits};
static const dgx_image_t bell_on_im = {.width = 6, .height = 6, .format = DGX_IMAGE_1BPP, .data = bell_on_bits};
static const dgx_image_t star_line_im = {.width = 5, .height = 5, .format = DGX_IMAGE_1BPP, .data = star_line_bits};
static const dgx_image_t star_full_im = {.width = 5, .height = 5, .format = DGX_IMAGE_1BPP, .data = star_full_bits};
static const dgx_image_t big_im = {.width = 20, .height = 20, .format = DGX_IMAGE_1BPP, .data = big_bits};
static const dgx_image_t four_im = {.width = 6, .height = 3, .format = DGX_IMAGE_4BPP, .data = four_bits};
static const dgx_ui_picture_t arrow = {.image = &arrow_im}, bells[2] = {{.image = &bell_off_im}, {.image = &bell_on_im}};
static const dgx_ui_picture_t stars[2] = {{.image = &star_line_im}, {.image = &star_full_im}}, big = {.image = &big_im}, four = {.image = &four_im};

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style = {.fonts = variant, .count = 1};
static dgx_ui_textbox_t      go_box = DGX_UI_TEXT_CONST("Go", 1, &style);

static int32_t on_place, choice = 1, odd_place;
static int     activated;
static void    on_button(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    activated += what == DGX_UI_EVENT_ACTIVATED;
}

static const dgx_ui_button_look_t no_focus_params = {.face = 2, .pressed = 3, .focused = DGX_UI_PAPER, .disabled = 0, .text = 1, .text_disabled = 2, .shadow = 0, .depth = 1};
static const dgx_ui_button_look_t flat_params     = {.face = 2, .pressed = 3, .focused = 1, .disabled = 0, .text = 1, .text_disabled = 2, .shadow = 0, .depth = 0};
static const dgx_ui_look_t        no_focus_look   = {.draw = dgx_ui_draw_picture_button, .params = &no_focus_params, .fill = -1};
static const dgx_ui_look_t        flat_look       = {.draw = dgx_ui_draw_picture_button, .params = &flat_params, .fill = -1};

static const dgx_ui_widget_t column_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t row_w     = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 2, .want_h = 12};
static const dgx_ui_widget_t radios_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 2, .want_h = 10};
static const dgx_ui_widget_t button_w  = {.kind = DGX_UI_BUTTON, .picture = &arrow, .text = &go_box, .look = &dgx_ui_look_picture_button, .on_event = on_button};
static const dgx_ui_widget_t toggle_w  = {.kind = DGX_UI_SWITCH, .value = &on_place, .picture = bells, .look = &dgx_ui_look_picture_button};
static const dgx_ui_widget_t switch_w  = {.kind = DGX_UI_SWITCH, .value = &on_place, .want_w = 14, .want_h = 8, .look = &dgx_ui_look_switch};
static const dgx_ui_widget_t check_w   = {.kind = DGX_UI_SWITCH, .value = &on_place, .want_w = 10, .want_h = 10, .look = &dgx_ui_look_checkbox};
/* a toggle and a switch of a range of -1..0: on and off as the value the range gives is not 0 or is */
static const dgx_ui_widget_t odd_toggle_w = {.kind = DGX_UI_SWITCH, .value = &odd_place, .min = -1, .max = 0, .picture = bells, .look = &dgx_ui_look_picture_button};
static const dgx_ui_widget_t odd_switch_w = {.kind = DGX_UI_SWITCH, .value = &odd_place, .min = -1, .max = 0, .want_w = 14, .want_h = 8, .look = &dgx_ui_look_switch};
static const dgx_ui_widget_t radio_w[3] = {
    {.kind = DGX_UI_RADIO, .value = &choice, .step = 1, .picture = stars, .look = &dgx_ui_look_picture_button},
    {.kind = DGX_UI_RADIO, .value = &choice, .step = 2, .picture = stars, .look = &dgx_ui_look_picture_button},
    {.kind = DGX_UI_RADIO, .value = &choice, .step = 3, .picture = stars, .look = &dgx_ui_look_picture_button}};
static const dgx_ui_widget_t big_w     = {.kind = DGX_UI_BUTTON, .picture = &big, .want_w = 6, .want_h = 6, .look = &dgx_ui_look_picture_button};
static const dgx_ui_widget_t four_w    = {.kind = DGX_UI_BUTTON, .picture = &four, .want_w = 6, .want_h = 5, .look = &dgx_ui_look_picture_button};
static const dgx_ui_widget_t none_w    = {.kind = DGX_UI_BUTTON, .want_w = 6, .want_h = 5, .look = &dgx_ui_look_picture_button};
static const dgx_ui_widget_t quiet_w   = {.kind = DGX_UI_BUTTON, .picture = &arrow, .look = &no_focus_look};
static const dgx_ui_widget_t flat_w    = {.kind = DGX_UI_BUTTON, .picture = &arrow, .look = &flat_look};

static void draw_bands(dgx_ui_paint_t *p, int x, int y, int w, int h, void *ctx)
{
    for (int row = y; row < y + h; ++row) dgx_fill_rectangle(p->scr, x - p->ox, row - p->oy, w, 1, row % 6 < 3 ? colors[0] : colors[2]);
}

static void paint_anew(void)
{
    dgx_ui_paint_free(&paint);
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
    paint.strips = &dgx_ui_strips;
#endif
    if (with_bands) dgx_ui_background_set(&paint, draw_bands, NULL);
}

static bool in_rect(dgx_ui_rect_t r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* a pass of a chain: no pixel twice, nothing outside the boxes given, the panel a fresh composing */
static int pass_in(dgx_ui_t *home, uint32_t now_ms, const dgx_ui_rect_t *boxes, int count)
{
    panel_count_from_now(panel);
    dgx_ui_paint(home, &paint, now_ms);
    int outside = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            if (!panel->writes[y][x]) continue;
            bool in = false;
            for (int k = 0; k < count; ++k) in |= in_rect(boxes[k], x, y);
            outside += !in;
        }
    dgx_ui_paint_t fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = now_ms};
    if (with_bands) dgx_ui_background_set(&fresh, draw_bands, NULL);
    dgx_fill_rectangle(memory, 0, 0, PANEL_W, PANEL_H, colors[3]);
    dgx_ui_paint_area_to(home, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint_free(&fresh);
    int off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) off += panel->mem[y][x] != dgx_get_pixel(memory, x, y);
    return panel_twice(panel) + outside + off;
}
static int pass_any(dgx_ui_t *home, uint32_t now_ms)
{
    const dgx_ui_rect_t all = {0, 0, PANEL_W, PANEL_H};
    return pass_in(home, now_ms, &all, 1);
}

static dgx_ui_rect_t box_of(const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    return dgx_ui_rect(n->x, n->y, n->w, n->h);
}

static bool bit_of(const dgx_image_t *im, int col, int row)
{
    const int stride = (im->width + 7) / 8;
    return im->data[row * stride + col / 8] & (0x80 >> (col % 8));
}

/* what a pixel of a picture button's box should be: one of the table's colours, or -1 the paper (left out) */
typedef struct look_state_t {
    bool               pressed, focused, disabled;
    const dgx_image_t *image; /* the picture shown, NULL none */
    int                depth; /* of the shadow, in points */
    bool               frame; /* the look shows the focus */
} look_state_t;
static int model(const dgx_ui_node_t *n, int unit, const look_state_t *s, int x, int y)
{
    const int     d    = s->depth * unit;
    const int     face = s->disabled ? 0 : s->pressed ? 3 : 2, ink = s->disabled ? 2 : 1;
    dgx_ui_rect_t f    = s->pressed ? dgx_ui_rect(n->x + d, n->y + d, n->w - d, n->h - d) : dgx_ui_rect(n->x, n->y, n->w - d, n->h - d);
    if (!in_rect(f, x, y)) {
        if (s->pressed) return -1; /* paper above and left */
        const bool corner = (x >= n->x + n->w - d && y < n->y + d) || (x < n->x + d && y >= n->y + n->h - d);
        return corner ? -1 : 0; /* the shadow */
    }
    if (s->focused && s->frame && !s->disabled) {
        const bool ring = x < f.x + unit || x >= f.x + f.w - unit || y < f.y + unit || y >= f.y + f.h - unit;
        if (ring) return 1;
    }
    if (!s->image) return face;
    const int hx = f.x + (f.w - s->image->width) / 2, hy = f.y + (f.h - s->image->height) / 2;
    const int col = x - hx, row = y - hy;
    if (col < 0 || row < 0 || col >= s->image->width || row >= s->image->height) return face;
    return bit_of(s->image, col, row) ? ink : face;
}
static int off_model(const dgx_ui_t *ui, int i, const look_state_t *s)
{
    const dgx_ui_node_t *n   = dgx_ui_node(ui, i);
    int                  off = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) {
            const int k = model(n, ui->unit, s, x, y);
            off += k >= 0 && panel->mem[y][x] != table[k];
        }
    return off;
}

/* the pixels of a colour in a box */
static int count_of(dgx_ui_rect_t r, uint32_t c)
{
    int count = 0;
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) count += panel->mem[y][x] == c;
    return count;
}

static void click(dgx_ui_t *home, dgx_ui_rect_t r, uint32_t ms)
{
    const int x = r.x + r.w / 2, y = r.y + r.h / 2;
    dgx_ui_pointer(home, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .down = true, .pressed = true, .ms = ms});
    dgx_ui_pointer(home, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .released = true, .ms = ms + 30});
}

static uint32_t panel_color(int k)
{
    static dgx_screen_t *probe;
    if (!probe) probe = dgx_vscreen_init(1, 1, PANEL_BITS, DgxScreenRGB);
    dgx_set_pixel(probe, 0, 0, colors[k]);
    return dgx_get_pixel(probe, 0, 0);
}

static dgx_ui_node_t nodes[16], dialog_nodes[2], page_nodes[2];
static dgx_ui_t      ui, dialog, page;

static void at_point(int unit)
{
    uint32_t now = 0;
    dgx_ui_init(&ui, nodes, 16, unit);
    dgx_ui_add(&ui, -1, &column_w);
    const int line = dgx_ui_add(&ui, 0, &row_w);
    const int button = dgx_ui_add(&ui, line, &button_w), toggle = dgx_ui_add(&ui, line, &toggle_w);
    const int sw = dgx_ui_add(&ui, line, &switch_w), check = dgx_ui_add(&ui, line, &check_w);
    const int radios = dgx_ui_add(&ui, 0, &radios_w);
    int       radio[3];
    for (int k = 0; k < 3; ++k) radio[k] = dgx_ui_add(&ui, radios, &radio_w[k]);
    const int odd_toggle = dgx_ui_add(&ui, radios, &odd_toggle_w), odd_switch = dgx_ui_add(&ui, radios, &odd_switch_w);
    const int odd = dgx_ui_add(&ui, 0, &row_w);
    const int big_i = dgx_ui_add(&ui, odd, &big_w), four_i = dgx_ui_add(&ui, odd, &four_w), none_i = dgx_ui_add(&ui, odd, &none_w);
    const int quiet = dgx_ui_add(&ui, odd, &quiet_w), flat = dgx_ui_add(&ui, odd, &flat_w);
    on_place = 0, choice = 1, activated = 0;
    paint_anew();
    CHECK(pass_any(&ui, now += 10) == 0);

    /* the size: the picture's and 5 points more, across and down */
    const dgx_ui_rect_t b = box_of(&ui, button), t = box_of(&ui, toggle);
    CHECK(b.w == 7 + 5 * unit && b.h == 7 + 5 * unit && t.w == 6 + 5 * unit && t.h == 6 + 5 * unit);
    CHECK(box_of(&ui, radio[0]).w == 5 + 5 * unit && box_of(&ui, flat).h == 7 + 5 * unit);
    /* face between the frame's ring and the picture's box on all four sides, at the default depth */
    CHECK((b.w - unit - 7) / 2 >= 2 * unit && (b.w - unit - 7) - (b.w - unit - 7) / 2 >= 2 * unit);

    /* the button in each state, the box alone written; the text beside its picture not drawn */
    look_state_t s = {.image = &arrow_im, .depth = 1, .frame = true};
    CHECK(off_model(&ui, button, &s) == 0);
    dgx_ui_set_state(&ui, button, DGX_UI_PRESSED, true), s.pressed = true;
    CHECK(pass_in(&ui, now += 10, &b, 1) == 0 && off_model(&ui, button, &s) == 0);
    dgx_ui_set_state(&ui, button, DGX_UI_PRESSED, false), s.pressed = false;
    CHECK(pass_in(&ui, now += 10, &b, 1) == 0 && off_model(&ui, button, &s) == 0);
    dgx_ui_focus(&ui, button), s.focused = true;
    CHECK(pass_in(&ui, now += 10, &b, 1) == 0 && off_model(&ui, button, &s) == 0);
    dgx_ui_set_state(&ui, button, DGX_UI_PRESSED, true), s.pressed = true;
    CHECK(pass_in(&ui, now += 10, &b, 1) == 0 && off_model(&ui, button, &s) == 0);
    dgx_ui_set_state(&ui, button, DGX_UI_PRESSED, false), s.pressed = false;
    CHECK(pass_in(&ui, now += 10, &b, 1) == 0);
    dgx_ui_set_state(&ui, button, DGX_UI_DISABLED, true), s.disabled = true, s.focused = false; /* disabled, it loses the focus */
    CHECK(pass_in(&ui, now += 10, &b, 1) == 0 && off_model(&ui, button, &s) == 0);
    dgx_ui_set_state(&ui, button, DGX_UI_DISABLED, false), s.disabled = false;
    CHECK(pass_in(&ui, now += 10, &b, 1) == 0 && off_model(&ui, button, &s) == 0);

    /* a click tells ACTIVATED */
    click(&ui, b, now += 10);
    CHECK(activated == 1 && pass_any(&ui, now += 50) == 0);

    /* the toggle, the switch and the checkbox of one place: on and off together, whatever is written */
    const uint32_t on_track = table[3];
    CHECK(pass_any(&ui, now += 10) == 0);
    on_place = 1;
    CHECK(pass_any(&ui, now += 10) == 0);
    const int check_on = count_of(box_of(&ui, check), table[1]);
    on_place = 0;
    CHECK(pass_any(&ui, now += 10) == 0);
    const int check_off = count_of(box_of(&ui, check), table[1]);
    CHECK(check_on != check_off);
    const int32_t written[] = {0, 1, 7, INT32_MIN, INT32_MAX, 0};
    for (size_t k = 0; k < sizeof written / sizeof *written; ++k) {
        on_place = written[k];
        CHECK(pass_any(&ui, now += 10) == 0);
        const bool         is_on = written[k] != 0;
        const look_state_t ts    = {.image = is_on ? &bell_on_im : &bell_off_im, .depth = 1, .frame = true, .focused = ui.focus == toggle};
        const bool         switch_on = count_of(box_of(&ui, sw), on_track) > 0, check_shows_on = count_of(box_of(&ui, check), table[1]) == check_on;
        CHECK(off_model(&ui, toggle, &ts) == 0 && switch_on == is_on && check_shows_on == is_on);
    }
    /* of a range of -1..0: the picture and the switch agree on what the range makes of the value */
    const int32_t odd_written[] = {-5, -1, 0, 3};
    for (size_t k = 0; k < sizeof odd_written / sizeof *odd_written; ++k) {
        odd_place = odd_written[k];
        CHECK(pass_any(&ui, now += 10) == 0);
        const bool         is_on = odd_written[k] < 0; /* -5 and -1 give -1, 0 and 3 give 0 */
        const look_state_t os    = {.image = is_on ? &bell_on_im : &bell_off_im, .depth = 1, .frame = true};
        CHECK(off_model(&ui, odd_toggle, &os) == 0 && (count_of(box_of(&ui, odd_switch), on_track) > 0) == is_on);
    }

    /* a click flips it; the key on it focused, too */
    click(&ui, t, now += 10);
    CHECK(on_place == 1 && pass_any(&ui, now += 50) == 0);
    dgx_ui_focus(&ui, toggle);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE) && on_place == 0 && pass_any(&ui, now += 10) == 0);
    const look_state_t focused_off = {.image = &bell_off_im, .depth = 1, .frame = true, .focused = true};
    CHECK(off_model(&ui, toggle, &focused_off) == 0);
    dgx_ui_focus(&ui, -1);
    CHECK(pass_any(&ui, now += 10) == 0);

    /* radios of one place: the chosen its second picture; a click on another draws the two that changed */
    for (int k = 0; k < 3; ++k) {
        const look_state_t rs = {.image = k == 0 ? &star_full_im : &star_line_im, .depth = 1, .frame = true};
        CHECK(off_model(&ui, radio[k], &rs) == 0);
    }
    click(&ui, box_of(&ui, radio[2]), now += 10);
    const dgx_ui_rect_t changed[2] = {box_of(&ui, radio[0]), box_of(&ui, radio[2])};
    CHECK(choice == 3 && pass_in(&ui, now += 50, changed, 2) == 0);
    for (int k = 0; k < 3; ++k) {
        const look_state_t rs = {.image = k == 2 ? &star_full_im : &star_line_im, .depth = 1, .frame = true, .focused = ui.focus == radio[k]};
        CHECK(off_model(&ui, radio[k], &rs) == 0);
    }

    /* larger than the face, cut to it; of 4 bits and none, the face plain; a flat one; a focus of paper, none shown */
    const look_state_t plain = {.depth = 1, .frame = true};
    CHECK(off_model(&ui, four_i, &plain) == 0 && off_model(&ui, none_i, &plain) == 0);
    const dgx_ui_rect_t big_box = box_of(&ui, big_i);
    dgx_ui_set_state(&ui, big_i, DGX_UI_PRESSED, true);
    CHECK(pass_in(&ui, now += 10, &big_box, 1) == 0);
    const look_state_t big_pressed = {.image = &big_im, .depth = 1, .frame = true, .pressed = true};
    CHECK(off_model(&ui, big_i, &big_pressed) == 0);
    dgx_ui_set_state(&ui, big_i, DGX_UI_PRESSED, false);
    CHECK(pass_in(&ui, now += 10, &big_box, 1) == 0);
    const look_state_t flat_state = {.image = &arrow_im, .depth = 0, .frame = true};
    CHECK(off_model(&ui, flat, &flat_state) == 0);
    dgx_ui_focus(&ui, quiet);
    CHECK(pass_any(&ui, now += 10) == 0);
    const look_state_t quiet_state = {.image = &arrow_im, .depth = 1, .frame = false, .focused = true};
    CHECK(off_model(&ui, quiet, &quiet_state) == 0);

    /* under a modal popup no frame; back when it closes */
    dgx_ui_focus(&ui, button), s = (look_state_t){.image = &arrow_im, .depth = 1, .frame = true, .focused = true};
    CHECK(pass_any(&ui, now += 10) == 0 && off_model(&ui, button, &s) == 0);
    dgx_ui_init(&dialog, dialog_nodes, 2, unit);
    dgx_ui_add(&dialog, -1, &(dgx_ui_widget_t){.kind = DGX_UI_FRAME, .look = &dgx_ui_look_frame});
    CHECK(dgx_ui_popup_open(&ui, &dialog, 110, 80, 40, 30, DGX_UI_POPUP_MODAL));
    const dgx_ui_rect_t veiled[2] = {b, {110, 80, 40, 30}};
    s.focused = false;
    CHECK(pass_in(&ui, now += 10, veiled, 2) == 0 && off_model(&ui, button, &s) == 0);
    dgx_ui_popup_close(&ui);
    s.focused = true;
    CHECK(pass_in(&ui, now += 10, veiled, 2) == 0 && off_model(&ui, button, &s) == 0);
    dgx_ui_free(&dialog);

    /* in a bar over a page in home's place: the finger reaches the button and the toggle */
    dgx_ui_focus(&ui, -1);
    CHECK(dgx_ui_page_place(&ui, odd));
    dgx_ui_init(&page, page_nodes, 2, unit);
    dgx_ui_add(&page, -1, &(dgx_ui_widget_t){.kind = DGX_UI_FRAME, .look = &dgx_ui_look_frame});
    CHECK(dgx_ui_page_open(&ui, &page) && pass_any(&ui, now += 10) == 0);
    const int was_activated = activated;
    const int32_t was_on   = on_place;
    click(&ui, b, now += 10);
    click(&ui, t, now += 100);
    CHECK(activated == was_activated + 1 && on_place != was_on && pass_any(&ui, now += 50) == 0);
    dgx_ui_page_back(&ui);
    CHECK(pass_any(&ui, now += 10) == 0);
    dgx_ui_page_place(&ui, -1);
    dgx_ui_free(&page), dgx_ui_free(&ui);
}

int main(void)
{
    panel  = panel_make();
    memory = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    for (int k = 0; k < 4; ++k) table[k] = panel_color(k);
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    for (int k = 0; k < (int)sizeof big_bits; ++k) big_bits[k] = (uint8_t)((k / 3) % 2 ? 0xa5 : 0x3c);
    for (int bands = 0; bands < 2; ++bands) {
        with_bands = bands;
        for (int unit = 1; unit <= 3; ++unit) at_point(unit);
    }
    dgx_ui_paint_free(&paint);
    CHECK_DONE();
}
