/**
 * @file hal.c
 * @brief Implementacion de la Hardware Abstraction Layer (HAL)
 *
 * Traduce las llamadas de la HAL a los drivers de bajo nivel:
 *   - driver_gpio  (gpio_config_out, gpio_config_in, gpio_read, gpio_write)
 *   - driver_timer (timer_config, timer_start, timer_stop, timer_setINTR,
 *                   timer_clearINTR, timer_setCont, timer_getCont)
 */

#include "hal_board.h"
#include "driver_gpio.h"
#include "driver_timer.h"

/* =========================================================================
 *  UTILIDADES INTERNAS
 * ========================================================================= */

/**
 * @brief Tabla de estado interno de los pines de salida.
 *
 * Se necesita para implementar hal_gpio_toggle(), ya que el driver de bajo
 * nivel no expone una funcion de lectura del registro de salida.
 * Solo pines 0-39 son validos en el ESP32.
 */
static hal_gpio_level_e _gpio_out_state[40] = {HAL_GPIO_LOW};

/** Marca si un pin fue inicializado como OUTPUT */
static bool _gpio_is_output[40] = {false};

static hal_gpio_type_logic_e _gpio_logic[40] = {HAL_POSITIVE_LOGIC};

/**
 * @brief Descompone un hal_timer_id_e en grupo y timer del driver.
 *
 * @param id      Identificador HAL del timer.
 * @param grupo   [out] Grupo del driver (TMR_GRUPO_0 / TMR_GRUPO_1).
 * @param timer   [out] Timer dentro del grupo (TIMER_0 / TIMER_1).
 * @return        true si el id es valido.
 */
static bool _hal_timer_decode(hal_timer_id_e id,
                               timer_grupo_e *grupo,
                               timer_e       *timer)
{
    switch (id) {
        case HAL_TIMER_0: *grupo = TMR_GRUPO_0; *timer = TIMER_0; return true;
        case HAL_TIMER_1: *grupo = TMR_GRUPO_0; *timer = TIMER_1; return true;
        case HAL_TIMER_2: *grupo = TMR_GRUPO_1; *timer = TIMER_0; return true;
        case HAL_TIMER_3: *grupo = TMR_GRUPO_1; *timer = TIMER_1; return true;
        default:                                                   return false;
    }
}

/**
 * @brief Convierte un gpio_err_e del driver a hal_err_e.
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
 *  IMPLEMENTACION HAL GPIO
 * ========================================================================= */

hal_err_e hal_gpio_init(const hal_gpio_cfg_t *cfg)
{
    if (cfg == NULL)   return HAL_ERR_PARAM;
    if (cfg->pin > 39) return HAL_ERR_PIN;

    gpio_err_e ret;

    if (cfg->dir == HAL_GPIO_OUTPUT) {
        _gpio_logic[cfg->pin] = cfg->logic_type;

        ret = gpio_config_out(cfg->pin);
        if (ret != GPIO_OK) return _gpio_err_to_hal(ret);
        _gpio_is_output[cfg->pin] = true;

        /* Aplicar nivel inicial si se pide HIGH */
        if (cfg->init_val == HAL_GPIO_HIGH) {
             hal_err_e werr = hal_gpio_write(cfg->pin, HAL_GPIO_HIGH);
            if (werr != HAL_OK) return werr;
        }
        else if (cfg->init_val == HAL_GPIO_LOW) {
             hal_err_e werr = hal_gpio_write(cfg->pin, HAL_GPIO_LOW);
            if (werr != HAL_OK) return werr;
        }

        /* Ignorar campos de interrupcion en pines de salida */

    } else { /* HAL_GPIO_INPUT */

        pull_mode_e pull;
        switch (cfg->pull) {
            case HAL_GPIO_PULLUP:   pull = PULLUP;   break;
            case HAL_GPIO_PULLDOWN: pull = PULLDOWN; break;
            default:                pull = NOPULL;   break;
        }

        ret = gpio_config_in(cfg->pin, pull);
        if (ret != GPIO_OK) return _gpio_err_to_hal(ret);

        _gpio_is_output[cfg->pin] = false;

        /* Configurar interrupcion si se especifico tipo y callback */
        if (cfg->intr_type != HAL_GPIO_INTR_DISABLE && cfg->intr_cb != NULL) {
            hal_err_e ierr = hal_gpio_intr_set(cfg->pin, cfg->intr_type,
                                               cfg->intr_cb, cfg->intr_arg);
            if (ierr != HAL_OK) return ierr;
        }
    }

    return HAL_OK;
}

hal_err_e hal_gpio_write(uint8_t pin, hal_gpio_level_e level)
{
    if (pin > 39)              return HAL_ERR_PIN;
    if (!_gpio_is_output[pin]) return HAL_ERR_NOT_INIT;
    if (_gpio_logic[pin] == HAL_NEGATIVE_LOGIC) {
        level = !level;
    }
    gpio_err_e ret = gpio_write(pin, (bool)level);

    if (ret != GPIO_OK) return _gpio_err_to_hal(ret);

    _gpio_out_state[pin] = level;
    return HAL_OK;
}

hal_err_e hal_gpio_read(uint8_t pin, hal_gpio_level_e *level)
{
    if (pin > 39)   return HAL_ERR_PIN;
    if (level == NULL) return HAL_ERR_PARAM;

    *level = gpio_read(pin) ? HAL_GPIO_HIGH : HAL_GPIO_LOW;
    return HAL_OK;
}

hal_err_e hal_gpio_toggle(uint8_t pin)
{
    if (pin > 39)              return HAL_ERR_PIN;
    if (!_gpio_is_output[pin]) return HAL_ERR_NOT_INIT;

    hal_gpio_level_e new_level = (_gpio_out_state[pin] == HAL_GPIO_HIGH)
                                 ? HAL_GPIO_LOW
                                 : HAL_GPIO_HIGH;
    return hal_gpio_write(pin, new_level);
}

/* --- Interrupciones GPIO ------------------------------------------------- */

hal_err_e hal_gpio_intr_set(uint8_t pin, hal_gpio_intr_type_e intr_type,
                              hal_gpio_intr_cb_t callback, void *arg)
{
    if (pin > 39)      return HAL_ERR_PIN;
    if (!callback)     return HAL_ERR_PARAM;

    gpio_err_e ret = gpio_drv_intr_set(pin, (gpio_intr_type_e)intr_type, 
                                   (gpio_intr_callback_t)callback, arg);
    return (ret == GPIO_OK) ? HAL_OK :
           (ret == GPIO_ERR_PIN_NUM) ? HAL_ERR_PIN : HAL_ERR_PARAM;
}

hal_err_e hal_gpio_intr_enable(uint8_t pin)
{
    if (pin > 39) return HAL_ERR_PIN;
    return (gpio_drv_intr_enable(pin) == GPIO_OK) ? HAL_OK : HAL_ERR_PIN;
}

hal_err_e hal_gpio_intr_disable(uint8_t pin)
{
    if (pin > 39) return HAL_ERR_PIN;
    return (gpio_drv_intr_disable(pin) == GPIO_OK) ? HAL_OK : HAL_ERR_PIN;
}

hal_err_e hal_gpio_intr_clear(uint8_t pin)
{
    if (pin > 39) return HAL_ERR_PIN;
    return (gpio_drv_intr_clear(pin) == GPIO_OK) ? HAL_OK : HAL_ERR_PIN;
}

/* =========================================================================
 *  IMPLEMENTACION HAL TIMER
 * ========================================================================= */

hal_err_e hal_timer_init(const hal_timer_cfg_t *cfg)
{
    if (cfg == NULL) return HAL_ERR_PARAM;

    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(cfg->id, &grupo, &timer)) return HAL_ERR_PARAM;

    timer_config(grupo, timer,
                 cfg->count_down,
                 cfg->divisor,
                 
                 cfg->alarm_val,
                 cfg->autoreload,
                 cfg->alarm_en);

    if (cfg->callback != NULL) {
        tx_int_type_e int_type = (cfg->int_type == HAL_TIMER_INT_LEVEL)
                                 ? TMR_LEVEL_INT
                                 : TMR_EDGE_INT;

        timer_setINTR(grupo, timer, int_type,
                      (timer_intr_callback_t)cfg->callback,
                      cfg->cb_arg);
    }

    return HAL_OK;
}

hal_err_e hal_timer_start(hal_timer_id_e id)
{
    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    timer_start(grupo, timer);
    return HAL_OK;
}

hal_err_e hal_timer_stop(hal_timer_id_e id)
{
    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    timer_stop(grupo, timer);
    return HAL_OK;
}

hal_err_e hal_timer_restart(hal_timer_id_e id)
{
    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    timer_stop(grupo, timer);
    timer_setCont(grupo, timer, 0ULL);
    timer_start(grupo, timer);
    return HAL_OK;
}

hal_err_e hal_timer_get_count(hal_timer_id_e id, uint64_t *value)
{
    if (value == NULL) return HAL_ERR_PARAM;

    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    *value = timer_getCont(grupo, timer);
    return HAL_OK;
}

hal_err_e hal_timer_set_count(hal_timer_id_e id, uint64_t value)
{
    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    timer_setCont(grupo, timer, value);
    return HAL_OK;
}

hal_err_e hal_timer_clear_intr(hal_timer_id_e id)
{
    timer_grupo_e grupo;
    timer_e       timer;
    if (!_hal_timer_decode(id, &grupo, &timer)) return HAL_ERR_PARAM;

    timer_clearINTR(grupo, timer);
    return HAL_OK;
}