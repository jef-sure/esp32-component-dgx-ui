# no REQUIRES: main takes every component, dgx-ui under the name of its checkout's directory, DGX-DI from the registry
idf_component_register(SRCS "controls.c" "keypad.c")
# the fonts, made at build time by DGX from a face of the build machine that ../fonts.rules picks: the widgets' letters
# with soft edges, 4 bits a pixel, of every character of this program's source; the keypad's digits of one bit, as
# DGX-DI draws its labels straight on the display, with no paper to mix soft edges with, and an H, by which it puts
# them in the middle of a key
dgx_fonts_rules(../fonts.rules)
dgx_font_need(text SIZE 14px BITS 4 TEXTS controls.c)
dgx_font_need(keys SIZE 20px LETTERS "0123456789H")
