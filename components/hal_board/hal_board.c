/**
 * @file hal.c
 * @brief Implementacion de la Hardware Abstraction Layer (HAL)
 *
 * En este archivo implementé la capa intermedia que traduce las instrucciones
 * generales de nuestro programa hacia los comandos específicos de los drivers
 * de más bajo nivel (GPIO y temporizadores).
 */

#include "hal_board.h"
#include "driver_gpio.h"
#include "driver_timer.h"

/* =========================================================================
 * UTILIDADES INTERNAS
 * ========================================================================= */

/** * Tablas de estado en memoria. 
 * Las creé para recordar cómo configuramos cada pin, ya que a veces el hardware
 * no nos permite leer directamente si un pin de salida está encendido o apagado.
 * El ESP32 maneja pines del 0 al 39, por eso el tamaño de los arreglos.
 */
static hal_gpio_level_e _gpio_out_state[40] = {HAL_GPIO_LOW};

/** Registro para saber si configuramos el pin como salida (true) o no (false) */
static bool _gpio_is_output[40] = {false};

/** Registro para manejar si el pin usa lógica positiva (normal) o negativa (invertida) */
static hal_gpio_type_logic_e _gpio_logic[40] = {HAL_POSITIVE_LOGIC};

/**
 * @brief Traduce la identificación general del temporizador a la física.
 *
 * Diseñé esta función interna para tomar el identificador abstracto que usamos
 * en el programa principal y descubrir a qué grupo y número de temporizador
 * real corresponde en el hardware.
 *
 * @param id      Identificador general del temporizador en la HAL.
 * @param grupo   Puntero donde guardaré el grupo de hardware resultante.
 * @param timer   Puntero donde guardaré el número de temporizador resultante.
 * @return true si el identificador es válido, false si no existe.
 */
static bool _hal_timer_decode(hal_timer_id_e id,
                               timer_grupo_e *grupo,
                               timer_e       *timer)
{
    // Mapeo directo entre la etiqueta de la HAL y la ubicación física
    switch (id) {
        case HAL_TIMER_0: *grupo = TMR_GRUPO_0; *timer = TIMER_0; return true;
        case HAL_TIMER_1: *grupo = TMR_GRUPO_0; *timer = TIMER_1; return true;
        case HAL_TIMER_2: *grupo = TMR_GRUPO_1; *timer = TIMER_0; return true;
        case HAL_TIMER_3: *grupo = TMR_GRUPO_1; *timer = TIMER_1; return true;
        default:                                                  return false;
    }
}

/**
 * @brief Estandariza los códigos de error.
 *
 * Convierte los errores específicos que me entrega el driver del hardware
 * a la lista de errores generales que maneja nuestra capa HAL.
 */
static hal_err_e _gpio_err_to_hal(gpio_err_e err)
{
    switch (err) {
        case GPIO_OK:          return HAL_OK;
        case GPIO_ERR_PIN_NUM: return HAL_ERR_PIN;
        default:               return HAL_ERR_PARAM;
    }
}

/* =========================================================================
 * IMPLEMENTACION HAL GPIO
 * ========================================================================= */

/**
 * @brief Prepara un pin para trabajar según la configuración indicada.
 *
 * @param cfg Estructura que contiene todas las reglas (entrada/salida, pull-up, etc.)
 * @return Código de error estándar de la HAL.
 */
hal_err_e hal_gpio_init(const hal_gpio_cfg_t *cfg)
{
    // Verificaciones de seguridad básicas
    if (cfg == NULL)   return HAL_ERR_PARAM;
    if (cfg->pin > 39) return HAL_ERR_PIN;

    gpio_err_e ret;

    // Si el usuario quiere que el pin sea una salida de energía
    if (cfg->dir == HAL_GPIO_OUTPUT) {
        
        // Guardo qué tipo de lógica usaremos (por si hay que invertir la señal luego)
        _gpio_logic[cfg->pin] = cfg->logic_type;

        // Mando la orden al driver físico
        ret = gpio_config_out(cfg->pin);
        if (ret != GPIO_OK) return _gpio_err_to_hal(ret);
        
        // Marco en mi memoria interna que este pin ahora es de salida
        _gpio_is_output[cfg->pin] = true;

        // Si me pidieron que el pin inicie encendido o apagado, aplico el nivel
        if (cfg->init_val == HAL_GPIO_HIGH) {
             hal_err_e werr = hal_gpio_write(cfg->pin, HAL_GPIO_HIGH);
            if (werr != HAL_OK) return werr;
        }
        else if (cfg->init_val == HAL_GPIO_LOW) {
             hal_err_e werr = hal_gpio_write(cfg->pin, HAL_GPIO_LOW);
            if (werr != HAL_OK) return werr;
        }

    // Si el usuario quiere que el pin sea una entrada de lectura
    } else { 

        pull_mode_e pull;
        // Traduzco el tipo de resistencia interna (Pull) al lenguaje del driver
        switch (cfg->pull) {
            case HAL_GPIO_PULLUP:   pull = PULLUP;   break;
            case HAL_GPIO_PULLDOWN: pull = PULLDOWN; break;
            default:                pull = NOPULL;   break;
        }

        // Configuro el hardware para leer
        ret = gpio_config_in(cfg->pin, pull);
        if (ret != GPIO_OK) return _gpio_err_to_hal(ret);

        // Me aseguro de recordar que este pin NO es de salida
        _gpio_is_output[cfg->pin] = false;

        // Si me pidieron configurar un aviso automático (interrupción) para este pin, lo enlazo
        if (cfg->intr_type != HAL_GPIO_INTR_DISABLE && cfg->intr_cb != NULL) {
            hal_err_e ierr = hal_gpio_intr_set(cfg->pin, cfg->intr_type,
                                               cfg->intr_cb, cfg->intr_arg);
            if (ierr != HAL_OK) return ierr;
        }
    }

    return HAL_OK;
}

/**
 * @brief Aplica un nivel de energía (Alto o Bajo) a un pin.
 *
 * @param pin   Número del pin físico.
 * @param level Nivel que se desea aplicar.
 */
hal_err_e hal_gpio_write(uint8_t pin, hal_gpio_level_e level)
{
    if (pin > 39)              return HAL_ERR_PIN;
    if (!_gpio_is_output[pin]) return HAL_ERR_NOT_INIT; // No puedo escribir en una entrada
    
    // Si la lógica del circuito está invertida, volteo la orden antes de mandarla
    if (_gpio_logic[pin] == HAL_NEGATIVE_LOGIC) {
        level = !level;
    }
    
    // Envío la orden final al hardware
    gpio_err_e ret = gpio_write(pin, (bool)level);
    if (ret != GPIO_OK) return _gpio_err_to_hal(ret);

    // Guardo el estado final en mi tabla para recordarlo más tarde
    _gpio_out_state[pin] = level;
    return HAL_OK;
}

/**
 * @brief Lee el estado actual de un pin.
 *
 * @param pin   Número del pin físico.
 * @param level Puntero donde guardaré la lectura.
 */
hal_err_e hal_gpio_read(uint8_t pin, hal_gpio_level_e *level)
{
    if (pin > 39)   return HAL_ERR_PIN;
    if (level == NULL) return HAL_ERR_PARAM;

    // Leo directamente del driver y lo traduzco a los valores de la HAL
    *level = gpio_read(pin) ? HAL_GPIO_HIGH : HAL_GPIO_LOW;
    return HAL_OK;
}

/**
 * @brief Invierte el estado actual de un pin de salida.
 *
 * Como no siempre puedo leer el estado real de una salida físicamente,
 * reviso mi memoria interna para saber cómo estaba y le aplico lo contrario.
 */
hal_err_e hal_gpio_toggle(uint8_t pin)
{
    if (pin > 39)              return HAL_ERR_PIN;
    if (!_gpio_is_output[pin]) return HAL_ERR_NOT_INIT;

    // Calculo el valor opuesto al que tengo guardado
    hal_gpio_level_e new_level = (_gpio_out_state[pin] == HAL_GPIO_HIGH)
                                 ? HAL_GPIO_LOW
                                 : HAL_GPIO_HIGH;
                                 
    // Reutilizo mi propia función de escritura para aplicar el cambio
    return hal_gpio_write(pin, new_level);
}

/* --- Interrupciones GPIO ------------------------------------------------- */

/**
 * @brief Configura la respuesta a un evento eléctrico en un pin.
 */
hal_err_e hal_gpio_intr_set(uint8_t pin, hal_gpio_intr_type_e intr_type,
                              hal_gpio_intr_cb_t callback, void *arg)
{
    if (pin > 39)      return HAL_ERR_PIN;
    if (!callback)     return HAL_ERR_PARAM;

    // Paso los parámetros directo al driver inferior
    gpio_err_e ret = gpio_drv_intr_set(pin, (gpio_intr_type_e)intr_type, 
                                   (gpio_intr_callback_t)callback, arg);
                                   
    return (ret == GPIO_OK) ? HAL_OK :
           (ret == GPIO_ERR_PIN_NUM) ? HAL_ERR_PIN : HAL_ERR_PARAM;
}

/** Habilita la interrupción en un pin específico. */
hal_err_e hal_gpio_intr_enable(uint8_t pin)
{
    if (pin > 39) return HAL_ERR_PIN;
    return (gpio_drv_intr_enable(pin) == GPIO_OK) ? HAL_OK : HAL_ERR_PIN;
}

/** Deshabilita temporalmente la interrupción de un pin. */
hal_err_e hal_gpio_intr_disable(uint8_t pin)
{
    if (pin > 39) return HAL_ERR_PIN;
    return (gpio_drv_intr_disable(pin) == GPIO_OK) ? HAL_OK : HAL_ERR_PIN;
}

/** Limpia la bandera de aviso después de que ocurrió una interrupción. */
hal_err_e hal_gpio_intr_clear(uint8_t pin)
{
    if (pin > 39) return HAL_ERR_PIN;
    return (gpio_drv_intr_clear(pin) == GPIO_OK) ? HAL_OK : HAL_ERR_PIN;
}

/* =========================================================================
 * IMPLEMENTACION HAL TIMER
 * ========================================================================= */

/**
 * @brief Inicializa un temporizador desde la capa general.
 *
 * Utiliza la configuración provista para ubicar el hardware correcto y 
 * aplicar todos los ajustes necesarios.
 */
hal_err_e hal_timer_init(const hal_timer_cfg_t *cfg)
{
    if (cfg == NULL) return HAL_ERR_PARAM;

    timer_grupo_e grupo;
    timer_e       timer;
    
    // Si el temporizador solicitado no existe, detengo el proceso
    if (!_hal_timer_decode(cfg->id, &grupo, &timer)) return HAL_ERR_PARAM;

    // Ejecuto la configuración en el driver físico
    timer_config(grupo, timer,
                 cfg->count_down,
                 cfg->divisor,
                 cfg->alarm_val,
                 cfg->autoreload,
                 cfg->alarm_en);

    // Si el usuario incluyó una función de respuesta, armo las interrupciones
    if (cfg->callback != NULL) {
        // Traduzco el tipo de aviso al que entiende el driver
        tx_int_type_e int_type = (cfg->int_type == HAL_TIMER_INT_LEVEL)
                                 ? TMR_LEVEL_INT
                                 : TMR_EDGE_INT;

        timer_setINTR(grupo, timer, int_type,
                      (timer_intr_callback_t)cfg->callback,
                      cfg->cb_arg);
    }

    return HAL_OK;
}

/**
 * @brief Arranca el conteo de un temporizador específico.
 */
hal_err_e hal_timer_start(hal_timer_id_e id)
{
    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    timer_start(grupo, timer);
    return HAL_OK;
}

/**
 * @brief Pausa el conteo de un temporizador específico.
 */
hal_err_e hal_timer_stop(hal_timer_id_e id)
{
    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    timer_stop(grupo, timer);
    return HAL_OK;
}

/**
 * @brief Reinicia el contador a cero y lo arranca de nuevo.
 */
hal_err_e hal_timer_restart(hal_timer_id_e id)
{
    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    // Secuencia segura: Detengo, reseteo a 0, y vuelvo a arrancar
    timer_stop(grupo, timer);
    timer_setCont(grupo, timer, 0ULL);
    timer_start(grupo, timer);
    return HAL_OK;
}

/**
 * @brief Extrae el valor actual del contador físico.
 */
hal_err_e hal_timer_get_count(hal_timer_id_e id, uint64_t *value)
{
    if (value == NULL) return HAL_ERR_PARAM;

    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    *value = timer_getCont(grupo, timer);
    return HAL_OK;
}

/**
 * @brief Fuerza al contador a tomar un valor específico.
 */
hal_err_e hal_timer_set_count(hal_timer_id_e id, uint64_t value)
{
    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    timer_setCont(grupo, timer, value);
    return HAL_OK;
}

/**
 * @brief Limpia la señal de alarma de un temporizador.
 *
 * Obligatorio llamar a esta función dentro de nuestra rutina de respuesta a 
 * la alarma para poder recibir los próximos avisos.
 */
hal_err_e hal_timer_clear_intr(hal_timer_id_e id)
{
    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    timer_clearINTR(grupo, timer);
    return HAL_OK;
}