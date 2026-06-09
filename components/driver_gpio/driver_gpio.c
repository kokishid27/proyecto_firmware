/**
 * @file driver_gpio.c
 * @brief Driver GPIO para ESP32 con soporte de interrupciones por pin
 *
 * Arquitectura de interrupciones:
 *  - El ESP32 expone UNA sola linea IRQ para todos los pines GPIO
 *    (fuente ETS_GPIO_INTR_SOURCE).
 *  - Este driver instala una ISR maestra (_gpio_isr_master) que lee
 *    los registros GPIO_STATUS / GPIO_STATUS1, determina que pines
 *    dispararon, limpia sus flags y despacha el callback registrado.
 *  - Cada pin tiene una entrada en la tabla _intr_table[40] con su
 *    tipo, callback y argumento de usuario.
 */

#include "driver_gpio.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"       /* IRAM_ATTR */

#define NUCLEO_PRO_APP ((1U << 15) | (1U << 13))

static volatile uint32_t *IO_MUX_MAP[40] = {
    GPIO_IO_MUX_0,   // 0
    NULL,            // 1
    GPIO_IO_MUX_2,   // 2
    NULL,            // 3
    GPIO_IO_MUX_4,   // 4
    GPIO_IO_MUX_5,   // 5
    NULL, NULL, NULL, NULL, NULL, // 6-10
    NULL,            // 11
    GPIO_IO_MUX_12,  // 12
    GPIO_IO_MUX_13,  // 13
    GPIO_IO_MUX_14,  // 14
    NULL,            // 15
    GPIO_IO_MUX_16,  // 16
    GPIO_IO_MUX_17,  // 17
    GPIO_IO_MUX_18,  // 18
    GPIO_IO_MUX_19,  // 19
    GPIO_IO_MUX_20,  // 20
    GPIO_IO_MUX_21,  // 21
    GPIO_IO_MUX_22,  // 22
    GPIO_IO_MUX_23,  // 23
    GPIO_IO_MUX_24,  // 24
    GPIO_IO_MUX_25,  // 25
    GPIO_IO_MUX_26,  // 26
    GPIO_IO_MUX_27,  // 27
    NULL, NULL, NULL, // 28-30
    NULL,            // 31
    GPIO_IO_MUX_32,  // 32
    GPIO_IO_MUX_33,  // 33
    GPIO_IO_MUX_34,  // 34
    GPIO_IO_MUX_35,  // 35
    GPIO_IO_MUX_36,  // 36
    GPIO_IO_MUX_37,  // 37
    GPIO_IO_MUX_38,  // 38
    GPIO_IO_MUX_39   // 39
};

typedef struct {
    gpio_intr_callback_t callback;  /**< Funcion de usuario        */
    void                *arg;       /**< Argumento de usuario       */
    bool                 active;    /**< true = interrupcion activa */
} _gpio_intr_entry_t;

static _gpio_intr_entry_t _intr_table[40] = {0};

/** Handle del servicio de interrupciones compartido */
static intr_handle_t _gpio_intr_handle = NULL;

/**
 * @brief Escribe el campo INT_TYPE del registro GPIO_PINn.
 *
 * @param pin   Numero de pin (0-39).
 * @param type  Valor de gpio_intr_type_e (0-5).
 */
static void _gpio_set_intr_type_reg(uint8_t pin, gpio_intr_type_e type)
{
    volatile uint32_t *reg = &GPIO_PIN_REG(pin);
    uint32_t val = *reg;
    val &= ~GPIO_PIN_INT_TYPE_MASK;
    val |= ((uint32_t)type << GPIO_PIN_INT_TYPE_SHIFT);
    if (type == GPIO_INTR_DISABLE) {
            int core_id = xPortGetCoreID(); // Obtiene el id del nucleo que procesa la linea
                                            // el PRO (0) o el APP (1)
            if (core_id == 0) {
                val |= (1u << 15); // Solo encender la alarma para el PRO CPU
            } else {
                val |= (1u << 13); // Solo encender la alarma para el APP CPU
            }
    }
    *reg = val;
}

/**
 * @brief ISR compartida para todos los pines GPIO.
 *
 * Lee GPIO_STATUS y GPIO_STATUS1, itera sobre los bits activos,
 * limpia el flag con write-1-to-clear y llama al callback registrado.
 */
static void IRAM_ATTR _gpio_isr_master(void *arg)
{
    uint32_t status0;
    uint32_t status1;

    // Procesar Banco 0 (Pines 0-31)
    do {
        status0 = GPIO_STATUS;
        if (status0) {
            uint8_t pin = (uint8_t)__builtin_ctz(status0);
            
            // 1. Limpiamos EL PIN ESPECÍFICO inmediatamente
            GPIO_STATUS_W1TC = (1U << pin); 

            // 2. Ejecutamos el callback
            if (_intr_table[pin].active && _intr_table[pin].callback != NULL) {
                _intr_table[pin].callback( _intr_table[pin].arg);
            }
        }
    } while (GPIO_STATUS != 0); // Repetir hasta que no haya más flags activos

    // Procesar Banco 1 (Pines 32-39)
    do {
        status1 = GPIO_STATUS1;
        if (status1) {
            uint8_t offset = (uint8_t)__builtin_ctz(status1);
            uint8_t pin = offset + 32;

            // 1. Limpiamos EL PIN ESPECÍFICO inmediatamente
            GPIO_STATUS1_W1TC = (1U << offset);

            // 2. Ejecutamos el callback
            if (pin <= 39 && _intr_table[pin].active && _intr_table[pin].callback != NULL) {
                _intr_table[pin].callback(_intr_table[pin].arg);
            }
        }
    } while (GPIO_STATUS1 != 0); // Repetir hasta que no haya más flags activos
}

gpio_err_e gpio_config_out(uint8_t pin)
{
    if (pin > 39 || IO_MUX_MAP[pin] == NULL) return GPIO_ERR_PIN_NUM;

    if (pin <= 31) GPIO_ENABLE  |= (1u << pin);
    else           GPIO_ENABLE1 |= (1u << (pin - 32));

    *IO_MUX_MAP[pin] &= ~MCU_SEL_MASK;
    *IO_MUX_MAP[pin] |=  MCU_SEL_GPIO;
    *IO_MUX_MAP[pin] &= ~(PULL_WPU | PULL_WPD);

    return GPIO_OK;

}

gpio_err_e gpio_config_in(uint8_t pin, pull_mode_e pull_mode)
{
    if (pin > 39 || IO_MUX_MAP[pin] == NULL) return GPIO_ERR_PIN_NUM;

    if (pin <= 31) GPIO_ENABLE  &= ~(1u << pin);
    else           GPIO_ENABLE1 &= ~(1u << (pin - 32));

    *IO_MUX_MAP[pin] &= ~MCU_SEL_MASK;
    *IO_MUX_MAP[pin] |=  MCU_SEL_GPIO;
    *IO_MUX_MAP[pin] &= ~(PULL_WPU | PULL_WPD);
    *IO_MUX_MAP[pin] |=  FUN_IE;

    if      (pull_mode == PULLUP)   *IO_MUX_MAP[pin] |= PULL_WPU;
    else if (pull_mode == PULLDOWN) *IO_MUX_MAP[pin] |= PULL_WPD;

    return GPIO_OK;
}

bool gpio_read(uint8_t pin)
{
    if (pin <= 31)            return (GPIO_IN  & (1u <<  pin))       ? true : false;
    else if (pin <= 39)       return (GPIO_IN1 & (1u << (pin - 32))) ? true : false;
    return false;
}

gpio_err_e gpio_write(uint8_t pin, bool value)
{
    if (pin <= 31) {
        if (value) GPIO_OUT_W1TS  |= (1u << pin);
        else       GPIO_OUT_W1TC  |= (1u << pin);
        return GPIO_OK;
    }
    else if (pin >= 32 && pin <= 39) {
        if (value) GPIO_OUT1_W1TS |= (1u << (pin - 32));
        else       GPIO_OUT1_W1TC |= (1u << (pin - 32));
        return GPIO_OK;
    }
    return GPIO_ERR_PIN_NUM;
}

gpio_err_e gpio_drv_intr_install(int intr_flags)
{
    if (_gpio_intr_handle != NULL) return GPIO_ERR_PARAM; /* ya instalado */

    esp_err_t ret = esp_intr_alloc(
        ETS_GPIO_INTR_SOURCE,
        intr_flags | ESP_INTR_FLAG_IRAM,
        _gpio_isr_master,
        NULL,
        &_gpio_intr_handle
    );
    if (ret != ESP_OK) {
        printf("ERROR PROFUNDO: esp_intr_alloc fallo con codigo %d\n", ret);
    }

    return (ret == ESP_OK) ? GPIO_OK : GPIO_ERR_PARAM;
}

gpio_err_e gpio_drv_intr_set(uint8_t pin, gpio_intr_type_e intr_type,
                          gpio_intr_callback_t callback, void *arg)
{
    if (pin > 39 || IO_MUX_MAP[pin] == NULL) return GPIO_ERR_PIN_NUM;
    if (callback == NULL)                    return GPIO_ERR_PARAM;

    /* Instalar la ISR maestra si todavia no se hizo */
    if (_gpio_intr_handle == NULL) {
        gpio_err_e ret = gpio_drv_intr_install(0);
        if (ret != GPIO_OK) return ret;
    }

    /* Guardar callback y argumento */
    _intr_table[pin].callback = callback;
    _intr_table[pin].arg      = arg;
    _intr_table[pin].active   = true;

    /* Limpiar flag previo antes de habilitar */
    if (pin <= 31) GPIO_STATUS_W1TC  = (1u << pin);
    else           GPIO_STATUS1_W1TC = (1u << (pin - 32));

    /* Configurar tipo de disparo en el registro GPIO_PINn */
    _gpio_set_intr_type_reg(pin, intr_type);

    return GPIO_OK;
}

gpio_err_e gpio_drv_intr_enable(uint8_t pin)
{
    if (pin > 39) return GPIO_ERR_PIN_NUM;

    _intr_table[pin].active = true;

    /* Restaurar tipo de disparo previamente configurado */
    /* (el tipo esta guardado implicitamente en el registro; solo activamos la entrada) */
    return GPIO_OK;
}

gpio_err_e gpio_drv_intr_disable(uint8_t pin)
{
    if (pin > 39) return GPIO_ERR_PIN_NUM;

    /* Quitar el tipo de disparo en el registro GPIO_PINn (INT_TYPE = 0 = disable) */
    _gpio_set_intr_type_reg(pin, GPIO_INTR_DISABLE);

    _intr_table[pin].active = false;
    return GPIO_OK;
}

gpio_err_e gpio_drv_intr_clear(uint8_t pin)
{
    if (pin > 39) return GPIO_ERR_PIN_NUM;

    if (pin <= 31) GPIO_STATUS_W1TC  = (1U << pin);
    else           GPIO_STATUS1_W1TC = (1U << (pin - 32));

    return GPIO_OK;
}