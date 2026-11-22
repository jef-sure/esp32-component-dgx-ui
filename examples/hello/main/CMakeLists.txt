# dgx-ui is not named here: the component manager makes what idf_component.yml names a requirement of main. esp_timer
# gives the loop its clock; a component the program comes to need is named beside it
idf_component_register(SRCS "main.c" PRIV_REQUIRES esp_timer)
