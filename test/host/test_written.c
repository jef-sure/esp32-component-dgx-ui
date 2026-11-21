/*
 * A text box over a string constant (DGX_UI_TEXT_CONST()) and a buffer the program wrote itself
 * (dgx_ui_text_written()). A constant caption drawn, never written by setting it, pointed at another constant and
 * told; one string under two boxes of two widgets, in the tree and in a popup over it, one of them swapped; a buffer
 * of the program written in place and fitted again in another variant; a buffer with no 0 within its size; the
 * lines look, a text scrolled and letters coming, each begun anew; a list's rows written in place, its window kept; a
 * dropdown's options written while its list is open; handwriting in lines and in a canvas written in place; a
 * constant field typed into, and a field emptied in place; the rectangle told by dgx_ui_changed(); a widget with no
 * box. After each pass on a panel behind a bus (16 bits straight, 18 through the strips: PANEL_BITS, by the
 * Makefile): no pixel written twice, and the panel as a fresh paint composes the tree.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_dropdown.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"
#include "panel.h"

static uint32_t       colors[4];
static panel_t       *panel;
static dgx_screen_t  *memory;
static uint8_t        buffer[PANEL_W * 3 * 8];
static dgx_ui_paint_t paint;

/* a bitmap font; a ladder of a large font then a small one; handwriting straight, and into a canvas */
static dgx_ui_font_variant_t terminus[1], ladder[2], hand[1], hand_canvas[1];
static dgx_ui_text_style_t   style        = {.variants = terminus, .count = 1};
static dgx_ui_text_style_t   ladder_style = {.variants = ladder, .count = 2};
static dgx_ui_text_style_t   hand_style   = {.variants = hand, .count = 1};
static dgx_ui_text_style_t   canvas_style = {.variants = hand_canvas, .count = 1};

static const char yes_text[] = "Yes";

static void paint_anew(void)
{
    paint = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer};
#if PANEL_BITS == 18
    paint.strips = &dgx_ui_strips;
#endif
}

/* a pass counted from now; then no pixel twice (twice_counted), and the panel as a fresh paint composes the tree and
   its popups */
static int pass_counted(dgx_ui_t *ui, uint32_t now_ms, bool twice_counted)
{
    panel_count_from_now(panel);
    dgx_ui_paint(ui, &paint, now_ms);
    dgx_ui_paint_t fresh = {.scr = memory, .colors = colors, .color_count = 4, .now_ms = now_ms};
    dgx_fill_rectangle(memory, 0, 0, PANEL_W, PANEL_H, colors[3]);
    dgx_ui_paint_area_to(ui, &fresh, memory, NULL, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_paint_free(&fresh);
    int off = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) off += panel->mem[y][x] != dgx_get_pixel(memory, x, y);
    return (twice_counted ? panel_twice(panel) : 0) + off;
}

static int pass(dgx_ui_t *ui, uint32_t now_ms)
{
    return pass_counted(ui, now_ms, true);
}

/* handwriting: straight on the panel the pen's strokes cross the paper laid under them and each other, so its pixels
   are written more than once as the hand means them, and only the panel is compared; through the strips of 18 bits
   each pixel is sent once, and a stroke drawn in a strip's screen lies as the stroke drawn whole */
static int pass_handwritten(dgx_ui_t *ui, uint32_t now_ms)
{
    return pass_counted(ui, now_ms, PANEL_BITS == 18);
}

/* a pass that should draw nothing: no pixel written */
static bool quiet(dgx_ui_t *ui, uint32_t now_ms)
{
    panel_count_from_now(panel);
    dgx_ui_paint(ui, &paint, now_ms);
    return panel_written(panel) == 0;
}

/* pixels written by the last pass within widget i's box */
static int written_in(const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    int                  count = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) count += panel->writes[y][x] != 0;
    return count;
}

/* a tree of a column holding one widget, laid on the panel */
static dgx_ui_node_t         nodes[4];
static dgx_ui_t              tree;
static const dgx_ui_widget_t column_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1};
static int                   make_one(const dgx_ui_widget_t *w)
{
    dgx_ui_init(&tree, nodes, 4, 1);
    dgx_ui_add(&tree, -1, &column_w);
    paint_anew();
    return dgx_ui_add(&tree, 0, w);
}
static void done_one(void)
{
    dgx_ui_paint_free(&paint), dgx_ui_free(&tree);
}

/* the panel as the passes left it, against a tree of the same widget painted anew on it at now_ms: the same text
   drawn from nothing by the same paint */
static uint32_t kept[PANEL_H][PANEL_W];
static int      same_as_anew(const dgx_ui_widget_t *w, uint32_t now_ms)
{
    memcpy(kept, panel->mem, sizeof kept);
    done_one();
    make_one(w);
    dgx_ui_paint(&tree, &paint, now_ms);
    int differ = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) differ += kept[y][x] != panel->mem[y][x];
    return differ;
}

typedef struct told_t {
    int           count;
    dgx_ui_rect_t at[16];
} told_t;
static void collect(void *ctx, dgx_ui_rect_t rect)
{
    told_t *told = ctx;
    if (told->count < 16) told->at[told->count++] = rect;
}

int main(void)
{
    panel  = panel_make();
    memory = dgx_vscreen_init(PANEL_W, PANEL_H, PANEL_BITS, DgxScreenRGB);
    colors[0] = dgx_screen_rgb(&panel->sbus.scr, 16, 20, 28), colors[1] = dgx_screen_rgb(&panel->sbus.scr, 232, 232, 222);
    colors[2] = dgx_screen_rgb(&panel->sbus.scr, 44, 60, 86), colors[3] = dgx_screen_rgb(&panel->sbus.scr, 240, 160, 60);
    terminus[0]    = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    ladder[0]      = (dgx_ui_font_variant_t){.font = &DejaVuSansBook32};
    ladder[1]      = terminus[0];
    hand[0]        = (dgx_ui_font_variant_t){.font = &font0ss, .height = 16, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting};
    hand_canvas[0] = (dgx_ui_font_variant_t){.font = &font0ss, .height = 16, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting_canvas};

    /* a constant caption: drawn; setting it changes nothing; pointed at another constant and told, drawn anew */
    {
        static dgx_ui_textbox_t      yes   = DGX_UI_TEXT_CONST(yes_text, 1, &style);
        static const dgx_ui_widget_t label = {.kind = DGX_UI_LABEL, .text = &yes};
        const int                    i     = make_one(&label);
        CHECK(pass(&tree, 0) == 0 && written_in(&tree, i) > 0 && dgx_ui_node(&tree, i)->h == dgx_ui_textbox_height(&yes));
        CHECK(!dgx_ui_text_set(&tree, i, "No") && !dgx_ui_text_printf(&tree, i, "%d", 5) && yes.buf == yes_text && strcmp(yes_text, "Yes") == 0);
        CHECK(pass(&tree, 10) == 0 && quiet(&tree, 15));
        yes.buf = (char *)"Oui";
        dgx_ui_text_written(&tree, i);
        CHECK(pass(&tree, 20) == 0 && written_in(&tree, i) > 0 && yes.size == 0);
        CHECK(!dgx_ui_text_set(&tree, i, "No") && strcmp(yes.buf, "Oui") == 0);
        CHECK(pass(&tree, 30) == 0 && quiet(&tree, 35));
        done_one();
    }

    /* one string under two boxes of two widgets, in the tree and in a popup over it: the popup's swapped and told,
       the tree's stays as it was */
    {
        static dgx_ui_textbox_t      home_yes = DGX_UI_TEXT_CONST(yes_text, 1, &style), popup_yes = DGX_UI_TEXT_CONST(yes_text, 1, &style);
        static const dgx_ui_widget_t home_w = {.kind = DGX_UI_LABEL, .text = &home_yes, .want_h = 14};
        static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .look = &dgx_ui_look_frame};
        static const dgx_ui_widget_t popup_w = {.kind = DGX_UI_LABEL, .text = &popup_yes};
        const int                    i       = make_one(&home_w);
        static dgx_ui_node_t         popup_nodes[2];
        dgx_ui_t                     popup;
        dgx_ui_init(&popup, popup_nodes, 2, 1);
        dgx_ui_add(&popup, -1, &frame_w);
        const int k = dgx_ui_add(&popup, 0, &popup_w);
        CHECK(dgx_ui_popup_open(&tree, &popup, 10, 30, 60, 24, 0));
        CHECK(pass(&tree, 0) == 0 && written_in(&tree, i) > 0 && written_in(&popup, k) > 0);
        popup_yes.buf = (char *)"Non";
        dgx_ui_text_written(&popup, k);
        CHECK(pass(&tree, 10) == 0 && written_in(&tree, i) == 0 && written_in(&popup, k) > 0 && home_yes.buf == yes_text);
        dgx_ui_popup_close(&tree);
        CHECK(pass(&tree, 20) == 0);
        dgx_ui_free(&popup);
        done_one();
    }

    /* a buffer of the program written in place: fitted again, the long text in the ladder's small variant */
    {
        static char                  text[40] = "Hi";
        static dgx_ui_textbox_t      box;
        static const dgx_ui_widget_t label = {.kind = DGX_UI_LABEL, .text = &box};
        dgx_ui_textbox_init(&box, text, sizeof text, 1, &ladder_style);
        strcpy(text, "Hi");
        const int i = make_one(&label);
        CHECK(pass(&tree, 0) == 0 && box.variant == 0);
        strcpy(text, "a text much longer than the field");
        CHECK(quiet(&tree, 10)); /* written, not told: nothing */
        dgx_ui_text_written(&tree, i);
        CHECK(pass(&tree, 20) == 0 && box.variant == 1 && written_in(&tree, i) > 0);
        /* told again with nothing changed: drawn again all the same, as the header says */
        dgx_ui_text_written(&tree, i);
        CHECK(pass(&tree, 30) == 0 && written_in(&tree, i) > 0);
        done_one();
    }

    /* a buffer with no 0 within its size: ended at size - 1 on a whole letter, a letter cut by the size left out */
    {
        static char                  text[8];
        static dgx_ui_textbox_t      box;
        static const dgx_ui_widget_t label = {.kind = DGX_UI_LABEL, .text = &box};
        dgx_ui_textbox_init(&box, text, sizeof text, 1, &style);
        const int i = make_one(&label);
        memcpy(text, "abcdef\xd0\xb6", 8); /* "abcdef" and a letter of two bytes, no 0 */
        dgx_ui_text_written(&tree, i);
        CHECK(strcmp(text, "abcdef") == 0 && pass(&tree, 0) == 0);
        memcpy(text, "abcdefgh", 8);
        dgx_ui_text_written(&tree, i);
        CHECK(strcmp(text, "abcdefg") == 0 && pass(&tree, 10) == 0);
        memcpy(text, "abcde\xd0\xb6x", 8); /* the last letter whole: only the x goes */
        dgx_ui_text_written(&tree, i);
        CHECK(strcmp(text, "abcde\xd0\xb6") == 0 && pass(&tree, 20) == 0);
        done_one();
    }

    /* the lines look: a text grown in place from one line to three, counted and drawn again; a text scrolled by hand
       back at its first line; letters coming begun anew */
    {
        static char                  text[120];
        static dgx_ui_textbox_t      box;
        static const dgx_ui_widget_t note = {.kind = DGX_UI_LABEL, .text = &box, .look = &dgx_ui_look_lines};
        dgx_ui_textbox_init(&box, text, sizeof text, 3, &style);
        const int i = make_one(&note);
        strcpy(text, "Short");
        dgx_ui_text_written(&tree, i);
        CHECK(pass(&tree, 0) == 0);
        strcpy(text, "A note that takes three lines of this field and some more");
        dgx_ui_text_written(&tree, i);
        CHECK(!(box.flags & DGX_UI_TEXTBOX_COUNTED) && pass(&tree, 10) == 0 && written_in(&tree, i) > 0);
        dgx_ui_text_scroll(&tree, i, 2);
        CHECK(pass(&tree, 20) == 0 && box.top > 0);
        strcpy(text, "Another note of more lines than the field shows at once, scrolled");
        dgx_ui_text_written(&tree, i);
        CHECK(box.top == 0 && pass(&tree, 30) == 0);
        dgx_ui_text_appear(&tree, i, 20);
        CHECK(pass(&tree, 40) == 0 && pass(&tree, 400) == 0 && box.letters_drawn > 0);
        strcpy(text, "Letters again");
        dgx_ui_text_written(&tree, i);
        CHECK((box.flags & DGX_UI_TEXTBOX_START) && !(box.flags & DGX_UI_TEXTBOX_CAME));
        CHECK(pass(&tree, 420) == 0 && box.since_ms == 420 && pass(&tree, 460) == 0);
        done_one();
    }

    /* a list's rows written in place: its window kept, the rows drawn anew */
    {
        static char                  rows[60];
        static dgx_ui_textbox_t      box;
        static int32_t               pick = 4;
        static const dgx_ui_widget_t list = {.kind = DGX_UI_LIST, .value = &pick, .min = 0, .max = 5, .want_h = 30, .text = &box, .look = &dgx_ui_look_list};
        dgx_ui_textbox_init(&box, rows, sizeof rows, 1, &style);
        strcpy(rows, "one\ntwo\nthree\nfour\nfive\nsix");
        const int i = make_one(&list);
        CHECK(pass(&tree, 0) == 0 && box.top > 0);
        const uint16_t top = box.top;
        strcpy(rows, "ONE\nTWO\nTHREE\nFOUR\nFIVE\nSIX");
        dgx_ui_text_written(&tree, i);
        CHECK(box.top == top && pass(&tree, 10) == 0 && written_in(&tree, i) > 0);
        done_one();
    }

    /* a dropdown's options written in place while its list is open: the list shows them at once */
    {
        static char                  options[60];
        static dgx_ui_textbox_t      box;
        static int32_t               fruit = 1;
        static dgx_ui_dropdown_t     dropdown;
        static const dgx_ui_widget_t chooser = {.kind = DGX_UI_DROPDOWN, .value = &fruit, .min = 0, .max = 2, .want_h = 14, .text = &box, .look = &dgx_ui_look_dropdown};
        dgx_ui_textbox_init(&box, options, sizeof options, 1, &style);
        strcpy(options, "Apple\nBanana\nCherry");
        const int i = make_one(&chooser);
        CHECK(pass(&tree, 0) == 0 && dgx_ui_dropdown_open(&tree, i, &dropdown, NULL, NULL));
        CHECK(pass(&tree, 10) == 0);
        strcpy(options, "Apricot\nBlueberry\nCoconut");
        dgx_ui_text_written(&tree, i);
        CHECK(pass(&tree, 20) == 0 && written_in(&dropdown.tree, 1) > 0);
        dgx_ui_popup_close(&tree);
        CHECK(pass(&tree, 30) == 0);
        done_one();
    }

    /* handwriting in lines, and in one line into a canvas, written in place: laid and written again from the start */
    {
        static char                  text[80];
        static dgx_ui_textbox_t      box, canvas_box;
        static const dgx_ui_widget_t lines_w = {.kind = DGX_UI_LABEL, .text = &box, .look = &dgx_ui_look_hand_lines};
        static const dgx_ui_widget_t canvas_w = {.kind = DGX_UI_LABEL, .flags = DGX_UI_CANVAS, .text = &canvas_box, .want_h = 20};
        dgx_ui_textbox_init(&box, text, sizeof text, 3, &hand_style);
        strcpy(text, "one two");
        const int i = make_one(&lines_w);
        CHECK(pass_handwritten(&tree, 0) == 0);
        strcpy(text, "three four five six seven");
        dgx_ui_text_written(&tree, i);
        CHECK(pass_handwritten(&tree, 10) == 0 && written_in(&tree, i) > 0 && same_as_anew(&lines_w, 10) == 0);
        done_one();
        static char canvas_text[20];
        dgx_ui_textbox_init(&canvas_box, canvas_text, sizeof canvas_text, 1, &canvas_style);
        strcpy(canvas_text, "Hello");
        const int k = make_one(&canvas_w);
        CHECK(pass(&tree, 0) == 0);
        strcpy(canvas_text, "Bye");
        dgx_ui_text_written(&tree, k);
        CHECK(pass(&tree, 10) == 0 && written_in(&tree, k) > 0);
        done_one();
    }

    /* a field over a constant is not typed into; a field emptied in place: drawn empty, the next letter first */
    {
        static dgx_ui_textbox_t      fixed = DGX_UI_TEXT_CONST("1234", 1, &style);
        static const dgx_ui_widget_t fixed_w = {.kind = DGX_UI_INPUT, .text = &fixed, .look = &dgx_ui_look_input};
        int                          i = make_one(&fixed_w);
        dgx_ui_focus(&tree, i);
        const uint32_t letter = 'x', erase = DGX_UI_CP_ERASE;
        CHECK(!dgx_ui_typed(&tree, &letter, 1) && !dgx_ui_typed(&tree, &erase, 1) && strcmp(fixed.buf, "1234") == 0 && pass(&tree, 0) == 0);
        done_one();
        static char                  pin[8];
        static dgx_ui_textbox_t      pin_box;
        static const dgx_ui_widget_t pin_w = {.kind = DGX_UI_INPUT, .text = &pin_box, .look = &dgx_ui_look_input};
        dgx_ui_textbox_init(&pin_box, pin, sizeof pin, 1, &style);
        i = make_one(&pin_w);
        dgx_ui_focus(&tree, i);
        const uint32_t digits[] = {'1', '2', '3'};
        CHECK(dgx_ui_typed(&tree, digits, 3) && strcmp(pin, "123") == 0 && pass(&tree, 0) == 0);
        pin[0] = 0; /* a wrong try: the program clears it */
        dgx_ui_text_written(&tree, i);
        CHECK(pass(&tree, 10) == 0 && written_in(&tree, i) > 0);
        CHECK(dgx_ui_typed(&tree, &letter, 1) && strcmp(pin, "x") == 0 && pass(&tree, 20) == 0);
        done_one();
    }

    /* the tree a layer of another compositor: the widget written is told */
    {
        static char                  text[20] = "Before";
        static dgx_ui_textbox_t      box;
        static const dgx_ui_widget_t label = {.kind = DGX_UI_LABEL, .text = &box};
        dgx_ui_textbox_init(&box, text, sizeof text, 1, &style);
        strcpy(text, "Before");
        const int i = make_one(&label);
        told_t    told = {0};
        dgx_ui_changed(&tree, &paint, 0, collect, &told);
        told.count = 0;
        strcpy(text, "After");
        dgx_ui_text_written(&tree, i);
        dgx_ui_changed(&tree, &paint, 10, collect, &told);
        const dgx_ui_node_t *n    = dgx_ui_node(&tree, i);
        bool                 seen = false;
        for (int k = 0; k < told.count; ++k)
            seen |= told.at[k].x <= n->x && told.at[k].y <= n->y && told.at[k].x + told.at[k].w >= n->x + n->w && told.at[k].y + told.at[k].h >= n->y + n->h;
        CHECK(told.count >= 1 && seen);
        done_one();
    }

    /* no widget, no box: nothing */
    {
        static const dgx_ui_widget_t bare = {.kind = DGX_UI_BOX, .want_h = 10};
        const int                    i    = make_one(&bare);
        CHECK(pass(&tree, 0) == 0);
        dgx_ui_text_written(&tree, i), dgx_ui_text_written(&tree, -1), dgx_ui_text_written(&tree, 99);
        CHECK(pass(&tree, 10) == 0 && quiet(&tree, 15));
        done_one();
    }
    CHECK_DONE();
}
