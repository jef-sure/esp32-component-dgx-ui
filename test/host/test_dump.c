/*
 * dgx_ui_dump(): a tree in the log, read back from the log of the computer. A line a widget, indented by its depth,
 * its number, kind, place, value, state, marks, tag and the start of its text, its line ends written \n and cut
 * between letters; a popup over it, headed and indented; the nodes in use and the slots; a tree that outgrew its
 * array told what to give it; a widget removed, a slot left. Nothing in the tree changed.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "check.h"
#include "dgx_ui.h"
#include "dgx_ui_text.h"

static char out[16384];
static int  count;
static char lines[160][200];

/* the log of a dump, line by line */
static void dump(const dgx_ui_t *ui)
{
    fflush(stderr);
    FILE     *f     = tmpfile();
    const int saved = dup(2);
    dup2(fileno(f), 2);
    dgx_ui_dump(ui);
    fflush(stderr);
    dup2(saved, 2), close(saved);
    rewind(f);
    const size_t n = fread(out, 1, sizeof out - 1, f);
    out[n]         = 0;
    fclose(f);
    count = 0;
    for (char *s = out, *e; *s && count < 160; s = e + 1) {
        e = strchr(s, '\n');
        if (!e) break;
        snprintf(lines[count++], sizeof lines[0], "%.*s", (int)(e - s), s);
    }
}

static bool starts(int k, const char *s) { return k < count && strncmp(lines[k], s, strlen(s)) == 0; }
static bool ends(int k, const char *s)
{
    const size_t a = k < count ? strlen(lines[k]) : 0, b = strlen(s);
    return k < count && a >= b && strcmp(lines[k] + a - b, s) == 0;
}
static bool has(int k, const char *s) { return k < count && strstr(lines[k], s); }

static char             bufs[6][40];
static dgx_ui_textbox_t boxes[6];

int main(void)
{
    static const dgx_ui_widget_t root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 2, .gap = 2};
    for (int k = 0; k < 6; ++k) dgx_ui_textbox_init(&boxes[k], bufs[k], sizeof bufs[k], 1, NULL);
    const dgx_ui_widget_t label_w  = {.kind = DGX_UI_LABEL, .text = &boxes[0], .want_h = 13};
    static int32_t list_w_place; /* its value */
    const dgx_ui_widget_t list_w   = {.kind = DGX_UI_LIST, .value = &list_w_place, .text = &boxes[1], .max = 11, .want_h = 52};
    const dgx_ui_widget_t button_w = {.kind = DGX_UI_BUTTON, .text = &boxes[2], .want_w = 60, .want_h = 20, .tag = 7};
    const dgx_ui_widget_t hidden_w = {.kind = DGX_UI_LABEL, .text = &boxes[3], .want_h = 13, .state = {.bits = DGX_UI_HIDDEN}};
    const dgx_ui_widget_t utf_w    = {.kind = DGX_UI_LABEL, .text = &boxes[4], .want_h = 13};
    static dgx_ui_node_t  nodes[8];
    static dgx_ui_t       ui;
    dgx_ui_init(&ui, nodes, 8, 1);
    dgx_ui_add(&ui, -1, &root_w);
    const int l = dgx_ui_add(&ui, 0, &label_w), li = dgx_ui_add(&ui, 0, &list_w), b = dgx_ui_add(&ui, 0, &button_w);
    const int h = dgx_ui_add(&ui, 0, &hidden_w), u = dgx_ui_add(&ui, 0, &utf_w);
    dgx_ui_text_set(&ui, l, "21.5 C");
    dgx_ui_text_set(&ui, li, "Jan\nFeb\nMar\nApr\nMay\nJun");
    dgx_ui_text_set(&ui, b, "OK");
    dgx_ui_text_set(&ui, h, "hidden");
    dgx_ui_text_set(&ui, u, "Ёлки-палки, лес густой");
    dgx_ui_set_value(&ui, li, 5);
    dgx_ui_focus(&ui, li);
    dgx_ui_screen(&ui, 240, 320, 0);
    dgx_ui_layout(&ui);
    const dgx_ui_node_t before = *dgx_ui_node(&ui, li);

    dump(&ui);
    CHECK(count == 7);
    CHECK(starts(0, "I dgx_ui: #0 BOX 0,0 240x320 v=0 bits=- marks="));
    CHECK(starts(1, "I dgx_ui:   #1 LABEL 2,2 236x13 v=0 bits=- marks=") && ends(1, " tag=0 \"21.5 C\""));
    CHECK(starts(2, "I dgx_ui:   #2 LIST 2,17 236x52 v=5 bits=FOCUSED marks=") && ends(2, " tag=0 \"Jan\\nFeb\\nMar\\nApr\\n...\""));
    CHECK(starts(3, "I dgx_ui:   #3 BUTTON 2,71 60x20 v=0 bits=- marks=") && ends(3, " tag=7 \"OK\""));
    CHECK(starts(4, "I dgx_ui:   #4 LABEL") && has(4, " bits=HIDDEN ") && ends(4, "\"hidden\""));
    CHECK(starts(5, "I dgx_ui:   #5 LABEL") && ends(5, "\"Ёлки-пал...\"")); /* 15 bytes: the 16th begins a letter */
    CHECK(strcmp(lines[6], "I dgx_ui: nodes: 6 in use, 6 of the array of 8, blocks: 0") == 0);
    CHECK(memcmp(&before, dgx_ui_node(&ui, li), sizeof before) == 0); /* nothing changed */

    /* a popup over it: headed, its widgets a level in, its own nodes */
    {
        static const dgx_ui_widget_t frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2};
        const dgx_ui_widget_t        ask_w   = {.kind = DGX_UI_LABEL, .text = &boxes[5], .want_h = 13};
        static dgx_ui_node_t         pnodes[2];
        static dgx_ui_t              pop;
        dgx_ui_init(&pop, pnodes, 2, 1);
        dgx_ui_add(&pop, -1, &frame_w);
        dgx_ui_text_set(&pop, dgx_ui_add(&pop, 0, &ask_w), "Really?");
        CHECK(dgx_ui_popup_open(&ui, &pop, 10, 20, 100, 50, 0));
        dgx_ui_layout(&pop);
        dump(&ui);
        CHECK(count == 11);
        CHECK(strcmp(lines[7], "I dgx_ui: popup 1 at 10,20 100x50") == 0);
        CHECK(starts(8, "I dgx_ui:   #0 FRAME 10,20 100x50 v=0 bits=- marks="));
        CHECK(starts(9, "I dgx_ui:     #1 LABEL 12,22 96x13 v=0 bits=- marks=") && ends(9, "\"Really?\""));
        CHECK(strcmp(lines[10], "I dgx_ui:   nodes: 2 in use, 2 of the array of 2, blocks: 0") == 0);
        dgx_ui_popup_close(&ui);
        dgx_ui_free(&pop);
    }
    dgx_ui_free(&ui);

    /* a tree of 70 widgets in an array of 4: two blocks, and what to give the array; one removed, a slot left */
    {
        static const dgx_ui_widget_t leaf_w = {.kind = DGX_UI_BOX, .want_h = 2};
        static dgx_ui_node_t         few[4];
        static dgx_ui_t              big;
        dgx_ui_init(&big, few, 4, 1);
        dgx_ui_add(&big, -1, &root_w);
        for (int k = 0; k < 70; ++k) dgx_ui_add(&big, 0, &leaf_w);
        dump(&big);
        CHECK(count == 73 && strcmp(lines[71], "I dgx_ui: nodes: 71 in use, 71 of the array of 4, blocks: 2") == 0);
        CHECK(strcmp(lines[72], "I dgx_ui: give the array 71 nodes and no block is needed") == 0);
        dgx_ui_remove(&big, 30);
        dump(&big);
        CHECK(strcmp(lines[70], "I dgx_ui: nodes: 70 in use, 71 of the array of 4, blocks: 2") == 0);
        dgx_ui_free(&big);
    }
    CHECK_DONE();
}
