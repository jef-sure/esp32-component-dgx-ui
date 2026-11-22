# no REQUIRES: main takes every component, dgx-ui under the name of its checkout's directory, DGX-DI from the registry
idf_component_register(SRCS "controls.c" "keypad.c")
# the fonts, made at build time by DGX from a face of the build machine that ../fonts.rules picks, with soft edges, 4
# bits a pixel: the widgets' letters, of every character of this program's source; the keypad's digits, which DGX-DI
# draws into its canvas, a virtual screen, so that DGX mixes each edge with the key under it, and an H, by which it puts
# them in the middle of a key
dgx_fonts_rules(../fonts.rules)
dgx_font_need(text SIZE 14px BITS 4 TEXTS controls.c)
dgx_font_need(keys SIZE 20px BITS 4 LETTERS "0123456789H")
