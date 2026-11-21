# Host tests of dgx-ui, the part that runs without a display.
# Run: make -C test/host
#
# DGX is taken from the managed component of the project dgx-ui sits in, else from a checkout next to dgx-ui's
# (../esp32-component-dgx); DGX=<path> takes another copy.
ROOT   := ../..
DGX    ?= $(firstword $(wildcard $(ROOT)/../../managed_components/jef-sure__dgx) $(ROOT)/../esp32-component-dgx)
# DGX-DI for the test of its adapter, the same way; without it that test is left out
DGX_DI ?= $(firstword $(wildcard $(ROOT)/../../managed_components/jef-sure__dgx-di) $(wildcard $(ROOT)/../esp32-component-dgx-di))
CC     ?= gcc
CXX    ?= g++
CFLAGS ?= -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer
BUILD  := build

include $(DGX)/host/dgx_host.mk
include $(ROOT)/host/dgx_ui_host.mk
CPPFLAGS += $(DGX_HOST_CPPFLAGS) $(DGX_UI_HOST_CPPFLAGS)
# a name DGX 1.0 takes away, warned at by DGX, is an error here
WARN     := -Wall -Wextra -Wno-unused-parameter -Werror=deprecated-declarations

all: run-test_rect run-test_core run-test_msg run-test_paint run-test_around run-test_lines run-test_controls run-test_popup run-test_nest run-test_skip run-test_list run-test_dropdown run-test_indexed run-test_deep run-test_deep18 run-test_background run-test_canvas run-test_scroll run-test_dump run-test_wake run-test_own run-test_hand run-test_changed run-test_page run-test_transition run-test_hand_lines run-test_hand_lines_plain run-test_two_paints run-test_tasks run-test_stats run-test_stats_ways run-test_soft run-test_place run-test_once run-test_once18 run-test_radio run-test_radio18 run-test_random run-test_written run-test_written18 run-test_focus run-test_focus18 run-test_shown run-test_level run-test_level18 run-test_ring run-test_ring18 run-test_veil run-test_veil18 run-test_place_page run-test_place_page18 run-test_picture_button run-test_picture_button18 run-test_repeat run-test_repeat18 run-test_idle run-test_idle18 run-test_load run-test_json run-test_stack run-test_params run-test_appearance run-test_generate run-test_generate_appearance run-test_preview run-test_preview_module run-test_preview_class check-werror check-lists $(if $(DGX_DI),run-test_di) check-cxx

run-%: $(BUILD)/%
	$<

$(BUILD)/test_rect: test_rect.c check.h $(ROOT)/src/dgx_ui_rect.c $(ROOT)/include/dgx_ui.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) -o $@ test_rect.c $(ROOT)/src/dgx_ui_rect.c

CORE := $(ROOT)/src/dgx_ui.c $(ROOT)/src/dgx_ui_text.c $(ROOT)/src/dgx_ui_rect.c $(ROOT)/src/dgx_ui_msg.c

$(BUILD)/test_core: test_core.c check.h $(CORE) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) -o $@ test_core.c $(CORE)

$(BUILD)/test_msg: test_msg.c check.h $(CORE) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) -o $@ test_msg.c $(CORE)

# the painter draws with DGX: its sources and its stubs of ESP-IDF, as DGX gives them (host/dgx_host.mk)
PAINT     := $(CORE) $(ROOT)/src/dgx_ui_paint.c $(ROOT)/src/dgx_ui_looks.c $(ROOT)/src/dgx_ui_text_look.c $(ROOT)/src/dgx_ui_lines_look.c \
             $(addprefix $(ROOT)/src/, dgx_ui_button_look.c dgx_ui_picture_button_look.c dgx_ui_input_look.c dgx_ui_control_looks.c dgx_ui_check_looks.c dgx_ui_level_look.c dgx_ui_ring_look.c dgx_ui_list_look.c dgx_ui_dropdown_look.c dgx_ui_dropdown.c dgx_ui_picture_look.c dgx_ui_rgb565_look.c dgx_ui_indexed_look.c dgx_ui_strips.c dgx_ui_background.c dgx_ui_canvas.c dgx_ui_hand.c dgx_ui_hand_canvas.c dgx_ui_changed.c dgx_ui_page.c dgx_ui_transition.c dgx_ui_hand_lines.c dgx_ui_idle.c dgx_ui_load.c dgx_ui_load_actions.c dgx_ui_json.c dgx_ui_read.c dgx_ui_appearance.c dgx_ui_params.c)
DGX_SRCS  := $(DGX_HOST_SRCS_SCREEN) $(DGX_HOST_SRCS_DRAW) $(DGX_HOST_SRCS_VSCREEN) $(DGX_HOST_SRCS_TEXT) $(DGX_HOST_SRCS_MORPH) \
             $(DGX_HOST_SRCS_FB) $(DGX_ROOT)/src/fonts/TerminusTTFMedium12.c $(DGX_ROOT)/src/fonts/DejaVuSansBook32.c
DGX_FLAGS := -I$(DGX)/src -Wno-format -Wno-sign-compare

$(BUILD)/test_paint: test_paint.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_paint.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_around: test_around.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_around.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_lines: test_lines.c check.h panel.h composed.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_lines.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_controls: test_controls.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_controls.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_popup: test_popup.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_popup.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_nest: test_nest.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_nest.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_skip: test_skip.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_skip.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_list: test_list.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_list.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_dropdown: test_dropdown.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_dropdown.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_indexed: test_indexed.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_indexed.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_deep $(BUILD)/test_deep18: $(BUILD)/test_deep%: test_deep.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,24) -o $@ test_deep.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_background: test_background.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_background.c $(PAINT) $(DGX_SRCS) -lm

# the canvas's allocator made to fail by the test: DGX's dgx_vscreen_init() wrapped
$(BUILD)/test_canvas: test_canvas.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -Wl,--wrap=dgx_vscreen_init -o $@ test_canvas.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_scroll: test_scroll.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_scroll.c $(PAINT) $(DGX_SRCS) -lm

# the dump read back from the computer's stderr, built as a program's test of its tree is built
$(BUILD)/test_dump: test_dump.c check.h $(CORE) $(ROOT)/src/dgx_ui_dump.c $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) -o $@ test_dump.c $(CORE) $(ROOT)/src/dgx_ui_dump.c

$(BUILD)/test_changed: test_changed.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_changed.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_page: test_page.c check.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_page.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_transition: test_transition.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_transition.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_stats: test_stats.c check.h panel.h $(PAINT) $(ROOT)/src/dgx_ui_stats.c $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=18 -o $@ test_stats.c $(PAINT) $(ROOT)/src/dgx_ui_stats.c $(DGX_SRCS) -lm

$(BUILD)/test_stats_ways: test_stats_ways.c check.h $(PAINT) $(ROOT)/src/dgx_ui_stats.c $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_stats_ways.c $(PAINT) $(ROOT)/src/dgx_ui_stats.c $(DGX_SRCS) -lm

$(BUILD)/test_soft: test_soft.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_soft.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_once $(BUILD)/test_once18: $(BUILD)/test_once%: test_once.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_once.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_tasks: test_tasks.c check.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -pthread -o $@ test_tasks.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_two_paints: test_two_paints.c check.h composed.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_two_paints.c $(PAINT) $(DGX_SRCS) $(DGX)/src/fonts/font0ss.c -lm

$(BUILD)/test_hand_lines: test_hand_lines.c check.h panel.h composed.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_hand_lines.c $(PAINT) $(DGX_SRCS) $(DGX)/src/fonts/font0ss.c -lm

$(BUILD)/test_hand_lines_plain: test_hand_lines.c check.h panel.h composed.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DDGX_UI_HAND_LINES_PLAIN=1 -o $@ test_hand_lines.c $(PAINT) $(DGX_SRCS) $(DGX)/src/fonts/font0ss.c -lm

$(BUILD)/test_wake: test_wake.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_wake.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_own: test_own.c check.h $(CORE) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) -o $@ test_own.c $(CORE) -lm

$(BUILD)/test_hand: test_hand.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_hand.c $(PAINT) $(DGX_SRCS) $(DGX)/src/fonts/font0ss.c -lm

# a widget's value in the program's memory
$(BUILD)/test_radio $(BUILD)/test_radio18: $(BUILD)/test_radio%: test_radio.c check.h panel.h $(PAINT) $(ROOT)/src/dgx_ui_stats.c $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_radio.c $(PAINT) $(ROOT)/src/dgx_ui_stats.c $(DGX_SRCS) -lm

$(BUILD)/test_written $(BUILD)/test_written18: $(BUILD)/test_written%: test_written.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_written.c $(PAINT) $(DGX_SRCS) $(DGX)/src/fonts/font0ss.c -lm

$(BUILD)/test_focus $(BUILD)/test_focus18: $(BUILD)/test_focus%: test_focus.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_focus.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_veil $(BUILD)/test_veil18: $(BUILD)/test_veil%: test_veil.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_veil.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_place_page $(BUILD)/test_place_page18: $(BUILD)/test_place_page%: test_place_page.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_place_page.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_picture_button $(BUILD)/test_picture_button18: $(BUILD)/test_picture_button%: test_picture_button.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_picture_button.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_repeat $(BUILD)/test_repeat18: $(BUILD)/test_repeat%: test_repeat.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_repeat.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_idle $(BUILD)/test_idle18: $(BUILD)/test_idle%: test_idle.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_idle.c $(PAINT) $(DGX_SRCS) -lm

# dgx_ui_c against the loader (test_generate.py): the tool built into this build by tools/dgx_ui_c.py, the one way it
# is built, as the package's rule builds it; the files it makes compiled with test_generate.c in one go
$(BUILD)/tools/dgx_ui_c: $(ROOT)/tools/dgx_ui_c.c $(ROOT)/tools/dgx_ui_names.c $(ROOT)/tools/dgx_ui_names.h $(ROOT)/tools/dgx_ui_c.py $(PAINT) $(wildcard $(ROOT)/include/*.h) $(wildcard $(ROOT)/src/*.h)
	python3 $(ROOT)/tools/dgx_ui_c.py $(abspath $(DGX)) $(abspath $(BUILD))/tools

run-test_generate: $(BUILD)/tools/dgx_ui_c test_generate.c test_generate.py generate_program.h check.h
	timeout 900 python3 test_generate.py $(BUILD)/tools/dgx_ui_c $(BUILD)/generate $(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) \
		test_generate.c $(PAINT) $(DGX_SRCS) -lm -Wl,--wrap=calloc

# appearances made C by dgx_ui_c against their loader (test_generate_appearance.py): the descriptions given out by a
# program of the tool's own list of draws, the made files compiled with test_generate_appearance.c in one go
$(BUILD)/appearance_dump: appearance_dump.c $(BUILD)/tools/dgx_ui_c $(ROOT)/src/dgx_ui_params.c $(wildcard $(ROOT)/include/*.h)
	$(CC) -std=gnu11 $(WARN) $(CPPFLAGS) -I$(BUILD)/tools -o $@ appearance_dump.c $(ROOT)/src/dgx_ui_params.c

run-test_generate_appearance: $(BUILD)/tools/dgx_ui_c $(BUILD)/appearance_dump test_generate_appearance.c test_generate_appearance.py appearance_program.h check.h
	timeout 900 python3 test_generate_appearance.py $(BUILD)/tools/dgx_ui_c $(BUILD)/appearance_dump $(BUILD)/generate_appearance $(CC) $(CFLAGS) -std=gnu11 \
		$(WARN) $(CPPFLAGS) $(DGX_FLAGS) test_generate_appearance.c $(PAINT) $(DGX_SRCS) $(DGX_ROOT)/src/fonts/font0ss.c -lm

# the preview of a description on the computer: its glue, the reading of the names it shares with dgx_ui_c, whose
# build makes the headers of the library's looks and draws, and the fonts built into it
PREVIEW       := $(ROOT)/editor/preview/dgx_ui_preview.c $(ROOT)/tools/dgx_ui_names.c
PREVIEW_FONTS := $(addprefix $(DGX_ROOT)/src/fonts/, font0ss.c LiberationSansRegular12.c DroidSansRegular9.c)

$(BUILD)/test_preview: test_preview.c check.h panel.h $(PREVIEW) $(ROOT)/editor/preview/dgx_ui_preview.h $(ROOT)/tools/dgx_ui_names.h $(BUILD)/tools/dgx_ui_c \
                       $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -I$(ROOT)/src -I$(ROOT)/tools -I$(ROOT)/editor/preview -I$(BUILD)/tools -o $@ \
		test_preview.c $(PREVIEW) $(PAINT) $(DGX_SRCS) $(PREVIEW_FONTS) -lm

# the module of the preview built to WebAssembly, editor/preview/dgx_ui_preview.js as it is in the tree, held to the
# same glue built here, scene by scene, byte for byte (test/preview/make_scenes.py); in Node
$(BUILD)/scene: $(ROOT)/test/preview/scene.c $(PREVIEW) $(ROOT)/editor/preview/dgx_ui_preview.h $(ROOT)/tools/dgx_ui_names.h $(BUILD)/tools/dgx_ui_c \
                $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -I$(ROOT)/src -I$(ROOT)/tools -I$(ROOT)/editor/preview -I$(BUILD)/tools -o $@ \
		$(ROOT)/test/preview/scene.c $(PREVIEW) $(PAINT) $(DGX_SRCS) $(PREVIEW_FONTS) -lm

run-test_preview_module: $(BUILD)/scene run-test_generate run-test_generate_appearance $(ROOT)/test/preview/scene.js $(ROOT)/test/preview/make_scenes.py
	timeout 1500 python3 $(ROOT)/test/preview/make_scenes.py $(BUILD)/scene $(ROOT)/editor/preview/dgx_ui_preview.js $(DGX_ROOT) $(BUILD)/scenes \
		$(BUILD)/generate $(BUILD)/generate_appearance

# the class of the module, DgxUiPreview, by its own calls, as a page makes them; in Node
run-test_preview_class: $(ROOT)/test/preview/test_class.js $(ROOT)/editor/preview/dgx_ui_preview.js
	timeout 300 node $(ROOT)/test/preview/test_class.js $(ROOT)/editor/preview/dgx_ui_preview.js

# the reader of JSON alone: no tree, no DGX
$(BUILD)/test_params: test_params.c check.h $(ROOT)/src/dgx_ui_params.c $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) -o $@ test_params.c $(ROOT)/src/dgx_ui_params.c

$(BUILD)/test_appearance: test_appearance.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_appearance.c $(PAINT) $(DGX_SRCS) $(DGX_ROOT)/src/fonts/font0ss.c -lm

# the stack a load takes, counted on a thread's own stack: no sanitizer, whose frames would blur the count
$(BUILD)/test_stack: test_stack.c check.h $(CORE) $(ROOT)/src/dgx_ui_load.c $(ROOT)/src/dgx_ui_json.c $(ROOT)/src/dgx_ui_read.c $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) -O2 -g -std=gnu11 $(WARN) $(CPPFLAGS) -I$(ROOT)/src -o $@ test_stack.c $(CORE) $(ROOT)/src/dgx_ui_load.c $(ROOT)/src/dgx_ui_json.c $(ROOT)/src/dgx_ui_read.c -lpthread

$(BUILD)/test_json: test_json.c check.h $(ROOT)/src/dgx_ui_json.c $(ROOT)/src/dgx_ui_json.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) -I$(ROOT)/src -o $@ test_json.c $(ROOT)/src/dgx_ui_json.c

$(BUILD)/test_load: test_load.c check.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_load.c $(PAINT) $(DGX_SRCS) -lm -Wl,--wrap=calloc

$(BUILD)/test_shown: test_shown.c check.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_shown.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_level $(BUILD)/test_level18: $(BUILD)/test_level%: test_level.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_level.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_ring $(BUILD)/test_ring18: $(BUILD)/test_ring%: test_ring.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -DPANEL_BITS=$(or $*,16) -o $@ test_ring.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_random: test_random.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_random.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_place: test_place.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_place.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_di: test_di.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -I$(DGX_DI)/include -o $@ test_di.c $(PAINT) $(DGX_DI)/src/dgx_di_touch.c $(DGX_DI)/src/dgx_di_pointer.c $(DGX_SRCS) -lm

# the library's sources read with every warning an error, as a program built by host/dgx_ui_host.mk with -Werror reads
# them: the queue's and the ticker's need FreeRTOS, which the stubs have not
SOURCES := $(filter-out %/dgx_ui_post.c %/dgx_ui_ticker.c,$(wildcard $(ROOT)/src/*.c))

# no warning, read and compiled: some warnings come only from the optimiser (a value it cannot see given), so each
# source is compiled at the levels a build takes, as ESP-IDF's -Werror=all would refuse it
check-werror: $(BUILD)/tools/dgx_ui_c
	$(CC) -std=gnu11 $(WARN) -Werror $(CPPFLAGS) -fsyntax-only $(SOURCES)
	@for level in -O1 -O2 -Os; do \
		for source in $(SOURCES); do $(CC) $$level -std=gnu11 $(WARN) -Werror $(CPPFLAGS) -c $$source -o /dev/null || exit 1; done; \
		for source in $(PREVIEW); do \
			$(CC) $$level -std=gnu11 $(WARN) -Werror $(CPPFLAGS) -I$(ROOT)/src -I$(ROOT)/tools -I$(ROOT)/editor/preview -I$(BUILD)/tools -c $$source -o /dev/null || exit 1; \
		done; \
	done
	@echo "the sources, the preview's and the names' reading among them: no warning, read and compiled at -O1, -O2 and -Os"

# every source of src/ in the component's list, and in the list of the build on a computer (host/dgx_ui_host.mk) but
# the two of FreeRTOS's queues and esp_timer
ALL_SRCS     := $(notdir $(wildcard $(ROOT)/src/*.c))
HOST_MISSING := $(filter-out $(notdir $(DGX_UI_HOST_SRCS)) dgx_ui_post.c dgx_ui_ticker.c,$(ALL_SRCS))

check-lists:
	@for f in $(ALL_SRCS); do grep -q "\"src/$$f\"" $(ROOT)/CMakeLists.txt || { echo "$$f: not in CMakeLists.txt"; exit 1; }; done
	@test -z "$(HOST_MISSING)" || { echo "not in host/dgx_ui_host.mk: $(HOST_MISSING)"; exit 1; }
	@echo "the lists of sources: whole"

# the public headers read by a C++ compiler, as a program in C++ (Arduino) includes them; those of FreeRTOS's queues
# and esp_timer, which the stubs of DGX do not have, are read by the C++ file of the ESP-IDF build check instead
HEADERS := $(filter-out %/dgx_ui_post.h %/dgx_ui_ticker.h,$(wildcard $(ROOT)/include/*.h))
CXX_DI  := $(if $(DGX_DI),-I$(DGX_DI)/include)

check-cxx: cxx_uses.cpp
	mkdir -p $(BUILD)
	for h in $(HEADERS); do echo "#include \"$$(basename $$h)\"" ; done > $(BUILD)/headers.cpp
	cat cxx_uses.cpp >> $(BUILD)/headers.cpp
	$(CXX) -std=gnu++17 $(WARN) -Werror $(CPPFLAGS) $(CXX_DI) -fsyntax-only $(BUILD)/headers.cpp
	@echo "the headers: read by C++"

# the code of the documentation's pages against the headers, its names and links (test/docs): not in all, it wants
# DGX-DI beside
check-docs:
	python3 ../docs/check_docs.py $(DGX) $(DGX_DI)

clean:
	rm -rf $(BUILD)

.PHONY: all clean run-test_generate run-test_generate_appearance run-test_preview_module run-test_preview_class check-cxx check-werror check-lists check-docs
