# The recipes of the cookbook run, by the code of the page itself:
#   make -C test/host -f ../docs/run.mk run-cookbook [OUT=<a directory for the pictures of the recipes>]
# The checker writes the page's C into $(BUILD)/docs first, and into its made_en what dgx_ui_c makes of the files the
# recipe of a description made C prints, with the files of pictures DGX's writer makes for the recipe of a picture from
# a PNG file, their dgx_assets.S assembled alone as a build assembles it; the runner is built with them, as the tests of
# test/host are.
include Makefile

RUN_WARN := -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable \
            -Wno-missing-field-initializers

run-cookbook:
	mkdir -p $(BUILD)/docs
	python3 ../docs/check_docs.py $(DGX) $(DGX_DI) $(BUILD)/docs
	cc -c -o $(BUILD)/docs/made_en/dgx_assets.o $(BUILD)/docs/made_en/out/dgx_assets.S
	$(CC) $(CFLAGS) -std=gnu11 $(RUN_WARN) -DDGX_HOST_WINDOW -I../docs/stubs -I. $(CPPFLAGS) $(DGX_FLAGS) -I$(DGX)/host/window \
		-I$(DGX_DI)/include -I$(BUILD)/docs -I$(BUILD)/docs/fonts -I$(BUILD)/docs/made_en -I$(BUILD)/docs/made_en/out -DMADE_DIR='"$(BUILD)/docs/made_en"' \
		-o $(BUILD)/run_cookbook ../docs/run_cookbook.c $(BUILD)/docs/made_en/sound.c $(BUILD)/docs/made_en/vocab.c $(BUILD)/docs/made_en/day.c \
		$(BUILD)/docs/made_en/sound_day.c $(BUILD)/docs/made_en/weather.c $(BUILD)/docs/made_en/icons.c $(BUILD)/docs/made_en/dgx_assets.o $(BUILD)/docs/fonts/dgx_fonts.c $(PAINT) \
		$(ROOT)/src/dgx_ui_stats.c $(ROOT)/src/dgx_ui_command.c $(DGX_SRCS) $(DGX)/src/fonts/font0ss.c $(DGX)/src/fonts/GLCD5x7.c -lm
	ASAN_OPTIONS=detect_leaks=0 $(BUILD)/run_cookbook $(OUT)

# The pictures of docs/images but the ring's and the strip's, as PPM in $(OUT): make ... pictures OUT=<directory>; their
# fonts made by DGX of ../docs/fonts.needs, as check_docs.py makes them for the pages' code; the getting-started page's
# of ../docs/start.needs, as its program asks for them
pictures:
	mkdir -p $(BUILD) $(OUT)
	python3 $(DGX)/font2c/dgx_fonts.py build ../docs/fonts.needs $(BUILD)/pictures_fonts
	$(CC) $(CFLAGS) -std=gnu11 $(RUN_WARN) $(CPPFLAGS) $(DGX_FLAGS) -I$(BUILD)/pictures_fonts -o $(BUILD)/pictures ../docs/pictures.c \
		$(BUILD)/pictures_fonts/dgx_fonts.c $(PAINT) $(DGX_SRCS) -lm
	ASAN_OPTIONS=detect_leaks=0 $(BUILD)/pictures $(OUT)
	python3 $(DGX)/font2c/dgx_fonts.py build ../docs/start.needs $(BUILD)/start_fonts
	$(CC) $(CFLAGS) -std=gnu11 $(RUN_WARN) $(CPPFLAGS) $(DGX_FLAGS) -I$(BUILD)/start_fonts -o $(BUILD)/picture_start ../docs/picture_start.c \
		$(BUILD)/start_fonts/dgx_fonts.c $(PAINT) $(DGX_SRCS) -lm
	ASAN_OPTIONS=detect_leaks=0 $(BUILD)/picture_start $(OUT)

.PHONY: run-cookbook pictures
