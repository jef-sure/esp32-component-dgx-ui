#pragma once
/*
 * dgx_ui_text.h
 *
 *  The text box of a widget: its string, in a buffer the program gives, and how it is fitted to its field. The fit
 *  picks the first variant of the style the text goes into whole; if none, the one that shows the most letters.
 *  Bitmap fonts are never scaled: a smaller size is another variant. The fit is made by the painter, after the
 *  layout: setting a text only keeps and compares it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "dgx_font.h"
#include "dgx_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief The library's handwriting, named by a variant of a handwritten font: &dgx_ui_handwriting. */
typedef struct dgx_ui_hand_t dgx_ui_hand_t;

/**
 * @brief A font a text may be drawn in.
 *
 * A handwritten font (DGX_FONT_HW) is drawn by the one-line look of a text when its variant names the library's
 * handwriting, so that a program of bitmap fonts carries none of it:
 *
 *     {.font = &font0ss, .height = 24, .narrow = 70, .joined = true, .pen = 2, .hand = &dgx_ui_handwriting}
 *
 * a line 24 pixels high that may be narrowed to 70% across to go in, in whole percents, its letters joined, written
 * by a pen two pixels wide. For a soft pen, `.pen = 2 | DGX_PEN_SOFT` with `.hand = &dgx_ui_handwriting_canvas` on a
 * widget of DGX_UI_CANVAS, whose 8-bit canvas holds the amounts of ink; written straight, a soft pen is the hard pen
 * of its width, told once in the log. A text too long for its box even narrowed is cut by the box. Letters coming
 * (dgx_ui_text_appear()) are written stroke by stroke at the pace of a pen, the whole text taking as long as so many
 * letters of letter_ms.
 *
 * A bitmap font taken as it is, not made for the board by a need, as a pixel font of a game: `.points = true` draws its
 * every pixel as a square of dgx_ui_init()'s unit x unit pixels, crisp, no smoothing, and every measure of its text,
 * the line's height, the widths, the fit of a ladder, the ellipsis, the words laid into lines, is so many times its
 * own; a soft font's pixel is a square of its coverage. In a program of one tree for every board a font made by a need
 * under DGX_FONTS_SCALE is at the board's size already: `.points = !DGX_FONT_TIME_MADE`.
 */
typedef struct dgx_ui_font_variant_t {
    const dgx_font_t    *font;
    int16_t              height; ///< a handwritten font: the height of a line in pixels; a bitmap font: 0, its own size
    uint8_t              narrow; ///< a handwritten font: its x may shrink to so many percent of y first; 0 never
    bool                 joined; ///< a handwritten font: its letters joined
    uint32_t             pen;    ///< a handwritten font: the width of the pen in pixels, DGX_PEN_SOFT added for a soft one; 0 is a pixel, a hairline
    const dgx_ui_hand_t *hand;   ///< a handwritten font: &dgx_ui_handwriting or &dgx_ui_handwriting_canvas; NULL, the variant is skipped
    bool                 points; ///< a bitmap font: every pixel of it a point, unit x unit pixels, crisp (below); a handwritten one: read as nothing
} dgx_ui_font_variant_t;

/** @brief Letters coming in handwriting: a pass asked for every so many ms, fifty frames a second. A pass called
 * sooner, for another widget, writes on to its own time. */
enum { DGX_UI_HAND_FRAME_MS = 20 };

/** @brief The library's handwriting, for dgx_ui_font_variant_t::hand: straight on the screen, by a hard pen. */
extern const dgx_ui_hand_t dgx_ui_handwriting;
/**
 * @brief The same, and on a widget of DGX_UI_CANVAS into its canvas, where a soft pen is soft. Needs the virtual
 * screens of DGX (CONFIG_DGX_ENABLE_VSCREEN); a program that names only dgx_ui_handwriting carries no canvas.
 */
extern const dgx_ui_hand_t dgx_ui_handwriting_canvas;

/** Flags of a style. */
enum {
    DGX_UI_TEXT_MIDDLE    = 1u << 0, ///< a text in a smaller variant stands in the middle of the field's height, not on the first one's base line
    DGX_UI_TEXT_IMPORTANT = 1u << 1, ///< a text too long scrolls, a line up every scroll_ms, rather than being cut
};

/** @brief How a text is drawn: its fonts, the best first, and where it stands in its field. */
typedef struct dgx_ui_text_style_t {
    const dgx_ui_font_variant_t *fonts;    ///< the variants of a font tried in turn, the best first
    uint8_t                      count;
    uint8_t                      flags;     ///< DGX_UI_TEXT_*
    dgx_font_align_t             align;     ///< each line in the field
    uint16_t                     scroll_ms; ///< an important text: a line up every so many ms; 0 by hand
} dgx_ui_text_style_t;

/** Flags of a text box, the library's. */
enum {
    DGX_UI_TEXTBOX_START   = 1u << 0, ///< the times of scrolling and appearing start at the next pass of the painter
    DGX_UI_TEXTBOX_CAME    = 1u << 1, ///< all its letters came: no time counted for them any more
    DGX_UI_TEXTBOX_ENDED   = 1u << 2, ///< an important text scrolled to its end: no time counted for it any more
    DGX_UI_TEXTBOX_COUNTED = 1u << 3, ///< lines and letters counted for the text, width and variant as they are
    DGX_UI_TEXTBOX_SEEN    = 1u << 4, ///< its look showed all it will show of the text (dgx_ui_text_state())
    DGX_UI_TEXTBOX_WHOLE   = 1u << 5, ///< its letters shown whole by the program (dgx_ui_text_whole()): no time for them
};

/**
 * @brief A text box: the program's buffer of the string, its style and the lines it shows. The text is laid into
 * lines, word by word, each time it is drawn: no table of lines is kept. A text box is one widget's; a second widget
 * showing the same text has a box of its own.
 *
 * A box of size 0 is over a string constant (DGX_UI_TEXT_CONST()): the library never writes its buffer, and setting
 * its text changes nothing. The box itself is still the program's RAM, as the library keeps in it the fit, the line on
 * top and the letters come.
 */
struct dgx_ui_textbox_t {
    char                      *buf;       ///< the program's: the string, cut to the size
    uint16_t                   size;      ///< bytes of buf, the 0 at the end included; 0: a constant's, never written
    uint8_t                    lines;     ///< the lines it shows: its height is so many lines of the first variant
    uint8_t                    variant;   ///< the variant the text was fitted in, by the painter
    const dgx_ui_text_style_t *style;
    uint16_t                   top;       ///< the first line shown: an important text scrolls, else dgx_ui_text_scroll(); a list's first row shown, kept with its pick in view
    uint8_t                    letter_ms; ///< appearing: a letter every so many ms; 0 all at once
    uint8_t                    flags;     ///< DGX_UI_TEXTBOX_*
    uint32_t                   since_ms;  ///< when the text was first drawn: its scroll and its letters count from it
    uint16_t                   letters_drawn; ///< the look of lines: the letters come when it last drew; handwriting: the ms of its time written
    uint16_t                   total_lines, total_letters; ///< the look of lines: of the text laid, while COUNTED
};

#if UINTPTR_MAX == 0xFFFFFFFFu
#ifdef __cplusplus
static_assert(sizeof(dgx_ui_font_variant_t) == 20, "a variant of a font is 20 bytes on a 32-bit chip");
static_assert(sizeof(dgx_ui_textbox_t) == 28, "a text box is 28 bytes on a 32-bit chip");
#else
_Static_assert(sizeof(dgx_ui_font_variant_t) == 20, "a variant of a font is 20 bytes on a 32-bit chip");
_Static_assert(sizeof(dgx_ui_textbox_t) == 28, "a text box is 28 bytes on a 32-bit chip");
#endif
#endif

/**
 * @brief A text box over a string constant, of so many lines in a style: no buffer of RAM, no call to fill it.
 *
 *     static dgx_ui_textbox_t yes = DGX_UI_TEXT_CONST("Yes", 1, &style);
 *     static const dgx_ui_widget_t yes_w = {.kind = DGX_UI_BUTTON, .text = &yes, .look = &dgx_ui_look_button};
 *
 * A list's rows and a dropdown's options take it too ("Low\nMiddle\nHigh", 3). The string may lie under any number of
 * boxes; a box is one widget's, so two widgets of one caption are two boxes over one string. Another constant is
 * shown by pointing the box at it and telling the widget: `yes.buf = (char *)"Oui"; dgx_ui_text_written(ui, i);`.
 * Every member is named, in its order, so that a program in C++ built with -Wextra takes it without a word.
 */
#define DGX_UI_TEXT_CONST(text, line_count, text_style)                                                                 \
    {.buf = (char *)(text), .size = 0, .lines = (line_count), .variant = 0, .style = (text_style), .top = 0, .letter_ms = 0, \
     .flags = 0, .since_ms = 0, .letters_drawn = 0, .total_lines = 0, .total_letters = 0}

/** @brief The height of a line of a variant in a tree: its own for a handwritten font, the font's advance for a bitmap
    one, times the tree's unit for a variant of points; ui NULL, a unit of 1. */
int dgx_ui_variant_line(const dgx_ui_t *ui, const dgx_ui_font_variant_t *v);

/**
 * @brief A text box over the program's buffer, empty, of so many lines in a style.
 * @param size Bytes of buf; a text set is cut to size - 1 bytes.
 */
void dgx_ui_textbox_init(dgx_ui_textbox_t *box, char *buf, int size, int lines, const dgx_ui_text_style_t *style);

/**
 * @brief The text of a widget set as by printf(), cut to its buffer on a whole letter; nothing is drawn or
 * measured here.
 *
 * A buffer of up to 256 bytes is compared with the text before: the same text changes nothing. A longer buffer
 * is taken as changed every time.
 * @return true when the text changed: the widget is marked to be fitted and drawn again.
 */
bool dgx_ui_text_printf(dgx_ui_t *ui, int i, const char *format, ...) __attribute__((format(printf, 3, 4)));

/** @brief The same with a va_list. */
bool dgx_ui_text_vprintf(dgx_ui_t *ui, int i, const char *format, va_list args);

/** @brief The text of a widget set as it is. */
bool dgx_ui_text_set(dgx_ui_t *ui, int i, const char *text);

/**
 * @brief The program wrote the buffer of widget i's text box itself, or pointed the box at another string: the widget
 * is fitted and drawn again, its text begun anew, and the lists over its tree that show the box are told, as by
 * dgx_ui_text_set(). A buffer with no 0 within its size is ended at size - 1 on a whole letter.
 *
 * It does not look whether the text differs: a program that calls it at every turn of its loop has the widget drawn
 * at every pass. The buffer is written by the task that owns the tree, as any value is; another task sends a message.
 * Two boxes over one buffer are two widgets, each told. No widget, no box or no buffer: nothing.
 */
void dgx_ui_text_written(dgx_ui_t *ui, int i);

/**
 * @brief The text of a widget appears letter by letter from the next pass of the painter, a letter every so many
 * ms; 0 shows it at once. A text set after this appears too; the painter asks for time while it appears.
 */
void dgx_ui_text_appear(dgx_ui_t *ui, int i, int letter_ms);

/** What a widget's text has shown, as the painter last drew it: bits of dgx_ui_text_state(). */
enum {
    DGX_UI_TEXT_COMING = 1u << 0, ///< its letters still coming (dgx_ui_text_appear()), or not drawn yet since they were asked to
    DGX_UI_TEXT_SEEN   = 1u << 1, ///< its look showed all it will show of it (below)
};

/**
 * @brief What widget i's text has shown: DGX_UI_TEXT_COMING, DGX_UI_TEXT_SEEN, or neither: drawn, every letter come,
 * an important text still scrolling to its end (or a text shown at once not drawn yet). 0 for no widget or no text.
 *
 * SEEN is set by the painter's own pass when the widget's look drew it, wholly or the part a cover leaves, and asked
 * for no more time; a text set anew, or dgx_ui_text_appear(), clears it. It is "the look showed all it will show", not
 * "every letter is on the screen": a text of one line cut with an ellipsis, or lines of a text that is not important
 * cut by a box too small, are SEEN at their first pass. By the way the text is drawn:
 *  - one line in a bitmap font (dgx_ui_look_text, and the captions of buttons, fields, lists, checks and dropdowns):
 *    no letters come; SEEN from its first pass;
 *  - lines in a bitmap font (dgx_ui_look_lines): COMING while its letters come; SEEN once all came and, an important
 *    text longer than its box, its last line is shown;
 *  - one handwritten line: COMING while the pen writes it; SEEN once written;
 *  - handwritten lines (dgx_ui_look_hand_lines): as lines in a bitmap font.
 * A hidden widget, or one wholly under a cover, is not drawn, and not SEEN.
 */
int dgx_ui_text_state(const dgx_ui_t *ui, int i);

/**
 * @brief Widget i's letters still coming shown whole at the next pass of the painter, as if they had all come; an
 * important text's scroll then counts from that pass, its first lines shown scroll_ms as always. A text whose letters
 * all came, scrolling or at its end, is left as it is: its scroll never begins again. The pace of dgx_ui_text_appear()
 * stays, so a text set after this appears again. No widget, no text: nothing.
 */
void dgx_ui_text_whole(dgx_ui_t *ui, int i);

/**
 * @brief The lines of a text that is not important moved by hand: so many up (down when negative), as far as there
 * are. Of a list that scrolls, its window, within its rows: the pick stays where it is, shown or not, and the next
 * change of the value brings the window back to it.
 */
void dgx_ui_text_scroll(dgx_ui_t *ui, int i, int lines);

#ifdef __cplusplus
}
#endif
