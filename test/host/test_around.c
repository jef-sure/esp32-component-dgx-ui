/*
 * The painter around children, on a 16-bit panel behind a bus that counts the writes of every pixel: a column drawn
 * whole writes every pixel once; a widget hidden, shown, added or removed writes no pixel twice and leaves the
 * widgets that did not move unwritten, and the panel then is as a whole paint of the same tree draws it; in a box and
 * in a frame. A box of neither a column nor a row is drawn whole, as before. A look of the program's that holds
 * children fills around them by dgx_ui_fill_around(). A widget shown with no look is told of once a tree, a box, a
 * frame and a label never.
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

static uint32_t               colors[4]     = {0x0000, 0xffff, 0x2104, 0xf800}; /* paper, ink, fill, border */
static const uint8_t          cross_bits[8] = {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81};
static const dgx_image_t      cross_im      = {.width = 8, .height = 8, .format = DGX_IMAGE_1BPP, .data = cross_bits};
static const dgx_ui_picture_t icons[1]      = {{.image = &cross_im}};

static dgx_ui_font_variant_t variant[1];
static dgx_ui_text_style_t   style;

/* a column of a label, an icon narrower than it, a label; in the root box or in a frame in it; or a row of them */
typedef struct {
    dgx_ui_node_t    nodes[8];
    dgx_ui_t         ui;
    char             b1[16], b2[16];
    dgx_ui_textbox_t t1, t2;
    dgx_ui_widget_t  l1, l2;
    int              parent, top, icon, bottom, extra;
} tree_t;

static const dgx_ui_widget_t box_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t row_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = 2, .gap = 2};
/* a box's look of the program's, a constant by the public draw: filled around as the library's own */
static const dgx_ui_look_t   own_box   = {.draw = dgx_ui_draw_box, .fill = -1};
static const dgx_ui_widget_t own_box_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2, .look = &own_box};
static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 3, .gap = 2, .want_h = 56};
static const dgx_ui_widget_t free_w  = {.kind = DGX_UI_BOX};
static const dgx_ui_widget_t icon_w  = {.kind = DGX_UI_ICON, .picture = icons, .want_w = 20, .want_h = 10, .at_x = 4, .at_y = 4, .look = &dgx_ui_look_picture};
static const dgx_ui_widget_t extra_w = {.kind = DGX_UI_ICON, .picture = icons, .want_w = 8, .want_h = 8, .look = &dgx_ui_look_picture};

/* a look of the program's for a widget that holds children: a ring's scale, say, drawn over what it fills */
static bool ring_draw(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    if (paint->whole) dgx_ui_fill_around(paint, ui, i, dgx_ui_rect(n->x, n->y, n->w, n->h), dgx_ui_color(paint, 2));
    return false;
}
static const dgx_ui_look_t   ring_look = {.draw = ring_draw, .fill = 2};
static const dgx_ui_widget_t ring_w    = {.kind = DGX_UI_OWN, .flags = DGX_UI_COLUMN, .pad = 3, .gap = 2, .look = &ring_look};

static void make_tree(tree_t *t, const dgx_ui_widget_t *root, bool in_frame)
{
    memset(t, 0, sizeof(*t));
    dgx_ui_init(&t->ui, t->nodes, 8, 1);
    dgx_ui_add(&t->ui, -1, root);
    t->parent = in_frame ? dgx_ui_add(&t->ui, 0, &frame_w) : 0;
    dgx_ui_textbox_init(&t->t1, t->b1, sizeof t->b1, 1, &style);
    dgx_ui_textbox_init(&t->t2, t->b2, sizeof t->b2, 1, &style);
    const int16_t lw = root == &row_w ? 28 : 60;
    t->l1     = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t->t1, .want_w = lw, .at_y = 3};
    t->l2     = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .text = &t->t2, .want_w = lw, .at_y = 40};
    t->top    = dgx_ui_add(&t->ui, t->parent, &t->l1);
    t->icon   = dgx_ui_add(&t->ui, t->parent, &icon_w);
    t->bottom = dgx_ui_add(&t->ui, t->parent, &t->l2);
    t->extra  = -1;
    dgx_ui_text_set(&t->ui, t->top, "top"), dgx_ui_text_set(&t->ui, t->bottom, "bottom");
}

static int differ(const panel_t *p, dgx_screen_t *v)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->mem[y][x] != dgx_get_pixel(v, x, y);
    return n;
}

/* pixels of a widget's box written since the count began */
static int written_in(const panel_t *p, const dgx_ui_t *ui, int i)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    int                  k = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) k += p->writes[y][x] != 0;
    return k;
}

enum { HIDE, SHOW, ADD, REMOVE };

static void act(tree_t *t, int what)
{
    if (what == HIDE) dgx_ui_show(&t->ui, t->icon, false);
    if (what == SHOW) dgx_ui_show(&t->ui, t->icon, true);
    if (what == ADD) t->extra = dgx_ui_add(&t->ui, t->parent, &extra_w);
    if (what == REMOVE) dgx_ui_remove(&t->ui, t->extra), t->extra = -1;
}

/* the steps one after another on a tree drawn once whole: each written once, the top label never, the bottom one
   only when it moved; the panel as a fresh tree of the same steps draws it whole */
static void steps(panel_t *panel, dgx_screen_t *v, const dgx_ui_widget_t *root, bool in_frame)
{
    static tree_t  a, b;
    dgx_ui_paint_t pa = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
    dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4};
    make_tree(&a, root, in_frame);
    panel_count_from_now(panel);
    dgx_ui_paint(&a.ui, &pa, 0);
    CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H); /* whole: every pixel once */
    const int order[] = {HIDE, SHOW, ADD, REMOVE};
    for (int s = 0; s < 4; ++s) {
        panel_count_from_now(panel);
        act(&a, order[s]);
        dgx_ui_paint(&a.ui, &pa, 0);
        CHECK(panel_twice(panel) == 0);
        CHECK(written_in(panel, &a.ui, a.top) == 0);
        if (order[s] >= ADD) CHECK(written_in(panel, &a.ui, a.bottom) == 0); /* nothing above it moved */
        CHECK(panel_written(panel) > 0);
        panel_count_from_now(panel); /* drawn: nothing again */
        dgx_ui_paint(&a.ui, &pa, 0);
        CHECK(panel_written(panel) == 0);
        make_tree(&b, root, in_frame);
        for (int k = 0; k <= s; ++k) act(&b, order[k]);
        dgx_ui_paint(&b.ui, &pv, 0);
        CHECK(differ(panel, v) == 0);
        dgx_ui_free(&b.ui);
    }
    dgx_ui_free(&a.ui);
}

int main(void)
{
    variant[0]     = (dgx_ui_font_variant_t){.font = TerminusTTFMedium12()};
    style          = (dgx_ui_text_style_t){.variants = variant, .count = 1};
    panel_t      *panel = panel_make();
    dgx_screen_t *v     = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);

    steps(panel, v, &box_w, false);
    steps(panel, v, &box_w, true);
    steps(panel, v, &row_w, false);
    steps(panel, v, &own_box_w, false);

    /* a box of neither: children where they ask, maybe over each other; the icon hidden, the box drawn whole with
       its children, the panel as a fresh paint draws it */
    {
        static tree_t  a, b;
        dgx_ui_paint_t pa = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4};
        make_tree(&a, &free_w, false);
        dgx_ui_paint(&a.ui, &pa, 0);
        act(&a, HIDE);
        panel_count_from_now(panel);
        dgx_ui_paint(&a.ui, &pa, 0);
        CHECK(panel_written(panel) == PANEL_W * PANEL_H);
        make_tree(&b, &free_w, false);
        act(&b, HIDE);
        dgx_ui_paint(&b.ui, &pv, 0);
        CHECK(differ(panel, v) == 0);
        dgx_ui_free(&a.ui), dgx_ui_free(&b.ui);
    }

    /* a look of the program's around its children: drawn whole, every pixel once; a child hidden, drawn whole again
       (the painter fills around for the looks of a box and a frame only), still every pixel once, as a fresh paint */
    {
        static tree_t  a, b;
        dgx_ui_paint_t pa = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4};
        make_tree(&a, &ring_w, false);
        panel_count_from_now(panel);
        dgx_ui_paint(&a.ui, &pa, 0);
        CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H);
        act(&a, HIDE);
        panel_count_from_now(panel);
        dgx_ui_paint(&a.ui, &pa, 0);
        CHECK(panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H);
        make_tree(&b, &ring_w, false);
        act(&b, HIDE);
        dgx_ui_paint(&b.ui, &pv, 0);
        CHECK(differ(panel, v) == 0);
        dgx_ui_free(&a.ui), dgx_ui_free(&b.ui);
    }

    /* no look: a box, a frame and a label never told of; a button with none told of once, and its place filled by
       the box, as it covers nothing: drawn whole, every pixel once */
    {
        static tree_t  a;
        dgx_ui_paint_t pa = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        make_tree(&a, &box_w, true);
        dgx_ui_paint(&a.ui, &pa, 0);
        CHECK(!a.ui.told_look);
        dgx_ui_free(&a.ui);
        make_tree(&a, &box_w, false);
        static const dgx_ui_widget_t bare = {.kind = DGX_UI_BUTTON, .want_h = 6};
        const int                    bt   = dgx_ui_insert(&a.ui, 0, a.icon, &bare);
        panel_count_from_now(panel);
        dgx_ui_paint(&a.ui, &pa, 0);
        CHECK(a.ui.told_look && panel_twice(panel) == 0 && panel_written(panel) == PANEL_W * PANEL_H);
        /* hidden it is not met: a tree of a hidden one is not told */
        static tree_t c;
        make_tree(&c, &box_w, false);
        static const dgx_ui_widget_t own = {.kind = DGX_UI_OWN, .want_h = 6, .state = {.bits = DGX_UI_HIDDEN}};
        const int                    o   = dgx_ui_add(&c.ui, 0, &own);
        dgx_ui_paint(&c.ui, &pa, 0);
        CHECK(!c.ui.told_look);
        dgx_ui_show(&c.ui, o, true);
        dgx_ui_paint(&c.ui, &pa, 0);
        CHECK(c.ui.told_look);
        (void)bt;
        dgx_ui_free(&a.ui), dgx_ui_free(&c.ui);
    }

    dgx_screen_destroy(&v);
    free(panel);
    CHECK_DONE();
}
