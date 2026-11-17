# controls: the controls of dgx-ui worked by a finger, the keys of the board and a keyboard on the display: a button
# that opens a popup, a switch, a slider, a field to type into. The board is chosen by tools/board_select.py of
# jef-sure/boards: the CYD (cyd_s028r), and the computer showing its display in a window (host_cyd_s028r), the mouse its
# finger and the arrows and Enter of the keyboard its buttons.
cmake_minimum_required(VERSION 3.22)
# BOARD=<board>, which the preset of a board sets, builds with sdkconfig.defaults.<board> that board_select.py writes,
# then with the program's own sdkconfig.defaults
set(BOARD "" CACHE STRING "a board of this project: its file sdkconfig.defaults.<board>")
if(NOT SDKCONFIG_DEFAULTS AND BOARD)
    if(NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.${BOARD})
        message(FATAL_ERROR "no sdkconfig.defaults.${BOARD} here: run board_select.py ${BOARD} . of jef-sure/boards")
    endif()
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults.${BOARD}")
    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults)
        list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults")
    endif()
elseif(NOT BOARD AND NOT SDKCONFIG)
    set(SDKCONFIG ${CMAKE_BINARY_DIR}/sdkconfig)
endif()
# dgx-ui as it is in this checkout, whatever its directory is named
set(EXTRA_COMPONENT_DIRS ../..)
# the computer (BOARD=host_<board>, the linux target of ESP-IDF): only main, dgx-ui and what they ask
if(BOARD MATCHES "^host_")
    get_filename_component(dgx_ui_name ../.. ABSOLUTE)
    get_filename_component(dgx_ui_name "${dgx_ui_name}" NAME)
    set(COMPONENTS main ${dgx_ui_name})
    set(CMAKE_EXE_LINKER_FLAGS "-B/usr/bin/") # the linker of the computer, not that of a chip first in PATH
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if(BOARD)
    idf_build_set_property(DEPENDENCIES_LOCK ${CMAKE_CURRENT_LIST_DIR}/dependencies.lock.${BOARD})
endif()
project(controls)
