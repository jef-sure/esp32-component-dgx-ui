# The preview of a description built to WebAssembly (dgx_ui_preview.h): dgx_ui_preview.js, the module's bytes in base64
# with the class that calls them (preview_class.js), one file a page loads with no server; and licenses/, the licences
# of what is in it, with taken.txt, what it took of the C library and of the compiler's builtins by its link map.
#
#     make DGX=<the registry's copy of DGX>           both made again here
#     make DGX=<the registry's copy of DGX> check     both made again in a directory of its own and held to the ones
#                                                      here, byte for byte
#
# The toolchain is Ubuntu's: clang 21 for wasm32-wasi, wasm-ld of lld-21, the C library of wasi-libc and the builtins
# of libclang-rt-21-dev-wasm32 (apt install lld-21 wasi-libc libclang-rt-21-dev-wasm32). DGX is the registry's copy at
# the version dgx-ui asks for at least, a project's managed_components/jef-sure__dgx: preview_build.py refuses another
# version and a checkout. The same toolchain and sources give the same bytes: no path of the machine and no time are in
# them. The module is the sources of host/dgx_ui_host.mk and of DGX's host/dgx_host.mk, the preview's glue, the reading
# of the names dgx_ui_c shares, and five fonts of DGX.
ROOT    := $(abspath ../..)
HERE    := $(ROOT)/editor/preview
BUILD   ?= build
CLANG   ?= clang-21
CHECK   := $(abspath $(BUILD))/check
OUT     := $(abspath $(BUILD))/out
ifeq ($(DGX),)
$(error DGX=<the registry's copy of jef-sure/dgx> is wanted: a project's managed_components/jef-sure__dgx)
endif
include $(abspath $(DGX))/host/dgx_host.mk
include $(ROOT)/host/dgx_ui_host.mk

MADE    := $(abspath $(BUILD))/made
FONTS   := $(addprefix $(DGX_ROOT)/src/fonts/, TerminusTTFMedium12.c DejaVuSansBook32.c LiberationSansRegular12.c DroidSansRegular9.c font0ss.c)
# dgx-ui's sources with every warning of ours; DGX's without two that clang alone gives there, of a function left unused
# and of a fraction in a font's average width, which are DGX's to see to
OURS    := $(HERE)/dgx_ui_preview.c $(ROOT)/tools/dgx_ui_names.c $(DGX_UI_HOST_SRCS)
THEIRS  := $(DGX_HOST_SRCS_SCREEN) $(DGX_HOST_SRCS_DRAW) $(DGX_HOST_SRCS_VSCREEN) $(DGX_HOST_SRCS_TEXT) $(DGX_HOST_SRCS_MORPH) $(FONTS)
SOURCES := $(OURS) $(THEIRS)
OBJ     := $(abspath $(BUILD))/obj
OBJECTS := $(sort $(addprefix $(OBJ)/,$(notdir $(SOURCES:.c=.o))))
HEADERS := $(sort $(wildcard $(ROOT)/include/*.h $(ROOT)/src/*.h) $(ROOT)/tools/dgx_ui_names.h $(HERE)/dgx_ui_preview.h \
           $(shell find $(DGX_ROOT)/include $(DGX_HOST_DIR)/stubs -name '*.h'))
EXPORTS := version format tables json screen text names font appearance page pointer key buttons value paint wake frame changed shown count widget \
           widget_at events notes
comma   := ,
FLAGS   := --target=wasm32-wasi -Os -flto -std=gnu11 -ffile-prefix-map=$(ROOT)=dgx-ui -ffile-prefix-map=$(DGX_ROOT)=dgx \
           -ffile-prefix-map=$(MADE)=made $(DGX_HOST_CPPFLAGS) $(DGX_UI_HOST_CPPFLAGS) -I$(DGX_ROOT)/src -I$(ROOT)/src -I$(ROOT)/tools -I$(HERE) \
           -I$(MADE) -Wall -Wextra -Wno-unused-parameter -Wno-format -Wno-sign-compare -Werror=deprecated-declarations
LINK    := -Wl,--strip-all -Wl,-z,stack-size=262144 -Wl,--Map=$(abspath $(BUILD))/dgx_ui_preview.map \
           $(addprefix -Wl$(comma)--export=preview_,$(EXPORTS)) -lm

# what is made: the file a page loads, and the licences of what is in it
LICENSES := LICENSE-DGX OFL-1.1.txt DejaVu-LICENSE.txt Apache-2.0.txt FONTS-DGX.md wasi-libc.copyright \
            libclang-rt-21-dev-wasm32.copyright taken.txt
MADE_OUT := $(OUT)/dgx_ui_preview.js $(addprefix $(OUT)/licenses/,$(LICENSES))

all: $(MADE_OUT)
	mkdir -p licenses
	cp $(OUT)/dgx_ui_preview.js dgx_ui_preview.js
	cp $(addprefix $(OUT)/licenses/,$(LICENSES)) licenses/

$(OUT)/licenses/LICENSE-DGX: $(DGX_ROOT)/LICENSE
	mkdir -p $(dir $@) && cp $< $@
$(OUT)/licenses/FONTS-DGX.md: $(DGX_ROOT)/src/fonts/README.md
	mkdir -p $(dir $@) && cp $< $@
$(OUT)/licenses/%.txt: $(DGX_ROOT)/src/fonts/licenses/%.txt
	mkdir -p $(dir $@) && cp $< $@
$(OUT)/licenses/%.copyright: /usr/share/doc/%/copyright
	mkdir -p $(dir $@) && cp $< $@
$(OUT)/licenses/taken.txt: $(OUT)/dgx_ui_preview.js preview_build.py
	python3 preview_build.py taken $(abspath $(BUILD))/dgx_ui_preview.map $@

$(MADE)/library_looks.h $(MADE)/library_draws.h: preview_build.py $(ROOT)/tools/dgx_ui_c.py $(ROOT)/include/dgx_ui_paint.h $(ROOT)/src/dgx_ui_params.c
	python3 preview_build.py headers $(MADE)

$(MADE)/preview_version.h: preview_build.py $(SOURCES) $(HEADERS) $(ROOT)/idf_component.yml $(DGX_ROOT)/idf_component.yml
	CLANG=$(CLANG) python3 preview_build.py version $(DGX_ROOT) $@ $(ROOT) $(SOURCES) $(HEADERS)

$(BUILD)/dgx_ui_preview.wasm: $(SOURCES) $(HEADERS) $(MADE)/library_looks.h $(MADE)/library_draws.h $(MADE)/preview_version.h
	@test $(words $(OBJECTS)) -eq $(words $(SOURCES)) || { echo "two sources of one name: their objects would meet"; exit 1; }
	rm -rf $(OBJ) && mkdir -p $(OBJ)
	cd $(OBJ) && $(CLANG) $(FLAGS) -c $(OURS)
	cd $(OBJ) && $(CLANG) $(FLAGS) -Wno-unused-function -Wno-literal-conversion -c $(THEIRS)
	$(CLANG) --target=wasm32-wasi -Os -flto -mexec-model=reactor $(LINK) -o $@ $(OBJECTS)

$(OUT)/dgx_ui_preview.js: $(BUILD)/dgx_ui_preview.wasm preview_class.js preview_build.py
	mkdir -p $(OUT)
	python3 preview_build.py js $< preview_class.js $(MADE)/preview_version.h $@

# made again from nothing, in a directory of its own, and held to the files here
check:
	rm -rf "$(CHECK)"
	$(MAKE) --no-print-directory BUILD=$(CHECK) DGX=$(DGX) CLANG=$(CLANG) $(addprefix $(CHECK)/out/,dgx_ui_preview.js $(addprefix licenses/,$(LICENSES)))
	cmp $(CHECK)/out/dgx_ui_preview.js dgx_ui_preview.js
	for name in $(LICENSES); do cmp $(CHECK)/out/licenses/$$name licenses/$$name || exit 1; done
	@echo "dgx_ui_preview.js and licenses/: made again the same, byte for byte"

clean:
	rm -rf $(BUILD)

.PHONY: all check clean
