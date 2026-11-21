/*
 * The stack a load takes: dgx_ui_load_need() and dgx_ui_load() of trees 1, 2, 8 and 24 deep, and of a value of an
 * editor's object 2 and 50 brackets deep, each run in a thread on a stack of its own filled with a pattern, the bytes
 * it touched below the thread's frame counted by the thread itself: the stack under its frame filled again before the
 * load, as the thread's start goes deeper than a load, and counted before its end, which goes deeper too. The walk
 * keeps the widgets whose children it walks in the load and the reader counts the brackets it passes, so from a depth
 * of 2 the bytes are the same, a depth of 1 takes no more, a value 50 brackets deep no more than one of 2; and a load
 * takes about what the need takes, one walk's place on the stack at a time. Built with no sanitizer, whose frames would
 * blur the count.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "dgx_ui_load.h"

enum { STACK_BYTES = 256 * 1024, FILL = 0xa5, NOTHING = 0, NEED, LOAD };

static uint8_t                   *stack_memory;
static size_t                     lowest_touched;
static int                        what, result;
static char                       text[16384];
static size_t                     text_len;
static const dgx_ui_load_names_t  names = {0};
static uint8_t                    memory[16384] __attribute__((aligned(16)));
static dgx_ui_node_t              nodes[32];
static dgx_ui_t                   ui;
static dgx_ui_load_error_t        err;

static void *measured(void *unused)
{
    /* below the thread's own frame the stack is free: filled again, as the thread's start has gone deeper than a load */
    volatile uint8_t   frame_mark = 0;
    const size_t       below      = (size_t)((const uint8_t *)&frame_mark - stack_memory) - 512;
    dgx_ui_load_need_t need;
    memset(stack_memory, FILL, below);
    if (what == NEED) result = dgx_ui_load_need(text, text_len, &names, &need, &err);
    else if (what == LOAD) result = dgx_ui_load(&ui, -1, text, text_len, &names, memory, sizeof memory, &err) >= 0;
    /* counted here, before the thread's end, which goes deeper than a load into its stack */
    size_t untouched = 0;
    while (untouched < below && stack_memory[untouched] == FILL) ++untouched;
    lowest_touched = untouched + (STACK_BYTES - below); /* so that the bytes are counted from the mark's place */
    return (void *)(uintptr_t)frame_mark;
}

/* the bytes of its own stack a thread touched doing what */
static size_t touched(int doing)
{
    what = doing, result = 0;
    if (doing == LOAD) dgx_ui_init(&ui, nodes, 32, 1);
    memset(stack_memory, FILL, STACK_BYTES);
    pthread_attr_t attributes;
    pthread_t      thread;
    pthread_attr_init(&attributes);
    pthread_attr_setstack(&attributes, stack_memory, STACK_BYTES);
    pthread_create(&thread, &attributes, measured, NULL);
    pthread_join(thread, NULL);
    pthread_attr_destroy(&attributes);
    if (doing == LOAD) dgx_ui_free(&ui);
    return STACK_BYTES - lowest_touched;
}

/* boxes in one another, depth deep, their children after their other keys */
static void boxes(int depth)
{
    char *at = text + sprintf(text, "{\"dgx-ui\": 1, \"tree\": ");
    for (int level = 1; level < depth; ++level) at += sprintf(at, "{\"kind\": \"box\", \"flags\": [\"column\"], \"pad\": 1, \"children\": [");
    at += sprintf(at, "{\"kind\": \"box\"}");
    for (int level = 1; level < depth; ++level) at += sprintf(at, "]}");
    at += sprintf(at, "}");
    text_len = (size_t)(at - text);
}

/* a box, and an editor's object whose value stands inside so many brackets with the top's */
static void editor(int brackets)
{
    char *at = text + sprintf(text, "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\"}, \"editor\": {\"a\": ");
    for (int k = 0; k < brackets - 2; ++k) *at++ = '[';
    *at++ = '1';
    for (int k = 0; k < brackets - 2; ++k) *at++ = ']';
    at += sprintf(at, "}}");
    text_len = (size_t)(at - text);
}

int main(void)
{
    stack_memory         = malloc(STACK_BYTES);
    const size_t nothing = touched(NOTHING);
    static const int depths[] = {1, 2, 8, 24};
    size_t           need_bytes[4], load_bytes[4];
    for (int k = 0; k < 4; ++k) {
        boxes(depths[k]);
        need_bytes[k] = touched(NEED) - nothing;
        CHECK(result == 1);
        load_bytes[k] = touched(LOAD) - nothing;
        CHECK(result == 1);
        printf("test_stack.c: a depth of %2d: need %zu, load %zu bytes\n", depths[k], need_bytes[k], load_bytes[k]);
    }
    CHECK(need_bytes[2] == need_bytes[1] && need_bytes[3] == need_bytes[1] && need_bytes[0] <= need_bytes[1]);
    CHECK(load_bytes[2] == load_bytes[1] && load_bytes[3] == load_bytes[1] && load_bytes[0] <= load_bytes[1]);
    /* one walk's place at a time: a load's own laying and adding over the need, not a second load on the stack */
    CHECK(load_bytes[3] < need_bytes[3] + sizeof(dgx_ui_load_need_t) + 256);
    size_t editor_bytes[2];
    for (int k = 0; k < 2; ++k) {
        editor(k ? 50 : 2);
        editor_bytes[k] = touched(NEED) - nothing;
        CHECK(result == 1);
        printf("test_stack.c: a value of an editor's object inside %d brackets: need %zu bytes\n", k ? 50 : 2, editor_bytes[k]);
    }
    CHECK(editor_bytes[1] == editor_bytes[0]);
    free(stack_memory);
    CHECK_DONE();
}
