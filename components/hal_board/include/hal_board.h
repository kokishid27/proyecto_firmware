#ifndef HAL_BOARD_H
#define HAL_BOARD_H

#include "driver_gpio.h"

typedef enum {
    GPIO_MODE_INPUT,
    GPIO_MODE_OUTPUT
} gpio_mode_e;
typedef struct{
    uint8_t pin_num;
    gpio_mode_e mode;
    pull_mode_e pull_mode;
} gpio_config_t;

gpio_err_e hal_gpio_init(gpio_config_t *config);
gpio_err_e hal_gpio_write_pin(uint8_t pin);
bool hal_gpio_read_pin(uint8_t pin, bool value);

#endif /* HAL_BOARD_H */