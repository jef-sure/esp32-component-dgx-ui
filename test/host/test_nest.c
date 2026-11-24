/*
 * Popups one over another: a dialog over the main tree, a list opened from it hanging out below it, a question over
 * the list. Each tree drawn around every popup over it: a change under the list's overhang writes nothing there, in
 * the main tree and in the dialog; drawn whole, every pixel once. A finger down on the overhang is the list's; a
 * finger outside the list closes it alone, and one on the main tree closes what the flags say; closing the dialog
 * closes all over it, and the screen is as the main tree drawn alone. Keys and letters to the top, messages to every
 * level, a slider of the third held and dragged as the main tree tells. No more than DGX_UI_POPUP_DEPTH.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

/* who was activated, who caught a message: the level in ctx */
static int  activated[8], activated_count, caught[4];
static void on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED && activated_count < 8) activated[activated_count++] = (int)(intptr_t)ctx;
}
static bool take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    caught[(int)(intptr_t)dgx_ui_widget(ui, i)->ctx] = msg->what;
    return false; /* the next level gets it too */
}

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static char                  low_buf[16], dlg_buf[16], field_buf[8];
static dgx_ui_textbox_t      low_box, dlg_box, field_box;

/* the main tree: a button at the top, a label low, under where the list hangs */
static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t main_bt = {.kind = DGX_UI_BUTTON, .want_h = 30, .look = &dgx_ui_look_button, .on = on, .ctx = (void *)0, .take = take};
static const dgx_ui_widget_t low_w   = {.kind = DGX_UI_LABEL, .text = &low_box, .want_h = 20};
/* the dialog at (8, 4, 60, 30): a button, a label under the list */
static const dgx_ui_rect_t   dlg_r   = {8, 4, 60, 30};
static const dgx_ui_widget_t dlg_w   = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 1};
static const dgx_ui_widget_t dlg_bt  = {.kind = DGX_UI_BUTTON, .want_h = 10, .look = &dgx_ui_look_button, .on = on, .ctx = (void *)1, .take = take};
static const dgx_ui_widget_t dlg_lb  = {.kind = DGX_UI_LABEL, .text = &dlg_box, .want_h = 14};
/* the list at (30, 20, 40, 36): over the dialog's bottom right, hanging out below it to y 56 */
static const dgx_ui_rect_t   list_r  = {30, 20, 40, 36};
static const dgx_ui_widget_t list_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2};
static const dgx_ui_widget_t list_bt = {.kind = DGX_UI_BUTTON, .want_h = 30, .look = &dgx_ui_look_button, .on = on, .ctx = (void *)2, .take = take};
/* the question at (40, 30, 50, 30): a slider and a field */
static const dgx_ui_rect_t   ask_r   = {40, 30, 50, 30};
static const dgx_ui_widget_t ask_w   = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 1};
static int32_t ask_sl_place; /* its value */
static const dgx_ui_widget_t ask_sl  = {.kind = DGX_UI_SLIDER, .value = &ask_sl_place, .want_h = 10, .max = 10, .look = &dgx_ui_look_slider, .ctx = (void *)3, .take = take};
static const dgx_ui_widget_t ask_in  = {.kind = DGX_UI_INPUT, .text = &field_box, .want_h = 12, .look = &dgx_ui_look_input};

static dgx_ui_node_t main_n[4], dlg_n[4], list_n[3], ask_n[4];
static dgx_ui_t      ui, dlg, list, ask;
static int           mbt, low, dbt, dlb, lbt, asl, ain;

static void make(void)
{
    dgx_ui_textbox_init(&low_box, low_buf, sizeof low_buf, 1, &style);
    dgx_ui_textbox_init(&dlg_box, dlg_buf, sizeof dlg_buf, 1, &style);
    dgx_ui_textbox_init(&field_box, field_buf, sizeof field_buf, 1, &style);
    dgx_ui_init(&ui, main_n, 4, 1), dgx_ui_add(&ui, -1, &root_w);
    mbt = dgx_ui_add(&ui, 0, &main_bt), low = dgx_ui_add(&ui, 0, &low_w);
    dgx_ui_text_set(&ui, low, "low");
    dgx_ui_init(&dlg, dlg_n, 4, 1), dgx_ui_add(&dlg, -1, &dlg_w);
    dbt = dgx_ui_add(&dlg, 0, &dlg_bt), dlb = dgx_ui_add(&dlg, 0, &dlg_lb);
    dgx_ui_text_set(&dlg, dlb, "dialog");
    dgx_ui_init(&list, list_n, 3, 1), dgx_ui_add(&list, -1, &list_w);
    lbt = dgx_ui_add(&list, 0, &list_bt);
    dgx_ui_init(&ask, ask_n, 4, 1), dgx_ui_add(&ask, -1, &ask_w);
    asl = dgx_ui_add(&ask, 0, &ask_sl), ain = dgx_ui_add(&ask, 0, &ask_in);
    activated_count = 0;
    memset(caught, 0, sizeof caught);
}

static bool open_at(dgx_ui_t *under, dgx_ui_t *p, dgx_ui_rect_t r, unsigned flags)
{
    return dgx_ui_popup_open(under, p, r.x, r.y, r.w, r.h, flags);
}

static int written_within(const panel_t *p, dgx_ui_rect_t r)
{
    int n = 0;
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) n += p->writes[y][x] != 0;
    return n;
}

static int differ(const panel_t *p, dgx_screen_t *v)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->mem[y][x] != dgx_get_pixel(v, x, y);
    return n;
}

static void tap(int x, int y)
{
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .down = true, .pressed = true});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .released = true});
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
    panel_t       *panel = panel_make();
    dgx_ui_paint_t pp    = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
    dgx_screen_t  *v     = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_ui_paint_t pv    = {.scr = v, .colors = colors, .color_count = 4};

    /* the dialog, the list from it: drawn whole, every pixel once */
    make();
    CHECK(open_at(&ui, &dlg, dlg_r, DGX_UI_POPUP_MODAL) && open_at(&dlg, &list, list_r, DGX_UI_POPUP_CLOSE_OUTSIDE));
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 0);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H);
    /* a change in the main tree under the list's overhang, and in the dialog under the list: nothing written there */
    const dgx_ui_node_t *ln = dgx_ui_node(&ui, low), *dn = dgx_ui_node(&dlg, dlb);
    CHECK(ln->y < list_r.y + list_r.h && ln->y + ln->h > dlg_r.y + dlg_r.h); /* the label lies under the overhang */
    CHECK(dn->x + dn->w > list_r.x && dn->y + dn->h > list_r.y);             /* the dialog's label under the list */
    panel_count_from_now(panel);
    dgx_ui_text_set(&ui, low, "LOW LOW LOW");
    dgx_ui_text_set(&dlg, dlb, "DIALOG");
    dgx_ui_paint(&ui, &pp, 1);
    CHECK(panel_written(panel) > 0 && panel_twice(panel) == 0 && written_within(panel, list_r) == 0);

    /* the list, open over the dialog, is not taken by another tree: it stays the dialog's */
    CHECK(!open_at(&ask, &list, list_r, 0) && ask.popup == NULL && dlg.popup == &list && list.level == 2);
    /* a finger down on the overhang, outside the dialog: the list's, the dialog left open */
    tap(list_r.x + 5, dlg_r.y + dlg_r.h + 6);
    CHECK(activated_count == 1 && activated[0] == 2 && ui.popup == &dlg && dlg.popup == &list);
    /* a finger on the dialog, outside the list: the list closes, the finger spent, the dialog stays */
    tap(dlg_r.x + 4, dlg_r.y + 4);
    CHECK(dlg.popup == NULL && ui.popup == &dlg && activated_count == 1);
    /* the list again; a finger on the main tree: the list closes by its flag, the modal dialog swallows the rest */
    CHECK(open_at(&dlg, &list, list_r, DGX_UI_POPUP_CLOSE_OUTSIDE));
    tap(2, 60);
    CHECK(dlg.popup == NULL && ui.popup == &dlg && activated_count == 1);
    /* a dialog closed by a finger outside it closes the list over it too */
    dgx_ui_popup_close(&ui);
    CHECK(open_at(&ui, &dlg, dlg_r, DGX_UI_POPUP_CLOSE_OUTSIDE) && open_at(&dlg, &list, list_r, 0));
    tap(list_r.x + 5, dlg_r.y + dlg_r.h + 6); /* on the overhang: outside the dialog, yet not closing it */
    CHECK(activated_count == 2 && activated[1] == 2 && ui.popup == &dlg && dlg.popup == &list);
    tap(2, 60);
    CHECK(ui.popup == NULL && dlg.popup == NULL && activated_count == 2);

    /* three levels: keys and letters to the top, a message to all, the slider of the third held and dragged */
    CHECK(open_at(&ui, &dlg, dlg_r, DGX_UI_POPUP_MODAL) && open_at(&dlg, &list, list_r, 0) && open_at(&list, &ask, ask_r, 0));
    dgx_ui_paint(&ui, &pp, 2);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_NEXT) && ask.focus == asl && list.focus == -1 && dlg.focus == -1 && ui.focus == -1);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_NEXT) && ask.focus == ain);
    const uint32_t ab[] = {'a', 'b'};
    CHECK(dgx_ui_typed(&ui, ab, 2) && strcmp(field_buf, "ab") == 0);
    dgx_ui_msg_t msg;
    dgx_ui_msg_value(&msg, 42, NULL, 0);
    dgx_ui_take(&ui, &msg);
    CHECK(caught[0] == 42 && caught[1] == 42 && caught[2] == 42 && caught[3] == 42);
    const dgx_ui_node_t *sn = dgx_ui_node(&ask, asl);
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(sn->x + 5), .y = (int16_t)(sn->y + 5), .down = true, .pressed = true});
    CHECK(dgx_ui_holding(&ui) && dgx_ui_dragging(&ui) && ask.pressed == asl);
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(sn->x + 40), .y = (int16_t)(sn->y + 5), .down = true});
    CHECK(dgx_ui_value_of(sn) > 0);
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(sn->x + 40), .y = (int16_t)(sn->y + 5), .released = true});
    CHECK(!dgx_ui_holding(&ui) && !dgx_ui_dragging(&ui));
    /* drawn whole with three open: every pixel once; then the dialog closed: all over it closed, the screen as the
       main tree drawn alone */
    dgx_ui_paint(&ui, &pp, 3);
    dgx_ui_redraw(&ui, 0), dgx_ui_redraw(&dlg, 0), dgx_ui_redraw(&list, 0), dgx_ui_redraw(&ask, 0);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 4);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H);
    /* composed in strips with three open: as drawn */
    {
        dgx_screen_t  *band = dgx_vscreen_init(PANEL_W, 4, 16, DgxScreenRGB);
        dgx_ui_paint_t pb   = {.scr = band, .colors = colors, .color_count = 4};
        int            off  = 0;
        for (int y0 = 0; y0 < PANEL_H; y0 += 4) {
            pb.oy = (int16_t)y0;
            dgx_ui_paint_area(&ui, &pb, 0, y0, PANEL_W, 4);
            for (int y = 0; y < 4 && y0 + y < PANEL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x) off += dgx_get_pixel(band, x, y) != panel->mem[y0 + y][x];
        }
        CHECK(off == 0);
        band->destroy(&band);
    }
    /* the question closed alone: where it lay composed from the levels under it, every pixel once */
    dgx_ui_popup_close(&list);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 5);
    CHECK(list.popup == NULL && dlg.popup == &list && panel_twice(panel) == 0 && written_within(panel, ask_r) == ask_r.w * ask_r.h);
    dgx_ui_popup_close(&ui);
    CHECK(ui.popup == NULL && dlg.popup == NULL && list.popup == NULL);
    panel_count_from_now(panel);
    dgx_ui_paint(&ui, &pp, 6);
    CHECK(panel_twice(panel) == 0);
    {
        static dgx_ui_node_t    alone_n[4];
        static char             alone_buf[16];
        static dgx_ui_textbox_t alone_box;
        static dgx_ui_widget_t  alone_low;
        dgx_ui_t                alone;
        dgx_ui_textbox_init(&alone_box, alone_buf, sizeof alone_buf, 1, &style);
        alone_low = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &alone_box, .want_h = 20};
        dgx_ui_init(&alone, alone_n, 4, 1), dgx_ui_add(&alone, -1, &root_w);
        dgx_ui_add(&alone, 0, &main_bt);
        const int al = dgx_ui_add(&alone, 0, &alone_low);
        dgx_ui_text_set(&alone, al, "LOW LOW LOW");
        dgx_ui_paint(&alone, &pv, 0);
        CHECK(differ(panel, v) == 0);
        dgx_ui_free(&alone);
    }

    /* no more than DGX_UI_POPUP_DEPTH over a tree; a tree with a popup of its own, or the tree itself, never */
    {
        static dgx_ui_node_t chain_n[DGX_UI_POPUP_DEPTH + 1][1];
        static dgx_ui_t      chain[DGX_UI_POPUP_DEPTH + 1];
        for (int k = 0; k <= DGX_UI_POPUP_DEPTH; ++k) dgx_ui_init(&chain[k], chain_n[k], 1, 1), dgx_ui_add(&chain[k], -1, &list_w);
        bool all = true;
        for (int k = 0; k < DGX_UI_POPUP_DEPTH; ++k) all &= open_at(k ? &chain[k - 1] : &ui, &chain[k], list_r, 0);
        CHECK(all && !open_at(&chain[DGX_UI_POPUP_DEPTH - 1], &chain[DGX_UI_POPUP_DEPTH], list_r, 0));
        CHECK(!open_at(&chain[2], &chain[0], list_r, 0) && !open_at(&dlg, &dlg, dlg_r, 0) && dlg.popup == NULL);
        dgx_ui_paint(&ui, &pp, 7); /* eight deep: drawn without a fault */
        dgx_ui_popup_close(&ui);
        for (int k = 0; k <= DGX_UI_POPUP_DEPTH; ++k) {
            CHECK(chain[k].popup == NULL && chain[k].level == 0); /* no popup now */
            dgx_ui_free(&chain[k]);
        }
    }

    dgx_ui_free(&ui), dgx_ui_free(&dlg), dgx_ui_free(&list), dgx_ui_free(&ask);
    v->destroy(&v);
    free(panel);
    CHECK_DONE();
}
