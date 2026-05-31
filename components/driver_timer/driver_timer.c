#include <stdio.h>
#include "driver_timer.h"

#include "soc/timer_group_reg.h"
#include "soc/timer_group_struct.h"
#include "esp_intr_alloc.h"

#define GET_HIGH32B(x) ((uint32_t)(x >> 32))
#define GET_LOW32B(x)  ((uint32_t)(x & 0xFFFFFFFFULL))

void timer_config(timer_grupo_e grupo, timer_e timer, bool cuenta_desc, uint16_t divisor, uint32_t alarm_val, bool autoreload)
{
    // Puntero a la estructura correspondiente segun el grupo
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    ptr_tmrG->hw_timer[timer].config.tx_en = 0;
    ptr_tmrG->hw_timer[timer].config.tx_divider = divisor;
    ptr_tmrG->hw_timer[timer].config.tx_increase = (cuenta_desc)? 0 : 1;
    ptr_tmrG->hw_timer[timer].config.tx_autoreload = (autoreload)? 1 : 0;
    ptr_tmrG->hw_timer[timer].alarmhi.val = GET_HIGH32B(alarm_val);
    ptr_tmrG->hw_timer[timer].alarmlo.val = GET_LOW32B(alarm_val);
    ptr_tmrG->hw_timer[timer].config.tx_alarm_en = 1;
    ptr_tmrG->hw_timer[timer].config.tx_level_int_en = 1;
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

void timer_setINTR(timer_grupo_e grupo, timer_e timer, timer_intr_callback_t callback, void * arg)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    if (timer == TIMER_0) ptr_tmrG->int_ena_timers.t0_int_ena = 1;
    else ptr_tmrG->int_ena_timers.t1_int_ena = 1;

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