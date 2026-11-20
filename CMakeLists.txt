# dgx-ui: a light layer of widgets on DGX. The same sources on a chip and on the linux target of ESP-IDF, where the
# displays of DGX are windows on the computer. DGX-DI is not required: its adapter is a header a program with touch
# includes.
set(srcs "src/dgx_ui.c" "src/dgx_ui_text.c" "src/dgx_ui_rect.c" "src/dgx_ui_msg.c" "src/dgx_ui_post.c" "src/dgx_ui_ticker.c"
	"src/dgx_ui_paint.c" "src/dgx_ui_looks.c" "src/dgx_ui_text_look.c" "src/dgx_ui_lines_look.c"
	"src/dgx_ui_button_look.c" "src/dgx_ui_input_look.c" "src/dgx_ui_control_looks.c" "src/dgx_ui_check_looks.c" "src/dgx_ui_level_look.c" "src/dgx_ui_list_look.c" "src/dgx_ui_dropdown_look.c" "src/dgx_ui_dropdown.c" "src/dgx_ui_picture_look.c" "src/dgx_ui_rgb565_look.c" "src/dgx_ui_indexed_look.c"
	"src/dgx_ui_strips.c" "src/dgx_ui_background.c" "src/dgx_ui_canvas.c" "src/dgx_ui_dump.c" "src/dgx_ui_hand.c" "src/dgx_ui_hand_canvas.c" "src/dgx_ui_changed.c" "src/dgx_ui_page.c" "src/dgx_ui_transition.c" "src/dgx_ui_hand_lines.c"
	"src/dgx_ui_stats.c")

idf_component_register(
	SRCS ${srcs}
	INCLUDE_DIRS "include"
	REQUIRES jef-sure__dgx
	PRIV_REQUIRES esp_timer  # the ticker's timer on a chip; on the linux target a header only, and not used
)
