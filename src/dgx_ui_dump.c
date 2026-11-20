/*
 * dgx_ui_dump.c
 *
 *  A tree written to the log: a line a widget, indented two spaces a level, then each popup over it the same way;
 *  under each tree how many of its nodes are in use and whether its array was large enough. Nothing in the tree is
 *  changed. A file of its own: a program that does not call it does not carry it.
 *
 *  Copyright (c) 2026 Anton Petrusevich
 *      Author: Anton Petrusevich
 */

#include <stdio.h>
#include <string.h>

#include "dgx_ui_priv.h"
#include "dgx_ui_text.h"
#include "esp_log.h"

/* a line of it: by ESP_LOGI where ESP-IDF builds it (a chip, its linux target); on a computer built by make, whose
   log stub writes no line of information, to stderr in the form of the log, so that a test of a tree there sees it */
#ifdef ESP_PLATFORM
static const char *TAG = "dgx_ui";
#define SAY(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#else
#define SAY(fmt, ...) fprintf(stderr, "I dgx_ui: " fmt "\n", ##__VA_ARGS__)
#endif

static const char *const kinds[] = {"BOX", "FRAME", "LABEL", "PICTURE", "ICON", "LIST", "SWITCH", "SLIDER", "BUTTON", "INPUT", "DROPDOWN", "OWN", "RADIO"};
static const char *const bit_names[]  = {"HIDDEN", "DISABLED", "FOCUSED", "PRESSED"};
static const char *const mark_names[] = {"CHANGED", "WHOLE", "FIT", "HARD_SIZE", "TIME", "AROUND", "BELOW", "WINDOW"};

/* the names of the bits set, joined by '|'; "-" none */
static const char *names_of(unsigned bits, const char *const *names, int count, char *out, size_t size)
{
    out[0] = 0;
    for (int k = 0; k < count; ++k) {
        if (!(bits & (1u << k))) continue;
        if (out[0]) strncat(out, "|", size - strlen(out) - 1);
        strncat(out, names[k], size - strlen(out) - 1);
    }
    if (!out[0]) strncpy(out, "-", size);
    return out;
}

/* the first 16 bytes of a text, in whole letters, its line ends written \n, and "..." when there is more */
static const char *start_of(const char *text, char *out)
{
    size_t k = 0, o = 0;
    for (; text[k] && k < 16; ++k) {
        if (text[k] == '\n') out[o++] = '\\', out[o++] = 'n';
        else out[o++] = text[k];
    }
    while (k > 0 && text[k] && ((unsigned char)text[k] & 0xC0) == 0x80) --k, --o; /* not in the middle of a letter */
    if (text[k]) memcpy(out + o, "...", 3), o += 3;
    out[o] = 0;
    return out;
}

static void line_of(const dgx_ui_t *ui, int i, int depth)
{
    const dgx_ui_node_t   *n = dgx_ui_node(ui, i);
    const dgx_ui_widget_t *w = n->widget;
    char                   bits[48], marks[96], text[40];
    const char            *kind = w->kind < sizeof kinds / sizeof *kinds ? kinds[w->kind] : "?";
    names_of(n->state.bits, bit_names, 4, bits, sizeof bits);
    names_of(n->marks, mark_names, 8, marks, sizeof marks);
    if (w->text && w->text->buf)
        SAY("%*s#%d %s %d,%d %dx%d v=%d bits=%s marks=%s tag=%d \"%s\"", 2 * depth, "", i, kind, n->x, n->y, n->w, n->h,
                 dgx_ui_value_of(n), bits, marks, w->tag, start_of(w->text->buf, text));
    else
        SAY("%*s#%d %s %d,%d %dx%d v=%d bits=%s marks=%s tag=%d", 2 * depth, "", i, kind, n->x, n->y, n->w, n->h, dgx_ui_value_of(n),
                 bits, marks, w->tag);
    for (int c = n->child; c >= 0; c = dgx_ui_node(ui, c)->next) line_of(ui, c, depth + 1);
}

/* a tree's widgets from its root, then how many of its nodes it uses and what its array should hold */
static void tree_of(const dgx_ui_t *ui, int depth)
{
    if (dgx_ui_widget(ui, 0)) line_of(ui, 0, depth);
    int used = 0;
    for (int i = 0; i < ui->count; ++i) used += dgx_ui_widget(ui, i) != NULL;
    SAY("%*snodes: %d in use, %d of the array of %d, blocks: %d", 2 * depth, "", used, ui->count, ui->first_size, ui->block_count);
    if (ui->block_count) SAY("%*sgive the array %d nodes and no block is needed", 2 * depth, "", ui->count);
}

void dgx_ui_dump(const dgx_ui_t *ui)
{
    tree_of(ui, 0);
    int level = 0;
    for (const dgx_ui_t *t = ui->popup; t; t = t->popup) {
        SAY("popup %d at %d,%d %dx%d", ++level, t->root_rect.x, t->root_rect.y, t->root_rect.w, t->root_rect.h);
        tree_of(t, 1);
    }
}
