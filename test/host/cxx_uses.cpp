/* the headers' macros in use, so that C++ reads their initializers: the members in their order of declaration */
static const dgx_ui_text_style_t         cxx_style   = {};
[[maybe_unused]] static dgx_ui_textbox_t cxx_caption = DGX_UI_TEXT_CONST("Yes", 1, &cxx_style);
/* a widget of every member, in their order of declaration; a percent a constant expression of C++ */
[[maybe_unused]] static const dgx_ui_widget_t cxx_widget = {
    .kind = DGX_UI_BOX, .state = {DGX_UI_HIDDEN}, .flags = DGX_UI_ROW | DGX_UI_PERCENT_W | DGX_UI_PERCENT_PAD, .pad = DGX_UI_PERCENT(5), .gap = 2,
    .at_x = 1, .at_y = 2, .want_w = DGX_UI_PERCENT(33.33), .want_h = 20, .min = 0, .max = 1, .step = 1, .tag = 7, .value = nullptr,
    .picture = nullptr, .text = nullptr, .look = nullptr, .take = nullptr, .on_event = nullptr, .ctx = nullptr, .own = nullptr};
static_assert(DGX_UI_PERCENT(50) == 12800 && DGX_UI_PERCENT(128) == 32767 && DGX_UI_PERCENT(-200) == -32768, "a percent at compile time");
