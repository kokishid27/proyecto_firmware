/**
 * @file bsp_board.c
 * @brief Implementación del BSP mapeando los pines al HAL
 *
 * Este archivo actúa como el traductor entre los nombres lógicos de los 
 * componentes de la placa (por ejemplo, "LED_SISTEMA" o "BOTON_1") y sus 
 * conexiones físicas reales en el hardware (los números de los pines).
 */

#include <stddef.h>
#include <stdint.h>
#include "bsp_board.h"
#include "hal_board.h"
#include <stdio.h>

/* =========================================================================
 * MAPEADO FÍSICO (Privado para el BSP)
 * ========================================================================= */

// Asignación de los números de pines físicos del microcontrolador
#define PIN_LED_SISTEMA     5
#define PIN_BUTTON_1        18
#define PIN_BUTTON_2        19
#define PIN_LED_RGB_ROJO    14
#define PIN_LED_RGB_VERDE   13
#define PIN_LED_RGB_AZUL    12
#define PIN_LED_4           2
#define PIN_LED_3           4
#define PIN_LED_2           16
#define PIN_LED_1           17

// Cantidad de botones físicos disponibles en la placa
#define NUM_BUTTONS 2

// Arreglos para guardar las funciones de respuesta (callbacks) y sus argumentos
// para cada botón cuando se genere una interrupción.
static bsp_button_callback_t cb_boton[NUM_BUTTONS] = {NULL};
static void *arg_boton[NUM_BUTTONS] = {NULL};

/* =========================================================================
 * FUNCIONES PRIVADAS DE AYUDA
 * ========================================================================= */

/**
 * @brief Traduce la etiqueta del LED a su pin físico.
 *
 * @param led Etiqueta lógica del LED.
 * @return El número de pin correspondiente, o 255 si la etiqueta no existe.
 */
static uint8_t bsp_get_led_pin(bsp_led_t led) {
    switch(led) {
        case BSP_LED_SISTEMA:   return PIN_LED_SISTEMA;
        case BSP_LED_RGB_ROJO:  return PIN_LED_RGB_ROJO;
        case BSP_LED_RGB_VERDE: return PIN_LED_RGB_VERDE;
        case BSP_LED_RGB_AZUL:  return PIN_LED_RGB_AZUL;
        case BSP_LED_4:         return PIN_LED_4;
        case BSP_LED_3:         return PIN_LED_3;
        case BSP_LED_2:         return PIN_LED_2;
        case BSP_LED_1:         return PIN_LED_1;
        default:                return 255; // Código de error para pin inválido
    }
}

/**
 * @brief Traduce la etiqueta del botón a su pin físico.
 *
 * @param btn Etiqueta lógica del botón.
 * @return El número de pin correspondiente, o 255 si la etiqueta no existe.
 */
static uint8_t bsp_get_button_pin(bsp_button_t btn) {
    switch(btn) {
        case BSP_BUTTON_1: return PIN_BUTTON_1;
        case BSP_BUTTON_2: return PIN_BUTTON_2;
        default:           return 255; // Código de error para pin inválido
    }
}

/**
 * @brief Rutina de atención de interrupción (ISR) para los botones.
 *
 * Esta función es llamada automáticamente por el hardware cuando se presiona un botón.
 * Identifica qué botón causó la interrupción y ejecuta su función asociada.
 *
 * @param arg Argumento que contiene la identificación del botón que generó el aviso.
 */
static void IRAM_ATTR isr_button_handler(void *arg) {
    // Recupero la etiqueta del botón desde el argumento
    bsp_button_t btn = (bsp_button_t)(uintptr_t)arg;

    // Verifico que el botón sea válido y que tenga una función registrada antes de ejecutarla
    if (btn < NUM_BUTTONS && cb_boton[btn] != NULL) {
        cb_boton[btn](arg_boton[btn]);
    }
}

/* =========================================================================
 * IMPLEMENTACIÓN DE LA API
 * ========================================================================= */

/**
 * @brief Enlaza una función de la aplicación al evento de presionar un botón.
 *
 * @param btn      Botón al que se le quiere asignar la función.
 * @param callback Función que se ejecutará cuando el botón sea presionado.
 * @param arg      Datos opcionales que se le entregarán a la función.
 */
void bsp_button_register_callback(bsp_button_t btn, bsp_button_callback_t callback, void *arg) {
    if (btn < NUM_BUTTONS) {
        // Guardo la función y su argumento en mis arreglos internos
        cb_boton[btn] = callback;
        arg_boton[btn] = arg;
    }
}

/**
 * @brief Inicializa todos los componentes de la placa (LEDs y botones).
 *
 * Configura las direcciones de los pines, el tipo de lógica eléctrica que usan
 * y las interrupciones iniciales necesarias.
 *
 * @return true si todos los componentes se configuraron correctamente, false en caso de error.
 */
bool bsp_board_init(void) {

    // 1. Configuración base para los LEDs estándar (encienden con voltaje alto)
    hal_gpio_cfg_t led_cfg = {
        .dir = HAL_GPIO_OUTPUT,
        .logic_type = HAL_POSITIVE_LOGIC
    };
    
    // 2. Configuración base para el LED RGB (enciende con voltaje bajo)
    hal_gpio_cfg_t rgb_cfg = {
        .dir = HAL_GPIO_OUTPUT,
        .logic_type = HAL_NEGATIVE_LOGIC
    };

    // Agrupo los pines físicos en arreglos para inicializarlos más rápido usando bucles
    uint8_t pines_leds[] = {
        PIN_LED_SISTEMA, PIN_LED_1, PIN_LED_2, PIN_LED_3, PIN_LED_4
    };
    uint8_t pines_rgb[] = {
        PIN_LED_RGB_ROJO, PIN_LED_RGB_VERDE, PIN_LED_RGB_AZUL
    };

    // Inicializo los LEDs RGB
    for(int i = 0; i < (sizeof(pines_rgb)/sizeof(pines_rgb[0])); i++) {
        rgb_cfg.pin = pines_rgb[i];
        if (hal_gpio_init(&rgb_cfg) != HAL_OK) return false;
    }
    
    // Inicializo los LEDs estándar
    for(int i = 0; i < (sizeof(pines_leds)/sizeof(pines_leds[0])); i++) {
        led_cfg.pin = pines_leds[i];
        if (hal_gpio_init(&led_cfg) != HAL_OK) return false;
    }
    
    // 3. Configuración independiente de cada botón
    hal_gpio_cfg_t btn1_cfg[2] = {
        {
            .pin = PIN_BUTTON_1,
            .dir = HAL_GPIO_INPUT,
            .pull = HAL_GPIO_PULLUP, // Mantiene el pin en HIGH cuando no está presionado
            .intr_type = HAL_GPIO_INTR_NEG_EDGE, // Dispara el aviso al caer el voltaje (al presionar)
            .intr_cb = isr_button_handler, // Enlazo la función ISR principal
            .intr_arg = (void *)(uintptr_t)BSP_BUTTON_1 // Paso el ID del botón como dato
        },
        {
            .pin = PIN_BUTTON_2,
            .dir = HAL_GPIO_INPUT,
            .pull = HAL_GPIO_PULLUP, // Este botón solo se lee de forma manual, sin interrupción
        }
    };

    // Inicializo los botones
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (hal_gpio_init(&btn1_cfg[i]) != HAL_OK) return false;
    }
    
    return true; // Todo el hardware quedó listo
}

/**
 * @brief Enciende o apaga un LED específico.
 *
 * @param led   Etiqueta lógica del LED que se quiere controlar.
 * @param level true para encender, false para apagar.
 */
void bsp_led_set(bsp_led_t led, bool level) {
    uint8_t pin = bsp_get_led_pin(led);
    if (pin != 255) {
        hal_gpio_write(pin, level);
    }
}

/**
 * @brief Invierte el estado actual de un LED específico.
 *
 * @param led Etiqueta lógica del LED que se quiere alternar.
 */
void bsp_led_toggle(bsp_led_t led) {
    uint8_t pin = bsp_get_led_pin(led);
    if (pin != 255) {
        hal_gpio_toggle(pin);
    }
}

/**
 * @brief Controla los tres colores del LED RGB simultáneamente.
 *
 * @param rojo  true enciende el canal rojo.
 * @param verde true enciende el canal verde.
 * @param azul  true enciende el canal azul.
 */
void bsp_led_rgb_set(bool rojo, bool verde, bool azul) {
    // Nota: El HAL ya maneja la lógica negativa automáticamente gracias a la configuración inicial
    hal_gpio_write(PIN_LED_RGB_ROJO, rojo);
    hal_gpio_write(PIN_LED_RGB_VERDE, verde);
    hal_gpio_write(PIN_LED_RGB_AZUL, azul);
}

/**
 * @brief Consulta físicamente si un botón está presionado en este instante.
 *
 * @param btn Etiqueta lógica del botón a consultar.
 * @return true si el botón está oprimido, false en caso contrario.
 */
bool bsp_button_is_pressed(bsp_button_t btn) {
    uint8_t pin = bsp_get_button_pin(btn);
    if (pin == 255) return false;

    hal_gpio_level_e estado;
    hal_gpio_read(pin, &estado);
    
    // Como usamos resistencias Pull-Up, el voltaje cae a LOW al presionar el botón
    return (estado == HAL_GPIO_LOW); 
}