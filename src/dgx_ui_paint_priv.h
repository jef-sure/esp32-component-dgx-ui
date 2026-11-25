#pragma once
/*
 * dgx_ui_paint_priv.h
 *
 *  What the painter and the looks of a box and a frame share: the look a widget is drawn by.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_font.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_priv.h"

/* the look of a widget: its own, else its kind's; NULL: nothing draws it */
const dgx_ui_look_t *dgx_ui_look_of(const dgx_ui_widget_t *w);

/* the part of the paint's buffer a look may use now, its bytes in *size: past the painter's strip while the widget is
   drawn in strips over the buffer (a display of 18 or 24 bits), else all of it; NULL none */
void *dgx_ui_buffer_free(const dgx_ui_paint_t *paint, size_t *size);

/* the painter's strips, named by the program (dgx_ui_strips, dgx_ui_strips.c): a part r of widget i drawn through the
   buffer, *more set when the look asks for time; false, not drawn: the painter draws the part straight */
struct dgx_ui_strips_t {
    bool (*draw)(dgx_ui_paint_t *paint, const dgx_ui_look_t *look, dgx_ui_t *ui, int i, uint32_t now_ms, dgx_ui_rect_t r, bool *more);
    /* pictures on a screen of 18 or 24 bits (dgx_ui_strips_pictures, dgx_ui_deep.c): w x h pixels of a source, a row
       every `stride` of them, put at x, y of the tree's screen, each pixel k x k, through a strip of the screen's depth
       in the free part of the buffer past `used` bytes, the caller's own strip of indices; 8-bit indices of the
       painter's table (transparent: 0 leaves the drawn background), or RGB565. false: no room, or no memory: nothing
       drawn. NULL in dgx_ui_strips */
    bool (*put_indexed)(dgx_ui_paint_t *paint, int x, int y, const uint8_t *src, int stride, int w, int h, int k, bool transparent, size_t used);
    bool (*put_rgb565)(dgx_ui_paint_t *paint, int x, int y, const uint16_t *src, int stride, int w, int h, int k);
};

/* the strips' draw, kept_half: the strip takes half the free room at most, the rest a look's of pictures */
bool dgx_ui_strips_draw(dgx_ui_paint_t *paint, const dgx_ui_look_t *look, dgx_ui_t *ui, int i, uint32_t now_ms, dgx_ui_rect_t r, bool *more,
                        bool kept_half);

/* the bytes a row of a source w pixels wide at k x k takes in the strip of the screen's depth: 3 a pixel */
static inline size_t dgx_ui_deep_row(int w, int k)
{
    return (size_t)3 * (size_t)w * (size_t)k * (size_t)k;
}

/* the scrolling boxes' part of a paint (dgx_ui_scroll_look.c), reached by the painter through these two only: a program
   of no scrolling box carries none of it */
struct dgx_ui_scroller_t {
    void (*before)(dgx_ui_paint_t *paint, dgx_ui_t *ui);          /* before a pass: an area of the panel let go and composed */
    void (*free)(dgx_ui_paint_t *paint);                          /* the area let go, the state freed */
};

/* a place a cover left composed before the walk, as the painter's own composing of it: a widget the walk will draw
   left to it, marked whole */
void dgx_ui_compose_left(dgx_ui_t *ui, dgx_ui_paint_t *paint, dgx_ui_rect_t area);

/* the children of widget i composed within an area of the tree's screen, while its look draws: each drawn whole under
   a clip of its part of the area, one the walk will draw left to it; the paint's state for the look given back */
void dgx_ui_compose_in(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, dgx_ui_rect_t area);

/* the drawn halves of laying the paper and of letters in a field (dgx_ui_background.c), named by the program through
   dgx_ui_background_set(): the drawn background under a clip of a rectangle of the tree's screen; letters over it */
struct dgx_ui_drawn_background_t {
    void (*lay)(dgx_ui_paint_t *paint, int x, int y, int w, int h);
    void (*text)(dgx_ui_paint_t *paint, const dgx_font_context_t *ctx, int x, int y, const char *text, uint8_t paper);
};

/* the canvases of a paint (dgx_ui_canvas.c), reached by the painter through these two only: a program with no canvas
   carries none of them */
struct dgx_ui_canvases_t {
    void (*end)(dgx_ui_paint_t *paint, dgx_ui_t *ui); /* after a pass over ui: the canvases of widgets gone freed, NO_MEMORY told */
    void (*free)(dgx_ui_paint_t *paint);              /* all freed, the table too */
};

/* memory a look keeps for widget i from pass to pass, of at least size bytes, in the paint's table of canvases
   (dgx_ui_canvas.c): freed when the widget goes, as a canvas is; *made: new or larger, its contents not set. NULL:
   none to be had, the look does without */
void *dgx_ui_kept(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, size_t size, bool *made);

/* the canvas of widget i as dgx_ui_canvas() gives it, and in *note sixteen bytes a look keeps with it (dgx_ui_canvas.c),
   the paint's as the canvas is: zeros when the canvas is made or made again, gone with it; what they mean is the
   look's. NULL in *note with no canvas */
enum { DGX_UI_CANVAS_NOTE = 16 };
dgx_screen_t *dgx_ui_canvas_noted(dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, bool *made, void **note);

/* a look's paper, an index or DGX_UI_PAPER, is the program's drawn background here: laid by dgx_ui_lay_paper(), and
   the letters and pictures over it drawn without paper */
static inline bool dgx_ui_paper_drawn(const dgx_ui_paint_t *paint, uint8_t paper)
{
    return paper == DGX_UI_PAPER && paint->on_background;
}

/* letters in a field of a look, as dgx_font_draw() draws them with ctx->field at x, y of paint's screen, the field's
   paper `paper`, an index or DGX_UI_PAPER (ctx->bg its colour). On a drawn background the field's rectangle is laid
   and the letters drawn over it without a field, set where the field sets them and cut to it */
void dgx_ui_field_text(dgx_ui_paint_t *paint, const dgx_font_context_t *ctx, int x, int y, const char *text, uint8_t paper);

/* a rectangle of the tree's screen composed, as dgx_ui_paint_area() does it; background: only where the drawn
   background shows (dgx_ui_background_changed()), nothing within a widget whose look fills */
void dgx_ui_compose(dgx_ui_t *ui, dgx_ui_paint_t *paint, dgx_ui_rect_t area, bool background);

/*
 * The focus of a widget, drawn by a look as a frame of a point round a rectangle of its box (the box, or a button's
 * face), inside it. dgx_ui_frame_begin() narrows the screen's clip to the rectangle less the frame while the widget has
 * the focus, so that the look draws its inside as it does without the focus; dgx_ui_frame_end() gives the clip back
 * and lays the frame's four sides in its colour, every pixel once. Without the focus, or with a colour of DGX_UI_PAPER
 * (the parameters' word for "no focus shown"), neither does anything.
 *
 * Between them the look may narrow the clip it finds and give back the one it found, as the text's field and
 * dgx_ui_lay_paper() do; it may not set a clip of its own choosing, nor draw outside the rectangle's part of the box.
 * begin() returns false when nothing of the inside lies within the clip: the look draws nothing then.
 */
typedef struct dgx_ui_frame_t {
    dgx_ui_rect_t rect;    /* the rectangle framed, in pixels of the tree's screen; w 0: no focus, no frame */
    int           clip[4]; /* the clip as it was */
    uint8_t       color;   /* the frame's: an index of the table */
} dgx_ui_frame_t;
bool dgx_ui_frame_begin(dgx_ui_paint_t *paint, const dgx_ui_t *ui, int i, dgx_ui_rect_t rect, uint8_t color, dgx_ui_frame_t *frame);
void dgx_ui_frame_end(dgx_ui_paint_t *paint, const dgx_ui_t *ui, const dgx_ui_frame_t *frame);

/* a button's face over its shadow, the face's inside drawn by `inside` at fx, fy, fw by fh, in the face's colour and the
   ink of the state (DGX_UI_PRESSED, DGX_UI_DISABLED): pressed, the face moves onto the shadow; the focus a frame round
   the face, moving with it (dgx_ui_button_look_t, dgx_ui_draw_button()). Shared by the looks of a button and of a
   picture button */
typedef void (*dgx_ui_face_fn)(dgx_ui_paint_t *paint, const dgx_ui_node_t *n, int fx, int fy, int fw, int fh, uint32_t face, uint32_t ink, int unit);
void dgx_ui_draw_button_face(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, dgx_ui_face_fn inside);

/* a control drawn with no place for its value (its widget's .value NULL), told in the log by the look that draws it,
   once a look: its number, its tag and its kind in words (dgx_ui_control_looks.c). A program of no control carries
   none of the words */
void dgx_ui_tell_no_place(int i, const dgx_ui_widget_t *w, const char *kind, bool *told);
