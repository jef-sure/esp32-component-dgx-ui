/*
 * The rectangles of dgx-ui: their common part and the parts of a box outside a cut, each case compared with the
 * pixels counted one by one on a small grid, every box against every cut.
 */
#include "check.h"
#include "dgx_ui.h"

static bool inside(const dgx_ui_rect_t *r, int x, int y)
{
    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

/* every rectangle with its corner in 0..4 and its size in -1..4: some empty (a negative size too), some touching,
   some apart */
enum { LO = 0, HI = 4, SIZE_LO = -1, GRID_LO = -1, GRID_HI = 10 };

static int intersect_failures, subtract_failures;

static void check_intersect(const dgx_ui_rect_t *a, const dgx_ui_rect_t *b)
{
    dgx_ui_rect_t out;
    const bool    any  = dgx_ui_rect_intersect(a, b, &out);
    bool          seen = false, wrong = false;
    for (int y = GRID_LO; y < GRID_HI; ++y)
        for (int x = GRID_LO; x < GRID_HI; ++x) {
            const bool both = inside(a, x, y) && inside(b, x, y);
            seen |= both;
            if (both != inside(&out, x, y)) wrong = true;
        }
    if (any != seen || (!any && (out.x || out.y || out.w || out.h)) || wrong) ++intersect_failures;
}

static void check_subtract(const dgx_ui_rect_t *box, const dgx_ui_rect_t *cut)
{
    dgx_ui_rect_t out[4];
    const int     n     = dgx_ui_rect_subtract(box, cut, out);
    bool          wrong = n < 0 || n > 4;
    for (int i = 0; i < n && !wrong; ++i)
        if (dgx_ui_rect_empty(&out[i])) wrong = true; /* no empty part is given */
    for (int y = GRID_LO; y < GRID_HI && !wrong; ++y)
        for (int x = GRID_LO; x < GRID_HI && !wrong; ++x) {
            int covered = 0;
            for (int i = 0; i < n; ++i) covered += inside(&out[i], x, y);
            const bool wanted = inside(box, x, y) && !(dgx_ui_rect_empty(cut) ? false : inside(cut, x, y));
            if (covered != (wanted ? 1 : 0)) wrong = true; /* every pixel wanted once, none else */
        }
    if (wrong) ++subtract_failures;
}

int main(void)
{
    /* the cases told by name */
    dgx_ui_rect_t out[4], r;
    const dgx_ui_rect_t box = dgx_ui_rect(10, 20, 100, 50);

    CHECK(dgx_ui_rect_subtract(&box, &(dgx_ui_rect_t){0, 0, 5, 5}, out) == 1 && out[0].x == 10 && out[0].w == 100);
    CHECK(dgx_ui_rect_subtract(&box, &(dgx_ui_rect_t){0, 0, 200, 200}, out) == 0);
    CHECK(dgx_ui_rect_subtract(&box, &(dgx_ui_rect_t){40, 30, 20, 10}, out) == 4);
    /* the band above first, as wide as the box */
    CHECK(out[0].x == 10 && out[0].y == 20 && out[0].w == 100 && out[0].h == 10);
    CHECK(dgx_ui_rect_subtract(&box, &(dgx_ui_rect_t){0, 30, 500, 10}, out) == 2); /* a band across: above and below */
    CHECK(dgx_ui_rect_subtract(&box, &(dgx_ui_rect_t){40, 30, 0, 10}, out) == 1);  /* an empty cut takes nothing */
    CHECK(dgx_ui_rect_subtract(&(dgx_ui_rect_t){10, 20, 0, 50}, &box, out) == 0);  /* an empty box has no part */

    /* touching sides share no pixel */
    CHECK(!dgx_ui_rect_intersect(&box, &(dgx_ui_rect_t){110, 20, 10, 10}, &r) && r.w == 0 && r.h == 0);
    /* the out argument may be one of the two */
    r = dgx_ui_rect(0, 0, 20, 30);
    CHECK(dgx_ui_rect_intersect(&r, &box, &r) && r.x == 10 && r.y == 20 && r.w == 10 && r.h == 10);
    /* at the far end of int16_t: right and bottom counted past it */
    /* the sums inside the calls past int16_t: the box's right and bottom, 32800, do not fit in it; the parts
       given still lie within 32767, as the contract of dgx_ui_rect_subtract() asks of the box */
    const dgx_ui_rect_t far = dgx_ui_rect(32700, 32700, 100, 100);
    CHECK(dgx_ui_rect_intersect(&far, &(dgx_ui_rect_t){32750, 32750, 40, 40}, &r) && r.x == 32750 && r.w == 40 && r.h == 40);
    CHECK(dgx_ui_rect_subtract(&far, &(dgx_ui_rect_t){32720, 32720, 20, 20}, out) == 4);

    /* every box against every cut on the grid */
    int pairs = 0;
    for (int bx = LO; bx <= HI; ++bx)
        for (int by = LO; by <= HI; ++by)
            for (int bw = SIZE_LO; bw <= HI; ++bw)
                for (int bh = SIZE_LO; bh <= HI; ++bh)
                    for (int cx = LO; cx <= HI; ++cx)
                        for (int cy = LO; cy <= HI; ++cy)
                            for (int cw = SIZE_LO; cw <= HI; ++cw)
                                for (int ch = SIZE_LO; ch <= HI; ++ch) {
                                    const dgx_ui_rect_t b = dgx_ui_rect(bx, by, bw, bh), c = dgx_ui_rect(cx, cy, cw, ch);
                                    check_intersect(&b, &c);
                                    check_subtract(&b, &c);
                                    ++pairs;
                                }
    CHECK(intersect_failures == 0);
    CHECK(subtract_failures == 0);
    if (intersect_failures || subtract_failures)
        fprintf(stderr, "of %d pairs: intersect wrong %d, subtract wrong %d\n", pairs, intersect_failures, subtract_failures);
    CHECK_DONE();
}
