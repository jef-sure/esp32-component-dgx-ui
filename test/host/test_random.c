/*
 * The painter under scripts made by chance, from a fixed set of seeds: values written, widgets shown and hidden,
 * disabled, a text set, a popup opened anywhere and closed, the keyboard's band laid and gone, the finger on the
 * slider, a group of radio buttons chosen by a click, by a key and by the program, a checkbox ticked, the row of them
 * shown and hidden, a radio of the group in the popup, a caption over a constant pointed at another and told, a page
 * opened and gone back from, the popup opened over the top of the chain, a popup closed from the middle of it; a level
 * bar of the library on the slider's place, and a ring scale with a child in it; a picture button and a toggle of two
 * pictures on the switch's place; the button repeating while a finger holds it, the time stepped at every step; an
 * idle of 200 ms named, its 1 and 0 told to home's root by turns, 1 first; home's pages put in a place of it and
 * on the screen again, the place's box removed and added again, by the script and from within the roots' on(), the
 * slider above the place in the bar the pages leave. Each seed in two modes: on the paper straight, and over a drawn background with the strips
 * named. After every pass: no pixel written twice, and the panel is the tree composed anew by a fresh paint, the
 * band's own rows left out. At the end of each seed, with the band gone, the popup closed and the finger up, a pass,
 * then the look at the places marks nothing shown and the next pass writes nothing. After every step: each tree's
 * root told last what a walk of the chain says of it (shown while no page of the screen lies above it, nor one of the
 * place above a tree of the place), its words alternating.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PANEL_W 200
#define PANEL_H 140
#include "check.h"
#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

enum { FIRST_SEED = 1, SEEDS = 24, STEPS = 200 };

static uint32_t              colors[4];
static dgx_ui_font_variant_t font[1];
static dgx_ui_text_style_t   style = {.variants = font, .count = 1, .align = DGX_FONT_ALIGN_LEFT};

/* a bar of the program's own, filled up to the slider's value */
static bool bar_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *node = dgx_ui_node(ui, i);
    const int            full = node->w * dgx_ui_value_of(node) / 100;
    if (full > 0) dgx_fill_rectangle(paint->scr, node->x - paint->ox, node->y - paint->oy, full, node->h, colors[3]);
    if (full < node->w) dgx_fill_rectangle(paint->scr, node->x + full - paint->ox, node->y - paint->oy, node->w - full, node->h, colors[2]);
    return false;
}
static const dgx_ui_look_t bar_look = {.draw = bar_draw, .fill = -1};

/* the program's memory */
static int32_t          level, row, flag, popup_level, choice, ticked;
static char             note_text[120], list_text[60], ask_text[16], button_text[12];
static dgx_ui_textbox_t note_box, list_box, ask_box, button_box;
static dgx_ui_textbox_t one_box = {.buf = (char *)"One", .lines = 1, .style = &style}, two_box = {.buf = (char *)"Two", .lines = 1, .style = &style};
static dgx_ui_textbox_t six_box = {.buf = (char *)"Six", .lines = 1, .style = &style}, four_box = {.buf = (char *)"Four", .lines = 1, .style = &style};
static dgx_ui_textbox_t tick_box = {.buf = (char *)"Tick", .lines = 1, .style = &style};
static dgx_ui_textbox_t caption_box = DGX_UI_TEXT_CONST("Yes", 1, &style);
static const char *const captions[] = {"Yes", "Oui", "Ja", "Si"};

/* the roots told where their trees are: 0 the home, 1 the popup, 2 the page; each one's last word, true shown */
enum { HOME_TREE, POPUP_TREE, PAGE_TREE, TREES };
static bool shown_last[TREES];
static int  out_of_turn;

/* home and the place its pages may lie in; a root's on() sometimes moves the pages or removes the place and adds it
   again, as the rules of SHOWN and COVERED let it: no tree opened, closed or freed */
static dgx_ui_t             *home_tree;
static int                   place_at = -1;
static bool                  roots_act;
static int                   acting_depth, roots_acted, steps_in_place, steps_repeating; /* how much of it the scripts reached */
static const dgx_ui_widget_t place_w = {.kind = DGX_UI_BOX, .want_h = 30};
static int                   chance(int below);
static void                  place_again(dgx_ui_t *home)
{
    if (dgx_ui_node(home, place_at)) dgx_ui_remove(home, place_at);
    place_at = dgx_ui_add(home, 0, &place_w);
}

static void on_root(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    const int tree = (int)(intptr_t)ctx;
    if (what == DGX_UI_SHOWN) out_of_turn += shown_last[tree], shown_last[tree] = true;
    if (what == DGX_UI_COVERED) out_of_turn += !shown_last[tree], shown_last[tree] = false;
    if (!roots_act || acting_depth > 1 || (what != DGX_UI_SHOWN && what != DGX_UI_COVERED) || chance(6)) return;
    ++acting_depth, ++roots_acted;
    if (chance(2)) dgx_ui_page_place(home_tree, chance(2) ? place_at : -1);
    else place_again(home_tree);
    --acting_depth;
}

/* the idle's messages heard by home's root: by turns, 1 first */
enum { IDLE_WHAT = 91 };
static int  idle_told[2], idle_out_of_turn, idle_last = 0;
static bool idle_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what != IDLE_WHAT) return false;
    const int byte = msg->bytes[0];
    idle_out_of_turn += byte != !idle_last, idle_last = byte, ++idle_told[byte & 1];
    return true;
}
static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2, .on = on_root, .ctx = (void *)(intptr_t)HOME_TREE, .take = idle_take};
static const dgx_ui_widget_t note_w   = {.kind = DGX_UI_LABEL, .text = &note_box, .look = &dgx_ui_look_lines};
static const dgx_ui_widget_t slider_w = {.kind = DGX_UI_SLIDER, .min = 0, .max = 100, .step = 5, .want_h = 12, .value = &level, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t bar_w    = {.kind = DGX_UI_OWN, .min = 0, .max = 100, .want_h = 6, .value = &level, .look = &bar_look};
static const dgx_ui_widget_t gauge_w  = {.kind = DGX_UI_OWN, .min = 0, .max = 100, .want_h = 6, .value = &level, .look = &dgx_ui_look_level};
static const dgx_ui_widget_t ring_w   = {.kind = DGX_UI_OWN, .flags = DGX_UI_COLUMN, .pad = 5, .min = 0, .max = 100, .want_w = 16, .value = &level, .look = &dgx_ui_look_ring};
static const dgx_ui_widget_t dot_w    = {.kind = DGX_UI_FRAME, .want_h = 6, .look = &dgx_ui_look_frame}; /* a child inside the ring */
static const dgx_ui_widget_t line_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 4, .want_h = 16};
static const dgx_ui_widget_t button_w = {.kind = DGX_UI_BUTTON, .flags = DGX_UI_REPEAT, .text = &button_box, .want_w = 50, .look = &dgx_ui_look_button};
static const dgx_ui_widget_t switch_w = {.kind = DGX_UI_SWITCH, .max = 1, .want_w = 26, .value = &flag, .look = &dgx_ui_look_switch};
static const dgx_ui_widget_t caption_w = {.kind = DGX_UI_LABEL, .want_w = 60, .text = &caption_box};
/* a picture button, and a toggle of two pictures of the switch's place */
static const uint8_t          dot_bits[5] = {0x70, 0xf8, 0xf8, 0xf8, 0x70}, ring_bits[5] = {0x70, 0x88, 0x88, 0x88, 0x70};
static const dgx_image_t      dot_im = {.width = 5, .height = 5, .format = DGX_IMAGE_1BPP, .data = dot_bits};
static const dgx_image_t      ring_im = {.width = 5, .height = 5, .format = DGX_IMAGE_1BPP, .data = ring_bits};
static const dgx_ui_picture_t dot_pic = {.image = &dot_im}, two_pics[2] = {{.image = &ring_im}, {.image = &dot_im}};
static const dgx_ui_widget_t  pic_button_w = {.kind = DGX_UI_BUTTON, .picture = &dot_pic, .look = &dgx_ui_look_picture_button};
static const dgx_ui_widget_t  pic_toggle_w = {.kind = DGX_UI_SWITCH, .value = &flag, .picture = two_pics, .look = &dgx_ui_look_picture_button};
static const dgx_ui_widget_t list_w   = {.kind = DGX_UI_LIST, .min = 0, .max = 5, .text = &list_box, .want_h = 30, .value = &row, .look = &dgx_ui_look_list};
static const dgx_ui_widget_t group_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 2, .want_h = 16};
static const dgx_ui_widget_t radio_w[3] = {
    {.kind = DGX_UI_RADIO, .value = &choice, .step = 0, .text = &one_box, .want_w = 44, .look = &dgx_ui_look_radio},
    {.kind = DGX_UI_RADIO, .value = &choice, .step = 1, .text = &two_box, .want_w = 44, .look = &dgx_ui_look_radio},
    {.kind = DGX_UI_RADIO, .value = &choice, .step = 2, .text = &six_box, .want_w = 44, .look = &dgx_ui_look_radio},
};
static const dgx_ui_widget_t tick_w  = {.kind = DGX_UI_SWITCH, .value = &ticked, .text = &tick_box, .want_w = 56, .look = &dgx_ui_look_checkbox};
static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .gap = 2, .look = &dgx_ui_look_frame, .on = on_root, .ctx = (void *)(intptr_t)POPUP_TREE};
static const dgx_ui_widget_t page_w  = {.kind = DGX_UI_FRAME, .look = &dgx_ui_look_frame, .on = on_root, .ctx = (void *)(intptr_t)PAGE_TREE};

/* each tree's last word what a walk of the chain from home says: shown while no page lies above it; out of it covered */
static bool as_the_chain_says(dgx_ui_t *home, dgx_ui_t *const trees[TREES])
{
    bool      shown_now[TREES] = {false};
    dgx_ui_t *chain[8];
    int       count = 0;
    for (dgx_ui_t *tree = home; tree && count < 8; tree = tree->popup) chain[count++] = tree;
    /* a page of the screen above covers; one of the place above covers the trees of the place, the lowest popup of the
       place and those over it, and while home names no place it covers as one of the screen */
    const bool named  = home->page_place != 0;
    int        lowest = count;
    for (int j = count - 1; j >= 1; --j)
        if (chain[j - 1]->popup_flags & DGX_UI_POPUP_PLACE) lowest = j;
    for (int k = 0; k < count; ++k) {
        bool covered = false;
        for (int m = k; m < count; ++m) {
            if (!chain[m]->popup) continue;
            const unsigned flags = chain[m]->popup_flags;
            covered |= (flags & DGX_UI_POPUP_SCREEN) || ((flags & DGX_UI_POPUP_PLACE) && (!named || k >= lowest));
        }
        for (int t = 0; t < TREES; ++t)
            if (trees[t] == chain[k]) shown_now[t] = !covered;
    }
    for (int t = 0; t < TREES; ++t)
        if (shown_now[t] != shown_last[t]) return false;
    return out_of_turn == 0;
}
static const dgx_ui_widget_t ask_w   = {.kind = DGX_UI_LABEL, .text = &ask_box};
static const dgx_ui_widget_t inner_w = {.kind = DGX_UI_SLIDER, .min = 0, .max = 100, .step = 5, .want_h = 12, .value = &popup_level, .look = &dgx_ui_look_slider};
static const dgx_ui_widget_t four_w  = {.kind = DGX_UI_RADIO, .value = &choice, .step = 3, .text = &four_box, .look = &dgx_ui_look_radio};

/* the program's picture under the tree: bands of two colours */
static void stripes(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    for (int band_row = y; band_row < y + h; ++band_row)
        dgx_fill_rectangle(paint->scr, x - paint->ox, band_row - paint->oy, w, 1, band_row % 8 < 4 ? colors[0] : colors[2]);
}

static uint32_t seed;
static int      chance(int below)
{
    seed = seed * 1664525u + 1013904223u;
    return (int)((seed >> 10) % (uint32_t)below);
}

/* the painter's look at the places before a pass, the library's own (src/dgx_ui_priv.h) */
void dgx_ui_places(dgx_ui_t *ui);

/* a click in the middle of widget i of a tree, through the home tree, as the finger comes */
static void click(dgx_ui_t *home, dgx_ui_t *tree, int i, uint32_t now)
{
    const dgx_ui_node_t *n = dgx_ui_node(tree, i);
    const int16_t        x = (int16_t)(n->x + n->w / 2), y = (int16_t)(n->y + n->h / 2);
    dgx_ui_pointer(home, &(dgx_ui_pointer_t){.x = x, .y = y, .down = true, .pressed = true, .ms = now});
    dgx_ui_pointer(home, &(dgx_ui_pointer_t){.x = x, .y = y, .released = true, .ms = now + 5});
}

int main(void)
{
    static uint8_t strip_buffer[8192];
    font[0] = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    static const char *const notes[] = {"The filter has worked for two thousand hours.", "Replace it and hold the button.", "", "Ready"};
    for (int drawn_background = 0; drawn_background < 2; ++drawn_background) {
        int faulty_seeds = 0, unquiet_seeds = 0, unsaid_seeds = 0;
        for (int s = FIRST_SEED; s < FIRST_SEED + SEEDS; ++s) {
            seed                        = (uint32_t)s * 2654435761u;
            panel_t       *panel        = panel_make();
            dgx_screen_t  *fresh_screen = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
            dgx_ui_paint_t paint        = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
            if (drawn_background) {
                paint.buffer = strip_buffer, paint.buffer_size = sizeof strip_buffer, paint.strips = &dgx_ui_strips;
                dgx_ui_background_set(&paint, stripes, NULL);
            }
            colors[0] = dgx_screen_rgb(paint.scr, 18, 22, 30), colors[1] = dgx_screen_rgb(paint.scr, 232, 232, 222);
            colors[2] = dgx_screen_rgb(paint.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(paint.scr, 240, 160, 60);
            static dgx_ui_node_t nodes[20], popup_nodes[4];
            dgx_ui_t             ui, popup;
            level = row = flag = popup_level = choice = ticked = 0;
            dgx_ui_textbox_init(&note_box, note_text, sizeof note_text, 2, &style);
            dgx_ui_textbox_init(&list_box, list_text, sizeof list_text, 1, &style);
            dgx_ui_textbox_init(&ask_box, ask_text, sizeof ask_text, 1, &style);
            dgx_ui_textbox_init(&button_box, button_text, sizeof button_text, 1, &style);
            dgx_ui_init(&ui, nodes, 20, 1);
            dgx_ui_add(&ui, -1, &root_w);
            const int note = dgx_ui_add(&ui, 0, &note_w), slider = dgx_ui_add(&ui, 0, &slider_w), bar = dgx_ui_add(&ui, 0, &bar_w);
            dgx_ui_add(&ui, 0, &gauge_w); /* the library's level bar of the slider's place */
            const int line = dgx_ui_add(&ui, 0, &line_w), button = dgx_ui_add(&ui, line, &button_w), toggle = dgx_ui_add(&ui, line, &switch_w);
            const int caption = dgx_ui_add(&ui, line, &caption_w);
            const int pic_button = dgx_ui_add(&ui, line, &pic_button_w), pic_toggle = dgx_ui_add(&ui, line, &pic_toggle_w);
            dgx_ui_add(&ui, dgx_ui_add(&ui, line, &ring_w), &dot_w); /* the library's ring scale of the slider's place, a child in it */
            caption_box.buf   = (char *)captions[0];
            home_tree = &ui, place_at = dgx_ui_add(&ui, 0, &place_w), roots_act = false;
            static dgx_ui_idle_t idle;
            idle = (dgx_ui_idle_t){.after_ms = 200, .what = IDLE_WHAT, .wake_acts = s % 2}, idle_last = 0;
            dgx_ui_idle(&ui, &idle, 0);
            const int list = dgx_ui_add(&ui, 0, &list_w), group = dgx_ui_add(&ui, 0, &group_w);
            int       radio[3];
            for (int k = 0; k < 3; ++k) radio[k] = dgx_ui_add(&ui, group, &radio_w[k]);
            const int tick = dgx_ui_add(&ui, group, &tick_w);
            dgx_ui_text_set(&ui, note, notes[0]), dgx_ui_text_set(&ui, list, "one\ntwo\nthree\nfour\nfive\nsix"), dgx_ui_text_set(&ui, button, "Go");
            static dgx_ui_node_t page_nodes[1];
            dgx_ui_t             page;
            dgx_ui_init(&page, page_nodes, 1, 1);
            dgx_ui_add(&page, -1, &page_w);
            dgx_ui_t *const trees[TREES] = {&ui, &popup, &page};
            shown_last[HOME_TREE] = true, shown_last[POPUP_TREE] = shown_last[PAGE_TREE] = false, out_of_turn = 0;
            int unsaid_steps = 0;
            dgx_ui_init(&popup, popup_nodes, 4, 1);
            dgx_ui_add(&popup, -1, &frame_w);
            dgx_ui_text_set(&popup, dgx_ui_add(&popup, 0, &ask_w), "Really?");
            dgx_ui_add(&popup, 0, &inner_w);
            const int four = dgx_ui_add(&popup, 0, &four_w);
            bool      held = false;
            int       first_bad = -1;
            char      history[4][40] = {"", "", "", ""};
            uint32_t  now = 0;
            for (int step = 0; step <= STEPS + 1; ++step) {
                now += 20;
                const int acts = step < STEPS ? 1 + chance(3) : 0;
                char      did[40] = "";
                if (step == STEPS) {
                    /* the end: the finger up, the popup closed, the band gone; this pass settles, the next is idle */
                    if (held) dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.released = true, .ms = now}), held = false;
                    if (ui.popup) dgx_ui_popup_close(&ui);
                    paint.cover = dgx_ui_rect(0, 0, 0, 0);
                }
                if (step == STEPS + 1) {
                    dgx_ui_places(&ui);
                    int marked = 0;
                    /* a hidden widget keeps its marks till it is shown */
                    for (int i = 0; i < ui.count; ++i) marked += dgx_ui_node(&ui, i) && dgx_ui_node(&ui, i)->marks && !dgx_ui_hidden(&ui, i);
                    if (marked) ++unquiet_seeds, printf("seed %d: %d widgets marked after the passes settled\n", s, marked);
                }
                for (int a = 0; a < acts; ++a) {
                    const int what = chance(30);
                    did[strlen(did)] = (char)('a' + what);
                    switch (what) {
                    case 0: level = chance(21) * 5; break;
                    case 1: row = chance(6); break;
                    case 2: flag = !flag; break;
                    case 3: dgx_ui_text_set(&ui, note, notes[chance(4)]); break;
                    case 4: dgx_ui_show(&ui, bar, chance(2)); break;
                    case 5: { /* over the top of the chain: home, or the page */
                        dgx_ui_t *top = &ui;
                        while (top->popup) top = top->popup;
                        if (!popup.level && top != &popup) dgx_ui_popup_open(top, &popup, chance(90), chance(80), 80 + chance(30), 54, chance(2) ? DGX_UI_POPUP_MODAL : 0);
                        break;
                    }
                    case 6:
                        if (ui.popup) dgx_ui_popup_close(&ui);
                        break;
                    case 7: popup_level = chance(21) * 5; break;
                    case 8: paint.cover = chance(2) ? dgx_ui_rect(0, 40 + chance(60), PANEL_W, 40) : dgx_ui_rect(0, 0, 0, 0); break;
                    case 9: dgx_ui_set_state(&ui, slider, DGX_UI_DISABLED, chance(2)); break;
                    case 10: {
                        const dgx_ui_node_t *n = dgx_ui_node(&ui, slider);
                        const int16_t        x = (int16_t)(n->x + chance(n->w > 0 ? n->w : 1)), y = (int16_t)(n->y + 4);
                        if (!held) dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = x, .y = y, .down = true, .pressed = true, .ms = now}), held = true;
                        else if (chance(3)) dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = x, .y = y, .down = true, .ms = now});
                        else dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = x, .y = y, .released = true, .ms = now}), held = false;
                        break;
                    }
                    case 11: dgx_ui_show(&ui, list, chance(4) != 0); break;
                    case 12: dgx_ui_set_state(&ui, chance(2) ? button : pic_button, DGX_UI_PRESSED, chance(2)); break;
                    case 13: { /* the focus on the slider, the switch, the picture button or the picture toggle */
                        const int focused[4] = {slider, toggle, pic_button, pic_toggle};
                        dgx_ui_focus(&ui, focused[chance(4)]);
                        break;
                    }
                    /* the group: its place written, a radio clicked, focused and chosen by the key, shown and hidden,
                       disabled; the popup's radio clicked; the checkbox ticked by the program and by a click */
                    case 14: choice = chance(5); break; /* 4: no button's number */
                    case 15:
                        if (!held) click(&ui, &ui, radio[chance(3)], now);
                        break;
                    case 16:
                        dgx_ui_focus(&ui, radio[chance(3)]);
                        dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE);
                        break;
                    case 17: dgx_ui_show(&ui, radio[chance(3)], chance(3) != 0); break;
                    case 18: dgx_ui_set_state(&ui, radio[chance(3)], DGX_UI_DISABLED, chance(2)); break;
                    case 19:
                        if (!held && ui.popup == &popup) click(&ui, &popup, four, now);
                        break;
                    case 20:
                        if (chance(3) == 0) ticked = !ticked;
                        else if (!held) click(&ui, &ui, chance(2) ? tick : pic_toggle, now);
                        break;
                    case 21: dgx_ui_show(&ui, group, chance(3) != 0); break; /* the row of the group and the checkbox */
                    case 23: /* a page over the top page, or home */
                        if (!page.level) dgx_ui_page_open(&ui, &page);
                        break;
                    case 24: dgx_ui_page_back(&ui); break;
                    case 25: /* a popup closed from the middle of the chain */
                        if (ui.popup && ui.popup->popup) dgx_ui_popup_close(ui.popup);
                        break;
                    case 26: dgx_ui_page_place(&ui, chance(3) ? place_at : -1); break; /* the pages in the place, or on the screen */
                    case 27: place_again(&ui); break;                                  /* the place's box removed and added again */
                    case 28: roots_act = !roots_act; break;                            /* the roots' on() acting, or not */
                    case 29: { /* the finger down on the repeating button, held a while, let go */
                        const dgx_ui_node_t *n = dgx_ui_node(&ui, button);
                        const int16_t        x = (int16_t)(n->x + n->w / 2), y = (int16_t)(n->y + n->h / 2);
                        if (!held) dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = x, .y = y, .down = true, .pressed = true, .ms = now}), held = true;
                        else dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = x, .y = y, .released = true, .ms = now}), held = false;
                        break;
                    }
                    case 22: /* a caption over a constant pointed at another, and told */
                        caption_box.buf = (char *)captions[chance(4)];
                        dgx_ui_text_written(&ui, caption);
                        break;
                    }
                }
                memmove(history[0], history[1], sizeof history[0] * 3), strcpy(history[3], did);
                steps_repeating += dgx_ui_time(&ui, now); /* the button held repeats */
                panel_count_from_now(panel);
                dgx_ui_paint(&ui, &paint, now);
                const int twice = panel_twice(panel), written = panel_written(panel);
                /* the tree composed anew by a fresh paint at this moment */
                dgx_ui_paint_t fresh = {.scr = fresh_screen, .colors = colors, .color_count = 4, .now_ms = now, .cover = paint.cover};
                if (drawn_background) dgx_ui_background_set(&fresh, stripes, NULL);
                dgx_fill_rectangle(fresh_screen, 0, 0, PANEL_W, PANEL_H, colors[0]);
                dgx_ui_paint_area_to(&ui, &fresh, fresh_screen, NULL, 0, 0, PANEL_W, PANEL_H);
                int off = 0;
                for (int y = 0; y < PANEL_H; ++y)
                    for (int x = 0; x < PANEL_W; ++x) {
                        if (paint.cover.w && y >= paint.cover.y && y < paint.cover.y + paint.cover.h) continue; /* the band's own rows are the keyboard's */
                        off += panel->mem[y][x] != dgx_get_pixel(fresh_screen, x, y);
                    }
                dgx_ui_paint_free(&fresh);
                unsaid_steps += !as_the_chain_says(&ui, trees);
                steps_in_place += ui.page_place && page.under && (page.under->popup_flags & DGX_UI_POPUP_PLACE);
                const bool unquiet = step == STEPS + 1 && written;
                if (unquiet) ++unquiet_seeds, printf("seed %d: %d pixels written by the pass after the passes settled\n", s, written);
                if ((twice || off) && first_bad < 0) {
                    first_bad = step;
                    printf("seed %d%s step %d: %d pixels twice, %d off a fresh composing; the last acts: %s | %s | %s | %s; popup %d, band %d\n", s,
                           drawn_background ? " over the background" : "", step, twice, off, history[0], history[1], history[2], history[3], ui.popup != NULL,
                           paint.cover.w != 0);
                }
            }
            faulty_seeds += first_bad >= 0;
            if (unsaid_steps) ++unsaid_seeds, printf("seed %d: %d steps with a tree told otherwise than it stands\n", s, unsaid_steps);
            if (ui.popup) dgx_ui_popup_close(&ui);
            dgx_ui_paint_free(&paint), dgx_ui_free(&ui), dgx_ui_free(&popup), dgx_ui_free(&page);
            dgx_screen_destroy(&fresh_screen), free(panel);
        }
        CHECK(faulty_seeds == 0);
        CHECK(unquiet_seeds == 0);
        CHECK(steps_in_place > 100 && roots_acted > 20 && steps_repeating > 100); /* the place, the roots' acts, the repeat reached */
        CHECK(idle_out_of_turn == 0 && idle_told[1] > 20 && idle_told[0] > 20); /* the idle told by turns, often enough */
        if (drawn_background)
            printf("test_random: %d steps with the page in the place, %d acts of the roots, %d steps repeating, idle told %d times and woken %d\n", steps_in_place, roots_acted,
                   steps_repeating, idle_told[1], idle_told[0]);
        CHECK(unsaid_seeds == 0);
    }
    CHECK_DONE();
}
