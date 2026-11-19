/*
 * Pages and the key back: dgx_ui_page_open(), dgx_ui_page_back(), dgx_ui_popup_back() and the popup flags
 * DGX_UI_POPUP_BACK and DGX_UI_POPUP_SCREEN. A page opened by a button and pressed before any pass: its own. Three
 * pages closed by the key back and by the page's "back" button, each root told DGX_UI_BACK once while still open, its
 * widgets read then; on the home, back is the program's. The finger and the keys to the top page only; a message
 * reaches a page under the top; the ninth popup refused. A page opened before the first pass laid at it, and laid again
 * when the screen turns; a tree that was a popup of its own rectangle, opened as a page, on the whole screen. A dropdown's list over a page closed by back first, its pick kept; a page opened over a list
 * closes it, over a modal dialog leaves it under; that dialog, not closed by back, keeps back the program's and the
 * page under it open. The root's on(DGX_UI_BACK) closing the page or calling dgx_ui_page_back() again: closed once,
 * told once; opening another over it: both stay, the program's. A popup without the flag: back the program's. A widget of
 * the page's own taking back: the page stays. Told by dgx_ui_changed(): a page closed is the screen.
 */
#include <string.h>

#include "check.h"
#include "dgx_ui.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"

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

enum { HOME_ROOT = 1, OPEN, SETTINGS_ROOT, BACK, SLIDER, DROPDOWN, OWN, READING, SECOND_ROOT, THIRD_ROOT, DIALOG_ROOT, DEEP_ROOT, TAGS };
static int told_back[TAGS], told_activated[TAGS], taken;

static dgx_ui_t      home, settings, second, third, dialog, deep[9];
static dgx_ui_node_t home_nodes[3], settings_nodes[7], second_nodes[2], third_nodes[2], dialog_nodes[1], deep_nodes[9][1];
static int           slider, dropdown, own;

/* what the settings' root does when told it goes */
enum { STAY, CLOSE_ITSELF, OPEN_ANOTHER, BACK_AGAIN };
static int  back_mode;
static bool open_when_told;
static int  slider_seen;

static void on_any(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    const int tag = node->widget->tag;
    if (what == DGX_UI_BACK) ++told_back[tag];
    if (what == DGX_UI_ACTIVATED) ++told_activated[tag];
    if (tag == OPEN && what == DGX_UI_ACTIVATED) dgx_ui_page_open(&home, &settings);
    if (tag == BACK && what == DGX_UI_ACTIVATED) CHECK(dgx_ui_page_back(&home)); /* the page's own "back" */
    if (tag == SETTINGS_ROOT && what == DGX_UI_BACK) {
        open_when_told = settings.level > 0;
        slider_seen    = dgx_ui_value_of(dgx_ui_node(&settings, slider));
        if (back_mode == CLOSE_ITSELF) dgx_ui_popup_close(&home);
        if (back_mode == OPEN_ANOTHER) dgx_ui_page_open(&home, &third);
        if (back_mode == BACK_AGAIN) CHECK(dgx_ui_page_back(&home));
    }
}

static bool take_reading(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what != 77) return false;
    ++taken;
    return true;
}

/* a widget of the page's own that takes the key back itself */
static bool own_key(dgx_ui_t *ui, int i, int key) { return key == DGX_UI_KEY_BACK; }
static bool draw_nothing(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms) { return false; }
static const dgx_ui_behavior_t own_behavior = {.flags = DGX_UI_BEHAVIOR_FOCUS, .key = own_key};
static const dgx_ui_look_t     own_look     = {.draw = draw_nothing, .fill = -1, .behavior = &own_behavior};

static char             dropdown_buf[16], reading_buf[16];
static dgx_ui_textbox_t dropdown_box, reading_box;

static void make_trees(void)
{
    static const dgx_ui_widget_t home_root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2, .tag = HOME_ROOT, .on = on_any};
    static const dgx_ui_widget_t open_w        = {.kind = DGX_UI_BUTTON, .want_h = 10, .look = &dgx_ui_look_button, .tag = OPEN, .on = on_any};
    static const dgx_ui_widget_t settings_w    = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .tag = SETTINGS_ROOT, .on = on_any};
    static const dgx_ui_widget_t back_w        = {.kind = DGX_UI_BUTTON, .want_h = 10, .look = &dgx_ui_look_button, .tag = BACK, .on = on_any};
    static int32_t slider_w_place; /* its value */
    static const dgx_ui_widget_t slider_w      = {.kind = DGX_UI_SLIDER, .value = &slider_w_place, .max = 100, .want_h = 8, .tag = SLIDER, .on = on_any};
    static int32_t own_w_place; /* its value */
    static const dgx_ui_widget_t own_w         = {.kind = DGX_UI_OWN, .value = &own_w_place, .want_h = 4, .look = &own_look, .tag = OWN, .on = on_any};
    static const dgx_ui_widget_t second_w      = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .tag = SECOND_ROOT, .on = on_any};
    static const dgx_ui_widget_t third_w       = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .tag = THIRD_ROOT, .on = on_any};
    static const dgx_ui_widget_t small_w       = {.kind = DGX_UI_BUTTON, .want_h = 10, .look = &dgx_ui_look_button, .on = on_any};
    static const dgx_ui_widget_t dialog_w      = {.kind = DGX_UI_FRAME, .tag = DIALOG_ROOT, .on = on_any};
    static const dgx_ui_widget_t deep_w        = {.kind = DGX_UI_FRAME, .tag = DEEP_ROOT, .on = on_any};
    static dgx_ui_widget_t       dropdown_w, reading_w;
    dgx_ui_textbox_init(&dropdown_box, dropdown_buf, sizeof dropdown_buf, 1, &style);
    dgx_ui_textbox_init(&reading_box, reading_buf, sizeof reading_buf, 1, &style);
    dropdown_w = (dgx_ui_widget_t){.kind = DGX_UI_DROPDOWN, .value = new_place(), .text = &dropdown_box, .max = 2, .want_h = 12, .look = &dgx_ui_look_dropdown, .tag = DROPDOWN, .on = on_any};
    reading_w  = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &reading_box, .want_h = 6, .tag = READING, .take = take_reading};
    strcpy(dropdown_buf, "a\nb\nc");
    dgx_ui_init(&home, home_nodes, 3, 1);
    dgx_ui_add(&home, -1, &home_root_w);
    dgx_ui_add(&home, 0, &open_w);
    dgx_ui_init(&settings, settings_nodes, 7, 1);
    dgx_ui_add(&settings, -1, &settings_w);
    dgx_ui_add(&settings, 0, &back_w);
    slider   = dgx_ui_add(&settings, 0, &slider_w);
    dropdown = dgx_ui_add(&settings, 0, &dropdown_w);
    own      = dgx_ui_add(&settings, 0, &own_w);
    dgx_ui_add(&settings, 0, &reading_w);
    dgx_ui_init(&second, second_nodes, 2, 1);
    dgx_ui_add(&second, -1, &second_w);
    dgx_ui_add(&second, 0, &small_w);
    dgx_ui_init(&third, third_nodes, 2, 1);
    dgx_ui_add(&third, -1, &third_w);
    dgx_ui_add(&third, 0, &small_w);
    dgx_ui_init(&dialog, dialog_nodes, 1, 1);
    dgx_ui_add(&dialog, -1, &dialog_w);
    for (int k = 0; k < 9; ++k) dgx_ui_init(&deep[k], deep_nodes[k], 1, 1), dgx_ui_add(&deep[k], -1, &deep_w);
}

static void click(dgx_ui_t *ui, int x, int y, uint32_t ms)
{
    dgx_ui_pointer(ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .down = true, .pressed = true, .ms = ms});
    dgx_ui_pointer(ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .released = true, .ms = ms + 50});
}

static bool screen_rect(const dgx_ui_t *tree, int w, int h)
{
    return tree->root_rect.x == 0 && tree->root_rect.y == 0 && tree->root_rect.w == w && tree->root_rect.h == h;
}

static int  told_count;
static void count_told(void *ctx, dgx_ui_rect_t rect)
{
    dgx_ui_rect_t *last = ctx;
    *last               = rect, ++told_count;
}

static void reset_told(void) { memset(told_back, 0, sizeof told_back), memset(told_activated, 0, sizeof told_activated); }

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    style      = (dgx_ui_text_style_t){.variants = variant, .count = 1};
    make_trees();
    dgx_screen_t  *screen = dgx_vscreen_init(96, 64, 16, DgxScreenRGB);
    dgx_ui_paint_t paint  = {.scr = screen, .colors = colors, .color_count = 4};
    dgx_ui_paint(&home, &paint, 0);

    /* the home's button opens the settings; no pass; the page's "back" pressed and let go at once: the page's, so
       activated, and the page closed by it, its root told first while open, the slider read then */
    dgx_ui_set_value(&settings, slider, 42);
    click(&home, 10, 6, 100);
    CHECK(home.popup == &settings && screen_rect(&settings, 96, 64));
    click(&home, 10, 6, 300);
    CHECK(told_activated[BACK] == 1 && told_back[SETTINGS_ROOT] == 1 && open_when_told && slider_seen == 42 && !home.popup);

    /* three pages by the calls; the key back closes the top one, its root told; on the home back is the program's */
    reset_told();
    CHECK(dgx_ui_page_open(&home, &settings) && dgx_ui_page_open(&home, &second) && dgx_ui_page_open(&home, &third));
    CHECK(home.popup == &settings && settings.popup == &second && second.popup == &third);
    dgx_ui_paint(&home, &paint, 400);
    /* the finger and the keys to the top only: home's button not pressed, second's focus untouched */
    click(&home, 10, 6, 500);
    CHECK(told_activated[OPEN] == 0 && second.focus == -1);
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_NEXT) && third.focus >= 0 && second.focus == -1 && settings.focus == -1);
    /* a message reaches the settings under two pages */
    dgx_ui_msg_t msg = {.what = 77};
    CHECK(dgx_ui_take(&home, &msg) && taken == 1);
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_BACK) && second.popup == NULL && told_back[THIRD_ROOT] == 1);
    CHECK(dgx_ui_page_back(&home) && settings.popup == NULL && told_back[SECOND_ROOT] == 1);
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_BACK) && home.popup == NULL && told_back[SETTINGS_ROOT] == 1);
    CHECK(!dgx_ui_key(&home, DGX_UI_KEY_BACK) && !dgx_ui_page_back(&home));

    /* the ninth popup refused */
    int opened = 0;
    for (int k = 0; k < 9; ++k) opened += dgx_ui_page_open(&home, &deep[k]);
    CHECK(opened == 8);
    dgx_ui_popup_close(&home);

    /* a widget of the page's own taking back: the page stays */
    CHECK(dgx_ui_page_open(&home, &settings));
    dgx_ui_focus(&settings, own);
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_BACK) && home.popup == &settings);
    dgx_ui_focus(&settings, -1);

    /* a dropdown's list over the page: back closes the list first, the pick kept; back again the page */
    static dgx_ui_dropdown_t dd;
    dgx_ui_set_value(&settings, dropdown, 1);
    dgx_ui_paint(&home, &paint, 600);
    CHECK(dgx_ui_dropdown_open(&settings, dropdown, &dd, NULL, NULL) && settings.popup == &dd.tree);
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_BACK) && settings.popup == NULL && home.popup == &settings);
    CHECK(dgx_ui_value_of(dgx_ui_node(&settings, dropdown)) == 1);
    /* a page opened over a list: the list closed, the pick kept, the new page on top; back shows the page, no list */
    CHECK(dgx_ui_dropdown_open(&settings, dropdown, &dd, NULL, NULL));
    CHECK(dgx_ui_page_open(&home, &second) && settings.popup == &second && dd.tree.level == 0);
    CHECK(dgx_ui_page_back(&home) && home.popup == &settings && settings.popup == NULL && dgx_ui_value_of(dgx_ui_node(&settings, dropdown)) == 1);
    /* a modal dialog that is not closed outside: under the new page, and there again after back */
    CHECK(dgx_ui_popup_open(&settings, &dialog, 10, 10, 40, 20, DGX_UI_POPUP_MODAL));
    CHECK(dgx_ui_page_open(&home, &second) && settings.popup == &dialog && dialog.popup == &second);
    CHECK(dgx_ui_page_back(&home) && settings.popup == &dialog && dialog.popup == NULL);
    /* a modal popup without the flag over the page: back the program's, the dialog and the page under it stay */
    CHECK(!dgx_ui_key(&home, DGX_UI_KEY_BACK) && settings.popup == &dialog);
    dgx_ui_popup_close(&settings);
    dgx_ui_popup_close(&home);
    /* a tree opened first as a popup of its own rectangle, then as a page: the whole screen */
    CHECK(dgx_ui_popup_open(&home, &dialog, 10, 10, 40, 20, DGX_UI_POPUP_MODAL) && dialog.root_rect.w == 40);
    dgx_ui_popup_close(&home);
    CHECK(dgx_ui_page_open(&home, &dialog) && screen_rect(&dialog, 96, 64));
    dgx_ui_paint(&home, &paint, 650);
    CHECK(screen_rect(&dialog, 96, 64) && dgx_ui_node(&dialog, 0)->w == 96);
    dgx_ui_popup_close(&home);

    /* the root's on(DGX_UI_BACK) closing the page, opening another, calling dgx_ui_page_back() again: told once,
       closed once */
    const int modes[3] = {CLOSE_ITSELF, OPEN_ANOTHER, BACK_AGAIN};
    for (int k = 0; k < 3; ++k) {
        reset_told();
        back_mode = modes[k];
        CHECK(dgx_ui_page_open(&home, &settings));
        CHECK(dgx_ui_key(&home, DGX_UI_KEY_BACK) && told_back[SETTINGS_ROOT] == 1);
        CHECK(modes[k] == OPEN_ANOTHER ? home.popup == &settings && settings.popup == &third : home.popup == NULL); /* the program's then */
        dgx_ui_popup_close(&home);
    }
    back_mode = STAY;

    /* opened before the first pass: laid at it; the screen turned: laid again */
    {
        static dgx_ui_t      fresh_home;
        static dgx_ui_node_t fresh_nodes[1];
        static const dgx_ui_widget_t box_w = {.kind = DGX_UI_BOX};
        dgx_ui_init(&fresh_home, fresh_nodes, 1, 1);
        dgx_ui_add(&fresh_home, -1, &box_w);
        CHECK(dgx_ui_page_open(&fresh_home, &second) && second.root_rect.w == 0);
        dgx_ui_paint_t fresh_paint = {.scr = screen, .colors = colors, .color_count = 4};
        dgx_ui_paint(&fresh_home, &fresh_paint, 0);
        CHECK(screen_rect(&second, 96, 64));
        dgx_screen_t *turned = dgx_vscreen_init(64, 96, 16, DgxScreenRGB);
        fresh_paint.scr      = turned;
        dgx_ui_paint(&fresh_home, &fresh_paint, 10);
        CHECK(screen_rect(&second, 64, 96) && dgx_ui_node(&second, 0)->w == 64 && dgx_ui_node(&second, 0)->h == 96);
        dgx_ui_popup_close(&fresh_home);
        /* a tree that was a popup of its own rectangle, opened as a page, follows a turn too */
        CHECK(dgx_ui_popup_open(&fresh_home, &third, 5, 5, 30, 30, DGX_UI_POPUP_MODAL));
        dgx_ui_popup_close(&fresh_home);
        CHECK(dgx_ui_page_open(&fresh_home, &third) && screen_rect(&third, 64, 96));
        fresh_paint.scr = screen;
        dgx_ui_paint(&fresh_home, &fresh_paint, 20);
        CHECK(screen_rect(&third, 96, 64) && dgx_ui_node(&third, 0)->w == 96);
        dgx_ui_popup_close(&fresh_home);
        dgx_ui_paint_free(&fresh_paint), dgx_ui_free(&fresh_home);
        turned->destroy(&turned);
    }

    /* told by dgx_ui_changed(): a page closed is one rectangle, the screen */
    CHECK(dgx_ui_page_open(&home, &settings));
    dgx_ui_rect_t last = {0};
    dgx_ui_changed(&home, &paint, 700, count_told, &last);
    CHECK(dgx_ui_page_back(&home));
    told_count = 0;
    dgx_ui_changed(&home, &paint, 800, count_told, &last);
    CHECK(told_count == 1 && last.x == 0 && last.y == 0 && last.w == 96 && last.h == 64);

    dgx_ui_paint_free(&paint);
    screen->destroy(&screen);
    CHECK_DONE();
}
