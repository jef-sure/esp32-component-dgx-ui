#pragma once
/* the few names of ESP-IDF's driver/gpio.h the recipes use, for the checks on the computer */
#include <stdint.h>

#include "esp_err.h"

typedef int gpio_num_t;
#define GPIO_NUM_NC (-1)
enum { GPIO_NUM_21 = 21, GPIO_NUM_22 = 22 }; /* the I2C of "A monochrome screen" */
typedef enum { GPIO_MODE_INPUT = 1 } gpio_mode_t;
typedef enum { GPIO_INTR_DISABLE, GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE, GPIO_INTR_ANYEDGE, GPIO_INTR_LOW_LEVEL, GPIO_INTR_HIGH_LEVEL } gpio_int_type_t;
typedef struct {
    uint64_t        pin_bit_mask;
    gpio_mode_t     mode;
    int             pull_up_en, pull_down_en;
    gpio_int_type_t intr_type;
} gpio_config_t;

esp_err_t gpio_config(const gpio_config_t *config);
esp_err_t gpio_install_isr_service(int flags);
esp_err_t gpio_isr_handler_add(gpio_num_t pin, void (*handler)(void *), void *arg);
esp_err_t gpio_intr_enable(gpio_num_t pin);
esp_err_t gpio_intr_disable(gpio_num_t pin);
