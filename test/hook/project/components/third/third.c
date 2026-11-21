#include "third.h"

#include "dgx_ui.h"
#include "third_page.h"

int third_count(void)
{
    static dgx_ui_node_t nodes[4];
    dgx_ui_t             ui;
    dgx_ui_init(&ui, nodes, 4, 1);
    const int count = third_page_add(&ui, -1) == 0 ? ui.count : -1;
    dgx_ui_free(&ui);
    return count;
}
