# hello: the smallest program of dgx-ui, a text and a count another task sends once a second. For a CYD
# (ESP32-2432S028R), its ILI9341 standing (240x320), and for a window of DGX on the computer, by the linux target of
# ESP-IDF. A project of your own starts from it.
cmake_minimum_required(VERSION 3.16)
# Inside the repository of dgx-ui the component is the checkout this example lies in, under the name the manifest gives
# it: a folder of that name in the build's folder, as ESP-IDF names a component after its folder. The folder holds a link
# to each file and folder of the checkout but the examples and the outputs of builds; a link to the checkout as a whole
# would be followed to the checkout's own name, and the component would come in twice. A project made outside has no
# dgx-ui above it and takes the one main/idf_component.yml names, into managed_components/.
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../../include/dgx_ui.h)
    get_filename_component(dgx_ui ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
    set(component_dir ${CMAKE_BINARY_DIR}/jef-sure__dgx-ui)
    file(MAKE_DIRECTORY ${component_dir})
    file(GLOB entries RELATIVE ${dgx_ui} ${dgx_ui}/*)
    foreach(entry ${entries})
        if(NOT entry MATCHES "^(examples|build|managed_components)$" AND NOT EXISTS ${component_dir}/${entry})
            file(CREATE_LINK ${dgx_ui}/${entry} ${component_dir}/${entry} SYMBOLIC)
        endif()
    endforeach()
    list(APPEND EXTRA_COMPONENT_DIRS ${component_dir})
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# the computer: only main and what it asks, as not every component of ESP-IDF builds for it. After project.cmake, which
# takes the target from sdkconfig when the build's folder is made anew and nothing else names it
if(IDF_TARGET STREQUAL "linux")
    set(COMPONENTS main)
    set(CMAKE_EXE_LINKER_FLAGS "-B/usr/bin/") # the linker of the computer, not that of a chip first in PATH
endif()
project(hello)
