idf_component_register(SRCS "second.c" INCLUDE_DIRS "." REQUIRES dgx-ui)
dgx_ui_tree(c.json NAMES words/names.txt HEADERS second.h)   # c.c, c.h in this component: the tree's name inside, second_page_add()
# a case test_hook.py configures, refused: the prefix main's b.json has
if(HOOK_CASE STREQUAL "prefix_twice")
    dgx_ui_tree(c.json PREFIX other NAMES words/names.txt HEADERS second.h)
endif()
