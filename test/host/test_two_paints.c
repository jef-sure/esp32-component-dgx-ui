/*
 * Two paints of one tree, a label written by hand into its canvas (dgx_ui_handwriting_canvas), in the one-line look
 * and in dgx_ui_look_hand_lines: the display's paint passes every 20 ms, a second paint composes the tree at a moment
 * of its own, 10 ms later, 10 ms earlier, or the two in turn, at every pass or at every seventh; after every pass
 * each screen as a fresh paint composes the tree at its moment (composed.h). The text changed while it is written and
 * set again, twice at one moment too; its letters begun again at another pace; the widget's size changed while letters
 * come, its canvases made again, one by the program's own dgx_ui_canvas(); lines cut and scrolled; a soft pen. A mirror
 * composed by the same paint, as the recipe does it, equal to the screen and writing nothing anew into the canvas.
 * And the looks that keep no canvas, under the same second paint: handwriting straight on the screen, the look of
 * lines and the one-line look in a bitmap font, letters coming and scrolled.
 */
#include <string.h>

#include "check.h"
#include "composed.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "fonts/font0ss.h"

enum { WIDTH = 320, HEIGHT = 80 };
static uint32_t              colors[2] = {0x0000, 0xffff};
static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static char                  buf[200];
static dgx_ui_textbox_t      box;
static dgx_ui_widget_t       label_w; /* the program's RAM: its size is set by each case */
static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX};
static dgx_ui_node_t         nodes[2];
static dgx_ui_t              ui;

static const char *const ONE_LINE = "Dear friend, the garden";
static const char *const NOTE     = "Dear friend, the garden is green and the sun is warm today, and the river runs";

/* how the label is written */
enum { INTO_CANVAS, STRAIGHT, BITMAP };

/* a tree of one label at 4, 4, of handwriting into its canvas, straight, or of a bitmap font; look NULL: the one-line
   look */
static void make(const dgx_ui_look_t *look, int width, int height, const char *text, bool important, bool soft, int way)
{
    variant[0] = (dgx_ui_font_variant_t){.font   = &font0ss,
                                         .height = 16,
                                         .joined = true,
                                         .pen    = 2 | (soft ? DGX_PEN_SOFT : 0),
                                         .hand   = way == INTO_CANVAS ? &dgx_ui_handwriting_canvas : &dgx_ui_handwriting};
    if (way == BITMAP) variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.variants = variant, .count = 1, .flags = important ? DGX_UI_TEXT_IMPORTANT : 0, .scroll_ms = important ? 300 : 0};
    dgx_ui_textbox_init(&box, buf, sizeof buf, 1, &style);
    label_w = (dgx_ui_widget_t){.kind   = DGX_UI_LABEL,
                                .flags  = way == INTO_CANVAS ? DGX_UI_CANVAS : 0,
                                .text   = &box,
                                .at_x   = 4,
                                .at_y   = 4,
                                .want_w = (int16_t)width,
                                .want_h = (int16_t)height,
                                .look   = look};
    dgx_ui_init(&ui, nodes, 2, 1);
    dgx_ui_add(&ui, -1, &root_w);
    dgx_ui_add(&ui, 0, &label_w);
    dgx_ui_text_set(&ui, 1, text);
}

enum { LATER, EARLIER, IN_TURN };

/* what happens to the tree while it is written */
enum {
    TEXT_CHANGED   = 1 << 0, /* another text at 600 ms, the first again with letters at 900 */
    TWICE_AT_ONCE  = 1 << 1, /* at 600 ms another text, all of it at once, drawn and composed, then the first again
                                with letters at the same moment */
    PACE_CHANGED   = 1 << 2, /* letters at 10 ms, begun again at 3000 ms at 40 ms; the second paint composes at 2900
                                ms, all of it come, and once more while they come again, past the time it held */
    SIZE_CHANGED   = 1 << 3, /* 40 pixels narrower at 500 ms */
    PROGRAM_CANVAS = 1 << 4, /* and the second paint's new canvas made by the program's own dgx_ui_canvas() */
};

typedef struct {
    const char          *name;
    const dgx_ui_look_t *look;
    int                  width, height;
    const char          *text;
    bool                 important, soft;
    int                  events;
    int                  second; /* LATER, EARLIER, IN_TURN */
    int                  every;  /* the second paint composes at every so many passes */
    int                  way;    /* INTO_CANVAS, STRAIGHT, BITMAP */
} case_t;

/* the passes of one case: the passes and composings after which a screen differs from the fresh paint's */
static void run(const case_t *c, dgx_screen_t *display, dgx_screen_t *second, dgx_screen_t *fresh)
{
    make(c->look, c->width, c->height, c->text, c->important, c->soft, c->way);
    dgx_ui_paint_t display_paint = {.scr = display, .colors = colors, .color_count = 2};
    dgx_ui_paint_t second_paint  = {.scr = second, .colors = colors, .color_count = 2};
    dgx_fill_rectangle(display, 0, 0, WIDTH, HEIGHT, 0);
    dgx_fill_rectangle(second, 0, 0, WIDTH, HEIGHT, 0);
    dgx_ui_text_appear(&ui, 1, (c->events & PACE_CHANGED) ? 10 : 40);
    const uint32_t again_at    = c->look ? 4000 : 3600; /* lines take longer */
    int            display_off = 0, second_off = 0, steps = 0;
    bool           more        = true;
    for (uint32_t ms = 100; ms <= 7000; ms += 20, ++steps) {
        if ((c->events & TEXT_CHANGED) && ms == 600) dgx_ui_text_set(&ui, 1, "another text");
        if ((c->events & TEXT_CHANGED) && ms == 900) dgx_ui_text_set(&ui, 1, c->text), dgx_ui_text_appear(&ui, 1, 40);
        if ((c->events & TWICE_AT_ONCE) && ms == 600) {
            dgx_ui_text_set(&ui, 1, "another text"), dgx_ui_text_appear(&ui, 1, 0);
            dgx_ui_paint(&ui, &display_paint, ms);
            second_paint.now_ms = ms;
            dgx_ui_paint_area_to(&ui, &second_paint, second, NULL, 0, 0, WIDTH, HEIGHT);
            dgx_ui_text_set(&ui, 1, c->text), dgx_ui_text_appear(&ui, 1, 40);
        }
        if ((c->events & PACE_CHANGED) && ms == 3000) dgx_ui_text_appear(&ui, 1, 40);
        if ((c->events & SIZE_CHANGED) && ms == 500) label_w.want_w = (int16_t)(c->width - 40), dgx_ui_relayout(&ui);
        more = dgx_ui_paint(&ui, &display_paint, ms);
        display_off += differ_from_fresh(&ui, &display_paint, fresh, 1) > 0;
        if ((c->events & PROGRAM_CANVAS) && ms == 500) {
            bool made;
            CHECK(dgx_ui_canvas(&second_paint, &ui, 1, &made) && made);
        }
        if ((c->events & PACE_CHANGED) ? ms != 2900 && ms != again_at : steps % c->every) continue;
        const bool later    = c->second == LATER || (c->second == IN_TURN && steps / c->every % 2 == 0);
        second_paint.now_ms = later ? ms + 10 : ms - 10;
        dgx_ui_paint_area_to(&ui, &second_paint, second, NULL, 0, 0, WIDTH, HEIGHT);
        second_off += differ_from_fresh(&ui, &second_paint, fresh, 1) > 0;
    }
    if (display_off || second_off) fprintf(stderr, "%s: the display off after %d passes, the second paint after %d\n", c->name, display_off, second_off);
    const int rows = c->height / (c->way == BITMAP ? TerminusTTFMedium12.y_advance : variant[0].height);
    CHECK(display_off == 0 && second_off == 0 && !more && steps > 100);
    if (c->look && rows == 2) CHECK(box.total_lines > rows && (!c->important || box.top == box.total_lines - rows)); /* cut, or scrolled to its end */
    dgx_ui_paint_free(&display_paint), dgx_ui_paint_free(&second_paint), dgx_ui_free(&ui);
}

int main(void)
{
    dgx_screen_t *display = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB), *second = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
    dgx_screen_t *fresh = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB), *mirror = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);

    const dgx_ui_look_t *lines = &dgx_ui_look_hand_lines;
    const case_t         cases[] = {
        {"one line, later", NULL, 300, 24, ONE_LINE, false, false, 0, LATER, 1, INTO_CANVAS},
        {"one line, earlier", NULL, 300, 24, ONE_LINE, false, false, 0, EARLIER, 1, INTO_CANVAS},
        {"one line, in turn", NULL, 300, 24, ONE_LINE, false, false, 0, IN_TURN, 1, INTO_CANVAS},
        {"one line, text changed", NULL, 300, 24, ONE_LINE, false, false, TEXT_CHANGED, LATER, 1, INTO_CANVAS},
        {"one line, text changed, seldom", NULL, 300, 24, ONE_LINE, false, false, TEXT_CHANGED, LATER, 7, INTO_CANVAS},
        {"one line, twice at once", NULL, 300, 24, ONE_LINE, false, false, TWICE_AT_ONCE, LATER, 7, INTO_CANVAS},
        {"one line, pace changed, seldom", NULL, 300, 24, ONE_LINE, false, false, PACE_CHANGED, LATER, 7, INTO_CANVAS},
        {"one line, size changed", NULL, 300, 24, ONE_LINE, false, false, SIZE_CHANGED, LATER, 1, INTO_CANVAS},
        {"one line, the program's canvas", NULL, 300, 24, ONE_LINE, false, false, SIZE_CHANGED | PROGRAM_CANVAS, LATER, 1, INTO_CANVAS},
        {"one line, soft, seldom", NULL, 300, 24, ONE_LINE, false, true, 0, LATER, 7, INTO_CANVAS},
        {"lines, later", lines, 150, 60, NOTE, false, false, 0, LATER, 1, INTO_CANVAS},
        {"lines, earlier", lines, 150, 60, NOTE, false, false, 0, EARLIER, 1, INTO_CANVAS},
        {"lines, in turn", lines, 150, 60, NOTE, false, false, 0, IN_TURN, 1, INTO_CANVAS},
        {"lines, text changed", lines, 150, 60, NOTE, false, false, TEXT_CHANGED, LATER, 1, INTO_CANVAS},
        {"lines, text changed, seldom", lines, 150, 60, NOTE, false, false, TEXT_CHANGED, LATER, 7, INTO_CANVAS},
        {"lines, twice at once", lines, 150, 60, NOTE, false, false, TWICE_AT_ONCE, LATER, 7, INTO_CANVAS},
        {"lines, pace changed, seldom", lines, 150, 60, NOTE, false, false, PACE_CHANGED, LATER, 7, INTO_CANVAS},
        {"lines, size changed", lines, 150, 60, NOTE, false, false, SIZE_CHANGED, LATER, 1, INTO_CANVAS},
        {"lines, the program's canvas", lines, 150, 60, NOTE, false, false, SIZE_CHANGED | PROGRAM_CANVAS, LATER, 1, INTO_CANVAS},
        {"lines, cut", lines, 150, 40, NOTE, false, false, 0, IN_TURN, 1, INTO_CANVAS},
        {"lines, scrolled", lines, 150, 40, NOTE, true, false, 0, IN_TURN, 1, INTO_CANVAS},
        {"lines, scrolled, seldom", lines, 150, 40, NOTE, true, false, 0, LATER, 7, INTO_CANVAS},
        {"lines, soft, seldom", lines, 150, 60, NOTE, false, true, 0, LATER, 7, INTO_CANVAS},
        {"straight: one line, in turn", NULL, 300, 24, ONE_LINE, false, false, TEXT_CHANGED, IN_TURN, 1, STRAIGHT},
        {"straight: lines, scrolled, in turn", lines, 150, 40, NOTE, true, false, TEXT_CHANGED, IN_TURN, 1, STRAIGHT},
        {"bitmap: one line, in turn", NULL, 300, 24, ONE_LINE, false, false, TEXT_CHANGED, IN_TURN, 1, BITMAP},
        {"bitmap: lines, cut, in turn", &dgx_ui_look_lines, 100, 26, NOTE, false, false, TEXT_CHANGED, IN_TURN, 1, BITMAP},
        {"bitmap: lines, scrolled, seldom", &dgx_ui_look_lines, 100, 26, NOTE, true, false, 0, LATER, 7, BITMAP},
    };
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; ++k) run(&cases[k], display, second, fresh);

    /* the mirror of the recipe, composed by the same paint after each pass: the display's screen, and nothing written
       anew into the canvas (a pixel set in a corner no stroke reaches stays) */
    const dgx_ui_look_t *looks[2] = {NULL, &dgx_ui_look_hand_lines};
    for (int k = 0; k < 2; ++k) {
        make(looks[k], k ? 150 : 300, k ? 60 : 24, k ? NOTE : ONE_LINE, false, false, INTO_CANVAS);
        dgx_ui_paint_t paint = {.scr = display, .colors = colors, .color_count = 2};
        dgx_fill_rectangle(display, 0, 0, WIDTH, HEIGHT, 0);
        dgx_ui_text_appear(&ui, 1, 40);
        int  off = 0, kept = 0, composings = 0;
        bool more = true;
        for (uint32_t ms = 100; more && ms <= 6000; ms += 20) {
            more = dgx_ui_paint(&ui, &paint, ms);
            bool          made;
            dgx_screen_t *canvas = dgx_ui_canvas(&paint, &ui, 1, &made);
            const int     corner_x = canvas ? canvas->width - 1 : 0, corner_y = canvas ? canvas->height - 1 : 0;
            if (canvas) dgx_set_pixel(canvas, corner_x, corner_y, 77);
            dgx_fill_rectangle(mirror, 0, 0, WIDTH, HEIGHT, 0);
            dgx_ui_paint_area_to(&ui, &paint, mirror, NULL, 0, 0, WIDTH, HEIGHT);
            ++composings;
            if (canvas) {
                kept += dgx_get_pixel(canvas, corner_x, corner_y) == 77;
                dgx_set_pixel(canvas, corner_x, corner_y, 0);
            }
            const dgx_ui_node_t *node = dgx_ui_node(&ui, 1);
            for (int y = node->y; y < node->y + node->h; ++y)
                for (int x = node->x; x < node->x + node->w; ++x)
                    off += !(x == node->x + corner_x && y == node->y + corner_y) && dgx_get_pixel(mirror, x, y) != dgx_get_pixel(display, x, y);
        }
        CHECK(!more && composings > 20 && kept == composings && off == 0);
        dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    }

    display->destroy(&display), second->destroy(&second), fresh->destroy(&fresh), mirror->destroy(&mirror);
    CHECK_DONE();
}
