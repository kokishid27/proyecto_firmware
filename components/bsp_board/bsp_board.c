/**
 * @file bsp_board.c
 * @brief Implementación del BSP mapeando los pines al HAL
 */

#include <stddef.h>
#include "bsp_board.h"
#include "hal_board.h"  // Tu archivo HAL
#include "stdio.h"

//**************************** */
//  MAPEADO FÍSICO (Privado para el BSP)
//***************** */

#define PIN_LED_SISTEMA     5
#define PIN_BUTTON_1        18
#define PIN_BUTTON_2        19
#define PIN_LED_RGB_ROJO    14
#define PIN_LED_RGB_VERDE   13
#define PIN_LED_RGB_AZUL    12
#define PIN_LED_4           2
#define PIN_LED_5           4
#define PIN_LED_16          16
#define PIN_LED_17          17

#define NUM_BUTTONS 2
static bsp_button_callback_t cb_boton [NUM_BUTTONS] = {NULL};
static void *arg_boton [NUM_BUTTONS] = {NULL};


//  FUNCIONES PRIVADAS DE AYUDA


// Función interna para traducir el Enum del BSP al pin de hardware real
static uint8_t bsp_get_led_pin(bsp_led_t led) {
    switch(led) {
        case BSP_LED_SISTEMA:   return PIN_LED_SISTEMA;
        case BSP_LED_RGB_ROJO:  return PIN_LED_RGB_ROJO;
        case BSP_LED_RGB_VERDE: return PIN_LED_RGB_VERDE;
        case BSP_LED_RGB_AZUL:  return PIN_LED_RGB_AZUL;
        case BSP_LED_4:         return PIN_LED_4;
        case BSP_LED_3:         return PIN_LED_5;
        case BSP_LED_2:        return PIN_LED_16;
        case BSP_LED_1:        return PIN_LED_17;
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



// Esta es la ISR real conectada al hardware
static void IRAM_ATTR isr_button_handler( void *arg) {
    bsp_button_t btn = (bsp_button_t)(uintptr_t)arg;

    // Validación de seguridad y ejecución directa
    if (btn < NUM_BUTTONS && cb_boton[btn] != NULL) {
        cb_boton[btn](arg_boton[btn]);
    }
}

// Función que la API expone al Main
void bsp_button_register_callback(bsp_button_t btn, bsp_button_callback_t callback, void *arg) {
    if (btn < NUM_BUTTONS) {
        cb_boton[btn] = callback;
        arg_boton[btn] = arg;
    }

}

// ************************************************************************** */
//  IMPLEMENTACIÓN DE LA API
// ************************************************************************** */

bool bsp_board_init(void) {

    // 1. Configurar LEDs (Salidas, sin pull, inician apagados)
    hal_gpio_cfg_t led_cfg = {
        .dir = HAL_GPIO_OUTPUT,
         .logic_type=HAL_POSITIVE_LOGIC // Activo Alto
    };
    hal_gpio_cfg_t rgb_cfg = {
        .dir = HAL_GPIO_OUTPUT,
        .logic_type=HAL_NEGATIVE_LOGIC // Activo Bajo
     };

    // Arreglo temporal para inicializar rápidamente todos los LEDs en un bucle
    uint8_t pines_leds[] = {
        PIN_LED_SISTEMA, PIN_LED_4, PIN_LED_5, PIN_LED_16, PIN_LED_17
    };
    uint8_t pines_rgb[] = {
        PIN_LED_RGB_ROJO, PIN_LED_RGB_VERDE, PIN_LED_RGB_AZUL
    };

    for(int i = 0; i < (sizeof(pines_rgb)/sizeof(pines_rgb[0])); i++) {
        rgb_cfg.pin = pines_rgb[i];
        if (hal_gpio_init(&rgb_cfg) != HAL_OK) return false;
    }
    for(int i = 0; i < (sizeof(pines_leds)/sizeof(pines_leds[0])); i++) {
        led_cfg.pin = pines_leds[i];
        if (hal_gpio_init(&led_cfg) != HAL_OK) return false;
    }
    

     hal_gpio_cfg_t btn1_cfg[2] = {
        {
            .pin = PIN_BUTTON_1,
            .dir = HAL_GPIO_INPUT,
            .pull = HAL_GPIO_PULLUP, // Usamos Pull-Up para que el estado normal sea HIGH
            .intr_type = HAL_GPIO_INTR_NEG_EDGE, // Interrupción por flanco de bajada (presionar el botón)
            .intr_cb = isr_button_handler, // Nuestra función ISR común para ambos botones
            .intr_arg = (void *)(uintptr_t)BSP_BUTTON_1
 // Pasamos el identificador del botón como argumento
        },
        {
            .pin = PIN_BUTTON_2,
            .dir = HAL_GPIO_INPUT,
            .pull = HAL_GPIO_PULLUP, // Usamos Pull-Up para que el estado normal sea HIGH
        }
    };

        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (hal_gpio_init(&btn1_cfg[i]) != HAL_OK) return false;
        }
    

    return true; // Todo se inicializó correctamente
}

void bsp_led_set(bsp_led_t led, bool level) {
    uint8_t pin = bsp_get_led_pin(led);
    if (pin != 255) {
            hal_gpio_write(pin, level);
    }
}

void bsp_led_toggle(bsp_led_t led) {
    uint8_t pin = bsp_get_led_pin(led);
    if (pin != 255) {
        hal_gpio_toggle(pin);
    }
}
void bsp_led_rgb_set(bool rojo, bool verde, bool azul) {
            hal_gpio_write(BSP_LED_RGB_ROJO, rojo);
            hal_gpio_write(BSP_LED_RGB_VERDE, verde);
            hal_gpio_write(BSP_LED_RGB_AZUL, azul);
}

bool bsp_button_is_pressed(bsp_button_t btn) {
    uint8_t pin = bsp_get_button_pin(btn);
    if (pin == 255) return false;

    hal_gpio_level_e estado;
    hal_gpio_read(pin, &estado);
    return (estado == HAL_GPIO_LOW); // Botón presionado si el estado es LOW (Pull-Up)
}