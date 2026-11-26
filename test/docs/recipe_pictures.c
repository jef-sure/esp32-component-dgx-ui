/* The pictures of the recipes, a part of run_cookbook.c: each drawn by the recipe's own widgets and functions, on a
   screen of 320x240 at 2 pixels a point, in the colours of the other pictures of docs/images. What a scene adds of its
   own, a caption beside a widget, is said where it is added. Written as PPM into the directory the runner is given. */

/* the colours of every picture of docs/images, as pictures.c has them */
static uint32_t picture_colors[COLORS];

static dgx_screen_t *picture_screen(dgx_ui_paint_t *paint)
{
    dgx_screen_t *scr = dgx_vscreen_init(320, 240, 16, DgxScreenRGB);
    picture_colors[PAPER]       = dgx_screen_rgb(scr, 16, 20, 28);
    picture_colors[INK]         = dgx_screen_rgb(scr, 230, 230, 220);
    picture_colors[FILL]        = dgx_screen_rgb(scr, 32, 48, 72);
    picture_colors[BORDER]      = dgx_screen_rgb(scr, 120, 160, 220);
    picture_colors[ACCENT]      = dgx_screen_rgb(scr, 40, 110, 200);
    picture_colors[ACCENT_DOWN] = dgx_screen_rgb(scr, 90, 160, 240);
    picture_colors[SHADOW]      = dgx_screen_rgb(scr, 0, 0, 0);
    picture_colors[DIM]         = dgx_screen_rgb(scr, 90, 96, 110);
    *paint = (dgx_ui_paint_t){.scr = scr, .colors = picture_colors, .color_count = COLORS, .background = PAPER};
    return scr;
}

/* the screen as ARGB, down to the given row */
static void picture_pixels(dgx_screen_t *scr, int rows, uint32_t *pixels)
{
    dgx_vscreen_to_argb8888(scr, 0, 0, scr->width, rows, NULL, pixels, scr->width);
}

/* the screen written down to the given row */
static void picture_write(dgx_screen_t *scr, const char *dir, const char *name, int rows)
{
    static uint32_t pixels[320 * 320];
    char            path[600];
    if (!dir) return;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    picture_pixels(scr, rows, pixels);
    FILE *file = fopen(path, "wb");
    fprintf(file, "P6\n%d %d\n255\n", scr->width, rows);
    for (int k = 0; k < scr->width * rows; ++k) fputc(pixels[k] >> 16 & 255, file), fputc(pixels[k] >> 8 & 255, file), fputc(pixels[k] & 255, file);
    fclose(file);
}

/* the lowest row of the widgets under the root but the boxes, which draw nothing, and a margin of 6 points under it */
static int picture_rows(dgx_ui_t *ui, int screen_rows)
{
    int rows = 0;
    for (int i = 1; i < ui->count; ++i) {
        const dgx_ui_node_t *node = dgx_ui_node(ui, i);
        if (node->widget && node->widget->kind != DGX_UI_BOX && node->y + node->h > rows) rows = node->y + node->h;
    }
    rows += 6 * ui->unit;
    return rows < screen_rows ? rows : screen_rows;
}

/* paint the tree, write the screen down to its lowest widget, and free them */
static void picture_done(dgx_ui_t *ui, dgx_ui_paint_t *paint, const char *dir, const char *name)
{
    dgx_screen_t *scr = paint->scr;
    dgx_ui_paint(ui, paint, 0);
    picture_write(scr, dir, name, picture_rows(ui, scr->height));
    dgx_ui_free(ui);
    dgx_ui_paint_free(paint);
    dgx_screen_destroy(&scr);
}

/* the scenes' own: a column for the whole screen, and lines of a caption and a place for a widget */
static const dgx_ui_widget_t pic_root_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 6, .gap = 6};
enum { PIC_LINES = 8 };
static dgx_ui_textbox_t pic_caption_box[PIC_LINES];
static dgx_ui_widget_t  pic_caption_w[PIC_LINES], pic_line_w[PIC_LINES], pic_place_w[PIC_LINES];
static int              pic_lines;

/* a line at least 10 points high: a caption 50 points wide, then the place of a widget as high as the widget, both in
   the middle of the line, a tall line's caption at its top; returns the place */
static int picture_line(dgx_ui_t *ui, int parent, const char *caption, int high)
{
    const int line = pic_lines++, line_high = high > 10 ? high : 10, caption_high = 8;
    pic_caption_box[line] = (dgx_ui_textbox_t)DGX_UI_TEXT_CONST(caption, 1, &st_left);
    pic_caption_w[line]   = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .want_w = 50, .want_h = caption_high,
                                              .at_y = (int16_t)(((line_high < 16 ? line_high : 16) - caption_high) / 2), .text = &pic_caption_box[line]};
    pic_line_w[line]      = (dgx_ui_widget_t){.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 4, .want_h = (int16_t)line_high};
    pic_place_w[line]     = (dgx_ui_widget_t){.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN | DGX_UI_REST, .want_h = (int16_t)high,
                                              .at_y = (int16_t)((line_high - high) / 2)};
    const int box         = dgx_ui_add(ui, parent, &pic_line_w[line]);
    dgx_ui_add(ui, box, &pic_caption_w[line]);
    return dgx_ui_add(ui, box, &pic_place_w[line]);
}

/* the controls: a slider and a switch, a stepper, radio buttons and a tick, level bars, an icon, buttons of pictures,
   pictures from flash */
static void control_pictures(const char *dir)
{
    static dgx_ui_node_t nodes[24];
    dgx_ui_t             ui;
    dgx_ui_paint_t       paint;

    /* A slider and a switch: backlight_w at 70, wifi_w on */
    picture_screen(&paint), pic_lines = 0;
    dgx_ui_init(&ui, nodes, 24, 2);
    int root = dgx_ui_add(&ui, -1, &pic_root_w);
    backlight_percent = 70, wifi_wanted = 1;
    dgx_ui_add(&ui, picture_line(&ui, root, "Backlight", 12), &backlight_w);
    dgx_ui_add(&ui, picture_line(&ui, root, "Wi-Fi", 12), &wifi_w);
    picture_done(&ui, &paint, dir, "slider.ppm");

    /* A stepper: step_add() at 15 minutes */
    picture_screen(&paint), pic_lines = 0;
    dgx_ui_init(&ui, nodes, 24, 2);
    root          = dgx_ui_add(&ui, -1, &pic_root_w);
    step_minutes  = 15;
    step_add(&ui, picture_line(&ui, root, "Timer", 16));
    picture_done(&ui, &paint, dir, "stepper.ppm");

    /* One of several, and a tick: air_add(), Day chosen, Beep ticked */
    picture_screen(&paint), pic_lines = 0;
    dgx_ui_init(&ui, nodes, 24, 2);
    root     = dgx_ui_add(&ui, -1, &pic_root_w);
    air_mode = AIR_DAY, beep_wanted = 1;
    air_add(&ui, root);
    picture_done(&ui, &paint, dir, "radio.ppm");

    /* A level bar: the charge at 35, in its middle band; the tank at 170 litres, above 160 */
    picture_screen(&paint), pic_lines = 0;
    dgx_ui_init(&ui, nodes, 24, 2);
    root           = dgx_ui_add(&ui, -1, &pic_root_w);
    charge_percent = 35, tank_litres = 170;
    dgx_ui_add(&ui, picture_line(&ui, root, "Battery", 6), &charge_w);
    dgx_ui_add(&ui, picture_line(&ui, root, "Tank", 40), &tank_w);
    picture_done(&ui, &paint, dir, "level.ppm");

    /* An icon that shows a state: three bars */
    picture_screen(&paint), pic_lines = 0;
    dgx_ui_init(&ui, nodes, 24, 2);
    root        = dgx_ui_add(&ui, -1, &pic_root_w);
    signal_bars = 3;
    dgx_ui_add(&ui, picture_line(&ui, root, "Signal", 8), &signal_w);
    picture_done(&ui, &paint, dir, "icon.ppm");

    /* A button of a picture: back, the sound on, the tabs with the first shown */
    picture_screen(&paint), pic_lines = 0;
    dgx_ui_init(&ui, nodes, 24, 2);
    root          = dgx_ui_add(&ui, -1, &pic_root_w);
    icon_sound_on = 1, icon_tab_shown = 1;
    static const dgx_ui_widget_t icons_row_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .gap = 6, .want_h = 13};
    const int                    icons       = dgx_ui_add(&ui, root, &icons_row_w);
    dgx_ui_add(&ui, icons, &icon_back_w), dgx_ui_add(&ui, icons, &icon_sound_w);
    dgx_ui_add(&ui, icons, &icon_tab_w[0]), dgx_ui_add(&ui, icons, &icon_tab_w[1]);
    picture_done(&ui, &paint, dir, "picture-button.ppm");

    /* A picture from flash: the logo of RGB565, the battery of 4 bits charged and low */
    picture_screen(&paint), pic_lines = 0;
    static uint8_t picture_buffer[256];
    paint.buffer = picture_buffer, paint.buffer_size = sizeof picture_buffer;
    dgx_ui_init(&ui, nodes, 24, 2);
    root            = dgx_ui_add(&ui, -1, &pic_root_w);
    battery_low_now = 0;
    dgx_ui_add(&ui, picture_line(&ui, root, "Logo", 16), &logo_w);
    dgx_ui_add(&ui, picture_line(&ui, root, "Battery", 8), &battery_w);
    picture_done(&ui, &paint, dir, "picture.ppm");
}

/* the texts: a log, a font that fits, soft letters, handwriting */
static void text_pictures(const char *dir)
{
    static dgx_ui_node_t nodes[24];
    dgx_ui_t             ui;
    dgx_ui_paint_t       paint;

    /* A log of the last lines: eight lines added by log_add(), the last five shown */
    picture_screen(&paint);
    dgx_ui_init(&ui, nodes, 24, 2);
    int root   = dgx_ui_add(&ui, -1, &pic_root_w);
    log_buf[0] = 0;
    const int log = log_make(&ui, root);
    static const char *const log_lines[] = {"08:00 started",     "08:00 Wi-Fi connected", "08:15 CO2 640 ppm",  "08:30 CO2 910 ppm",
                                            "08:41 window open", "09:00 CO2 520 ppm",     "09:12 filter at 80%", "09:30 CO2 480 ppm"};
    for (unsigned k = 0; k < sizeof log_lines / sizeof *log_lines; ++k) log_add(&ui, log, log_lines[k]);
    picture_done(&ui, &paint, dir, "log.ppm");

    /* A font that fits: a short name in the large font and a long one in the small, each label in a frame of the scene's own,
       of one height */
    static dgx_ui_textbox_t      city_box[2]  = {DGX_UI_TEXT_CONST("Oslo", 1, &st_city), DGX_UI_TEXT_CONST("Llanfairpwllgwyngyll", 1, &st_city)};
    static const dgx_ui_widget_t city_w[2]    = {{.kind = DGX_UI_LABEL, .text = &city_box[0]}, {.kind = DGX_UI_LABEL, .text = &city_box[1]}};
    static const dgx_ui_widget_t city_frame_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 2, .want_h = 24};
    picture_screen(&paint);
    dgx_ui_init(&ui, nodes, 24, 2);
    root = dgx_ui_add(&ui, -1, &pic_root_w);
    for (int k = 0; k < 2; ++k) dgx_ui_add(&ui, dgx_ui_add(&ui, root, &city_frame_w), &city_w[k]);
    picture_done(&ui, &paint, dir, "font.ppm");

    /* Letters with soft edges: heading_add(), and under it the scene's own: its first letters three times as large, a
       pixel a square, to show the shades between the paper and the ink */
    static const dgx_ui_widget_t soft_room_w = {.kind = DGX_UI_BOX, .want_h = 52};
    picture_screen(&paint);
    dgx_ui_init(&ui, nodes, 24, 2);
    root              = dgx_ui_add(&ui, -1, &pic_root_w);
    const int heading = heading_add(&ui, root), room = dgx_ui_add(&ui, root, &soft_room_w);
    dgx_ui_paint(&ui, &paint, 0);
    const dgx_ui_node_t *letters = dgx_ui_node(&ui, heading), *under = dgx_ui_node(&ui, room);
    const int            from_y  = letters->y + (letters->h - 34) / 2;
    for (int row = 0; row < 34; ++row)
        for (int column = 0; column < 44; ++column)
            dgx_fill_rectangle(paint.scr, under->x + column * 3, under->y + row * 3, 3, 3, dgx_get_pixel(paint.scr, letters->x + column, from_y + row));
    picture_write(paint.scr, dir, "soft.ppm", under->y + 34 * 3 + 6 * ui.unit);
    dgx_ui_free(&ui), dgx_ui_paint_free(&paint), dgx_screen_destroy(&paint.scr);

    /* A handwritten text, on a tree of a pixel a point: the greeting narrowed, the signature by a soft pen in its canvas,
       the note of several lines; all in a column 200 pixels wide of the scene's own */
    static const dgx_ui_widget_t hand_root_w  = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 12};
    static const dgx_ui_widget_t hand_place_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN | DGX_UI_REST, .gap = 8, .want_w = 200};
    picture_screen(&paint);
    dgx_ui_init(&ui, nodes, 24, 1);
    const int place = dgx_ui_add(&ui, dgx_ui_add(&ui, -1, &hand_root_w), &hand_place_w);
    greeting_init(), signature_init(), note_init();
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, place, &greeting_w), "Good afternoon to you");
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, place, &signature_w), "With thanks");
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, place, &note_w), "The quick brown fox jumps over the lazy dog and runs on and on");
    picture_done(&ui, &paint, dir, "hand.ppm");
}

/* two trees side by side on one screen, each in a half, painted each by its own paint and written as one picture */
static void picture_pair_done(dgx_ui_t *left, dgx_ui_paint_t *left_paint, dgx_ui_t *right, dgx_ui_paint_t *right_paint, const char *dir, const char *name)
{
    dgx_screen_t *scr = left_paint->scr;
    dgx_ui_paint(left, left_paint, 0), dgx_ui_paint(right, right_paint, 0);
    const int rows_left = picture_rows(left, scr->height), rows_right = picture_rows(right, scr->height);
    picture_write(scr, dir, name, rows_left > rows_right ? rows_left : rows_right);
    dgx_ui_free(left), dgx_ui_free(right);
    dgx_ui_paint_free(left_paint), dgx_ui_paint_free(right_paint);
    dgx_screen_destroy(&scr);
}

/* the pages: colours of your own and a night mode, showing and disabling, the bar over the pages, a slide half way */
static void page_pictures(const char *dir)
{
    static dgx_ui_node_t nodes[2][16];
    dgx_ui_t             ui[2];
    dgx_ui_paint_t       paint[2];

    /* Colours of your own, and a night mode: one tree in the day's table at the left; at the right the same, given the
       night's by colors_set(). The button is the scene's own in look_blue_button; the slider and the switch the library's */
    static uint32_t day_colors[COLORS];
    static int32_t  mode_level = 60, mode_on = 1;
    static dgx_ui_textbox_t      mode_title_box[2] = {DGX_UI_TEXT_CONST("Day", 1, &st_left), DGX_UI_TEXT_CONST("Night", 1, &st_left)};
    static dgx_ui_textbox_t      mode_ok_box       = DGX_UI_TEXT_CONST("OK", 1, &st_centre);
    static const dgx_ui_widget_t mode_title_w[2]   = {{.kind = DGX_UI_LABEL, .text = &mode_title_box[0]}, {.kind = DGX_UI_LABEL, .text = &mode_title_box[1]}};
    static const dgx_ui_widget_t mode_ok_w = {.kind = DGX_UI_BUTTON, .want_w = 60, .want_h = 18, .text = &mode_ok_box, .look = &look_blue_button};
    static const dgx_ui_widget_t mode_slider_w = {.kind = DGX_UI_SLIDER, .max = 100, .want_h = 12, .value = &mode_level, .look = &dgx_ui_look_slider};
    static const dgx_ui_widget_t mode_switch_w = {.kind = DGX_UI_SWITCH, .max = 1, .want_w = 26, .want_h = 12, .value = &mode_on, .look = &dgx_ui_look_switch};
    picture_screen(&paint[0]);
    day_colors[PAPER] = dgx_screen_rgb(paint[0].scr, 236, 236, 228), day_colors[INK] = dgx_screen_rgb(paint[0].scr, 24, 28, 36);
    day_colors[FILL] = dgx_screen_rgb(paint[0].scr, 200, 210, 224), day_colors[BORDER] = dgx_screen_rgb(paint[0].scr, 90, 120, 170);
    day_colors[ACCENT] = dgx_screen_rgb(paint[0].scr, 40, 110, 200), day_colors[ACCENT_DOWN] = dgx_screen_rgb(paint[0].scr, 90, 160, 240);
    day_colors[SHADOW] = dgx_screen_rgb(paint[0].scr, 150, 150, 140), day_colors[DIM] = dgx_screen_rgb(paint[0].scr, 150, 156, 170);
    for (int half = 0; half < 2; ++half) {
        paint[half] = (dgx_ui_paint_t){.scr = paint[0].scr, .colors = day_colors, .color_count = COLORS, .background = PAPER};
        dgx_ui_init(&ui[half], nodes[half], 16, 2);
        dgx_ui_set_root(&ui[half], 160 * half, 0, 160, 240);
        const int root = dgx_ui_add(&ui[half], -1, &pic_root_w);
        dgx_ui_add(&ui[half], root, &mode_title_w[half]);
        dgx_ui_add(&ui[half], root, &mode_ok_w), dgx_ui_add(&ui[half], root, &mode_slider_w), dgx_ui_add(&ui[half], root, &mode_switch_w);
    }
    colors_set(&ui[1], &paint[1], picture_colors);
    picture_pair_done(&ui[0], &paint[0], &ui[1], &paint[1], dir, "night.ppm");

    /* Showing, hiding, disabling: one tree built twice, advanced_show() of the expert off at the left and on at the right;
       the widgets the scene's own */
    static int32_t               expert[2] = {0, 1}, gain = 40;
    static dgx_ui_textbox_t      expert_box = DGX_UI_TEXT_CONST("Expert", 1, &st_left), gain_box = DGX_UI_TEXT_CONST("Gain", 1, &st_left);
    static dgx_ui_textbox_t      calibrate_box = DGX_UI_TEXT_CONST("Calibrate", 1, &st_centre);
    static const dgx_ui_widget_t expert_w[2]   = {
        {.kind = DGX_UI_SWITCH, .max = 1, .text = &expert_box, .value = &expert[0], .look = &dgx_ui_look_checkbox},
        {.kind = DGX_UI_SWITCH, .max = 1, .text = &expert_box, .value = &expert[1], .look = &dgx_ui_look_checkbox}};
    static const dgx_ui_widget_t advanced_w   = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .gap = 4};
    static const dgx_ui_widget_t gain_title_w = {.kind = DGX_UI_LABEL, .text = &gain_box};
    static const dgx_ui_widget_t gain_w       = {.kind = DGX_UI_SLIDER, .max = 100, .want_h = 12, .value = &gain, .look = &dgx_ui_look_slider};
    static const dgx_ui_widget_t calibrate_w  = {.kind = DGX_UI_BUTTON, .want_h = 18, .text = &calibrate_box, .look = &dgx_ui_look_button};
    picture_screen(&paint[0]);
    for (int half = 0; half < 2; ++half) {
        paint[half] = (dgx_ui_paint_t){.scr = paint[0].scr, .colors = picture_colors, .color_count = COLORS, .background = PAPER};
        dgx_ui_init(&ui[half], nodes[half], 16, 2);
        dgx_ui_set_root(&ui[half], 160 * half, 0, 160, 240);
        const int root = dgx_ui_add(&ui[half], -1, &pic_root_w);
        dgx_ui_add(&ui[half], root, &expert_w[half]);
        const int box = dgx_ui_add(&ui[half], root, &advanced_w);
        dgx_ui_add(&ui[half], box, &gain_title_w), dgx_ui_add(&ui[half], box, &gain_w);
        const int button = dgx_ui_add(&ui[half], root, &calibrate_w);
        advanced_show(&ui[half], box, button, expert[half] != 0);
    }
    picture_pair_done(&ui[0], &paint[0], &ui[1], &paint[1], dir, "show.ppm");

    /* A bar over the pages: tab_build(), the clock's text set, Air tapped by the keys: its page, the scene's own frame
       holding air_add()'s buttons, over the place */
    static dgx_ui_node_t         air_nodes[8], log_nodes[2];
    static const dgx_ui_widget_t tab_page_w = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 4, .gap = 2};
    picture_screen(&paint[0]);
    dgx_ui_init(&ui[0], nodes[0], 16, 2);
    dgx_ui_init(&tab_air, air_nodes, 8, 2), air_add(&tab_air, dgx_ui_add(&tab_air, -1, &tab_page_w));
    dgx_ui_init(&tab_log, log_nodes, 2, 2), dgx_ui_add(&tab_log, -1, &tab_page_w);
    air_mode = AIR_AUTO, beep_wanted = 1;
    tab_build(&ui[0]);
    dgx_ui_text_set(&ui[0], 2, "Thu 26 Nov 10:42"); /* the clock, as tab_build() added it: the root, the bar, the clock */
    dgx_ui_focus(&ui[0], 3), dgx_ui_key(&ui[0], DGX_UI_KEY_ACTIVATE); /* the tab Air */
    dgx_ui_paint(&ui[0], &paint[0], 0);
    picture_write(paint[0].scr, dir, "bar.ppm", 240);
    dgx_ui_page_back(&ui[0]);
    dgx_ui_free(&tab_air), dgx_ui_free(&tab_log), dgx_ui_free(&ui[0]);
    dgx_ui_paint_free(&paint[0]), dgx_screen_destroy(&paint[0].scr);

    /* Pages that slide: the settings half way in from the right, over the home. The trees and their widgets are the
       scene's own, the button's on_event() and the page root's the recipe's, as slide_init() and slide_show() */
    static int32_t               slide_level = 70;
    static dgx_ui_textbox_t      home_box = DGX_UI_TEXT_CONST("Home", 1, &st_left), open_box = DGX_UI_TEXT_CONST("Settings", 1, &st_centre);
    static dgx_ui_textbox_t      settings_box     = DGX_UI_TEXT_CONST("Settings", 1, &st_left);
    static const dgx_ui_widget_t home_title_w     = {.kind = DGX_UI_LABEL, .text = &home_box};
    static const dgx_ui_widget_t home_settings_w  = {.kind = DGX_UI_BUTTON, .want_h = 18, .text = &open_box, .look = &dgx_ui_look_button, .on_event = on_slide_settings};
    static const dgx_ui_widget_t settings_page_w  = {.kind = DGX_UI_FRAME, .flags = DGX_UI_COLUMN, .pad = 6, .gap = 6, .on_event = on_slide_root};
    static const dgx_ui_widget_t settings_title_w = {.kind = DGX_UI_LABEL, .text = &settings_box};
    static const dgx_ui_widget_t settings_level_w = {.kind = DGX_UI_SLIDER, .max = 100, .want_h = 12, .value = &slide_level, .look = &dgx_ui_look_slider};
    picture_screen(&paint[0]);
    dgx_ui_init(&ui[0], nodes[0], 16, 2);
    int root = dgx_ui_add(&ui[0], -1, &pic_root_w);
    dgx_ui_add(&ui[0], root, &home_title_w);
    const int open = dgx_ui_add(&ui[0], root, &home_settings_w);
    dgx_ui_init(&page_settings, page_settings_nodes, 8, 2);
    root = dgx_ui_add(&page_settings, -1, &settings_page_w);
    dgx_ui_add(&page_settings, root, &settings_title_w), dgx_ui_add(&page_settings, root, &settings_level_w);
    pages_home = &ui[0], big_memory_there = true, page_slide = (dgx_ui_transition_t){.ms = 250};
    slide_init(&paint[0]);
    slide_show(&ui[0], &paint[0], NULL, 0);
    dgx_ui_focus(&ui[0], open), dgx_ui_key(&ui[0], DGX_UI_KEY_ACTIVATE);
    uint32_t now = 20;
    for (; page_slide.running && now <= 120; now += 20) slide_show(&ui[0], &paint[0], NULL, now);
    picture_write(paint[0].scr, dir, "slide.ppm", 240);
    for (; page_slide.running && now < 2000; now += 20) slide_show(&ui[0], &paint[0], NULL, now);
    if (page_slide.from) dgx_screen_destroy(&page_slide.from);
    if (page_slide.to) dgx_screen_destroy(&page_slide.to);
    while (big_count > 0) free(big_blocks[--big_count]);
    dgx_ui_free(&page_settings), dgx_ui_free(&ui[0]);
    dgx_ui_paint_free(&paint[0]), dgx_screen_destroy(&paint[0].scr);
}


/* the layout: a row and a column, a line between rows, one tree on every board, the screen turned */
static void layout_pictures(const char *dir)
{
    static dgx_ui_node_t nodes[24];
    dgx_ui_t             ui;
    dgx_ui_paint_t       paint;

    /* A row and a column that fit any screen: the recipe's page with its title, a row of a name and a value in the
       content, and in the bottom row two buttons of the scene's own; then the box of every widget in dashes */
    static dgx_ui_textbox_t      save_box = DGX_UI_TEXT_CONST("Save", 1, &st_centre), back_box = DGX_UI_TEXT_CONST("Back", 1, &st_centre);
    static const dgx_ui_widget_t bottom_button_w[2] = {
        {.kind = DGX_UI_BUTTON, .flags = DGX_UI_STRETCH | DGX_UI_REST, .text = &save_box, .look = &dgx_ui_look_button},
        {.kind = DGX_UI_BUTTON, .flags = DGX_UI_STRETCH | DGX_UI_REST, .text = &back_box, .look = &dgx_ui_look_button}};
    picture_screen(&paint);
    dgx_ui_init(&ui, nodes, 24, 2);
    dgx_ui_textbox_init(&title_box, title_buf, sizeof title_buf, 1, &st_left);
    dgx_ui_textbox_init(&name_box, name_buf, sizeof name_buf, 1, &st_left);
    dgx_ui_textbox_init(&value_box, value_buf, sizeof value_buf, 1, &st_left);
    int root = dgx_ui_add(&ui, -1, &page_w);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, root, &title_w), "Air");
    const int row = dgx_ui_add(&ui, dgx_ui_add(&ui, root, &content_w), &fit_row_w);
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, row, &name_w), "Temperature");
    dgx_ui_text_set(&ui, dgx_ui_add(&ui, row, &value_w), "21.5 C");
    const int bottom = dgx_ui_add(&ui, root, &bottom_w);
    dgx_ui_add(&ui, bottom, &bottom_button_w[0]), dgx_ui_add(&ui, bottom, &bottom_button_w[1]);
    dgx_ui_paint(&ui, &paint, 0);
    for (int i = 0; i < ui.count; ++i) {
        const dgx_ui_node_t *node = dgx_ui_node(&ui, i);
        dashes(paint.scr, node->x, node->y, node->w, node->h, dgx_screen_rgb(paint.scr, 255, 170, 60));
    }
    picture_write(paint.scr, dir, "rows.ppm", 240);
    dgx_ui_free(&ui), dgx_ui_paint_free(&paint), dgx_screen_destroy(&paint.scr);

    /* A line between rows: rows of the scene's own, across_w between them, and down_w between the two labels of the last */
    enum { LINE_ROWS = 3 };
    static dgx_ui_textbox_t line_text_box[LINE_ROWS][2] = {
        {DGX_UI_TEXT_CONST("Kitchen", 1, &st_left), DGX_UI_TEXT_CONST("21.5 C", 1, &st_left)},
        {DGX_UI_TEXT_CONST("Bedroom", 1, &st_left), DGX_UI_TEXT_CONST("19.0 C", 1, &st_left)},
        {DGX_UI_TEXT_CONST("Fan", 1, &st_left), DGX_UI_TEXT_CONST("Auto", 1, &st_left)}};
    static dgx_ui_widget_t       line_text_w[LINE_ROWS][2];
    static const dgx_ui_widget_t line_row_w = {.kind = DGX_UI_BOX, .flags = DGX_UI_ROW, .want_h = 10, .gap = 4};
    static const dgx_ui_widget_t lines_w    = {.kind = DGX_UI_BOX, .flags = DGX_UI_COLUMN, .pad = 6, .gap = 3};
    picture_screen(&paint);
    dgx_ui_init(&ui, nodes, 24, 2);
    root = dgx_ui_add(&ui, -1, &lines_w);
    for (int k = 0; k < LINE_ROWS; ++k) {
        line_text_w[k][0] = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .flags = DGX_UI_REST, .text = &line_text_box[k][0]};
        line_text_w[k][1] = (dgx_ui_widget_t){.kind = DGX_UI_LABEL, .want_w = 40, .text = &line_text_box[k][1]};
        if (k) dgx_ui_add(&ui, root, &across_w);
        const int line = dgx_ui_add(&ui, root, &line_row_w);
        dgx_ui_add(&ui, line, &line_text_w[k][0]);
        if (k == LINE_ROWS - 1) dgx_ui_add(&ui, line, &down_w);
        dgx_ui_add(&ui, line, &line_text_w[k][1]);
    }
    picture_done(&ui, &paint, dir, "line.ppm");

    /* One tree on every board, and the screen turned: face_add() on a screen of 320x240 at its scale; then the same
       tree painted on a screen of 240x320, as a display turned to stand gives it (a virtual screen of DGX is not turned
       itself: the painter sees the other size and lays the tree again); the two side by side, 16 pixels apart */
    static uint32_t wide[320 * 240], tall[240 * 320], both[(320 + 16 + 240) * 320];
    dgx_screen_t   *standing = dgx_vscreen_init(240, 320, 16, DgxScreenRGB);
    picture_screen(&paint);
    face_ui_init(&ui, nodes, 24);
    face_add(&ui, -1);
    dgx_ui_paint(&ui, &paint, 0);
    picture_pixels(paint.scr, 240, wide);
    picture_write(paint.scr, dir, "face.ppm", 240);
    dgx_screen_t *lying = paint.scr;
    paint.scr           = standing;
    dgx_ui_paint(&ui, &paint, 10);
    picture_pixels(standing, 320, tall);
    enum { BOTH_W = 320 + 16 + 240 };
    for (int k = 0; k < BOTH_W * 320; ++k) both[k] = 0xffffffffu;
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x) both[y * BOTH_W + x] = wide[y * 320 + x];
    for (int y = 0; y < 320; ++y)
        for (int x = 0; x < 240; ++x) both[y * BOTH_W + 336 + x] = tall[y * 240 + x];
    if (dir) {
        char path[600];
        snprintf(path, sizeof path, "%s/turn.ppm", dir);
        FILE *file = fopen(path, "wb");
        fprintf(file, "P6\n%d %d\n255\n", BOTH_W, 320);
        for (int k = 0; k < BOTH_W * 320; ++k) fputc(both[k] >> 16 & 255, file), fputc(both[k] >> 8 & 255, file), fputc(both[k] & 255, file);
        fclose(file);
    }
    dgx_ui_free(&ui), dgx_ui_paint_free(&paint), dgx_screen_destroy(&lying), dgx_screen_destroy(&standing);
}

/* a widget's own: a field, a look of your own, a widget of your own, a canvas, a graph */
static void own_pictures(const char *dir)
{
    static dgx_ui_node_t nodes[24];
    dgx_ui_t             ui;
    dgx_ui_paint_t       paint;

    /* A field and a keyboard: pass_add(), a text typed, the focus on it and the caret shown; the keyboard is DGX-DI's */
    picture_screen(&paint), pic_lines = 0;
    dgx_ui_init(&ui, nodes, 24, 2);
    int       root  = dgx_ui_add(&ui, -1, &pic_root_w);
    const int field = pass_add(&ui, picture_line(&ui, root, "Password", 14));
    pic_caption_w[pic_lines - 1].at_y = 0; /* the field's text stands at its top, and the caption beside it */
    static const uint32_t typed[] = {'g', 'a', 'r', 'd', 'e', 'n', '-', '4', '2'};
    dgx_ui_focus(&ui, field), dgx_ui_typed(&ui, typed, sizeof typed / sizeof *typed);
    picture_done(&ui, &paint, dir, "field.ppm");

    /* A look of your own: level_w at 60 */
    picture_screen(&paint), pic_lines = 0;
    dgx_ui_init(&ui, nodes, 24, 2);
    root          = dgx_ui_add(&ui, -1, &pic_root_w);
    level_percent = 60;
    dgx_ui_add(&ui, picture_line(&ui, root, "Level", 6), &level_w);
    picture_done(&ui, &paint, dir, "own-look.ppm");

    /* A widget of your own that the finger and the keys work: the pad at 70 across and 30 down, with the focus; under it
       the level bar that is tapped */
    picture_screen(&paint), pic_lines = 0;
    dgx_ui_init(&ui, nodes, 24, 2);
    root      = dgx_ui_add(&ui, -1, &pic_root_w);
    xy_x      = 70, xy_data.y = 30;
    const int pad = dgx_ui_add(&ui, picture_line(&ui, root, "Pad", 40), &xy_w);
    dgx_ui_add(&ui, picture_line(&ui, root, "Tap", 6), &tap_w);
    dgx_ui_focus(&ui, pad);
    picture_done(&ui, &paint, dir, "own-widget.ppm");

    /* A picture that stays between passes, and a graph that sweeps: 120 values of CO2 in each */
    for (int graph = 0; graph < 2; ++graph) {
        picture_screen(&paint), pic_lines = 0;
        dgx_ui_init(&ui, nodes, 24, 2);
        root          = dgx_ui_add(&ui, -1, &pic_root_w);
        history_count = 0, trace_count = 0;
        for (int k = 0; k < 120; ++k) { /* a room through a day: the air worse while people are in, better when a window opens */
            const int ppm = graph ? 600 + 900 * (k % 40) / 40 + 60 * (k * 7 % 3) : 450 + (k < 60 ? 18 * k : 1080 - 14 * (k - 60)) + 30 * (k * 5 % 4);
            if (graph) trace_add(ppm);
            else history_add(ppm);
        }
        dgx_ui_add(&ui, picture_line(&ui, root, "CO2", 20), graph ? &trace_w : &history_w);
        picture_done(&ui, &paint, dir, graph ? "graph.ppm" : "canvas.ppm");
    }
}

static void recipe_pictures(const char *dir)
{
    control_pictures(dir);
    text_pictures(dir);
    page_pictures(dir);
    layout_pictures(dir);
    own_pictures(dir);
}
