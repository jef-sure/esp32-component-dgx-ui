/*
 * dgx_ui_strips.c
 *
 *  The painter drawing through the buffer in strips, named by the program: paint's strips = &dgx_ui_strips. On a
 *  display behind a bus of 18 or 24 bits a field of text is filled before its letters are written, and blinks; and on
 *  any, the letters over a drawn background come after its piece. There every widget without children is drawn whole
 *  in strips of a virtual screen of the display's depth over the buffer, each strip sent by one transfer: every pixel
 *  once, and the soft edges of letters mixed with the picture under them, which a strip can read. It needs the virtual screens of DGX (CONFIG_DGX_ENABLE_VSCREEN):
 *  named without them, the program does not link. A program that does not name it does not carry it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_paint_priv.h"
#include "dgx_vscreen.h"
#include "esp_log.h"

static const char *TAG = "dgx_ui";

/*
 * A part r of widget i drawn through the buffer: strip by strip of as many rows as the free room takes, each a virtual
 * screen of the display's depth over the buffer (set there, it has no clip), the look drawing it whole there (a strip
 * holds nothing else), sent by one transfer within the part, under its clip. The look remembers what it drew only in
 * the last strip: the others are drawn as composing. false, not drawn: a widget with children (it fills around them,
 * which a strip would send as what it held), a display that does not blink (16 bits off a drawn background, a virtual
 * screen), no room for a row of the part, or no memory for the strip's screen; the painter draws the part straight.
 */
bool dgx_ui_strips_draw(dgx_ui_paint_t *p, const dgx_ui_look_t *look, dgx_ui_t *ui, int i, uint32_t now_ms, dgx_ui_rect_t r, bool *more, bool kept_half)
{
    dgx_screen_t *scr  = p->scr;
    const uint8_t bits = scr->color_bits;
    if (dgx_ui_at(ui, i)->child >= 0 || scr->screen_subtype != DgxPhysicalScreenWithBus || (bits != 18 && bits != 24 && !p->on_background)) return false;
    size_t       room;
    void        *buffer = dgx_ui_buffer_free(p, &room);
    if (kept_half) room /= 2; /* the rest a look's of pictures: its row and its strip of the screen's depth */
    const size_t row    = dgx_color_points_to_bytes(bits, (uint32_t)r.w);
    const int    fit    = buffer && row ? (int)(room / row) : 0;
    const int    rows   = fit < r.h ? fit : r.h; /* no more than the part: the rest of the room is a look's own */
    if (rows < 1) {
        static bool told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
        if (!told) ESP_LOGW(TAG, "widget %d: drawn straight, and blinks: the paint's buffer takes no row of it (%u bytes)", i, (unsigned)row);
        told = true;
        return false;
    }
    if (p->strip && p->strip->color_bits != bits) dgx_screen_destroy(&p->strip); /* another screen given */
    if (!p->strip) p->strip = dgx_vscreen_from_array(buffer, r.w, 1, bits, scr->rgb_order);
    if (!p->strip) return false;
    const int16_t ox = p->ox, oy = p->oy;
    const bool    whole = p->whole, composing = p->composing;
    p->buffer_used = (size_t)rows * row; /* the free room for a look of its own is past the strip */
    for (int y0 = r.y; y0 < r.y + r.h; y0 += rows) {
        const int h = r.y + r.h - y0 < rows ? r.y + r.h - y0 : rows;
        dgx_vscreen_array_set(p->strip, buffer, r.w, h);
        p->scr = p->strip, p->ox = r.x, p->oy = (int16_t)y0;
        p->whole     = true;
        p->composing = composing || y0 + h < r.y + r.h; /* the look stores in the last part only, by its own test */
        *more |= look->draw(look, p, ui, i, now_ms);
        p->scr = scr, p->ox = ox, p->oy = oy;
        dgx_vscreen_to_screen(scr, r.x - ox, y0 - oy, p->strip, 0);
    }
    p->buffer_used = 0;
    p->whole       = whole; /* composing is as it was: the last strip has it so */
    return true;
}

static bool draw(dgx_ui_paint_t *p, const dgx_ui_look_t *look, dgx_ui_t *ui, int i, uint32_t now_ms, dgx_ui_rect_t r, bool *more)
{
    return dgx_ui_strips_draw(p, look, ui, i, now_ms, r, more, false);
}

const dgx_ui_strips_t dgx_ui_strips = {.draw = draw};
