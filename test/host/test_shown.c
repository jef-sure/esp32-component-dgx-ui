/*
 * Where a tree is, told to its root: DGX_UI_EVENT_SHOWN when it comes to the screen, DGX_UI_EVENT_COVERED when it leaves it, in
 * pairs. A page over home and back by the key; a page shown again when the page over it closes; two pages opened in
 * one go; a dialog, a popup closed by a finger outside and a dropdown's list, which cover nothing; a page over a
 * dialog over home; a chain closed from home; a page opened over home while one is open there; a popup opened again
 * elsewhere; an on(SHOWN) that opens a page, an on(COVERED) that closes the page that covers it, an on(COVERED) of a
 * popup that goes back a page while dgx_ui_page_open() closes it; a page on the heap freed in its on(COVERED); a tree
 * freed with a page over it; an open refused; a root with no on_event(), and one removed; a page opened, closed and opened
 * over another tree; one dialog over two pages in turn; transitions, told at their start, an inner one from an on_event()
 * changing the pages at once under the outer one's pictures. Each case's log of what each root was told, and after
 * each: every tree's last word what a walk of the chain says, and each tree's words alternating.
 */
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_draw.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_transition.h"
#include "dgx_vscreen.h"

enum { TREES = 7 };
enum { HOME, P1, P2, DIALOG, LIST, P3, HEAP };
static const char *const names[TREES] = {"home", "P1", "P2", "D", "L", "P3", "H"};

static dgx_ui_t  home, p1, p2, dialog, list, p3;
static dgx_ui_t *heap; /* a page whose struct is on the heap */
static dgx_ui_t *trees[TREES];

/* the log, and each tree's last word: true shown */
static char log_text[400];
static bool shown_last[TREES];
static int  out_of_turn; /* a SHOWN to a tree shown, a COVERED to one covered */

/* what a root's on_event() does besides logging: set by a case */
static void (*then)(int tree, int what);

static void on_root(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    const int tree = (int)(intptr_t)ctx;
    if (what != DGX_UI_EVENT_SHOWN && what != DGX_UI_EVENT_COVERED && what != DGX_UI_EVENT_BACK) return;
    char word[16];
    snprintf(word, sizeof word, "%s%s ", names[tree], what == DGX_UI_EVENT_SHOWN ? "+" : what == DGX_UI_EVENT_COVERED ? "-" : "!");
    strncat(log_text, word, sizeof log_text - strlen(log_text) - 1);
    if (what == DGX_UI_EVENT_SHOWN) out_of_turn += shown_last[tree], shown_last[tree] = true;
    if (what == DGX_UI_EVENT_COVERED) out_of_turn += !shown_last[tree], shown_last[tree] = false;
    if (then) then(tree, what);
}

static dgx_ui_node_t         nodes[TREES][2];
static const dgx_ui_widget_t home_root_w = {.kind = DGX_UI_BOX, .on_event = on_root, .ctx = (void *)(intptr_t)HOME};
static dgx_ui_widget_t       root_w[TREES];

static void make(dgx_ui_t *ui, int tree)
{
    dgx_ui_init(ui, nodes[tree], 2, 1);
    root_w[tree] = (dgx_ui_widget_t){.kind = DGX_UI_FRAME, .look = &dgx_ui_look_frame, .on_event = on_root, .ctx = (void *)(intptr_t)tree};
    dgx_ui_add(ui, -1, tree == HOME ? &home_root_w : &root_w[tree]);
    dgx_ui_screen(ui, 96, 64, 0);
}

/* everything closed and made anew, the log empty: home shown, the rest never shown */
static void start(void)
{
    if (home.popup) dgx_ui_popup_close(&home);
    then = NULL;
    make(&home, HOME), make(&p1, P1), make(&p2, P2), make(&dialog, DIALOG), make(&list, LIST), make(&p3, P3);
    trees[HOME] = &home, trees[P1] = &p1, trees[P2] = &p2, trees[DIALOG] = &dialog, trees[LIST] = &list, trees[P3] = &p3, trees[HEAP] = NULL;
    memset(shown_last, 0, sizeof shown_last), shown_last[HOME] = true;
    log_text[0] = 0, out_of_turn = 0;
}

/* the log so far, and anew */
static bool logged(const char *expected)
{
    const bool same = strcmp(log_text, expected) == 0;
    if (!same) fprintf(stderr, "  logged \"%s\", expected \"%s\"\n", log_text, expected);
    log_text[0] = 0;
    return same;
}

/* each tree's last word what the chain from home says: shown while no page lies above it; a tree out of the chain
   covered */
static bool as_the_chain_says(void)
{
    bool in_chain[TREES] = {false}, shown_now[TREES] = {false};
    dgx_ui_t *chain[TREES + 1];
    int       count = 0;
    for (dgx_ui_t *tree = &home; tree && count <= TREES; tree = tree->popup) chain[count++] = tree;
    bool covered = false;
    for (int k = count - 1; k >= 0; --k) {
        covered |= chain[k]->popup && (chain[k]->popup_flags & DGX_UI_POPUP_SCREEN);
        for (int t = 0; t < TREES; ++t)
            if (trees[t] == chain[k]) in_chain[t] = true, shown_now[t] = !covered;
    }
    int off = 0;
    for (int t = 0; t < TREES; ++t)
        if (trees[t] && (in_chain[t] ? shown_now[t] : false) != shown_last[t]) off += 1, fprintf(stderr, "  %s told %s\n", names[t], shown_last[t] ? "shown" : "covered");
    return off == 0 && out_of_turn == 0;
}

static void open_dialog(dgx_ui_t *under, dgx_ui_t *popup)
{
    CHECK(dgx_ui_popup_open(under, popup, 10, 10, 40, 30, DGX_UI_POPUP_MODAL));
}

/* the actions of some cases, from an on_event() */
static void home_opens_p2_when_shown(int tree, int what)
{
    if (tree == HOME && what == DGX_UI_EVENT_SHOWN) then = NULL, dgx_ui_page_open(&home, &p2);
}
static void home_closes_its_cover(int tree, int what)
{
    if (tree == HOME && what == DGX_UI_EVENT_COVERED) then = NULL, dgx_ui_popup_close(&home);
}
static void list_goes_back_a_page(int tree, int what)
{
    if (tree == LIST && what == DGX_UI_EVENT_COVERED) then = NULL, dgx_ui_page_back(&home);
}
static void home_reopens_p1_when_shown(int tree, int what)
{
    if (tree == HOME && what == DGX_UI_EVENT_SHOWN) then = NULL, dgx_ui_page_open(&home, &p1);
}
static void p1_reopens_itself_when_covered(int tree, int what)
{
    if (tree == P1 && what == DGX_UI_EVENT_COVERED) then = NULL, dgx_ui_page_open(&home, &p1);
}
static void heap_frees_itself(int tree, int what)
{
    if (tree == HEAP && what == DGX_UI_EVENT_COVERED) {
        then = NULL;
        dgx_ui_free(heap), free(heap);
        heap = NULL, trees[HEAP] = NULL;
    }
}

/* the transitions' pictures and the paint */
static dgx_screen_t       *screen, *from_seen, *to_seen;
static dgx_ui_paint_t      paint;
static dgx_ui_transition_t tr;
static uint32_t            colors[4] = {0x0000, 0xffff, 0x2104, 0xf800};
static void                p1_backs_by_when_shown(int tree, int what)
{
    if (tree == P1 && what == DGX_UI_EVENT_SHOWN) then = NULL, dgx_ui_page_back_by(&home, &paint, &tr, DGX_UI_SLIDE_RIGHT);
}
static void p1_goes_back_when_shown(int tree, int what)
{
    if (tree == P1 && what == DGX_UI_EVENT_SHOWN) then = NULL, dgx_ui_page_back(&home);
}
static void p1_opens_by_when_shown(int tree, int what)
{
    if (tree == P1 && what == DGX_UI_EVENT_SHOWN) then = NULL, dgx_ui_page_open_by(&home, &p2, &paint, &tr, DGX_UI_SLIDE_LEFT);
}

/* the screen as the trees stand, composed into a picture */
static void composed(dgx_screen_t *picture)
{
    dgx_ui_paint_t fresh = {.scr = picture, .colors = colors, .color_count = 4};
    dgx_fill_rectangle(picture, 0, 0, 96, 64, 0x1234);
    dgx_ui_paint_area_to(&home, &fresh, picture, NULL, 0, 0, 96, 64);
    dgx_ui_paint_free(&fresh);
}
static int pictures_differ(dgx_screen_t *a, dgx_screen_t *b)
{
    int count = 0;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 96; ++x) count += dgx_get_pixel(a, x, y) != dgx_get_pixel(b, x, y);
    return count;
}

/* home's chain whole: each popup's level one more than its under's */
static bool home_level_ok(void)
{
    for (const dgx_ui_t *tree = &home; tree->popup; tree = tree->popup)
        if (tree->popup->under != tree || tree->popup->level != tree->level + 1) return false;
    return true;
}

int main(void)
{
    /* a page over home, a second over it, back by the key twice */
    start();
    CHECK(dgx_ui_page_open(&home, &p1) && logged("home- P1+ ") && as_the_chain_says());
    CHECK(dgx_ui_page_open(&home, &p2) && logged("P1- P2+ ") && as_the_chain_says());
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_BACK) && logged("P2! P2- P1+ ") && as_the_chain_says());
    CHECK(dgx_ui_key(&home, DGX_UI_KEY_BACK) && logged("P1! P1- home+ ") && as_the_chain_says());

    /* two pages opened in one go, before any pass: told at the calls, the first one's pair too */
    start();
    dgx_ui_page_open(&home, &p1), dgx_ui_page_open(&home, &p2);
    CHECK(logged("home- P1+ P1- P2+ ") && as_the_chain_says());

    /* a dialog over a page covers nothing; a page over the dialog covers both, the top first */
    start();
    dgx_ui_page_open(&home, &p1);
    log_text[0] = 0;
    open_dialog(&p1, &dialog);
    CHECK(logged("D+ ") && as_the_chain_says());
    CHECK(dgx_ui_page_open(&home, &p2) && logged("D- P1- P2+ ") && as_the_chain_says());
    /* the chain closed from home: only the top was shown */
    dgx_ui_popup_close(&home);
    CHECK(logged("P2- home+ ") && as_the_chain_says());

    /* a page over a dialog over home: the dialog and home covered, the top first */
    start();
    open_dialog(&home, &dialog);
    CHECK(logged("D+ ") && dgx_ui_page_open(&home, &p1) && logged("D- home- P1+ ") && as_the_chain_says());
    dgx_ui_page_back(&home);
    CHECK(logged("P1! P1- home+ D+ ") && as_the_chain_says());

    /* a popup closed by a finger outside over a page: dgx_ui_page_open() closes it first; the page under it was seen */
    start();
    dgx_ui_page_open(&home, &p1);
    CHECK(dgx_ui_popup_open(&p1, &list, 0, 0, 30, 30, DGX_UI_POPUP_CLOSE_OUTSIDE) && logged("home- P1+ L+ "));
    CHECK(dgx_ui_page_open(&home, &p2) && logged("L- P1- P2+ ") && as_the_chain_says());

    /* a page opened over home by dgx_ui_popup_open() while one is open there: the one closed, home nothing */
    start();
    dgx_ui_page_open(&home, &p1);
    log_text[0] = 0;
    CHECK(dgx_ui_popup_open(&home, &p2, 0, 0, 0, 0, DGX_UI_POPUP_MODAL | DGX_UI_POPUP_BACK | DGX_UI_POPUP_SCREEN));
    CHECK(logged("P1- P2+ ") && as_the_chain_says());

    /* a dialog opened again elsewhere: as it was, nothing told; with a page over it, the page goes */
    start();
    open_dialog(&home, &dialog);
    log_text[0] = 0;
    CHECK(dgx_ui_popup_open(&home, &dialog, 20, 20, 40, 30, DGX_UI_POPUP_MODAL) && logged("") && as_the_chain_says());
    dgx_ui_page_open(&home, &p1);
    log_text[0] = 0;
    CHECK(dgx_ui_popup_open(&home, &dialog, 5, 5, 40, 30, DGX_UI_POPUP_MODAL) && logged("P1- home+ D+ ") && as_the_chain_says());

    /* an on(SHOWN) that opens a page: every tree told as it stands after */
    start();
    dgx_ui_page_open(&home, &p1);
    log_text[0] = 0;
    then = home_opens_p2_when_shown;
    dgx_ui_page_back(&home);
    CHECK(logged("P1! P1- home+ home- P2+ ") && as_the_chain_says());

    /* an on(COVERED) that closes the page that covers it: the page never told */
    start();
    then = home_closes_its_cover;
    dgx_ui_page_open(&home, &p1);
    CHECK(logged("home- home+ ") && !home.popup && as_the_chain_says());

    /* a popup's on(COVERED) that goes back a page while dgx_ui_page_open() closes it: the new page over home */
    start();
    dgx_ui_page_open(&home, &p1);
    dgx_ui_popup_open(&p1, &list, 0, 0, 30, 30, DGX_UI_POPUP_CLOSE_OUTSIDE);
    log_text[0] = 0;
    then = list_goes_back_a_page;
    CHECK(dgx_ui_page_open(&home, &p2) && home.popup == &p2 && !p1.under && as_the_chain_says());
    CHECK(logged("L- P1! P1- home+ home- P2+ "));

    /* a page on the heap, freed in its on(COVERED) */
    start();
    heap = malloc(sizeof *heap), trees[HEAP] = heap;
    make(heap, HEAP);
    dgx_ui_page_open(&home, heap);
    CHECK(logged("home- H+ "));
    then = heap_frees_itself;
    dgx_ui_page_back(&home);
    CHECK(!heap && logged("H! H- home+ ") && as_the_chain_says());

    /* a tree freed while it is open as a page, and home freed with a page over it */
    start();
    dgx_ui_page_open(&home, &p1);
    log_text[0] = 0;
    dgx_ui_free(&p1);
    make(&p1, P1);
    CHECK(logged("P1- home+ ") && !home.popup && as_the_chain_says());
    dgx_ui_page_open(&home, &p2);
    log_text[0] = 0;
    dgx_ui_free(&home);
    CHECK(logged("P2- ") && !p2.under); /* home, being freed, told nothing */
    make(&home, HOME);

    /* a page freed, home's on(SHOWN) opening it again: told after the page is wiped, so the page lies in the chain as
       any page opened does; home freed with a page over it: the page COVERED, home nothing */
    start();
    dgx_ui_page_open(&home, &p1);
    log_text[0] = 0;
    then = home_reopens_p1_when_shown;
    dgx_ui_free(&p1);
    CHECK(logged("P1- home+ home- ") && home.popup == &p1 && p1.under == &home && p1.level == 1 && home_level_ok());
    dgx_ui_page_back(&home);
    CHECK(!home.popup && !p1.under && logged("home+ "));
    make(&p1, P1);
    start();
    dgx_ui_page_open(&home, &p2);
    log_text[0] = 0;
    dgx_ui_free(&home);
    CHECK(logged("P2- ") && !p2.under && !p2.level && !home.popup);
    make(&home, HOME);
    /* a page freed whose own on(COVERED) opens it again: closed again before it is wiped, in no chain after */
    start();
    dgx_ui_page_open(&home, &p1);
    log_text[0] = 0;
    then = p1_reopens_itself_when_covered;
    dgx_ui_free(&p1);
    CHECK(logged("P1- P1+ home+ ") && !home.popup && !p1.under && !p1.level);
    make(&p1, P1);

    /* an open refused, the popup open already: nothing told */
    start();
    dgx_ui_page_open(&home, &p1);
    log_text[0] = 0;
    CHECK(!dgx_ui_popup_open(&dialog, &p1, 0, 0, 10, 10, 0) && logged("") && as_the_chain_says());

    /* a root with no on_event(), and a root removed while shown: nothing told, the rest as it stands */
    start();
    static const dgx_ui_widget_t mute_w = {.kind = DGX_UI_FRAME, .look = &dgx_ui_look_frame};
    dgx_ui_init(&p3, nodes[P3], 2, 1), dgx_ui_add(&p3, -1, &mute_w), dgx_ui_screen(&p3, 96, 64, 0);
    trees[P3] = NULL;
    dgx_ui_page_open(&home, &p3);
    CHECK(logged("home- "));
    dgx_ui_page_back(&home);
    CHECK(logged("home+ ") && as_the_chain_says());
    dgx_ui_page_open(&home, &p1);
    dgx_ui_remove(&p1, 0);
    log_text[0] = 0;
    dgx_ui_page_back(&home);
    CHECK(logged("home+ "));
    make(&p1, P1);

    /* a page opened, closed and opened over another tree; one dialog over two pages in turn */
    start();
    dgx_ui_page_open(&home, &p1), dgx_ui_page_back(&home);
    open_dialog(&home, &dialog);
    log_text[0] = 0;
    dgx_ui_page_open(&home, &p1);
    CHECK(p1.under == &dialog && logged("D- home- P1+ ") && as_the_chain_says());
    start();
    dgx_ui_page_open(&home, &p1);
    open_dialog(&p1, &dialog);
    dgx_ui_popup_close(&p1);
    dgx_ui_page_open(&home, &p2);
    open_dialog(&p2, &dialog);
    CHECK(dialog.under == &p2 && logged("home- P1+ D+ D- P1- P2+ D+ ") && as_the_chain_says());

    /* transitions: told at the start, nothing at the frames; one asked from an on_event() within another's setting up changes
       the pages at once, and the outer one slides from what the display showed to what the trees are at the end */
    screen    = dgx_vscreen_init(96, 64, 16, DgxScreenRGB);
    from_seen = dgx_vscreen_init(96, 64, 16, DgxScreenRGB), to_seen = dgx_vscreen_init(96, 64, 16, DgxScreenRGB);
    tr        = (dgx_ui_transition_t){.from = dgx_vscreen_init(96, 64, 16, DgxScreenRGB), .to = dgx_vscreen_init(96, 64, 16, DgxScreenRGB), .ms = 100};
    paint     = (dgx_ui_paint_t){.scr = screen, .colors = colors, .color_count = 4};
    {
        start();
        dgx_ui_paint(&home, &paint, 0);
        CHECK(dgx_ui_page_open_by(&home, &p1, &paint, &tr, DGX_UI_SLIDE_LEFT) && logged("home- P1+ "));
        for (uint32_t ms = 10; tr.running; ms += 30) dgx_ui_transition_frame(&tr, &paint, ms);
        CHECK(logged("") && as_the_chain_says());
        dgx_ui_page_back_by(&home, &paint, &tr, DGX_UI_SLIDE_RIGHT);
        CHECK(logged("P1! P1- home+ "));
        for (uint32_t ms = 400; tr.running; ms += 30) dgx_ui_transition_frame(&tr, &paint, ms);
    }
    /* (a) P2 over P1 over home; back by a transition closes P2, and P1's on(SHOWN) goes back by one as well: both
       closed at once, one slide from P2's picture to home's */
    {
        start();
        dgx_ui_page_open(&home, &p1), dgx_ui_page_open(&home, &p2);
        dgx_ui_paint(&home, &paint, 0);
        composed(from_seen);
        log_text[0] = 0;
        then = p1_backs_by_when_shown;
        CHECK(dgx_ui_page_back_by(&home, &paint, &tr, DGX_UI_SLIDE_RIGHT) && !home.popup && tr.running);
        CHECK(logged("P2! P2- P1+ P1! P1- home+ ") && as_the_chain_says());
        composed(to_seen);
        CHECK(pictures_differ(tr.from, from_seen) == 0 && pictures_differ(tr.to, to_seen) == 0);
        for (uint32_t ms = 10; tr.running; ms += 30) dgx_ui_transition_frame(&tr, &paint, ms);
    }
    /* (b) a page's on(SHOWN), told within an open by a transition, opens another by one: one slide from home's picture
       to the top page's */
    {
        start();
        dgx_ui_paint(&home, &paint, 0);
        composed(from_seen);
        then = p1_opens_by_when_shown;
        CHECK(dgx_ui_page_open_by(&home, &p1, &paint, &tr, DGX_UI_SLIDE_LEFT) && home.popup == &p1 && p1.popup == &p2 && tr.running);
        CHECK(logged("home- P1+ P1- P2+ ") && as_the_chain_says());
        composed(to_seen);
        CHECK(pictures_differ(tr.from, from_seen) == 0 && pictures_differ(tr.to, to_seen) == 0);
        for (uint32_t ms = 10; tr.running; ms += 30) dgx_ui_transition_frame(&tr, &paint, ms);
    }
    /* a page's on(SHOWN), told within an open by a transition, goes back at once: no slide, home on top */
    {
        start();
        dgx_ui_paint(&home, &paint, 0);
        then = p1_goes_back_when_shown;
        CHECK(dgx_ui_page_open_by(&home, &p1, &paint, &tr, DGX_UI_SLIDE_LEFT) && !home.popup && !tr.running);
        CHECK(logged("home- P1+ P1! P1- home+ ") && as_the_chain_says());
    }
    dgx_ui_popup_close(&home);
    dgx_ui_paint_free(&paint);
    dgx_screen_destroy(&screen), dgx_screen_destroy(&from_seen), dgx_screen_destroy(&to_seen);
    dgx_screen_destroy(&tr.from), dgx_screen_destroy(&tr.to);
    CHECK_DONE();
}
