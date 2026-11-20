# dgx_ui_c, a description of dgx_ui_load.h made C constants: a program of the computer (tools/README.md).
# Run: make -C tools; DGX=<path> takes another copy of DGX, BUILD=<directory> another place for what is made.
#
# DGX is taken from the managed component of the project dgx-ui sits in, else from a checkout next to dgx-ui's
# (../esp32-component-dgx), as the host tests take it.
ROOT   := ..
DGX    ?= $(firstword $(wildcard $(ROOT)/../../managed_components/jef-sure__dgx) $(ROOT)/../esp32-component-dgx)
CC     ?= cc
CFLAGS ?= -O1 -g
BUILD  ?= build

include $(DGX)/host/dgx_host.mk
include $(ROOT)/host/dgx_ui_host.mk

# the reader, the loader and the core of dgx-ui; of DGX what they call
SRCS := $(DGX_UI_HOST_SRCS) $(DGX_HOST_SRCS_SCREEN) $(DGX_HOST_SRCS_DRAW) $(DGX_HOST_SRCS_VSCREEN) $(DGX_HOST_SRCS_TEXT) $(DGX_HOST_SRCS_MORPH) $(DGX_HOST_SRCS_FB)

all: $(BUILD)/dgx_ui_c

# the library's own looks, by the declarations of include/dgx_ui_paint.h: LOOK(word) each
$(BUILD)/library_looks.h: $(ROOT)/include/dgx_ui_paint.h
	mkdir -p $(BUILD)
	sed -n '/^extern const dgx_ui_look_t /,/;/p' $< | grep -o 'dgx_ui_look_[a-z0-9_]*' | grep -v '^dgx_ui_look_t$$' | sort -u | sed 's/^dgx_ui_look_\(.*\)$$/LOOK(\1)/' > $@

$(BUILD)/dgx_ui_c: dgx_ui_c.c $(BUILD)/library_looks.h $(SRCS) $(wildcard $(ROOT)/include/*.h) $(ROOT)/src/dgx_ui_json.h
	$(CC) $(CFLAGS) -std=gnu11 -Wall -Wextra -Wno-unused-parameter $(DGX_HOST_CPPFLAGS) $(DGX_UI_HOST_CPPFLAGS) -I$(DGX)/src -I$(ROOT)/src -I$(BUILD) \
		-Wno-format -Wno-sign-compare -o $@ dgx_ui_c.c $(SRCS) -lm

clean:
	rm -rf $(BUILD)

.PHONY: all clean
