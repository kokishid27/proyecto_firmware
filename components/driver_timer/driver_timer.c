/**
 * @file driver_timer.c
 * @author Diego Alejandro Saenz Duarte, Hector Teruel Grado, Jorge Habib Hernandez Rojas
 * 
 * @brief Driver de temporizadores para ESP32 con soporte de interrupciones
 *
 * Este módulo implementa funciones de configuración y control de los
 * temporizadores por hardware del ESP32, incluyendo la gestión de alarmas
 * e interrupciones asociadas.
 *
 * Arquitectura de interrupciones:
 *  - El ESP32 expone una única línea IRQ para todos los pines GPIO
 *    (fuente ETS_GPIO_INTR_SOURCE).
 *  - Este driver instala una ISR maestra (_gpio_isr_master) que lee los
 *    registros GPIO_STATUS / GPIO_STATUS1, identifica los pines que
 *    dispararon la interrupción, limpia sus banderas y despacha el
 *    callback registrado por el usuario.
 *  - Cada pin dispone de una entrada en la tabla _intr_table[40], donde
 *    se almacena su tipo de interrupción, el callback asociado y el
 *    argumento de usuario.
 *
 * Funcionalidades principales:
 *  - Configuración de temporizadores (modo ascendente/descendente,
 *    divisor de reloj, valor de alarma, autorecarga).
 *  - Inicio y detención de temporizadores.
 *  - Registro y limpieza de interrupciones por tiempo.
 *  - Lectura y escritura del valor del contador de 64 bits.
 *
 * Notas de implementación:
 *  - Se emplean macros auxiliares (GET_HIGH32B, GET_LOW32B) para dividir
 *    valores de 64 bits en registros de 32 bits.
 *  - Las rutinas de interrupción se asignan mediante esp_intr_alloc(),
 *    con la bandera ESP_INTR_FLAG_IRAM para ejecución en memoria IRAM.
 */
#include <stdio.h>
#include "driver_timer.h"

#include "soc/timer_group_reg.h"   /**< Definiciones de registros para el mapeo de los grupos de temporizadores del ESP32. */
#include "soc/timer_group_struct.h" /**< Estructuras de datos que representan los registros de los grupos de temporizadores. */
#include "esp_intr_alloc.h"        /**< Librería de la API de ESP-IDF para la asignación y manejo de interrupciones, incluyendo esp_intr_alloc() para registrar rutinas de servicio (ISR). */


#define GET_HIGH32B(x) ((uint32_t)(x >> 32))
#define GET_LOW32B(x)  ((uint32_t)(x & 0xFFFFFFFFULL))

void timer_config(timer_grupo_e grupo, timer_e timer, bool cuenta_desc, uint16_t divisor, uint64_t alarm_val, bool autoreload, bool alarm_en)
{
    // Puntero a la estructura correspondiente segun el grupo dado
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    // Se asegura que el timer este deshabilitado antes de configurarlo
    ptr_tmrG->hw_timer[timer].config.tx_en = 0;

    // Se asigna el divisor (prescaler) para ajustar la frecuencia del timer
    ptr_tmrG->hw_timer[timer].config.tx_divider = divisor;

    // Se define la direccion del conteo: 0 si es descendente, 1: si es ascendente
    ptr_tmrG->hw_timer[timer].config.tx_increase = (cuenta_desc)? 0 : 1;

    // Se configura si el timer debe auto recargarse
    ptr_tmrG->hw_timer[timer].config.tx_autoreload = (autoreload)? 1 : 0;
    
    // Se carga el valor de la alarma en dos regs de 32 bits cada uno
    ptr_tmrG->hw_timer[timer].alarmhi.val = GET_HIGH32B(alarm_val);
    ptr_tmrG->hw_timer[timer].alarmlo.val = GET_LOW32B(alarm_val);

    // Se habilita o deshabilita la generacion de alarma segun parametro
    ptr_tmrG->hw_timer[timer].config.tx_alarm_en = (alarm_en)? 1 : 0;
}

/**
 * @brief Inicia el conteo de un temporizador.
 *
 * Esta función habilita el temporizador indicado dentro del grupo
 * correspondiente (TIMERG0 o TIMERG1). Al poner el bit de habilitación
 * en 1, el temporizador comienza a contar según la configuración previa.
 *
 * @param grupo Grupo de temporizadores al que pertenece (TMR_GRUPO_0 o TMR_GRUPO_1).
 * @param timer Número de temporizador dentro del grupo (TIMER_0 o TIMER_1).
 */
void timer_start(timer_grupo_e grupo, timer_e timer)
{
    // Se obtiene un puntero al grupo de temporizadores correspondiente.
    // Si el grupo es TMR_GRUPO_0 se apunta a TIMERG0, de lo contrario a TIMERG1.
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    // Se habilita el temporizador seleccionado poniendo el bit tx_en en 1.
    ptr_tmrG->hw_timer[timer].config.tx_en = 1;
}

/**
 * @brief Detiene el conteo de un temporizador.
 *
 * Esta función deshabilita el temporizador indicado dentro del grupo
 * correspondiente (TIMERG0 o TIMERG1). Al poner el bit de habilitación
 * en 0, el temporizador deja de contar.
 *
 * @param grupo Grupo de temporizadores al que pertenece (TMR_GRUPO_0 o TMR_GRUPO_1).
 * @param timer Número de temporizador dentro del grupo (TIMER_0 o TIMER_1).
 */
void timer_stop(timer_grupo_e grupo, timer_e timer)
{
    // Se obtiene un puntero al grupo de temporizadores correspondiente.
    // Si el grupo es TMR_GRUPO_0 se apunta a TIMERG0, de lo contrario a TIMERG1.
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    // Se deshabilita el temporizador seleccionado poniendo el bit tx_en en 0.
    ptr_tmrG->hw_timer[timer].config.tx_en = 0;
}

/**
 * @brief Configura y habilita la interrupción de un temporizador.
 *
 * Esta función activa la interrupción para el temporizador indicado dentro del grupo
 * correspondiente (TIMERG0 o TIMERG1). Permite seleccionar el tipo de interrupción
 * (nivel o flanco) y registrar una función de callback que será llamada cuando
 * ocurra la interrupción. Además, se asigna la fuente de interrupción en la matriz
 * de interrupciones del ESP32 mediante la función esp_intr_alloc().
 *
 * @param grupo Grupo de temporizadores al que pertenece (TMR_GRUPO_0 o TMR_GRUPO_1).
 * @param timer Número de temporizador dentro del grupo (TIMER_0 o TIMER_1).
 * @param int_type Tipo de interrupción: nivel (TMR_LEVEL_INT) o flanco (TMR_EDGE_INT).
 * @param callback Función de callback que se ejecutará cuando ocurra la interrupción.
 * @param arg Argumento que se pasa a la función de callback.
 */
void timer_setINTR(timer_grupo_e grupo, timer_e timer, tx_int_type_e int_type, timer_intr_callback_t callback, void * arg)
{
    // Se obtiene un puntero al grupo de temporizadores correspondiente.
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    // Se habilita la interrupción del temporizador seleccionado (T0 o T1).
    if (timer == TIMER_0) 
        ptr_tmrG->int_ena_timers.t0_int_ena = 1;
    else 
        ptr_tmrG->int_ena_timers.t1_int_ena = 1;
    
    // Se configura el tipo de interrupción: nivel o flanco.
    if (int_type == TMR_LEVEL_INT) {
        ptr_tmrG->hw_timer[timer].config.tx_level_int_en = 1; // habilita interrupción por nivel
        ptr_tmrG->hw_timer[timer].config.tx_edge_int_en = 0;  // deshabilita interrupción por flanco
    }
    else {
        ptr_tmrG->hw_timer[timer].config.tx_level_int_en = 0; // deshabilita interrupción por nivel
        ptr_tmrG->hw_timer[timer].config.tx_edge_int_en = 1;  // habilita interrupción por flanco
    }

    // Se selecciona la fuente de interrupción correspondiente según grupo y temporizador.
    int scr_intr = (grupo == TMR_GRUPO_0)? 
                    ((timer == TIMER_0)? ETS_TG0_T0_LEVEL_INTR_SOURCE : ETS_TG0_T1_LEVEL_INTR_SOURCE) :
                    ((timer == TIMER_0)? ETS_TG1_T0_LEVEL_INTR_SOURCE : ETS_TG1_T1_LEVEL_INTR_SOURCE);
    
    // Se registra la interrupción en la matriz de interrupciones del ESP32.
    // Se pasa el callback y su argumento, con la bandera ESP_INTR_FLAG_IRAM para ejecución en IRAM.
    esp_intr_alloc(
        scr_intr,
        ESP_INTR_FLAG_IRAM,
        (intr_handler_t)callback,
        arg,
        NULL
    );
}

/**
 * @brief Limpia la interrupción de un temporizador.
 *
 * Esta función borra el flag de interrupción del temporizador indicado
 * dentro del grupo correspondiente (TIMERG0 o TIMERG1). Una vez limpiado,
 * se vuelve a habilitar la alarma para que el temporizador pueda generar
 * nuevas interrupciones.
 *
 * @param grupo Grupo de temporizadores al que pertenece (TMR_GRUPO_0 o TMR_GRUPO_1).
 * @param timer Número de temporizador dentro del grupo (TIMER_0 o TIMER_1).
 */
void timer_clearINTR(timer_grupo_e grupo, timer_e timer)
{
    // Se obtiene un puntero al grupo de temporizadores correspondiente.
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    // Se limpia el flag de interrupción del temporizador seleccionado.
    if (timer == TIMER_0) 
        ptr_tmrG->int_clr_timers.t0_int_clr = 1;
    else 
        ptr_tmrG->int_clr_timers.t1_int_clr = 1;

    // Se vuelve a habilitar la alarma para permitir futuras interrupciones.
    ptr_tmrG->hw_timer[timer].config.tx_alarm_en = 1;
}

/**
 * @brief Establece un valor en el contador de un temporizador.
 *
 * Esta función carga un valor de 64 bits en el contador del temporizador
 * indicado dentro del grupo correspondiente. El valor se divide en dos
 * registros de 32 bits (parte alta y parte baja) y se activa la carga.
 *
 * @param grupo Grupo de temporizadores al que pertenece (TMR_GRUPO_0 o TMR_GRUPO_1).
 * @param timer Número de temporizador dentro del grupo (TIMER_0 o TIMER_1).
 * @param cont_val Valor de 64 bits que se desea cargar en el contador.
 */
void timer_setCont(timer_grupo_e grupo, timer_e timer, uint64_t cont_val)
{
    // Se obtiene un puntero al grupo de temporizadores correspondiente.
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    // Se carga la parte alta y baja del valor en los registros correspondientes.
    ptr_tmrG->hw_timer[timer].loadhi.val = GET_HIGH32B(cont_val);
    ptr_tmrG->hw_timer[timer].loadlo.val = GET_LOW32B(cont_val);

    // Se activa la carga del nuevo valor en el contador.
    ptr_tmrG->hw_timer[timer].load.val = 1;
}

/**
 * @brief Obtiene el valor actual del contador de un temporizador.
 *
 * Esta función lee el valor de 64 bits del contador del temporizador
 * indicado dentro del grupo correspondiente. Primero fuerza la actualización
 * de los registros internos y luego combina la parte alta y baja para
 * devolver el valor completo.
 *
 * @param grupo Grupo de temporizadores al que pertenece (TMR_GRUPO_0 o TMR_GRUPO_1).
 * @param timer Número de temporizador dentro del grupo (TIMER_0 o TIMER_1).
 * @return Valor de 64 bits que representa el conteo actual del temporizador.
 */
uint64_t timer_getCont(timer_grupo_e grupo, timer_e timer)
{
    // Se obtiene un puntero al grupo de temporizadores correspondiente.
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    // Se fuerza la actualización de los registros de conteo.
    ptr_tmrG->hw_timer[timer].update.tx_update = 1;

    // Se leen las partes baja y alta del contador.
    uint64_t low = ptr_tmrG->hw_timer[timer].lo.val;
    uint64_t high = ptr_tmrG->hw_timer[timer].hi.val;

    // Se combinan las dos partes en un valor de 64 bits y se retorna.
    return ((high << 32) | low);
}