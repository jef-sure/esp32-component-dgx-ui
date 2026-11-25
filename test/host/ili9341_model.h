#pragma once
/*
 * An ILI9341 behind a bus, as DGX's own host test of the panel's scroll models it (test/host/test_panel_scroll.c
 * there): the controller's memory as its datasheet has it, MADCTL, the window, the vertical scroll's area and its
 * start, and how many times each cell of memory was written. Copied, as DGX keeps it inside a test of its own; the
 * driver is DGX's (drivers/panel.c, drivers/ili9341.c), compiled into the test. Upright the screen is 240 x 320 and a
 * move along y is the panel's; lying, a move along y is across its rows, which the panel cannot make.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"

#include "drivers/dgx_lcd_init.c"
#include "drivers/panel.c"
#include "drivers/ili9341.c"

#include "dgx_draw.h"

enum { LCD_COLUMNS = 240, LCD_PAGES = 320 };

static uint16_t lcd_memory[LCD_PAGES][LCD_COLUMNS];
static uint8_t  lcd_writes[LCD_PAGES][LCD_COLUMNS];
static uint8_t  lcd_cmd, lcd_madctl, lcd_params[16], lcd_half[2];
static int      lcd_params_n, lcd_half_n;
static int      lcd_sc, lcd_ec, lcd_sp, lcd_ep, lcd_column, lcd_page;
static int      lcd_tfa, lcd_vsa = LCD_PAGES, lcd_bfa, lcd_vsp;
static long     lcd_pixels, lcd_bad;

static void lcd_command(uint8_t value)
{
    lcd_cmd = value, lcd_params_n = 0, lcd_half_n = 0;
    if (value == 0x2c) lcd_column = lcd_sc, lcd_page = lcd_sp;
}

static void lcd_pixel(uint16_t color)
{
    if (!(lcd_sc <= lcd_ec && lcd_sp <= lcd_ep)) ++lcd_bad;
    const bool swap = lcd_madctl & 0x20;
    int        row = swap ? lcd_column : lcd_page, col = swap ? lcd_page : lcd_column;
    if (lcd_madctl & 0x80) row = LCD_PAGES - 1 - row;
    if (lcd_madctl & 0x40) col = LCD_COLUMNS - 1 - col;
    if (row >= 0 && row < LCD_PAGES && col >= 0 && col < LCD_COLUMNS) {
        lcd_memory[row][col] = color;
        if (lcd_writes[row][col] < 255) ++lcd_writes[row][col];
    } else {
        ++lcd_bad;
    }
    ++lcd_pixels;
    if (++lcd_column > lcd_ec) {
        lcd_column = lcd_sc;
        if (++lcd_page > lcd_ep) lcd_page = lcd_sp;
    }
}

static void lcd_data(const uint8_t *data, uint32_t bytes)
{
    for (uint32_t i = 0; i < bytes; ++i) {
        if (lcd_cmd == 0x2c) {
            lcd_half[lcd_half_n++] = data[i];
            if (lcd_half_n == 2) lcd_pixel((uint16_t)(lcd_half[0] << 8 | lcd_half[1])), lcd_half_n = 0;
            continue;
        }
        if (lcd_params_n < (int)sizeof lcd_params) lcd_params[lcd_params_n++] = data[i];
        const uint8_t *p = lcd_params;
        if (lcd_cmd == 0x36) lcd_madctl = p[0];
        if (lcd_cmd == 0x2a && lcd_params_n == 4) lcd_sc = p[0] << 8 | p[1], lcd_ec = p[2] << 8 | p[3];
        if (lcd_cmd == 0x2b && lcd_params_n == 4) lcd_sp = p[0] << 8 | p[1], lcd_ep = p[2] << 8 | p[3];
        if (lcd_cmd == 0x33 && lcd_params_n == 6) {
            lcd_tfa = p[0] << 8 | p[1], lcd_vsa = p[2] << 8 | p[3], lcd_bfa = p[4] << 8 | p[5];
            if (lcd_tfa + lcd_vsa + lcd_bfa != LCD_PAGES) ++lcd_bad;
        }
        if (lcd_cmd == 0x37 && lcd_params_n == 2) {
            lcd_vsp = p[0] << 8 | p[1];
            if (lcd_vsp < lcd_tfa || lcd_vsp >= lcd_tfa + lcd_vsa) ++lcd_bad;
        }
    }
}

/* what a place of the panel shows: a row of the area scrolled is shown from the row vsp of memory on */
static uint16_t lcd_shows(int row, int col)
{
    if (row >= lcd_tfa && row < lcd_tfa + lcd_vsa) row = lcd_tfa + (row - lcd_tfa + lcd_vsp - lcd_tfa) % lcd_vsa;
    return lcd_memory[row][col];
}

static uint8_t lcd_bus_buffer[1024];
static void    lcd_bus_command(struct _dgx_bus_protocols_t *bus, uint8_t cmd) { (void)bus, lcd_command(cmd); }
static void    lcd_bus_data(struct _dgx_bus_protocols_t *bus, const uint8_t *data, uint32_t len) { (void)bus, lcd_data(data, len / 8); }
static void    lcd_bus_sync(struct _dgx_bus_protocols_t *bus) { (void)bus; }

static dgx_bus_protocols_t lcd_bus = {.bus_type = DGX_BUS_I2C, .write_command = lcd_bus_command, .write_data = lcd_bus_data, .write_data_async = lcd_bus_data,
                                      .sync_write = lcd_bus_sync, .buffer = lcd_bus_buffer, .buffer_len = sizeof lcd_bus_buffer};

/* the panel, upright (a move along y the panel's) or lying (along x) */
static dgx_screen_t *lcd_make(bool lying)
{
    lcd_tfa = 0, lcd_vsa = LCD_PAGES, lcd_bfa = 0, lcd_vsp = 0;
    dgx_screen_t *scr = dgx_panel_init(&lcd_bus, &dgx_panel_ili9341, GPIO_NUM_NC, GPIO_NUM_NC, 16, DgxScreenRGB);
    if (scr && lying) dgx_screen_orientation(scr, DgxScreenLeftRight, DgxScreenTopBottom, true);
    return scr;
}

/* where a pixel of the screen is on the panel: the driver's upright picture has no MY */
static void lcd_on_panel(const dgx_screen_t *scr, int x, int y, int *row, int *col)
{
    const int along = scr->swap_xy ? x : y, across = scr->swap_xy ? y : x;
    *row = scr->dir_y == DgxScreenBottomTop ? LCD_PAGES - 1 - along : along;
    *col = scr->dir_x == DgxScreenRightLeft ? LCD_COLUMNS - 1 - across : across;
}

/* the pixels the panel shows otherwise than ref holds */
static int lcd_mismatches(const dgx_screen_t *scr, dgx_screen_t *ref)
{
    int wrong = 0;
    for (int y = 0; y < scr->height; ++y)
        for (int x = 0; x < scr->width; ++x) {
            int row, col;
            lcd_on_panel(scr, x, y, &row, &col);
            wrong += lcd_shows(row, col) != (uint16_t)dgx_get_pixel(ref, x, y);
        }
    return wrong;
}

/* the counts of the writes from now on; the cells written more than once since */
static void lcd_count_from_now(void)
{
    memset(lcd_writes, 0, sizeof lcd_writes);
    lcd_pixels = 0;
}

static int lcd_twice(void)
{
    int twice = 0;
    for (int r = 0; r < LCD_PAGES; ++r)
        for (int c = 0; c < LCD_COLUMNS; ++c) twice += lcd_writes[r][c] > 1;
    return twice;
}
