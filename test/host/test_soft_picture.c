/*
 * The look of soft pictures, dgx_ui_look_soft_picture, on a panel behind a bus of 16 bits (18 with -DPANEL_BITS=18, its
 * widgets drawn in the strips there):
 *  - over the paper of a colour, a mask of 4 and of 8 bits, a picture of 16 bits with its opacity and rows of RGB565
 *    opaque, one at a scale of 2: every pixel of the panel written once, each the colour DGX's rule mixes of the paper
 *    and the picture by its opacity (a value of a mask of 4 bits times 17); a picture of 4 bits that is not a mask its
 *    box all paper; an icon's value changed, its box written once again, of its new picture;
 *  - the tree composed by bands of 8 rows at an offset into a virtual screen: the panel's pixels;
 *  - over a drawn background on the panel, which cannot be read: a pixel covered half or more the picture's, the rest
 *    the background's; with the strips named, which can be read: mixed with the background, every pixel once;
 *  - on a screen of 1 bit, the paper hatched: around the picture the hatch, under it no pattern, a pixel covered half
 *    or more the ink and the rest not lit. The mask there is 16 pixels wide: DGX 0.12.11 sends a row of 1 bit by whole
 *    bytes, so a row of a width not divisible by 8 wraps in its window and its first pixels take the bits left over.
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_vscreen.h"
#include "panel.h"

enum { PW = 12, PH = 10, CELL_W = 24, CELL_H = 30 };

static panel_t       *panel;
static dgx_screen_t  *map; /* the program's picture under the tree */
static uint32_t       colors[4];
static uint8_t        buffer[PANEL_W * 3 * 8];
static dgx_ui_paint_t paint;

enum { WIDE = 16 }; /* the mask of the screen of 1 bit, of whole bytes a row */
static uint8_t     mask4_data[PH * PW / 2], mask8_data[PH * PW], soft_rgb[PH * PW * 2], soft_alpha[PH * PW], opaque_rgb[PH * PW * 2], wide_data[PH * WIDE];
static dgx_image_t wide = {.width = WIDE, .height = PH, .format = DGX_IMAGE_8BPP, .data = wide_data};
static const dgx_ui_picture_t pic_wide = {.image = &wide, .mask = 1};
static dgx_image_t mask4 = {.width = PW, .height = PH, .format = DGX_IMAGE_4BPP, .data = mask4_data};
static dgx_image_t mask8 = {.width = PW, .height = PH, .format = DGX_IMAGE_8BPP, .data = mask8_data};
static dgx_image_t soft16 = {.width = PW, .height = PH, .format = DGX_IMAGE_16BPP, .data = soft_rgb, .alpha = soft_alpha};

static const dgx_ui_picture_t pic_mask4 = {.image = &mask4, .mask = 1}, pic_mask8 = {.image = &mask8, .mask = 1}, pic_soft16 = {.image = &soft16};
static const dgx_ui_picture_t pic_rgb = {.rgb565 = (const uint16_t *)opaque_rgb, .width = PW, .height = PH};
static const dgx_ui_picture_t pic_twice = {.image = &mask4, .mask = 1, .scale = 2}, pic_not_mask = {.image = &mask4};
static const dgx_ui_picture_t icons[2] = {{.image = &mask8, .mask = 1}, {.image = &soft16}};
static int32_t                icon_value;

#define CELL(pic) {.kind = DGX_UI_PICTURE, .picture = pic, .want_w = CELL_W, .want_h = CELL_H, .look = &dgx_ui_look_soft_picture}
static const dgx_ui_widget_t root = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN}, row = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_w = PANEL_W, .want_h = CELL_H};
static const dgx_ui_widget_t cells[7] = {CELL(&pic_mask4), CELL(&pic_mask8), CELL(&pic_soft16), CELL(&pic_rgb), CELL(&pic_twice), CELL(&pic_not_mask),
                                         {.kind = DGX_UI_ICON, .picture = icons, .value = &icon_value, .min = 0, .max = 1, .want_w = CELL_W, .want_h = CELL_H,
                                          .look = &dgx_ui_look_soft_picture}};
static int cell[7];

static void draw_map(dgx_ui_paint_t *p, int x, int y, int w, int h, void *ctx)
{
    dgx_vscreen_region_to_screen(p->scr, x - p->ox, y - p->oy, (dgx_screen_t *)ctx, x, y, w, h, 0);
}

/* DGX's mix of two colours of the panel's depth by an opacity 0..255, channel by channel */
static uint32_t mix(uint32_t under, uint32_t color, unsigned a)
{
#if PANEL_BITS == 16
    static const unsigned shifts[3] = {11, 5, 0}, tops[3] = {31, 63, 31};
#else
    static const unsigned shifts[3] = {18, 10, 2}, tops[3] = {63, 63, 63};
#endif
    uint32_t out = 0;
    for (int k = 0; k < 3; ++k) {
        const uint32_t u = under >> shifts[k] & tops[k], c = color >> shifts[k] & tops[k];
        out |= (u * (255u - a) + c * a + 127u) / 255u << shifts[k];
    }
    return out;
}

/* a colour of RGB565 on a screen, as DGX spreads it to another depth */
static uint32_t of_rgb565(dgx_screen_t *scr, uint16_t v)
{
    if (scr->color_bits == 16) return v;
    return dgx_screen_rgb(scr, (uint8_t)((v >> 8) & 0xf8u), (uint8_t)((v >> 3) & 0xfcu), (uint8_t)((v << 3) & 0xf8u));
}

/* what the picture shown in widget i puts at x, y of the screen, over `under`, the paper or the background there:
   how = 0 over the paper (opaque, mixed), 1 the half rule, 2 mixed with the background */
static uint32_t expected(dgx_ui_t *ui, int i, dgx_screen_t *scr, int x, int y, uint32_t under, int how)
{
    const dgx_ui_node_t    *n   = dgx_ui_node(ui, i);
    const dgx_ui_picture_t *pic = n->widget->kind == DGX_UI_ICON ? &icons[icon_value] : n->widget->picture;
    const dgx_image_t      *im  = pic->image;
    const int               w = im ? im->width : pic->width, h = im ? im->height : pic->height, k = dgx_ui_picture_scale(ui, pic);
    const int               hx = n->x + (n->w - w * k) / 2, hy = n->y + (n->h - h * k) / 2;
    if ((im && !pic->mask && im->format != DGX_IMAGE_16BPP) || x < hx || y < hy || x >= hx + w * k || y >= hy + h * k) return under;
    const int px = (x - hx) / k, py = (y - hy) / k;
    uint32_t  c;
    unsigned  a;
    if (pic->mask) {
        c = colors[1];
        a = im->format == DGX_IMAGE_4BPP ? (unsigned)(im->data[py * PW / 2 + px / 2] >> (px & 1 ? 0 : 4) & 15) * 17u : im->data[py * PW + px];
    } else {
        const uint8_t *data = im ? im->data : (const uint8_t *)pic->rgb565;
        c                   = of_rgb565(scr, (uint16_t)(data[2 * (py * w + px)] << 8 | data[2 * (py * w + px) + 1]));
        a                   = im && im->alpha ? im->alpha[py * w + px] : 255u;
    }
    if (a == 255) return c;
    if (how == 1) return a >= 128 ? c : under;
    return a == 0 ? under : mix(under, c, a);
}

/* the cells' pixels on the panel against what they should be; over the paper colors[0], or the map's pixel */
static int off_cells(dgx_ui_t *ui, int how, bool on_map)
{
    int off = 0;
    for (int k = 0; k < 7; ++k) {
        const dgx_ui_node_t *n = dgx_ui_node(ui, cell[k]);
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) {
                const uint32_t under = on_map ? dgx_get_pixel(map, x, y) : colors[0];
                off += panel->mem[y][x] != expected(ui, cell[k], &panel->sbus.scr, x, y, under, how);
            }
    }
    return off;
}

static void paint_anew(bool on_map, bool strips)
{
    dgx_ui_paint_free(&paint);
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
    if (strips || PANEL_BITS != 16) paint.strips = &dgx_ui_strips;
    if (on_map) dgx_ui_background_set(&paint, draw_map, map);
}

static int whole_pass(dgx_ui_t *ui)
{
    panel_count_from_now(panel);
    dgx_ui_redraw(ui, 0);
    dgx_ui_paint(ui, &paint, 0);
    return panel_twice(panel);
}

int main(void)
{
    for (int y = 0; y < PH; ++y)
        for (int x = 0; x < PW; ++x) {
            const int p = y * PW + x;
            if (!(x & 1)) mask4_data[p / 2] = (uint8_t)(((x + 3 * y) % 16) << 4 | ((x + 1 + 3 * y) % 16));
            mask8_data[p] = (uint8_t)((x * 23 + y * 41) % 256);
            wide_data[y * WIDE + x] = mask8_data[p], wide_data[y * WIDE + x + WIDE - PW] = (uint8_t)(255 - mask8_data[p]);
            soft_alpha[p] = (uint8_t)(x == 0 ? 0 : x == PW - 1 ? 255 : (x * 31 + y * 17) % 256);
            const uint16_t s = (uint16_t)((x * 2) << 11 | (y * 6) << 5 | (31 - x)), o = (uint16_t)((31 - y * 3) << 11 | (x * 5) << 5 | (y * 3));
            soft_rgb[2 * p] = (uint8_t)(s >> 8), soft_rgb[2 * p + 1] = (uint8_t)s;
            opaque_rgb[2 * p] = (uint8_t)(o >> 8), opaque_rgb[2 * p + 1] = (uint8_t)o;
        }
    panel             = panel_make();
    dgx_screen_t *scr = &panel->sbus.scr;
    colors[0] = dgx_screen_rgb(scr, 20, 30, 60), colors[1] = dgx_screen_rgb(scr, 250, 220, 120), colors[2] = dgx_screen_rgb(scr, 60, 60, 60),
    colors[3] = dgx_screen_rgb(scr, 200, 0, 0);
    map = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) dgx_set_pixel(map, x, y, dgx_screen_rgb(map, (uint8_t)(x * 2), (uint8_t)(y * 4), (uint8_t)((x ^ y) * 3)));

    static dgx_ui_node_t nodes[12];
    dgx_ui_t             ui;
    dgx_ui_init(&ui, nodes, 12, 1);
    dgx_ui_add(&ui, -1, &root);
    for (int r = 0, k = 0; r < 2; ++r) {
        const int at = dgx_ui_add(&ui, 0, &row);
        for (int c = 0; c < 4 && k < 7; ++c, ++k) cell[k] = dgx_ui_add(&ui, at, &cells[k]);
    }

    /* over the paper: every pixel once, each mixed by DGX's rule; a picture of 4 bits that is not a mask, paper */
    paint_anew(false, false);
    CHECK(whole_pass(&ui) == 0);
    int off = off_cells(&ui, 0, false);
    CHECK(off == 0);
    if (off) fprintf(stderr, "  over the paper: %d pixels off\n", off);
    int unwritten = 0; /* every pixel of every cell written */
    for (int k = 0; k < 7; ++k) {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, cell[k]);
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) unwritten += panel->writes[y][x] != 1;
    }
    CHECK(unwritten == 0);
    /* an icon's value changed: its box written once, of its new picture */
    icon_value = 1;
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &paint, 10);
    const dgx_ui_node_t *icon = dgx_ui_node(&ui, cell[6]);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == CELL_W * CELL_H && panel_written_outside(panel, icon->x, icon->y, icon->x + icon->w, icon->y + icon->h) == 0);
    CHECK(off_cells(&ui, 0, false) == 0);

    /* composed by bands of 8 rows at an offset: the panel's pixels */
    {
        static uint8_t rows[PANEL_W * 8 * 3];
        dgx_screen_t  *band  = dgx_vscreen_from_array(rows, PANEL_W, 8, PANEL_BITS, DgxScreenRGB);
        dgx_ui_paint_t fresh = {.scr = band, .colors = colors, .color_count = 4};
        int            moved = 0;
        for (int y0 = 0; y0 < 2 * CELL_H; y0 += 8) {
            dgx_ui_paint_area_to(&ui, &fresh, band, NULL, 0, y0, PANEL_W, 8);
            for (int y = y0; y < y0 + 8 && y < 2 * CELL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x) moved += dgx_get_pixel(band, x, y - y0) != panel->mem[y][x];
        }
        CHECK(moved == 0);
        if (moved) fprintf(stderr, "  composed at an offset: %d pixels off\n", moved);
        dgx_ui_paint_free(&fresh);
        dgx_screen_destroy(&band);
    }

    /* over the drawn background: the panel cannot be read, the half rule (on 18 bits the strips: mixed); with the
       strips named, mixed and every pixel once */
    for (int strips = 0; strips < 2; ++strips) {
        paint_anew(true, strips);
        const int twice = whole_pass(&ui);
        const bool read = strips || PANEL_BITS != 16;
        off             = off_cells(&ui, read ? 2 : 1, true);
        CHECK(off == 0 && (!read || twice == 0));
        if (off || (read && twice)) fprintf(stderr, "  over the background, strips %d: %d pixels off, %d twice\n", strips, off, twice);
    }

    /* 1 bit, the paper hatched: the hatch around the picture, none under it; covered half or more the ink */
    {
        dgx_screen_t                *one       = dgx_vscreen_init(PANEL_W, PANEL_H, 1, DgxScreenRGB);
        static const uint32_t        bw[3]     = {0, 1, DGX_UI_HATCH};
        static const dgx_ui_picture_look_t hatched = {.ink = 1, .paper = 2};
        static const dgx_ui_look_t   look      = {.draw = dgx_ui_draw_soft_picture, .params = &hatched, .fill = DGX_UI_PAPER};
        static const dgx_ui_widget_t one_w     = {.kind = DGX_UI_PICTURE, .picture = &pic_wide, .want_w = CELL_W, .want_h = CELL_H, .look = &look};
        static dgx_ui_node_t         one_nodes[2];
        dgx_ui_t                     tree;
        dgx_ui_init(&tree, one_nodes, 2, 1);
        dgx_ui_add(&tree, -1, &root);
        const int      i  = dgx_ui_add(&tree, 0, &one_w);
        dgx_ui_paint_t on = {.scr = one, .colors = bw, .color_count = 3};
        dgx_ui_paint(&tree, &on, 0);
        const dgx_ui_node_t *n = dgx_ui_node(&tree, i);
        const int            hx = n->x + (n->w - WIDE) / 2, hy = n->y + (n->h - PH) / 2;
        int                  wrong = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) {
                const bool     inside = x >= hx && x < hx + WIDE && y >= hy && y < hy + PH;
                const uint32_t want   = inside ? wide_data[(y - hy) * WIDE + x - hx] >= 128 : (uint32_t)((x + y) & 1);
                wrong += dgx_get_pixel(one, x, y) != want;
            }
        CHECK(wrong == 0);
        if (wrong) fprintf(stderr, "  1 bit over the hatch: %d pixels off\n", wrong);
        dgx_ui_paint_free(&on), dgx_ui_free(&tree);
        dgx_screen_destroy(&one);
    }

    dgx_ui_paint_free(&paint);
    dgx_ui_free(&ui);
    dgx_screen_destroy(&map);
    free(panel);
    CHECK_DONE();
}
