/*
 * The painter skips what nothing touched, and the hit goes down only where the finger is: a random run of calls on a
 * tree (widgets added, removed, hidden, shown, disabled, set; texts set; the finger, the keys, a popup, the
 * keyboard's band moved), and after every pass the panel as the whole tree composed from nothing by
 * dgx_ui_paint_area(), which walks all; a widget the painter skipped wrongly is a difference. The widget under a
 * point as a search of every widget finds it.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_image.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t               colors[4]     = {0x0000, 0xffff, 0x2104, 0xf800};
static const uint8_t          cross_bits[8] = {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81};
static const dgx_image_t      cross_im      = {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = cross_bits};
static const dgx_ui_picture_t icons[2]      = {{.image = &cross_im}, {.image = &cross_im}};

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;

enum { LABELS = 24 };
static char             label_buf[LABELS][12];
static dgx_ui_textbox_t label_box[LABELS];
static dgx_ui_widget_t  label_w[LABELS];

/* what may be added: holders and controls, some asking for places out of their box */
static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1, .gap = 1};
static const dgx_ui_widget_t kinds[] = {
    {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1, .gap = 1, .want_h = 20},
    {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 1, .gap = 1, .want_h = 14},
    {.kind = DGX_UI_BOX, .want_h = 16},                                             /* children where they ask */
    {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .want_h = 22},
    {.kind = DGX_UI_BUTTON, .want_w = 20, .want_h = 8, .look = &dgx_ui_look_button},
    {.kind = DGX_UI_BUTTON, .want_w = 120, .want_h = 6, .at_x = -4, .look = &dgx_ui_look_button}, /* out of its box */
    {.kind = DGX_UI_SLIDER, .want_w = 30, .want_h = 6, .max = 10, .look = &dgx_ui_look_slider},
    {.kind = DGX_UI_SWITCH, .want_w = 12, .want_h = 6, .max = 1, .look = &dgx_ui_look_switch},
    {.kind = DGX_UI_ICON, .picture = icons, .max = 1, .want_w = 10, .want_h = 9, .at_x = 3, .at_y = 2, .look = &dgx_ui_look_picture},
    {.kind = DGX_UI_BOX, .want_h = 10, .flags = DGX_UI_COLUMN | DGX_UI_REST},
    {.kind = DGX_UI_LIST, .flags = DGX_UI_COLUMN, .want_h = 24, .max = 3},                         /* a holder too */
    {.kind = DGX_UI_SLIDER, .flags = DGX_UI_RELATIVE, .want_w = 40, .want_h = 6, .max = 10, .look = &dgx_ui_look_slider},
};
enum { KINDS = sizeof kinds / sizeof kinds[0] };

/* a widget of a kind made at run time, with a place of its own for its value */
static dgx_ui_widget_t made[4096];
static int32_t         made_value[4096];
static int             made_count;
static const dgx_ui_widget_t *made_of(int kind)
{
    const int k   = made_count++ % 4096;
    made[k]       = kinds[kind];
    made_value[k] = 0;
    made[k].value = &made_value[k];
    return &made[k];
}

static const dgx_ui_widget_t pop_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2};
static const dgx_ui_widget_t pop_bt = {.kind = DGX_UI_BUTTON, .want_h = 10, .look = &dgx_ui_look_button};

static uint32_t seed = 12345;
static int      rnd(int n)
{
    seed = seed * 1103515245u + 12345u;
    return n > 0 ? (int)((seed >> 16) % (unsigned)n) : 0;
}

/* a live widget picked at random, -1 none; holders only when asked: a box of neither a column nor a row only while
   it holds none, as children that lie over each other are not drawn as the tree's order says (only popups do) */
static int pick(const dgx_ui_t *ui, bool holder, bool root_too)
{
    int n = 0, at[256];
    for (int i = root_too ? 0 : 1; i < ui->count && n < 256; ++i) {
        const dgx_ui_widget_t *w = dgx_ui_widget(ui, i);
        if (!w) continue;
        const bool free_box = !(w->flags & (DGX_UI_COLUMN | DGX_UI_ROW)) && dgx_ui_node(ui, i)->child >= 0;
        if (!holder || ((w->kind == DGX_UI_BOX || w->kind == DGX_UI_FRAME || w->kind == DGX_UI_LIST) && !free_box)) at[n++] = i;
    }
    return n ? at[rnd(n)] : -1;
}

/* the widget under a point by a search of every one, in the tree's order: the deepest usable, of two as deep the
   smaller, of two as small the first; seen only within its parent's box */
static bool answers(const dgx_ui_widget_t *w)
{
    return w->kind == DGX_UI_LIST || w->kind == DGX_UI_SWITCH || w->kind == DGX_UI_SLIDER || w->kind == DGX_UI_BUTTON ||
           w->kind == DGX_UI_INPUT || w->kind == DGX_UI_DROPDOWN;
}
static void search(const dgx_ui_t *ui, int i, int d, bool off, int x, int y, int *best, int *depth, long *area)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    off |= (n->state.bits & (DGX_UI_HIDDEN | DGX_UI_DISABLED)) != 0;
    const bool in = x >= n->x && y >= n->y && x < n->x + n->w && y < n->y + n->h;
    off |= !in; /* nothing of it outside its box: nothing of those under it either */
    const long a  = (long)n->w * n->h;
    if (!off && answers(n->widget) && (d > *depth || (d == *depth && a < *area))) *best = i, *depth = d, *area = a;
    for (int c = n->child; c >= 0; c = dgx_ui_node(ui, c)->next) search(ui, c, d + 1, off, x, y, best, depth, area);
}

/* the panel and the composed screen differ, outside the keyboard's band (the keyboard's own) */
static int differ(const panel_t *p, dgx_screen_t *v, dgx_ui_rect_t band)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x)
            if (!(x >= band.x && y >= band.y && x < band.x + band.w && y < band.y + band.h)) n += p->mem[y][x] != dgx_get_pixel(v, x, y);
    return n;
}

static panel_t       *panel;
static dgx_screen_t  *v;
static dgx_ui_paint_t pp, pv;
static dgx_ui_t       ui;
static int            differences, wrong_hits, passes, at_step;
static uint32_t       now;

/* a pass of the painter, then the panel against the tree composed from nothing, and the widget under some points */
static void pass(void)
{
    dgx_ui_paint(&ui, &pp, now += 7);
    ++passes;
    dgx_ui_paint_area(&ui, &pv, 0, 0, PANEL_W, PANEL_H);
    const int d = differ(panel, v, pp.cover);
    if (d && !differences) fprintf(stderr, "step %d: %d pixels differ\n", at_step, d);
    if (d && !differences && getenv("SKIP_DUMP")) { /* where, and the tree then */
        int x0 = 999, y0 = 999, x1 = -1, y1 = -1;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x)
                if (panel->mem[y][x] != dgx_get_pixel(v, x, y)) x0 = x < x0 ? x : x0, y0 = y < y0 ? y : y0, x1 = x > x1 ? x : x1, y1 = y > y1 ? y : y1;
        fprintf(stderr, "  differ in %d,%d .. %d,%d; pressed %d focus %d\n", x0, y0, x1, y1, ui.pressed, ui.focus);
        for (int j = 0; j < ui.count; ++j) {
            const dgx_ui_node_t *n = dgx_ui_node(&ui, j);
            if (!n->widget) continue;
            fprintf(stderr, "  %2d parent %2d kind %2d at %d,%d %dx%d bits %x/%x value %d/%d marks %x%s\n", j, n->parent, n->widget->kind, n->x, n->y, n->w, n->h,
                    n->state.bits, n->state_drawn.bits, dgx_ui_value_of(n), n->state_drawn.value, n->marks, dgx_ui_hidden(&ui, j) ? " hidden" : "");
        }
    }
    differences += d != 0;
    for (int k = 0; k < 4; ++k) {
        const int x = rnd(PANEL_W), y = rnd(PANEL_H);
        int       best = -1, depth = -1;
        long      area = 0;
        search(&ui, 0, 0, false, x, y, &best, &depth, &area);
        if (dgx_ui_hit(&ui, x, y) != best) {
            if (!wrong_hits) fprintf(stderr, "step %d: at %d,%d hit %d, a search finds %d\n", at_step, x, y, dgx_ui_hit(&ui, x, y), best);
            ++wrong_hits;
        }
    }
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.fonts = variant, .count = 1};
    if (getenv("SKIP_SEED")) seed = (uint32_t)strtoul(getenv("SKIP_SEED"), NULL, 10); /* another run */
    for (int k = 0; k < LABELS; ++k) {
        dgx_ui_textbox_init(&label_box[k], label_buf[k], sizeof label_buf[k], 1, &style);
        label_w[k] = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &label_box[k], .want_w = (int16_t)(16 + k), .want_h = 12};
    }
    panel = panel_make();
    v     = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    pp    = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
    pv    = (dgx_ui_paint_t){.scr = v, .colors = colors, .color_count = 4};
    static dgx_ui_node_t first[8], pop_n[4];
    dgx_ui_t             pop;
    dgx_ui_init(&ui, first, 8, 1);
    dgx_ui_add(&ui, -1, &root_w);
    dgx_ui_init(&pop, pop_n, 4, 1);
    dgx_ui_add(&pop, -1, &pop_w);
    dgx_ui_add(&pop, 0, &pop_bt);
    bool label_used[LABELS] = {0};

    for (int step = 0; step < 3000; ++step) {
        at_step = step;
        const int what = rnd(16);
        int       i;
        switch (what) {
        case 0:
        case 1: /* a widget added, a label sometimes */
            if ((i = pick(&ui, true, true)) >= 0 && ui.count < 200) {
                const int l = rnd(LABELS);
                if (rnd(3) == 0 && !label_used[l]) {
                    const int t = dgx_ui_insert(&ui, i, rnd(2) ? -1 : dgx_ui_node(&ui, i)->child, &label_w[l]);
                    if (t >= 0) label_used[l] = true, dgx_ui_text_printf(&ui, t, "%d", rnd(1000));
                } else
                    dgx_ui_add(&ui, i, made_of(rnd(KINDS)));
            }
            break;
        case 2: /* one removed, with what is under it */
            if ((i = pick(&ui, false, false)) >= 0) {
                for (int k = 0; k < LABELS; ++k) label_used[k] = false;
                dgx_ui_remove(&ui, i);
                for (int j = 0; j < ui.count; ++j) /* the labels still in the tree */
                    for (int k = 0; k < LABELS; ++k)
                        if (dgx_ui_widget(&ui, j) == &label_w[k]) label_used[k] = true;
            }
            break;
        case 3: if ((i = pick(&ui, false, false)) >= 0) dgx_ui_show(&ui, i, dgx_ui_hidden(&ui, i)); break;
        case 4: if ((i = pick(&ui, false, false)) >= 0) dgx_ui_set_value(&ui, i, rnd(12)); break;
        case 5: if ((i = pick(&ui, false, false)) >= 0) dgx_ui_set_state(&ui, i, DGX_UI_DISABLED, rnd(2)); break;
        case 6:
            for (int k = 0; k < LABELS; ++k)
                for (int j = 0; j < ui.count; ++j)
                    if (dgx_ui_widget(&ui, j) == &label_w[k] && rnd(4) == 0) dgx_ui_text_printf(&ui, j, "%d", rnd(100000));
            break;
        case 7:
        case 8: { /* the finger: down, moved, up */
            const int x = rnd(PANEL_W), y = rnd(PANEL_H);
            dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .down = true, .pressed = true, .ms = now});
            pass(); /* pressed: drawn so */
            dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(x + rnd(20) - 10), .y = (int16_t)(y + rnd(24) - 12), .down = true, .ms = now});
            pass();
            dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)x, .y = (int16_t)y, .released = true, .ms = now});
            break;
        }
        case 9: dgx_ui_key(&ui, (const int[]){DGX_UI_KEY_NEXT, DGX_UI_KEY_PREV, DGX_UI_KEY_ACTIVATE, DGX_UI_KEY_MORE, DGX_UI_KEY_LESS}[rnd(5)]); break;
        case 10: /* a popup opened, or closed */
            if (ui.popup) dgx_ui_popup_close(&ui);
            else dgx_ui_popup_open(&ui, &pop, rnd(40), rnd(30), 30 + rnd(30), 16 + rnd(20), rnd(2) ? DGX_UI_POPUP_MODAL : 0);
            break;
        case 11: /* the keyboard's band moved, or gone */
            pp.cover = rnd(3) ? dgx_ui_rect(0, 0, 0, 0) : dgx_ui_rect(rnd(40), 40 + rnd(10), 30 + rnd(50), 10 + rnd(10));
            pv.cover = pp.cover;
            break;
        case 12: if ((i = pick(&ui, false, true)) >= 0) dgx_ui_redraw(&ui, i); break;
        case 13: if ((i = pick(&ui, false, false)) >= 0) dgx_ui_focus(&ui, i); break;
        default: break; /* a pass with nothing changed */
        }
        (void)what;
        pass();
    }
    printf("%d passes over up to %d widgets\n", passes, ui.count);
    CHECK(differences == 0);
    CHECK(wrong_hits == 0);
    dgx_ui_popup_close(&ui);
    dgx_ui_free(&ui), dgx_ui_free(&pop);
    v->destroy(&v);
    free(panel);
    CHECK_DONE();
}
