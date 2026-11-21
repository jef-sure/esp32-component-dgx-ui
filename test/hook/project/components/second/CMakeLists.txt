idf_component_register(SRCS "second.c" INCLUDE_DIRS "." REQUIRES dgx-ui)
dgx_ui_appearance(look.json NAMES words/names.txt HEADERS second.h)   # look.c, look.h: theme_appearance, of the name inside
dgx_ui_tree(c.json NAMES words/names.txt APPEARANCE look.json HEADERS second.h)   # c.c, c.h: second_page_add(), its slider the appearance's
# a case test_hook.py configures, refused: the prefix main's b.json has
if(HOOK_CASE STREQUAL "prefix_twice")
    dgx_ui_tree(c.json PREFIX other NAMES words/names.txt HEADERS second.h)
endif()
