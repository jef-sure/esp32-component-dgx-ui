/*
 * Pages changed by a transition (dgx_ui_transition.h), on a screen that can be read, as a DSI display, and on a
 * counting panel that cannot, as one behind QSPI. A slide: every frame the two pictures at its offset, each pixel
 * written once; the last the new screen whole; after it dgx_ui_paint() writes nothing, a label of the page changed
 * meanwhile drawn by that pass alone, the clock under the page changed meanwhile drawn when the home shows again. A
 * crossfade: a middle frame as DGX mixes the two pictures at its alpha, in the frame buffer and strip by strip in the
 * paint's buffer. A look of the program's own that counts the painter's own passes: the last frame shows the count
 * the trees are marked as showing, `to` composed after the telling pass. Back by the key, by a button that calls dgx_ui_page_back(), and by one that calls
 * dgx_ui_page_back_by() with the root's on(DGX_UI_BACK) calling it too: one transition each, composed once. A
 * transition asked for while one runs starts from the trees as they stand. At once, the page changed: no pictures,
 * no time, a picture of another size or depth, the keyboard's band open, a crossfade on the panel with no buffer.
 * wake_ms 0 while the frames run.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_ui_transition.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

/* a place for the value of a widget made at run time, a new one each time, 0 in it */
static int32_t  place_pool[256];
static int      place_next;
static int32_t *new_place(void)
{
    int32_t *place = &place_pool[place_next++ % 256];
    *place         = 0;
    return place;
}

static uint32_t              colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};
static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;

static dgx_ui_t            home, settings;
static dgx_ui_node_t       home_nodes[4], settings_nodes[6];
static int                 clock_label, page_label, page_list;
static dgx_ui_paint_t      paint;
static dgx_ui_transition_t tr;

/* how the next page comes, and how the settings go back */
static int kind = DGX_UI_SLIDE_LEFT;
enum { BACK_PLAIN, BACK_BY };
static int back_button_way = BACK_PLAIN, root_back_by = 1;
static int told_back;

/* the home's widget that counts its composings: a picture of the home composed once a transition */
static int  composed;
static bool draw_counter(const dgx_ui_look_t *look, dgx_ui_paint_t *p, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *node = dgx_ui_node(ui, i);
    if (p->composing) ++composed;
    dgx_ui_lay_paper(p, DGX_UI_PAPER, node->x, node->y, node->w, node->h);
    return false;
}
static const dgx_ui_look_t counter_look = {.draw = draw_counter, .fill = -1};

/* a look of the program's own that keeps a count of the painter's own passes over it, and draws it as a bar: a
   picture composed before the telling pass would show one less than the trees are marked as showing */
static int  own_passes;
static bool draw_passes(const dgx_ui_look_t *look, dgx_ui_paint_t *p, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *node = dgx_ui_node(ui, i);
    if (!p->composing && p->part + 1 >= p->parts) ++own_passes;
    const int bar = own_passes % node->w;
    dgx_ui_lay_paper(p, DGX_UI_PAPER, node->x, node->y, node->w, node->h);
    dgx_fill_rectangle(p->scr, node->x - p->ox, node->y - p->oy, bar, node->h, dgx_ui_color(p, 3));
    return false;
}
static const dgx_ui_look_t passes_look = {.draw = draw_passes, .fill = -1};

static bool take_time(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what != 21) return false;
    dgx_ui_text_set(ui, i, "12:01");
    return true;
}

enum { OPEN = 1, SETTINGS_ROOT, BACK };
static void on_any(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    const int tag = node->widget->tag;
    if (tag == OPEN && what == DGX_UI_ACTIVATED) dgx_ui_page_open_by(&home, &settings, &paint, &tr, kind);
    if (tag == BACK && what == DGX_UI_ACTIVATED) {
        if (back_button_way == BACK_PLAIN) dgx_ui_page_back(&home);
        else dgx_ui_page_back_by(&home, &paint, &tr, DGX_UI_SLIDE_RIGHT);
    }
    if (tag == SETTINGS_ROOT && what == DGX_UI_BACK) {
        ++told_back;
        if (root_back_by) dgx_ui_page_back_by(&home, &paint, &tr, DGX_UI_SLIDE_RIGHT);
    }
}

static char             clock_buf[16], label_buf[16], list_buf[64];
static dgx_ui_textbox_t clock_box, label_box, list_box;

static void make_trees(void)
{
    static const dgx_ui_widget_t home_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
    static const dgx_ui_widget_t open_w      = {.kind = DGX_UI_BUTTON, .want_h = 10, .look = &dgx_ui_look_button, .tag = OPEN, .on = on_any};
    static int32_t counter_w_place; /* its value */
    static const dgx_ui_widget_t counter_w   = {.kind = DGX_UI_OWN, .value = &counter_w_place, .want_h = 2, .look = &counter_look};
    static const dgx_ui_widget_t root_w      = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .tag = SETTINGS_ROOT, .on = on_any};
    static const dgx_ui_widget_t back_w      = {.kind = DGX_UI_BUTTON, .want_h = 10, .look = &dgx_ui_look_button, .tag = BACK, .on = on_any};
    static int32_t passes_w_place; /* its value */
    static const dgx_ui_widget_t passes_w    = {.kind = DGX_UI_OWN, .value = &passes_w_place, .want_h = 2, .look = &passes_look};
    static dgx_ui_widget_t       clock_w, label_w, list_w;
    dgx_ui_textbox_init(&clock_box, clock_buf, sizeof clock_buf, 1, &style);
    dgx_ui_textbox_init(&label_box, label_buf, sizeof label_buf, 1, &style);
    dgx_ui_textbox_init(&list_box, list_buf, sizeof list_buf, 1, &style);
    clock_w = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &clock_box, .want_h = 13, .take = take_time};
    label_w = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &label_box, .want_h = 13};
    list_w  = (dgx_ui_widget_t){.kind = DGX_UI_LIST, .value = new_place(), .text = &list_box, .max = 11, .want_h = 26, .look = &dgx_ui_look_list};
    dgx_ui_init(&home, home_nodes, 4, 1);
    dgx_ui_add(&home, -1, &home_root_w);
    clock_label = dgx_ui_add(&home, 0, &clock_w);
    dgx_ui_add(&home, 0, &open_w);
    dgx_ui_add(&home, 0, &counter_w);
    dgx_ui_text_set(&home, clock_label, "12:00");
    dgx_ui_init(&settings, settings_nodes, 6, 1);
    dgx_ui_add(&settings, -1, &root_w);
    dgx_ui_add(&settings, 0, &back_w);
    page_label = dgx_ui_add(&settings, 0, &label_w);
    page_list  = dgx_ui_add(&settings, 0, &list_w);
    dgx_ui_add(&settings, 0, &passes_w);
    dgx_ui_text_set(&settings, page_label, "volume");
    dgx_ui_text_set(&settings, page_list, "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl");
}

static void click(dgx_ui_t *ui, int x, int y, uint32_t ms)
{
    dgx_ui_pointer(ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .down = true, .pressed = true, .ms = ms});
    dgx_ui_pointer(ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .released = true, .ms = ms + 50});
}

/* a pixel of the display: of the panel's memory, or of a virtual screen */
static panel_t *panel;
static uint32_t pixel_of(dgx_screen_t *screen, int x, int y) { return screen == &panel->sbus.scr ? panel->mem[y][x] : dgx_get_pixel(screen, x, y); }

/* pixels of the display unlike a picture, or the slide's two pictures at the offset of `elapsed`, to the left or to the
   right */
static int unlike(dgx_screen_t *screen, const dgx_screen_t *expected)
{
    int count = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) count += pixel_of(screen, x, y) != dgx_get_pixel((dgx_screen_t *)expected, x, y);
    return count;
}
/* the same outside a widget's box */
static int unlike_outside(dgx_screen_t *screen, const dgx_screen_t *expected, const dgx_ui_node_t *box)
{
    int count = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            const bool in_box = x >= box->x && x < box->x + box->w && y >= box->y && y < box->y + box->h;
            count += !in_box && pixel_of(screen, x, y) != dgx_get_pixel((dgx_screen_t *)expected, x, y);
        }
    return count;
}
static int unlike_slide(dgx_screen_t *screen, uint32_t elapsed)
{
    const int shift = (int)((uint32_t)PANEL_W * elapsed / tr.ms);
    int       count = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            uint32_t want;
            if (tr.kind == DGX_UI_SLIDE_LEFT) want = x < PANEL_W - shift ? dgx_get_pixel(tr.from, x + shift, y) : dgx_get_pixel(tr.to, x - (PANEL_W - shift), y);
            else want = x < shift ? dgx_get_pixel(tr.to, x + PANEL_W - shift, y) : dgx_get_pixel(tr.from, x - shift, y); /* to the right */
            count += pixel_of(screen, x, y) != want;
        }
    return count;
}

/* the trees composed now, as the next pass would show them */
static dgx_screen_t *fresh;
static void          compose_fresh(void) { dgx_ui_paint_area_to(&home, &paint, fresh, NULL, 0, 0, PANEL_W, PANEL_H); }

/* the frames to the end from `start`, every 20 ms: the faults of a slide's frames on the screen, its frames counted */
static int run_slide(uint32_t start, int *frames, int *wake_faults)
{
    int faults = 0;
    *frames = *wake_faults = 0;
    for (uint32_t now_ms = start; tr.running; now_ms += 20, ++*frames) {
        if (paint.scr == &panel->sbus.scr) panel_count_from_now(panel);
        CHECK(dgx_ui_transition_frame(&tr, &paint, now_ms));
        *wake_faults += paint.wake_ms != 0;
        if (tr.running) faults += unlike_slide(paint.scr, now_ms - start);
        else faults += unlike(paint.scr, tr.to);
        if (paint.scr == &panel->sbus.scr) faults += panel_twice(panel) + (panel_written(panel) != PANEL_W * PANEL_H);
    }
    return faults;
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
    make_trees();
    panel                  = panel_make();
    dgx_screen_t *readable = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    fresh                  = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    tr = (dgx_ui_transition_t){.from = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), .to = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), .ms = 200};
    static uint8_t buffer[PANEL_W * 2 * 8];
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
    dgx_ui_paint(&home, &paint, 0);

    /* a slide on the panel: the list's pick far down set before; every frame the two pictures at its offset, each pixel
       once; the last the new screen, as the trees are marked: the next pass writes nothing */
    dgx_ui_set_value(&settings, page_list, 11);
    click(&home, 10, 20, 100);
    CHECK(home.popup == &settings && tr.running);
    int frames, wake_faults;
    CHECK(run_slide(1000, &frames, &wake_faults) == 0 && frames >= 10 && wake_faults == 0);
    compose_fresh();
    CHECK(unlike(paint.scr, fresh) == 0);
    panel_count_from_now(panel);
    dgx_ui_paint(&home, &paint, 1300);
    CHECK(panel_written(panel) == 0);

    /* changed during the frames: the page's label drawn by the first pass after, alone; the clock under the page
       when the home shows again */
    composed = 0;
    tr.kind  = DGX_UI_SLIDE_RIGHT;
    root_back_by = 0;
    dgx_ui_page_back(&home); /* at once: the plain call */
    root_back_by = 1;
    dgx_ui_paint(&home, &paint, 1400);
    dgx_ui_set_value(&settings, page_list, 0);
    click(&home, 10, 20, 1500);
    CHECK(tr.running);
    dgx_ui_transition_frame(&tr, &paint, 1600);
    dgx_ui_text_set(&settings, page_label, "louder");
    dgx_ui_msg_t msg = {.what = 21};
    CHECK(dgx_ui_take(&home, &msg));
    for (uint32_t now_ms = 1620; tr.running; now_ms += 20) dgx_ui_transition_frame(&tr, &paint, now_ms);
    panel_count_from_now(panel);
    dgx_ui_paint(&home, &paint, 2000);
    const dgx_ui_node_t *label_node = dgx_ui_node(&settings, page_label);
    CHECK(panel_written(panel) > 0 && panel_written_outside(panel, label_node->x, label_node->y, label_node->x + label_node->w, label_node->y + label_node->h) == 0);
    compose_fresh();
    CHECK(unlike(paint.scr, fresh) == 0);
    root_back_by = 0;
    dgx_ui_page_back(&home);
    root_back_by = 1;
    dgx_ui_paint(&home, &paint, 2100);
    compose_fresh();
    CHECK(unlike(paint.scr, fresh) == 0 && strcmp(clock_buf, "12:01") == 0);

    /* a crossfade: a middle frame as DGX mixes the two pictures at its alpha, on the readable screen and on the panel
       through the paint's buffer */
    dgx_screen_t *mixed     = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_screen_t *screens[] = {readable, &panel->sbus.scr};
    for (int k = 0; k < 2; ++k) {
        paint.scr = screens[k];
        dgx_ui_redraw(&home, 0);
        dgx_ui_paint(&home, &paint, 2200 + (uint32_t)k * 1000);
        kind = DGX_UI_CROSSFADE;
        click(&home, 10, 20, 2300 + (uint32_t)k * 1000);
        CHECK(tr.running && tr.kind == DGX_UI_CROSSFADE);
        const uint32_t start = 2400 + (uint32_t)k * 1000;
        dgx_ui_transition_frame(&tr, &paint, start);
        dgx_ui_transition_frame(&tr, &paint, start + 100);
        dgx_vscreen_to_screen(mixed, 0, 0, tr.from, 0);
        dgx_vscreen_to_screen(mixed, 0, 0, tr.to, DGX_VSCREEN_ALPHA(255u * 100 / tr.ms));
        CHECK(unlike(paint.scr, mixed) == 0 && unlike(paint.scr, tr.from) > 0);
        for (uint32_t now_ms = start + 120; tr.running; now_ms += 20) dgx_ui_transition_frame(&tr, &paint, now_ms);
        CHECK(unlike(paint.scr, tr.to) == 0);
        root_back_by = 0;
        dgx_ui_page_back(&home);
        root_back_by = 1;
    }
    mixed->destroy(&mixed);
    paint.scr = &panel->sbus.scr;
    kind      = DGX_UI_SLIDE_LEFT;
    dgx_ui_redraw(&home, 0);
    dgx_ui_paint(&home, &paint, 4500);

    /* back three ways, one transition each, the home composed once into `to`: the key with the root's on() calling
       dgx_ui_page_back_by(); a button calling dgx_ui_page_back(); a button calling dgx_ui_page_back_by() with the
       root's on() calling it too */
    const int ways[3][2] = {{-1, 1}, {BACK_PLAIN, 1}, {BACK_BY, 1}};
    for (int k = 0; k < 3; ++k) {
        click(&home, 10, 20, 5000 + (uint32_t)k * 1000);
        for (uint32_t now_ms = 5100 + (uint32_t)k * 1000; tr.running; now_ms += 20) dgx_ui_transition_frame(&tr, &paint, now_ms);
        dgx_ui_paint(&home, &paint, 5400 + (uint32_t)k * 1000);
        back_button_way = ways[k][0] < 0 ? BACK_PLAIN : ways[k][0];
        root_back_by    = ways[k][1];
        composed = told_back = 0;
        if (ways[k][0] < 0) CHECK(dgx_ui_key(&home, DGX_UI_KEY_BACK));
        else click(&home, 10, 6, 5500 + (uint32_t)k * 1000);
        CHECK(!home.popup && tr.running && told_back == 1 && composed == 1);
        /* `from` composed with the page still open: what the display shows, but the "back" button the finger pressed
           and focused, which no pass has drawn so yet */
        CHECK(unlike_outside(paint.scr, tr.from, dgx_ui_node(&settings, 1)) == 0 && unlike(paint.scr, tr.to) > 0);
        CHECK(run_slide(5600 + (uint32_t)k * 1000, &frames, &wake_faults) == 0 && frames >= 10);
    }
    back_button_way = BACK_PLAIN;
    dgx_ui_paint(&home, &paint, 9000);

    /* asked for while one runs: back from the middle of the open's frames, from the trees as they stand */
    click(&home, 10, 20, 9100);
    dgx_ui_transition_frame(&tr, &paint, 9200);
    dgx_ui_transition_frame(&tr, &paint, 9260);
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_BACK) && !home.popup && tr.running && !tr.started);
    for (uint32_t now_ms = 9300; tr.running; now_ms += 20) dgx_ui_transition_frame(&tr, &paint, now_ms);
    compose_fresh();
    CHECK(unlike(paint.scr, fresh) == 0);
    dgx_ui_paint(&home, &paint, 9600);

    /* at once, the page changed, a line in the log: no pictures, no time, a picture of another size or depth, the
       band open, a crossfade on the panel with no buffer */
    dgx_screen_t *small = dgx_vscreen_init(PANEL_W - 1, PANEL_H, 16, DgxScreenRGB), *shallow = dgx_vscreen_init(PANEL_W, PANEL_H, 8, DgxScreenRGB);
    dgx_screen_t *const kept_from = tr.from;
    for (int k = 0; k < 6; ++k) {
        const uint16_t kept_ms = tr.ms;
        if (k == 0) tr.from = NULL;
        if (k == 1) tr.ms = 0;
        if (k == 2) tr.from = small;
        if (k == 3) tr.from = shallow;
        if (k == 4) paint.cover = dgx_ui_rect(0, 40, PANEL_W, 24);
        if (k == 5) kind = DGX_UI_CROSSFADE, paint.buffer = NULL;
        click(&home, 10, 20, 10000 + (uint32_t)k * 1000);
        CHECK(home.popup == &settings && !tr.running);
        tr.from = kept_from, tr.ms = kept_ms, paint.cover = dgx_ui_rect(0, 0, 0, 0), kind = DGX_UI_SLIDE_LEFT, paint.buffer = buffer;
        root_back_by = 0;
        dgx_ui_page_back(&home);
        root_back_by = 1;
        dgx_ui_paint(&home, &paint, 10500 + (uint32_t)k * 1000);
    }
    small->destroy(&small), shallow->destroy(&shallow);

    dgx_ui_paint_free(&paint);
    tr.from->destroy(&tr.from), tr.to->destroy(&tr.to);
    readable->destroy(&readable), fresh->destroy(&fresh);
    free(panel);
    CHECK_DONE();
}
