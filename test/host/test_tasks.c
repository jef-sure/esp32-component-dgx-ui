/*
 * Two trees worked by two tasks, as two displays are: what one call of the library guards for its tree is the tree's
 * own. The key back closing a page over each tree at once, the second task's page told DGX_UI_EVENT_BACK while the first
 * task's root is being told: each root told once, both pages closed, and the first tree's page told again when it is
 * opened again and closed. A back by a transition on one tree while the other's runs: both pages closed.
 */
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_transition.h"
#include "dgx_vscreen.h"

enum { WIDTH = 64, HEIGHT = 48 };
static uint32_t colors[2] = {0x0000, 0xffff};

/* a tree and the page over it, each of a root alone */
typedef struct {
    dgx_ui_node_t nodes[1], page_nodes[1];
    dgx_ui_t      ui, page;
} pair_t;
static pair_t first, second;
static sem_t  first_in, second_in, first_out;
static int    first_told, second_told;

/* a task waits for the other, 2 s at most: a guard shared by the two trees may keep the other from coming */
static void wait_for(sem_t *sem)
{
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += 2;
    sem_timedwait(sem, &until);
}

static void on_first_page(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_EVENT_BACK) return;
    if (++first_told == 1) sem_post(&first_in), wait_for(&second_in); /* the second task told meanwhile */
}
static void on_second_page(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what != DGX_UI_EVENT_BACK) return;
    if (++second_told > 1) return;
    sem_post(&second_in), wait_for(&first_out);
    dgx_ui_popup_back(&second.ui); /* its page closed from its on_event(), as a page may: told once */
}
static const dgx_ui_widget_t root_w        = {.kind = DGX_UI_BOX, .look = &dgx_ui_look_box};
static const dgx_ui_widget_t first_page_w  = {.kind = DGX_UI_BOX, .look = &dgx_ui_look_box, .on_event = on_first_page};
static const dgx_ui_widget_t second_page_w = {.kind = DGX_UI_BOX, .look = &dgx_ui_look_box, .on_event = on_second_page};

static void make(pair_t *pair, const dgx_ui_widget_t *page_w)
{
    memset(pair, 0, sizeof *pair);
    dgx_ui_init(&pair->ui, pair->nodes, 1, 1), dgx_ui_add(&pair->ui, -1, &root_w);
    dgx_ui_init(&pair->page, pair->page_nodes, 1, 1), dgx_ui_add(&pair->page, -1, page_w);
    dgx_ui_screen(&pair->ui, WIDTH, HEIGHT, 0);
    dgx_ui_page_open(&pair->ui, &pair->page);
}

static void *first_task(void *arg)
{
    dgx_ui_key(&first.ui, DGX_UI_KEY_BACK);
    sem_post(&first_out);
    return NULL;
}
static void *second_task(void *arg)
{
    wait_for(&first_in);
    dgx_ui_key(&second.ui, DGX_UI_KEY_BACK);
    return NULL;
}

/* the same with a back by a transition: each task its display, its paint and its pictures */
static dgx_screen_t        *screens[2], *pictures[4];
static dgx_ui_paint_t       paints[2];
static dgx_ui_transition_t  transitions[2];
static bool                 second_closed;
static void                 on_first_slid(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (what == DGX_UI_EVENT_BACK && ++first_told == 1) sem_post(&first_in), wait_for(&second_in);
}
static const dgx_ui_widget_t first_slid_w = {.kind = DGX_UI_BOX, .look = &dgx_ui_look_box, .on_event = on_first_slid};
static void *first_slide(void *arg)
{
    dgx_ui_page_back_by(&first.ui, &paints[0], &transitions[0], DGX_UI_SLIDE_RIGHT);
    return NULL;
}
static void *second_slide(void *arg)
{
    wait_for(&first_in);
    second_closed = dgx_ui_page_back_by(&second.ui, &paints[1], &transitions[1], DGX_UI_SLIDE_RIGHT) && !second.ui.popup;
    sem_post(&second_in);
    return NULL;
}

int main(void)
{
    sem_init(&first_in, 0, 0), sem_init(&second_in, 0, 0), sem_init(&first_out, 0, 0);
    make(&first, &first_page_w), make(&second, &second_page_w);
    pthread_t tasks[2];
    pthread_create(&tasks[0], NULL, first_task, NULL), pthread_create(&tasks[1], NULL, second_task, NULL);
    pthread_join(tasks[0], NULL), pthread_join(tasks[1], NULL);
    CHECK(first_told == 1 && second_told == 1 && !first.ui.popup && !second.ui.popup);
    /* the first tree's page opened again and closed: its root told again */
    dgx_ui_page_open(&first.ui, &first.page);
    dgx_ui_key(&first.ui, DGX_UI_KEY_BACK);
    CHECK(first_told == 2 && !first.ui.popup);
    dgx_ui_free(&first.ui), dgx_ui_free(&first.page), dgx_ui_free(&second.ui), dgx_ui_free(&second.page);

    /* a back by a transition on each display at once */
    first_told = 0;
    make(&first, &first_slid_w), make(&second, &second_page_w);
    second_told = 2; /* its on_event() does nothing more */
    for (int k = 0; k < 2; ++k) {
        screens[k]         = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
        pictures[2 * k]     = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
        pictures[2 * k + 1] = dgx_vscreen_init(WIDTH, HEIGHT, 16, DgxScreenRGB);
        paints[k]           = (dgx_ui_paint_t){.scr = screens[k], .colors = colors, .color_count = 2};
        transitions[k]      = (dgx_ui_transition_t){.ms = 100, .from = pictures[2 * k], .to = pictures[2 * k + 1]};
        dgx_ui_paint(k ? &second.ui : &first.ui, &paints[k], 0);
    }
    pthread_create(&tasks[0], NULL, first_slide, NULL), pthread_create(&tasks[1], NULL, second_slide, NULL);
    pthread_join(tasks[0], NULL), pthread_join(tasks[1], NULL);
    CHECK(first_told == 1 && !first.ui.popup && second_closed);
    for (int k = 0; k < 2; ++k) dgx_ui_paint_free(&paints[k]), screens[k]->destroy(&screens[k]);
    for (int k = 0; k < 4; ++k) pictures[k]->destroy(&pictures[k]);
    dgx_ui_free(&first.ui), dgx_ui_free(&first.page), dgx_ui_free(&second.ui), dgx_ui_free(&second.page);
    CHECK_DONE();
}
