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

all: run-test_rect run-test_core run-test_msg run-test_paint run-test_lines run-test_controls run-test_popup $(if $(DGX_DI),run-test_di) check-cxx

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
             $(addprefix $(ROOT)/src/, dgx_ui_button_look.c dgx_ui_input_look.c dgx_ui_control_looks.c dgx_ui_picture_look.c dgx_ui_rgb565_look.c)
DGX_SRCS  := $(DGX_HOST_SRCS_SCREEN) $(DGX_HOST_SRCS_DRAW) $(DGX_HOST_SRCS_VSCREEN) $(DGX_HOST_SRCS_TEXT) $(DGX_HOST_SRCS_MORPH) \
             $(DGX_HOST_SRCS_FB) $(DGX_ROOT)/src/fonts/TerminusTTFMedium12.c $(DGX_ROOT)/src/fonts/DejaVuSansBook32.c
DGX_FLAGS := -I$(DGX)/src -Wno-format -Wno-sign-compare

$(BUILD)/test_paint: test_paint.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_paint.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_lines: test_lines.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_lines.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_controls: test_controls.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_controls.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_popup: test_popup.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $@ test_popup.c $(PAINT) $(DGX_SRCS) -lm

$(BUILD)/test_di: test_di.c check.h panel.h $(PAINT) $(wildcard $(ROOT)/include/*.h)
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 $(WARN) $(CPPFLAGS) $(DGX_FLAGS) -I$(DGX_DI)/include -o $@ test_di.c $(PAINT) $(DGX_DI)/src/dgx_di_touch.c $(DGX_SRCS) -lm

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

.PHONY: all clean check-cxx check-docs
