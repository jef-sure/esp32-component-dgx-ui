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
WARN     := -Wall -Wextra -Wno-unused-parameter

all: run-test_rect run-test_core run-test_msg run-test_paint run-test_around run-test_lines run-test_controls run-test_popup run-test_nest run-test_skip run-test_list run-test_dropdown run-test_indexed run-test_deep run-test_deep18 run-test_background run-test_canvas run-test_scroll run-test_dump run-test_wake run-test_own run-test_hand run-test_changed run-test_page run-test_transition run-test_hand_lines run-test_hand_lines_plain run-test_two_paints run-test_tasks run-test_stats run-test_stats_ways run-test_soft run-test_place run-test_once run-test_once18 run-test_radio run-test_radio18 run-test_random check-werror $(if $(DGX_DI),run-test_di) check-cxx

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
             $(addprefix $(ROOT)/src/, dgx_ui_button_look.c dgx_ui_input_look.c dgx_ui_control_looks.c dgx_ui_check_looks.c dgx_ui_list_look.c dgx_ui_dropdown_look.c dgx_ui_dropdown.c dgx_ui_picture_look.c dgx_ui_rgb565_look.c dgx_ui_indexed_look.c dgx_ui_strips.c dgx_ui_background.c dgx_ui_canvas.c dgx_ui_hand.c dgx_ui_hand_canvas.c dgx_ui_changed.c dgx_ui_page.c dgx_ui_transition.c dgx_ui_hand_lines.c)
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

check-werror:
	$(CC) -std=gnu11 $(WARN) -Werror $(CPPFLAGS) -fsyntax-only $(SOURCES)
	@echo "the sources: no warning"

# the public headers read by a C++ compiler, as a program in C++ (Arduino) includes them; those of FreeRTOS's queues
# and esp_timer, which the stubs of DGX do not have, are read by the C++ file of the ESP-IDF build check instead
HEADERS := $(filter-out %/dgx_ui_post.h %/dgx_ui_ticker.h,$(wildcard $(ROOT)/include/*.h))
CXX_DI  := $(if $(DGX_DI),-I$(DGX_DI)/include)

check-cxx:
	mkdir -p $(BUILD)
	for h in $(HEADERS); do echo "#include \"$$(basename $$h)\"" ; done > $(BUILD)/headers.cpp
	$(CXX) -std=gnu++17 $(WARN) -Werror $(CPPFLAGS) $(CXX_DI) -fsyntax-only $(BUILD)/headers.cpp
	@echo "the headers: read by C++"

# the code of the documentation's pages against the headers, its names and links (test/docs): not in all, it wants
# DGX-DI beside
check-docs:
	python3 ../docs/check_docs.py $(DGX) $(DGX_DI)

clean:
	rm -rf $(BUILD)

.PHONY: all clean check-cxx check-werror check-docs
