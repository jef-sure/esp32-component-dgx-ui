/*
 * Every pixel once when what lay over a marked widget goes: the painter composes the place left before its walk and
 * leaves to the walk every widget the walk draws anyway, marked whole. On a panel behind a bus, 16 bits straight or 18
 * bits through the strips (PANEL_BITS, given by the Makefile), after each pass: no pixel written twice, and the panel
 * the tree composed anew by a fresh paint. A slider whose place was written while it lay wholly or half under the
 * keyboard's band, under a popup closed, under a page closed, under a dropdown's list closed; half under a band gone and
 * half under a popup still open, then that popup closed; a widget removed under the band; a drawn background under the
 * place; a slider of a page under a popup over the page; letters coming under a popup closed; a box of which only a
 * child is marked; a mirror composed by dgx_ui_paint_area_to() between two passes, which leaves the marks alone; a
 * popup closed and the band gone, moved or staying in the same pass, their places overlapping or apart.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t       colors[4];
static panel_t       *panel;
static dgx_screen_t  *memory, *map;
static uint8_t        buffer[PANEL_W * 3 * 8];
static dgx_ui_paint_t paint;
static bool           with_map;

/* the program's memory: the places of the sliders */
static int32_t first_place, second_place, child_place, page_place;

static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t first_w  = {.kind = DGX_UI_SLIDER, .value = &first_place, .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t second_w = {.kind = DGX_UI_SLIDER, .value = &second_place, .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t group_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .want_h = 16};
static const dgx_ui_widget_t child_w  = {.kind = DGX_UI_SLIDER, .value = &child_place, .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t frame_w  = {.kind = DGX_UI_FRAME, .look = &dgx_ui_look_frame};

/* the program's picture under the tree: a checker of two colours */
static void draw_map(dgx_ui_paint_t *p, int x, int y, int w, int h, void *ctx)
{
    dgx_vscreen_region_to_screen(p->scr, x - p->ox, y - p->oy, (dgx_screen_t *)ctx, x, y, w, h, 0);
}

/* a paint of the panel, the strips named on 18 bits, the map under the tree when asked */
static void paint_anew(void)
{
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
    paint.strips = &dgx_ui_strips;
#endif
    if (with_map) dgx_ui_background_set(&paint, draw_map, map);
}

/* after a pass: no pixel twice, and the panel as a fresh paint composes the tree at now_ms; the keyboard's band, the
   program's, left out */
static int twice_or_off(dgx_ui_t *ui, uint32_t now_ms)
{
    const dgx_ui_rect_t band = paint.cover;
    dgx_ui_paint_t fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = now_ms};
    if (with_map) dgx_ui_background_set(&fresh, draw_map, map);
    dgx_fill_rectangle(memory, 0, 0, PANEL_W, PANEL_H, colors[3]);
    dgx_ui_paint_area_to(ui, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint_free(&fresh);
    int off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            const bool in_band = x >= band.x && x < band.x + band.w && y >= band.y && y < band.y + band.h;
            off += !in_band && panel->mem[y][x] != dgx_get_pixel(memory, x, y);
        }
    return panel_twice(panel) + off;
}

/* the home tree: two sliders and a box holding a third */
static dgx_ui_node_t home_nodes[6];
static dgx_ui_t      home;
static int           first, second, group, child;
static void          make_home(void)
{
    dgx_ui_init(&home, home_nodes, 6, 1);
    dgx_ui_add(&home, -1, &root_w);
    first = dgx_ui_add(&home, 0, &first_w), second = dgx_ui_add(&home, 0, &second_w);
    group = dgx_ui_add(&home, 0, &group_w), child = dgx_ui_add(&home, group, &child_w);
    first_place = second_place = child_place = 1;
    paint_anew();
    dgx_ui_paint(&home, &paint, 0);
}
static void done_home(void)
{
    dgx_ui_paint_free(&paint), dgx_ui_free(&home);
}

/* the pass after what lay over the widgets went: its count from now */
static int pass_after(dgx_ui_t *ui, uint32_t now_ms)
{
    panel_count_from_now(panel);
    dgx_ui_paint(ui, &paint, now_ms);
    return twice_or_off(ui, now_ms);
}

int main(void)
{
    panel  = panel_make();
    memory = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    map    = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) dgx_set_pixel(map, x, y, (x / 4 + y / 4) % 2 ? colors[2] : colors[0]);
    static dgx_ui_font_variant_t variant[1];
    static dgx_ui_text_style_t   style;
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.variants = variant, .count = 1};

    for (int pictured = 0; pictured < 2; ++pictured) {
        with_map = pictured;
        /* the band over the first slider wholly, then half of it; its place written meanwhile; the band gone */
        for (int half = 0; half < 2; ++half) {
            make_home();
            const dgx_ui_node_t *n = dgx_ui_node(&home, first);
            paint.cover            = dgx_ui_rect(0, 0, PANEL_W, half ? n->y + n->h / 2 : n->y + n->h);
            dgx_ui_paint(&home, &paint, 1);
            first_place = 7;
            dgx_ui_paint(&home, &paint, 2);
            paint.cover = dgx_ui_rect(0, 0, 0, 0);
            CHECK(pass_after(&home, 3) == 0 && n->state_drawn.value == 7);
            done_home();
        }
        /* a popup closed over the first slider */
        {
            make_home();
            static dgx_ui_node_t popup_nodes[1];
            dgx_ui_t             popup;
            dgx_ui_init(&popup, popup_nodes, 1, 1), dgx_ui_add(&popup, -1, &frame_w);
            const dgx_ui_node_t *n = dgx_ui_node(&home, first);
            CHECK(dgx_ui_popup_open(&home, &popup, 0, 0, PANEL_W, n->y + n->h, 0));
            dgx_ui_paint(&home, &paint, 1);
            first_place = 7;
            dgx_ui_paint(&home, &paint, 2);
            dgx_ui_popup_close(&home);
            CHECK(pass_after(&home, 3) == 0);
            dgx_ui_free(&popup), done_home();
        }
        /* a page closed over the tree, both sliders written */
        {
            make_home();
            static dgx_ui_node_t page_nodes[1];
            dgx_ui_t             page;
            dgx_ui_init(&page, page_nodes, 1, 1), dgx_ui_add(&page, -1, &frame_w);
            CHECK(dgx_ui_page_open(&home, &page));
            dgx_ui_paint(&home, &paint, 1);
            first_place = 7, second_place = 3;
            dgx_ui_paint(&home, &paint, 2);
            dgx_ui_page_back(&home);
            CHECK(pass_after(&home, 3) == 0);
            dgx_ui_free(&page), done_home();
        }
        /* half under the band that goes and half under a popup still open; then the popup closed */
        {
            make_home();
            static dgx_ui_node_t popup_nodes[1];
            dgx_ui_t             popup;
            dgx_ui_init(&popup, popup_nodes, 1, 1), dgx_ui_add(&popup, -1, &frame_w);
            const dgx_ui_node_t *n = dgx_ui_node(&home, first);
            CHECK(dgx_ui_popup_open(&home, &popup, n->x + n->w / 2, 0, PANEL_W - n->x - n->w / 2, n->y + n->h, 0));
            paint.cover = dgx_ui_rect(0, 0, n->x + n->w / 2, n->y + n->h);
            dgx_ui_paint(&home, &paint, 1);
            first_place = 7;
            dgx_ui_paint(&home, &paint, 2);
            paint.cover = dgx_ui_rect(0, 0, 0, 0);
            CHECK(pass_after(&home, 3) == 0);
            dgx_ui_popup_close(&home);
            CHECK(pass_after(&home, 4) == 0);
            dgx_ui_free(&popup), done_home();
        }
        /* a popup closed and the band gone or moved in the same pass: their places overlapping or apart, the band
           moved down over a part of the popup's place, the band staying over it; a child slider in the part they share
           written meanwhile, left to the walk by both */
        {
            static const struct {
                dgx_ui_rect_t popup, band, band_after;
                bool          written;
            } both[] = {
                {{10, 10, 60, 30}, {0, 30, 96, 20}, {0, 0, 0, 0}, false},
                {{10, 2, 60, 12}, {0, 44, 96, 20}, {0, 0, 0, 0}, false},
                {{10, 10, 60, 30}, {0, 30, 96, 20}, {0, 44, 96, 20}, false},
                {{10, 10, 60, 30}, {0, 30, 96, 20}, {0, 30, 96, 20}, false},
                {{10, 10, 60, 30}, {0, 30, 96, 20}, {0, 0, 0, 0}, true},
            };
            for (size_t k = 0; k < sizeof both / sizeof both[0]; ++k) {
                make_home();
                static dgx_ui_node_t popup_nodes[1];
                dgx_ui_t             popup;
                dgx_ui_init(&popup, popup_nodes, 1, 1), dgx_ui_add(&popup, -1, &frame_w);
                CHECK(dgx_ui_popup_open(&home, &popup, both[k].popup.x, both[k].popup.y, both[k].popup.w, both[k].popup.h, 0));
                paint.cover = both[k].band;
                dgx_ui_paint(&home, &paint, 1);
                if (both[k].written) child_place = 9;
                dgx_ui_paint(&home, &paint, 2);
                dgx_ui_popup_close(&home);
                paint.cover = both[k].band_after;
                CHECK(pass_after(&home, 3) == 0);
                CHECK(!both[k].written || dgx_ui_node(&home, child)->state_drawn.value == 9);
                paint.cover = dgx_ui_rect(0, 0, 0, 0);
                dgx_ui_free(&popup), done_home();
            }
        }
        /* a widget removed while it lay under the band: its place paper once */
        {
            make_home();
            const dgx_ui_node_t *n = dgx_ui_node(&home, second);
            paint.cover            = dgx_ui_rect(0, 0, PANEL_W, n->y + n->h);
            dgx_ui_paint(&home, &paint, 1);
            dgx_ui_remove(&home, second);
            dgx_ui_paint(&home, &paint, 2);
            paint.cover = dgx_ui_rect(0, 0, 0, 0);
            CHECK(pass_after(&home, 3) == 0);
            done_home();
        }
        /* a box of which only the child is marked, under the band: the box composed around it, the child left to the
           walk */
        {
            make_home();
            const dgx_ui_node_t *n = dgx_ui_node(&home, group);
            paint.cover            = dgx_ui_rect(0, 0, PANEL_W, n->y + n->h);
            dgx_ui_paint(&home, &paint, 1);
            child_place = 9;
            dgx_ui_paint(&home, &paint, 2);
            paint.cover = dgx_ui_rect(0, 0, 0, 0);
            CHECK(pass_after(&home, 3) == 0 && dgx_ui_node(&home, child)->state_drawn.value == 9);
            done_home();
        }
        /* a slider of a page under a popup over the page: the popup closed */
        {
            make_home();
            static dgx_ui_node_t         page_nodes[2], popup_nodes[1];
            dgx_ui_t                     page, popup;
            static const dgx_ui_widget_t page_root_w   = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 4, .look = &dgx_ui_look_frame};
            static const dgx_ui_widget_t page_slider_w = {.kind = DGX_UI_SLIDER, .value = &page_place, .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};
            page_place                                 = 1;
            dgx_ui_init(&page, page_nodes, 2, 1), dgx_ui_add(&page, -1, &page_root_w);
            const int slider = dgx_ui_add(&page, 0, &page_slider_w);
            dgx_ui_init(&popup, popup_nodes, 1, 1), dgx_ui_add(&popup, -1, &frame_w);
            CHECK(dgx_ui_page_open(&home, &page));
            dgx_ui_paint(&home, &paint, 1);
            const dgx_ui_node_t *n = dgx_ui_node(&page, slider);
            CHECK(dgx_ui_popup_open(&page, &popup, 0, 0, PANEL_W, n->y + n->h, 0));
            dgx_ui_paint(&home, &paint, 2);
            page_place = 8;
            dgx_ui_paint(&home, &paint, 3);
            dgx_ui_popup_close(&page);
            CHECK(pass_after(&home, 4) == 0 && n->state_drawn.value == 8);
            dgx_ui_free(&popup), dgx_ui_free(&page), done_home();
        }
        /* a mirror composed by the same paint between two passes, the first slider written: the mark left alone, the
           pass after draws it */
        {
            make_home();
            first_place = 5;
            dgx_screen_t *mirror = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
            dgx_ui_paint_area_to(&home, &paint, mirror, NULL, 0, 0, PANEL_W, PANEL_H);
            dgx_ui_paint_t fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = paint.now_ms};
            if (with_map) dgx_ui_background_set(&fresh, draw_map, map);
            dgx_ui_paint_area_to(&home, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
            dgx_ui_paint_free(&fresh);
            int mirror_off = 0; /* the mirror has the slider as its memory is: the program's composing leaves nothing out */
            for (int y = 0; y < PANEL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x) mirror_off += dgx_get_pixel(mirror, x, y) != dgx_get_pixel(memory, x, y);
            CHECK(mirror_off == 0 && dgx_ui_node(&home, first)->state_drawn.value == 1);
            CHECK(pass_after(&home, 1) == 0 && dgx_ui_node(&home, first)->state_drawn.value == 5);
            dgx_screen_destroy(&mirror), done_home();
        }
    }

    /* a dropdown's list closed over a slider whose place was written while the list was open */
    {
        with_map = false;
        static char              options[32];
        static dgx_ui_textbox_t  box;
        static dgx_ui_dropdown_t dropdown;
        static int32_t           choice;
        static dgx_ui_node_t     nodes[3];
        dgx_ui_t                 ui;
        dgx_ui_textbox_init(&box, options, sizeof options, 1, &style);
        const dgx_ui_widget_t choice_w = {.kind = DGX_UI_DROPDOWN, .value = &choice, .text = &box, .max = 3, .want_h = 14, .look = &dgx_ui_look_dropdown};
        choice = 0, first_place = 1;
        dgx_ui_init(&ui, nodes, 3, 1), dgx_ui_add(&ui, -1, &root_w);
        const int d = dgx_ui_add(&ui, 0, &choice_w), slider = dgx_ui_add(&ui, 0, &first_w);
        dgx_ui_text_set(&ui, d, "a\nb\nc\nd");
        paint_anew();
        dgx_ui_paint(&ui, &paint, 0);
        CHECK(dgx_ui_dropdown_open(&ui, d, &dropdown, NULL, NULL));
        dgx_ui_paint(&ui, &paint, 1);
        first_place = 6;
        dgx_ui_paint(&ui, &paint, 2);
        dgx_ui_key(&ui, DGX_UI_KEY_BACK);
        CHECK(pass_after(&ui, 3) == 0 && dgx_ui_node(&ui, slider)->state_drawn.value == 6);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }

    /* letters coming in a label of lines under a popup closed: the label drawn whole once by the walk */
    {
        with_map = false;
        static char                  note_buf[64];
        static dgx_ui_textbox_t      note_box;
        static dgx_ui_node_t         nodes[2], popup_nodes[1];
        dgx_ui_t                     ui, popup;
        dgx_ui_textbox_init(&note_box, note_buf, sizeof note_buf, 2, &style);
        const dgx_ui_widget_t note_w = {.kind = DGX_UI_LABEL, .text = &note_box, .look = &dgx_ui_look_lines};
        dgx_ui_init(&ui, nodes, 2, 1), dgx_ui_add(&ui, -1, &root_w);
        const int note = dgx_ui_add(&ui, 0, &note_w);
        dgx_ui_text_set(&ui, note, "letters come one by one");
        dgx_ui_text_appear(&ui, note, 40);
        paint_anew();
        dgx_ui_paint(&ui, &paint, 0);
        dgx_ui_init(&popup, popup_nodes, 1, 1), dgx_ui_add(&popup, -1, &frame_w);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, note);
        CHECK(dgx_ui_popup_open(&ui, &popup, 0, 0, PANEL_W, n->y + n->h, 0));
        for (uint32_t now = 20; now <= 200; now += 20) dgx_ui_paint(&ui, &paint, now);
        dgx_ui_popup_close(&ui);
        CHECK(pass_after(&ui, 220) == 0);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui), dgx_ui_free(&popup);
    }

    dgx_screen_destroy(&memory), dgx_screen_destroy(&map), free(panel);
    CHECK_DONE();
}
