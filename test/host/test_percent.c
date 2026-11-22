/*
 * Sizes, places, pads and gaps in percent (DGX_UI_PERCENT_*): DGX_UI_PERCENT() at its edges; along a row's or a
 * column's stacking, percents of the room the children share by running totals, so that two of 50 % with a gap and
 * three of 33.33 % fill it to the pixel, a hidden child out of it all, points and a REST beside them; across, and in a
 * box of neither, of the inside, with places from both edges; a pad of the box's width, a gap of the inside along; 0 %
 * against 0 points; STRETCH over a percent; nested; the root's children of the screen and of a rectangle given; a
 * point of 2 pixels; a turned screen; and trees made by chance against a plain writing of the rules.
 */
#include <math.h>
#include <string.h>

#include "check.h"
#include "dgx_ui.h"

/* the macro is a constant expression: these are laid at compile time */
static const int16_t percents[] = {DGX_UI_PERCENT(50),     DGX_UI_PERCENT(33.33), DGX_UI_PERCENT(-5),  DGX_UI_PERCENT(127.99), DGX_UI_PERCENT(128),
                                   DGX_UI_PERCENT(1000),   DGX_UI_PERCENT(-128),  DGX_UI_PERCENT(-200), DGX_UI_PERCENT(0),     DGX_UI_PERCENT(0.01),
                                   DGX_UI_PERCENT(-127.99), DGX_UI_PERCENT(127.998)};
static const int16_t expected_percents[] = {12800, 8532, -1280, 32765, 32767, 32767, -32768, -32768, 0, 3, -32765, 32767};

enum { NODES = 64 };
static dgx_ui_node_t nodes[NODES];
static dgx_ui_t      ui;

/* a tree of a root and its children, laid on a screen of width x height, or in the rectangle given when w > 0 */
static void lay_out(const dgx_ui_widget_t *root, const dgx_ui_widget_t *const *children, int count, int width, int height, int unit)
{
    dgx_ui_init(&ui, nodes, NODES, unit);
    dgx_ui_screen(&ui, width, height, 0);
    const int at = dgx_ui_add(&ui, -1, root);
    for (int k = 0; k < count; ++k) dgx_ui_add(&ui, at, children[k]);
    dgx_ui_layout(&ui);
}

static const dgx_ui_node_t *node(int i)
{
    return dgx_ui_node(&ui, i);
}

/* a percent of a length, as the rules say it: rounded half away from zero */
static int plain_percent(int length, int32_t percent)
{
    return (int)round((double)length * percent / 25600.0);
}

/* a running total of percents, held within 255.99 % either way */
static int32_t plain_total(int32_t total, int32_t percent)
{
    total += percent;
    return total > 65535 ? 65535 : total < -65535 ? -65535 : total;
}

static uint32_t chance = 4949;
static int      next_chance(int n)
{
    chance = chance * 1103515245u + 12345u;
    return (int)((chance >> 8) % (uint32_t)n);
}

int main(void)
{
    for (size_t k = 0; k < sizeof percents / sizeof *percents; ++k) {
        if (percents[k] != expected_percents[k]) fprintf(stderr, "  DGX_UI_PERCENT #%zu: %d\n", k, percents[k]);
        CHECK(percents[k] == expected_percents[k]);
    }

    /* two of 50 % with a gap fill a row exactly, every width, with a pad and without */
    for (int pad = 0; pad <= 3; pad += 3)
        for (int width = 1; width <= 400; ++width) {
            const dgx_ui_widget_t  row  = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .pad = (int16_t)pad, .gap = 4};
            const dgx_ui_widget_t  half = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_W, .want_w = DGX_UI_PERCENT(50)};
            const dgx_ui_widget_t *two[] = {&half, &half};
            lay_out(&row, two, 2, width, 50, 1);
            const int inside = width - 2 * pad, room = inside - 4 > 0 ? inside - 4 : 0;
            CHECK(node(1)->w + node(2)->w == room && node(1)->w == plain_percent(room, 12800));
            CHECK(node(1)->x == pad && node(2)->x == pad + node(1)->w + 4 && node(1)->h == 50 - 2 * pad);
        }

    /* three of 33.33 % fill every room of 1 to 3125 pixels; past it the 0.02 % they lack shows */
    {
        const dgx_ui_widget_t  row   = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW};
        const dgx_ui_widget_t  third = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_W, .want_w = DGX_UI_PERCENT(33.33)};
        const dgx_ui_widget_t *three[] = {&third, &third, &third};
        int                    filled = 0;
        for (int width = 1; width <= 3125; ++width) {
            lay_out(&row, three, 3, width, 10, 1);
            filled += node(1)->w + node(2)->w + node(3)->w == width;
            CHECK(node(1)->w == plain_percent(width, 8532) && node(2)->w == plain_percent(width, 2 * 8532) - node(1)->w);
        }
        CHECK(filled == 3125);
        lay_out(&row, three, 3, 4000, 10, 1);
        CHECK(node(1)->w + node(2)->w + node(3)->w == 3999);

        /* the middle one hidden: no gap for it, no part of the room, no step of the totals: two thirds, not two halves */
        const dgx_ui_widget_t  gapped = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 5};
        const dgx_ui_widget_t  hidden = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_W, .want_w = DGX_UI_PERCENT(33.33), .state = {DGX_UI_HIDDEN}};
        const dgx_ui_widget_t *middle[] = {&third, &hidden, &third};
        lay_out(&gapped, middle, 3, 305, 10, 1);
        CHECK(node(1)->w == plain_percent(300, 8532) && node(3)->w == plain_percent(300, 2 * 8532) - node(1)->w);
        CHECK(node(3)->x == node(1)->w + 5 && node(1)->w == 100 && node(3)->w == 100);
    }

    /* points, a percent and a REST: none takes from the percents; at a point of 2 pixels the points double, the percents not */
    for (int unit = 1; unit <= 2; ++unit) {
        const dgx_ui_widget_t  row     = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 2};
        const dgx_ui_widget_t  points  = {.kind = DGX_UI_BOX, .want_w = 50};
        const dgx_ui_widget_t  quarter = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_W, .want_w = DGX_UI_PERCENT(25)};
        const dgx_ui_widget_t  rest    = {.kind = DGX_UI_BOX, .flags = DGX_UI_REST};
        const dgx_ui_widget_t *mixed[] = {&points, &quarter, &rest};
        lay_out(&row, mixed, 3, 200 * unit, 40, unit);
        const int room = 200 * unit - 2 * 2 * unit;
        CHECK(node(1)->w == 50 * unit && node(2)->w == plain_percent(room, 6400) && node(3)->w == room - 50 * unit - node(2)->w);
        CHECK(node(3)->x + node(3)->w == 200 * unit);
    }

    /* across the stacking, and in a box of neither: of the inside; places from both edges */
    {
        const dgx_ui_widget_t  column = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 10};
        const dgx_ui_widget_t  right  = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_W | DGX_UI_PERCENT_X, .want_w = DGX_UI_PERCENT(25),
                                         .want_h = 20, .at_x = DGX_UI_PERCENT(-10)};
        const dgx_ui_widget_t  left   = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_X, .want_w = 30, .want_h = 20, .at_x = DGX_UI_PERCENT(10)};
        const dgx_ui_widget_t *two[]  = {&right, &left};
        lay_out(&column, two, 2, 220, 100, 1); /* the inside 200 x 80 */
        CHECK(node(1)->w == 50 && node(1)->x == 10 + 200 - 50 - 20 && node(1)->y == 10);
        CHECK(node(2)->w == 30 && node(2)->x == 10 + 20 && node(2)->y == 30);
        const dgx_ui_widget_t  free_box = {.kind = DGX_UI_BOX};
        const dgx_ui_widget_t  low      = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_W | DGX_UI_PERCENT_H | DGX_UI_PERCENT_X | DGX_UI_PERCENT_Y,
                                           .want_w = DGX_UI_PERCENT(20), .want_h = DGX_UI_PERCENT(10), .at_x = DGX_UI_PERCENT(10),
                                           .at_y = DGX_UI_PERCENT(-10)};
        const dgx_ui_widget_t  rest_of  = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_X, .at_x = DGX_UI_PERCENT(-25), .want_h = 5};
        const dgx_ui_widget_t *free_two[] = {&low, &rest_of};
        lay_out(&free_box, free_two, 2, 200, 100, 1);
        CHECK(node(1)->w == 40 && node(1)->h == 10 && node(1)->x == 20 && node(1)->y == 100 - 10 - 10);
        CHECK(node(2)->w == 150 && node(2)->x == 0); /* 0 points wide: the inside less its place, from the far edge */
        const dgx_ui_widget_t  row     = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW};
        const dgx_ui_widget_t  bottom  = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_H | DGX_UI_PERCENT_Y, .want_w = 10,
                                          .want_h = DGX_UI_PERCENT(50), .at_y = DGX_UI_PERCENT(-25)};
        const dgx_ui_widget_t *one[]   = {&bottom};
        lay_out(&row, one, 1, 100, 80, 1);
        CHECK(node(1)->h == 40 && node(1)->y == 80 - 40 - 20);
    }

    /* at a point of 2 pixels a place in percent is still of the pixels, one in points doubles */
    {
        const dgx_ui_widget_t  column = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        const dgx_ui_widget_t  placed = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_X, .want_w = 30, .want_h = 10, .at_x = DGX_UI_PERCENT(10)};
        const dgx_ui_widget_t  points = {.kind = DGX_UI_BOX, .want_w = 30, .want_h = 10, .at_x = -5};
        const dgx_ui_widget_t *two[]  = {&placed, &points};
        lay_out(&column, two, 2, 200, 100, 2);
        CHECK(node(1)->x == 20 && node(1)->w == 60 && node(2)->x == 200 - 60 - 10 && node(2)->y == 20);
    }

    /* a pad of the box's width on every side, a gap of the inside along its stacking */
    {
        const dgx_ui_widget_t  column = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN | DGX_UI_PERCENT_PAD | DGX_UI_PERCENT_GAP,
                                         .pad = DGX_UI_PERCENT(5), .gap = DGX_UI_PERCENT(2.5)};
        const dgx_ui_widget_t  line   = {.kind = DGX_UI_BOX, .want_h = 30};
        const dgx_ui_widget_t *two[]  = {&line, &line};
        lay_out(&column, two, 2, 200, 120, 1); /* pad 10 all round, the inside 180 x 100, the gap 2.5 % of 100 */
        CHECK(node(1)->x == 10 && node(1)->y == 10 && node(1)->w == 180 && node(2)->y == 10 + 30 + 3);
    }

    /* 0 % is no size; 0 points is the content's, across the stacking all of the box; STRETCH over a percent */
    {
        const dgx_ui_widget_t  column   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        const dgx_ui_widget_t  nothing  = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_W | DGX_UI_PERCENT_H, .want_h = 0};
        const dgx_ui_widget_t  all      = {.kind = DGX_UI_BOX, .want_h = 10};
        const dgx_ui_widget_t  stretch  = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_W | DGX_UI_STRETCH, .want_w = DGX_UI_PERCENT(10),
                                           .want_h = 10};
        const dgx_ui_widget_t *three[]  = {&nothing, &all, &stretch};
        lay_out(&column, three, 3, 100, 60, 1);
        CHECK(node(1)->w == 0 && node(1)->h == 0 && node(2)->w == 100 && node(3)->w == 100);
    }

    /* nested: a row of halves in a column's half; the root's children of the screen, then of a rectangle given */
    {
        const dgx_ui_widget_t  column  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN};
        const dgx_ui_widget_t  row     = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW | DGX_UI_PERCENT_H, .want_h = DGX_UI_PERCENT(50)};
        const dgx_ui_widget_t  half    = {.kind = DGX_UI_BOX, .flags = DGX_UI_PERCENT_W, .want_w = DGX_UI_PERCENT(50)};
        dgx_ui_init(&ui, nodes, NODES, 1);
        dgx_ui_screen(&ui, 320, 240, 0);
        const int root = dgx_ui_add(&ui, -1, &column), inner = dgx_ui_add(&ui, root, &row);
        dgx_ui_add(&ui, inner, &half), dgx_ui_add(&ui, inner, &half);
        dgx_ui_layout(&ui);
        CHECK(node(1)->h == 120 && node(2)->w == 160 && node(3)->x == 160 && node(3)->h == 120);
        dgx_ui_set_root(&ui, 20, 10, 200, 100);
        dgx_ui_layout(&ui);
        CHECK(node(1)->y == 10 && node(1)->h == 50 && node(2)->x == 20 && node(2)->w == 100 && node(3)->x == 120);
        /* turned: a screen of the other way, the percents of the box as it is laid now */
        dgx_ui_init(&ui, nodes, NODES, 1);
        dgx_ui_screen(&ui, 320, 240, 0);
        dgx_ui_add(&ui, -1, &column), dgx_ui_add(&ui, 0, &row), dgx_ui_add(&ui, 1, &half), dgx_ui_add(&ui, 1, &half);
        dgx_ui_layout(&ui);
        dgx_ui_screen(&ui, 240, 320, 1);
        dgx_ui_layout(&ui);
        CHECK(node(1)->h == 160 && node(2)->w == 120 && node(3)->x == 120);
    }

    /* trees made by chance against a plain writing of the rules: a row or a column of children of points, of percents
       and of the rest, some hidden, a pad and a gap of points or percent, at a point of 1 or 2 pixels */
    for (int round_ = 0; round_ < 4000; ++round_) {
        const bool      column = next_chance(2), pad_percent = next_chance(2), gap_percent = next_chance(2);
        const int       unit = 1 + next_chance(2), width = 1 + next_chance(600), height = 1 + next_chance(600), count = 1 + next_chance(8);
        dgx_ui_widget_t box  = {.kind = DGX_UI_BOX, .flags = column ? DGX_UI_COLUMN : DGX_UI_ROW};
        box.pad              = (int16_t)(pad_percent ? next_chance(30 * 256) : next_chance(6));
        box.gap              = (int16_t)(gap_percent ? next_chance(10 * 256) : next_chance(6));
        box.flags |= (pad_percent ? DGX_UI_PERCENT_PAD : 0) | (gap_percent ? DGX_UI_PERCENT_GAP : 0);
        static dgx_ui_widget_t  children[8];
        const dgx_ui_widget_t  *list[8];
        for (int k = 0; k < count; ++k) {
            dgx_ui_widget_t *w = &children[k];
            *w                 = (dgx_ui_widget_t){.kind = DGX_UI_BOX, .want_w = 10, .want_h = 10};
            const int what     = next_chance(4);
            int16_t  *along    = column ? &w->want_h : &w->want_w;
            if (what == 0) *along = (int16_t)next_chance(80);
            else if (what <= 2) *along = (int16_t)next_chance(60 * 256), w->flags = column ? DGX_UI_PERCENT_H : DGX_UI_PERCENT_W;
            else w->flags = DGX_UI_REST;
            if (next_chance(6) == 0) w->state.bits = DGX_UI_HIDDEN;
            list[k] = w;
        }
        lay_out(&box, list, count, width, height, unit);
        /* the rules, written plainly */
        const int pad    = pad_percent ? plain_percent(width, box.pad) : box.pad * unit;
        const int inside = (column ? height : width) - 2 * pad;
        const int gap    = gap_percent ? plain_percent(inside, box.gap) : box.gap * unit;
        int       shown = 0, takers = 0, fixed = 0;
        int32_t   total = 0;
        for (int k = 0; k < count; ++k) {
            const dgx_ui_widget_t *w = &children[k];
            if (w->state.bits & DGX_UI_HIDDEN) continue;
            ++shown;
            if (w->flags & DGX_UI_REST) ++takers;
            else if (w->flags & (DGX_UI_PERCENT_W | DGX_UI_PERCENT_H)) total = plain_total(total, column ? w->want_h : w->want_w);
            else fixed += (column ? w->want_h : w->want_w) * unit;
        }
        int room = inside - (shown > 1 ? (shown - 1) * gap : 0);
        if (room < 0) room = 0;
        int left = room - fixed - plain_percent(room, total);
        if (left < 0) left = 0;
        int     before = 0, taken = 0, along = 0;
        int32_t sum    = 0;
        bool    same   = true;
        for (int k = 0; k < count; ++k) {
            const dgx_ui_widget_t *w = &children[k];
            if (w->state.bits & DGX_UI_HIDDEN) continue;
            int size;
            if (w->flags & DGX_UI_REST) size = ++taken < takers ? left / takers : left - left / takers * (takers - 1);
            else if (w->flags & (DGX_UI_PERCENT_W | DGX_UI_PERCENT_H)) {
                sum = plain_total(sum, column ? w->want_h : w->want_w);
                const int upto = plain_percent(room, sum);
                size = upto - before, before = upto;
            } else {
                size = (column ? w->want_h : w->want_w) * unit;
            }
            const dgx_ui_node_t *n = node(1 + k);
            same = same && (column ? n->h : n->w) == size && (column ? n->y : n->x) == pad + along;
            along += size + gap;
        }
        if (!same) fprintf(stderr, "  round %d: %s %dx%d unit %d, %d children\n", round_, column ? "column" : "row", width, height, unit, count);
        CHECK(same);
    }

    CHECK_DONE();
}
