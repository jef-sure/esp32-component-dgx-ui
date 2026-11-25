/*
 * The canvases of widgets of DGX_UI_CANVAS. A look of the program's adds bars to its canvas, as many as its value,
 * indices of the painter's table: made once and empty, then only the bars it lacks drawn; a popup opened and closed
 * over it puts the canvas again and draws nothing in it; its box of another size, a new canvas; every pixel once on a
 * panel behind a bus, as a virtual screen has it. No memory (DGX's allocator made to fail for 8 bits): a larger canvas
 * refused keeps the old one, DGX_UI_MARK_HARD_SIZE, never changed again; none at all, the box paper and
 * DGX_UI_EVENT_NO_MEMORY told once, after the pass, from a pass or from a composing. A canvas freed when its widget leaves
 * the tree and when its popup closes; a second paint has its own. A look of a soft pen puts amounts of its ink: over
 * the paper, over a drawn background mixed with it in a strip, its holes the picture; indices over the background,
 * their 0 the picture; a screen of 24 bits, the box paper.
 */
#include <string.h>

#define PANEL_H 80
#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_vscreen.h"
#include "panel.h"

/* a place for the value of a widget made at run time, a new one each time, 0 in it */
static int32_t  place_pool[256];
static int      place_next;
static int32_t *new_place(void)
{
    int32_t *place = &place_pool[place_next++ % 256];
    *place         = 0;
    return place;
}

static uint32_t colors[4] = {0x0000, 0xffff, 0x2104, 0xf800}; /* the paper, ink, a fill, the accent */

/* DGX's allocator of virtual screens, failing for 8 bits while asked: no memory for a canvas */
dgx_screen_t *__real_dgx_vscreen_init(int width, int height, uint8_t color_bits, dgx_color_order_t cbo);
dgx_screen_t *__wrap_dgx_vscreen_init(int width, int height, uint8_t color_bits, dgx_color_order_t cbo);
static bool   no_memory;
dgx_screen_t *__wrap_dgx_vscreen_init(int width, int height, uint8_t color_bits, dgx_color_order_t cbo)
{
    return no_memory && color_bits == 8 ? NULL : __real_dgx_vscreen_init(width, height, color_bits, cbo);
}

/* the map under the tree, as in the test of the background: no pixel of it one of the table's colours */
static dgx_screen_t *map;
static uint32_t      map_at(int x, int y) { return (uint32_t)(((x * 3) & 0x1f) << 11 | ((y * 2) & 0x1f) << 5 | ((x + y) & 0x0f) | 0x10); }
static void          draw_map(dgx_ui_paint_t *paint, int x, int y, int w, int h, void *ctx)
{
    dgx_vscreen_region_to_screen(paint->scr, x - paint->ox, y - paint->oy, (dgx_screen_t *)ctx, x, y, w, h, 0);
}

/* the program's data of a widget of bars: for each paint, the bars its canvas has; canvases made, bars drawn, told */
typedef struct {
    const dgx_ui_paint_t *paint[3];
    int                   drawn[3];
    int                   made, bars, told;
} bars_t;

static int slot_of(bars_t *b, const dgx_ui_paint_t *p)
{
    for (int k = 0; k < 3; ++k)
        if (b->paint[k] == p || !b->paint[k]) return b->paint[k] = p, k;
    return 2;
}

/* bar k at 3k, two points wide, of the colour 1 + k % 3, as many as the value: only those the canvas lacks */
static bool draw_bars(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(ui, i);
    bars_t              *b = n->widget->own;
    const int            s = slot_of(b, paint);
    bool                 made;
    dgx_screen_t        *c = dgx_ui_canvas(paint, ui, i, &made);
    if (c && made) b->drawn[s] = 0, ++b->made;
    for (; c && b->drawn[s] < dgx_ui_value_of(n); ++b->drawn[s], ++b->bars)
        dgx_fill_rectangle(c, 3 * b->drawn[s], 0, 2, c->height, (uint32_t)(1 + b->drawn[s] % 3));
    dgx_ui_canvas_put(paint, ui, i, -1);
    return false;
}
static const dgx_ui_look_t bars_look = {.draw = draw_bars, .fill = -1};

/* a soft circle of the ink 1 in the middle of its canvas, drawn when the canvas is made */
static bool draw_soft(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    bool          made;
    dgx_screen_t *c = dgx_ui_canvas(paint, ui, i, &made);
    if (c && made) dgx_fill_circle(c, c->width / 2, c->height / 2, DGX_PEN_SOFT | 6, 255);
    dgx_ui_canvas_put(paint, ui, i, 1);
    return false;
}
static const dgx_ui_look_t soft_look = {.draw = draw_soft, .fill = -1};

static void on_canvas(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_NO_MEMORY) ++((bars_t *)node->widget->own)->told;
}

/* told no memory, it removes another widget that waits to be told too */
static int  remove_i = -1;
static void on_remove(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_EVENT_NO_MEMORY) return;
    ++((bars_t *)node->widget->own)->told;
    if (remove_i >= 0) dgx_ui_remove(ui, remove_i), remove_i = -1;
}

static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
static const dgx_ui_widget_t pop_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2};

/* a tree of a widget of bars, or of the soft circle */
typedef struct {
    dgx_ui_node_t   nodes[4];
    dgx_ui_t        ui;
    bars_t          own;
    dgx_ui_widget_t w;
    int             i;
} tree_t;

static void make(tree_t *t, const dgx_ui_look_t *look, const dgx_ui_widget_t *root)
{
    memset(t, 0, sizeof *t);
    t->w = (dgx_ui_widget_t){.kind = DGX_UI_OWN, .value = new_place(), .flags = DGX_UI_CANVAS, .want_h = 20, .max = 30, .look = look, .own = &t->own, .on_event = on_canvas};
    dgx_ui_init(&t->ui, t->nodes, 4, 1);
    dgx_ui_add(&t->ui, -1, root);
    t->i = dgx_ui_add(&t->ui, 0, &t->w);
    dgx_ui_set_value(&t->ui, t->i, 4);
}

static int differ(const panel_t *p, dgx_screen_t *v)
{
    int n = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) n += p->mem[y][x] != dgx_get_pixel(v, x, y);
    return n;
}

/* the bars of a widget as they should be on a screen: up to cw points of its box its canvas, the rest paper */
static int bars_wrong(dgx_screen_t *s, const dgx_ui_t *t, int i, int bars, int cw, uint32_t paper)
{
    const dgx_ui_node_t *n   = dgx_ui_node(t, i);
    int                  bad = 0;
    for (int y = n->y; y < n->y + n->h; ++y)
        for (int x = n->x; x < n->x + n->w; ++x) {
            const int k = (x - n->x) / 3, on = (x - n->x) % 3 < 2 && k < bars && x - n->x < cw;
            bad += dgx_get_pixel(s, x, y) != (on ? colors[1 + k % 3] : paper);
        }
    return bad;
}

int main(void)
{
    panel_t       *panel = panel_make();
    dgx_screen_t  *v     = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    static tree_t  a, b;
    make(&a, &bars_look, &root_w), make(&b, &bars_look, &root_w);
    /* the paper the colour 2: the canvas's 0 is the paper, not the table's 0 */
    dgx_ui_paint_t pa = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .background = 2};
    dgx_ui_paint_t pv = {.scr = v, .colors = colors, .color_count = 4, .background = 2};

    /* made once, empty: four bars; every pixel once, as the virtual screen has it */
    dgx_ui_paint(&a.ui, &pa, 0), dgx_ui_paint(&b.ui, &pv, 0);
    const int full = dgx_ui_node(&b.ui, b.i)->w;
    CHECK(a.own.made == 1 && a.own.bars == 4 && b.own.made == 1 && panel_twice(panel) == 0 && differ(panel, v) == 0);
    CHECK(bars_wrong(v, &b.ui, b.i, 4, full, colors[2]) == 0);
    /* three more: only those drawn in the canvas */
    panel_count_from_now(panel);
    dgx_ui_set_value(&a.ui, a.i, 7), dgx_ui_set_value(&b.ui, b.i, 7);
    dgx_ui_paint(&a.ui, &pa, 1), dgx_ui_paint(&b.ui, &pv, 1);
    CHECK(a.own.made == 1 && a.own.bars == 7 && panel_twice(panel) == 0 && differ(panel, v) == 0 && bars_wrong(v, &b.ui, b.i, 7, full, colors[2]) == 0);
    /* a popup opened over it and closed: the canvas put again, nothing drawn in it */
    {
        static dgx_ui_node_t pa_nodes[1], pb_nodes[1];
        static dgx_ui_t      popa, popb;
        dgx_ui_init(&popa, pa_nodes, 1, 1), dgx_ui_init(&popb, pb_nodes, 1, 1);
        dgx_ui_add(&popa, -1, &pop_w), dgx_ui_add(&popb, -1, &pop_w);
        CHECK(dgx_ui_popup_open(&a.ui, &popa, 10, 5, 30, 15, 0) && dgx_ui_popup_open(&b.ui, &popb, 10, 5, 30, 15, 0));
        dgx_ui_paint(&a.ui, &pa, 2), dgx_ui_paint(&b.ui, &pv, 2);
        dgx_ui_popup_close(&a.ui), dgx_ui_popup_close(&b.ui);
        panel_count_from_now(panel);
        dgx_ui_paint(&a.ui, &pa, 3), dgx_ui_paint(&b.ui, &pv, 3);
        CHECK(a.own.made == 1 && a.own.bars == 7 && panel_written(panel) > 0 && panel_twice(panel) == 0 && differ(panel, v) == 0);
        CHECK(bars_wrong(v, &b.ui, b.i, 7, full, colors[2]) == 0);
        dgx_ui_free(&popa), dgx_ui_free(&popb);
    }
    /* its box narrower: a new canvas, the bars drawn again in it */
    dgx_ui_set_root(&a.ui, 0, 0, PANEL_W - 20, PANEL_H), dgx_ui_set_root(&b.ui, 0, 0, PANEL_W - 20, PANEL_H);
    dgx_ui_paint(&a.ui, &pa, 4), dgx_ui_paint(&b.ui, &pv, 4);
    const int narrow = dgx_ui_node(&b.ui, b.i)->w;
    CHECK(narrow < full && a.own.made == 2 && a.own.bars == 14 && differ(panel, v) == 0 && bars_wrong(v, &b.ui, b.i, 7, narrow, colors[2]) == 0);
    /* wider with no memory: the old canvas kept at the box's left, the rest paper, marked; never changed again */
    no_memory = true;
    dgx_ui_set_root(&a.ui, 0, 0, PANEL_W, PANEL_H), dgx_ui_set_root(&b.ui, 0, 0, PANEL_W, PANEL_H);
    dgx_ui_set_value(&a.ui, a.i, 30), dgx_ui_set_value(&b.ui, b.i, 30);
    dgx_ui_paint(&a.ui, &pa, 5), dgx_ui_paint(&b.ui, &pv, 5);
    CHECK(a.own.made == 2 && (dgx_ui_node(&a.ui, a.i)->marks & DGX_UI_MARK_HARD_SIZE) && a.own.told == 0);
    CHECK(differ(panel, v) == 0 && bars_wrong(v, &b.ui, b.i, 30, narrow, colors[2]) == 0);
    no_memory = false;
    dgx_ui_set_root(&a.ui, 0, 0, PANEL_W - 40, PANEL_H), dgx_ui_set_root(&b.ui, 0, 0, PANEL_W - 40, PANEL_H);
    dgx_ui_paint(&a.ui, &pa, 6), dgx_ui_paint(&b.ui, &pv, 6);
    CHECK(a.own.made == 2 && differ(panel, v) == 0 && bars_wrong(v, &b.ui, b.i, 30, dgx_ui_node(&b.ui, b.i)->w, colors[2]) == 0);

    /* none at all: the box paper, told once after the pass; again at the same size, not told again; memory back and the
       widget drawn again, made */
    {
        static tree_t  c;
        dgx_screen_t  *vc = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t pc = {.scr = vc, .colors = colors, .color_count = 4};
        make(&c, &bars_look, &root_w);
        no_memory = true;
        dgx_ui_paint(&c.ui, &pc, 0);
        CHECK(c.own.told == 1 && c.own.made == 0 && bars_wrong(vc, &c.ui, c.i, 0, 0, colors[0]) == 0);
        for (int k = 1; k <= 10; ++k) dgx_ui_redraw(&c.ui, c.i), dgx_ui_paint(&c.ui, &pc, (uint32_t)k); /* ten passes more */
        CHECK(c.own.told == 1 && c.own.made == 0);
        no_memory = false;
        dgx_ui_redraw(&c.ui, c.i);
        dgx_ui_paint(&c.ui, &pc, 20);
        CHECK(c.own.told == 1 && c.own.made == 1 && bars_wrong(vc, &c.ui, c.i, 4, PANEL_W, colors[0]) == 0);
        /* met in a composing: told at the end of the next pass, once */
        static tree_t d;
        make(&d, &bars_look, &root_w);
        dgx_ui_paint_t pd = {.scr = vc, .colors = colors, .color_count = 4};
        dgx_ui_show(&d.ui, d.i, false);
        dgx_ui_paint(&d.ui, &pd, 0);
        no_memory = true;
        dgx_ui_show(&d.ui, d.i, true);
        dgx_ui_paint_area(&d.ui, &pd, 0, 0, PANEL_W, PANEL_H);
        CHECK(d.own.told == 0);
        dgx_ui_paint(&d.ui, &pd, 1);
        CHECK(d.own.told == 1);
        no_memory = false;
        dgx_ui_paint_free(&pc), dgx_ui_paint_free(&pd);
        dgx_ui_free(&c.ui), dgx_ui_free(&d.ui);
        vc->destroy(&vc);
    }

    /* two without memory, the first's on_event() removing the second: the second not told, nothing read of it */
    {
        static tree_t                t;
        static bars_t                own2;
        static const dgx_ui_widget_t after = {.kind = DGX_UI_BOX, .want_h = 4};
        dgx_ui_paint_t               pt    = {.scr = v, .colors = colors, .color_count = 4};
        make(&t, &bars_look, &root_w);
        t.w.on_event              = on_remove;
        dgx_ui_widget_t two = t.w;
        two.value = new_place();
        two.on_event = on_canvas, two.own = &own2;
        remove_i = dgx_ui_add(&t.ui, 0, &two);
        dgx_ui_add(&t.ui, 0, &after);
        no_memory = true;
        dgx_ui_paint(&t.ui, &pt, 0);
        no_memory = false;
        CHECK(t.own.told == 1 && own2.told == 0 && remove_i < 0 && !dgx_ui_widget(&t.ui, 2));
        dgx_ui_paint_free(&pt);
        dgx_ui_free(&t.ui);
    }

    /* the widget out of its tree: its canvas freed at the end of the pass; put back in its place, the same size, a
       new one */
    {
        static tree_t                r;
        static const dgx_ui_widget_t after = {.kind = DGX_UI_BOX, .want_h = 4}; /* its slot is not the last: it stays */
        dgx_ui_paint_t               pr    = {.scr = v, .colors = colors, .color_count = 4};
        make(&r, &bars_look, &root_w);
        dgx_ui_add(&r.ui, 0, &after);
        dgx_ui_paint(&r.ui, &pr, 0);
        dgx_ui_remove(&r.ui, r.i);
        dgx_ui_paint(&r.ui, &pr, 1);
        CHECK(r.own.made == 1 && dgx_ui_add(&r.ui, 0, &r.w) == r.i);
        dgx_ui_set_value(&r.ui, r.i, 4);
        dgx_ui_paint(&r.ui, &pr, 2);
        CHECK(r.own.made == 2);
        dgx_ui_paint_free(&pr);
        dgx_ui_free(&r.ui);
    }
    /* a widget of a popup: its canvas freed when the popup closes, made again when it opens */
    {
        static dgx_ui_node_t pnodes[2];
        static dgx_ui_t      pop;
        static bars_t        own;
        dgx_ui_widget_t      w = a.w;
        w.value = new_place();
        w.own                  = &own;
        dgx_ui_init(&pop, pnodes, 2, 1);
        dgx_ui_add(&pop, -1, &pop_w);
        const int pi = dgx_ui_add(&pop, 0, &w);
        dgx_ui_set_value(&pop, pi, 3);
        CHECK(dgx_ui_popup_open(&a.ui, &pop, 4, 4, 60, 30, 0));
        dgx_ui_paint(&a.ui, &pa, 12);
        dgx_ui_popup_close(&a.ui);
        dgx_ui_paint(&a.ui, &pa, 13);
        CHECK(dgx_ui_popup_open(&a.ui, &pop, 4, 4, 60, 30, 0));
        dgx_ui_paint(&a.ui, &pa, 14);
        CHECK(own.made == 2);
        dgx_ui_popup_close(&a.ui);
        dgx_ui_paint(&a.ui, &pa, 15);
        dgx_ui_free(&pop);
    }
    /* the same tree drawn by a second paint: a canvas of its own, made; the first paint's kept */
    {
        dgx_screen_t  *v2  = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
        dgx_ui_paint_t pv2 = {.scr = v2, .colors = colors, .color_count = 4};
        const int      made = a.own.made;
        dgx_ui_paint_area(&a.ui, &pv2, 0, 0, PANEL_W, PANEL_H);
        CHECK(a.own.made == made + 1);
        dgx_ui_redraw(&a.ui, a.i);
        dgx_ui_paint(&a.ui, &pa, 16);
        CHECK(a.own.made == made + 1);
        dgx_ui_paint_free(&pv2);
        v2->destroy(&v2);
    }
    dgx_ui_paint_free(&pa), dgx_ui_paint_free(&pv);
    CHECK(pa.canvases == NULL);
    dgx_ui_free(&a.ui), dgx_ui_free(&b.ui);

    /* two trees, a bar at the top and a page under it, each its root given: drawn by two paints, their canvases kept
       pass after pass; drawn in turn by one paint, each pass frees the other's, made again, the picture still right */
    {
        static tree_t bar, page, bar2, page2;
        make(&bar, &bars_look, &root_w), make(&page, &bars_look, &root_w), make(&bar2, &bars_look, &root_w), make(&page2, &bars_look, &root_w);
        dgx_ui_set_root(&bar.ui, 0, 0, PANEL_W, 26), dgx_ui_set_root(&page.ui, 0, 26, PANEL_W, PANEL_H - 26);
        dgx_ui_set_root(&bar2.ui, 0, 0, PANEL_W, 26), dgx_ui_set_root(&page2.ui, 0, 26, PANEL_W, PANEL_H - 26);
        dgx_ui_paint_t p1 = {.scr = &panel->sbus.scr, .colors = colors, .color_count = 4}, p2 = p1, one = {.scr = v, .colors = colors, .color_count = 4};
        for (int k = 0; k < 3; ++k) {
            dgx_ui_redraw(&bar.ui, bar.i), dgx_ui_redraw(&page.ui, page.i), dgx_ui_redraw(&bar2.ui, bar2.i), dgx_ui_redraw(&page2.ui, page2.i);
            dgx_ui_paint(&bar.ui, &p1, (uint32_t)k), dgx_ui_paint(&page.ui, &p2, (uint32_t)k);
            dgx_ui_paint(&bar2.ui, &one, (uint32_t)k), dgx_ui_paint(&page2.ui, &one, (uint32_t)k);
        }
        CHECK(bar.own.made == 1 && page.own.made == 1 && bar2.own.made == 3 && page2.own.made == 3 && differ(panel, v) == 0);
        dgx_ui_paint_free(&p1), dgx_ui_paint_free(&p2), dgx_ui_paint_free(&one);
        dgx_ui_free(&bar.ui), dgx_ui_free(&page.ui), dgx_ui_free(&bar2.ui), dgx_ui_free(&page2.ui);
    }

    /* a soft circle of the ink 1: over the paper, its middle the ink and its corner the paper; every pixel once */
    map = dgx_vscreen_init(PANEL_W, PANEL_H, 16, DgxScreenRGB);
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) dgx_set_pixel(map, x, y, map_at(x, y));
    {
        static tree_t s, t;
        make(&s, &soft_look, &root_w), make(&t, &soft_look, &root_w);
        pa = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4};
        pv = (dgx_ui_paint_t){.scr = v, .colors = colors, .color_count = 4};
        panel_count_from_now(panel);
        dgx_ui_paint(&s.ui, &pa, 0), dgx_ui_paint(&t.ui, &pv, 0);
        const dgx_ui_node_t *n  = dgx_ui_node(&t.ui, t.i);
        const int            mx = n->x + n->w / 2, my = n->y + n->h / 2;
        CHECK(panel_twice(panel) == 0 && differ(panel, v) == 0 && dgx_get_pixel(v, mx, my) == colors[1] && dgx_get_pixel(v, n->x, n->y) == colors[0]);
        int soft = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) soft += dgx_get_pixel(v, x, y) != colors[0] && dgx_get_pixel(v, x, y) != colors[1];
        CHECK(soft > 0);
        dgx_ui_paint_free(&pa), dgx_ui_paint_free(&pv);
        dgx_ui_free(&s.ui), dgx_ui_free(&t.ui);
    }
    /* over the map: in a strip mixed with it as on the virtual screen, its corner the map; straight, its corner the map
       too, its soft edge mixed with the paper's colour instead */
    {
        static tree_t  s, t;
        static uint8_t buffer[PANEL_W * 2 * 8];
        make(&s, &soft_look, &root_w), make(&t, &soft_look, &root_w);
        pa = (dgx_ui_paint_t){.scr = &panel->sbus.scr, .colors = colors, .color_count = 4, .buffer = buffer, .buffer_size = sizeof buffer,
                              .strips = &dgx_ui_strips};
        pv = (dgx_ui_paint_t){.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_background_set(&pa, draw_map, map), dgx_ui_background_set(&pv, draw_map, map);
        panel_count_from_now(panel);
        dgx_ui_paint(&s.ui, &pa, 0), dgx_ui_paint(&t.ui, &pv, 0);
        const dgx_ui_node_t *n = dgx_ui_node(&t.ui, t.i);
        CHECK(panel_twice(panel) == 0 && differ(panel, v) == 0 && dgx_get_pixel(v, n->x, n->y) == map_at(n->x, n->y));
        CHECK(dgx_get_pixel(v, n->x + n->w / 2, n->y + n->h / 2) == colors[1]);
        pa.strips = NULL;
        dgx_ui_redraw(&s.ui, s.i);
        dgx_ui_paint(&s.ui, &pa, 1);
        int halo = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) halo += panel->mem[y][x] != dgx_get_pixel(v, x, y);
        CHECK(halo > 0 && panel->mem[n->y][n->x] == map_at(n->x, n->y) && panel->mem[n->y + n->h / 2][n->x + n->w / 2] == colors[1]);
        dgx_ui_paint_free(&pa), dgx_ui_paint_free(&pv);
        dgx_ui_free(&s.ui), dgx_ui_free(&t.ui);
    }
    /* indices over the map: the gaps between the bars the map */
    {
        static tree_t t;
        make(&t, &bars_look, &root_w);
        pv = (dgx_ui_paint_t){.scr = v, .colors = colors, .color_count = 4};
        dgx_ui_background_set(&pv, draw_map, map);
        dgx_ui_paint(&t.ui, &pv, 0);
        const dgx_ui_node_t *n = dgx_ui_node(&t.ui, t.i);
        CHECK(dgx_get_pixel(v, n->x, n->y) == colors[1] && dgx_get_pixel(v, n->x + 2, n->y) == map_at(n->x + 2, n->y));
        CHECK(dgx_get_pixel(v, n->x + 20, n->y) == map_at(n->x + 20, n->y));
        dgx_ui_paint_free(&pv);
        dgx_ui_free(&t.ui);
    }
    /* a screen of 24 bits: the box paper, told in the log */
    {
        static tree_t  t;
        dgx_screen_t  *v24 = dgx_vscreen_init(PANEL_W, PANEL_H, 24, DgxScreenRGB);
        static uint32_t c24[4] = {0x102030, 0xffffff, 0x404040, 0xff0000};
        dgx_ui_paint_t p24 = {.scr = v24, .colors = c24, .color_count = 4};
        make(&t, &bars_look, &root_w);
        dgx_ui_paint(&t.ui, &p24, 0);
        const dgx_ui_node_t *n = dgx_ui_node(&t.ui, t.i);
        int                  off = 0;
        for (int y = n->y; y < n->y + n->h; ++y)
            for (int x = n->x; x < n->x + n->w; ++x) off += dgx_get_pixel(v24, x, y) != c24[0];
        CHECK(off == 0);
        dgx_ui_paint_free(&p24);
        dgx_ui_free(&t.ui);
        v24->destroy(&v24);
    }

    v->destroy(&v), map->destroy(&map);
    free(panel);
    CHECK_DONE();
}
