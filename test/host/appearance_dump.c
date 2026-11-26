/*
 * The descriptions of the library's draws, of a text style and of a variant of a font, printed as JSON for
 * test_generate_appearance.py, which makes appearances by chance of them: the draws by the list dgx_ui_c is built
 * with, library_draws.h of its build, so that the test and the tool know the same draws.
 */
#include <stdio.h>
#include <string.h>

#include "dgx_ui_appearance.h"

#define DRAW(word, type) {#word, #type, &dgx_ui_params_##word},
#define COUNT(type, member, count)
static const struct {
    const char            *word, *type;
    const dgx_ui_params_t *params;
} draws[] = {
#include "library_draws.h"
};

static void print_params(const dgx_ui_params_t *described)
{
    printf("{\"size\": %u, \"kinds\": %u, \"fields\": [", (unsigned)described->size, (unsigned)described->kinds);
    for (int k = 0; k < described->field_count; ++k) {
        const dgx_ui_field_t *field = &described->fields[k];
        printf("%s{\"name\": \"%s\", \"kind\": %d, \"size\": %d, \"low\": %d, \"high\": %d, \"words\": [", k ? ", " : "", field->name, field->kind, field->size,
               (int)field->low, (int)field->high);
        int place = 0;
        for (const char *word = field->kind == DGX_UI_FIELD_PART ? NULL : field->words; word && *word; word += strlen(word) + 1, ++place) printf("%s\"%s\"", place ? ", " : "", word);
        printf("]}");
    }
    printf("]}");
}

int main(void)
{
    printf("{\"draws\": {");
    for (size_t k = 0; k < sizeof draws / sizeof *draws; ++k) printf("%s\"%s\": ", k ? ", " : "", draws[k].word), print_params(draws[k].params);
    printf("}, \"style\": "), print_params(&dgx_ui_params_text_style);
    printf(", \"variant\": "), print_params(&dgx_ui_params_font_variant);
    printf("}\n");
    return 0;
}
