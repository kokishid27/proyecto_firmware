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
 * 
 * @brief Direccion del pin GPIO.
 
 */

typedef enum {
    HAL_NEGATIVE_LOGIC = 0, /**< Logica negativa: LOW=ON, HIGH=OFF */
    HAL_POSITIVE_LOGIC = 1, /**< Logica positiva: HIGH=ON, LOW=OFF */
} hal_gpio_type_logic_e;

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
 * @brief Tipo de disparo de la interrupcion GPIO.
 *
 * Definido antes de hal_gpio_cfg_t para poder usarlo como campo de la struct.
 */
typedef enum {
    HAL_GPIO_INTR_DISABLE    = 0, /**< Sin interrupcion               */
    HAL_GPIO_INTR_POS_EDGE   = 1, /**< Flanco ascendente              */
    HAL_GPIO_INTR_NEG_EDGE   = 2, /**< Flanco descendente             */
    HAL_GPIO_INTR_ANY_EDGE   = 3, /**< Cualquier flanco               */
    HAL_GPIO_INTR_LOW_LEVEL  = 4, /**< Nivel bajo (mantenido)         */
    HAL_GPIO_INTR_HIGH_LEVEL = 5, /**< Nivel alto (mantenido)         */
} hal_gpio_intr_type_e;

/**
 * @brief Prototipo del callback de interrupcion GPIO.
 *
 * Se ejecuta en contexto de ISR: debe ser breve y declararse IRAM_ATTR.
 *
 * @param pin  Pin que genero la interrupcion.
 * @param arg  Argumento de usuario registrado en hal_gpio_cfg_t.
 */
typedef void (*hal_gpio_intr_cb_t)(uint8_t pin, void *arg);

/**
 * @brief Estructura de configuracion de un pin GPIO.
 *
 * Agrupa la configuracion electrica y la de interrupcion en un solo bloque.
 * Se pasa a hal_gpio_init() para configurar el pin en una sola llamada.
 *
 * Campos de interrupcion (intr_type, intr_cb, intr_arg):
 *   - Solo aplican a pines de entrada (dir == HAL_GPIO_INPUT).
 *   - Si intr_type == HAL_GPIO_INTR_DISABLE o intr_cb == NULL,
 *     no se configura ninguna interrupcion.
 */
typedef struct {
    /* --- Configuracion electrica --- */
    uint8_t               pin;       /**< Numero de pin (0-39)                       */
    hal_gpio_dir_e        dir;       /**< Direccion: INPUT u OUTPUT                  */
    hal_gpio_pull_e       pull;      /**< Pull-up, pull-down o ninguno               */
    hal_gpio_level_e      init_val;  /**< Nivel inicial (solo OUTPUT)                */
    hal_gpio_type_logic_e logic_type; /**< Tipo de logica (solo OUTPUT)                */

    /* --- Configuracion de interrupcion (solo INPUT) --- */
    hal_gpio_intr_type_e  intr_type; /**< Tipo de disparo; DISABLE = sin interrupcion */
    hal_gpio_intr_cb_t    intr_cb;   /**< Callback ISR (NULL = sin interrupcion)      */
    void                 *intr_arg;  /**< Argumento de usuario para el callback        */
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

/* --- Interrupciones GPIO ------------------------------------------------- */

/**
 * @brief Configura la interrupcion de un pin de entrada y registra callback.
 *
 * El pin debe estar inicializado como HAL_GPIO_INPUT con hal_gpio_init().
 * La primera llamada instala automaticamente el servicio de ISR compartido.
 *
 * @param pin        Numero de pin (0-39).
 * @param intr_type  Tipo de disparo.
 * @param callback   Funcion ISR (debe ser IRAM_ATTR).
 * @param arg        Argumento de usuario para el callback.
 * @return           HAL_OK, HAL_ERR_PIN o HAL_ERR_PARAM.
 */
hal_err_e hal_gpio_intr_set(uint8_t pin, hal_gpio_intr_type_e intr_type,
                             hal_gpio_intr_cb_t callback, void *arg);

/**
 * @brief Habilita la interrupcion de un pin ya configurado.
 * @param pin  Numero de pin (0-39).
 * @return     HAL_OK o HAL_ERR_PIN.
 */
hal_err_e hal_gpio_intr_enable(uint8_t pin);

/**
 * @brief Deshabilita temporalmente la interrupcion de un pin.
 * @param pin  Numero de pin (0-39).
 * @return     HAL_OK o HAL_ERR_PIN.
 */
hal_err_e hal_gpio_intr_disable(uint8_t pin);

/**
 * @brief Limpia el flag de interrupcion pendiente de un pin.
 *
 * Llamar dentro del callback para interrupciones por nivel.
 *
 * @param pin  Numero de pin (0-39).
 * @return     HAL_OK o HAL_ERR_PIN.
 */
hal_err_e hal_gpio_intr_clear(uint8_t pin);

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