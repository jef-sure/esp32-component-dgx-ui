/*
 * The messages of dgx-ui without FreeRTOS: data by value copied out by number and size, data by reference done with
 * once at the end of the loop's body, the walk that stops at a claim and offers the hidden too, a widget that keeps
 * the data past the walk, a take() that removes widgets, and code points typed into a field.
 */
#include <string.h>

#include "check.h"
#include "dgx_ui.h"
#include "dgx_ui_msg.h"
#include "dgx_ui_text.h"

enum { MSG_CO2 = 7, MSG_BIG = 8, MSG_DROP = 9 };
typedef struct {
    float   rh;
    int16_t ppm, t;
} co2_t;
DGX_UI_DECODER(co2, MSG_CO2, co2_t)

/* the data of a message by reference, and how many times it was done with */
static char big_data[100];
static int  done_count;
static void big_done(void *data, size_t size)
{
    CHECK(data == big_data && size == sizeof big_data);
    ++done_count;
}

/* what the widgets saw: the order of the offers, and the ppm each caught */
static int  offered[16], offered_count, caught_ppm[16];
static int  claim_at = -1, keep_at = -1, drop_at = -1;
static bool keep_twice;
static dgx_ui_msg_t kept, kept2;

static bool take(dgx_ui_t *ui, int i, const dgx_ui_msg_t *msg)
{
    if (offered_count < 16) offered[offered_count++] = i;
    co2_t c;
    if (msg_decode_co2(msg, &c) == ESP_OK) caught_ppm[i] = c.ppm;
    if (i == drop_at && msg->what == MSG_DROP) dgx_ui_remove(ui, i);
    if (i == keep_at && msg->what == MSG_BIG) {
        dgx_ui_msg_keep(ui, &kept);
        if (keep_twice) dgx_ui_msg_keep(ui, &kept2);
    }
    return i == claim_at;
}

static const dgx_ui_widget_t root_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .take = take};
static const dgx_ui_widget_t text_w   = {.kind = DGX_UI_LABEL, .take = take};
static const dgx_ui_widget_t hidden_w = {.kind = DGX_UI_LABEL, .take = take, .state = {.bits = DGX_UI_HIDDEN}};
static const dgx_ui_widget_t box_w    = {.kind = DGX_UI_BOX, .take = take};

static dgx_ui_node_t nodes[8];
static dgx_ui_t      ui;
static int           a, b, h, box, inner;

static void make(void)
{
    dgx_ui_init(&ui, nodes, 8, 1);
    dgx_ui_add(&ui, -1, &root_w);
    a     = dgx_ui_add(&ui, 0, &text_w);
    box   = dgx_ui_add(&ui, 0, &box_w);
    inner = dgx_ui_add(&ui, box, &text_w);
    h     = dgx_ui_add(&ui, 0, &hidden_w);
    b     = dgx_ui_add(&ui, 0, &text_w);
    offered_count = 0, claim_at = keep_at = drop_at = -1;
    memset(caught_ppm, 0, sizeof caught_ppm);
}

static void test_value(void)
{
    dgx_ui_msg_t msg;
    const co2_t  c = {.rh = 40.5f, .ppm = 812, .t = 215};
    CHECK(sizeof(dgx_ui_msg_t) == 4 + 4 + 2 * sizeof(void *) + sizeof(size_t)); /* 20 on a 32-bit chip */
    CHECK(dgx_ui_msg_value(&msg, MSG_CO2, &c, sizeof c) && msg.count == sizeof c);
    char too_big[13] = {0};
    CHECK(!dgx_ui_msg_value(&msg, MSG_CO2, too_big, sizeof too_big));
    dgx_ui_msg_value(&msg, MSG_CO2, &c, sizeof c);
    co2_t out;
    CHECK(msg_decode_co2(&msg, &out) == ESP_OK && out.ppm == 812 && out.t == 215);
    CHECK(dgx_ui_msg_copy(&msg, MSG_BIG, &out, sizeof out) == ESP_ERR_NOT_FOUND);
    CHECK(dgx_ui_msg_copy(&msg, MSG_CO2, &out, sizeof out - 2) == ESP_ERR_INVALID_SIZE);
    /* every widget offered it in the order of the tree, the hidden too; each caught its value */
    make();
    CHECK(!dgx_ui_take(&ui, &msg));
    CHECK(offered_count == 6 && offered[0] == 0 && offered[1] == a && offered[2] == box && offered[3] == inner && offered[4] == h && offered[5] == b);
    CHECK(caught_ppm[h] == 812 && caught_ppm[b] == 812);
    /* a claim stops the walk */
    make();
    claim_at = box;
    CHECK(dgx_ui_take(&ui, &msg) && offered_count == 3 && caught_ppm[inner] == 0);
    dgx_ui_msg_done(&msg); /* by value: nothing */
    dgx_ui_free(&ui);
}

static void test_ref(void)
{
    dgx_ui_msg_t msg;
    /* by reference: nothing done with by the walk; once by the loop's done, however often it is called */
    make();
    done_count = 0;
    dgx_ui_msg_ref(&msg, MSG_BIG, big_data, sizeof big_data, big_done);
    CHECK(!dgx_ui_take(&ui, &msg) && done_count == 0);
    size_t      size;
    const void *data = dgx_ui_msg_data(&msg, &size);
    CHECK(data == big_data && size == sizeof big_data); /* the program may still look */
    dgx_ui_msg_done(&msg);
    dgx_ui_msg_done(&msg);
    CHECK(done_count == 1);
    /* kept by a widget: claimed, the loop's done does nothing, the widget's copy is done with once */
    make();
    done_count = 0, keep_at = inner;
    dgx_ui_msg_ref(&msg, MSG_BIG, big_data, sizeof big_data, big_done);
    CHECK(dgx_ui_take(&ui, &msg) && offered_count == 4);
    dgx_ui_msg_done(&msg);
    CHECK(done_count == 0);
    dgx_ui_msg_done(&kept);
    dgx_ui_msg_done(&kept);
    CHECK(done_count == 1);
    /* kept twice in one take(): the second copy has no data, the data is done with once */
    make();
    done_count = 0, keep_at = inner, keep_twice = true;
    dgx_ui_msg_ref(&msg, MSG_BIG, big_data, sizeof big_data, big_done);
    CHECK(dgx_ui_take(&ui, &msg));
    CHECK(kept2.flags == 0 && kept2.what == 0);
    dgx_ui_msg_done(&msg), dgx_ui_msg_done(&kept2), dgx_ui_msg_done(&kept);
    CHECK(done_count == 1);
    keep_twice = false;
    /* keep outside a take: an empty copy */
    dgx_ui_msg_keep(&ui, &kept);
    CHECK(kept.flags == 0 && kept.what == 0);
    /* a message that lives for ever: no done */
    dgx_ui_msg_ref(&msg, MSG_BIG, big_data, sizeof big_data, NULL);
    dgx_ui_take(&ui, &msg);
    dgx_ui_msg_done(&msg);
    /* a take() that removes its widget: the walk goes on past it */
    make();
    drop_at = box;
    dgx_ui_msg_value(&msg, MSG_DROP, NULL, 0);
    CHECK(!dgx_ui_take(&ui, &msg));
    CHECK(offered_count == 5 && offered[2] == box && offered[3] == h && offered[4] == b);
    CHECK(dgx_ui_widget(&ui, box) == NULL && dgx_ui_widget(&ui, inner) == NULL);
    dgx_ui_free(&ui);
}

/* a field: typed into, erased, done and cancelled */
static int  field_told[8], field_told_count;
static void field_on(dgx_ui_t *u, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    if (field_told_count < 8) field_told[field_told_count++] = what;
}
static char                  field_buf[6];
static dgx_ui_textbox_t      field_box;
static const dgx_ui_widget_t field_w = {.kind = DGX_UI_INPUT, .text = &field_box, .on = field_on};

static void test_typed(void)
{
    dgx_ui_textbox_init(&field_box, field_buf, sizeof field_buf, 1, NULL);
    dgx_ui_init(&ui, nodes, 8, 1);
    dgx_ui_add(&ui, -1, &root_w);
    const int f = dgx_ui_add(&ui, 0, &field_w);
    const uint32_t ab[] = {'a', 0x431 /* б */};
    CHECK(!dgx_ui_typed(&ui, ab, 2)); /* no focus: nobody's */
    dgx_ui_focus(&ui, f);
    field_told_count = 0;
    CHECK(dgx_ui_typed(&ui, ab, 2) && strcmp(field_buf, "a\xd0\xb1") == 0);
    CHECK(dgx_ui_node(&ui, f)->marks & DGX_UI_MARK_FIT);
    const uint32_t more[] = {'c', 'd', 'e'};
    dgx_ui_typed(&ui, more, 3);
    CHECK(strcmp(field_buf, "a\xd0\xb1" "cd") == 0); /* 5 bytes and a 0: no room for e */
    const uint32_t erase[] = {DGX_UI_CP_ERASE, DGX_UI_CP_ERASE, DGX_UI_CP_ERASE};
    dgx_ui_typed(&ui, erase, 3);
    CHECK(strcmp(field_buf, "a") == 0); /* б went whole */
    field_told_count = 0;
    const uint32_t done_cancel[] = {DGX_UI_CP_DONE, DGX_UI_CP_CANCEL};
    dgx_ui_typed(&ui, done_cancel, 2);
    CHECK(field_told_count == 2 && field_told[0] == DGX_UI_ACTIVATED && field_told[1] == DGX_UI_BACK);
    dgx_ui_free(&ui);
}

int main(void)
{
    test_value();
    test_ref();
    test_typed();
    CHECK_DONE();
}
