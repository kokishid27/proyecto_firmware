/**
 * @file hal.h
 * @brief Hardware Abstraction Layer (HAL) - GPIO y Timer
 *
 * Capa de abstraccion que unifica el acceso al hardware de GPIO y Timer,
 * desacoplando la logica de aplicacion de los drivers de bajo nivel.
 */

#ifndef HAL_BOARD_H
#define HAL_BOARD_H

#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 *  TIPOS Y ENUMERACIONES COMUNES
 * ========================================================================= */

/**
 * @brief Codigos de retorno unificados de la HAL.
 */
typedef enum {
    HAL_OK             =  0,  /**< Operacion exitosa                        */
    HAL_ERR_PIN        = -1,  /**< Numero de pin invalido o no disponible   */
    HAL_ERR_PARAM      = -2,  /**< Parametro fuera de rango                 */
    HAL_ERR_NOT_INIT   = -3,  /**< Periferico no inicializado               */
    HAL_ERR_TIMEOUT    = -4,  /**< Tiempo de espera agotado                 */
} hal_err_e;

/* =========================================================================
 *  HAL GPIO
 * ========================================================================= */

/**
 * @brief Modo de resistencia de pull del pin.
 */
typedef enum {
    HAL_GPIO_NOPULL   = 0,  /**< Sin resistencia interna    */
    HAL_GPIO_PULLUP   = 1,  /**< Pull-up habilitado         */
    HAL_GPIO_PULLDOWN = 2,  /**< Pull-down habilitado       */
} hal_gpio_pull_e;

/**
 * @brief Direccion del pin GPIO.
 */
typedef enum {
    HAL_GPIO_INPUT  = 0,  /**< Pin configurado como entrada  */
    HAL_GPIO_OUTPUT = 1,  /**< Pin configurado como salida   */
} hal_gpio_dir_e;

/**
 * @brief Nivel logico del pin GPIO.
 */
typedef enum {
    HAL_GPIO_LOW  = 0,  /**< Nivel bajo  */
    HAL_GPIO_HIGH = 1,  /**< Nivel alto  */
} hal_gpio_level_e;

/**
 * @brief Estructura de configuracion de un pin GPIO.
 *
 * Se pasa a hal_gpio_init() para configurar el pin en una sola llamada.
 */
typedef struct {
    uint8_t          pin;       /**< Numero de pin (0-39)          */
    hal_gpio_dir_e   dir;       /**< Direccion: INPUT u OUTPUT      */
    hal_gpio_pull_e  pull;      /**< Pull-up, pull-down o ninguno   */
    hal_gpio_level_e init_val;  /**< Valor inicial (solo salidas)   */
} hal_gpio_cfg_t;

/**
 * @brief Inicializa un pin GPIO con la configuracion indicada.
 *
 * @param cfg  Puntero a la estructura de configuracion.
 * @return     HAL_OK si la inicializacion fue exitosa, error en caso contrario.
 */
hal_err_e hal_gpio_init(const hal_gpio_cfg_t *cfg);

/**
 * @brief Escribe un nivel logico en un pin de salida.
 *
 * @param pin    Numero de pin (0-39).
 * @param level  HAL_GPIO_LOW o HAL_GPIO_HIGH.
 * @return       HAL_OK si la escritura fue exitosa, error en caso contrario.
 */
hal_err_e hal_gpio_write(uint8_t pin, hal_gpio_level_e level);

/**
 * @brief Lee el nivel logico de un pin de entrada.
 *
 * @param pin    Numero de pin (0-39).
 * @param level  Puntero donde se almacena el nivel leido.
 * @return       HAL_OK si la lectura fue exitosa, error en caso contrario.
 */
hal_err_e hal_gpio_read(uint8_t pin, hal_gpio_level_e *level);

/**
 * @brief Invierte el estado actual de un pin de salida (toggle).
 *
 * @param pin  Numero de pin (0-39).
 * @return     HAL_OK si la operacion fue exitosa, error en caso contrario.
 */
hal_err_e hal_gpio_toggle(uint8_t pin);

/* =========================================================================
 *  HAL TIMER
 * ========================================================================= */

/**
 * @brief Identificador de la instancia de timer (grupo + timer).
 */
typedef enum {
    HAL_TIMER_0 = 0,  /**< Grupo 0, Timer 0  */
    HAL_TIMER_1 = 1,  /**< Grupo 0, Timer 1  */
    HAL_TIMER_2 = 2,  /**< Grupo 1, Timer 0  */
    HAL_TIMER_3 = 3,  /**< Grupo 1, Timer 1  */
} hal_timer_id_e;

/**
 * @brief Tipo de disparo de la interrupcion del timer.
 */
typedef enum {
    HAL_TIMER_INT_LEVEL = 0,  /**< Interrupcion por nivel */
    HAL_TIMER_INT_EDGE  = 1,  /**< Interrupcion por flanco */
} hal_timer_int_type_e;

/**
 * @brief Prototipo del callback de interrupcion del timer.
 *
 * @param arg  Argumento de usuario registrado con hal_timer_init().
 */
typedef void (*hal_timer_callback_t)(void *arg);

/**
 * @brief Estructura de configuracion del timer.
 */
typedef struct {
    hal_timer_id_e       id;           /**< Timer a configurar (0-3)              */
    uint16_t             divisor;      /**< Preescalador del reloj (2-65535)      */
    uint64_t             alarm_val;    /**< Valor de alarma (conteo objetivo)     */
    bool                 autoreload;   /**< Recarga automatica al alcanzar alarma */
    bool                 count_down;   /**< true = cuenta regresiva               */
    bool                 alarm_en;     /**< Habilitar alarma al inicializar       */
    hal_timer_int_type_e int_type;     /**< Tipo de interrupcion (nivel/flanco)   */
    hal_timer_callback_t callback;     /**< Funcion ISR (NULL si no se usa)       */
    void                *cb_arg;       /**< Argumento para el callback             */
} hal_timer_cfg_t;

/**
 * @brief Inicializa y configura un timer segun la estructura dada.
 *
 * Configura divisor, alarma, autoreload y, si se provee callback,
 * registra la interrupcion. El timer no se inicia automaticamente.
 *
 * @param cfg  Puntero a la estructura de configuracion.
 * @return     HAL_OK si la inicializacion fue exitosa, error en caso contrario.
 */
hal_err_e hal_timer_init(const hal_timer_cfg_t *cfg);

/**
 * @brief Inicia la cuenta del timer especificado.
 *
 * @param id  Identificador del timer.
 * @return    HAL_OK si la operacion fue exitosa, error en caso contrario.
 */
hal_err_e hal_timer_start(hal_timer_id_e id);

/**
 * @brief Detiene la cuenta del timer especificado.
 *
 * @param id  Identificador del timer.
 * @return    HAL_OK si la operacion fue exitosa, error en caso contrario.
 */
hal_err_e hal_timer_stop(hal_timer_id_e id);

/**
 * @brief Reinicia el contador del timer a cero y lo inicia.
 *
 * @param id  Identificador del timer.
 * @return    HAL_OK si la operacion fue exitosa, error en caso contrario.
 */
hal_err_e hal_timer_restart(hal_timer_id_e id);

/**
 * @brief Obtiene el valor actual del contador del timer.
 *
 * @param id     Identificador del timer.
 * @param value  Puntero donde se almacena el valor del contador.
 * @return       HAL_OK si la lectura fue exitosa, error en caso contrario.
 */
hal_err_e hal_timer_get_count(hal_timer_id_e id, uint64_t *value);

/**
 * @brief Establece el valor del contador del timer.
 *
 * @param id     Identificador del timer.
 * @param value  Valor a cargar en el contador.
 * @return       HAL_OK si la operacion fue exitosa, error en caso contrario.
 */
hal_err_e hal_timer_set_count(hal_timer_id_e id, uint64_t value);

/**
 * @brief Limpia el flag de interrupcion del timer y rehabilita la alarma.
 *
 * Debe llamarse al inicio del ISR para poder recibir la siguiente alarma.
 *
 * @param id  Identificador del timer.
 * @return    HAL_OK si la operacion fue exitosa, error en caso contrario.
 */
hal_err_e hal_timer_clear_intr(hal_timer_id_e id);

#endif /* HAL_H */