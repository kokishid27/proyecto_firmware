/**
 * @file driver_timer.c
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
    // Puntero a la estructura correspondiente segun el grupo
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    ptr_tmrG->hw_timer[timer].config.tx_en = 0;
    ptr_tmrG->hw_timer[timer].config.tx_divider = divisor;
    ptr_tmrG->hw_timer[timer].config.tx_increase = (cuenta_desc)? 0 : 1;
    ptr_tmrG->hw_timer[timer].config.tx_autoreload = (autoreload)? 1 : 0;
    ptr_tmrG->hw_timer[timer].alarmhi.val = GET_HIGH32B(alarm_val);
    ptr_tmrG->hw_timer[timer].alarmlo.val = GET_LOW32B(alarm_val);
    ptr_tmrG->hw_timer[timer].config.tx_alarm_en = (alarm_en)? 1 : 0;
}

void timer_start(timer_grupo_e grupo, timer_e timer)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    ptr_tmrG->hw_timer[timer].config.tx_en = 1;
}

void timer_stop(timer_grupo_e grupo, timer_e timer)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    ptr_tmrG->hw_timer[timer].config.tx_en = 0;
}

void timer_setINTR(timer_grupo_e grupo, timer_e timer, tx_int_type_e int_type, timer_intr_callback_t callback, void * arg)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    if (timer == TIMER_0) ptr_tmrG->int_ena_timers.t0_int_ena = 1;
    else ptr_tmrG->int_ena_timers.t1_int_ena = 1;
    
    if (int_type == TMR_LEVEL_INT) {
        ptr_tmrG->hw_timer[timer].config.tx_level_int_en = 1;
        ptr_tmrG->hw_timer[timer].config.tx_edge_int_en = 0;
    }
    else {
        ptr_tmrG->hw_timer[timer].config.tx_level_int_en = 0;
        ptr_tmrG->hw_timer[timer].config.tx_edge_int_en = 1;
    }

    int scr_intr = (grupo == TMR_GRUPO_0)? 
                    ((timer == TIMER_0)? ETS_TG0_T0_LEVEL_INTR_SOURCE : ETS_TG0_T1_LEVEL_INTR_SOURCE) :
                    ((timer == TIMER_0)? ETS_TG1_T0_LEVEL_INTR_SOURCE : ETS_TG1_T1_LEVEL_INTR_SOURCE);
    
    esp_intr_alloc(
        scr_intr,
        ESP_INTR_FLAG_IRAM,
        (intr_handler_t)callback,
        arg,
        NULL
    );
}

void timer_clearINTR(timer_grupo_e grupo, timer_e timer)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    if (timer == TIMER_0) ptr_tmrG->int_clr_timers.t0_int_clr = 1;
    else ptr_tmrG->int_clr_timers.t1_int_clr = 1;

    ptr_tmrG->hw_timer[timer].config.tx_alarm_en = 1;
}

void timer_setCont(timer_grupo_e grupo, timer_e timer, uint64_t cont_val)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    ptr_tmrG->hw_timer[timer].loadhi.val = GET_HIGH32B(cont_val);
    ptr_tmrG->hw_timer[timer].loadlo.val = GET_LOW32B(cont_val);
    ptr_tmrG->hw_timer[timer].load.val = 1;
}

uint64_t timer_getCont(timer_grupo_e grupo, timer_e timer)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    ptr_tmrG->hw_timer[timer].update.tx_update = 1; // forzar actualizacion
    uint64_t low = ptr_tmrG->hw_timer[timer].lo.val;
    uint64_t high = ptr_tmrG->hw_timer[timer].hi.val;

    return ((high << 32) | low);
}