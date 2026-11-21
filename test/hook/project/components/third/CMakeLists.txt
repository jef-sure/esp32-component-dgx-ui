# a third component that asks for a tree and does not depend on the second: its rule waits for the tool by its target
idf_component_register(SRCS "third.c" INCLUDE_DIRS "." REQUIRES dgx-ui)
dgx_ui_tree(d.json PREFIX third_page)   # third_page.c, third_page.h: by the rule, no names file
