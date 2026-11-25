/*
 * Pages in home's place (dgx_ui_page_place()): a bar over the pages, its clock, its button back, a tab and a dropdown,
 * and a line under it that is hidden and shown. A page opened in the place writes the place alone; the clock changed
 * under it writes the clock alone; a finger on the bar reaches home's widgets and one on the page the page's; the keys
 * go to the page; the line hidden, a finger at once, before any pass, meets the page where the place is now, and a
 * composing shows it there. Two pages in the place: the lower one covered, home shown throughout; a dialog of the page
 * over the bar takes the finger there; a page of the whole screen over the place covers home too. The place removed,
 * and named -1, with a page open: the page on the screen, home told it is covered; named again: shown. The bar's
 * dropdown opens its list over the page and closes it alone; a dropdown of home under a hint that stays keeps the
 * hint; the list closed by the key back and by a finger outside, the list alone. A tab: back and open from its on_event()
 * while the finger is down, the finger let go after. A transition in the place, begun while a button of the bar is
 * pressed: no pixel outside the place written by its frames. After each pass no pixel twice and the panel the chain as
 * a fresh paint composes it, on 16 bits straight and 18 through the strips (PANEL_BITS, by the Makefile).
 */
#define PANEL_W 160
#define PANEL_H 120

#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_ui_transition.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t       colors[4];
static panel_t       *panel;
static dgx_screen_t  *memory;
static uint8_t        buffer[PANEL_W * 3 * 8];
static dgx_ui_paint_t paint;

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style = {.fonts = variant, .count = 1};

/* the trees, and what their roots were told */
enum { HOME, P1, P2, DIALOG, ALARM, HINT, TREES };
static const char *const names[TREES] = {"home", "P1", "P2", "D", "A", "H"};
static dgx_ui_t          home, p1, p2, dialog, alarm, hint;
static dgx_ui_t *const   trees[TREES] = {&home, &p1, &p2, &dialog, &alarm, &hint};
static char              log_text[400];

static int  bar, clock_i, back_i, tab_i, bar_dd, line, place, page_go[TREES]; /* home's widgets, each page's button */
static bool clock_when_shown; /* P2's root told SHOWN sets the clock, outside the place, within the call */
static void on_root(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_EVENT_SHOWN && what != DGX_UI_EVENT_COVERED) return;
    if (clock_when_shown && what == DGX_UI_EVENT_SHOWN && (int)(intptr_t)ctx == P2) clock_when_shown = false, dgx_ui_text_set(&home, clock_i, "12:03");
    char word[16];
    snprintf(word, sizeof word, "%s%s ", names[(int)(intptr_t)ctx], what == DGX_UI_EVENT_SHOWN ? "+" : "-");
    strncat(log_text, word, sizeof log_text - strlen(log_text) - 1);
}

static bool logged(const char *expected)
{
    const bool same = strcmp(log_text, expected) == 0;
    if (!same) fprintf(stderr, "  logged \"%s\", expected \"%s\"\n", log_text, expected);
    log_text[0] = 0;
    return same;
}

/* the program: what its widgets were told */
static int  back_clicked, page_clicked, bar_changed, tabs;
static bool tab_switches; /* the tab's on_event() goes back and opens the other page */
static void on_back(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_ACTIVATED) ++back_clicked, dgx_ui_page_back(&home);
}
static void on_tab(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_EVENT_ACTIVATED || !tab_switches) return;
    ++tabs;
    dgx_ui_t *other = home.popup == &p1 ? &p2 : &p1;
    dgx_ui_page_back(&home);
    dgx_ui_page_open(&home, other);
}
static void on_page_button(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_ACTIVATED) ++page_clicked;
}
static void on_bar_dropdown(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_CHANGED) ++bar_changed;
}

static char             clock_text[8] = "12:00";
static dgx_ui_textbox_t clock_box;
static dgx_ui_textbox_t back_box = DGX_UI_TEXT_CONST("<", 1, &style), tab_box = DGX_UI_TEXT_CONST("Tab", 1, &style);
static dgx_ui_textbox_t options_box = DGX_UI_TEXT_CONST("Aa\nBb\nCc", 1, &style), line_box = DGX_UI_TEXT_CONST("line", 1, &style);
static dgx_ui_textbox_t under_box = DGX_UI_TEXT_CONST("under", 1, &style), go_box = DGX_UI_TEXT_CONST("Go", 1, &style);
static dgx_ui_textbox_t ok_box = DGX_UI_TEXT_CONST("OK", 1, &style), wake_box = DGX_UI_TEXT_CONST("Wake", 1, &style);
static int32_t          bar_option;

static const dgx_ui_widget_t home_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .on_event = on_root, .ctx = (void *)(intptr_t)HOME};
static const dgx_ui_widget_t bar_w       = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 1, .gap = 2, .want_h = 18};
static const dgx_ui_widget_t clock_w     = {.kind = DGX_UI_LABEL, .want_w = 34, .text = &clock_box};
static const dgx_ui_widget_t back_w      = {.kind = DGX_UI_BUTTON, .want_w = 20, .text = &back_box, .look = &dgx_ui_look_button, .on_event = on_back};
static const dgx_ui_widget_t tab_w       = {.kind = DGX_UI_BUTTON, .want_w = 30, .text = &tab_box, .look = &dgx_ui_look_button, .on_event = on_tab};
static const dgx_ui_widget_t bar_dd_w    = {.kind = DGX_UI_DROPDOWN, .value = &bar_option, .max = 2, .want_w = 40, .text = &options_box, .look = &dgx_ui_look_dropdown, .on_event = on_bar_dropdown};
static const dgx_ui_widget_t line_w      = {.kind = DGX_UI_LABEL, .want_h = 14, .text = &line_box};
static const dgx_ui_widget_t place_w     = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN | DGX_UI_REST | DGX_UI_STRETCH};
static const dgx_ui_widget_t under_w     = {.kind = DGX_UI_LABEL, .want_h = 14, .text = &under_box};
static dgx_ui_widget_t       page_root_w[TREES];
static const dgx_ui_widget_t go_w   = {.kind = DGX_UI_BUTTON, .want_w = 40, .want_h = 16, .text = &go_box, .look = &dgx_ui_look_button, .on_event = on_page_button};
static const dgx_ui_widget_t go2_w  = {.kind = DGX_UI_BUTTON, .want_w = 40, .want_h = 16, .text = &go_box, .look = &dgx_ui_look_button, .on_event = on_page_button};
static const dgx_ui_widget_t ok_w   = {.kind = DGX_UI_BUTTON, .want_w = 30, .want_h = 16, .text = &ok_box, .look = &dgx_ui_look_button};
static const dgx_ui_widget_t wake_w = {.kind = DGX_UI_LABEL, .want_h = 14, .text = &wake_box};

static dgx_ui_node_t home_nodes[12], tree_nodes[TREES][4];

static void make_tree(int t, const dgx_ui_widget_t *inside)
{
    dgx_ui_init(trees[t], tree_nodes[t], 4, 1);
    page_root_w[t] = (dgx_ui_widget_t){.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .look = &dgx_ui_look_frame, .on_event = on_root, .ctx = (void *)(intptr_t)t};
    dgx_ui_add(trees[t], -1, &page_root_w[t]);
    page_go[t] = dgx_ui_add(trees[t], 0, inside);
}

static void make_all(void)
{
    dgx_ui_textbox_init(&clock_box, clock_text, sizeof clock_text, 1, &style);
    dgx_ui_init(&home, home_nodes, 12, 1);
    dgx_ui_add(&home, -1, &home_root_w);
    bar     = dgx_ui_add(&home, 0, &bar_w);
    clock_i = dgx_ui_add(&home, bar, &clock_w);
    back_i  = dgx_ui_add(&home, bar, &back_w);
    tab_i   = dgx_ui_add(&home, bar, &tab_w);
    bar_dd  = dgx_ui_add(&home, bar, &bar_dd_w);
    line    = dgx_ui_add(&home, 0, &line_w);
    place   = dgx_ui_add(&home, 0, &place_w);
    dgx_ui_add(&home, place, &under_w);
    make_tree(P1, &go_w), make_tree(P2, &go2_w), make_tree(DIALOG, &ok_w), make_tree(ALARM, &wake_w), make_tree(HINT, &wake_w);
    log_text[0] = 0;
}

static void paint_anew(void)
{
    dgx_ui_paint_free(&paint);
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
    paint.strips = &dgx_ui_strips;
#endif
}

static bool in_rect(dgx_ui_rect_t r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* pixels of the panel written since the count began outside a rectangle */
static int written_outside(dgx_ui_rect_t r)
{
    int count = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) count += panel->writes[y][x] && !in_rect(r, x, y);
    return count;
}

/* the panel against the chain composed anew: pixels that differ */
static int off_fresh(uint32_t now_ms)
{
    dgx_ui_paint_t fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = now_ms};
    dgx_fill_rectangle(memory, 0, 0, PANEL_W, PANEL_H, colors[3]);
    dgx_ui_paint_area_to(&home, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint_free(&fresh);
    int off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) off += panel->mem[y][x] != dgx_get_pixel(memory, x, y);
    return off;
}

/* a pass of home's chain: no pixel twice, nothing written outside `within`, the panel a fresh composing */
static int pass_within(uint32_t now_ms, dgx_ui_rect_t within)
{
    panel_count_from_now(panel);
    dgx_ui_paint(&home, &paint, now_ms);
    return panel_twice(panel) + written_outside(within) + off_fresh(now_ms);
}
static int pass_any(uint32_t now_ms)
{
    return pass_within(now_ms, dgx_ui_rect(0, 0, PANEL_W, PANEL_H));
}
#if PANEL_BITS == 16 /* the transitions' pictures are of 16 bits */
static bool quiet_pass(uint32_t now_ms)
{
    panel_count_from_now(panel);
    dgx_ui_paint(&home, &paint, now_ms);
    return panel_written(panel) == 0;
}
#endif

static dgx_ui_rect_t box_of(const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    return dgx_ui_rect(n->x, n->y, n->w, n->h);
}
static bool same_rect(dgx_ui_rect_t a, dgx_ui_rect_t b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static bool finger(int x, int y, bool down, bool pressed, bool released, uint32_t ms)
{
    const dgx_ui_pointer_t p = {.x = (int16_t)x, .y = (int16_t)y, .down = down, .pressed = pressed, .released = released, .ms = ms};
    return dgx_ui_pointer(&home, &p);
}
static void click_at(dgx_ui_rect_t r, uint32_t ms)
{
    finger(r.x + r.w / 2, r.y + r.h / 2, true, true, false, ms);
    finger(r.x + r.w / 2, r.y + r.h / 2, false, false, true, ms + 30);
}

static uint32_t panel_color(int k)
{
    static dgx_screen_t *probe;
    if (!probe) probe = dgx_vscreen_init(1, 1, PANEL_BITS, DgxScreenRGB);
    dgx_set_pixel(probe, 0, 0, colors[k]);
    return dgx_get_pixel(probe, 0, 0);
}

int main(void)
{
    panel  = panel_make();
    memory = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    (void)panel_color;
    make_all();
    paint_anew();
    CHECK(pass_any(0) == 0);
    uint32_t now = 10;

    /* the place named; a page opened there: the place alone written, home told nothing */
    CHECK(!dgx_ui_page_place(&home, 99));
    CHECK(dgx_ui_page_place(&home, place) && logged(""));
    CHECK(dgx_ui_page_open(&home, &p1) && logged("P1+ "));
    CHECK(!dgx_ui_page_place(&p1, 0)); /* a page names no place of its own */
    CHECK((home.popup_flags & DGX_UI_POPUP_PLACE) && same_rect(p1.root_rect, box_of(&home, place)));
    CHECK(pass_within(now += 10, box_of(&home, place)) == 0);

    /* the clock changed under the page: the clock alone written */
    dgx_ui_text_set(&home, clock_i, "12:01");
    CHECK(pass_within(now += 10, box_of(&home, clock_i)) == 0);

    /* a finger on the page's button is the page's; on the bar's back button home's, which goes back */
    click_at(box_of(&p1, page_go[P1]), now += 10);
    CHECK(page_clicked == 1 && back_clicked == 0);
    CHECK(pass_any(now += 50) == 0);
    click_at(box_of(&home, back_i), now += 10);
    CHECK(back_clicked == 1 && home.popup == NULL && logged("P1- "));
    CHECK(pass_any(now += 50) == 0);

    /* the keys go to the page, not to the bar */
    CHECK(dgx_ui_page_open(&home, &p1) && logged("P1+ "));
    CHECK(pass_any(now += 10) == 0);
    dgx_ui_focus(&home, -1);
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_NEXT) && p1.focus == page_go[P1] && home.focus < 0);
    CHECK(pass_any(now += 10) == 0);

    /* the line hidden: home laid again; a finger at once, before any pass, meets the page where the place is now, and a
       composing at that moment shows it there */
    const dgx_ui_rect_t was_place = box_of(&home, place), was_go = box_of(&p1, page_go[P1]);
    static dgx_screen_t *early;
    if (!early) early = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    dgx_ui_show(&home, line, false);
    const int moved_by = -(int)line_w.want_h; /* the place goes up by the line's height */
    click_at(dgx_ui_rect(was_go.x, was_go.y + moved_by, was_go.w, was_go.h), now += 10); /* before anything lays home */
    CHECK(page_clicked == 2 && same_rect(p1.root_rect, box_of(&home, place)) && box_of(&home, place).y == was_place.y + moved_by);
    CHECK(pass_any(now += 50) == 0 && logged(""));
    dgx_ui_show(&home, line, true);
    CHECK(pass_any(now += 10) == 0);
    dgx_ui_show(&home, line, false); /* and a composing first, before any pass: the page where the pass draws it */
    {
        dgx_ui_paint_t composing = {.scr = early, .colors = colors, .color_count = 4, .now_ms = now};
        dgx_fill_rectangle(early, 0, 0, PANEL_W, PANEL_H, colors[3]);
        dgx_ui_paint_area_to(&home, &composing, early, NULL, 0, 0, PANEL_W, PANEL_H);
        dgx_ui_paint_free(&composing);
    }
    CHECK(same_rect(p1.root_rect, box_of(&home, place)) && !same_rect(box_of(&home, place), was_place));
    CHECK(pass_any(now += 10) == 0);
    int early_off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) early_off += panel->mem[y][x] != dgx_get_pixel(early, x, y);
    CHECK(early_off == 0); /* the composing before any pass showed the page where the pass drew it */
    dgx_ui_show(&home, line, true);
    CHECK(pass_any(now += 10) == 0 && same_rect(p1.root_rect, box_of(&home, place)));

    /* a second page: the first covered, home shown; back to each */
    CHECK(dgx_ui_page_open(&home, &p2) && logged("P1- P2+ "));
    CHECK(same_rect(p2.root_rect, box_of(&home, place)) && pass_within(now += 10, box_of(&home, place)) == 0);
    CHECK(dgx_ui_page_back(&home) && logged("P2- P1+ "));
    CHECK(pass_within(now += 10, box_of(&home, place)) == 0);

    /* a dialog of the page over the bar takes the finger there, even where it does not lie */
    CHECK(dgx_ui_popup_open(&p1, &dialog, 4, 2, 50, 30, DGX_UI_POPUP_MODAL) && logged("D+ "));
    CHECK(pass_any(now += 10) == 0);
    click_at(box_of(&home, back_i), now += 10);
    click_at(box_of(&home, tab_i), now += 10);
    CHECK(back_clicked == 1 && home.popup == &p1 && p1.popup == &dialog);
    dgx_ui_popup_close(&p1);
    CHECK(logged("D- ") && pass_any(now += 50) == 0);

    /* a page of the whole screen over the place: home covered too; gone, shown again */
    CHECK(dgx_ui_popup_open(&p1, &alarm, 0, 0, 0, 0, DGX_UI_POPUP_MODAL | DGX_UI_POPUP_BACK | DGX_UI_POPUP_SCREEN) && logged("P1- home- A+ "));
    CHECK(alarm.root_rect.w == PANEL_W && alarm.root_rect.h == PANEL_H && pass_any(now += 10) == 0);
    dgx_ui_popup_close(&p1);
    CHECK(logged("A- home+ P1+ ") && pass_any(now += 10) == 0);

    /* the place removed with the page open: the page on the screen, home covered, told from within the call; added
       again and named: shown. Named -1 and named again: the same */
    dgx_ui_remove(&home, place);
    CHECK(logged("home- ") && p1.root_rect.w == PANEL_W && p1.root_rect.h == PANEL_H && pass_any(now += 10) == 0);
    place = dgx_ui_add(&home, 0, &place_w);
    dgx_ui_add(&home, place, &under_w);
    CHECK(dgx_ui_page_place(&home, place) && logged("home+ "));
    CHECK(pass_any(now += 10) == 0 && same_rect(p1.root_rect, box_of(&home, place)));
    CHECK(dgx_ui_page_place(&home, -1) && logged("home- ") && pass_any(now += 10) == 0);
    CHECK(dgx_ui_page_place(&home, place) && logged("home+ ") && pass_any(now += 10) == 0);

    /* the bar's dropdown: its list over the page, the page still open and told nothing; the choice written and told
       in home, the list closed alone */
    static dgx_ui_dropdown_t dd;
    CHECK(dgx_ui_dropdown_open(&home, bar_dd, &dd, NULL, NULL) && home.popup == &p1 && p1.popup == &dd.tree && logged(""));
    CHECK(pass_any(now += 10) == 0);
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_NEXT) && dgx_ui_key(&home, DGX_UI_KEY_ACTIVATE));
    CHECK(p1.popup == NULL && home.popup == &p1 && bar_option == 1 && bar_changed == 1 && logged(""));
    CHECK(pass_any(now += 10) == 0);
    /* closed by the key back, and by a finger outside it: the list alone */
    CHECK(dgx_ui_dropdown_open(&home, bar_dd, &dd, NULL, NULL));
    CHECK(pass_any(now += 10) == 0);
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_BACK) && p1.popup == NULL && home.popup == &p1 && logged(""));
    CHECK(pass_any(now += 10) == 0);
    CHECK(dgx_ui_dropdown_open(&home, bar_dd, &dd, NULL, NULL));
    CHECK(pass_any(now += 10) == 0);
    finger(PANEL_W - 2, PANEL_H - 2, true, true, false, now += 10);
    finger(PANEL_W - 2, PANEL_H - 2, false, false, true, now += 10);
    CHECK(p1.popup == NULL && home.popup == &p1 && bar_option == 1 && logged(""));
    CHECK(pass_any(now += 10) == 0);

    /* a tab: back and open from its on_event() while the finger is down on it; let go after, nothing pressed */
    tab_switches = true;
    click_at(box_of(&home, tab_i), now += 10);
    CHECK(tabs == 1 && home.popup == &p2 && home.pressed < 0 && logged("P1- P2+ "));
    CHECK(pass_any(now += 50) == 0);
    click_at(box_of(&home, tab_i), now += 10);
    CHECK(tabs == 2 && home.popup == &p1 && home.pressed < 0 && logged("P2- P1+ "));
    CHECK(pass_any(now += 50) == 0);
    tab_switches = false;

#if PANEL_BITS == 16
    /* a transition in the place, begun while the bar's back button is pressed: its frames write the place alone; the
       clock changed meanwhile is drawn by the pass after; the button, held past a click, is let go with nothing told */
    static dgx_ui_transition_t tr;
    tr = (dgx_ui_transition_t){.from = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), .to = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB), .ms = 100};
    const dgx_ui_rect_t back_box_now = box_of(&home, back_i);
    finger(back_box_now.x + 2, back_box_now.y + 2, true, true, false, now += 10);
    CHECK(pass_any(now += 10) == 0);
    /* first a page whose on(SHOWN) sets the clock: the frames write the place, the last frame the clock too, so that
       the panel is the chain as it stands and the next pass writes nothing */
    clock_when_shown = true;
    CHECK(dgx_ui_page_open_by(&home, &p2, &paint, &tr, DGX_UI_CROSSFADE) && tr.running && logged("P1- P2+ "));
    panel_count_from_now(panel);
    for (uint32_t ms = now + 10; tr.running && ms < now + 2000; ms += 20) dgx_ui_transition_frame(&tr, &paint, ms);
    now += 2000;
    CHECK(!tr.running && written_outside(box_of(&home, place)) == (int)(box_of(&home, clock_i).w * box_of(&home, clock_i).h));
    CHECK(off_fresh(now) == 0 && quiet_pass(now += 10));
    CHECK(dgx_ui_page_back(&home) && logged("P2- P1+ ") && pass_any(now += 10) == 0);
    CHECK(dgx_ui_page_open_by(&home, &p2, &paint, &tr, DGX_UI_SLIDE_LEFT) && tr.running && logged("P1- P2+ "));
    int outside = 0;
    panel_count_from_now(panel);
    for (uint32_t ms = now + 10; tr.running && ms < now + 2000; ms += 20) {
        if (ms == now + 50) dgx_ui_text_set(&home, clock_i, "12:02");
        dgx_ui_transition_frame(&tr, &paint, ms);
    }
    outside = written_outside(box_of(&home, place));
    now += 2000;
    CHECK(!tr.running && outside == 0);
    CHECK(pass_any(now += 10) == 0);
    /* the button held across the frames, past a click: let go, home's, and nothing told */
    CHECK(home.pressed == back_i);
    finger(back_box_now.x + 2, back_box_now.y + 2, false, false, true, now += 10);
    CHECK(back_clicked == 1 && p1.popup == &p2 && home.pressed < 0 && logged(""));
    CHECK(pass_any(now += 10) == 0);
    CHECK(dgx_ui_page_back(&home) && logged("P2- P1+ ") && pass_any(now += 10) == 0);
    dgx_screen_destroy(&tr.from), dgx_screen_destroy(&tr.to);
#endif

    /* a dropdown of home under a hint that stays: the list over the hint, the hint kept after the choice */
    dgx_ui_page_back(&home);
    CHECK(logged("P1- ") && dgx_ui_page_place(&home, -1) && pass_any(now += 10) == 0);
    CHECK(dgx_ui_popup_open(&home, &hint, 100, 60, 50, 24, 0) && logged("H+ "));
    CHECK(pass_any(now += 10) == 0);
    CHECK(dgx_ui_dropdown_open(&home, bar_dd, &dd, NULL, NULL) && home.popup == &hint && hint.popup == &dd.tree);
    CHECK(pass_any(now += 10) == 0);
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_NEXT) && dgx_ui_key(&home, DGX_UI_KEY_ACTIVATE));
    CHECK(home.popup == &hint && hint.popup == NULL && bar_option == 2 && bar_changed == 2 && logged(""));
    CHECK(pass_any(now += 10) == 0);
    dgx_ui_popup_close(&home);
    CHECK(logged("H- ") && pass_any(now += 10) == 0);

    for (int t = TREES - 1; t >= 0; --t) dgx_ui_free(trees[t]);
    dgx_ui_paint_free(&paint);
    CHECK_DONE();
}
