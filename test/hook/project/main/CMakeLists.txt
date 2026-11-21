idf_component_register(SRCS "main.c" INCLUDE_DIRS "." REQUIRES dgx-ui second third)
# where the program reads a.json at run time, to load it beside the tree made of it
target_compile_definitions(${COMPONENT_LIB} PRIVATE PAGES_DIR="${CMAKE_CURRENT_LIST_DIR}/pages")

dgx_ui_names(vocab.txt HEADERS headers/program.h)                            # vocab.c, vocab.h: vocab_names
dgx_ui_tree(pages/a.json NAMES vocab.txt HEADERS headers/program.h)           # a.c, a.h: the tree's name inside, sound_add()
dgx_ui_tree(b.json PREFIX other NAMES vocab.txt HEADERS headers/program.h)    # other.c, other.h: other_add()

# the cases test_hook.py configures, each refused
if(HOOK_CASE STREQUAL "files_twice")
    dgx_ui_tree(more/a.json NAMES vocab.txt HEADERS headers/program.h)
elseif(HOOK_CASE STREQUAL "prefix_not_c")
    dgx_ui_tree(b.json PREFIX 1x NAMES vocab.txt)
elseif(HOOK_CASE STREQUAL "prefix_dgx")
    dgx_ui_tree(b.json PREFIX dgx_x NAMES vocab.txt)
elseif(HOOK_CASE STREQUAL "names_not_c")
    dgx_ui_names(my-vocab.txt)
elseif(HOOK_CASE STREQUAL "file_no_name")
    dgx_ui_tree("odd page.json" NAMES vocab.txt)
elseif(HOOK_CASE STREQUAL "file_dash")
    dgx_ui_tree(-page.json NAMES vocab.txt)
elseif(HOOK_CASE STREQUAL "word")
    dgx_ui_tree(b.json PREFX other2)
elseif(HOOK_CASE STREQUAL "refused")
    dgx_ui_tree(bad.json PREFIX bad NAMES vocab.txt)
endif()
