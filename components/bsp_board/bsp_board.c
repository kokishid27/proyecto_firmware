/**
 * @file bsp_board.c
 * @brief Implementación del BSP mapeando los pines al HAL
 */

#include "bsp_board.h"
#include "hal_board.h"  // Tu archivo HAL

//**************************** */
//  MAPEADO FÍSICO (Privado para el BSP)
//***************** */

#define PIN_LED_SISTEMA     2
#define PIN_BUTTON_1        18
#define PIN_BUTTON_2        19
#define PIN_LED_RGB_ROJO    12
#define PIN_LED_RGB_VERDE   13
#define PIN_LED_RGB_AZUL    14
#define PIN_LED_4           4
#define PIN_LED_5           5
#define PIN_LED_16          16
#define PIN_LED_17          17


//  FUNCIONES PRIVADAS DE AYUDA


// Función interna para traducir el Enum del BSP al pin de hardware real
static uint8_t bsp_get_led_pin(bsp_led_t led) {
    switch(led) {
        case BSP_LED_SISTEMA:   return PIN_LED_SISTEMA;
        case BSP_LED_RGB_ROJO:  return PIN_LED_RGB_ROJO;
        case BSP_LED_RGB_VERDE: return PIN_LED_RGB_VERDE;
        case BSP_LED_RGB_AZUL:  return PIN_LED_RGB_AZUL;
        case BSP_LED_4:         return PIN_LED_4;
        case BSP_LED_5:         return PIN_LED_5;
        case BSP_LED_16:        return PIN_LED_16;
        case BSP_LED_17:        return PIN_LED_17;
        default:                return 255; // Pin inválido
    }
}

static uint8_t bsp_get_button_pin(bsp_button_t btn) {
    switch(btn) {
        case BSP_BUTTON_1: return PIN_BUTTON_1;
        case BSP_BUTTON_2: return PIN_BUTTON_2;
        default:           return 255; // Pin inválido
    }
}

// ************************************************************************** */
//  IMPLEMENTACIÓN DE LA API
// ************************************************************************** */

bool bsp_board_init(void) {
    hal_err_e err = HAL_OK;

    // 1. Configurar LEDs (Salidas, sin pull, inician apagados)
    hal_gpio_cfg_t led_cfg = {
        .dir = HAL_GPIO_OUTPUT,
        .pull = HAL_GPIO_NOPULL,
    };

    // Arreglo temporal para inicializar rápidamente todos los LEDs en un bucle
    uint8_t pines_leds[] = {
        PIN_LED_SISTEMA, PIN_LED_RGB_ROJO, PIN_LED_RGB_VERDE, PIN_LED_RGB_AZUL,
        PIN_LED_4, PIN_LED_5, PIN_LED_16, PIN_LED_17
    };

    for(int i = 0; i < (sizeof(pines_leds)/sizeof(pines_leds[0])); i++) {
        led_cfg.pin = pines_leds[i];
        if (hal_gpio_init(&led_cfg) != HAL_OK) return false;
    }

    // 2. Configurar Botones (Entradas, con Pull-Up asumiendo que conectan a GND)
    hal_gpio_cfg_t btn_cfg = {
        .dir = HAL_GPIO_INPUT,
        .pull = HAL_GPIO_PULLUP,
    };

    btn_cfg.pin = PIN_BUTTON_1;
    if (hal_gpio_init(&btn_cfg) != HAL_OK) return false;

    btn_cfg.pin = PIN_BUTTON_2;
    if (hal_gpio_init(&btn_cfg) != HAL_OK) return false;

    return true; // Todo se inicializó correctamente
}

void bsp_led_set(bsp_led_t led, bool encender) {
    uint8_t pin = bsp_get_led_pin(led);
    if (pin != 255) {
        hal_gpio_level_e nivel = encender ? HAL_GPIO_HIGH : HAL_GPIO_LOW;
        hal_gpio_write(pin, nivel);
    }
}

void bsp_led_toggle(bsp_led_t led) {
    uint8_t pin = bsp_get_led_pin(led);
    if (pin != 255) {
        hal_gpio_toggle(pin);
    }
}

bool bsp_button_is_pressed(bsp_button_t btn) {
    uint8_t pin = bsp_get_button_pin(btn);
    if (pin == 255) return false;

    hal_gpio_level_e estado;
    hal_gpio_read(pin, &estado);

    // Si usamos Pull-Up, presionar el botón conecta a GND (LOW), por lo tanto:
    return (estado == HAL_GPIO_LOW); 
}