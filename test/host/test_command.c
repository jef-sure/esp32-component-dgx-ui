/*
 * The action command and the button of one line, DGX_UI_BUTTON(): activated by the finger and by the key, a button
 * sends its command to the program's queue (dgx_ui_post() stands in here, a queue of four), and the loop offers the
 * message to the tree after the input call; a take() claims it, or it is the program's; two readers both leave it to
 * the program; a take() that hides the button that sent it, and one that removes it. A button drawn pressed by
 * dgx_ui_set_state() sends nothing. No queue named, and a full one: the command lost, told once. The action named by a
 * description, its ctx a number, an object refused. dgx_ui_enable_tag(): the buttons of a command off and on, the focus
 * and a press lost there, a tag of 0 refused. DGX_UI_LABEL().
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "check.h"
#include "dgx_ui.h"
#include "dgx_ui_load.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_post.h"
#include "fonts/TerminusTTFMedium12.h"

/* the program's queue: what the action sent, in order, QUEUE_ROOM at most */
enum { QUEUE_ROOM = 4 };
static dgx_ui_msg_t queued[QUEUE_ROOM];
static int          queued_count;
static int          queue_place; /* its handle is its address */
#define QUEUE ((QueueHandle_t)&queue_place)

bool dgx_ui_post(QueueHandle_t q, int what, const void *data, size_t size)
{
    if (q != QUEUE || queued_count == QUEUE_ROOM) return false;
    return dgx_ui_msg_value(&queued[queued_count++], what, data, size);
}

/* the program's commands, apart from its other message numbers */
enum { CMD_SAVE = 100, CMD_BACK, CMD_HIDE, CMD_DROP };

static const dgx_ui_font_variant_t variant[1] = {{.font = &TerminusTTFMedium12}};
static const dgx_ui_text_style_t   st         = {.fonts = variant, .count = 1};

/* the takes: what they saw while the input call ran, and since; the reader claims the command `claim`, the two others
   read CMD_BACK and leave it, by the rule of a message for several */
static int  claim = -1, takes_seen, back_readers;
static int  hide_button = -1, drop_button = -1; /* what the take of CMD_HIDE hides and that of CMD_DROP removes */
static bool reader_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    ++takes_seen;
    if (msg->what == CMD_BACK) ++back_readers;
    if (msg->what == CMD_HIDE && hide_button >= 0) dgx_ui_show(ui, hide_button, false);
    if (msg->what == CMD_DROP && drop_button >= 0) dgx_ui_remove(ui, drop_button), drop_button = -1;
    return msg->what == claim;
}
static bool second_reader_take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (msg->what == CMD_BACK) ++back_readers;
    return false;
}

static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 1, .gap = 1};
static const dgx_ui_widget_t save_w   = DGX_UI_BUTTON("Save", &st, CMD_SAVE, .want_w = 40, .want_h = 16);
static const dgx_ui_widget_t save2_w  = DGX_UI_BUTTON("Save too", &st, CMD_SAVE, .want_w = 40, .want_h = 16);
static const dgx_ui_widget_t back_w   = DGX_UI_BUTTON("Back", &st, CMD_BACK, .want_w = 40, .want_h = 16);
static const dgx_ui_widget_t hide_w   = DGX_UI_BUTTON("Hide", &st, CMD_HIDE, .want_w = 40, .want_h = 16);
static const dgx_ui_widget_t drop_w   = DGX_UI_BUTTON("Drop", &st, CMD_DROP, .want_w = 40, .want_h = 16);
static const dgx_ui_widget_t reader_w = DGX_UI_LABEL("Reader", &st, .want_h = 16, .take = reader_take);
static const dgx_ui_widget_t second_w = {.kind = DGX_UI_LABEL, .want_h = 4, .take = second_reader_take};

static dgx_ui_node_t nodes[10];
static dgx_ui_t      ui;
static int           save, save2, back, hide, drop;

/* a click in the middle of a widget: down and up */
static void click(dgx_ui_t *tree, int i, uint32_t ms)
{
    const dgx_ui_node_t *n = dgx_ui_node(tree, i);
    const int16_t        x = (int16_t)(n->x + n->w / 2), y = (int16_t)(n->y + n->h / 2);
    dgx_ui_pointer(tree, &(dgx_ui_pointer_t){.x = x, .y = y, .down = true, .pressed = true, .ms = ms});
    dgx_ui_pointer(tree, &(dgx_ui_pointer_t){.x = x, .y = y, .released = true, .ms = ms + 50});
}

/* the loop: each message of the queue offered to the tree, as the program's loop does after the input call; the
   numbers no widget claimed written to unclaimed[], their count returned */
static int offer(dgx_ui_t *tree, int *unclaimed)
{
    int left = 0;
    for (int k = 0; k < queued_count; ++k) {
        if (!dgx_ui_take(tree, &queued[k])) unclaimed[left++] = queued[k].what;
        dgx_ui_msg_done(&queued[k]);
    }
    queued_count = 0;
    return left;
}

/* the log of a call, as the stub writes it to stderr: how many lines hold the words */
static char log_text[4096];
static void log_start(FILE **file, int *saved)
{
    fflush(stderr);
    *file  = tmpfile();
    *saved = dup(2);
    dup2(fileno(*file), 2);
}
static int log_lines(FILE *file, int saved, const char *words)
{
    fflush(stderr);
    dup2(saved, 2), close(saved);
    rewind(file);
    log_text[fread(log_text, 1, sizeof log_text - 1, file)] = 0;
    fclose(file);
    int lines = 0;
    for (const char *at = log_text; (at = strstr(at, words)); ++at) ++lines;
    return lines;
}

int main(void)
{
    /* the macros: the widget as written by hand */
    CHECK(save_w.kind == DGX_UI_BUTTON && save_w.look == &dgx_ui_look_button && save_w.on_event == dgx_ui_action_command &&
          (intptr_t)save_w.ctx == CMD_SAVE && save_w.tag == CMD_SAVE && save_w.want_w == 40 && save_w.text &&
          strcmp(save_w.text->buf, "Save") == 0 && save_w.text->size == 0 && save_w.text->lines == 1 && save_w.text->style == &st);
    CHECK(reader_w.kind == DGX_UI_LABEL && reader_w.look == NULL && reader_w.take == reader_take && strcmp(reader_w.text->buf, "Reader") == 0 &&
          reader_w.text->lines == 1 && reader_w.text->style == &st && reader_w.on_event == NULL && reader_w.tag == 0);

    dgx_ui_init(&ui, nodes, 10, 1);
    dgx_ui_screen(&ui, 200, 200, 0);
    dgx_ui_add(&ui, -1, &root_w);
    save = dgx_ui_add(&ui, 0, &save_w), save2 = dgx_ui_add(&ui, 0, &save2_w), back = dgx_ui_add(&ui, 0, &back_w);
    hide = dgx_ui_add(&ui, 0, &hide_w), drop = dgx_ui_add(&ui, 0, &drop_w);
    dgx_ui_add(&ui, 0, &reader_w), dgx_ui_add(&ui, 0, &second_w);
    dgx_ui_layout(&ui);
    dgx_ui_commands_to(QUEUE);
    int unclaimed[QUEUE_ROOM];

    /* the finger: one message, the command with no data, sent and not yet offered while the input call ran; then
       offered by the loop and claimed by the reader */
    claim = CMD_SAVE, takes_seen = 0;
    click(&ui, save, 0);
    CHECK(queued_count == 1 && queued[0].what == CMD_SAVE && queued[0].count == 0 && !(queued[0].flags & DGX_UI_MSG_REF) && takes_seen == 0);
    CHECK(offer(&ui, unclaimed) == 0 && takes_seen == 1);

    /* the key of activation on the focused button: unclaimed, the program's */
    claim = -1;
    dgx_ui_focus(&ui, save2);
    CHECK(dgx_ui_key(&ui, DGX_UI_KEY_ACTIVATE) && queued_count == 1 && queued[0].what == CMD_SAVE);
    CHECK(offer(&ui, unclaimed) == 1 && unclaimed[0] == CMD_SAVE);

    /* two readers: both read it and leave it, the program has it */
    back_readers = 0;
    click(&ui, back, 100);
    CHECK(offer(&ui, unclaimed) == 1 && unclaimed[0] == CMD_BACK && back_readers == 2);

    /* a take that hides the button that sent it, and one that removes it: the walk goes on (ASan sees no read freed) */
    hide_button = hide;
    click(&ui, hide, 200);
    CHECK(offer(&ui, unclaimed) == 1 && unclaimed[0] == CMD_HIDE && (dgx_ui_node(&ui, hide)->state.bits & DGX_UI_HIDDEN));
    dgx_ui_layout(&ui); /* the column laid again without it, where the next click is aimed */
    drop_button = drop;
    click(&ui, drop, 300);
    CHECK(offer(&ui, unclaimed) == 1 && unclaimed[0] == CMD_DROP && dgx_ui_widget(&ui, drop) == NULL);
    dgx_ui_layout(&ui);

    /* drawn pressed for a key of the board, and let go: nothing sent */
    dgx_ui_set_state(&ui, save, DGX_UI_PRESSED, true);
    dgx_ui_set_state(&ui, save, DGX_UI_PRESSED, false);
    CHECK(queued_count == 0);

    /* dgx_ui_enable_tag(): the two buttons of CMD_SAVE off, the focus and a press there lost (the finger down on the
       second gave it the focus too); clicked off, nothing; on again, sent; a tag of 0 refused */
    {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, save2);
        dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(n->x + 2), .y = (int16_t)(n->y + 2), .down = true, .pressed = true, .ms = 400});
    }
    CHECK(ui.pressed == save2 && ui.focus == save2);
    CHECK(dgx_ui_enable_tag(&ui, CMD_SAVE, false) == 2);
    CHECK((dgx_ui_node(&ui, save)->state.bits & DGX_UI_DISABLED) && (dgx_ui_node(&ui, save2)->state.bits & DGX_UI_DISABLED));
    CHECK(ui.focus == -1 && ui.pressed == -1 && !(dgx_ui_node(&ui, back)->state.bits & DGX_UI_DISABLED));
    {
        const dgx_ui_node_t *n = dgx_ui_node(&ui, save2);
        dgx_ui_pointer(&ui, &(dgx_ui_pointer_t){.x = (int16_t)(n->x + 2), .y = (int16_t)(n->y + 2), .released = true, .ms = 450});
    }
    click(&ui, save, 500);
    CHECK(queued_count == 0);
    CHECK(dgx_ui_enable_tag(&ui, CMD_SAVE, true) == 2 && !(dgx_ui_node(&ui, save)->state.bits & DGX_UI_DISABLED));
    click(&ui, save, 600);
    CHECK(queued_count == 1 && queued[0].what == CMD_SAVE);
    queued_count = 0;
    CHECK(dgx_ui_enable_tag(&ui, 0, false) == 0 && !(dgx_ui_node(&ui, 0)->state.bits & DGX_UI_DISABLED));

    /* no queue named: lost, told once; a full queue: lost, told once */
    {
        FILE *file;
        int   saved;
        log_start(&file, &saved);
        dgx_ui_commands_to(NULL);
        click(&ui, save, 700), click(&ui, save, 800);
        const int no_queue = log_lines(file, saved, "no queue named by dgx_ui_commands_to()");
        CHECK(queued_count == 0 && no_queue == 1);
        dgx_ui_commands_to(QUEUE);
        log_start(&file, &saved);
        for (int k = 0; k < QUEUE_ROOM + 2; ++k) click(&ui, back, 900 + 100 * (uint32_t)k);
        const int full = log_lines(file, saved, "lost, the queue is full");
        CHECK(queued_count == QUEUE_ROOM && full == 1);
        queued_count = 0;
    }
    dgx_ui_free(&ui);

    /* a description: "on_event": "command" and a number in "ctx"; a context of the program's refused there */
    {
        static const dgx_ui_look_name_t     looks[]    = {DGX_UI_LOOK_NAME(button)};
        static const dgx_ui_style_name_t    styles[]   = {{"text", &st}};
        static const dgx_ui_on_event_name_t handlers[] = {DGX_UI_ACTION_NAME(command)};
        static const dgx_ui_context_name_t  contexts[] = {{"other", (void *)&queue_place, false}};
        static const dgx_ui_load_names_t    names      = {.looks = looks, .look_count = 1, .styles = styles, .style_count = 1, .handlers = handlers,
                                                          .handler_count = 1, .contexts = contexts, .context_count = 1};
        static const char  page[]  = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"button\", \"text\": \"Go\", \"on_event\": \"command\", \"ctx\": 7, \"want\": [40, 16]}}";
        static const char  wrong[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"button\", \"text\": \"Go\", \"on_event\": \"command\", \"ctx\": \"other\"}}";
        static void       *room[64]; /* aligned as a pointer */
        static dgx_ui_node_t page_nodes[2];
        dgx_ui_t             tree;
        dgx_ui_load_error_t  err;
        dgx_ui_init(&tree, page_nodes, 2, 1);
        dgx_ui_screen(&tree, 100, 100, 0);
        const int button = dgx_ui_load(&tree, -1, page, strlen(page), &names, room, sizeof room, &err);
        CHECK(button >= 0 && dgx_ui_widget(&tree, button)->on_event == dgx_ui_action_command && (intptr_t)dgx_ui_widget(&tree, button)->ctx == 7);
        dgx_ui_layout(&tree);
        click(&tree, button, 0);
        CHECK(queued_count == 1 && queued[0].what == 7 && offer(&tree, unclaimed) == 1 && unclaimed[0] == 7);
        dgx_ui_free(&tree);
        dgx_ui_init(&tree, page_nodes, 2, 1);
        CHECK(dgx_ui_load(&tree, -1, wrong, strlen(wrong), &names, room, sizeof room, &err) < 0 && err.code == DGX_UI_LOAD_FIELD);
        dgx_ui_free(&tree);
    }
    CHECK_DONE();
}
