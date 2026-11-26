/*
 * The pictures of files of DGX drawn (test_picture_files.py makes the files, the C and pf_expected.h): the page made C,
 * added, and the same page loaded on the tables made C of its names file, each drawn on a screen of 16 bits; every
 * pixel of every picture, where its look puts it in its box, the colour of its PNG.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_load.h"
#include "dgx_ui_paint.h"
#include "dgx_vscreen.h"
#include "pf.h"
#include "pf_expected.h"
#include "pft.h"

enum { W = 64, H = 360 };

static uint8_t buffer[W * 2 * 16];

/* the page drawn whole on a screen; every picture's pixels against its PNG's */
static int off_pictures(dgx_ui_t *ui, const char *how)
{
    dgx_screen_t  *s     = dgx_vscreen_init(W, H, 16, DgxScreenRGB);
    dgx_ui_paint_t paint = {.scr = s, .colors = pf_table, .color_count = (int)(sizeof pf_table / sizeof *pf_table), .buffer = buffer, .buffer_size = sizeof buffer};
    dgx_ui_paint(ui, &paint, 0);
    int off = 0;
    for (int k = 0; k < PF_PICTURES; ++k) {
        const dgx_ui_node_t *n = dgx_ui_node(ui, k + 1);
        const int            w = pf_pictures[k].w * pf_pictures[k].scale, h = pf_pictures[k].h * pf_pictures[k].scale;
        const int            x0 = n->x + (n->w - w) / 2, y0 = n->y + (n->h - h) / 2;
        int                  wrong = 0;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const uint32_t want = pf_pictures[k].px[(y / pf_pictures[k].scale) * pf_pictures[k].w + x / pf_pictures[k].scale];
                wrong += dgx_get_pixel(s, x0 + x, y0 + y) != want;
            }
        if (wrong) fprintf(stderr, "  %s: picture %s, %d of %d pixels off\n", how, pf_pictures[k].name, wrong, w * h);
        off += wrong;
    }
    dgx_ui_paint_free(&paint);
    dgx_screen_destroy(&s);
    return off;
}

int main(void)
{
    static dgx_ui_node_t nodes[16];
    dgx_ui_t             ui;
    /* the page made C, added */
    dgx_ui_init(&ui, nodes, 16, 1);
    dgx_ui_set_root(&ui, 0, 0, W, H);
    CHECK(pf_add(&ui, -1) == 0);
    CHECK(off_pictures(&ui, "added") == 0);
    dgx_ui_free(&ui);
    /* the same loaded on the tables of the names file, which read the files at the load's start */
    dgx_ui_load_need_t  need;
    dgx_ui_load_error_t err;
    CHECK(dgx_ui_load_need(pf_page, strlen(pf_page), &pft_names, &need, &err));
    void *memory = calloc(1, need.bytes ? need.bytes : 1);
    dgx_ui_init(&ui, nodes, 16, 1);
    dgx_ui_set_root(&ui, 0, 0, W, H);
    CHECK(dgx_ui_load(&ui, -1, pf_page, strlen(pf_page), &pft_names, memory, need.bytes, &err) == 0);
    CHECK(off_pictures(&ui, "loaded") == 0);
    dgx_ui_free(&ui);
    free(memory);
    CHECK_DONE();
}
