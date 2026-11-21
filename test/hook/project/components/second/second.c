#include "second.h"

#include "c.h"
#include "dgx_ui.h"
#include "look.h"

int32_t level = 3;

int second_count(void)
{
    static dgx_ui_node_t nodes[4];
    dgx_ui_t             ui;
    dgx_ui_init(&ui, nodes, 4, 1);
    int count = second_page_add(&ui, -1) == 0 ? ui.count : -1;
    if (count == 2 && dgx_ui_node(&ui, 1)->widget->look != &theme_look[1]) count = -1;
    /* the appearance's style a constant, its font there with nothing called first */
    if (count == 2 && theme_style[0].variants[0].font != &TerminusTTFMedium12) count = -1;
    dgx_ui_free(&ui);
    return count;
}
