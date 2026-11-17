# no REQUIRES: main takes every component, dgx-ui under the name of its checkout's directory, DGX-DI from the registry
idf_component_register(SRCS "controls.c" "keypad.c")
