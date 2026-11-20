# thin_paint: the first picture of dgx-ui, a frame with a value that changes and two lines under it, drawn by the
# painter only where they change. For the linux target of ESP-IDF, in a window of DGX on the computer.
cmake_minimum_required(VERSION 3.16)
# dgx-ui as it is in this checkout, whatever its directory is named
set(EXTRA_COMPONENT_DIRS ../..)
if(IDF_TARGET STREQUAL "linux")
    get_filename_component(dgx_ui_name ../.. ABSOLUTE)
    get_filename_component(dgx_ui_name "${dgx_ui_name}" NAME)
    set(COMPONENTS main ${dgx_ui_name})
    set(CMAKE_EXE_LINKER_FLAGS "-B/usr/bin/") # the linker of the computer, not that of a chip first in PATH
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(thin_paint)
