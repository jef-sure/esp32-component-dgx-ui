/*
 * scene <scene file> <out file>: a scene played on the preview built for the computer, as scene.js plays it on the
 * module built to WebAssembly, each writing what came of it in the same form, so that the two files are held equal byte
 * for byte (make_scenes.py). A scene, a command a line, the texts in files beside it:
 *
 *     screen <w> <h> <depth> <pixels a point>     names <file>|-     appearance <file>|-     page <file>
 *     pagefile <name> <file>     paint <ms>     pointer <x> <y> <down> <ms>     key <key>     buttons <held> <ms>
 *     value <name> <v>     dump
 *
 * A verdict, a paint's answer, a value's, and at dump the widgets, the events, the notes, the rectangle written and the
 * frame's bytes are written out.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgx_ui_preview.h"

static char  directory[1024];
static FILE *out;

static struct {
    char   name[24];
    char  *text;
    size_t length;
} pages[16];
static int page_count;

static char *read_file(const char *name, size_t *length)
{
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", directory, name);
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "scene: %s: no such file\n", path);
        exit(2);
    }
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (!text || fread(text, 1, (size_t)size, file) != (size_t)size) exit(2);
    fclose(file);
    *length = (size_t)size;
    return text;
}

size_t preview_page_text(const char *name, char *text_out, size_t size)
{
    for (int k = 0; k < page_count; ++k)
        if (strcmp(pages[k].name, name) == 0) {
            if (text_out && size >= pages[k].length) memcpy(text_out, pages[k].text, pages[k].length);
            return pages[k].length;
        }
    return 0;
}

static void verdict(const char *what, const preview_verdict_t *said)
{
    fprintf(out, "%s %d %d [%s] [%s]\n", what, (int)said->code, (int)said->offset, said->name, said->said);
}

/* lines of the preview's text, each led */
static void lines(const char *lead, const char *text)
{
    for (const char *line = text; *line;) {
        const char *end = strchr(line, '\n');
        const int   length = end ? (int)(end - line) : (int)strlen(line);
        fprintf(out, "%s %.*s\n", lead, length, line);
        line += length + (end != NULL);
    }
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "scene <scene file> <out file>\n");
        return 2;
    }
    snprintf(directory, sizeof directory, "%s", argv[1]);
    char *slash = strrchr(directory, '/');
    if (slash) *slash = 0;
    else strcpy(directory, ".");
    FILE *scene = fopen(argv[1], "r");
    out         = fopen(argv[2], "wb");
    if (!scene || !out) return 2;
    char line[1024];
    int  width = 0, height = 0;
    while (fgets(line, sizeof line, scene)) {
        char   word[32] = "", a[256] = "", b[256] = "";
        int    x = 0, y = 0, z = 0, w = 0;
        size_t length;
        if (sscanf(line, "%31s", word) != 1 || word[0] == '#') continue;
        if (strcmp(word, "screen") == 0 && sscanf(line, "%*s %d %d %d %d", &x, &y, &z, &w) == 4) {
            const int said = preview_screen(x, y, z, w);
            if (said == 0) width = x, height = y;
            fprintf(out, "screen %d\n", said);
        } else if (strcmp(word, "names") == 0 || strcmp(word, "appearance") == 0 || strcmp(word, "page") == 0) {
            sscanf(line, "%*s %255s", a);
            char *text = strcmp(a, "-") == 0 ? NULL : read_file(a, &length);
            const preview_verdict_t *said = word[0] == 'n' ? preview_names(text, text ? length : 0)
                                            : word[0] == 'a' ? preview_appearance(text, text ? length : 0)
                                                             : preview_page(text, text ? length : 0);
            verdict(word, said);
            free(text);
        } else if (strcmp(word, "pagefile") == 0 && sscanf(line, "%*s %23s %255s", a, b) == 2 && page_count < 16) {
            snprintf(pages[page_count].name, sizeof pages[page_count].name, "%s", a);
            pages[page_count].text = read_file(b, &pages[page_count].length);
            ++page_count;
        } else if (strcmp(word, "paint") == 0 && sscanf(line, "%*s %d", &x) == 1) {
            fprintf(out, "paint %d\n", preview_paint((uint32_t)x));
            fprintf(out, "wake %u\n", (unsigned)preview_wake((uint32_t)x));
        } else if (strcmp(word, "pointer") == 0 && sscanf(line, "%*s %d %d %d %d", &x, &y, &z, &w) == 4) {
            preview_pointer(x, y, z, (uint32_t)w);
        } else if (strcmp(word, "key") == 0 && sscanf(line, "%*s %d", &x) == 1) {
            preview_key(x);
        } else if (strcmp(word, "buttons") == 0 && sscanf(line, "%*s %d %d", &x, &y) == 2) {
            preview_buttons((uint32_t)x, (uint32_t)y);
        } else if (strcmp(word, "value") == 0 && sscanf(line, "%*s %255s %d", a, &x) == 2) {
            fprintf(out, "value %d\n", preview_value(a, x));
        } else if (strcmp(word, "dump") == 0) {
            const char *shown = preview_shown();
            fprintf(out, "shown %s[%s]\n", shown ? "" : "none ", shown ? shown : "");
            const int count = preview_count();
            fprintf(out, "count %d\n", count);
            for (int i = 0; i < count; ++i) {
                const preview_widget_t *widget = preview_widget(i);
                fprintf(out, "widget %d %d %d %d %d %d %d %d %u %d [%s]\n", i, (int)widget->kind, (int)widget->parent, (int)widget->x,
                        (int)widget->y, (int)widget->w, (int)widget->h, (int)widget->shown, (unsigned)widget->bits, (int)widget->value,
                        widget->text ? widget->text : "");
            }
            for (int k = 0; k < 4; ++k) {
                const int at = preview_widget_at((k + 1) * width / 5, (k + 1) * height / 5);
                fprintf(out, "at %d\n", at);
            }
            lines("event", preview_events());
            lines("note", preview_notes());
            const int32_t *changed = preview_changed();
            fprintf(out, "changed %d %d %d %d\n", (int)changed[0], (int)changed[1], (int)changed[2], (int)changed[3]);
            const uint8_t *frame = preview_frame();
            fprintf(out, "frame %d\n", frame ? width * height * 4 : 0);
            if (frame) fwrite(frame, 4, (size_t)width * (size_t)height, out);
            fputc('\n', out);
        } else {
            fprintf(stderr, "scene: not understood: %s", line);
            return 2;
        }
    }
    fclose(out);
    return 0;
}
