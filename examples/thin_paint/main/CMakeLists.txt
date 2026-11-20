# no REQUIRES: main takes every component, dgx-ui under the name of its checkout's directory
idf_component_register(SRCS "thin_paint.c" "cxx_check.cpp")
# the fonts, made at build time by DGX from a face of the build machine that ../fonts.rules picks, both with soft
# edges, 4 bits a pixel: the CO2 value large, of the characters its line shows alone; the clock and the build number
# small, of every character of this program's source
dgx_fonts_rules(../fonts.rules)
dgx_font_need(large SIZE 32px BITS 4 LETTERS "CO2 pm0123456789-")
dgx_font_need(small SIZE 13px BITS 4 TEXTS thin_paint.c)
