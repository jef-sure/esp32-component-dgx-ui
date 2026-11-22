/*
 * The state of a text for the program, dgx_ui_text_state(), and its letters shown whole, dgx_ui_text_whole(): COMING
 * while letters come, SEEN once the look showed all it will show, neither while an important text scrolls; by the four
 * ways a text is drawn (one line and lines, in a bitmap font and handwritten), important and not, fitting, cut and
 * scrolling. Whole at the start, in the middle of the letters (every letter at the very next pass), during the scroll
 * and at the end (nothing changes); the scroll after it counted from that pass; a text set after it appearing again.
 * SEEN through every way of painting: straight, through the strips with a buffer, on a drawn background; a hidden
 * widget and one wholly under a popup are not seen, one partly under it is; an area composed sets nothing and clears
 * nothing; a pass that draws another widget leaves it as it was.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"
#include "panel.h"

static uint32_t              colors[2] = {0x0000, 0xffff};
static dgx_ui_font_variant_t bitmap[1], hand[1];
static dgx_ui_text_style_t   plain, important, written, written_important;

static dgx_ui_node_t         nodes[8];
static dgx_ui_t              ui;
static char                  buf[200], other_buf[32];
static dgx_ui_textbox_t      box, other_box;
static dgx_ui_widget_t       text_w, other_w;
static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX};
static int                   text_i, other_i;

/* a tree of a text of so many lines in a look and a style, w x h at 4, 4, and another label below */
static void make(const dgx_ui_look_t *look, const dgx_ui_text_style_t *style, int lines, int w, int h, const char *text)
{
    static bool made;
    if (made) dgx_ui_free(&ui); /* what a look keeps of the tree let go with it */
    made = true;
    dgx_ui_textbox_init(&box, buf, sizeof buf, lines, style);
    dgx_ui_textbox_init(&other_box, other_buf, sizeof other_buf, 1, &plain);
    text_w  = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &box, .look = look, .want_w = (int16_t)w, .want_h = (int16_t)h, .at_x = 4, .at_y = 4};
    other_w = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &other_box, .want_w = 40, .want_h = 12, .at_x = 4, .at_y = -2};
    dgx_ui_init(&ui, nodes, 8, 1);
    dgx_ui_add(&ui, -1, &root_w);
    text_i = dgx_ui_add(&ui, 0, &text_w), other_i = dgx_ui_add(&ui, 0, &other_w);
    dgx_ui_text_set(&ui, text_i, text), dgx_ui_text_set(&ui, other_i, "other");
}

/* a drawn background: a colour laid, the program's picture */
static void draw_back(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    (void)ctx;
    dgx_fill_rectangle(paint->scr, x - paint->ox, y - paint->oy, w, h, 0x0841);
}

static int state(void)
{
    return dgx_ui_text_state(&ui, text_i);
}

int main(void)
{
    bitmap[0]         = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    hand[0]           = (dgx_ui_font_variant_t){.font = &font0ss, .height = 14, .pen = 1, .hand = &dgx_ui_handwriting};
    plain             = (dgx_ui_text_style_t){.variants = bitmap, .count = 1};
    important         = (dgx_ui_text_style_t){.variants = bitmap, .count = 1, .flags = DGX_UI_TEXT_IMPORTANT, .scroll_ms = 100};
    written           = (dgx_ui_text_style_t){.variants = hand, .count = 1};
    written_important = (dgx_ui_text_style_t){.variants = hand, .count = 1, .flags = DGX_UI_TEXT_IMPORTANT, .scroll_ms = 100};
    dgx_screen_t  *v  = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 2};
    CHECK(dgx_ui_text_state(&ui, 99) == 0);

    /* one line in a bitmap font: no letters come; SEEN from its first pass, cut with an ellipsis too */
    for (int cut = 0; cut <= 1; ++cut) {
        make(&dgx_ui_look_text, &plain, 1, cut ? 20 : 80, 14, "Hello there");
        CHECK(state() == 0);
        dgx_ui_paint(&ui, &pv, 0);
        CHECK(state() == DGX_UI_TEXT_SEEN);
        dgx_ui_text_appear(&ui, text_i, 20); /* asked to appear: not drawn since, coming; drawn, at once */
        CHECK(state() == DGX_UI_TEXT_COMING);
        dgx_ui_paint(&ui, &pv, 10);
        CHECK(state() == DGX_UI_TEXT_SEEN);
        dgx_ui_text_whole(&ui, text_i);
        dgx_ui_paint(&ui, &pv, 20);
        CHECK(state() == DGX_UI_TEXT_SEEN);
    }

    /* lines in a bitmap font, not important: COMING while the letters come, SEEN once all came, cut by its box or not */
    for (int cut = 0; cut <= 1; ++cut) {
        make(&dgx_ui_look_lines, &plain, cut ? 1 : 3, 60, cut ? 13 : 40, "aa bb cc dd ee ff");
        dgx_ui_text_appear(&ui, text_i, 10);
        dgx_ui_paint(&ui, &pv, 1000);
        const int letters = box.total_letters;
        CHECK(letters == 16 && state() == DGX_UI_TEXT_COMING);
        dgx_ui_paint(&ui, &pv, 1000 + 10 * (letters - 1));
        CHECK(state() == DGX_UI_TEXT_COMING && box.letters_drawn == letters - 1);
        dgx_ui_paint(&ui, &pv, 1000 + 10 * letters);
        CHECK(state() == DGX_UI_TEXT_SEEN);
    }

    /* lines in a bitmap font, important and longer than its box: COMING, then scrolling (neither), then SEEN */
    make(&dgx_ui_look_lines, &important, 2, 60, 26, "aa bb cc dd ee ff gg hh ii jj");
    dgx_ui_text_appear(&ui, text_i, 10);
    dgx_ui_paint(&ui, &pv, 0);
    const int letters = box.total_letters, lines = box.total_lines;
    CHECK(lines == 4 && state() == DGX_UI_TEXT_COMING);
    dgx_ui_paint(&ui, &pv, 10 * letters);
    CHECK(state() == 0 && box.top == 0);
    dgx_ui_paint(&ui, &pv, 10 * letters + 100);
    CHECK(state() == 0 && box.top == 1);
    dgx_ui_text_whole(&ui, text_i); /* during the scroll: nothing changes */
    dgx_ui_paint(&ui, &pv, 10 * letters + 150);
    CHECK(state() == 0 && box.top == 1);
    dgx_ui_paint(&ui, &pv, 10 * letters + 200);
    CHECK(state() == DGX_UI_TEXT_SEEN && box.top == 2);
    dgx_ui_text_whole(&ui, text_i); /* at the end: nothing */
    dgx_ui_paint(&ui, &pv, 10 * letters + 900);
    CHECK(state() == DGX_UI_TEXT_SEEN && box.top == 2);

    /* whole in the middle of the letters: every letter at the very next pass, the scroll counted from that pass */
    dgx_ui_text_set(&ui, text_i, "bb aa cc dd ee ff gg hh ii jj");
    dgx_ui_paint(&ui, &pv, 5000);
    dgx_ui_paint(&ui, &pv, 5030);
    CHECK(state() == DGX_UI_TEXT_COMING && box.letters_drawn == 3);
    dgx_ui_text_whole(&ui, text_i);
    CHECK(state() == 0); /* its letters came: the pass to come shows them */
    dgx_ui_paint(&ui, &pv, 5031);
    CHECK(box.letters_drawn == letters && box.top == 0 && state() == 0);
    dgx_ui_paint(&ui, &pv, 5130);
    CHECK(box.top == 0);
    dgx_ui_paint(&ui, &pv, 5131);
    CHECK(box.top == 1 && state() == 0);
    dgx_ui_paint(&ui, &pv, 5231);
    CHECK(box.top == 2 && state() == DGX_UI_TEXT_SEEN);
    /* a text set after it appears again, at the pace asked */
    dgx_ui_text_set(&ui, text_i, "ii jj");
    CHECK(state() == DGX_UI_TEXT_COMING);
    dgx_ui_paint(&ui, &pv, 6000);
    dgx_ui_paint(&ui, &pv, 6020);
    CHECK(state() == DGX_UI_TEXT_COMING && box.letters_drawn == 2);
    /* whole at the start, before its first pass */
    dgx_ui_text_set(&ui, text_i, "bb aa cc dd ee ff gg hh ii jj");
    dgx_ui_text_whole(&ui, text_i);
    dgx_ui_paint(&ui, &pv, 7000);
    CHECK(box.letters_drawn == letters && box.top == 0 && state() == 0);

    /* one handwritten line: COMING while the pen writes, SEEN once written; whole writes it all at the next pass */
    make(&dgx_ui_look_text, &written, 1, 90, 18, "Hello");
    dgx_ui_text_appear(&ui, text_i, 40);
    dgx_ui_paint(&ui, &pv, 0);
    CHECK(state() == DGX_UI_TEXT_COMING);
    dgx_ui_paint(&ui, &pv, 100);
    CHECK(state() == DGX_UI_TEXT_COMING);
    dgx_ui_paint(&ui, &pv, 200);
    CHECK(state() == DGX_UI_TEXT_SEEN && (box.flags & DGX_UI_TEXTBOX_CAME));
    dgx_ui_text_set(&ui, text_i, "Again");
    dgx_ui_paint(&ui, &pv, 1000);
    dgx_ui_paint(&ui, &pv, 1050);
    CHECK(state() == DGX_UI_TEXT_COMING);
    dgx_ui_text_whole(&ui, text_i);
    dgx_ui_paint(&ui, &pv, 1051);
    CHECK(state() == DGX_UI_TEXT_SEEN);

    /* handwritten lines, important and longer than the box: as lines in a bitmap font */
    make(&dgx_ui_look_hand_lines, &written_important, 2, 60, 28, "aa bb cc dd ee ff gg hh ii jj kk ll mm nn");
    dgx_ui_text_appear(&ui, text_i, 10);
    dgx_ui_paint(&ui, &pv, 0);
    const int hand_letters = box.total_letters;
    CHECK(box.total_lines > 2 && state() == DGX_UI_TEXT_COMING);
    dgx_ui_paint(&ui, &pv, 30);
    dgx_ui_text_whole(&ui, text_i);
    dgx_ui_paint(&ui, &pv, 31);
    CHECK(state() == 0 && box.top == 0 && (box.flags & DGX_UI_TEXTBOX_CAME));
    dgx_ui_paint(&ui, &pv, 131);
    CHECK(box.top == 1);
    for (int t = 231; t < 2000 && state() != DGX_UI_TEXT_SEEN; t += 100) dgx_ui_paint(&ui, &pv, (uint32_t)t);
    CHECK(state() == DGX_UI_TEXT_SEEN && box.top == box.total_lines - 2);
    dgx_ui_text_set(&ui, text_i, "bb aa cc dd ee ff gg hh ii jj kk ll mm nn");
    dgx_ui_paint(&ui, &pv, 3000);
    dgx_ui_paint(&ui, &pv, 3000 + 10 * hand_letters);
    CHECK(state() == 0 && box.top == 0); /* the letters' time past, the scroll after it as before */
    dgx_ui_paint(&ui, &pv, 3000 + 10 * hand_letters + 100);
    CHECK(box.top == 1);

    /* hidden: not drawn, not seen; shown, seen */
    make(&dgx_ui_look_text, &plain, 1, 80, 14, "Hidden");
    dgx_ui_show(&ui, text_i, false);
    dgx_ui_paint(&ui, &pv, 0);
    CHECK(state() == 0);
    dgx_ui_show(&ui, text_i, true);
    dgx_ui_paint(&ui, &pv, 10);
    CHECK(state() == DGX_UI_TEXT_SEEN);

    /* under a popup: wholly, not seen; partly, the part drawn, seen */
    {
        static dgx_ui_node_t         popup_nodes[2];
        static dgx_ui_t              popup;
        static const dgx_ui_widget_t cover_w = {.kind = DGX_UI_FRAME};
        for (int partly = 0; partly <= 1; ++partly) {
            make(&dgx_ui_look_text, &plain, 1, 40, 14, "Under");
            dgx_ui_init(&popup, popup_nodes, 2, 1);
            dgx_ui_add(&popup, -1, &cover_w);
            dgx_ui_popup_open(&ui, &popup, partly ? 20 : 0, 0, PANEL_W, 30, DGX_UI_POPUP_MODAL);
            dgx_ui_paint(&ui, &pv, 0);
            CHECK(state() == (partly ? DGX_UI_TEXT_SEEN : 0));
            dgx_ui_popup_close(&ui);
        }
    }

    /* through the strips with a buffer, and on a drawn background */
    {
        panel_t       *panel = panel_make();
        static uint8_t buffer[PANEL_W * 2 * 16];
        dgx_ui_paint_t pa    = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 2, .buffer = buffer, .buffer_size = sizeof buffer,
                                .strips = &dgx_ui_strips};
        make(&dgx_ui_look_lines, &important, 2, 60, 26, "aa bb cc dd ee ff gg hh ii jj");
        dgx_ui_text_appear(&ui, text_i, 10);
        dgx_ui_paint(&ui, &pa, 0);
        CHECK(state() == DGX_UI_TEXT_COMING);
        for (int t = 10; t < 3000 && state() != DGX_UI_TEXT_SEEN; t += 10) dgx_ui_paint(&ui, &pa, (uint32_t)t);
        CHECK(state() == DGX_UI_TEXT_SEEN && box.top == 2);
        dgx_ui_paint_free(&pa);
        dgx_ui_paint_t pb = {.scr = v, .colors = colors, .color_count = 2, .background = 0};
        dgx_ui_background_set(&pb, draw_back, NULL);
        make(&dgx_ui_look_text, &plain, 1, 80, 14, "On a background");
        dgx_ui_paint(&ui, &pb, 0);
        CHECK(state() == DGX_UI_TEXT_SEEN);
        dgx_ui_paint_free(&pb);
        free(panel);
    }

    /* an area composed sets nothing and clears nothing; a pass drawing another widget leaves it */
    make(&dgx_ui_look_lines, &plain, 2, 60, 26, "aa bb cc");
    dgx_ui_paint(&ui, &pv, 0);
    CHECK(state() == DGX_UI_TEXT_SEEN);
    dgx_ui_paint_area(&ui, &pv, 4, 4, 10, 6); /* a part of it */
    CHECK(state() == DGX_UI_TEXT_SEEN);
    dgx_ui_text_set(&ui, other_i, "changed");
    dgx_ui_paint(&ui, &pv, 10);
    CHECK(state() == DGX_UI_TEXT_SEEN);
    dgx_ui_text_set(&ui, text_i, "dd ee ff");
    CHECK(state() == 0);
    dgx_ui_paint_area(&ui, &pv, 0, 0, PANEL_W, PANEL_H); /* composed: not the painter's own pass */
    CHECK(state() == 0);
    dgx_ui_paint(&ui, &pv, 20);
    CHECK(state() == DGX_UI_TEXT_SEEN);

    dgx_ui_free(&ui);
    dgx_ui_paint_free(&pv);
    v->destroy(&v);
    CHECK_DONE();
}
