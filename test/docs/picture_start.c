/* The picture of the getting-started page, docs/images/getting-started.png: the page's program as it stands, drawn by
   dgx-ui on a virtual screen of DGX, its sensor's message given here. The same widgets, looks, colours and code; the
   fonts made by DGX of start.needs as the page's main/CMakeLists.txt asks for them, the number in a font of its digits
   alone (run.mk). */
#include <stdio.h>

#include "dgx_colors.h"
#include "dgx_fonts.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"

static void shot(dgx_screen_t *scr, const char *name)
{
    static uint32_t px[240 * 240];
    dgx_vscreen_to_argb8888(scr, 0, 0, scr->width, scr->height, NULL, px, scr->width);
    FILE *f = fopen(name, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", scr->width, scr->height);
    for (int i = 0; i < scr->width * scr->height; ++i) fputc(px[i] >> 16 & 255, f), fputc(px[i] >> 8 & 255, f), fputc(px[i] & 255, f);
    fclose(f);
}

/* ---------------------------------------------------------------- the getting-started program */
enum { MSG_AIR = 1 };
typedef struct {
    uint16_t ppm;
    int16_t  t_dC;
    uint16_t rh_d;
} air_t;
DGX_UI_DECODER(air, MSG_AIR, air_t)
enum { AIR_PAPER, AIR_INK, RING_OFF, RING_LOW, RING_MID, RING_HIGH, AIR_COLORS };

static int32_t                  ring_ppm;
static char                     ppm_buf[8], temp_buf[12], rh_buf[8];
static dgx_ui_textbox_t         ppm_box, temp_box, rh_box, unit_box;
static dgx_ui_font_variant_t    vbig[2], vsmall[1];
static dgx_ui_text_style_t      st_big, st_centre, st_left, st_right;
static const dgx_ui_band_t      ring_bands[] = {{800, RING_LOW}, {1200, RING_MID}};
static const dgx_ui_ring_look_t ring_params  = {
    .on = RING_HIGH, .off = RING_OFF, .paper = DGX_UI_PAPER, .width = 14, .start = -135, .sweep = 270,
    .band_count = 2, .bands_by = DGX_UI_BANDS_BY_PLACE, .bands = ring_bands};
static const dgx_ui_look_t ring_look = {.draw = dgx_ui_draw_ring, .params = &ring_params, .fill = -1};

static bool ring_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    air_t a;
    if (msg_decode_air(msg, &a) == ESP_OK) ring_ppm = a.ppm;
    return false;
}
static bool ppm_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    air_t a;
    if (msg_decode_air(msg, &a) == ESP_OK) dgx_ui_text_printf(ui, i, "%d", a.ppm);
    return false;
}
static bool temp_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    air_t a;
    if (msg_decode_air(msg, &a) == ESP_OK) dgx_ui_text_printf(ui, i, "%.1f C", a.t_dC / 10.0f);
    return false;
}
static bool rh_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    air_t a;
    if (msg_decode_air(msg, &a) == ESP_OK) dgx_ui_text_printf(ui, i, "%.0f %%", a.rh_d / 10.0f);
    return false;
}
static const dgx_ui_widget_t ring_w     = {.kind = DGX_UI_OWN, .flags = DGX_UI_COLUMN, .pad = 46, .gap = 2,
                                           .min = 400, .max = 2000, .value = &ring_ppm, .look = &ring_look, .take = ring_take};
static const dgx_ui_widget_t ppm_w      = {.kind = DGX_UI_LABEL, .text = &ppm_box, .want_h = 44, .take = ppm_take};
static const dgx_ui_widget_t unit_w     = {.kind = DGX_UI_LABEL, .text = &unit_box};
static const dgx_ui_widget_t rest_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_REST};
static const dgx_ui_widget_t row_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 13};
static const dgx_ui_widget_t temp_w     = {.kind = DGX_UI_LABEL, .flags = DGX_UI_REST, .text = &temp_box, .take = temp_take};
static const dgx_ui_widget_t rh_w       = {.kind = DGX_UI_LABEL, .text = &rh_box, .want_w = 50, .take = rh_take};

int main(int argc, char **argv)
{
    char out[512];
    snprintf(out, sizeof out, "%s/getting-started.ppm", argc > 1 ? argv[1] : ".");
    static dgx_ui_node_t nodes[8];
    static dgx_ui_t      ui;
    dgx_screen_t        *scr = dgx_vscreen_init(240, 240, 16, DgxScreenRGB);
    const uint32_t       table[AIR_COLORS] = {
        DGX_BLACK(DGX_RGB_16), DGX_WHITE(DGX_RGB_16), DGX_GRAY(DGX_RGB_16), DGX_LIME(DGX_RGB_16), DGX_YELLOW(DGX_RGB_16), DGX_RED(DGX_RGB_16),
    };
    vbig[0]    = (dgx_ui_font_variant_t){.font = &dgx_font_large};
    vbig[1]    = (dgx_ui_font_variant_t){.font = &dgx_font_body};
    vsmall[0]  = (dgx_ui_font_variant_t){.font = &dgx_font_body};
    st_big    = (dgx_ui_text_style_t){.fonts = vbig, .count = 2, .align = DGX_FONT_ALIGN_CENTER, .flags = DGX_UI_TEXT_MIDDLE};
    st_centre = (dgx_ui_text_style_t){.fonts = vsmall, .count = 1, .align = DGX_FONT_ALIGN_CENTER};
    st_left   = (dgx_ui_text_style_t){.fonts = vsmall, .count = 1, .align = DGX_FONT_ALIGN_LEFT};
    st_right  = (dgx_ui_text_style_t){.fonts = vsmall, .count = 1, .align = DGX_FONT_ALIGN_RIGHT};
    unit_box      = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST("ppm CO2", 1, &st_centre);
    dgx_ui_textbox_init(&ppm_box, ppm_buf, sizeof ppm_buf, 1, &st_big);
    dgx_ui_textbox_init(&temp_box, temp_buf, sizeof temp_buf, 1, &st_left);
    dgx_ui_textbox_init(&rh_box, rh_buf, sizeof rh_buf, 1, &st_right);
    dgx_ui_init(&ui, nodes, 8, 1);
    const int ring = dgx_ui_add(&ui, -1, &ring_w);
    dgx_ui_add(&ui, ring, &ppm_w);
    dgx_ui_add(&ui, ring, &unit_w);
    dgx_ui_add(&ui, ring, &rest_w);
    const int row = dgx_ui_add(&ui, ring, &row_w);
    dgx_ui_add(&ui, row, &temp_w);
    dgx_ui_add(&ui, row, &rh_w);
    /* the message the sensor's task would send */
    dgx_ui_msg_t msg;
    const air_t  a = {.ppm = 1340, .t_dC = 215, .rh_d = 410};
    dgx_ui_msg_value(&msg, MSG_AIR, &a, sizeof a), dgx_ui_take(&ui, &msg), dgx_ui_msg_done(&msg);
    dgx_ui_paint_t paint = {.scr = scr, .colors = table, .color_count = AIR_COLORS, .background = AIR_PAPER};
    dgx_ui_paint(&ui, &paint, 0);
    shot(scr, out);
    dgx_ui_paint_free(&paint);
    dgx_screen_destroy(&scr);
    return 0;
}
