/*
 * Time as "when": an important text that scrolls every 2000 ms names its next step, pass after pass falling to 0, and
 * draws at the step; a pass in the middle, woken by something else, names the rest; letters coming name the next
 * letter; a held key repeats at 500, then every 100; a look that asks and names nothing, 0; nobody, none; two, the
 * smaller; the clock running over; a popup's look counted; a look under the keyboard's band asks nothing. And the
 * mirror: the screen composed in bands of 8 rows onto a screen of its own by dgx_ui_paint_area_to(), equal to the
 * display, a look's canvas not made again, the paint as it was.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "freertos/FreeRTOS.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   important, plain;

/* a look of the program's that asks for time and names no moment */
static bool draw_asking(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    dgx_fill_rectangle(paint->scr, n->x - paint->ox, n->y - paint->oy, n->w, n->h, colors[2]);
    return true;
}
static const dgx_ui_look_t asking_look = {.draw = draw_asking, .fill = -1};

/* a look of a canvas, its canvases counted */
static int  made_count;
static bool draw_canvas(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    bool          made;
    dgx_screen_t *c = dgx_ui_canvas(paint, ui, i, &made);
    if (c && made) ++made_count, dgx_fill_rectangle(c, 0, 0, c->width / 2, c->height, 3);
    dgx_ui_canvas_put(paint, ui, i, -1);
    return false;
}
static const dgx_ui_look_t canvas_look = {.draw = draw_canvas, .fill = -1};

static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    important  = (dgx_ui_text_style_t){.variants = variant, .count = 1, .flags = DGX_UI_TEXT_IMPORTANT, .scroll_ms = 2000};
    plain      = (dgx_ui_text_style_t){.variants = variant, .count = 1};
    dgx_screen_t *v = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);

    /* an important text of one row and three lines, a step every 2000 ms */
    static char             tb[120], lb[40];
    static dgx_ui_textbox_t text, label;
    dgx_ui_textbox_init(&text, tb, sizeof tb, 1, &important), dgx_ui_textbox_init(&label, lb, sizeof lb, 1, &plain);
    const dgx_ui_widget_t scroll_w = {.kind = DGX_UI_LABEL, .text = &text, .look = &dgx_ui_look_lines};
    const dgx_ui_widget_t label_w  = {.kind = DGX_UI_LABEL, .text = &label, .want_h = 13};
    static dgx_ui_node_t  nodes[4];
    static dgx_ui_t       ui;
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_add(&ui, -1, &root_w);
    const int t = dgx_ui_add(&ui, 0, &scroll_w), l = dgx_ui_add(&ui, 0, &label_w);
    dgx_ui_text_set(&ui, t, "one two three four five six seven eight nine ten eleven twelve");
    dgx_ui_text_set(&ui, l, "21.5 C");
    dgx_ui_paint_t paint = {.scr = v, .colors = colors, .color_count = 4};
    const uint32_t T0    = 4294965000u; /* near the end of the clock: it runs over in the middle */
    CHECK(dgx_ui_paint(&ui, &paint, T0) && paint.wake_ms == 2000 && dgx_ui_wake(&paint, &ui, T0) == 2000);
    CHECK(dgx_ui_wake(&paint, &ui, T0 + 300) == 1700); /* the loop asks later: the rest */
    for (uint32_t k = 1; k < 20; ++k) {
        dgx_ui_paint(&ui, &paint, T0 + k * 100);
        CHECK(paint.wake_ms == 2000 - k * 100 && text.top == 0);
    }
    /* something else woke the loop in the middle of the next step: the rest of it named again */
    dgx_ui_paint(&ui, &paint, T0 + 2000);
    CHECK(text.top == 1 && paint.wake_ms == 2000);
    dgx_ui_text_set(&ui, l, "22.0 C");
    dgx_ui_paint(&ui, &paint, T0 + 2700);
    CHECK(text.top == 1 && paint.wake_ms == 1300);
    /* run over the clock: the next step still at T0 + 4000, by difference */
    const uint32_t over = T0 + 3990;
    CHECK(over < T0 && dgx_ui_paint(&ui, &paint, over) && paint.wake_ms == 10);
    /* letters coming, 50 ms each: the next letter */
    dgx_ui_text_set(&ui, t, "abc def");
    dgx_ui_text_appear(&ui, t, 50);
    dgx_ui_paint(&ui, &paint, 1000);
    CHECK(paint.wake_ms == 50);
    dgx_ui_paint(&ui, &paint, 1070);
    CHECK(paint.wake_ms == 30);
    /* a held key: its repeat at 500, then every 100 */
    dgx_ui_focus(&ui, -1);
    dgx_ui_buttons(&ui, DGX_UI_PAD_DOWN, 5000);
    CHECK(dgx_ui_wake(NULL, &ui, 5000) == 500 && dgx_ui_wake(NULL, &ui, 5200) == 300);
    dgx_ui_time(&ui, 5500);
    CHECK(dgx_ui_wake(NULL, &ui, 5500) == 100);
    dgx_ui_buttons(&ui, 0, 5550);
    CHECK(dgx_ui_wake(NULL, &ui, 5550) == DGX_UI_WAKE_NONE);
    /* the smaller of the paint's and the key's */
    dgx_ui_buttons(&ui, DGX_UI_PAD_DOWN, 1080); /* the look's letter in 20, the key's repeat in 500 */
    CHECK(dgx_ui_wake(&paint, &ui, 1080) == 20);
    dgx_ui_buttons(&ui, 0, 1080);
    dgx_ui_free(&ui);

    /* nobody asking: none; a look of the program's that asks and names nothing: 0, the smaller of two */
    {
        static dgx_ui_node_t qn[3];
        static dgx_ui_t      q;
        dgx_ui_init(&q, qn, 3, 1);
        dgx_ui_add(&q, -1, &root_w);
        dgx_ui_paint_t pq = {.scr = v, .colors = colors, .color_count = 4};
        CHECK(!dgx_ui_paint(&q, &pq, 0) && pq.wake_ms == DGX_UI_WAKE_NONE && dgx_ui_wake(&pq, &q, 0) == DGX_UI_WAKE_NONE);
        /* a text that names 2000, then a look that asks and names nothing: the next pass */
        static char             qb[120];
        static dgx_ui_textbox_t qt;
        dgx_ui_textbox_init(&qt, qb, sizeof qb, 1, &important);
        const dgx_ui_widget_t qtext = {.kind = DGX_UI_LABEL, .text = &qt, .look = &dgx_ui_look_lines};
        dgx_ui_text_set(&q, dgx_ui_add(&q, 0, &qtext), "one two three four five six seven eight nine ten eleven");
        static int32_t ask_w_place; /* its value */
        const dgx_ui_widget_t ask_w = {.kind = DGX_UI_OWN, .value = &ask_w_place, .want_h = 10, .look = &asking_look};
        dgx_ui_add(&q, 0, &ask_w);
        CHECK(dgx_ui_paint(&q, &pq, 1) && pq.wake_ms == 0);
        dgx_ui_remove(&q, 2);
        /* in a popup: counted; under the keyboard's band: asks nothing */
        static dgx_ui_node_t pn[2];
        static dgx_ui_t      pop;
        static char          pb[120];
        static dgx_ui_textbox_t pt;
        dgx_ui_textbox_init(&pt, pb, sizeof pb, 1, &important);
        const dgx_ui_widget_t ptext = {.kind = DGX_UI_LABEL, .text = &pt, .look = &dgx_ui_look_lines};
        dgx_ui_init(&pop, pn, 2, 1);
        dgx_ui_add(&pop, -1, &root_w);
        dgx_ui_text_set(&pop, dgx_ui_add(&pop, 0, &ptext), "one two three four five six seven eight nine ten");
        /* the tree's text of letters of 50 first, the popup's step of 2000 after: the least, not the last */
        dgx_ui_text_appear(&q, 1, 50);
        dgx_ui_paint(&q, &pq, 2);
        CHECK(dgx_ui_popup_open(&q, &pop, 0, 30, PANEL_W, 20, 0));
        CHECK(dgx_ui_paint(&q, &pq, 10) && pq.wake_ms == 42);
        dgx_ui_remove(&q, 1);
        dgx_ui_paint(&q, &pq, 11);
        CHECK(pq.wake_ms == 1999);
        pq.cover = dgx_ui_rect(0, 30, PANEL_W, 20); /* the band over the popup */
        dgx_ui_paint(&q, &pq, 20);
        dgx_ui_paint(&q, &pq, 30);
        CHECK(pq.wake_ms == DGX_UI_WAKE_NONE);
        pq.cover = dgx_ui_rect(0, 0, 0, 0);
        dgx_ui_popup_close(&q);
        dgx_ui_paint_free(&pq);
        dgx_ui_free(&q), dgx_ui_free(&pop);
    }

    /* the mirror: bands of 8 rows composed onto a screen of their own, equal to the display; the canvas not made
       again; the paint as it was */
    {
        panel_t       *panel = panel_make();
        static char    mb[40];
        static dgx_ui_textbox_t mt;
        dgx_ui_textbox_init(&mt, mb, sizeof mb, 1, &plain);
        const dgx_ui_widget_t text_w   = {.kind = DGX_UI_LABEL, .text = &mt, .want_h = 13};
        static int32_t canvas_w_place; /* its value */
        const dgx_ui_widget_t canvas_w = {.kind = DGX_UI_OWN, .value = &canvas_w_place, .flags = DGX_UI_CANVAS, .want_h = 20, .look = &canvas_look};
        static dgx_ui_node_t mn[4];
        static dgx_ui_t      m;
        dgx_ui_init(&m, mn, 4, 1);
        dgx_ui_add(&m, -1, &root_w);
        dgx_ui_text_set(&m, dgx_ui_add(&m, 0, &text_w), "mirror");
        dgx_ui_add(&m, 0, &canvas_w);
        dgx_ui_paint_t pm = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        dgx_ui_paint(&m, &pm, 0);
        CHECK(made_count == 1);
        const dgx_ui_paint_t before = pm;
        static uint16_t      rows[PANEL_W * 8];
        dgx_screen_t        *band = dgx_vscreen_from_array(rows, PANEL_W, 8, 16, DgxScreenRGB);
        int                  off  = 0;
        for (int y0 = 0; y0 < PANEL_H; y0 += 8) {
            dgx_ui_paint_area_to(&m, &pm, band, NULL, 0, y0, PANEL_W, 8);
            for (int y = 0; y < 8 && y0 + y < PANEL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x) off += dgx_get_pixel(band, x, y) != panel->mem[y0 + y][x];
        }
        CHECK(off == 0 && made_count == 1);
        CHECK(pm.scr == before.scr && pm.colors == before.colors && pm.ox == before.ox && pm.oy == before.oy && pm.canvases == before.canvases);
        /* with a table of its own, the ink and the paper changed places: the band otherwise, the table given back */
        static uint32_t other[4] = {0xffff, 0x0000, 0x2104, 0xf800};
        dgx_ui_paint_area_to(&m, &pm, band, other, 0, 0, PANEL_W, 8);
        CHECK(dgx_get_pixel(band, 0, 0) == 0xffff && panel->mem[0][0] == 0x0000 && pm.colors == colors);
        dgx_screen_destroy(&band);
        dgx_ui_paint_free(&pm);
        dgx_ui_free(&m);
        free(panel);
    }
    /* two trees drawn in turn by one paint, a bar and a page, each its root given, at the same now_ms: the bar's
       letters ask for 40 ms, the page for nothing; after the page, as the loop of the pages asks, the bar's 40 ms, not
       for ever; the same through dgx_ui_changed(); a page painted at a moment of its own forgets the bar's */
    {
        static char             bar_buf[40], page_buf[16];
        static dgx_ui_textbox_t bar_box, page_box;
        static dgx_ui_node_t    bar_nodes[2], page_nodes[2];
        static dgx_ui_t         bar, page;
        dgx_ui_textbox_init(&bar_box, bar_buf, sizeof bar_buf, 1, &plain), dgx_ui_textbox_init(&page_box, page_buf, sizeof page_buf, 1, &plain);
        const dgx_ui_widget_t bar_w  = {.kind = DGX_UI_LABEL, .text = &bar_box, .look = &dgx_ui_look_lines, .want_h = 13};
        const dgx_ui_widget_t page_w = {.kind = DGX_UI_LABEL, .text = &page_box, .want_h = 13};
        dgx_ui_init(&bar, bar_nodes, 2, 1), dgx_ui_init(&page, page_nodes, 2, 1);
        dgx_ui_add(&bar, -1, &root_w), dgx_ui_add(&page, -1, &root_w);
        dgx_ui_add(&bar, 0, &bar_w), dgx_ui_add(&page, 0, &page_w);
        dgx_ui_set_root(&bar, 0, 0, PANEL_W, 20), dgx_ui_set_root(&page, 0, 20, PANEL_W, PANEL_H - 20);
        dgx_ui_text_set(&bar, 1, "letters coming"), dgx_ui_text_set(&page, 1, "page");
        dgx_ui_text_appear(&bar, 1, 40);
        dgx_ui_paint_t two = {.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_paint(&bar, &two, 1000), dgx_ui_paint(&page, &two, 1000);
        for (uint32_t now = 1020; now <= 1100; now += 20) {
            dgx_ui_paint(&bar, &two, now);
            dgx_ui_paint(&page, &two, now);
            const uint32_t   ms   = dgx_ui_wake(&two, &page, now);
            const TickType_t wait = ms == DGX_UI_WAKE_NONE ? portMAX_DELAY : pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1;
            CHECK(ms != DGX_UI_WAKE_NONE && ms <= 40 && wait != portMAX_DELAY);
        }
        /* through dgx_ui_changed(), nobody told; and once all the letters came, a moment later, none asked */
        dgx_ui_changed(&bar, &two, 1120, NULL, NULL), dgx_ui_changed(&page, &two, 1120, NULL, NULL);
        CHECK(dgx_ui_wake(&two, &page, 1120) <= 40);
        dgx_ui_changed(&bar, &two, 9000, NULL, NULL), dgx_ui_changed(&page, &two, 9000, NULL, NULL);
        dgx_ui_changed(&bar, &two, 9020, NULL, NULL), dgx_ui_changed(&page, &two, 9020, NULL, NULL);
        CHECK(dgx_ui_wake(&two, &page, 9020) == DGX_UI_WAKE_NONE);
        dgx_ui_text_appear(&bar, 1, 40);
        /* the page at a moment of its own: its moments only */
        dgx_ui_paint(&bar, &two, 1140), dgx_ui_paint(&page, &two, 1141);
        CHECK(dgx_ui_wake(&two, &page, 1141) == DGX_UI_WAKE_NONE);
        dgx_ui_paint_free(&two), dgx_ui_free(&bar), dgx_ui_free(&page);
    }

    v->destroy(&v);
    CHECK_DONE();
}
