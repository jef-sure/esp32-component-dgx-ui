#pragma once
#include "freertos/FreeRTOS.h"
void       vTaskDelay(TickType_t ticks);
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned prio, void *handle);
#define portYIELD_FROM_ISR() ((void)0)
