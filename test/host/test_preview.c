/*
 * The preview of a description (editor/preview/dgx_ui_preview.c) on the computer: a page drawn by it pixel for pixel
 * as the same page loaded by a program on the same tables and drawn on a panel of panel.h, at a point of 2 pixels
 * and of 1 as the program's dgx_ui_init() takes it; the loader's verdicts the same; the widgets of the page on the
 * screen as its tree has them; a click told, a value set, a page opened by the action page and closed by back; the
 * events and the notes in whole lines, those past their room counted; an appearance's colours and looks; the panel of
 * 16, 18 and 24 bits; a names file refused in dgx_ui_c's words; the stand-ins said; and texts spoiled at a byte given
 * to every call, none of them a fault.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "dgx_ui.h"
#include "dgx_ui_load.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_preview.h"
#include "dgx_ui_text.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"

static const char page[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"pad\": 2, \"gap\": 2, \"children\": ["
                           "{\"kind\": \"frame\", \"want\": [0, 20], \"pad\": 2, \"children\": [{\"kind\": \"label\", \"text\": \"Hello\"}]},"
                           "{\"kind\": \"slider\", \"value\": \"v\", \"min\": 0, \"max\": 100, \"want\": [0, 10]},"
                           "{\"kind\": \"button\", \"text\": \"Go\", \"on\": \"go\", \"want\": [0, 16]}]}}";

static const char settings[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"children\": ["
                               "{\"kind\": \"label\", \"text\": \"Settings\"}, "
                               "{\"kind\": \"button\", \"text\": \"Back\", \"on\": \"back\", \"want\": [0, 16]}]}}";

/* the pages the action page opens, by their name */
size_t preview_page_text(const char *name, char *out, size_t size)
{
    if (strcmp(name, "settings") != 0) return 0;
    if (out && size >= sizeof settings - 1) memcpy(out, settings, sizeof settings - 1);
    return sizeof settings - 1;
}

/* the same page loaded by a program on tables of its own, as the rule makes them, drawn on panel.h's panel */
static int  went;
static void on_go(dgx_ui_t *ui, int i, const dgx_ui_node_t *node, int what, void *ctx)
{
    (void)ui, (void)i, (void)node, (void)ctx;
    went += what == DGX_UI_ACTIVATED;
}

static const dgx_ui_font_variant_t terminus[1]  = {{.font = &TerminusTTFMedium12}};
static const dgx_ui_text_style_t   plain        = {.variants = terminus, .count = 1};
static int32_t                     direct_value = 0;

static panel_t *drawn_directly(int pixels_a_point)
{
    static const dgx_ui_look_name_t  looks[]    = {DGX_UI_LOOK_NAME(slider), DGX_UI_LOOK_NAME(button)};
    static const dgx_ui_style_name_t styles[]   = {{"the default", &plain}};
    const dgx_ui_value_name_t        values[]   = {{"v", &direct_value}};
    static const dgx_ui_on_name_t    handlers[] = {{"go", on_go, false}};
    const dgx_ui_load_names_t names = {.looks = looks, .look_count = 2, .styles = styles, .style_count = 1, .values = values, .value_count = 1,
                                       .handlers = handlers, .handler_count = 1};
    static dgx_ui_node_t nodes[16];
    static uint8_t       memory[1024] __attribute__((aligned(16)));
    static dgx_ui_t      ui;
    dgx_ui_load_error_t  err;
    dgx_ui_init(&ui, nodes, 16, pixels_a_point);
    CHECK(dgx_ui_load(&ui, -1, page, sizeof page - 1, &names, memory, sizeof memory, &err) == 0);
    panel_t       *p      = panel_make();
    dgx_screen_t  *scr    = &p->sbus.scr;
    const uint32_t colors[] = {dgx_screen_rgb(scr, 16, 20, 28), dgx_screen_rgb(scr, 230, 230, 220), dgx_screen_rgb(scr, 32, 48, 72),
                              dgx_screen_rgb(scr, 120, 160, 220)};
    dgx_ui_paint_t paint  = {.scr = scr, .colors = colors, .color_count = 4};
    dgx_ui_time(&ui, 1000);
    dgx_ui_paint(&ui, &paint, 1000);
    dgx_ui_paint_free(&paint);
    dgx_ui_free(&ui);
    return p;
}

/* the panel's pixel as the preview shows it, red, green, blue */
static void rgb_of(uint32_t pixel, uint8_t out[3])
{
    const unsigned red = pixel >> 11, green = (pixel >> 5) & 0x3f, blue = pixel & 0x1f;
    out[0] = (uint8_t)(red << 3 | red >> 2), out[1] = (uint8_t)(green << 2 | green >> 4), out[2] = (uint8_t)(blue << 3 | blue >> 2);
}

static int widget_of_kind(int kind)
{
    for (int i = 0; i < preview_count(); ++i)
        if (preview_widget(i)->kind == kind) return i;
    return -1;
}

static void click(int i, uint32_t *now)
{
    const preview_widget_t *w = preview_widget(i);
    const int               x = w->x + w->w / 2, y = w->y + w->h / 2;
    preview_pointer(x, y, 1, *now), preview_paint(*now), *now += 50;
    preview_pointer(x, y, 0, *now), preview_paint(*now), *now += 50;
}

static uint32_t chance = 4646;
static uint32_t next_chance(void)
{
    chance = chance * 1103515245u + 12345u;
    return chance >> 8;
}

int main(void)
{
    CHECK(strstr(preview_version(), "\"dgx_ui\"") && strstr(preview_version(), "\"dgx\"") && strstr(preview_version(), "\"sources\""));
    CHECK(preview_screen(0, 10, 16, 1) == PREVIEW_NO_SCREEN && preview_screen(10, 10, 12, 1) == PREVIEW_NO_SCREEN);
    CHECK(preview_screen(10, 10, 16, 0) == PREVIEW_NO_SCREEN);

    /* the page by the rule, drawn as the program draws it, at a point of 2 pixels and of 1 */
    const int32_t *changed;
    const uint8_t *frame;
    for (int pixels_a_point = 2; pixels_a_point >= 1; --pixels_a_point) {
        CHECK(preview_screen(PANEL_W, PANEL_H, 16, pixels_a_point) == 0);
        CHECK(preview_names(NULL, 0)->code == 0 && preview_appearance(NULL, 0)->code == 0);
        char *room = preview_text(sizeof page);
        memcpy(room, page, sizeof page - 1);
        CHECK(preview_page(room, sizeof page - 1)->code == 0);
        CHECK(preview_paint(1000) == 1);
        changed = preview_changed();
        CHECK(changed[0] == 0 && changed[1] == 0 && changed[2] == PANEL_W && changed[3] == PANEL_H);
        CHECK(preview_paint(1001) == 0 && preview_changed()[2] == 0);
        panel_t *direct = drawn_directly(pixels_a_point);
        frame           = preview_frame();
        int apart       = 0;
        for (int y = 0; y < PANEL_H; ++y)
            for (int x = 0; x < PANEL_W; ++x) {
                uint8_t rgb[3];
                rgb_of(direct->mem[y][x], rgb);
                const uint8_t *got = frame + 4 * (y * PANEL_W + x);
                apart += got[0] != rgb[0] || got[1] != rgb[1] || got[2] != rgb[2] || got[3] != 255;
            }
        if (apart) fprintf(stderr, "  at %d pixels a point: %d pixels apart from the program's\n", pixels_a_point, apart);
        CHECK(apart == 0);
        free(direct);
        if (pixels_a_point == 2) CHECK(preview_widget(1)->h == 2 * 20); /* the frame's 20 points */
    }
    CHECK(strstr(preview_notes(), "style of a text that names none") != NULL);

    /* the widgets as the tree has them */
    CHECK(preview_count() == 5 && preview_widget(5) == NULL && preview_widget(-1) == NULL);
    const int label = widget_of_kind(DGX_UI_LABEL), slider = widget_of_kind(DGX_UI_SLIDER), button = widget_of_kind(DGX_UI_BUTTON);
    CHECK(label == 2 && slider == 3 && button == 4 && preview_widget(label)->parent == 1 && strcmp(preview_widget(label)->text, "Hello") == 0);
    CHECK(preview_widget(0)->w == PANEL_W && preview_widget(0)->h == PANEL_H && preview_widget(0)->shown);
    const preview_widget_t *b = preview_widget(button);
    CHECK(preview_widget_at(b->x + 1, b->y + 1) == button && preview_widget_at(-5, -5) == -1);

    /* shown: not a hidden widget, nor a child of one */
    static const char hidden[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"children\": ["
                                 "{\"kind\": \"box\", \"state\": "
                                 "[\"hidden\"], \"children\": [{\"kind\": \"label\", \"text\": \"x\"}]}, {\"kind\": \"label\", \"text\": \"y\"}]}}";
    CHECK(preview_page(hidden, sizeof hidden - 1)->code == 0 && preview_paint(500) == 1);
    CHECK(!preview_widget(1)->shown && (preview_widget(1)->bits & DGX_UI_HIDDEN) && !preview_widget(2)->shown && preview_widget(3)->shown);
    CHECK(preview_page(page, sizeof page - 1)->code == 0 && preview_paint(1000) == 1);

    /* a click told; a value set */
    uint32_t now = 2000;
    preview_events();
    click(button, &now);
    CHECK(strstr(preview_events(), "told 4 ACTIVATED\n") != NULL);
    preview_changed();
    CHECK(preview_value("v", 70) == 0 && preview_value("none", 1) == -1 && preview_widget(slider)->value == 70);
    CHECK(preview_paint(now) == 1);
    changed = preview_changed();
    const preview_widget_t *s = preview_widget(slider);
    CHECK(changed[2] > 0 && changed[0] >= s->x && changed[1] >= s->y);
    CHECK(changed[0] + changed[2] <= s->x + s->w && changed[1] + changed[3] <= s->y + s->h);

    /* the loader's verdict, the same as on a program's tables */
    static const char broken[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"pad\": 300}}";
    const preview_verdict_t *verdict = preview_page(broken, sizeof broken - 1);
    dgx_ui_load_need_t       need;
    dgx_ui_load_error_t      err;
    const dgx_ui_load_names_t none = {0};
    CHECK(!dgx_ui_load_need(broken, sizeof broken - 1, &none, &need, &err));
    CHECK(verdict->code == err.code && verdict->offset == err.offset && strcmp(verdict->name, err.name) == 0 && verdict->code == DGX_UI_LOAD_RANGE);
    CHECK(preview_count() == 0 && preview_paint(now) == 0);

    /* a names file: refused in dgx_ui_c's words; taken, the page on it alone */
    static const char wrong[] = "value v &v\nvalues w &w\n";
    verdict = preview_names(wrong, sizeof wrong - 1);
    CHECK(verdict->code == PREVIEW_NAMES_REFUSED && strstr(verdict->said, "names:2: no table \"values\"") == verdict->said);
    static const char file[] = "value v &v\nlook slider\nlook button\non go go_fn\nstyle body &body\n";
    CHECK(preview_names(file, sizeof file - 1)->code == 0);
    CHECK(preview_page(page, sizeof page - 1)->code == 0 && preview_count() == 5);
    static const char other[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"slider\", \"value\": \"w\"}}";
    verdict = preview_page(other, sizeof other - 1);
    CHECK(verdict->code == DGX_UI_LOAD_UNKNOWN && strcmp(verdict->name, "w") == 0);
    CHECK(preview_names(NULL, 0)->code == 0);

    /* a page opened by the action page, by its name, and closed by back */
    static const char home_page[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"children\": ["
                                    "{\"kind\": \"button\", \"text\": \"More\", \"on\": \"page\", \"ctx\": \"settings\", \"want\": [0, 16]},"
                                    "{\"kind\": \"button\", \"text\": \"Lost\", \"on\": \"page\", \"ctx\": \"nowhere\", \"want\": [0, 16]}]}}";
    CHECK(preview_page(home_page, sizeof home_page - 1)->code == 0);
    preview_paint(now), preview_changed(), preview_events();
    click(1, &now);
    CHECK(strstr(preview_events(), "told 1 ACTIVATED\n") != NULL && strcmp(preview_shown(), "settings") == 0);
    CHECK(strstr(preview_notes(), "page settings: style of a text that names none") != NULL); /* a page's notes led by its name */
    preview_paint(now);
    CHECK(preview_changed()[2] == PANEL_W); /* the page over the screen */
    /* the widgets are the page's on the screen */
    CHECK(strcmp(preview_shown(), "settings") == 0 && preview_count() == 3 && strcmp(preview_widget(1)->text, "Settings") == 0);
    CHECK(preview_widget_at(preview_widget(2)->x + 2, preview_widget(2)->y + 2) == 2 && preview_widget(2)->kind == DGX_UI_BUTTON);
    preview_key(DGX_UI_KEY_NEXT), preview_key(DGX_UI_KEY_ACTIVATE), preview_paint(now); /* the page's button, by the keys */
    const char *told = preview_events();
    CHECK(strstr(told, "told 2 FOCUS page settings\n") != NULL && strstr(told, "told 2 ACTIVATED page settings\n") != NULL);
    CHECK(strcmp(preview_shown(), "") == 0 && preview_count() == 3 && preview_widget(1)->kind == DGX_UI_BUTTON); /* home again */
    preview_paint(now);
    click(2, &now);
    CHECK(strstr(preview_events(), "unopened nowhere no-text\n") != NULL);

    /* more than the events hold: whole lines, and the last says how many were lost */
    for (int round = 0; round < 400; ++round) click(2, &now); /* "Lost": told, and no page for it */
    const char *many = preview_events();
    CHECK(strlen(many) < 4096 && strncmp(many, "told 2 ", 7) == 0);
    const char *last = strrchr(many, '\n');
    while (last > many && last[-1] != '\n') --last;
    unsigned lost = 0;
    CHECK(sscanf(last, "lost %u\n", &lost) == 1 && lost > 0);
    for (const char *line = many; line < last; line = strchr(line, '\n') + 1)
        CHECK(strncmp(line, "told ", 5) == 0 || strncmp(line, "unopened ", 9) == 0);
    CHECK(preview_events()[0] == 0); /* read, and gone */

    /* an appearance: its colours, its looks and styles first; a font not built in stands in, and is said */
    static const char look[] = "{\"dgx-ui\": 1, \"name\": \"day\", \"colors\": {\"paper\": \"#c4e8fc\", \"ink\": \"#ffffff\", \"fill\": \"#204060\", "
                               "\"border\": \"#808080\", \"accent\": \"#f0a03c\"}, \"styles\": {\"body\": {\"fonts\": [\"small\"]}}, "
                               "\"looks\": {\"big\": {\"draw\": \"button\", \"params\": {\"face\": \"accent\", \"pressed\": \"ink\", "
                               "\"focused\": null, \"disabled\": \"border\", \"text\": \"paper\", \"text_disabled\": \"border\", "
                               "\"shadow\": \"paper\", \"depth\": 1}}}}";
    static const char themed[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"pad\": 4, \"children\": ["
                                 "{\"kind\": \"label\", \"text\": \"Day\"}, "
                                 "{\"kind\": \"button\", \"look\": \"big\", \"text\": \"Go\", \"want\": [0, 16]}]}}";
    for (int depth = 16; depth <= 24; depth += 4) {
        const int d = depth == 20 ? 18 : depth;
        CHECK(preview_screen(PANEL_W, PANEL_H, d, 1) == 0);
        CHECK(preview_appearance(look, sizeof look - 1)->code == 0);
        CHECK(preview_page(themed, sizeof themed - 1)->code == 0 && preview_paint(now) == 1);
        frame                 = preview_frame();
        const uint8_t want[3] = {0xc4, 0xe8, 0xfc}; /* the paper, its low bits set where 18 bits take six */
        uint8_t       near[3];
        for (int c = 0; c < 3; ++c) near[c] = d == 24 ? want[c] : d == 18 ? (uint8_t)((want[c] & 0xfc) | want[c] >> 6) : 0;
        if (d == 16) rgb_of((uint32_t)(0xc4 >> 3) << 11 | (uint32_t)(0xe8 >> 2) << 5 | (0xfc >> 3), near);
        CHECK(frame[0] == near[0] && frame[1] == near[1] && frame[2] == near[2]);
        const preview_widget_t *go     = preview_widget(2);
        const uint8_t          *middle = frame + 4 * ((go->y + go->h / 2) * PANEL_W + go->x + 3);
        CHECK(middle[0] > 0xc0 && middle[1] > 0x80 && middle[2] < 0x60); /* the accent's face */
    }
    CHECK(strstr(preview_notes(), "font small: TerminusTTFMedium12 stands in") != NULL);
    CHECK(strstr(preview_notes(), "style of a text that names none") == NULL); /* the appearance's style first */
    static const char wrong_look[] = "{\"dgx-ui\": 1, \"colors\": {\"paper\": \"#102030\"}}";
    CHECK(preview_appearance(wrong_look, sizeof wrong_look - 1)->code == DGX_UI_LOAD_MISSING);
    CHECK(preview_appearance(NULL, 0)->code == 0);

    /* a look of the program's own and a picture: their stand-ins drawn and said */
    static const char own[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"row\"], \"children\": ["
                              "{\"kind\": \"own\", \"look\": \"dial\", "
                              "\"want\": [40, 30]}, {\"kind\": \"picture\", \"picture\": \"logo\"}]}}";
    CHECK(preview_screen(PANEL_W, PANEL_H, 16, 1) == 0 && preview_page(own, sizeof own - 1)->code == 0 && preview_paint(now) == 1);
    CHECK(strstr(preview_notes(), "look dial: the program's own") && strstr(preview_notes(), "picture logo: a crossed square"));
    CHECK(preview_widget(2)->w == 16 && preview_widget(2)->h == 16);

    /* more stand-ins than the notes hold: whole lines, the last says how many were lost */
    static char crowd[16384];
    int         at = snprintf(crowd, sizeof crowd, "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"children\": [");
    for (int k = 0; k < 200; ++k)
        at += snprintf(crowd + at, sizeof crowd - (size_t)at, "%s{\"kind\": \"own\", \"look\": \"own%d\"}", k ? ", " : "", k);
    at += snprintf(crowd + at, sizeof crowd - (size_t)at, "]}}");
    CHECK(preview_page(crowd, (size_t)at)->code == 0);
    const char *noted = preview_notes(), *noted_last = noted + strlen(noted) - 1;
    while (noted_last > noted && noted_last[-1] != '\n') --noted_last;
    unsigned noted_lost = 0;
    CHECK(strlen(noted) < 4096 && sscanf(noted_last, "lost %u\n", &noted_lost) == 1 && noted_lost > 0);

    /* spoiled at a byte: a verdict, never a fault */
    const char *texts[]   = {page, look, file, home_page};
    size_t      lengths[] = {sizeof page - 1, sizeof look - 1, sizeof file - 1, sizeof home_page - 1};
    static const char bytes[] = "\"{}[],:#0a1 x.-\n&";
    for (int round = 0; round < 1500; ++round) {
        const int which = (int)(next_chance() % 4);
        char      spoiled[1024];
        memcpy(spoiled, texts[which], lengths[which]);
        spoiled[next_chance() % lengths[which]] = bytes[next_chance() % (sizeof bytes - 1)];
        if (which == 1) preview_appearance(spoiled, lengths[which]), preview_page(themed, sizeof themed - 1);
        else if (which == 2) preview_names(spoiled, lengths[which]), preview_page(page, sizeof page - 1);
        else preview_page(spoiled, lengths[which]);
        preview_paint(now);
        if (preview_count()) click(0, &now);
        preview_value("v", (int32_t)(next_chance() % 200) - 50), preview_paint(now);
        preview_events(), preview_changed();
        if (which == 1) preview_appearance(NULL, 0);
        if (which == 2) preview_names(NULL, 0);
    }
    CHECK_DONE();
}
