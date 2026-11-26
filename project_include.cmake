# dgx-ui: descriptions of dgx_ui_load.h made C at build time, appearances of dgx_ui_appearance.h too, and the loader's
# tables of a names file.
#
# ESP-IDF includes this file before the CMakeLists of every component, so a component, the program's main among them,
# asks for them in its own CMakeLists, after idf_component_register():
#
#     dgx_ui_names(vocab.txt HEADERS program.h)                        # vocab.c, vocab.h: const dgx_ui_load_names_t vocab_names
#     dgx_ui_tree(settings.json NAMES vocab.txt HEADERS program.h)     # settings.c, settings.h: <name>_add() of the tree named inside
#     dgx_ui_tree(pages/sound.json PREFIX volume NAMES vocab.txt)      # sound.c, sound.h: volume_add()
#     dgx_ui_appearance(day.json NAMES vocab.txt)                       # day.c, day.h: <name>_appearance of the one named inside
#     dgx_ui_tree(settings.json APPEARANCE day.json NAMES vocab.txt)   # its looks and styles the appearance's first
#     dgx_ui_names(vocab.txt APPEARANCE day.json)                      # vocab_names with the appearance's first
#
# An appearance is made before the trees and tables made on it, in their component or in one it requires: they name
# its arrays and are compiled against its header, whose stamp of its names holds them to the very appearance.
# Each is made by tools/dgx_ui_c (tools/README.md) into the build directory of the component that asks, compiled into
# it, and its header is on the component's own include path, with the directory of each header HEADERS names: the
# program's headers that declare what the names file's expressions lead to. A tree's files are named by PREFIX, else
# by the description's file; its C by PREFIX, else by the tree's name inside it, which the tool reads when it makes
# them, so CMake opens no description and an edit of one makes its files again with no new configure. The tables'
# files and C are named by PREFIX, else by the names file's own name, which must then be a name of C.
#
# The tool is built once for the project, from its source in dgx-ui and DGX's headers, by the computer's C compiler
# (tools/dgx_ui_c.py): a project that asks for nothing builds nothing and needs nothing of the computer. It is built
# again, and every file made again after it, when its source changes. Paths are taken from the CMakeLists that gives
# them.

set(DGX_UI_COMPONENT_DIR "${COMPONENT_DIR}")

function(_dgx_ui_absolute out path)
    get_filename_component(abs "${path}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
    set(${out} "${abs}" PARENT_SCOPE)
endfunction()

# the tool, its rule made by the first call of the project; its path, and the target that builds it
function(_dgx_ui_c_rule out)
    get_property(binary GLOBAL PROPERTY DGX_UI_C_BINARY)
    if(NOT binary)
        idf_build_get_property(python PYTHON)
        idf_component_get_property(dgx_dir jef-sure__dgx COMPONENT_DIR)
        set(dir "${CMAKE_BINARY_DIR}/dgx_ui_c")
        set(binary "${dir}/dgx_ui_c")
        if(CMAKE_HOST_WIN32)
            set(binary "${binary}.exe")
        endif()
        set(tool_sources "${DGX_UI_COMPONENT_DIR}/tools/dgx_ui_c.py" "${DGX_UI_COMPONENT_DIR}/tools/dgx_ui_c.c" "${DGX_UI_COMPONENT_DIR}/tools/dgx_ui_names.c"
                         "${DGX_UI_COMPONENT_DIR}/tools/dgx_ui_names.h")
        foreach(name dgx_ui.c dgx_ui_text.c dgx_ui_rect.c dgx_ui_msg.c dgx_ui_load.c dgx_ui_json.c dgx_ui_read.c dgx_ui_appearance.c dgx_ui_params.c dgx_ui_scroll.c)
            list(APPEND tool_sources "${DGX_UI_COMPONENT_DIR}/src/${name}")
        endforeach()
        list(APPEND tool_sources "${dgx_dir}/src/vscreen/image_file.c")
        file(GLOB headers "${DGX_UI_COMPONENT_DIR}/include/*.h" "${DGX_UI_COMPONENT_DIR}/src/*.h")
        add_custom_command(OUTPUT "${binary}"
            COMMAND ${python} "${DGX_UI_COMPONENT_DIR}/tools/dgx_ui_c.py" "${dgx_dir}" "${dir}"
            DEPENDS ${tool_sources} ${headers} "${dgx_dir}/include/dgx_colors.h"
            COMMENT "dgx-ui: dgx_ui_c built for the computer"
            VERBATIM)
        add_custom_target(dgx_ui_c_built DEPENDS "${binary}")
        set_property(GLOBAL PROPERTY DGX_UI_C_BINARY "${binary}")
    endif()
    set(${out} "${binary}" PARENT_SCOPE)
endfunction()

# the files' name, once in the component's directory; the prefix, once in the project
function(_dgx_ui_claim call files prefix source)
    get_property(names DIRECTORY PROPERTY DGX_UI_MADE_NAMES)
    get_property(sources DIRECTORY PROPERTY DGX_UI_MADE_SOURCES)
    list(FIND names "${files}" at)
    if(NOT at EQUAL -1)
        list(GET sources ${at} other)
        message(FATAL_ERROR "${call}: ${files}.c and ${files}.h are made of ${other} already in this component: give one of them a PREFIX")
    endif()
    set_property(DIRECTORY APPEND PROPERTY DGX_UI_MADE_NAMES "${files}")
    set_property(DIRECTORY APPEND PROPERTY DGX_UI_MADE_SOURCES "${source}")
    if(prefix)
        get_property(prefixes GLOBAL PROPERTY DGX_UI_PREFIXES)
        get_property(owners GLOBAL PROPERTY DGX_UI_PREFIX_OWNERS)
        list(FIND prefixes "${prefix}" at)
        if(NOT at EQUAL -1)
            list(GET owners ${at} other)
            message(FATAL_ERROR "${call}: the prefix ${prefix} is taken already, by ${other}: two of it would collide at link")
        endif()
        set_property(GLOBAL APPEND PROPERTY DGX_UI_PREFIXES "${prefix}")
        set_property(GLOBAL APPEND PROPERTY DGX_UI_PREFIX_OWNERS "${source} of ${COMPONENT_NAME}")
    endif()
endfunction()

# a prefix of C that is not the libraries'
function(_dgx_ui_check_prefix call prefix what)
    if(NOT prefix MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "${call}: ${what} \"${prefix}\" is not a name of C${ARGN}")
    endif()
    if(prefix MATCHES "^dgx_")
        message(FATAL_ERROR "${call}: ${what} \"${prefix}\" begins with dgx_, as the libraries' names and headers do${ARGN}")
    endif()
endfunction()

# the pictures of files of DGX a names file names, picture <name> @<asset>: the files DGX's build makes of them, which the
# made C reads and waits for; the component asks for the targets of DGX's rules once they are all made (below)
function(_dgx_ui_pictures_of names_file depends_out)
    file(STRINGS "${names_file}" lines REGEX "^[ \t]*picture[ \t]+[^ \t#]+[ \t]+@")
    set(depends "")
    foreach(line IN LISTS lines)
        if(line MATCHES "^[ \t]*picture[ \t]+[^ \t#]+[ \t]+@([A-Za-z_][A-Za-z0-9_]*)")
            list(APPEND depends "${CMAKE_BINARY_DIR}/dgx_images/${CMAKE_MATCH_1}.dgxi")
        endif()
    endforeach()
    set_property(GLOBAL APPEND PROPERTY DGX_UI_PICTURE_USERS "${COMPONENT_LIB}")
    set_property(GLOBAL APPEND PROPERTY DGX_UI_PICTURE_NAMES "${names_file}")
    set(${depends_out} "${depends}" PARENT_SCOPE)
endfunction()

# the rule that makes two files, compiled into the component that asks
function(_dgx_ui_made call files source args depends headers comment)
    _dgx_ui_c_rule(binary)
    set(out "${CMAKE_CURRENT_BINARY_DIR}/dgx_ui_made")
    file(MAKE_DIRECTORY "${out}")
    set(include_dirs "${out}")
    set(header_args "")
    foreach(header IN LISTS headers)
        _dgx_ui_absolute(abs "${header}")
        get_filename_component(dir "${abs}" DIRECTORY)
        get_filename_component(name "${abs}" NAME)
        list(APPEND header_args -i "${name}")
        list(APPEND include_dirs "${dir}")
    endforeach()
    # the options first, the description last; the folder of DGX's files of pictures for a names file's @<asset>
    add_custom_command(OUTPUT "${out}/${files}.c" "${out}/${files}.h"
        COMMAND "${binary}" -d "${out}" -f "${files}" -g "${CMAKE_BINARY_DIR}/dgx_images" ${header_args} ${args}
        DEPENDS "${binary}" "${source}" ${depends}
        COMMENT "dgx-ui: ${comment}"
        VERBATIM)
    target_sources(${COMPONENT_LIB} PRIVATE "${out}/${files}.c")
    list(REMOVE_DUPLICATES include_dirs)
    target_include_directories(${COMPONENT_LIB} PRIVATE ${include_dirs})
    add_dependencies(${COMPONENT_LIB} dgx_ui_c_built)
endfunction()

# what a tree or tables made on an appearance take of it: the options of dgx_ui_c, the files it depends on, the
# directory of its header and its target, known once dgx_ui_appearance() made it
function(_dgx_ui_appearance_of call appearance args_out depends_out dir_out target_out)
    _dgx_ui_absolute(source "${appearance}")
    get_property(files GLOBAL PROPERTY "DGX_UI_APPEARANCE_FILES:${source}")
    if(NOT files)
        message(FATAL_ERROR "${call}: ${appearance} is not made by dgx_ui_appearance() before: call it in this component or in one it requires")
    endif()
    get_property(prefix GLOBAL PROPERTY "DGX_UI_APPEARANCE_PREFIX:${source}")
    get_property(names GLOBAL PROPERTY "DGX_UI_APPEARANCE_NAMES:${source}")
    get_property(dir GLOBAL PROPERTY "DGX_UI_APPEARANCE_DIR:${source}")
    get_property(target GLOBAL PROPERTY "DGX_UI_APPEARANCE_TARGET:${source}")
    set(args -a "${source}" -af "${files}")
    set(depends "${source}")
    if(prefix)
        list(APPEND args -ap "${prefix}")
    endif()
    if(names)
        list(APPEND args -an "${names}")
        list(APPEND depends "${names}")
    endif()
    set(${args_out} "${args}" PARENT_SCOPE)
    set(${depends_out} "${depends}" PARENT_SCOPE)
    set(${dir_out} "${dir}" PARENT_SCOPE)
    set(${target_out} "${target}" PARENT_SCOPE)
endfunction()

# the component of a call made on an appearance: its header on the include path, the appearance made first
function(_dgx_ui_on_appearance dir target)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${dir}")
    add_dependencies(${COMPONENT_LIB} ${target})
endfunction()

# An appearance made C: dgx_ui_appearance(<appearance> [PREFIX <prefix>] [NAMES <names file>] [HEADERS <header>...])
function(dgx_ui_appearance appearance)
    set(call "dgx_ui_appearance(${appearance})")
    cmake_parse_arguments(PARSE_ARGV 1 A "" "PREFIX;NAMES" "HEADERS")
    if(A_UNPARSED_ARGUMENTS OR A_KEYWORDS_MISSING_VALUES)
        list(JOIN A_UNPARSED_ARGUMENTS " " words)
        message(FATAL_ERROR "${call}: \"${words}${A_KEYWORDS_MISSING_VALUES}\" not understood: PREFIX <prefix>, NAMES <names file>, HEADERS <header>...")
    endif()
    _dgx_ui_absolute(source "${appearance}")
    set(args "")
    if(DEFINED A_PREFIX)
        _dgx_ui_check_prefix("${call}" "${A_PREFIX}" "PREFIX")
        set(files "${A_PREFIX}")
        list(APPEND args -p "${A_PREFIX}")
    else()
        get_filename_component(files "${source}" NAME_WLE)
        if(NOT files MATCHES "^[A-Za-z0-9_][A-Za-z0-9_.-]*$")
            message(FATAL_ERROR "${call}: \"${files}\" is no name for the files made: give the appearance a PREFIX")
        endif()
    endif()
    _dgx_ui_claim("${call}" "${files}" "${A_PREFIX}" "${appearance}")
    set(depends "")
    set(names "")
    if(DEFINED A_NAMES)
        _dgx_ui_absolute(names "${A_NAMES}")
        list(APPEND args -n "${names}")
        list(APPEND depends "${names}")
    endif()
    list(APPEND args -A "${source}")
    _dgx_ui_made("${call}" "${files}" "${source}" "${args}" "${depends}" "${A_HEADERS}" "${appearance} made C, ${files}.c and ${files}.h")
    # known to the trees and tables made on it: a target of its files, which their components wait for
    set(out "${CMAKE_CURRENT_BINARY_DIR}/dgx_ui_made")
    string(MAKE_C_IDENTIFIER "dgx_ui_appearance_${COMPONENT_NAME}_${files}" target)
    add_custom_target(${target} DEPENDS "${out}/${files}.c" "${out}/${files}.h")
    # one target builds the tool and one the files, the others wait for them: make runs a rule of each that names them
    add_dependencies(${target} dgx_ui_c_built)
    add_dependencies(${COMPONENT_LIB} ${target})
    set_property(GLOBAL PROPERTY "DGX_UI_APPEARANCE_FILES:${source}" "${files}")
    set_property(GLOBAL PROPERTY "DGX_UI_APPEARANCE_PREFIX:${source}" "${A_PREFIX}")
    set_property(GLOBAL PROPERTY "DGX_UI_APPEARANCE_NAMES:${source}" "${names}")
    set_property(GLOBAL PROPERTY "DGX_UI_APPEARANCE_DIR:${source}" "${out}")
    set_property(GLOBAL PROPERTY "DGX_UI_APPEARANCE_TARGET:${source}" "${target}")
endfunction()

# A description made C: dgx_ui_tree(<description> [PREFIX <prefix>] [NAMES <names file>] [APPEARANCE <appearance>]
# [HEADERS <header>...])
function(dgx_ui_tree description)
    set(call "dgx_ui_tree(${description})")
    cmake_parse_arguments(PARSE_ARGV 1 T "" "PREFIX;NAMES;APPEARANCE" "HEADERS")
    if(T_UNPARSED_ARGUMENTS OR T_KEYWORDS_MISSING_VALUES)
        list(JOIN T_UNPARSED_ARGUMENTS " " words)
        message(FATAL_ERROR "${call}: \"${words}${T_KEYWORDS_MISSING_VALUES}\" not understood: PREFIX <prefix>, NAMES <names file>, APPEARANCE <appearance>, HEADERS <header>...")
    endif()
    _dgx_ui_absolute(source "${description}")
    set(args "")
    if(DEFINED T_PREFIX)
        _dgx_ui_check_prefix("${call}" "${T_PREFIX}" "PREFIX")
        set(files "${T_PREFIX}")
        list(APPEND args -p "${T_PREFIX}")
    else()
        get_filename_component(files "${source}" NAME_WLE)
        if(NOT files MATCHES "^[A-Za-z0-9_][A-Za-z0-9_.-]*$")
            message(FATAL_ERROR "${call}: \"${files}\" is no name for the files made: give the tree a PREFIX")
        endif()
    endif()
    _dgx_ui_claim("${call}" "${files}" "${T_PREFIX}" "${description}")
    set(depends "")
    if(DEFINED T_NAMES)
        _dgx_ui_absolute(names "${T_NAMES}")
        list(APPEND args -n "${names}")
        list(APPEND depends "${names}")
        _dgx_ui_pictures_of("${names}" pictures)
        list(APPEND depends ${pictures})
    endif()
    if(DEFINED T_APPEARANCE)
        _dgx_ui_appearance_of("${call}" "${T_APPEARANCE}" appearance_args appearance_depends appearance_dir appearance_target)
        list(APPEND args ${appearance_args})
        list(APPEND depends ${appearance_depends})
    endif()
    list(APPEND args "${source}")
    _dgx_ui_made("${call}" "${files}" "${source}" "${args}" "${depends}" "${T_HEADERS}" "${description} made C, ${files}.c and ${files}.h")
    if(DEFINED T_APPEARANCE)
        _dgx_ui_on_appearance("${appearance_dir}" "${appearance_target}")
    endif()
endfunction()

# The loader's tables of a names file: dgx_ui_names(<names file> [PREFIX <prefix>] [APPEARANCE <appearance>]
# [HEADERS <header>...])
function(dgx_ui_names names_file)
    set(call "dgx_ui_names(${names_file})")
    cmake_parse_arguments(PARSE_ARGV 1 N "" "PREFIX;APPEARANCE" "HEADERS")
    if(N_UNPARSED_ARGUMENTS OR N_KEYWORDS_MISSING_VALUES)
        list(JOIN N_UNPARSED_ARGUMENTS " " words)
        message(FATAL_ERROR "${call}: \"${words}${N_KEYWORDS_MISSING_VALUES}\" not understood: PREFIX <prefix>, APPEARANCE <appearance>, HEADERS <header>...")
    endif()
    _dgx_ui_absolute(source "${names_file}")
    if(DEFINED N_PREFIX)
        set(prefix "${N_PREFIX}")
        _dgx_ui_check_prefix("${call}" "${prefix}" "PREFIX")
    else()
        get_filename_component(prefix "${source}" NAME_WLE)
        _dgx_ui_check_prefix("${call}" "${prefix}" "the names file's name" ": give its tables a PREFIX")
    endif()
    _dgx_ui_claim("${call}" "${prefix}" "${prefix}" "${names_file}")
    set(args -n "${source}")
    _dgx_ui_pictures_of("${source}" depends)
    if(DEFINED N_APPEARANCE)
        _dgx_ui_appearance_of("${call}" "${N_APPEARANCE}" appearance_args depends appearance_dir appearance_target)
        list(APPEND args ${appearance_args})
    endif()
    list(APPEND args -t "${prefix}")
    _dgx_ui_made("${call}" "${prefix}" "${source}" "${args}" "${depends}" "${N_HEADERS}" "${names_file} made the loader's tables, ${prefix}.c and ${prefix}.h")
    if(DEFINED N_APPEARANCE)
        _dgx_ui_on_appearance("${appearance_dir}" "${appearance_target}")
    endif()
endfunction()

# The components whose made C reads pictures of files wait for DGX's rules that make the files and embed them,
# dgx_images_made and dgx_assets_made; and a names file of theirs that changes configures the project again, so that a
# line of an asset added is seen. DGX creates those rules in calls deferred to the end of the configure step, and which
# of the two files is included first is not to be relied on: this call is deferred once more from within a deferred
# one, so that it runs after every call deferred meanwhile, DGX's among them.
function(_dgx_ui_pictures_finish)
    get_property(users GLOBAL PROPERTY DGX_UI_PICTURE_USERS)
    get_property(names GLOBAL PROPERTY DGX_UI_PICTURE_NAMES)
    if(NOT users OR NOT TARGET dgx_images_made) # no picture made of a file in this project: nothing to wait for
        return()
    endif()
    list(REMOVE_DUPLICATES users)
    foreach(lib IN LISTS users)
        foreach(rule dgx_images_made dgx_assets_made)
            if(TARGET ${rule})
                add_dependencies(${lib} ${rule})
            endif()
        endforeach()
    endforeach()
    set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${names})
endfunction()
function(_dgx_ui_pictures_after_all)
    cmake_language(DEFER CALL _dgx_ui_pictures_finish)
endfunction()
cmake_language(DEFER CALL _dgx_ui_pictures_after_all)
