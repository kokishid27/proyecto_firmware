/**
 * @file driver_gpio.h
 * @brief Driver GPIO para ESP32 - incluye soporte de interrupciones
 */

#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_intr_alloc.h"

/* =========================================================================
 *  REGISTROS GPIO - BANCO 0 (pines 0-31)
 * ========================================================================= */
#define GPIO_BASE           0x3FF44000UL
#define GPIO_PIN_REG(pin)    (*((volatile uint32_t *)((GPIO_BASE + 0x88) + (pin * 4))))

#define GPIO_OUT_W1TS       (*(volatile uint32_t *)(GPIO_BASE + 0x08))
#define GPIO_OUT_W1TC       (*(volatile uint32_t *)(GPIO_BASE + 0x0C))
#define GPIO_ENABLE         (*(volatile uint32_t *)(GPIO_BASE + 0x20))
#define GPIO_IN             (*(volatile uint32_t *)(GPIO_BASE + 0x3C))

/* Registros de interrupcion - banco 0 */
#define GPIO_STATUS         (*(volatile uint32_t *)(GPIO_BASE + 0x44))  /**< Flags de INT pendientes pines 0-31  */
#define GPIO_STATUS_W1TC    (*(volatile uint32_t *)(GPIO_BASE + 0x48))  /**< Limpiar flags INT (write-1-to-clear) */
#define GPIO_PIN_BASE       (volatile uint32_t *)(GPIO_BASE + 0x88)     /**< Registro GPIO_PINn: base, +4*n       */

#define GPIO_OUT1_W1TS      (*(volatile uint32_t *)(GPIO_BASE + 0x14))
#define GPIO_OUT1_W1TC      (*(volatile uint32_t *)(GPIO_BASE + 0x18))
#define GPIO_ENABLE1        (*(volatile uint32_t *)(GPIO_BASE + 0x2C))
#define GPIO_IN1            (*(volatile uint32_t *)(GPIO_BASE + 0x40))

/* Registros de interrupcion - banco 1 */
#define GPIO_STATUS1        (*(volatile uint32_t *)(GPIO_BASE + 0x4C)) /**< Flags de INT pendientes pines 32-39 */
#define GPIO_STATUS1_W1TC   (*(volatile uint32_t *)(GPIO_BASE + 0x50)) /**< Limpiar flags INT banco 1           */

/* =========================================================================
 *  REGISTROS IO_MUX (un registro por pin fisico)
 * ========================================================================= */
#define IO_MUX_BASE         0x3FF49000UL

#define GPIO_IO_MUX_0       ((volatile uint32_t *)(IO_MUX_BASE + 0x44))
#define GPIO_IO_MUX_2       ((volatile uint32_t *)(IO_MUX_BASE + 0x40))
#define GPIO_IO_MUX_4       ((volatile uint32_t *)(IO_MUX_BASE + 0x48))
#define GPIO_IO_MUX_5       ((volatile uint32_t *)(IO_MUX_BASE + 0x6C))
#define GPIO_IO_MUX_12      ((volatile uint32_t *)(IO_MUX_BASE + 0x04))
#define GPIO_IO_MUX_13      ((volatile uint32_t *)(IO_MUX_BASE + 0x08))
#define GPIO_IO_MUX_14      ((volatile uint32_t *)(IO_MUX_BASE + 0x0C))
#define GPIO_IO_MUX_16      ((volatile uint32_t *)(IO_MUX_BASE + 0x10))
#define GPIO_IO_MUX_17      ((volatile uint32_t *)(IO_MUX_BASE + 0x14))
#define GPIO_IO_MUX_18      ((volatile uint32_t *)(IO_MUX_BASE + 0x18))
#define GPIO_IO_MUX_19      ((volatile uint32_t *)(IO_MUX_BASE + 0x1C))
#define GPIO_IO_MUX_20      ((volatile uint32_t *)(IO_MUX_BASE + 0x20))
#define GPIO_IO_MUX_21      ((volatile uint32_t *)(IO_MUX_BASE + 0x24))
#define GPIO_IO_MUX_22      ((volatile uint32_t *)(IO_MUX_BASE + 0x28))
#define GPIO_IO_MUX_23      ((volatile uint32_t *)(IO_MUX_BASE + 0x2C))
#define GPIO_IO_MUX_24      ((volatile uint32_t *)(IO_MUX_BASE + 0x30))
#define GPIO_IO_MUX_25      ((volatile uint32_t *)(IO_MUX_BASE + 0x34))
#define GPIO_IO_MUX_26      ((volatile uint32_t *)(IO_MUX_BASE + 0x38))
#define GPIO_IO_MUX_27      ((volatile uint32_t *)(IO_MUX_BASE + 0x3C))
#define GPIO_IO_MUX_32      ((volatile uint32_t *)(IO_MUX_BASE + 0x58))
#define GPIO_IO_MUX_33      ((volatile uint32_t *)(IO_MUX_BASE + 0x5C))
#define GPIO_IO_MUX_34      ((volatile uint32_t *)(IO_MUX_BASE + 0x60))
#define GPIO_IO_MUX_35      ((volatile uint32_t *)(IO_MUX_BASE + 0x64))
#define GPIO_IO_MUX_36      ((volatile uint32_t *)(IO_MUX_BASE + 0x68))
#define GPIO_IO_MUX_37      ((volatile uint32_t *)(IO_MUX_BASE + 0x50))
#define GPIO_IO_MUX_38      ((volatile uint32_t *)(IO_MUX_BASE + 0x54))
#define GPIO_IO_MUX_39      ((volatile uint32_t *)(IO_MUX_BASE + 0x70))

/* =========================================================================
 *  BITS DE CONTROL IO_MUX / GPIO_PIN
 * ========================================================================= */
#define FUN_IE              (1 << 9)    /**< Habilitar entrada               */
#define PULL_WPU            (1 << 8)    /**< Pull-up interno                 */
#define PULL_WPD            (1 << 7)    /**< Pull-down interno               */
#define MCU_SEL_MASK        (0x7 << 12) /**< Mascara de funcion de pin       */
#define MCU_SEL_GPIO        (0x2 << 12) /**< Funcion GPIO                    */

/* Bits del registro GPIO_PINn para interrupcion */
#define GPIO_PIN_INT_TYPE_SHIFT     7   /**< Posicion del campo INT_TYPE     */
#define GPIO_PIN_INT_TYPE_MASK      (0x7 << GPIO_PIN_INT_TYPE_SHIFT)

/* Fuente de interrupcion GPIO en el controlador de interrupciones */
#define ETS_GPIO_INTR_SOURCE       22  /**< Numero de fuente IRQ GPIO       */

/** @brief Codigos de error del driver GPIO */
typedef enum {
    GPIO_OK            =  0,
    GPIO_ERR_PIN_NUM   = -1,
    GPIO_ERR_PARAM     = -2,
} gpio_err_e;

/** @brief Modo de pull del pin */
typedef enum {
    NOPULL   = 0,
    PULLUP   = 1,
    PULLDOWN = 2,
} pull_mode_e;

/**
 * @brief Tipo de disparo de la interrupcion GPIO.
 *
 * Valores compatibles con el campo INT_TYPE del registro GPIO_PINn
 * (TRM del ESP32, seccion 4.4.7).
 */
typedef enum {
    GPIO_INTR_DISABLE    = 0, /**< Interrupcion deshabilitada              */
    GPIO_INTR_POS_EDGE   = 1, /**< Flanco ascendente                       */
    GPIO_INTR_NEG_EDGE   = 2, /**< Flanco descendente                      */
    GPIO_INTR_ANY_EDGE   = 3, /**< Cualquier flanco                        */
    GPIO_INTR_LOW_LEVEL  = 4, /**< Nivel bajo (mantenido)                  */
    GPIO_INTR_HIGH_LEVEL = 5, /**< Nivel alto (mantenido)                  */
} gpio_intr_type_e;

/**
 * @brief Prototipo del callback de interrupcion GPIO.
 *
 * Se ejecuta dentro de la ISR compartida del GPIO; debe declararse
 * con IRAM_ATTR y ser lo mas breve posible.
 *
 * @param pin  Numero de pin que genero la interrupcion.
 * @param arg  Argumento de usuario registrado con gpio_intr_set().
 */
typedef void (*gpio_intr_callback_t)(uint8_t pin, void *arg);

/**
 * @brief Configura un pin como salida e inicializa su nivel en LOW.
 * @param pin  Numero de pin (0-39).
 * @return     GPIO_OK o GPIO_ERR_PIN_NUM.
 */
gpio_err_e gpio_config_out(uint8_t pin);

/**
 * @brief Configura un pin como entrada.
 * @param pin       Numero de pin (0-39).
 * @param pull_mode Resistencia de pull (NOPULL / PULLUP / PULLDOWN).
 * @return          GPIO_OK o GPIO_ERR_PIN_NUM.
 */
gpio_err_e gpio_config_in(uint8_t pin, pull_mode_e pull_mode);

/**
 * @brief Lee el nivel logico de un pin.
 * @param pin  Numero de pin (0-39).
 * @return     true = nivel alto, false = nivel bajo o pin invalido.
 */
bool gpio_read(uint8_t pin);

/**
 * @brief Escribe un nivel logico en un pin de salida.
 * @param pin    Numero de pin (0-39).
 * @param value  true = HIGH, false = LOW.
 * @return       GPIO_OK o GPIO_ERR_PIN_NUM.
 */
gpio_err_e gpio_write(uint8_t pin, bool value);

/**
 * @brief Instala el servicio de interrupciones GPIO compartido.
 *
 * Debe llamarse UNA sola vez antes de registrar cualquier pin con
 * gpio_drv_intr_set(). Registra la ISR maestra que despacha los callbacks
 * individuales por pin.
 *
 * @param intr_flags  Flags para esp_intr_alloc() (p.ej. ESP_INTR_FLAG_IRAM).
 *                    Pasar 0 para usar los flags por defecto.
 * @return            GPIO_OK, o GPIO_ERR_PARAM si el servicio ya fue instalado.
 */
gpio_err_e gpio_drv_intr_install(int intr_flags);

/**
 * @brief Configura la interrupcion de un pin y registra su callback.
 *
 * El pin debe estar previamente configurado como entrada con gpio_config_in().
 * Llama internamente gpio_drv_intr_install() si aun no se hizo.
 *
 * @param pin        Numero de pin (0-39).
 * @param intr_type  Tipo de disparo (flanco o nivel).
 * @param callback   Funcion a ejecutar en la ISR (debe ser IRAM_ATTR).
 * @param arg        Argumento de usuario pasado al callback.
 * @return           GPIO_OK, GPIO_ERR_PIN_NUM o GPIO_ERR_PARAM.
 */
gpio_err_e gpio_drv_intr_set(uint8_t pin, gpio_intr_type_e intr_type,
                              gpio_intr_callback_t callback, void *arg);

/**
 * @brief Habilita la interrupcion de un pin previamente configurado.
 * @param pin  Numero de pin (0-39).
 * @return     GPIO_OK o GPIO_ERR_PIN_NUM.
 */
gpio_err_e gpio_drv_intr_enable(uint8_t pin);

/**
 * @brief Deshabilita la interrupcion de un pin sin borrar su configuracion.
 * @param pin  Numero de pin (0-39).
 * @return     GPIO_OK o GPIO_ERR_PIN_NUM.
 */
gpio_err_e gpio_drv_intr_disable(uint8_t pin);

/**
 * @brief Limpia el flag de interrupcion pendiente de un pin.
 *
 * Debe llamarse dentro del callback para evitar re-disparos inmediatos
 * cuando se usan interrupciones por nivel.
 *
 * @param pin  Numero de pin (0-39).
 * @return     GPIO_OK o GPIO_ERR_PIN_NUM.
 */
gpio_err_e gpio_drv_intr_clear(uint8_t pin);

#endif /* DRIVER_GPIO_H */