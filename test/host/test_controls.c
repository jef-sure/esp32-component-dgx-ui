/*
 * The looks of the controls and pictures on the counting panel: every change of state (a button pressed and let
 * go, focused and disabled, a switch flipped, a slider moved, an icon of another value and of none) writes every
 * pixel of the widget's box once and nothing outside it; a picture of RGB565 is its own pixels; the screen
 * composed in strips equals the screen drawn whole.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

/* two icons of 8x8, 1 bit: a frame and a cross */
static const uint8_t      frame_bits[8] = {0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff};
static const uint8_t      cross_bits[8] = {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81};
static const dgx_image_t  frame_im = {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = frame_bits};
static const dgx_image_t  cross_im = {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = cross_bits};
static const dgx_ui_picture_t icons[2] = {{.image = &frame_im}, {.image = &cross_im}};
/* a picture of RGB565 of 4x3: high byte first, as the virtual screen of DGX holds it */
static uint16_t           rgb[12];
static dgx_ui_picture_t   rgb_pic = {.rgb565 = rgb, .width = 4, .height = 3};

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static char                  ok_buf[8];
static dgx_ui_textbox_t      ok_box;

static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1, .gap = 1};
static const dgx_ui_widget_t button_w = {.kind = DGX_UI_BUTTON, .text = &ok_box, .want_h = 16, .look = &dgx_ui_look_button};
static int32_t switch_w_place; /* its value */
static const dgx_ui_widget_t switch_w = {.kind = DGX_UI_SWITCH, .value = &switch_w_place, .want_h = 8, .want_w = 20, .max = 1, .look = &dgx_ui_look_switch};
static int32_t slider_w_place; /* its value */
static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .value = &slider_w_place, .want_h = 8, .max = 10, .step = 1, .look = &dgx_ui_look_slider};
static int32_t icon_w_place; /* its value */
static const dgx_ui_widget_t icon_w   = {.kind = DGX_UI_ICON, .value = &icon_w_place, .picture = icons, .min = 0, .max = 1, .want_h = 10, .look = &dgx_ui_look_picture};
static const dgx_ui_widget_t rgb_w    = {.kind = DGX_UI_PICTURE, .picture = &rgb_pic, .want_h = 5, .look = &dgx_ui_look_rgb565};

static dgx_ui_node_t nodes[8];
static dgx_ui_t      ui;
static int           bt, sw, sl, ic, pic;

static void make(void)
{
    dgx_ui_textbox_init(&ok_box, ok_buf, sizeof ok_buf, 1, &style);
    dgx_ui_init(&ui, nodes, 8, 1);
    dgx_ui_add(&ui, -1, &root_w);
    bt  = dgx_ui_add(&ui, 0, &button_w);
    sw  = dgx_ui_add(&ui, 0, &switch_w);
    sl  = dgx_ui_add(&ui, 0, &slider_w);
    ic  = dgx_ui_add(&ui, 0, &icon_w);
    pic = dgx_ui_add(&ui, 0, &rgb_w);
    dgx_ui_text_set(&ui, bt, "OK");
}

/* the change drawn: every pixel of the widget's box once, nothing else */
static bool once_in_box(panel_t *panel, dgx_ui_paint_t *pp, int i, uint32_t now)
{
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, pp, now);
    const dgx_ui_node_t *n = dgx_ui_node(&ui, i);
    return panel_twice(panel) == 0 && panel_written_outside(panel, n->x, n->y, n->x + n->w, n->y + n->h) == 0 &&
           panel_written(panel) == n->w * n->h;
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
    for (int k = 0; k < 12; ++k) rgb[k] = (uint16_t)(0x1111 * (k + 1)); /* as stored: high byte first is the value read back */
    panel_t       *panel = panel_make();
    dgx_ui_paint_t pp    = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
    make();
    dgx_ui_paint(&ui, &pp, 0);

    /* the button: pressed, let go, focused, disabled */
    dgx_ui_set_state(&ui, bt, DGX_UI_PRESSED, true);
    CHECK(once_in_box(panel, &pp, bt, 1));
    dgx_ui_set_state(&ui, bt, DGX_UI_PRESSED, false);
    CHECK(once_in_box(panel, &pp, bt, 2));
    dgx_ui_focus(&ui, bt);
    CHECK(once_in_box(panel, &pp, bt, 3));
    dgx_ui_set_state(&ui, bt, DGX_UI_DISABLED, true);
    CHECK(once_in_box(panel, &pp, bt, 4));
    /* a text changed */
    dgx_ui_text_set(&ui, bt, "Go");
    CHECK(once_in_box(panel, &pp, bt, 5));
    /* the switch flipped, the slider moved, the icon of another value and of none */
    dgx_ui_set_value(&ui, sw, 1);
    CHECK(once_in_box(panel, &pp, sw, 6));
    dgx_ui_set_value(&ui, sl, 7);
    CHECK(once_in_box(panel, &pp, sl, 7));
    dgx_ui_set_value(&ui, ic, 1);
    CHECK(once_in_box(panel, &pp, ic, 8));
    dgx_ui_set_value(&ui, ic, 5);
    CHECK(once_in_box(panel, &pp, ic, 9));
    {
        /* none: all paper */
        const dgx_ui_node_t *n = dgx_ui_node(&ui, ic);
        int                  ink = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) ink += panel->mem[y][x] != 0;
        CHECK(ink == 0);
    }
    dgx_ui_set_value(&ui, ic, 1);
    dgx_ui_paint(&ui, &pp, 10);
    /* the cross where it stands: its corners in the ink */
    {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, ic);
        const int            x0 = n->x + (n->w - 8) / 2, y0 = n->y + (n->h - 8) / 2;
        CHECK(panel->mem[y0][x0] == 0xffff && panel->mem[y0][x0 + 1] == 0 && panel->mem[y0 + 3][x0 + 3] == 0xffff);
    }
    /* the picture of RGB565: its own pixels, in the middle of its box */
    {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, pic);
        const int            x0 = n->x + (n->w - 4) / 2, y0 = n->y + (n->h - 3) / 2;
        dgx_screen_t        *src = dgx_vscreen_from_array(rgb, 4, 3, 16, DgxScreenRGB);
        int                  off = 0;
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 4; ++x) off += panel->mem[y0 + y][x0 + x] != dgx_get_pixel(src, x, y);
        CHECK(off == 0 && panel->mem[n->y][n->x] == 0);
        src->destroy(&src);
    }

    /* the screen composed in strips of 3 rows equals the screen drawn whole */
    dgx_screen_t  *v  = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4};
    dgx_ui_redraw(&ui, 0);
    dgx_ui_paint(&ui, &pv, 11);
    dgx_screen_t  *band = dgx_vscreen_init(PANEL_W, 3, 16, DgxScreenRGB);
    dgx_ui_paint_t pb   = {.scr = band, .colors = colors, .color_count = 4};
    int            off  = 0;
    for (int y0 = 0; y0 < PANEL_H; y0 += 3) {
        const int hh = PANEL_H - y0 < 3 ? PANEL_H - y0 : 3;
        dgx_fill_rectangle(band, 0, 0, PANEL_W, 3, 0x1234);
        pb.oy = (int16_t)y0;
        dgx_ui_paint_area(&ui, &pb, 0, y0, PANEL_W, hh);
        for (int y = 0; y < hh; ++y)
            for (int x = 0; x < PANEL_W; ++x) off += dgx_get_pixel(band, x, y) != dgx_get_pixel(v, x, y0 + y);
    }
    CHECK(off == 0);
    dgx_ui_paint_free(&pb), dgx_ui_paint_free(&pv); /* the screens of RGB565 they made */
    band->destroy(&band), v->destroy(&v);

    /* a button lower than its font's cell, pressed, writes every pixel once */
    {
        static const dgx_ui_widget_t low_w = {.kind = DGX_UI_BUTTON, .text = &ok_box, .want_h = 8, .look = &dgx_ui_look_button};
        static dgx_ui_node_t         ln[2];
        dgx_ui_t                     lu;
        dgx_ui_init(&lu, ln, 2, 1);
        dgx_ui_add(&lu, -1, &root_w);
        const int lb = dgx_ui_add(&lu, 0, &low_w);
        dgx_ui_paint(&lu, &pp, 20);
        dgx_ui_set_state(&lu, lb, DGX_UI_PRESSED, true);
        panel_count_from_now(panel);
        dgx_ui_paint(&lu, &pp, 21);
        const dgx_ui_node_t *n = dgx_ui_node(&lu, lb);
        CHECK(panel_twice(panel) == 0 && panel_written(panel) == n->w * n->h);
        dgx_ui_free(&lu);
    }
    /* the picture of RGB565 on a screen of 8 bits: its box is paper, not left as it was */
    {
        dgx_screen_t  *s8 = dgx_vscreen_init(PANEL_W, PANEL_H, 8, DgxScreenRGB);
        uint32_t       c8[4] = {0, 1, 2, 3};
        dgx_ui_paint_t p8 = {.scr = s8, .colors = c8, .color_count = 4};
        dgx_fill_rectangle(s8, 0, 0, PANEL_W, PANEL_H, 7);
        dgx_ui_redraw(&ui, pic); /* the picture alone: nothing else puts paper on its place */
        dgx_ui_paint(&ui, &p8, 30);
        const dgx_ui_node_t *n    = dgx_ui_node(&ui, pic);
        int                  left = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) left += dgx_get_pixel(s8, x, y) == 7;
        CHECK(left == 0);
        dgx_ui_paint_free(&p8);
        s8->destroy(&s8);
    }
    /* the screen of RGB565 kept by the paint: one, pointed at each picture, given back */
    CHECK(pp.rgb565 != NULL);
    dgx_screen_t *kept = pp.rgb565;
    dgx_ui_redraw(&ui, pic);
    dgx_ui_paint(&ui, &pp, 31);
    CHECK(pp.rgb565 == kept);
    dgx_ui_paint_free(&pp);
    CHECK(pp.rgb565 == NULL);
    dgx_ui_free(&ui);
    free(panel);
    CHECK_DONE();
}
