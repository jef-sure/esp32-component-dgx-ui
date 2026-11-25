# dgx-ui built on a PC with make, beside DGX's host/dgx_host.mk: include this file from a Makefile.
#
#   include path/to/dgx/host/dgx_host.mk
#   include path/to/dgx-ui/host/dgx_ui_host.mk
#   CPPFLAGS += $(DGX_HOST_CPPFLAGS) $(DGX_UI_HOST_CPPFLAGS)
#   CFLAGS   += $(DGX_HOST_CFLAGS)
#   LDLIBS   += $(DGX_HOST_LDLIBS)
#   check: main.c $(DGX_HOST_SRCS) $(DGX_UI_HOST_SRCS)
#
# The paths are absolute, found from where this file lies.

DGX_UI_HOST_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
DGX_UI_ROOT     := $(patsubst %/,%,$(dir $(DGX_UI_HOST_DIR)))

DGX_UI_HOST_SRCS     := $(addprefix $(DGX_UI_ROOT)/src/, dgx_ui.c dgx_ui_text.c dgx_ui_rect.c dgx_ui_msg.c dgx_ui_paint.c dgx_ui_looks.c dgx_ui_text_look.c dgx_ui_lines_look.c \
                        dgx_ui_button_look.c dgx_ui_picture_button_look.c dgx_ui_input_look.c dgx_ui_control_looks.c dgx_ui_check_looks.c dgx_ui_level_look.c dgx_ui_ring_look.c dgx_ui_list_look.c dgx_ui_dropdown_look.c dgx_ui_dropdown.c \
                        dgx_ui_picture_look.c dgx_ui_rgb565_look.c dgx_ui_indexed_look.c dgx_ui_nine_look.c dgx_ui_strips.c dgx_ui_background.c dgx_ui_canvas.c dgx_ui_dump.c dgx_ui_hand.c dgx_ui_hand_canvas.c dgx_ui_changed.c dgx_ui_page.c dgx_ui_transition.c dgx_ui_hand_lines.c \
                        dgx_ui_stats.c dgx_ui_idle.c dgx_ui_load.c dgx_ui_load_actions.c dgx_ui_json.c dgx_ui_read.c dgx_ui_appearance.c dgx_ui_params.c)
# dgx_ui_post.c and dgx_ui_ticker.c need FreeRTOS's queues and esp_timer, which the stubs of DGX do not have; the looks
# of pictures, the strips and the canvases need DGX's virtual screens, which $(DGX_HOST_SRCS) has
DGX_UI_HOST_CPPFLAGS := -I$(DGX_UI_ROOT)/include
