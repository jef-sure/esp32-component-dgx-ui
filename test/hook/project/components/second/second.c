#include "second.h"

#include "c.h"
#include "dgx_ui.h"

int32_t level = 3;

int second_count(void)
{
    static dgx_ui_node_t nodes[4];
    dgx_ui_t             ui;
    dgx_ui_init(&ui, nodes, 4, 1);
    const int count = second_page_add(&ui, -1) == 0 ? ui.count : -1;
    dgx_ui_free(&ui);
    return count;
}
