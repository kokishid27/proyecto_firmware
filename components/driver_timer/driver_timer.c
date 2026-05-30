#include <stdio.h>
#include "driver_timer.h"

#include "soc/timer_group_reg.h"
#include "soc/timer_group_struct.h"
#include "esp_intr_alloc.h"

void timer_config(timer_grupo_e grupo, timer_e timer, bool cuenta_desc,uint16_t divisor, uint32_t alarm_val, bool autoreload)
{
    // Puntero a la estructura correspondiente segun el grupo
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    ptr_tmrG->hw_timer[timer].config.tx_en = 0;
    ptr_tmrG->hw_timer[timer].config.tx_divider = divisor;
    ptr_tmrG->hw_timer[timer].config.tx_increase = (cuenta_desc)? 0 : 1;
    ptr_tmrG->hw_timer[timer].config.tx_autoreload = (autoreload)? 1 : 0;
    ptr_tmrG->hw_timer[timer].alarmhi.val = 0;
    ptr_tmrG->hw_timer[timer].alarmlo.val = alarm_val;
    ptr_tmrG->hw_timer[timer].config.tx_alarm_en = 1;
    ptr_tmrG->hw_timer[timer].config.tx_level_int_en = 1;
}

void timer_start(timer_grupo_e grupo, timer_e timer);

void timer_stop(timer_grupo_e grupo, timer_e timer);

void timer_setINTR(timer_grupo_e grupo, timer_e timer, timer_intr_callback_t callback, void * arg);

void timer_clearINTR(void);

void timer_setCont(timer_grupo_e grupo, timer_e timer, uint64_t cont_val);

uint64_t timer_getCont(timer_grupo_e grupo, timer_e timer);