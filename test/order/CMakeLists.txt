# The order of the deferred calls of project_include.cmake of dgx-ui and of DGX, on CMake alone (test_order.py): dgx-ui's
# file included first, DGX's rules of pictures made by a call deferred after it, as when ESP-IDF includes dgx-ui's file
# before DGX's. The component's made C, here a copy of the file of the picture, waits for DGX's rule all the same.
cmake_minimum_required(VERSION 3.19)
project(order NONE)
set(COMPONENT_DIR "${DGX_UI}")
include("${DGX_UI}/project_include.cmake")

# the component, as dgx_ui_names() leaves it: its made C read of the file of @logo
set(COMPONENT_LIB main_lib)
add_custom_target(main_lib ALL)
_dgx_ui_pictures_of("${CMAKE_CURRENT_LIST_DIR}/names.txt" depends)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/made.c"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/dgx_images/logo.dgxi" "${CMAKE_BINARY_DIR}/made.c"
    DEPENDS ${depends} VERBATIM)
add_custom_target(main_made ALL DEPENDS "${CMAKE_BINARY_DIR}/made.c")
add_dependencies(main_lib main_made)

# DGX's rule, deferred after dgx-ui's call was: the file of the picture, a little late, and its target
function(_dgx_images_finish)
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/dgx_images/logo.dgxi"
        COMMAND ${CMAKE_COMMAND} -E sleep 1
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/logo.txt" "${CMAKE_BINARY_DIR}/dgx_images/logo.dgxi"
        DEPENDS "${CMAKE_CURRENT_LIST_DIR}/logo.txt" VERBATIM)
    add_custom_target(dgx_images_made DEPENDS "${CMAKE_BINARY_DIR}/dgx_images/logo.dgxi")
endfunction()
cmake_language(DEFER CALL _dgx_images_finish)

# what the component waits for, once every deferred call ran
function(_order_told)
    get_property(deps TARGET main_lib PROPERTY MANUALLY_ADDED_DEPENDENCIES)
    file(WRITE "${CMAKE_BINARY_DIR}/waits.txt" "${deps}")
endfunction()
function(_order_later)
    cmake_language(DEFER CALL _order_later_still)
endfunction()
function(_order_later_still)
    cmake_language(DEFER CALL _order_told)
endfunction()
cmake_language(DEFER CALL _order_later)
