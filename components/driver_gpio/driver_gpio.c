/**
 * @file driver_gpio.c
 * @author Diego Alejandro Saenz Duarte, Hector Teruel Grado, Jorge Habib Hernandez Rojas
 * 
 * @brief Driver GPIO de bajo nivel para ESP32 con manejo de interrupciones.
 *
 * Arquitectura de interrupciones:
 * - El ESP32 rutea todas las interrupciones GPIO a una sola linea de hardware 
 * (fuente ETS_GPIO_INTR_SOURCE).
 * - Se instala una ISR maestra (_gpio_isr_master) en RAM (IRAM_ATTR) para minimizar latencia.
 * - La ISR lee los registros GPIO_STATUS / GPIO_STATUS1, usa una instruccion de 
 * un solo ciclo (ctz) para hallar el pin, limpia su flag por hardware (W1TC) 
 * y despacha el callback registrado.
 * - Soporta hasta 40 pines, mapeados mediante la matriz IO_MUX.
 */

#include "driver_gpio.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"       /* Para macros como IRAM_ATTR */

/**
 * @brief Mapa de punteros a los registros IO_MUX de cada pin.
 * @note Los pines marcados con NULL no existen, son de solo entrada (ej. 34-39 no tienen pull-ups),
 * o estan reservados internamente por el chip flash.
 */
static volatile uint32_t *IO_MUX_MAP[40] = {
    GPIO_IO_MUX_0,   // 0
    NULL,            // 1 (Normalmente TXD)
    GPIO_IO_MUX_2,   // 2
    NULL,            // 3 (Normalmente RXD)
    GPIO_IO_MUX_4,   // 4
    GPIO_IO_MUX_5,   // 5
    NULL, NULL, NULL, NULL, NULL, // 6-10 (Reservados para Flash SPI)
    NULL,            // 11 (Reservado para Flash SPI)
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
    NULL, NULL, NULL, // 28-30 (No existen en ESP32)
    NULL,            // 31 (No existe en ESP32)
    GPIO_IO_MUX_32,  // 32
    GPIO_IO_MUX_33,  // 33
    GPIO_IO_MUX_34,  // 34 (Solo entrada)
    GPIO_IO_MUX_35,  // 35 (Solo entrada)
    GPIO_IO_MUX_36,  // 36 (Solo entrada)
    GPIO_IO_MUX_37,  // 37 (Solo entrada)
    GPIO_IO_MUX_38,  // 38 (Solo entrada)
    GPIO_IO_MUX_39   // 39 (Solo entrada)
};

/**
 * @brief Estructura de control para el estado de la interrupcion de un pin.
 */
typedef struct {
    gpio_intr_callback_t callback;  /**< Puntero a la funcion a ejecutar al dispararse */
    void                *arg;       /**< Argumentos opcionales para el callback */
    bool                 active;    /**< Bandera de estado (true = interrupcion habilitada) */
    gpio_intr_type_e     intr_type; /**< Tipo de interrupcion configurada (flanco/nivel) */
} _gpio_intr_entry_t;

static _gpio_intr_entry_t _intr_table[40] = {0};

/** Handle del servicio de interrupciones, se usa para evitar multiples instalaciones */
static intr_handle_t _gpio_intr_handle = NULL;

/**
 * @brief Configura la logica de disparo (flanco/nivel) y rutea al core correspondiente.
 * * Escribe sobre el registro individual GPIO_PINn_REG para definir que evento electrico
 * genera la interrupcion y a que nucleo (PRO o APP) se le notifica.
 *
 * @param pin  Numero fisico del pin (0-39).
 * @param type Condicion de disparo (ej. Flanco de bajada, nivel alto, etc).
 */
static void _gpio_set_intr_type_reg(uint8_t pin, gpio_intr_type_e type)
{
    volatile uint32_t *reg = &GPIO_PIN_REG(pin);
    uint32_t val = *reg;
    
    // Limpiamos los bits del tipo de interrupcion actual
    val &= ~GPIO_PIN_INT_TYPE_MASK;
    // Insertamos la nueva configuracion corrida a su posicion correspondiente
    val |= ((uint32_t)type << GPIO_PIN_INT_TYPE_SHIFT);
    
    if (type == GPIO_INTR_DISABLE) {
        val &= ~((1u << 15) | (1u << 13)); // Limpiar ruteo a PRO y APP CPU
    } else {
        val |= ((1u << 15) | (1u << 13));  // Habilitar ruteo a PRO y APP CPU
    }
    
    // Escribir de vuelta el registro completo
    *reg = val;
}

/**
 * @brief Rutina de Servicio de Interrupcion (ISR) maestra compartida.
 *
 * Se declara con IRAM_ATTR para evitar page faults si la memoria flash esta ocupada.
 * Utiliza __builtin_ctz para encontrar el primer bit en '1' en un solo ciclo de reloj.
 *
 * @param arg Parametro opcional (no utilizado).
 */
static void IRAM_ATTR _gpio_isr_master(void *arg)
{
    uint32_t status0;
    uint32_t status1;

    // --- Procesar Banco 0 (Pines 0 al 31) ---
    do {
        status0 = GPIO_STATUS; // Leer espejo de estado hardware
        if (status0) {
            // ctz cuenta ceros a la derecha para dar el indice exacto del pin
            uint8_t pin = (uint8_t)__builtin_ctz(status0);
            
            // Limpiamos la bandera fisicamente ANTES de procesar para no perder 
            // eventos nuevos que ocurran durante la ejecucion del callback.
            GPIO_STATUS_W1TC = (1U << pin); 

            // 2. Ejecutar la logica de usuario
            if (_intr_table[pin].active && _intr_table[pin].callback != NULL) {
                _intr_table[pin].callback(_intr_table[pin].arg);
            }
        }
    } while (GPIO_STATUS != 0); // Bucle atrapa interrupciones anidadas sin salir de la ISR

    // --- Procesar Banco 1 (Pines 32 al 39) ---
    do {
        status1 = GPIO_STATUS1;
        if (status1) {
            uint8_t offset = (uint8_t)__builtin_ctz(status1);
            uint8_t pin = offset + 32;

            GPIO_STATUS1_W1TC = (1U << offset);

            if (pin <= 39 && _intr_table[pin].active && _intr_table[pin].callback != NULL) {
                _intr_table[pin].callback(_intr_table[pin].arg);
            }
        }
    } while (GPIO_STATUS1 != 0);
}

/**
 * @brief Configura un pin especifico como salida digital.
 * @param pin Numero del pin (0-39).
 * @return GPIO_OK si el pin es valido, GPIO_ERR_PIN_NUM si no.
 */
gpio_err_e gpio_config_out(uint8_t pin)
{
    if (pin > 39 || IO_MUX_MAP[pin] == NULL) return GPIO_ERR_PIN_NUM;

    // Habilitar salida en el registro correspondiente al banco
    if (pin <= 31) GPIO_ENABLE  |= (1u << pin);
    else           GPIO_ENABLE1 |= (1u << (pin - 32));

    // Configurar multiplexor para funcion GPIO generica 
    *IO_MUX_MAP[pin] &= ~MCU_SEL_MASK;
    *IO_MUX_MAP[pin] |=  MCU_SEL_GPIO;
    
    // Apagar resistencias pull-up y pull-down por precaucion
    *IO_MUX_MAP[pin] &= ~(PULL_WPU | PULL_WPD);

    return GPIO_OK;
}

/**
 * @brief Configura un pin especifico como entrada digital.
 * @param pin Numero del pin (0-39).
 * @param pull_mode Resistencia interna a activar (PULLUP, PULLDOWN o NONE).
 * @return GPIO_OK si el pin es valido, GPIO_ERR_PIN_NUM si no.
 */
gpio_err_e gpio_config_in(uint8_t pin, pull_mode_e pull_mode)
{
    if (pin > 39 || IO_MUX_MAP[pin] == NULL) return GPIO_ERR_PIN_NUM;

    // Deshabilitar la salida hardware
    if (pin <= 31) GPIO_ENABLE  &= ~(1u << pin);
    else           GPIO_ENABLE1 &= ~(1u << (pin - 32));

    // Ruteo a GPIO generico y habilitacion de lectura (FUN_IE)
    *IO_MUX_MAP[pin] &= ~MCU_SEL_MASK;
    *IO_MUX_MAP[pin] |=  MCU_SEL_GPIO;
    *IO_MUX_MAP[pin] &= ~(PULL_WPU | PULL_WPD);
    *IO_MUX_MAP[pin] |=  FUN_IE;

    // Activar resistencias internas de hardware si se requieren
    if      (pull_mode == PULLUP)   *IO_MUX_MAP[pin] |= PULL_WPU;
    else if (pull_mode == PULLDOWN) *IO_MUX_MAP[pin] |= PULL_WPD;

    return GPIO_OK;
}

/**
 * @brief Lee el estado logico (voltaje) actual en un pin.
 * @param pin Numero de pin.
 * @return true si esta en ALTO (Vcc), false si esta en BAJO (GND).
 */
bool gpio_read(uint8_t pin)
{
    if (pin <= 31)            return (GPIO_IN  & (1u <<  pin))       ? true : false;
    else if (pin <= 39)       return (GPIO_IN1 & (1u << (pin - 32))) ? true : false;
    return false;
}

/**
 * @brief Fuerza la salida de un pin a nivel logico ALTO o BAJO.
 * * Se usan los registros W1TS (Set) y W1TC (Clear) para evitar 
 * operaciones Read-Modify-Write que puedan causar condiciones de carrera.
 */
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

/**
 * @brief Instala y aloja el manejador global de interrupciones para el periferico GPIO.
 * @param intr_flags Banderas de prioridad (ej. ESP_INTR_FLAG_LEVEL1).
 * @return Estado de la operacion.
 */
gpio_err_e gpio_drv_intr_install(int intr_flags)
{
    if (_gpio_intr_handle != NULL) return GPIO_ERR_PARAM; /* Prevencion de doble instalacion */

    esp_err_t ret = esp_intr_alloc(
        ETS_GPIO_INTR_SOURCE, //Decimos quien es la fuente de interrupcion (GPIO)
        intr_flags | ESP_INTR_FLAG_IRAM, // Forzar ubicacion en RAM para velocidad y acepta otras 
                                         //banderas como prioridad
        _gpio_isr_master,
        NULL,                           //args opcionales para la ISR, no usados en este caso
        &_gpio_intr_handle
    );
    
    if (ret != ESP_OK) {
        printf("ERROR PROFUNDO: esp_intr_alloc fallo con codigo %d\n", ret);
    }

    return (ret == ESP_OK) ? GPIO_OK : GPIO_ERR_PARAM;
}

/**
 * @brief Registra el callback de un pin especifico y configura su tipo de disparo.
 */
gpio_err_e gpio_drv_intr_set(uint8_t pin, gpio_intr_type_e intr_type,
                          gpio_intr_callback_t callback, void *arg)
{
    if (pin > 39 || IO_MUX_MAP[pin] == NULL) return GPIO_ERR_PIN_NUM;
    if (callback == NULL)                    return GPIO_ERR_PARAM;

    /* Instalar la ISR maestra por defecto si el usuario olvido hacerlo */
    if (_gpio_intr_handle == NULL) {
        gpio_err_e ret = gpio_drv_intr_install(0);
        if (ret != GPIO_OK) return ret;
    }

    /* Guardar logica de usuario en la tabla hash/array estatico */
    _intr_table[pin].callback = callback;
    _intr_table[pin].arg      = arg;
    _intr_table[pin].active   = true;
    _intr_table[pin].intr_type = intr_type;

    /* Limpiar cualquier interrupcion residual en el hardware antes de activar */
    if (pin <= 31) GPIO_STATUS_W1TC  = (1u << pin);
    else           GPIO_STATUS1_W1TC = (1u << (pin - 32));

    /* Configurar flanco/nivel y ruteo a CPU */
    _gpio_set_intr_type_reg(pin, intr_type);

    return GPIO_OK;
}

/**
 * @brief Rehabilita por software la interrupcion de un pin.
 */
gpio_err_e gpio_drv_intr_enable(uint8_t pin)
{
    if (pin > 39) return GPIO_ERR_PIN_NUM;

    _gpio_set_intr_type_reg(pin, _intr_table[pin].intr_type); // Reconfigurar el tipo de interrupcion para reactivar el ruteo
    _intr_table[pin].active = true;
    return GPIO_OK;
}

/**
 * @brief Deshabilita la interrupcion del pin a nivel de hardware y software.
 */
gpio_err_e gpio_drv_intr_disable(uint8_t pin)
{
    if (pin > 39) return GPIO_ERR_PIN_NUM;

    /* Cortar el ruteo de interrupcion directo en el multiplexor */
    _gpio_set_intr_type_reg(pin, GPIO_INTR_DISABLE);

    _intr_table[pin].active = false;
    return GPIO_OK;
}

/**
 * @brief Limpia manualmente la bandera de interrupcion del hardware.
 */
gpio_err_e gpio_drv_intr_clear(uint8_t pin)
{
    if (pin > 39) return GPIO_ERR_PIN_NUM;

    if (pin <= 31) GPIO_STATUS_W1TC  = (1U << pin);
    else           GPIO_STATUS1_W1TC = (1U << (pin - 32));

    return GPIO_OK;
}