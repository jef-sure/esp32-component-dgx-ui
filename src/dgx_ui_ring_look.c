/*
 * dgx_ui_ring_look.c
 *
 *  The look of a ring scale: the value of the widget's place as an arc of a ring in the middle of its box. The box is
 *  written row by row in runs of one colour, each run cut by the widget's children on that row, so that every pixel of
 *  the box is written once by the ring or by a child. A pixel is classed by its centre less the ring's middle, in
 *  halves of a pixel of the tree's screen and in integers alone: its square distance against the two circles, and in
 *  the ring its turn from the start against the rays of the value and of the bands, compared without division. So a
 *  part under any clip and a strip moved by any whole pixels give the same pixels, and a value reached by steps is the
 *  same picture as that value drawn whole. The rays come from a table of a quarter of a sine, the same on every target.
 *  Along a row on one side of the middle column the turn only grows or only shrinks, so the ring's part of a row is
 *  not classed pixel by pixel: a piece whose first and last pixels lie on the same side of every ray and in the same
 *  band is one class, and any other is halved.
 *  When only the value moved, only the pixels of the arc whose colour changed are written, within the box that holds
 *  the arc between the two values. A program that names it not carries none of it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include "dgx_ui_paint_priv.h"

/* a quarter of a sine, a degree a step, of 32767 */
static const int16_t quarter_sine[91] = {
    0,     572,   1144,  1715,  2286,  2856,  3425,  3993,  4560,  5126,  5690,  6252,  6813,  7371,  7927,  8481,  9032,  9580,  10126,
    10668, 11207, 11743, 12275, 12803, 13328, 13848, 14364, 14876, 15383, 15886, 16383, 16876, 17364, 17846, 18323, 18794, 19260, 19720,
    20173, 20621, 21062, 21497, 21925, 22347, 22762, 23170, 23571, 23964, 24351, 24730, 25101, 25465, 25821, 26169, 26509, 26841, 27165,
    27481, 27788, 28087, 28377, 28659, 28932, 29196, 29451, 29697, 29934, 30162, 30381, 30591, 30791, 30982, 31163, 31335, 31498, 31650,
    31794, 31927, 32051, 32165, 32269, 32364, 32448, 32523, 32587, 32642, 32687, 32722, 32747, 32762, 32767};

enum { DEGREE = 64, QUARTER = 90 * DEGREE, TURN = 360 * DEGREE }; /* angles in 64ths of a degree */
enum { AROUND = 0x100 };                                           /* the class of the paper; the others are colours */
enum { BANDS_READ = 16 };                                          /* bands by place whose rays are made once a call; the rest
                                                                      each time a pixel asks */

/* the sine of an angle in 64ths of a degree, of 32767: a line between two degrees of the table */
static int32_t sine_of(int32_t angle)
{
    angle %= TURN;
    if (angle < 0) angle += TURN;
    const int quarter = angle / QUARTER;
    int32_t   in      = angle % QUARTER;
    if (quarter & 1) in = QUARTER - in; /* the second and the fourth quarters run back */
    const int     degree = in / DEGREE, part = in % DEGREE;
    const int32_t s      = degree >= 90 ? 32767 : quarter_sine[degree] + (quarter_sine[degree + 1] - quarter_sine[degree]) * part / DEGREE;
    return quarter >= 2 ? -s : s;
}

/* a direction's turn clockwise from the top: its quarter, and in it along / of, which grows with the angle and is
   compared by cross multiplication; x right, y up */
typedef struct turn_t {
    int8_t  quarter;
    int32_t along, of;
} turn_t;

static turn_t turn_of(int32_t x, int32_t y)
{
    if (x >= 0 && y > 0) return (turn_t){0, x, x + y};
    if (x > 0 && y <= 0) return (turn_t){1, -y, x - y};
    if (x <= 0 && y < 0) return (turn_t){2, -x, -x - y};
    return (turn_t){3, y, y - x};
}

static bool before(const turn_t *a, const turn_t *b)
{
    return a->quarter != b->quarter ? a->quarter < b->quarter : a->along * b->of < b->along * a->of;
}

/* a turn within the turn clockwise from the start up to a ray, the start's ray in it and the other not: given whether
   it lies at the start's ray or past it, whether before the other, and whether the turn up to the other passes the top */
static bool within_past(bool past_start, bool wraps, bool before_ray)
{
    return wraps ? past_start || before_ray : past_start && before_ray;
}

/* the turn of the ray at an angle, clockwise from the top */
static turn_t ray_at(int32_t angle)
{
    return turn_of(sine_of(angle), sine_of(angle + QUARTER));
}

/* how far a value lights the arc: none, up to a ray, all of the sweep */
enum { NONE, UP_TO, ALL };
typedef struct lit_t {
    uint8_t how;
    bool    wraps; /* the turn from the start to `to` passes the top */
    turn_t  to;
    int32_t angle; /* the ray's, from the start, in 64ths of a degree */
    int     color; /* the arc's colour, by the value shown; by place, -1 */
} lit_t;

typedef struct ring_t {
    dgx_ui_paint_t            *paint;
    dgx_ui_t                  *ui;
    const dgx_ui_node_t       *n;
    const dgx_ui_ring_look_t  *p;
    int                        cx2, cy2;       /* the middle, in halves of a pixel of the tree's screen */
    int32_t                    outer2, inner2; /* the squares of the radii, in halves */
    int32_t                    outer, inner;
    bool                       disc;           /* no inner circle */
    bool                       mirrored;       /* counter-clockwise: the pixels mirrored across the middle */
    bool                       full;           /* a sweep of all the circle */
    int32_t                    start, sweep;   /* in 64ths of a degree, clockwise */
    turn_t                     from, to;       /* the sweep's rays */
    bool                       wraps;          /* the sweep passes the top */
    int                        bands;          /* bands by place read */
    lit_t                      band[BANDS_READ];
} ring_t;

/* the value's lighting: within min..max, none at min, all at max */
static lit_t lit_of(const ring_t *r, int32_t value)
{
    const dgx_ui_widget_t *w   = r->n->widget;
    lit_t                  lit = {.how = NONE, .color = -1};
    if (w->max <= w->min || value <= w->min) lit.how = NONE;
    else if (value >= w->max) lit.how = ALL, lit.angle = r->sweep;
    else {
        lit.how   = UP_TO;
        lit.angle = (int32_t)((uint32_t)r->sweep * (uint32_t)(value - w->min) / (uint32_t)(w->max - w->min));
        lit.to    = ray_at(r->start + lit.angle);
        lit.wraps = before(&lit.to, &r->from);
    }
    if (r->p->bands_by != DGX_UI_BANDS_BY_PLACE || !r->p->band_count) {
        lit.color = r->p->on;
        for (int k = 0; k < r->p->band_count; ++k)
            if (value < r->p->bands[k].below) {
                lit.color = r->p->bands[k].color;
                break;
            }
    }
    return lit;
}

/* the key of a pixel of the ring, its centre dx, dy halves of a pixel from the middle, y down: on which side of each
   ray it lies, and the first band by place that holds it; its class is the key's. Along a piece of a row on one side
   of the middle column the turn only grows or only shrinks, so each side changes once at most, and the band, while
   the side of the start's ray stays: two pixels of a piece of one key hold that class between them. `drawn` is the
   lighting drawn, or in a pass drawn whole `now` again */
enum { PAST_START = 1, BEFORE_END = 2, PAST_END = 4, BEFORE_NOW = 8, BEFORE_DRAWN = 16, BAND_SHIFT = 5 };
static uint32_t key_of(const ring_t *r, int32_t dx, int32_t dy, const lit_t *now, const lit_t *drawn)
{
    const turn_t t          = dx == 0 && dy == 0 ? r->from : turn_of(r->mirrored ? -dx : dx, -dy); /* the middle at the start */
    const bool   past_start = !before(&t, &r->from);
    uint32_t     key        = (past_start ? PAST_START : 0) | (before(&t, &r->to) ? BEFORE_END : 0) | (before(&r->to, &t) ? PAST_END : 0);
    if (now->how == UP_TO && before(&t, &now->to)) key |= BEFORE_NOW;
    if (drawn->how == UP_TO && before(&t, &drawn->to)) key |= BEFORE_DRAWN;
    int k = 0;
    if (now->color < 0)
        for (; k < r->p->band_count; ++k) {
            lit_t        read;
            const lit_t *band = k < r->bands ? &r->band[k] : (read = lit_of(r, r->p->bands[k].below), &read);
            if (band->how == ALL || (band->how == UP_TO && within_past(past_start, band->wraps, before(&t, &band->to)))) break;
        }
    return key | (uint32_t)k << BAND_SHIFT;
}

/* the class of a key, by a lighting whose ray's side is the key's bit `before_lit`: AROUND, or a colour */
static int class_of(const ring_t *r, uint32_t key, const lit_t *lit, uint32_t before_lit)
{
    const bool past_start = key & PAST_START;
    if (!r->sweep) return AROUND; /* no scale */
    if (!r->full && !within_past(past_start, r->wraps, key & BEFORE_END) && (key & (BEFORE_END | PAST_END)))
        return AROUND; /* outside the sweep, and not on its end ray */
    const bool on = lit->how == ALL || (lit->how == UP_TO && within_past(past_start, lit->wraps, key & before_lit));
    if (!on) return r->p->off;
    if (lit->color >= 0) return lit->color;
    const int band = (int)(key >> BAND_SHIFT);
    return band < r->p->band_count ? r->p->bands[band].color : r->p->on;
}

/* the runs of a row being made: from `start` to `end`, of a class, -1 none or unchanged */
typedef struct run_t {
    int start, end, cls;
} run_t;

static void emit(const ring_t *r, int y, int x0, int x1, int cls, int child);

/* pixels from a to b of one class added to the run, which is laid when the next ones differ; at the row's end a class
   of -1 lays the last */
static void add_run(const ring_t *r, int y, run_t *run, int a, int b, int cls)
{
    if (cls == run->cls && a == run->end) {
        run->end = b;
        return;
    }
    if (run->cls >= 0 && run->end > run->start) emit(r, y, run->start, run->end, run->cls, r->n->child);
    *run = (run_t){a, b, cls};
}

/* the pixels from a to b of a piece of a row on one side of the middle column, of the keys first_key and last_key at its
   ends: of one key, a run of its class, or with `drawn`, of its class where the class changed; else its two halves */
static void piece(const ring_t *r, int y, run_t *run, int32_t dy, int a, int b, uint32_t first_key, uint32_t last_key, const lit_t *now, const lit_t *drawn)
{
    if (first_key == last_key) {
        int cls = class_of(r, first_key, now, BEFORE_NOW);
        if (drawn && class_of(r, first_key, drawn, BEFORE_DRAWN) == cls) cls = -1;
        add_run(r, y, run, a, b, cls);
        return;
    }
    const lit_t   *second    = drawn ? drawn : now;
    const int      half      = a + (b - a) / 2;
    const uint32_t left_end  = half - 1 == a ? first_key : key_of(r, 2 * (half - 1) + 1 - r->cx2, dy, now, second);
    const uint32_t right_end = half == b - 1 ? last_key : key_of(r, 2 * half + 1 - r->cx2, dy, now, second);
    piece(r, y, run, dy, a, half, first_key, left_end, now, drawn);
    piece(r, y, run, dy, half, b, right_end, last_key, now, drawn);
}

/* a run of a row laid: the paper or a colour */
static void lay(const ring_t *r, int y, int x0, int x1, int cls)
{
    dgx_ui_lay_paper(r->paint, cls == AROUND ? r->p->paper : (uint8_t)cls, x0, y, x1 - x0, 1);
}

/* a run of a row less the children on that row, from `child` on: what lies under a child is the child's */
static void emit(const ring_t *r, int y, int x0, int x1, int cls, int child)
{
    for (; x0 < x1 && child >= 0; child = dgx_ui_at(r->ui, child)->next) {
        const dgx_ui_node_t *k = dgx_ui_at(r->ui, child);
        if ((k->state.bits & DGX_UI_HIDDEN) || !dgx_ui_look_of(k->widget)) continue; /* covers nothing */
        if (y < k->y || y >= k->y + k->h || k->x >= x1 || k->x + k->w <= x0) continue;
        if (k->x > x0) emit(r, y, x0, k->x, cls, k->next); /* left of it, past the children before it already */
        x0 = k->x + k->w;
    }
    if (x0 < x1) lay(r, y, x0, x1, cls);
}

/* the least x whose centre lies within `reach` halves of the middle, and the last; reach < 0: none */
static void span(const ring_t *r, int32_t reach, int *x0, int *x1)
{
    /* |2x + 1 - cx2| <= reach: x from ceil((cx2 - 1 - reach) / 2) to floor((cx2 - 1 + reach) / 2) */
    const int32_t lo = r->cx2 - 1 - reach, hi = r->cx2 - 1 + reach;
    *x0 = lo >= 0 ? (lo + 1) / 2 : -((-lo) / 2);
    *x1 = hi >= 0 ? hi / 2 : -((-hi + 1) / 2);
}

/* the integer square root: the largest m with m * m <= v; v < 0: -1 */
static int32_t root_of(int32_t v)
{
    if (v < 0) return -1;
    uint32_t m = 0;
    for (uint32_t bit = 1u << 15; bit; bit >>= 1)
        if ((m + bit) * (m + bit) <= (uint32_t)v) m += bit;
    return (int32_t)m;
}

/* a row of the box from x0 to x1 (the clip's): its runs, of the classes `now` gives, or, `drawn` given, only the pixels
   of the ring whose class changed. The row is cut into the paper outside the outer circle, the ring, the paper inside
   the inner one, the ring, the paper outside again: the paper's runs laid whole, the ring's parts cut where the middle
   column is, each piece into runs of one class (piece()) */
static void row(const ring_t *r, int y, int x0, int x1, const lit_t *now, const lit_t *drawn)
{
    const int32_t dy    = 2 * y + 1 - r->cy2;
    const int32_t outer = dy > r->outer || dy < -r->outer ? -1 : root_of(r->outer2 - dy * dy);
    const int32_t inner = r->disc || outer < 0 ? -1 : root_of(r->inner2 - dy * dy);
    int           cut[6], kind[5], parts = 0; /* the parts: from cut[k] to cut[k + 1], of kind[k], 1 the ring */
    cut[0] = x0;
    if (outer >= 0) {
        int ox0, ox1;
        span(r, outer, &ox0, &ox1);
        cut[++parts] = ox0, kind[parts - 1] = 0;
        if (inner >= 0) {
            int ix0, ix1;
            span(r, inner, &ix0, &ix1);
            cut[++parts] = ix0, kind[parts - 1] = 1;
            cut[++parts] = ix1 + 1, kind[parts - 1] = 0;
        }
        cut[++parts] = ox1 + 1, kind[parts - 1] = 1;
    }
    cut[++parts] = x1, kind[parts - 1] = 0;
    const int child = r->n->child;
    for (int k = 0; k < parts; ++k) {
        const int a = cut[k] < x0 ? x0 : cut[k] > x1 ? x1 : cut[k], b = cut[k + 1] < x0 ? x0 : cut[k + 1] > x1 ? x1 : cut[k + 1];
        if (b <= a) continue;
        if (!kind[k]) {
            if (!drawn) emit(r, y, a, b, AROUND, child); /* the paper never changes with the value */
            continue;
        }
        /* the part cut where the middle column is: left of it, on it, right of it */
        const lit_t *second    = drawn ? drawn : now;
        run_t        run       = {a, a, -1};
        const int    middle    = r->cx2 >= 1 ? r->cx2 / 2 : -((1 - r->cx2) / 2); /* the least x of dx >= 0 */
        const int    on_middle = r->cx2 & 1;                                       /* a column of dx == 0 */
        const int    ends[4]   = {a, middle < a ? a : middle > b ? b : middle,
                                  middle + on_middle < a ? a : middle + on_middle > b ? b : middle + on_middle, b};
        for (int e = 0; e < 3; ++e)
            if (ends[e + 1] > ends[e])
                piece(r, y, &run, dy, ends[e], ends[e + 1], key_of(r, 2 * ends[e] + 1 - r->cx2, dy, now, second),
                      key_of(r, 2 * (ends[e + 1] - 1) + 1 - r->cx2, dy, now, second), now, drawn);
        add_run(r, y, &run, b, b, -1);
    }
}

/* the rows and columns that hold the arc between two angles from the start, in pixels of the tree's screen */
static void arc_box(const ring_t *r, int32_t a0, int32_t a1, int *x0, int *y0, int *x1, int *y1)
{
    int32_t lo = a0 < a1 ? a0 : a1, hi = a0 < a1 ? a1 : a0;
    int32_t xs[12], ys[12];
    int     count = 0;
    /* the rays' ends on the two circles, and the outer circle's farthest points the arc passes */
    const int32_t radii[2] = {r->outer, r->disc ? 0 : r->inner};
    const int32_t ends[2]  = {lo, hi};
    for (int e = 0; e < 2; ++e)
        for (int k = 0; k < 2; ++k) {
            const int32_t angle = r->start + ends[e];
            xs[count] = radii[k] * sine_of(angle) / 32767, ys[count] = radii[k] * sine_of(angle + QUARTER) / 32767, ++count;
        }
    for (int32_t q = ((r->start + lo) / QUARTER - 1) * QUARTER; q <= r->start + hi && count < 12; q += QUARTER)
        if (q >= r->start + lo) xs[count] = r->outer * sine_of(q) / 32767, ys[count] = r->outer * sine_of(q + QUARTER) / 32767, ++count;
    int32_t minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
    for (int k = 1; k < count; ++k) {
        minx = xs[k] < minx ? xs[k] : minx, maxx = xs[k] > maxx ? xs[k] : maxx;
        miny = ys[k] < miny ? ys[k] : miny, maxy = ys[k] > maxy ? ys[k] : maxy;
    }
    if (r->mirrored) {
        const int32_t m = minx;
        minx = -maxx, maxx = -m;
    }
    /* halves of a pixel from the middle, y up, to pixels; a pixel more on each side for the table's error */
    *x0 = (r->cx2 + minx) / 2 - 2, *x1 = (r->cx2 + maxx) / 2 + 3;
    *y0 = (r->cy2 - maxy) / 2 - 2, *y1 = (r->cy2 - miny) / 2 + 3;
}

bool dgx_ui_draw_ring(const dgx_ui_look_t *look, dgx_ui_paint_t *paint, dgx_ui_t *ui, int i, uint32_t now_ms)
{
    const dgx_ui_node_t      *n = dgx_ui_node(ui, i);
    const dgx_ui_ring_look_t *p = look->params;
    static bool               told; /* once for the program, every tree and task: two tasks at once at worst tell it twice */
    dgx_ui_tell_no_place(i, n->widget, "a ring scale", &told);
    if (n->w <= 0 || n->h <= 0) return false;
    ring_t r = {.paint = paint, .ui = ui, .n = n, .p = p};
    /* the ring in the middle of the box, as large as its lesser side; in halves of a pixel */
    const int side = n->w < n->h ? n->w : n->h;
    r.cx2 = 2 * n->x + n->w, r.cy2 = 2 * n->y + n->h;
    r.outer = side, r.inner = side - 2 * p->width * ui->unit;
    r.disc  = r.inner <= 0;
    r.outer2 = r.outer * r.outer, r.inner2 = r.disc ? 0 : r.inner * r.inner;
    /* the sweep clockwise, a counter-clockwise one mirrored */
    int32_t start = p->start, sweep = p->sweep;
    if (sweep < 0) r.mirrored = true, start = -start, sweep = -sweep;
    if (sweep > 360) sweep = 360;
    r.start = start * DEGREE, r.sweep = sweep * DEGREE, r.full = sweep == 360;
    r.from = ray_at(r.start), r.to = ray_at(r.start + r.sweep), r.wraps = before(&r.to, &r.from);
    if (p->bands_by == DGX_UI_BANDS_BY_PLACE)
        for (; r.bands < p->band_count && r.bands < BANDS_READ; ++r.bands) r.band[r.bands] = lit_of(&r, p->bands[r.bands].below);
    const lit_t now = lit_of(&r, dgx_ui_value_of(n));
    /* the rows and columns to go through: the clip's, and when only the value moved, the arc's between the two */
    int cx, cy, cw, ch;
    dgx_screen_clip_get(paint->scr, &cx, &cy, &cw, &ch);
    int        x0 = cx + paint->ox, y0 = cy + paint->oy, x1 = x0 + cw, y1 = y0 + ch;
    const bool moved = !paint->whole && dgx_ui_bits_shown(ui, n) == n->state_drawn.bits;
    lit_t      drawn = now;
    if (moved) {
        drawn = lit_of(&r, n->state_drawn.value);
        if (drawn.color == now.color) {
            int ax0, ay0, ax1, ay1;
            arc_box(&r, drawn.how == NONE ? 0 : drawn.angle, now.how == NONE ? 0 : now.angle, &ax0, &ay0, &ax1, &ay1);
            x0 = x0 > ax0 ? x0 : ax0, y0 = y0 > ay0 ? y0 : ay0, x1 = x1 < ax1 ? x1 : ax1, y1 = y1 < ay1 ? y1 : ay1;
        }
    }
    x0 = x0 > n->x ? x0 : n->x, y0 = y0 > n->y ? y0 : n->y;
    x1 = x1 < n->x + n->w ? x1 : n->x + n->w, y1 = y1 < n->y + n->h ? y1 : n->y + n->h;
    for (int y = y0; y < y1 && x0 < x1; ++y) row(&r, y, x0, x1, &now, moved ? &drawn : NULL);
    return false;
}

static const dgx_ui_ring_look_t ring_default = {.on = 3, .off = 2, .paper = DGX_UI_PAPER, .width = 3, .start = -135, .sweep = 270, .bands_by = DGX_UI_BANDS_BY_PLACE};
const dgx_ui_look_t             dgx_ui_look_ring = {.draw = dgx_ui_draw_ring, .params = &ring_default, .fill = -1};
