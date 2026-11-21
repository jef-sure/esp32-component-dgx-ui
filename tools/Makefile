# dgx_ui_c, a description of dgx_ui_load.h made C: a program of the computer (tools/README.md), built by dgx_ui_c.py,
# the one way it is built, here as by the rule of project_include.cmake.
# Run: make -C tools; DGX=<path> takes another copy of DGX, BUILD=<directory> another place for what is made.
#
# DGX is taken from the managed component of the project dgx-ui sits in, else from a checkout next to dgx-ui's
# (../esp32-component-dgx), as the host tests take it.
ROOT   := ..
DGX    ?= $(firstword $(wildcard $(ROOT)/../../managed_components/jef-sure__dgx) $(ROOT)/../esp32-component-dgx)
BUILD  ?= build

all: $(BUILD)/dgx_ui_c

$(BUILD)/dgx_ui_c: dgx_ui_c.py dgx_ui_c.c $(addprefix $(ROOT)/src/, dgx_ui.c dgx_ui_text.c dgx_ui_rect.c dgx_ui_msg.c dgx_ui_load.c dgx_ui_json.c dgx_ui_read.c) \
                   $(wildcard $(ROOT)/include/*.h) $(wildcard $(ROOT)/src/*.h)
	python3 dgx_ui_c.py $(DGX) $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all clean
