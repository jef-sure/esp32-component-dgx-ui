# hook: a program for the linux target of ESP-IDF that asks project_include.cmake of dgx-ui for trees made C and the
# loader's tables of a names file, in main and in a second component; test_hook.py builds it, changes it and runs it.
# dgx-ui is the copy test_hook.py puts into ext/dgx-ui, of a checkout or of a package; DGX comes by its manifest.
cmake_minimum_required(VERSION 3.16)
# the computer: only main and what it asks; the linker of the computer, not that of a chip first in PATH
if("${IDF_TARGET}" STREQUAL "linux")
    set(COMPONENTS main)
    set(CMAKE_EXE_LINKER_FLAGS "-B/usr/bin/")
endif()
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/ext)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hook)
