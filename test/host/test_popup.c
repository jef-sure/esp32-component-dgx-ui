/*
 * A popup over a tree: drawn over it in its rectangle, the tree under it drawn around it and never inside it, the
 * pointer and the keys to it first, a modal one taking all, one closed by a finger outside; messages to both trees;
 * where it lay composed again when it closes; the screen composed in strips with it open.
 */
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_text.h"
#include "dgx_vscreen.h"
#include "fonts/DejaVuSansBook32.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};

static int  activated[8], activated_count, caught;
static void on(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_ACTIVATED && activated_count < 8) activated[activated_count++] = (int)(intptr_t)ctx;
}
static bool take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    caught = msg->what;
    return true;
}

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;
static char                  under_buf[16], ok_buf[8];
static dgx_ui_textbox_t      under_box, ok_box;

static const dgx_ui_widget_t root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t under_w = {.kind = DGX_UI_LABEL, .text = &under_box};
static const dgx_ui_widget_t main_bt = {.kind = DGX_UI_BUTTON, .want_h = 14, .look = &dgx_ui_look_button, .on = on, .ctx = (void *)1};
static const dgx_ui_widget_t pop_w   = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2};
static const dgx_ui_widget_t pop_bt  = {.kind = DGX_UI_BUTTON, .text = &ok_box, .want_h = 14, .look = &dgx_ui_look_button, .on = on, .ctx = (void *)2, .take = take};

static dgx_ui_t *editor_of_focus;
static void      on_focus_opens(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_FOCUS) dgx_ui_popup_open(ui, editor_of_focus, 0, 0, PANEL_W, 20, DGX_UI_POPUP_MODAL);
}
static void on_focus_closes(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_FOCUS) dgx_ui_popup_close(ui);
    on(ui, i, node, what, ctx);
}
static const dgx_ui_widget_t focus_closes_w = {.kind = DGX_UI_BUTTON, .want_h = 14, .on = on_focus_closes, .ctx = (void *)5};
static char                  focus_buf[8];
static dgx_ui_textbox_t      focus_box;
static const dgx_ui_widget_t focus_opens_w = {.kind = DGX_UI_INPUT, .text = &focus_box, .want_h = 14, .on = on_focus_opens};

static dgx_ui_node_t nodes[4], pop_nodes[4];
static dgx_ui_t      ui, pop;
static int           under, mbt, pbt;

static void make(void)
{
    dgx_ui_textbox_init(&under_box, under_buf, sizeof under_buf, 1, &style);
    dgx_ui_textbox_init(&ok_box, ok_buf, sizeof ok_buf, 1, &style);
    dgx_ui_init(&ui, nodes, 4, 1);
    dgx_ui_add(&ui, -1, &root_w);
    under = dgx_ui_add(&ui, 0, &under_w);
    mbt   = dgx_ui_add(&ui, 0, &main_bt);
    dgx_ui_text_set(&ui, under, "under it");
    dgx_ui_init(&pop, pop_nodes, 4, 1);
    dgx_ui_add(&pop, -1, &pop_w);
    pbt = dgx_ui_add(&pop, 0, &pop_bt);
    dgx_ui_text_set(&pop, pbt, "OK");
    activated_count = 0, caught = 0;
}

static int differ(const panel_t *p, dgx_screen_t *v)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->mem[y][x] != dgx_get_pixel(v, x, y);
    return n;
}

int main(void)
{
    variant[0] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
    style      = (dgx_ui_text_style_t){.variants = variant, .count = 1};
    panel_t       *panel = panel_make();
    dgx_ui_paint_t pp    = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
    dgx_screen_t  *v     = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    dgx_ui_paint_t pv    = {.scr = v, .colors = colors, .color_count = 4};
    const int      px = 10, py = 4, pw = 60, ph = 22; /* over the text and the top of the button */

    make();
    dgx_ui_paint(&ui, &pp, 0);
    dgx_ui_popup_open(&ui, &pop, px, py, pw, ph, DGX_UI_POPUP_MODAL);
    dgx_ui_paint(&ui, &pp, 1);
    /* the tree under it changed: drawn around the popup, every pixel once, nothing inside it */
    panel_count_from_now(panel);
    dgx_ui_text_set(&ui, under, "UNDER IT");
    dgx_ui_paint(&ui, &pp, 2);
    CHECK(panel_written(panel) > 0 && panel_twice(panel) == 0);
    {
        int inside = 0;
        for (int y = py; y < py + ph; ++y)
            for (int x = px; x < px + pw; ++x) inside += panel->writes[y][x] != 0;
        CHECK(inside == 0);
    }
    /* the pointer: the popup's button pressed; a modal popup swallows a finger on the tree under it */
    const dgx_ui_node_t *pn = dgx_ui_node(&pop, pbt), *mn = dgx_ui_node(&ui, mbt);
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(pn->x + 2), .y = (int16_t)(pn->y + 2), .down = true, .pressed = true});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(pn->x + 2), .y = (int16_t)(pn->y + 2), .released = true});
    CHECK(activated_count == 1 && activated[0] == 2);
    const int my = mn->y + mn->h - 2; /* the bottom of the main button: outside the popup */
    CHECK(my >= py + ph);
    CHECK(dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 4, .y = (int16_t)my, .down = true, .pressed = true}));
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 4, .y = (int16_t)my, .released = true});
    CHECK(activated_count == 1);
    /* the keys: the popup's focus; BACK is the program's */
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_NEXT) && pop.focus == pbt && ui.focus == -1);
    CHECK(!dgx_ui_key(&ui, DGX_UI_KEY_BACK));
    /* a message reaches the popup's widgets */
    dgx_ui_msg_t msg;
    dgx_ui_msg_value(&msg, 42, NULL, 0);
    CHECK(dgx_ui_take(&ui, &msg) && caught == 42);
    /* composed in strips with it open: as drawn */
    {
        dgx_ui_redraw(&ui, 0);
        dgx_ui_redraw(&pop, 0);
        dgx_ui_paint(&ui, &pp, 3);
        dgx_screen_t  *band = dgx_vscreen_init(PANEL_W, 4, 16, DgxScreenRGB);
        dgx_ui_paint_t pb   = {.scr = band, .colors = colors, .color_count = 4};
        int            off  = 0;
        for (int y0 = 0; y0 < PANEL_H; y0 += 4) {
            pb.oy = (int16_t)y0;
            dgx_ui_paint_area(&ui, &pb, 0, y0, PANEL_W, 4);
            for (int y = 0; y < 4 && y0 + y < PANEL_H; ++y)
                for (int x = 0; x < PANEL_W; ++x) off += dgx_get_pixel(band, x, y) != panel->mem[y0 + y][x];
        }
        CHECK(off == 0);
        band->destroy(&band);
    }
    /* closed: where it lay drawn again from the tree under it, as the tree drawn alone */
    dgx_ui_popup_close(&ui);
    CHECK(ui.popup == NULL);
    dgx_ui_paint(&ui, &pp, 4);
    {
        static dgx_ui_node_t alone_nodes[4];
        static char          alone_buf[16];
        static dgx_ui_textbox_t alone_box;
        static dgx_ui_widget_t  alone_under;
        dgx_ui_t                alone;
        dgx_ui_textbox_init(&alone_box, alone_buf, sizeof alone_buf, 1, &style);
        alone_under = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &alone_box};
        dgx_ui_init(&alone, alone_nodes, 4, 1);
        dgx_ui_add(&alone, -1, &root_w);
        const int au = dgx_ui_add(&alone, 0, &alone_under);
        dgx_ui_add(&alone, 0, &main_bt);
        dgx_ui_text_set(&alone, au, "UNDER IT");
        dgx_ui_paint(&alone, &pv, 0);
        CHECK(differ(panel, v) == 0);
        dgx_ui_free(&alone);
    }
    /* closed by a finger outside it: the finger spent, the tree under it untouched */
    dgx_ui_popup_open(&ui, &pop, px, py, pw, ph, DGX_UI_POPUP_CLOSE_OUTSIDE);
    CHECK(dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 4, .y = (int16_t)my, .down = true, .pressed = true}));
    CHECK(ui.popup == NULL && ui.pressed == -1);
    /* not modal: a finger on the tree under it is the tree's */
    dgx_ui_popup_open(&ui, &pop, px, py, pw, ph, 0);
    activated_count = 0;
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 4, .y = (int16_t)my, .down = true, .pressed = true});
    dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = 4, .y = (int16_t)my, .released = true});
    CHECK(activated_count == 1 && activated[0] == 1);
    dgx_ui_popup_close(&ui);

    /* a change made under a popup over all of the screen is drawn when it closes */
    {
        static dgx_ui_font_variant_t two[2];
        static dgx_ui_text_style_t   st2;
        static char                  b2[24], c2[24];
        static dgx_ui_textbox_t      t2, u2;
        static dgx_ui_widget_t       w2, x2;
        static dgx_ui_node_t         n2[2], m2[2], q2[2];
        static const dgx_ui_widget_t cover_all = {.kind = DGX_UI_FRAME};
        two[0] = (dgx_ui_font_variant_t){.font = &DejaVuSansBook32}, two[1] = (dgx_ui_font_variant_t){.font = &TerminusTTFMedium12};
        st2    = (dgx_ui_text_style_t){.variants = two, .count = 2, .flags = DGX_UI_TEXT_MIDDLE};
        dgx_ui_t a, b, all;
        dgx_ui_textbox_init(&t2, b2, sizeof b2, 1, &st2);
        w2 = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t2, .want_w = 80};
        dgx_ui_init(&a, n2, 2, 1), dgx_ui_add(&a, -1, &root_w);
        const int at = dgx_ui_add(&a, 0, &w2);
        dgx_ui_init(&all, q2, 2, 1), dgx_ui_add(&all, -1, &cover_all);
        dgx_ui_text_set(&a, at, "21");
        dgx_ui_paint(&a, &pp, 10);
        dgx_ui_popup_open(&a, &all, 0, 0, PANEL_W, PANEL_H, DGX_UI_POPUP_MODAL);
        dgx_ui_paint(&a, &pp, 11);
        dgx_ui_text_set(&a, at, "21.5 C, 40%"); /* a message while the popup hides it */
        dgx_ui_paint(&a, &pp, 12);
        dgx_ui_popup_close(&a);
        dgx_ui_paint(&a, &pp, 13);
        dgx_ui_textbox_init(&u2, c2, sizeof c2, 1, &st2);
        x2 = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &u2, .want_w = 80};
        dgx_ui_init(&b, m2, 2, 1), dgx_ui_add(&b, -1, &root_w);
        const int bt2 = dgx_ui_add(&b, 0, &x2);
        dgx_ui_text_set(&b, bt2, "21.5 C, 40%");
        dgx_ui_paint(&b, &pv, 0);
        CHECK(t2.variant == u2.variant && differ(panel, v) == 0);
        dgx_ui_free(&a), dgx_ui_free(&b), dgx_ui_free(&all);
    }
    /* 2: a popup not modal over the tree's button: a finger on the popup's frame presses nothing under it */
    make();
    dgx_ui_paint(&ui, &pp, 20);
    dgx_ui_popup_open(&ui, &pop, 0, 0, PANEL_W, 40, 0);
    activated_count = 0;
    {
        const dgx_ui_node_t *b = dgx_ui_node(&ui, mbt);
        CHECK(b->y < 40);
        CHECK(!dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(b->x + 1), .y = (int16_t)(b->y + 1), .down = true, .pressed = true}));
        dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(b->x + 1), .y = (int16_t)(b->y + 1), .released = true});
        CHECK(activated_count == 0 && ui.pressed == -1);
    }
    dgx_ui_popup_close(&ui);
    dgx_ui_free(&ui), dgx_ui_free(&pop);
    /* 3: a slider of a popup held: the tree tells it is dragged */
    {
        static int32_t pslider_place; /* its value */
        static const dgx_ui_widget_t pslider = {.kind = DGX_UI_SLIDER, .value = &pslider_place, .want_h = 10, .max = 10, .look = &dgx_ui_look_slider};
        static dgx_ui_node_t         tn[2], pn2[3];
        dgx_ui_t                     tree, p2;
        dgx_ui_init(&tree, tn, 2, 1), dgx_ui_add(&tree, -1, &root_w);
        dgx_ui_init(&p2, pn2, 3, 1), dgx_ui_add(&p2, -1, &pop_w);
        dgx_ui_add(&p2, 0, &pslider);
        dgx_ui_set_root(&tree, 0, 0, PANEL_W, PANEL_H);
        dgx_ui_popup_open(&tree, &p2, 10, 10, 60, 30, DGX_UI_POPUP_MODAL);
        dgx_ui_layout(&p2);
        const dgx_ui_node_t *sn = dgx_ui_node(&p2, 1);
        dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(sn->x + 5), .y = (int16_t)(sn->y + 5), .down = true, .pressed = true});
        CHECK(dgx_ui_holding(&tree) && dgx_ui_dragging(&tree));
        dgx_ui_free(&tree), dgx_ui_free(&p2);
    }
    /* 4: a field whose on(FOCUS) opens a popup over it (an editor): the finger that touched it is spent, nothing of
       the tree under the popup stays pressed */
    {
        static dgx_ui_node_t tn[2], en[1];
        static dgx_ui_t      tree, editor;
        dgx_ui_textbox_init(&focus_box, focus_buf, sizeof focus_buf, 1, &style);
        dgx_ui_init(&tree, tn, 2, 1), dgx_ui_add(&tree, -1, &root_w);
        const int f = dgx_ui_add(&tree, 0, &focus_opens_w);
        dgx_ui_init(&editor, en, 1, 1), dgx_ui_add(&editor, -1, &pop_w);
        editor_of_focus = &editor;
        dgx_ui_set_root(&tree, 0, 0, PANEL_W, PANEL_H);
        dgx_ui_layout(&tree);
        const dgx_ui_node_t *fn = dgx_ui_node(&tree, f);
        CHECK(dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(fn->x + 2), .y = (int16_t)(fn->y + 2), .down = true, .pressed = true}));
        CHECK(tree.popup == &editor && tree.pressed == -1 && !(fn->state.bits & DGX_UI_PRESSED));
        dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(fn->x + 2), .y = (int16_t)(fn->y + 2), .released = true});
        CHECK(tree.pressed == -1 && !dgx_ui_holding(&tree));
        dgx_ui_popup_close(&tree);
        dgx_ui_free(&tree), dgx_ui_free(&editor);
    }
    /* 5: a button whose on(FOCUS) closes a popup not modal open elsewhere (a hint): its first click is a click */
    {
        static dgx_ui_node_t tn[2], hn[1];
        static dgx_ui_t      tree, hint;
        dgx_ui_init(&tree, tn, 2, 1), dgx_ui_add(&tree, -1, &root_w);
        const int b = dgx_ui_add(&tree, 0, &focus_closes_w);
        dgx_ui_init(&hint, hn, 1, 1), dgx_ui_add(&hint, -1, &pop_w);
        dgx_ui_set_root(&tree, 0, 0, PANEL_W, PANEL_H);
        dgx_ui_layout(&tree);
        CHECK(dgx_ui_popup_open(&tree, &hint, 0, 40, 40, 20, 0));
        activated_count = 0;
        const dgx_ui_node_t *bn = dgx_ui_node(&tree, b);
        CHECK(bn->y + 2 < 40); /* the button above the hint */
        dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(bn->x + 60), .y = (int16_t)(bn->y + 2), .down = true, .pressed = true});
        CHECK(tree.popup == NULL && tree.pressed == b);
        dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(bn->x + 60), .y = (int16_t)(bn->y + 2), .released = true});
        CHECK(activated_count == 1 && activated[0] == 5);
        dgx_ui_free(&tree), dgx_ui_free(&hint);
    }
    /* 6: a popup of labels alone over a tree of labels and a button: modal, it takes every pointer that reaches it, down,
       moving and up, inside and outside, one rule, and the tree under it gets none; not modal, it takes what a widget
       of it takes, inside nothing, and outside the tree under it has the finger */
    {
        static dgx_ui_node_t         tn[3], ln[2];
        static dgx_ui_t              tree, labels;
        static char                  word_buf[8];
        static dgx_ui_textbox_t      word_box;
        static const dgx_ui_widget_t word_w = {.kind = DGX_UI_LABEL, .text = &word_box, .want_h = 14};
        dgx_ui_textbox_init(&word_box, word_buf, sizeof word_buf, 1, &style);
        const unsigned flags_of[] = {DGX_UI_POPUP_MODAL, DGX_UI_POPUP_MODAL | DGX_UI_POPUP_CLOSE_OUTSIDE, 0, DGX_UI_POPUP_CLOSE_OUTSIDE};
        for (size_t k = 0; k < sizeof flags_of / sizeof *flags_of; ++k) {
            const unsigned flags = flags_of[k];
            const bool     modal = flags & DGX_UI_POPUP_MODAL;
            dgx_ui_init(&tree, tn, 3, 1), dgx_ui_add(&tree, -1, &root_w);
            dgx_ui_add(&tree, 0, &under_w);
            const int b = dgx_ui_add(&tree, 0, &main_bt);
            dgx_ui_init(&labels, ln, 2, 1), dgx_ui_add(&labels, -1, &pop_w), dgx_ui_add(&labels, 0, &word_w);
            dgx_ui_text_set(&labels, 1, "word");
            dgx_ui_set_root(&tree, 0, 0, PANEL_W, PANEL_H);
            dgx_ui_layout(&tree);
            const dgx_ui_node_t *bn = dgx_ui_node(&tree, b);
            CHECK(dgx_ui_popup_open(&tree, &labels, 0, bn->y + bn->h + 4, PANEL_W, 20, flags));
            dgx_ui_layout(&labels);
            const dgx_ui_node_t *word = dgx_ui_node(&labels, 1);
            const int            ix = word->x + 2, iy = word->y + 2, ox = bn->x + 2, oy = bn->y + 2;
            activated_count = 0;
            /* inside, over a label: down, moving, up */
            const bool down_in = dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)ix, .y = (int16_t)iy, .down = true, .pressed = true});
            const bool move_in = dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(ix + 1), .y = (int16_t)iy, .down = true});
            const bool up_in   = dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(ix + 1), .y = (int16_t)iy, .released = true});
            CHECK(down_in == modal && move_in == modal && up_in == modal);
            CHECK(tree.pressed == -1 && activated_count == 0 && tree.popup == &labels);
            /* outside, over the tree's button */
            const bool down_out = dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)ox, .y = (int16_t)oy, .down = true, .pressed = true});
            if (flags & DGX_UI_POPUP_CLOSE_OUTSIDE) {
                CHECK(down_out && tree.popup == NULL && tree.pressed == -1); /* closed by it, the finger spent */
                dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)ox, .y = (int16_t)oy, .released = true});
                CHECK(activated_count == 0);
            } else {
                const bool move_out = dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(ox + 1), .y = (int16_t)oy, .down = true});
                const bool up_out   = dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(ox + 1), .y = (int16_t)oy, .released = true});
                if (modal) CHECK(down_out && move_out && up_out && activated_count == 0); /* nothing under it */
                else CHECK(down_out && up_out && activated_count == 1 && activated[0] == 1); /* the tree's button clicked */
                dgx_ui_popup_close(&tree);
            }
            dgx_ui_free(&tree), dgx_ui_free(&labels);
        }
        /* a button in a modal popup: pressed and clicked as before */
        dgx_ui_init(&tree, tn, 3, 1), dgx_ui_add(&tree, -1, &root_w), dgx_ui_add(&tree, 0, &under_w);
        dgx_ui_init(&labels, ln, 2, 1), dgx_ui_add(&labels, -1, &pop_w);
        const int ok = dgx_ui_add(&labels, 0, &pop_bt);
        dgx_ui_set_root(&tree, 0, 0, PANEL_W, PANEL_H);
        CHECK(dgx_ui_popup_open(&tree, &labels, 0, 30, PANEL_W, 24, DGX_UI_POPUP_MODAL));
        dgx_ui_layout(&labels);
        const dgx_ui_node_t *okn = dgx_ui_node(&labels, ok);
        activated_count = 0;
        CHECK(dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(okn->x + 2), .y = (int16_t)(okn->y + 2), .down = true, .pressed = true}));
        CHECK(labels.pressed == ok);
        CHECK(dgx_ui_pointer(&tree, &(dgx_ui_pointer_t){.x = (int16_t)(okn->x + 2), .y = (int16_t)(okn->y + 2), .released = true}));
        CHECK(activated_count == 1 && activated[0] == 2);
        dgx_ui_popup_close(&tree);
        dgx_ui_free(&tree), dgx_ui_free(&labels);
    }
    v->destroy(&v);
    free(panel);
    CHECK_DONE();
}
