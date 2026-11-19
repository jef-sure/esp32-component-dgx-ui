# The recipes of the cookbook run, by the code of the page itself:
#   make -C test/host -f ../docs/run.mk run-cookbook [OUT=<a directory for ring.ppm and ring-layout.ppm>]
# The checker writes the page's C into $(BUILD)/docs first; the runner is built as the tests of test/host are.
include Makefile

RUN_WARN := -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable \
            -Wno-missing-field-initializers

run-cookbook:
	mkdir -p $(BUILD)/docs
	python3 ../docs/check_docs.py $(DGX) $(DGX_DI) $(BUILD)/docs
	$(CC) $(CFLAGS) -std=gnu11 $(RUN_WARN) -DDGX_HOST_WINDOW -I../docs/stubs -I. $(CPPFLAGS) $(DGX_FLAGS) -I$(DGX)/host/window \
		-I$(DGX_DI)/include -I$(BUILD)/docs -o $(BUILD)/run_cookbook ../docs/run_cookbook.c $(PAINT) $(ROOT)/src/dgx_ui_stats.c $(DGX_SRCS) $(DGX)/src/fonts/font0ss.c -lm
	ASAN_OPTIONS=detect_leaks=0 $(BUILD)/run_cookbook $(OUT)

# The pictures of docs/images but the ring's and the strip's, as PPM in $(OUT): make ... pictures OUT=<directory>
pictures:
	mkdir -p $(BUILD) $(OUT)
	$(CC) $(CFLAGS) -std=gnu11 $(RUN_WARN) $(CPPFLAGS) $(DGX_FLAGS) -o $(BUILD)/pictures ../docs/pictures.c $(PAINT) $(DGX_SRCS) -lm
	ASAN_OPTIONS=detect_leaks=0 $(BUILD)/pictures $(OUT)

.PHONY: run-cookbook pictures
