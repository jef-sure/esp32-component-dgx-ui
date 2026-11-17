#pragma once
#include "freertos/FreeRTOS.h"
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned length, unsigned item_size);
BaseType_t    xQueueSend(QueueHandle_t q, const void *item, TickType_t wait);
BaseType_t    xQueueReceive(QueueHandle_t q, void *item, TickType_t wait);
BaseType_t    xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t *woken);
