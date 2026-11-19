/*
 * A widget's value in the program's memory. A widget names the place of its value and its node keeps none; the
 * painter looks at every place at the start of a pass. Memory written and nothing called: the next pass draws that
 * widget alone, every pixel once, and a pass after it nothing. Two widgets of one place both follow the user and the
 * program. A page's slider and a list of the tree under it, of one place: the list right when the page goes, and
 * right again when it is picked anew. Memory out of a widget's bounds drawn within them and left as it is. A list's
 * pick written into memory: its window follows, under a page too. A control of no place. Undo by a copy. A take()
 * that writes the memory itself. A hidden widget, and one under the keyboard's band written three times: drawn once
 * when seen. dgx_ui_changed() telling the widget whose memory changed. A key stepping from memory out of the bounds.
 * The place written while the finger holds the slider: the last writer wins. The dropdown's place written while its
 * list is open. A slider of a page open over the tree, its place written. A tree with no paint reads the memory at
 * once. A place written after a pass asks for no pass.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;

static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};

static int panel_differ(const panel_t *p, dgx_screen_t *v)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->mem[y][x] != dgx_get_pixel(v, x, y);
    return n;
}

static int differ(dgx_screen_t *a, dgx_screen_t *b)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += dgx_get_pixel(a, x, y) != dgx_get_pixel(b, x, y);
    return n;
}

/* a tree of two widgets under the root, drawn whole on a screen: what the memory looks like drawn from nothing */
static void fresh(dgx_screen_t *v, const dgx_ui_widget_t *first, const dgx_ui_widget_t *second)
{
    static dgx_ui_node_t nodes[3];
    dgx_ui_t             ui;
    dgx_ui_paint_t       paint = {.scr = v, .colors = colors, .color_count = 4};
    dgx_fill_rectangle(v, 0, 0, PANEL_W, PANEL_H, 0x1234);
    dgx_ui_init(&ui, nodes, 3, 1);
    dgx_ui_add(&ui, -1, &root_w);
    dgx_ui_add(&ui, 0, first);
    if (second) dgx_ui_add(&ui, 0, second);
    dgx_ui_paint(&ui, &paint, 0);
    dgx_ui_paint_free(&paint);
    dgx_ui_free(&ui);
}

/* the program's memory */
static int32_t level, other, shared, pick, month, edit[2];
static const int32_t kept[2] = {3, 1};

static const dgx_ui_widget_t level_w  = {.kind = DGX_UI_SLIDER, .value = &level, .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t other_w  = {.kind = DGX_UI_SLIDER, .value = &other, .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t both_s_w = {.kind = DGX_UI_SLIDER, .value = &shared, .max = 1, .want_h = 8, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t both_w_w = {.kind = DGX_UI_SWITCH, .value = &shared, .max = 1, .want_w = 20, .want_h = 8, .look = &dgx_ui_look_switch};
static const dgx_ui_widget_t nowhere_w = {.kind = DGX_UI_SLIDER, .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t edit_s_w = {.kind = DGX_UI_SLIDER, .value = &edit[0], .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t edit_w_w = {.kind = DGX_UI_SWITCH, .value = &edit[1], .max = 1, .want_w = 20, .want_h = 8, .look = &dgx_ui_look_switch};

/* a take() that writes the program's memory itself, and not through the widget */
enum { MSG_LEVEL = 5 };
static bool take_level(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    int32_t v;
    if (dgx_ui_msg_copy(msg, MSG_LEVEL, &v, sizeof v) == ESP_OK) level = v;
    return false;
}
static const dgx_ui_widget_t taking_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2, .take = take_level};

typedef struct {
    dgx_ui_rect_t at[8];
    int           count;
} told_t;
static void collect(void *ctx, dgx_ui_rect_t rect)
{
    told_t *told = ctx;
    if (told->count < 8) told->at[told->count++] = rect;
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    style      = (dgx_ui_text_style_t){.variants = variant, .count = 1};
    panel_t      *panel = panel_make();
    dgx_screen_t *v = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), *e = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);

    /* memory written and nothing called: the next pass draws that widget alone, every pixel once; then nothing */
    {
        static dgx_ui_node_t nodes[3];
        dgx_ui_t             ui;
        dgx_ui_paint_t       paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        level = 2, other = 5;
        dgx_ui_init(&ui, nodes, 3, 1);
        dgx_ui_add(&ui, -1, &root_w);
        const int l = dgx_ui_add(&ui, 0, &level_w), o = dgx_ui_add(&ui, 0, &other_w);
        dgx_ui_paint(&ui, &paint, 0);
        fresh(v, &level_w, &other_w);
        CHECK(panel_differ(panel, v) == 0 && dgx_ui_value(&ui, l) == 2 && dgx_ui_value(&ui, o) == 5); /* drawn from the memory at start */
        const dgx_ui_node_t *n = dgx_ui_node(&ui, l);
        panel_count_from_now(panel);
        level = 7; /* the program writes its memory, and calls nothing */
        dgx_ui_paint(&ui, &paint, 1);
        fresh(v, &level_w, &other_w);
        CHECK(panel_written(panel) > 0 && panel_twice(panel) == 0 && panel_written_outside(panel, n->x, n->y, n->x + n->w, n->y + n->h) == 0);
        CHECK(panel_differ(panel, v) == 0 && dgx_ui_value(&ui, l) == 7 && n->state_drawn.value == 7);
        panel_count_from_now(panel);
        dgx_ui_paint(&ui, &paint, 2);
        CHECK(panel_written(panel) == 0);

        /* out of its bounds, as flash may bring it: drawn within them, the memory left as it is; the user then moves
           it from the bound */
        level = 10;
        dgx_ui_paint(&ui, &paint, 3);
        panel_count_from_now(panel);
        level = 999;
        dgx_ui_paint(&ui, &paint, 4);
        CHECK(panel_written(panel) == 0 && dgx_ui_value(&ui, l) == 10 && level == 999);
        level = -5;
        dgx_ui_paint(&ui, &paint, 5);
        fresh(v, &level_w, &other_w);
        CHECK(panel_differ(panel, v) == 0 && dgx_ui_value(&ui, l) == 0 && level == -5);
        dgx_ui_focus(&ui, l);
        CHECK(dgx_ui_key(&ui, DGX_UI_KEY_MORE) && level == 1);

        /* hidden: its memory changed, nothing drawn; shown, drawn as the memory is */
        dgx_ui_focus(&ui, -1);
        dgx_ui_show(&ui, o, false);
        dgx_ui_paint(&ui, &paint, 6);
        panel_count_from_now(panel);
        other = 9;
        dgx_ui_paint(&ui, &paint, 7);
        CHECK(panel_written(panel) == 0);
        dgx_ui_show(&ui, o, true);
        dgx_ui_paint(&ui, &paint, 8);
        fresh(v, &level_w, &other_w);
        CHECK(panel_differ(panel, v) == 0);

        /* dgx_ui_set_value() writes into the place; a widget's number that holds none reads 0 */
        dgx_ui_set_value(&ui, l, 6);
        CHECK(level == 6 && dgx_ui_value(&ui, 99) == 0);
        dgx_ui_set_value(&ui, l, 50);
        CHECK(level == 10);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&ui);
    }

    /* two widgets of one place: both follow the user, and the program */
    {
        static dgx_ui_node_t nodes[3];
        dgx_ui_t             ui;
        dgx_ui_paint_t       paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        shared = 0;
        dgx_ui_init(&ui, nodes, 3, 1);
        dgx_ui_add(&ui, -1, &root_w);
        const int s = dgx_ui_add(&ui, 0, &both_s_w), w = dgx_ui_add(&ui, 0, &both_w_w);
        dgx_ui_paint(&ui, &paint, 0);
        const dgx_ui_node_t *sn = dgx_ui_node(&ui, s), *wn = dgx_ui_node(&ui, w);
        dgx_ui_focus(&ui, s);
        dgx_ui_paint(&ui, &paint, 1);
        panel_count_from_now(panel);
        CHECK(dgx_ui_key(&ui, DGX_UI_KEY_MORE) && shared == 1 && dgx_ui_value(&ui, w) == 1); /* the user moved the slider */
        dgx_ui_paint(&ui, &paint, 2);
        CHECK(panel_twice(panel) == 0 && panel_written_outside(panel, sn->x, sn->y, sn->x + sn->w, sn->y + sn->h) > 0); /* the switch too */
        CHECK(wn->state_drawn.value == 1 && sn->state_drawn.value == 1);
        shared = 0; /* the program */
        dgx_ui_paint(&ui, &paint, 3);
        CHECK(wn->state_drawn.value == 0 && sn->state_drawn.value == 0);
        dgx_ui_focus(&ui, -1);
        dgx_ui_paint(&ui, &paint, 4);
        fresh(v, &both_s_w, &both_w_w);
        CHECK(panel_differ(panel, v) == 0);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&ui);
    }

    /* a page's slider and a list of the tree under it, of one place: the slider moved, the page closed, the list is
       right; then the list picked again: the row it showed and the row it shows, two rows and not three */
    {
        const int line = dgx_ui_variant_line(&variant[0]);
        static char             buf[16], ref_buf[16];
        static dgx_ui_textbox_t box, ref_box;
        dgx_ui_textbox_init(&box, buf, sizeof buf, 1, &style), dgx_ui_textbox_init(&ref_box, ref_buf, sizeof ref_buf, 1, &style);
        const dgx_ui_widget_t list_w = {.kind = DGX_UI_LIST, .value = &pick, .text = &box, .max = 3, .want_h = (int16_t)(4 * line), .look = &dgx_ui_look_list};
        const dgx_ui_widget_t ref_w  = {.kind = DGX_UI_LIST, .value = &pick, .text = &ref_box, .max = 3, .want_h = (int16_t)(4 * line), .look = &dgx_ui_look_list};
        static const dgx_ui_widget_t page_root_w   = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2};
        static const dgx_ui_widget_t page_slider_w = {.kind = DGX_UI_SLIDER, .value = &pick, .max = 3, .want_h = 8, .look = &dgx_ui_look_slider};
        static dgx_ui_node_t home_nodes[2], page_nodes[2];
        dgx_ui_t             home, page;
        dgx_ui_paint_t       paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        pick = 0;
        dgx_ui_init(&home, home_nodes, 2, 1);
        dgx_ui_add(&home, -1, &root_w);
        const int list = dgx_ui_add(&home, 0, &list_w);
        dgx_ui_text_set(&home, list, "a\nb\nc\nd");
        strcpy(ref_buf, "a\nb\nc\nd");
        dgx_ui_init(&page, page_nodes, 2, 1);
        dgx_ui_add(&page, -1, &page_root_w);
        const int slider = dgx_ui_add(&page, 0, &page_slider_w);
        dgx_ui_paint(&home, &paint, 0);
        CHECK(dgx_ui_page_open(&home, &page));
        dgx_ui_paint(&home, &paint, 1);
        dgx_ui_focus(&page, slider);
        CHECK(dgx_ui_key(&home, DGX_UI_KEY_MORE) && dgx_ui_key(&home, DGX_UI_KEY_MORE) && pick == 2 && dgx_ui_value(&home, list) == 2);
        dgx_ui_paint(&home, &paint, 2);
        CHECK(dgx_ui_page_back(&home));
        dgx_ui_paint(&home, &paint, 3);
        fresh(v, &ref_w, NULL);
        const dgx_ui_node_t *n = dgx_ui_node(&home, list);
        CHECK(panel_differ(panel, v) == 0 && n->state_drawn.value == 2);
        panel_count_from_now(panel);
        pick = 1;
        dgx_ui_paint(&home, &paint, 4);
        fresh(v, &ref_w, NULL);
        CHECK(panel_differ(panel, v) == 0 && panel_twice(panel) == 0 && panel_written(panel) == 2 * (n->h / 4) * n->w);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&home), dgx_ui_free(&page);
    }

    /* a list that scrolls: its pick written into memory, its window follows */
    {
        const int line = dgx_ui_variant_line(&variant[0]);
        static char             buf[64];
        static dgx_ui_textbox_t box;
        dgx_ui_textbox_init(&box, buf, sizeof buf, 1, &style);
        const dgx_ui_widget_t months_w = {.kind = DGX_UI_LIST, .value = &month, .text = &box, .max = 11, .want_h = (int16_t)(4 * line), .look = &dgx_ui_look_list};
        static dgx_ui_node_t  nodes[2];
        dgx_ui_t              ui;
        dgx_ui_paint_t        paint = {.scr = v, .colors = colors, .color_count = 4};
        month = 0;
        dgx_ui_init(&ui, nodes, 2, 1);
        dgx_ui_add(&ui, -1, &root_w);
        dgx_ui_text_set(&ui, dgx_ui_add(&ui, 0, &months_w), "Jan\nFeb\nMar\nApr\nMay\nJun\nJul\nAug\nSep\nOct\nNov\nDec");
        dgx_ui_paint(&ui, &paint, 0);
        CHECK(box.top == 0);
        month = 11;
        dgx_ui_paint(&ui, &paint, 1);
        CHECK(box.top == 8);
        month = 2;
        dgx_ui_paint(&ui, &paint, 2);
        CHECK(box.top == 2);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&ui);
    }

    /* a control of no place: it shows 0, and the keys change nothing in it */
    {
        static dgx_ui_node_t nodes[2];
        dgx_ui_t             ui;
        dgx_ui_paint_t       paint = {.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_init(&ui, nodes, 2, 1);
        dgx_ui_add(&ui, -1, &root_w);
        const int s = dgx_ui_add(&ui, 0, &nowhere_w);
        dgx_ui_paint(&ui, &paint, 0);
        dgx_ui_focus(&ui, s);
        dgx_ui_key(&ui, DGX_UI_KEY_MORE);
        dgx_ui_set_value(&ui, s, 4);
        CHECK(dgx_ui_value(&ui, s) == 0);
        dgx_ui_paint(&ui, &paint, 1);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&ui);
    }

    /* Undo by a copy: the widgets of a page point at an edit copy; the kept values copied back, the screen as before */
    {
        static dgx_ui_node_t nodes[3];
        dgx_ui_t             ui;
        dgx_ui_paint_t       paint = {.scr = v, .colors = colors, .color_count = 4};
        memcpy(edit, kept, sizeof edit);
        dgx_ui_init(&ui, nodes, 3, 1);
        dgx_ui_add(&ui, -1, &root_w);
        const int s = dgx_ui_add(&ui, 0, &edit_s_w), w = dgx_ui_add(&ui, 0, &edit_w_w);
        dgx_ui_focus(&ui, s);
        dgx_ui_paint(&ui, &paint, 0);
        dgx_vscreen_copy(e, v);
        CHECK(memcmp(edit, kept, sizeof edit) == 0); /* nothing to discard */
        dgx_ui_key(&ui, DGX_UI_KEY_MORE), dgx_ui_key(&ui, DGX_UI_KEY_MORE);
        dgx_ui_focus(&ui, w);
        dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE);
        dgx_ui_focus(&ui, s);
        dgx_ui_paint(&ui, &paint, 1);
        CHECK(edit[0] == 5 && edit[1] == 0 && memcmp(edit, kept, sizeof edit) != 0 && differ(v, e) > 0); /* "discard the changes?" */
        memcpy(edit, kept, sizeof edit); /* Undo */
        dgx_ui_paint(&ui, &paint, 2);
        CHECK(differ(v, e) == 0);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&ui);
    }

    /* a take() that writes the memory itself: the widget of that place is drawn by the pass after it */
    {
        static dgx_ui_node_t nodes[2];
        dgx_ui_t             ui;
        dgx_ui_paint_t       paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        level = 0;
        dgx_ui_init(&ui, nodes, 2, 1);
        dgx_ui_add(&ui, -1, &taking_root_w);
        const int l = dgx_ui_add(&ui, 0, &level_w);
        dgx_ui_paint(&ui, &paint, 0);
        dgx_ui_msg_t  msg;
        const int32_t eight = 8;
        dgx_ui_msg_value(&msg, MSG_LEVEL, &eight, sizeof eight);
        dgx_ui_take(&ui, &msg);
        dgx_ui_msg_done(&msg);
        panel_count_from_now(panel);
        dgx_ui_paint(&ui, &paint, 1);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, l);
        CHECK(level == 8 && n->state_drawn.value == 8 && panel_written(panel) > 0 && panel_twice(panel) == 0);

        /* dgx_ui_changed(): the widget whose memory changed told, and nothing at the call after; nothing written */
        told_t told = {0};
        level       = 3;
        panel_count_from_now(panel);
        dgx_ui_changed(&ui, &paint, 2, collect, &told);
        CHECK(told.count == 1 && told.at[0].x == n->x && told.at[0].y == n->y && told.at[0].w == n->w && told.at[0].h == n->h);
        told.count = 0;
        dgx_ui_changed(&ui, &paint, 3, collect, &told);
        CHECK(told.count == 0 && panel_written(panel) == 0);
        dgx_ui_paint_free(&paint);
        dgx_ui_free(&ui);
    }

    /* a key stepping from memory out of the bounds: from the bound it shows, not from the memory. LESS from 999 in
       0..10 writes 9; MORE from 999 changes nothing, the memory left as it is */
    {
        static dgx_ui_node_t nodes[2];
        dgx_ui_t             ui;
        dgx_ui_paint_t       paint = {.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_init(&ui, nodes, 2, 1);
        dgx_ui_add(&ui, -1, &root_w);
        const int l = dgx_ui_add(&ui, 0, &level_w);
        level       = 999;
        dgx_ui_paint(&ui, &paint, 0);
        dgx_ui_focus(&ui, l);
        dgx_ui_key(&ui, DGX_UI_KEY_MORE);
        CHECK(level == 999 && dgx_ui_value(&ui, l) == 10);
        CHECK(dgx_ui_key(&ui, DGX_UI_KEY_LESS) && level == 9);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }

    /* the place written while the finger holds the slider: shown at the next pass; the finger's next move writes over
       it, the last writer wins; every pixel once, nothing left old */
    {
        static dgx_ui_node_t nodes[2];
        dgx_ui_t             ui;
        dgx_ui_paint_t       paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        dgx_ui_init(&ui, nodes, 2, 1);
        dgx_ui_add(&ui, -1, &root_w);
        const int            l = dgx_ui_add(&ui, 0, &level_w);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, l);
        level                  = 0;
        dgx_ui_paint(&ui, &paint, 0);
        dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(n->x + n->w / 4), .y = (int16_t)(n->y + n->h / 2), .down = true, .pressed = true, .ms = 10});
        dgx_ui_paint(&ui, &paint, 10);
        level = 9; /* a message, meanwhile */
        panel_count_from_now(panel);
        dgx_ui_paint(&ui, &paint, 20);
        fresh(v, &level_w, NULL);
        CHECK(panel_twice(panel) == 0 && panel_differ(panel, v) == 0);
        dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(n->x + n->w / 2), .y = (int16_t)(n->y + n->h / 2), .down = true, .ms = 30});
        const int32_t moved = level;
        panel_count_from_now(panel);
        dgx_ui_paint(&ui, &paint, 30);
        fresh(v, &level_w, NULL);
        CHECK(moved != 9 && panel_twice(panel) == 0 && panel_differ(panel, v) == 0);
        dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(n->x + n->w / 2), .y = (int16_t)(n->y + n->h / 2), .released = true, .ms = 40});
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }

    /* under the keyboard's band, wholly: its place written three times, nothing drawn; the band gone, drawn once, as
       the memory is then */
    {
        static dgx_ui_node_t nodes[3];
        dgx_ui_t             ui;
        dgx_ui_paint_t       paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        dgx_ui_init(&ui, nodes, 3, 1);
        dgx_ui_add(&ui, -1, &root_w);
        const int            l = dgx_ui_add(&ui, 0, &level_w);
        dgx_ui_add(&ui, 0, &other_w);
        const dgx_ui_node_t *n = dgx_ui_node(&ui, l);
        level = 1, other = 1;
        dgx_ui_paint(&ui, &paint, 0);
        paint.cover = dgx_ui_rect(0, 0, PANEL_W, n->y + n->h); /* the band over the first slider, wholly */
        dgx_ui_paint(&ui, &paint, 1);
        int written = 0;
        for (int k = 0; k < 3; ++k) {
            level = 3 + k;
            panel_count_from_now(panel);
            dgx_ui_paint(&ui, &paint, (uint32_t)(2 + k));
            written += panel_written(panel);
        }
        CHECK(written == 0 && n->state_drawn.value == 1);
        paint.cover = dgx_ui_rect(0, 0, 0, 0);
        panel_count_from_now(panel);
        dgx_ui_paint(&ui, &paint, 10);
        fresh(v, &level_w, &other_w);
        CHECK(panel_twice(panel) == 0 && panel_differ(panel, v) == 0 && n->state_drawn.value == 5);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }

    /* a list that scrolls, under a page: its pick written into memory while covered; the page gone, its window at the
       pick */
    {
        const int               line = dgx_ui_variant_line(&variant[0]);
        static char             buf[64];
        static dgx_ui_textbox_t box;
        dgx_ui_textbox_init(&box, buf, sizeof buf, 1, &style);
        const dgx_ui_widget_t        months_w    = {.kind = DGX_UI_LIST, .value = &month, .text = &box, .max = 11, .want_h = (int16_t)(4 * line), .look = &dgx_ui_look_list};
        static const dgx_ui_widget_t page_root_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2};
        static dgx_ui_node_t         home_nodes[2], page_nodes[1];
        dgx_ui_t                     home, page;
        dgx_ui_paint_t               paint = {.scr = v, .colors = colors, .color_count = 4};
        month = 0;
        dgx_ui_init(&home, home_nodes, 2, 1), dgx_ui_add(&home, -1, &root_w);
        dgx_ui_text_set(&home, dgx_ui_add(&home, 0, &months_w), "Jan\nFeb\nMar\nApr\nMay\nJun\nJul\nAug\nSep\nOct\nNov\nDec");
        dgx_ui_init(&page, page_nodes, 1, 1), dgx_ui_add(&page, -1, &page_root_w);
        dgx_ui_paint(&home, &paint, 0);
        CHECK(dgx_ui_page_open(&home, &page));
        dgx_ui_paint(&home, &paint, 1);
        month = 11;
        dgx_ui_paint(&home, &paint, 2);
        CHECK(dgx_ui_page_back(&home));
        dgx_ui_paint(&home, &paint, 3);
        CHECK(box.top == 8);
        dgx_ui_paint_free(&paint), dgx_ui_free(&home), dgx_ui_free(&page);
    }

    /* the dropdown's place written by the program while its list is open: the list's own place does not follow; closed
       by back with no choice, the dropdown shows the program's value, drawn once; a row chosen writes over it */
    {
        static char              options[32];
        static dgx_ui_textbox_t  box;
        static dgx_ui_dropdown_t dropdown;
        static int32_t           chosen;
        dgx_ui_textbox_init(&box, options, sizeof options, 1, &style);
        const dgx_ui_widget_t choice_w = {.kind = DGX_UI_DROPDOWN, .value = &chosen, .text = &box, .max = 3, .want_h = 14, .look = &dgx_ui_look_dropdown};
        static dgx_ui_node_t  nodes[2];
        dgx_ui_t              ui;
        dgx_ui_paint_t        paint = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        chosen = 0;
        dgx_ui_init(&ui, nodes, 2, 1), dgx_ui_add(&ui, -1, &root_w);
        const int d = dgx_ui_add(&ui, 0, &choice_w);
        dgx_ui_text_set(&ui, d, "one\ntwo\nthree\nfour");
        dgx_ui_paint(&ui, &paint, 0);
        CHECK(dgx_ui_dropdown_open(&ui, d, &dropdown, NULL, NULL));
        dgx_ui_paint(&ui, &paint, 1);
        chosen = 2; /* a message sets the choice */
        dgx_ui_paint(&ui, &paint, 2);
        CHECK(dropdown.pick == 0 && dgx_ui_value(&dropdown.tree, 1) == 0 && dgx_ui_value(&ui, d) == 2);
        CHECK(dgx_ui_key(&ui, DGX_UI_KEY_BACK) && !ui.popup);
        panel_count_from_now(panel);
        dgx_ui_paint(&ui, &paint, 3);
        CHECK(chosen == 2 && panel_twice(panel) == 0 && dgx_ui_node(&ui, d)->state_drawn.value == 2);
        /* opened again: the list begins at the program's value; a row chosen writes over it */
        CHECK(dgx_ui_dropdown_open(&ui, d, &dropdown, NULL, NULL) && dropdown.pick == 2);
        dgx_ui_paint(&ui, &paint, 4);
        dgx_ui_key(&ui, DGX_UI_KEY_NEXT);
        dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE);
        CHECK(chosen == 3 && !ui.popup);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }

    /* a slider of a page open over the tree: the program writes its place, and calls nothing; the next pass draws it,
       the look at the places made for the trees over the tree too */
    {
        static int32_t               volume;
        static const dgx_ui_widget_t page_root_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2};
        static const dgx_ui_widget_t volume_w    = {.kind = DGX_UI_SLIDER, .value = &volume, .max = 10, .want_h = 8, .look = &dgx_ui_look_slider};
        static dgx_ui_node_t         home_nodes[2], page_nodes[2];
        dgx_ui_t                     home, page;
        dgx_ui_paint_t               paint = {.scr = v, .colors = colors, .color_count = 4};
        volume = 1;
        dgx_ui_init(&home, home_nodes, 2, 1), dgx_ui_add(&home, -1, &root_w), dgx_ui_add(&home, 0, &level_w);
        dgx_ui_init(&page, page_nodes, 2, 1), dgx_ui_add(&page, -1, &page_root_w);
        const int slider = dgx_ui_add(&page, 0, &volume_w);
        dgx_ui_paint(&home, &paint, 0);
        CHECK(dgx_ui_page_open(&home, &page));
        dgx_ui_paint(&home, &paint, 1);
        volume = 8;
        dgx_ui_paint(&home, &paint, 2);
        CHECK(dgx_ui_node(&page, slider)->state_drawn.value == 8);
        dgx_ui_paint_free(&paint), dgx_ui_free(&home), dgx_ui_free(&page);
    }

    /* a tree with no paint, the tests of logic with no display: dgx_ui_value() reads the memory at once */
    {
        static dgx_ui_node_t nodes[2];
        dgx_ui_t             ui;
        dgx_ui_init(&ui, nodes, 2, 1), dgx_ui_add(&ui, -1, &root_w);
        const int l = dgx_ui_add(&ui, 0, &level_w);
        level       = 4;
        CHECK(dgx_ui_value(&ui, l) == 4);
        level = 77;
        CHECK(dgx_ui_value(&ui, l) == 10 && level == 77);
        dgx_ui_free(&ui);
    }

    /* a place written after a pass asks for no pass: dgx_ui_wake() stays as it was, the task that writes it wakes its
       own loop; the next pass draws it */
    {
        static dgx_ui_node_t nodes[2];
        dgx_ui_t             ui;
        dgx_ui_paint_t       paint = {.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_init(&ui, nodes, 2, 1), dgx_ui_add(&ui, -1, &root_w);
        const int l = dgx_ui_add(&ui, 0, &level_w);
        level       = 1;
        dgx_ui_paint(&ui, &paint, 0);
        CHECK(dgx_ui_wake(&paint, &ui, 0) == DGX_UI_WAKE_NONE);
        level = 6;
        CHECK(dgx_ui_wake(&paint, &ui, 5) == DGX_UI_WAKE_NONE);
        dgx_ui_paint(&ui, &paint, 1000);
        CHECK(dgx_ui_node(&ui, l)->state_drawn.value == 6 && dgx_ui_wake(&paint, &ui, 1000) == DGX_UI_WAKE_NONE);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }

    v->destroy(&v), e->destroy(&e);
    free(panel);
    CHECK_DONE();
}
