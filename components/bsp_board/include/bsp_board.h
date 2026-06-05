/**
 * @file bsp_board.h
 * @brief Board Support Package para la placa personalizada
 */

#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include <stdbool.h>
#include "esp_attr.h" /* IRAM_ATTR */
// *******************************************************************************/
//  ENUMERACIONES DE COMPONENTES FÍSICOS
// *******************************************************************************/

typedef enum {
    BSP_LED_SISTEMA,
    BSP_LED_RGB_ROJO,
    BSP_LED_RGB_VERDE,
    BSP_LED_RGB_AZUL,
    BSP_LED_4,
    BSP_LED_5,
    BSP_LED_16,
    BSP_LED_17
} bsp_led_t;

typedef enum {
    BSP_BUTTON_1,
    BSP_BUTTON_2
} bsp_button_t;

// ****************************************************************************/
//  API DEL BSP
// ***************************************************************************/

/**
 * @brief Inicializa todo el hardware de la placa (Pines de LEDs y Botones).
 * @return true si todo se inicializó correctamente, false si hubo un error.
 */
bool bsp_board_init(void);

/**
 * @brief Enciende o apaga un LED específico de la placa.
 */
void bsp_led_set(bsp_led_t led, bool level);

/**
 * @brief Invierte el estado actual del LED indicado.
 */
void bsp_led_toggle(bsp_led_t led);

/**
 * @brief Lee el estado físico de un botón.
 * @return true si está presionado, false si no lo está.
 */
bool bsp_button_is_pressed(bsp_button_t btn);

// Definimos un tipo para la función de callback (el "gancho")
typedef void (*bsp_button_callback_t)(void *arg);

// Agregamos una función para que el main pueda registrar su propio callback
void bsp_button_register_callback(bsp_button_t btn, bsp_button_callback_t callback, void *arg);


#endif /* BSP_BOARD_H */