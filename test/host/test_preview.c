/*
 * The preview of a description (editor/preview/dgx_ui_preview.c) on the computer: a page drawn by it pixel for pixel
 * as the same page loaded by a program on the same tables and drawn on a panel of panel.h, at a point of 2 pixels
 * and of 1 as the program's dgx_ui_init() takes it; the loader's verdicts the same; the widgets of the page on the
 * screen as its tree has them; a click told, a value set, a page opened by the action page and closed by back; the
 * events and the notes in whole lines, those past their room counted; an appearance's colours and looks, and what it
 * may say, each draw's fields as the library describes them and its look's values; the panel of
 * 16, 18 and 24 bits; a names file refused in dgx_ui_c's words; the stand-ins said; fonts given as their files, as
 * DGX's font2c writes them, each drawn pixel for pixel as the same font of its C, a file winning over the font built
 * in of its name, the names' kind against the file's said, a file given anew and let go, and files spoiled and cut;
 * and texts spoiled at a byte given to every call, none of them a fault.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "dgx_ui.h"
#include "dgx_ui_appearance.h"
#include "dgx_ui_load.h"
#include "dgx_ui_paint.h"
#include "dgx_ui_preview.h"
#include "dgx_ui_names.h"
#include "dgx_ui_text.h"
#include "fonts/TerminusTTFMedium12.h"
#include "panel.h"
#include "dgxf_font.h"
#include "fonts/IBMPlexMonoMedium28.h"
#include "fonts/font0ss.h"

static const char page[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"pad\": 2, \"gap\": 2, \"children\": ["
                           "{\"kind\": \"frame\", \"want\": [0, 20], \"pad\": 2, \"children\": [{\"kind\": \"label\", \"text\": \"Hello\"}]},"
                           "{\"kind\": \"slider\", \"value\": \"v\", \"min\": 0, \"max\": 100, \"want\": [0, 10]},"
                           "{\"kind\": \"button\", \"text\": \"Go\", \"on_event\": \"go\", \"want\": [0, 16]}]}}";

static const char settings[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"children\": ["
                               "{\"kind\": \"label\", \"text\": \"Settings\"}, "
                               "{\"kind\": \"button\", \"text\": \"Back\", \"on_event\": \"back\", \"want\": [0, 16]}]}}";

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
    went += what == DGX_UI_EVENT_ACTIVATED;
}

static const dgx_ui_font_variant_t terminus[1]  = {{.font = &TerminusTTFMedium12}};
static const dgx_ui_text_style_t   plain        = {.fonts = terminus, .count = 1};
static int32_t                     direct_value = 0;

static panel_t *drawn_directly(int pixels_a_point)
{
    static const dgx_ui_look_name_t  looks[]    = {DGX_UI_LOOK_NAME(slider), DGX_UI_LOOK_NAME(button)};
    static const dgx_ui_style_name_t styles[]   = {{"the default", &plain}};
    const dgx_ui_value_name_t        values[]   = {{"v", &direct_value}};
    static const dgx_ui_on_event_name_t    handlers[] = {{"go", on_go, false}};
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

/* ---- fonts as files ---- */

/* a font's file as DGX's font2c writes it, into memory */
static uint8_t *file_of(const dgx_font_t *font, size_t *length)
{
    char  *bytes = NULL;
    size_t size  = 0;
    FILE  *out   = open_memstream(&bytes, &size);
    char   why[160] = "";
    const int failed = dgxf_of_font(font, "name: a font of test_preview\n", out, why, sizeof why);
    fclose(out);
    if (failed) fprintf(stderr, "  dgxf_of_font: %s\n", why);
    CHECK(!failed);
    *length = size;
    return (uint8_t *)bytes;
}

/* a font of 4 bits, two letters, by hand as DGX's test_soft_text.c makes one */
static uint8_t           soft_bits[2][64];
static dgx_glyph_t       soft_glyphs[2];
static dgx_glyph_array_t soft_ranges[2]; /* the last ends the table */
static dgx_font_t        soft_font;

static void make_soft_font(void)
{
    for (int letter = 0; letter < 2; ++letter) {
        const int width = letter ? 5 : 6, height = 8, pitch = (width * 4 + 7) / 8;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) soft_bits[letter][y * pitch + x / 2] |= (uint8_t)(((x + 2 * y + letter) % 16) << (x % 2 ? 0 : 4));
        soft_glyphs[letter] = (dgx_glyph_t){.offset    = (uint32_t)(letter * sizeof soft_bits[0]),
                                            .width     = (int16_t)width,
                                            .height    = (int16_t)height,
                                            .x_advance = 7,
                                            .x_offset  = 0,
                                            .y_offset  = -7};
    }
    soft_ranges[0] = (dgx_glyph_array_t){.first = 'A', .number = 2, .glyph = 0};
    soft_font      = (dgx_font_t){.glyph_ranges     = soft_ranges,
                                  .glyphs           = soft_glyphs,
                                  .data             = soft_bits[0],
                                  .y_advance        = 9,
                                  .y_offset_lowest  = -7,
                                  .x_widest         = 6,
                                  .x_width_average  = 6,
                                  .f_type           = DGX_FONT_BITMAP_DEPTH4,
                                  .y_bottom_max     = 1,
                                  .x_right_max      = 6,
                                  .number_of_ranges = 1};
}

/* the appearance whose style "body" takes the font "f", a handwritten one through the library's hand, and a page of it */
static const char font_look[] = "{\"dgx-ui\": 1, \"colors\": {\"paper\": \"#102030\", \"ink\": \"#e0f0ff\", \"fill\": \"#204060\", "
                                "\"border\": \"#808080\"}, \"styles\": {\"body\": {\"fonts\": [\"f\"]}}}";
static const char hand_look[] = "{\"dgx-ui\": 1, \"colors\": {\"paper\": \"#102030\", \"ink\": \"#e0f0ff\", \"fill\": \"#204060\", "
                                "\"border\": \"#808080\"}, \"styles\": {\"body\": {\"fonts\": [{\"font\": \"f\", \"hand\": \"handwriting\", "
                                "\"height\": 20}]}}}";
static const char font_page[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"pad\": 2, \"children\": ["
                                "{\"kind\": \"label\", \"text\": \"ABAB\", \"want\": [0, 30]}, {\"kind\": \"label\", \"text\": \"Text 12\"}]}}";

/* the page drawn by a program whose appearance takes the font of C */
static panel_t *drawn_on(const dgx_font_t *font, bool written)
{
    const dgx_ui_font_name_t        fonts[] = {{"f", font}};
    const dgx_ui_hand_name_t        hands[] = {DGX_UI_HAND_NAME(handwriting)};
    const dgx_ui_appearance_names_t names   = {.fonts = fonts, .font_count = 1, .hands = hands, .hand_count = 1};
    const char                     *look    = written ? hand_look : font_look;
    dgx_ui_appearance_need_t        need;
    dgx_ui_appearance_t             appearance;
    dgx_ui_load_error_t             err;
    static uint8_t                  memory[4096] __attribute__((aligned(16)));
    CHECK(dgx_ui_appearance_need(look, strlen(look), &names, &need, &err) && need.bytes <= sizeof memory);
    CHECK(dgx_ui_appearance_load(look, strlen(look), &names, memory, sizeof memory, &appearance, &err));
    const dgx_ui_load_names_t page_names = {.styles = appearance.styles, .style_count = appearance.style_count};
    static dgx_ui_node_t      nodes[8];
    static uint8_t            page_memory[1024] __attribute__((aligned(16)));
    static dgx_ui_t           ui;
    dgx_ui_init(&ui, nodes, 8, 1);
    CHECK(dgx_ui_load(&ui, -1, font_page, sizeof font_page - 1, &page_names, page_memory, sizeof page_memory, &err) == 0);
    panel_t      *p   = panel_make();
    dgx_screen_t *scr = &p->sbus.scr;
    uint32_t      colors[4];
    dgx_ui_appearance_pack(&appearance, dgx_ui_pack_screen, scr, colors);
    dgx_ui_paint_t paint = {.scr = scr, .colors = colors, .color_count = 4};
    dgx_ui_time(&ui, 1000), dgx_ui_paint(&ui, &paint, 1000);
    dgx_ui_paint_free(&paint), dgx_ui_free(&ui);
    return p;
}

static void rgb_of(uint32_t pixel, uint8_t out[3]);

/* the preview's frame against a program's panel: the pixels apart */
static int apart_from(panel_t *direct)
{
    const uint8_t *frame = preview_frame();
    int            apart = 0;
    for (int y = 0; y < PANEL_H; ++y)
        for (int x = 0; x < PANEL_W; ++x) {
            uint8_t rgb[3];
            rgb_of(direct->mem[y][x], rgb);
            const uint8_t *got = frame + 4 * (y * PANEL_W + x);
            apart += got[0] != rgb[0] || got[1] != rgb[1] || got[2] != rgb[2];
        }
    free(direct);
    return apart;
}

/* the page in the preview on a names file, a font's file given for a name of C, the appearance and the page */
static void preview_on_file(const char *names, const char *c_name, const uint8_t *bytes, size_t length, bool written)
{
    CHECK(preview_screen(PANEL_W, PANEL_H, 16, 1) == 0);
    CHECK(preview_names(names, names ? strlen(names) : 0)->code == 0);
    CHECK(preview_font(c_name, bytes, length)->code == 0);
    const char *look = written ? hand_look : font_look;
    CHECK(preview_appearance(look, strlen(look))->code == 0);
    CHECK(preview_page(font_page, sizeof font_page - 1)->code == 0 && preview_paint(1000) == 1);
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
    static const char broken[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"pad\": 32768}}";
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
    static const char file[] = "value v &v\nlook slider\nlook button\non_event go go_fn\nstyle body &body\n";
    CHECK(preview_names(file, sizeof file - 1)->code == 0);
    CHECK(preview_page(page, sizeof page - 1)->code == 0 && preview_count() == 5);
    static const char other[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"slider\", \"value\": \"w\"}}";
    verdict = preview_page(other, sizeof other - 1);
    CHECK(verdict->code == DGX_UI_LOAD_UNKNOWN && strcmp(verdict->name, "w") == 0);
    CHECK(preview_names(NULL, 0)->code == 0);

    /* a page opened by the action page, by its name, and closed by back */
    static const char home_page[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"flags\": [\"column\"], \"children\": ["
                                    "{\"kind\": \"button\", \"text\": \"More\", \"on_event\": \"page\", \"ctx\": \"settings\", \"want\": [0, 16]},"
                                    "{\"kind\": \"button\", \"text\": \"Lost\", \"on_event\": \"page\", \"ctx\": \"nowhere\", \"want\": [0, 16]}]}}";
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
    CHECK(strstr(preview_notes(), "font small: no file for dgx_font_small, TerminusTTFMedium12 stands in") != NULL);
    CHECK(strstr(preview_notes(), "style of a text that names none") == NULL); /* the appearance's style first */
    static const char wrong_look[] = "{\"dgx-ui\": 1, \"colors\": {\"paper\": \"#102030\"}}";
    CHECK(preview_appearance(wrong_look, sizeof wrong_look - 1)->code == DGX_UI_LOAD_MISSING);
    CHECK(preview_appearance(NULL, 0)->code == 0);

    /* an appearance on a names file of its own beside the page's, as dgx_ui_c -a reads one made with NAMES; by the rule,
       as one made with none; on the page's, which has not its draw: the page then refused, its look unknown */
    {
        static const char page_names[]  = "look slider\nvalue volume &volume\n";
        static const char its_names[]   = "draw frame\nfont small TerminusTTFMedium12\n";
        static const char bad_names[]   = "what is this\n";
        static const char framed_look[] = "{\"dgx-ui\": 1, \"colors\": {\"paper\": \"#000000\", \"ink\": \"#ffffff\", \"fill\": \"#204060\", "
                                          "\"border\": \"#808080\"}, \"styles\": {\"body\": {\"fonts\": [\"small\"]}}, "
                                          "\"looks\": {\"pane\": {\"draw\": \"frame\", \"fill\": \"fill\", "
                                          "\"params\": {\"border\": \"border\", \"width\": 1}}}}";
        static const char framed[]      = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"frame\", \"look\": \"pane\", \"pad\": 4, \"flags\": [\"column\"], "
                                          "\"children\": [{\"kind\": \"slider\", \"value\": \"volume\"}, "
                                          "{\"kind\": \"label\", \"text\": \"Framed\"}]}}";
        const size_t look_length = sizeof framed_look - 1, page_length = sizeof framed - 1;
        CHECK(preview_screen(PANEL_W, PANEL_H, 16, 1) == 0 && preview_names(page_names, sizeof page_names - 1)->code == 0);
        CHECK(preview_appearance_on(PREVIEW_ON_PAGE, NULL, 0)->code == 0);
        CHECK(preview_appearance(framed_look, look_length)->code == DGX_UI_LOAD_UNKNOWN);
        const preview_verdict_t *unknown = preview_page(framed, page_length);
        CHECK(unknown->code == DGX_UI_LOAD_UNKNOWN && strcmp(unknown->name, "pane") == 0);
        CHECK(preview_appearance_on(PREVIEW_ON_OWN, its_names, sizeof its_names - 1)->code == 0);
        CHECK(preview_appearance(framed_look, look_length)->code == 0);
        CHECK(preview_page(framed, page_length)->code == 0 && preview_paint(now) == 1);
        CHECK(strstr(preview_notes(), "font small: no file for TerminusTTFMedium12") == NULL); /* the font its own names give */
        CHECK(preview_widget(0)->kind == DGX_UI_FRAME && preview_widget(1)->kind == DGX_UI_SLIDER);
        const uint8_t *inside = preview_frame() + 4 * ((preview_widget(0)->y + 2) * PANEL_W + preview_widget(0)->x + 2);
        CHECK(inside[0] < 0x30 && inside[1] > 0x30 && inside[1] < 0x50 && inside[2] > 0x50); /* the pane's fill */
        CHECK(preview_appearance_on(PREVIEW_ON_RULE, NULL, 0)->code == 0 && preview_appearance(framed_look, look_length)->code == 0);
        CHECK(preview_page(framed, page_length)->code == 0 && strstr(preview_notes(), "font small: no file for dgx_font_small") != NULL);
        CHECK(preview_appearance_on(PREVIEW_ON_OWN, NULL, 0)->code == 0 && preview_appearance(framed_look, look_length)->code == 0);
        CHECK(strstr(preview_notes(), "font small: no file for dgx_font_small") != NULL); /* its own with none: the rule */
        const preview_verdict_t *refused = preview_appearance_on(PREVIEW_ON_OWN, bad_names, sizeof bad_names - 1);
        CHECK(refused->code == PREVIEW_NAMES_REFUSED && strstr(refused->said, "appearance's names") != NULL);
        CHECK(preview_appearance(framed_look, look_length)->code == 0); /* refused, it is read by the rule */
        CHECK(preview_appearance_on(PREVIEW_ON_PAGE, NULL, 0)->code == 0);
        CHECK(preview_appearance(framed_look, look_length)->code == DGX_UI_LOAD_UNKNOWN);
        CHECK(preview_appearance_on(PREVIEW_ON_OWN, its_names, sizeof its_names - 1)->code == 0 && preview_appearance(framed_look, look_length)->code == 0);
        const char *format = preview_appearance_format();
        CHECK(strstr(format, "\"draw\": \"frame\"") != NULL && strstr(format, "\"draw\": \"slider\"") == NULL); /* its own names: frame alone */
        CHECK(strstr(format, "\"fonts\": [\"small\"]") != NULL && strstr(format, "\"by_rule\": false") != NULL);
        static const char own_names[] = "draw dial draw_dial\ndraw frame\nfont small TerminusTTFMedium12\n";
        CHECK(preview_appearance_on(PREVIEW_ON_OWN, own_names, sizeof own_names - 1)->code == 0 && preview_appearance(framed_look, look_length)->code == 0);
        CHECK(strstr(preview_appearance_format(), "{\"draw\": \"dial\", \"own\": true, \"kinds\": [], \"fields\": []}") != NULL);
        CHECK(preview_appearance_on(PREVIEW_ON_PAGE, NULL, 0)->code == 0);
        preview_appearance(NULL, 0), preview_page(NULL, 0), preview_names(NULL, 0);
    }

    /* what an appearance may say, by the rule: every draw of the library with its fields as the library describes them,
       in their order, and the values of the library's look of the draw's word */
    {
        static const struct {
            const char            *word;
            const dgx_ui_params_t *params;
            const dgx_ui_look_t   *look;
        } library[] = {
#define DRAW(word) {#word, &dgx_ui_params_##word, &dgx_ui_look_##word}
            DRAW(box),      DRAW(frame), DRAW(text),     DRAW(lines),   DRAW(hand_lines), DRAW(button), DRAW(picture_button),
            DRAW(input),    DRAW(switch), DRAW(slider),  DRAW(list),    DRAW(dropdown),   DRAW(picture), DRAW(rgb565),
            DRAW(indexed),  DRAW(radio), DRAW(checkbox), DRAW(level),   DRAW(ring),   DRAW(nine),
#undef DRAW
        };
        static const char *const types[] = {"color", "color_or_under", "number", "bool", "word", "flags", "bit", "bands", "font", "hand", "variants",
                                            "picture"};
        static const char *const four[]  = {"\"paper\"", "\"ink\"", "\"fill\"", "\"border\""};
        CHECK(preview_names(NULL, 0)->code == 0 && preview_appearance(NULL, 0)->code == 0);
        const char *format = preview_appearance_format();
        CHECK(strstr(format, "\"by_rule\": true") != NULL && strstr(format, "\"hands\": [\"handwriting\", \"handwriting_canvas\"]") != NULL);
        int fields_held = 0;
        for (size_t d = 0; d < sizeof library / sizeof *library; ++d) {
            char head[64];
            snprintf(head, sizeof head, "{\"draw\": \"%s\", \"own\": false", library[d].word);
            const char *at = strstr(format, head), *end = at ? strstr(at + 1, "\n  {\"draw\"") : NULL;
            if (!end && at) end = strstr(at, "],\n \"fonts\"");
            CHECK(at != NULL && end != NULL);
            if (!at || !end) continue;
            const char *defaults = strstr(at, "\"defaults\": {");
            CHECK(library[d].params->field_count == 0 || (defaults != NULL && defaults < end));
            const char *from = at;
            for (int f = 0; f < library[d].params->field_count; ++f) {
                const dgx_ui_field_t *field = &library[d].params->fields[f];
                char said[96];
                snprintf(said, sizeof said, "{\"name\": \"%s\", \"type\": \"%s\"", field->name, types[field->kind]);
                const char *found = strstr(from, said);
                CHECK(found != NULL && found < end); /* in the library's order */
                if (found) from = found + 1;
                if (found && field->kind == DGX_UI_FIELD_NUMBER) {
                    snprintf(said, sizeof said, "\"min\": %ld, \"max\": %ld}", (long)field->low, (long)field->high);
                    CHECK(strstr(found, said) == strchr(found, '}') - strlen(said) + 1); /* the field's own bounds, its object's end */
                }
                if (!defaults) continue;
                /* a colour's default by its index: the four by their names, DGX_UI_PAPER null */
                if (field->kind == DGX_UI_FIELD_COLOR || field->kind == DGX_UI_FIELD_COLOR_OR_UNDER) {
                    const uint8_t index = *((const uint8_t *)library[d].look->params + field->offset);
                    snprintf(said, sizeof said, "\"%s\": %s", field->name, index == DGX_UI_PAPER ? "null" : index < 4 ? four[index] : "?");
                    CHECK(strstr(defaults, said) != NULL && strstr(defaults, said) < end);
                }
                ++fields_held;
            }
        }
        CHECK(fields_held > 80);
        /* numbers of the defaults as the members hold them: a signed one below 0 */
        CHECK(strstr(format, "\"width\": 3, \"start\": -135, \"sweep\": 270, \"bands_by\": \"place\", \"bands\": []") != NULL);
        CHECK(strstr(format, "\"disabled\": \"fill\", \"track_h\": 2}") != NULL);
    }

    /* what a names file may say, and the names a page uses and a file has not, held to the reading dgx_ui_c shares: its
       tables and their keys, the library's looks, and the C its rule gives each name */
    {
        const char *format = preview_names_format();
        char        said[160];
        for (int t = 0; t < TABLES; ++t) {
            snprintf(said, sizeof said, "\"%s\"", table_words[t]);
            CHECK(strstr(format, said) != NULL);
        }
        CHECK(strstr(format, "\"keys\": [\"look\", \"value\", \"picture\", \"style\", \"on_event\", \"take\", \"ctx\", \"id\"]") != NULL);
        for (int k = 0; k < library_look_count; ++k) {
            snprintf(said, sizeof said, "\"%s\"", library_looks[k]);
            CHECK(strstr(strstr(format, "\"bare\""), said) != NULL);
        }
        static const char named[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"id\": \"top\", \"children\": ["
                                    "{\"kind\": \"slider\", \"value\": \"settings.volume\"}, {\"kind\": \"label\", \"text\": \"x\"},"
                                    "{\"kind\": \"button\", \"text\": \"Go\", \"style\": \"big\", \"on_event\": \"go\", \"ctx\": \"settings\", \"picture\": \"icons\"},"
                                    "{\"kind\": \"button\", \"text\": \"Back\", \"on_event\": \"back\", \"look\": \"plain\", \"take\": \"take_it\"},"
                                    "{\"kind\": \"slider\", \"value\": \"not-c\"}]}}";
        CHECK(preview_names(NULL, 0)->code == 0 && preview_appearance(NULL, 0)->code == 0 && preview_page(named, sizeof named - 1)->code == 0);
        const char *missing = preview_names_missing("value other &other\n", 19);
        names_collect(named, sizeof named - 1);
        names_rule(false);
        int held = 0;
        for (int t = 0; t < TABLES; ++t)
            for (int k = 0; k < tables[t].count; ++k) {
                const entry_t *entry = &tables[t].entries[k];
                if (!entry->ruled && !entry_of(&used[t], entry->name)) continue; /* a look of the library's no widget takes */
                if (t == T_ID || !entry->c || !c_name(entry->name, true)) continue; /* an id's C is of the prefix; not-c none */
                if (entry->ruled) snprintf(said, sizeof said, "{\"table\": \"%s\", \"name\": \"%s\", \"c\": \"%s\", \"bare\": false", table_words[t], entry->name, entry->c);
                else snprintf(said, sizeof said, "{\"table\": \"%s\", \"name\": \"%s\", \"c\": null, \"bare\": true", table_words[t], entry->name);
                if (!strstr(missing, said)) fprintf(stderr, "  not missing as the rule gives it: %s\n", said);
                CHECK(strstr(missing, said) != NULL);
                ++held;
            }
        CHECK(held >= 7);
        CHECK(strstr(missing, "\"name\": \"go\", \"c\": \"go\", \"bare\": false, \"number\": true") != NULL);
        CHECK(strstr(missing, "\"name\": \"settings\", \"c\": \"&settings\", \"bare\": false, \"page\": true") != NULL);
        CHECK(strstr(missing, "\"name\": \"icons\", \"c\": \"icons\", \"bare\": false, \"count\": 65535") != NULL);
        CHECK(strstr(missing, "{\"table\": \"look\", \"name\": \"slider\", \"c\": null, \"bare\": true}") != NULL); /* of its kind's word */
        CHECK(strstr(missing, "{\"table\": \"id\", \"name\": \"top\", \"c\": \"top\"") != NULL);
        CHECK(strstr(missing, "{\"table\": \"value\", \"name\": \"not-c\", \"c\": null, \"bare\": false}") != NULL); /* no C by the rule */
        CHECK(strstr(missing, "\"name\": \"other\"") == NULL && strstr(missing, "\"name\": \"body\"") == NULL); /* the file's; a style named */
        /* a text that names no style, and no style anywhere: "body" wanted; none when the appearance has a style, nor the
           look of a kind's word the appearance has */
        static const char unstyled[] = "{\"dgx-ui\": 1, \"tree\": {\"kind\": \"box\", \"children\": [{\"kind\": \"label\", \"text\": \"x\"}, "
                                       "{\"kind\": \"slider\", \"value\": \"v\"}]}}";
        static const char styled[]   = "{\"dgx-ui\": 1, \"colors\": {\"paper\": \"#000000\", \"ink\": \"#ffffff\", \"fill\": \"#204060\", "
                                       "\"border\": \"#808080\"}, \"styles\": {\"plain\": {\"fonts\": [\"small\"]}}, \"looks\": {\"slider\": "
                                       "{\"draw\": \"slider\", \"params\": {\"track\": \"fill\", \"fill\": \"border\", \"knob\": \"ink\", "
                                       "\"focused\": \"ink\", \"disabled\": \"fill\"}}}}";
        CHECK(preview_page(unstyled, sizeof unstyled - 1)->code == 0);
        missing = preview_names_missing("", 0);
        CHECK(strstr(missing, "{\"table\": \"style\", \"name\": \"body\", \"c\": \"&body\", \"bare\": false}") != NULL);
        CHECK(strstr(missing, "{\"table\": \"look\", \"name\": \"slider\", \"c\": null, \"bare\": true}") != NULL);
        CHECK(preview_appearance(styled, sizeof styled - 1)->code == 0 && preview_page(unstyled, sizeof unstyled - 1)->code == 0);
        missing = preview_names_missing("", 0);
        CHECK(strstr(missing, "\"name\": \"body\"") == NULL && strstr(missing, "\"name\": \"slider\"") == NULL &&
              strstr(missing, "{\"table\": \"value\", \"name\": \"v\", \"c\": \"&v\"") != NULL);
        preview_appearance(NULL, 0), preview_page(NULL, 0);
    }

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

    /* fonts as files, each drawn as the same font of its C */
    preview_appearance(NULL, 0), preview_page(NULL, 0);
    make_soft_font();
    size_t   terminus_length, plex_length, soft_length, hand_length;
    uint8_t *terminus = file_of(&TerminusTTFMedium12, &terminus_length), *plex = file_of(&IBMPlexMonoMedium28, &plex_length);
    uint8_t *soft = file_of(&soft_font, &soft_length), *hand = file_of(&font0ss, &hand_length);
    static const char terminus_names[] = "font f TerminusTTFMedium12\n", hand_names[] = "font f font0ss handwritten\nhand handwriting\n";
    preview_on_file(terminus_names, "TerminusTTFMedium12", terminus, terminus_length, false);
    CHECK(apart_from(drawn_on(&TerminusTTFMedium12, false)) == 0 && strstr(preview_notes(), "font f") == NULL);
    /* a file wins over the font built in of its name: the project's font is the board's */
    preview_on_file(terminus_names, "TerminusTTFMedium12", plex, plex_length, false);
    CHECK(apart_from(drawn_on(&IBMPlexMonoMedium28, false)) == 0 && strstr(preview_notes(), "font f") == NULL);
    preview_font("TerminusTTFMedium12", NULL, 0);
    /* by the rule, a font of 4 bits */
    preview_on_file(NULL, "dgx_font_f", soft, soft_length, false);
    CHECK(apart_from(drawn_on(&soft_font, false)) == 0 && strstr(preview_notes(), "font f") == NULL);
    /* given anew for its name, the page on the new one, the old freed after */
    CHECK(preview_font("dgx_font_f", terminus, terminus_length)->code == 0 && preview_paint(1100) == 1);
    CHECK(apart_from(drawn_on(&TerminusTTFMedium12, false)) == 0);
    /* let go: the stand-in again, said */
    CHECK(preview_font("dgx_font_f", NULL, 0)->code == 0 && strstr(preview_notes(), "font f: no file for dgx_font_f, TerminusTTFMedium12 stands in"));
    /* a handwritten font, through the library's hand */
    preview_on_file(hand_names, "font0ss", hand, hand_length, true);
    CHECK(apart_from(drawn_on(&font0ss, true)) == 0 && strstr(preview_notes(), "font f") == NULL);
    /* the names' kind against the file's: said, and the loader follows the file */
    static const char unmarked[] = "font f font0ss\n", marked[] = "font f TerminusTTFMedium12 handwritten\nhand handwriting\n";
    CHECK(preview_names(unmarked, sizeof unmarked - 1)->code == 0);
    const preview_verdict_t *no_hand = preview_appearance(font_look, sizeof font_look - 1);
    CHECK(no_hand->code == DGX_UI_LOAD_MISSING && strcmp(no_hand->name, "hand") == 0);
    CHECK(strstr(preview_notes(), "font f: the names file does not call it handwritten, its file is a handwritten font") != NULL);
    CHECK(preview_names(marked, sizeof marked - 1)->code == 0 && preview_font("TerminusTTFMedium12", terminus, terminus_length)->code == 0);
    CHECK(preview_appearance(font_look, sizeof font_look - 1)->code == 0);
    CHECK(strstr(preview_notes(), "font f: the names file calls it handwritten, its file is a bitmap font") != NULL);
    CHECK(preview_names(NULL, 0)->code == 0 && preview_font("dgx_font_f", hand, hand_length)->code == 0);
    CHECK(strstr(preview_notes(), "font f: taken for a bitmap font by the rule, its file is a handwritten font") != NULL);
    preview_font("dgx_font_f", NULL, 0), preview_font("font0ss", NULL, 0), preview_font("TerminusTTFMedium12", NULL, 0);
    /* names refused in words, never cut */
    char long_name[80];
    memset(long_name, 'n', 64), long_name[64] = 0;
    const preview_verdict_t *named = preview_font(long_name, soft, soft_length);
    CHECK(named->code == PREVIEW_FONT_REFUSED && strstr(named->said, "a name of C of 64 bytes: 1 to 63 are taken") != NULL);
    long_name[63] = 0;
    CHECK(preview_font(long_name, soft, soft_length)->code == 0 && preview_font(long_name, NULL, 0)->code == 0);
    CHECK(preview_font("", soft, soft_length)->code == PREVIEW_FONT_REFUSED);
    /* spoiled at every byte, cut at every length: a verdict, never a fault; a spoiled file refused keeps the one before */
    CHECK(preview_appearance(font_look, sizeof font_look - 1)->code == 0 && preview_page(font_page, sizeof font_page - 1)->code == 0);
    CHECK(preview_font("dgx_font_f", soft, soft_length)->code == 0);
    int refused = 0, taken = 0;
    for (size_t at = 0; at < soft_length; ++at)
        for (int way = 0; way < 3; ++way) {
            uint8_t spoiled[4096];
            memcpy(spoiled, soft, soft_length);
            spoiled[at] = way == 0 ? 0xff : way == 1 ? 0 : (uint8_t)(spoiled[at] + 1);
            const preview_verdict_t *said = preview_font("dgx_font_f", spoiled, soft_length);
            CHECK(said->code == 0 || (said->code == PREVIEW_FONT_REFUSED && strncmp(said->said, "dgx_font_f: ESP_ERR_", 20) == 0));
            said->code ? ++refused : ++taken;
            preview_paint(2000 + (uint32_t)at);
        }
    for (size_t length = 0; length < soft_length; ++length) CHECK(preview_font("dgx_font_f", soft, length)->code == PREVIEW_FONT_REFUSED);
    CHECK(refused > 0 && taken > 0);
    for (size_t at = 0; at < terminus_length; at += 7) {
        uint8_t *spoiled = malloc(terminus_length);
        memcpy(spoiled, terminus, terminus_length), spoiled[at] ^= 0x5a;
        preview_font("dgx_font_f", spoiled, terminus_length), preview_paint(3000);
        free(spoiled);
    }
    preview_font("dgx_font_f", NULL, 0);
    free(terminus), free(plex), free(soft), free(hand);

    /* spoiled at a byte: a verdict, never a fault */
    const char *texts[]   = {page, look, file, home_page};
    size_t      lengths[] = {sizeof page - 1, sizeof look - 1, sizeof file - 1, sizeof home_page - 1};
    static const char bytes[] = "\"{}[],:#0a1 x.-\n&";
    for (int round = 0; round < 1500; ++round) {
        const int which = (int)(next_chance() % 4);
        char      spoiled[1024];
        memcpy(spoiled, texts[which], lengths[which]);
        spoiled[next_chance() % lengths[which]] = bytes[next_chance() % (sizeof bytes - 1)];
        if (which == 1) preview_appearance(spoiled, lengths[which]), preview_appearance_format(), preview_page(themed, sizeof themed - 1);
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
