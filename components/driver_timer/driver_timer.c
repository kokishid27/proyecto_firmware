#include <stdio.h>
#include "driver_timer.h"

#include "soc/timer_group_reg.h"
#include "soc/timer_group_struct.h"
#include "esp_intr_alloc.h"

/* Macros para dividir un valor de 64 bits en dos partes de 32 bits */
#define GET_HIGH32B(x) ((uint32_t)(x >> 32))
#define GET_LOW32B(x)  ((uint32_t)(x & 0xFFFFFFFFULL))

/**
 * @brief Configura las condiciones iniciales de un temporizador específico.
 *
 * Esta función se encarga de definir cómo se comportará el temporizador seleccionado.
 * Establece si la cuenta va hacia arriba o hacia abajo, la velocidad del conteo mediante
 * el divisor, el valor límite para activar la alarma y si debe reiniciarse automáticamente.
 *
 * @param grupo       Grupo de temporizadores al que pertenece (Grupo 0 o Grupo 1).
 * @param timer       El temporizador específico dentro del grupo (Temporizador 0 o 1).
 * @param cuenta_desc Define si la cuenta es descendente (true) o ascendente (false).
 * @param divisor     Valor numérico para ajustar la velocidad del reloj del temporizador.
 * @param alarm_val   Valor del contador en el cual se activará la alarma.
 * @param autoreload  Define si el contador vuelve a empezar automáticamente tras la alarma.
 * @param alarm_en    Activa o desactiva la funcionalidad de la alarma.
 */
void timer_config(timer_grupo_e grupo, timer_e timer, bool cuenta_desc, uint16_t divisor, uint64_t alarm_val, bool autoreload, bool alarm_en)
{
    // Selecciono la base de datos de registros según el grupo de hardware elegido
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;

    // Desactivo temporalmente el temporizador para aplicar los cambios de forma segura
    ptr_tmrG->hw_timer[timer].config.tx_en = 0;
    
    // Configuro la velocidad del reloj interno del temporizador
    ptr_tmrG->hw_timer[timer].config.tx_divider = divisor;
    
    // Configuro el sentido de la cuenta: hacia atrás (0) o hacia adelante (1)
    ptr_tmrG->hw_timer[timer].config.tx_increase = (cuenta_desc)? 0 : 1;
    
    // Configuro si el temporizador se reinicia solo al alcanzar la alarma
    ptr_tmrG->hw_timer[timer].config.tx_autoreload = (autoreload)? 1 : 0;
    
    // Cargo el valor límite de la alarma dividiendo el dato de 64 bits en dos registros de 32 bits
    ptr_tmrG->hw_timer[timer].alarmhi.val = GET_HIGH32B(alarm_val);
    ptr_tmrG->hw_timer[timer].alarmlo.val = GET_LOW32B(alarm_val);
    
    // Habilito o deshabilito el sistema de alarmas según se requiera
    ptr_tmrG->hw_timer[timer].config.tx_alarm_en = (alarm_en)? 1 : 0;
}

/**
 * @brief Inicia el conteo del temporizador seleccionado.
 *
 * Permite que el temporizador comience a incrementar o decrementar su contador interno
 * a partir del valor en el que se encuentre actualmente.
 *
 * @param grupo Grupo de temporizadores al que pertenece.
 * @param timer El temporizador específico que se va a encender.
 */
void timer_start(timer_grupo_e grupo, timer_e timer)
{
    // Obtengo la dirección del grupo de temporizadores correspondiente
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    
    // Enciendo el bit de habilitación para que empiece a contar
    ptr_tmrG->hw_timer[timer].config.tx_en = 1;
}

/**
 * @brief Detiene el conteo del temporizador seleccionado.
 *
 * Pausa el contador interno del temporizador inmediatamente, congelando su valor actual.
 *
 * @param grupo Grupo de temporizadores al que pertenece.
 * @param timer El temporizador específico que se va a apagar.
 */
void timer_stop(timer_grupo_e grupo, timer_e timer)
{
    // Obtengo la dirección del grupo de temporizadores correspondiente
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    
    // Apago el bit de habilitación para congelar la cuenta
    ptr_tmrG->hw_timer[timer].config.tx_en = 0;
}

/**
 * @brief Configura y enlaza el aviso por interrupción del temporizador.
 *
 * Prepara el sistema para recibir un aviso del hardware cuando ocurra un evento en el temporizador.
 * Configura la forma del aviso (por nivel o borde), activa los permisos del sistema y enlaza
 * una función propia para que responda inmediatamente cuando ocurra la alarma.
 *
 * @param grupo    Grupo de temporizadores al que pertenece.
 * @param timer    El temporizador específico que generará el aviso.
 * @param int_type Tipo de aviso eléctrico (por nivel o por borde).
 * @param callback Función propia que se ejecutará de forma automática al activarse el aviso.
 * @param arg      Puntero opcional con datos que se le quieran pasar a la función de respuesta.
 */
void timer_setINTR(timer_grupo_e grupo, timer_e timer, tx_int_type_e int_type, timer_intr_callback_t callback, void * arg)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    
    // Activo el permiso del aviso de forma individual según el temporizador usado
    if (timer == TIMER_0) ptr_tmrG->int_ena_timers.t0_int_ena = 1;
    else ptr_tmrG->int_ena_timers.t1_int_ena = 1;
    
    // Configuro el comportamiento de la señal eléctrica del aviso
    if (int_type == TMR_LEVEL_INT) {
        ptr_tmrG->hw_timer[timer].config.tx_level_int_en = 1;
        ptr_tmrG->hw_timer[timer].config.tx_edge_int_en = 0;
    }
    else {
        ptr_tmrG->hw_timer[timer].config.tx_level_int_en = 0;
        ptr_tmrG->hw_timer[timer].config.tx_edge_int_en = 1;
    }

    // Identifico cuál es el canal o ruta de aviso físico interno que usará este temporizador
    int scr_intr = (grupo == TMR_GRUPO_0)? 
                    ((timer == TIMER_0)? ETS_TG0_T0_LEVEL_INTR_SOURCE : ETS_TG0_T1_LEVEL_INTR_SOURCE) :
                    ((timer == TIMER_0)? ETS_TG1_T0_LEVEL_INTR_SOURCE : ETS_TG1_T1_LEVEL_INTR_SOURCE);
    
    // Enlazo la señal del hardware con nuestra función de respuesta en memoria segura (IRAM)
    esp_intr_alloc(
        scr_intr,
        ESP_INTR_FLAG_IRAM,
        (intr_handler_t)callback,
        arg,
        NULL
    );
}

/**
 * @brief Limpia la señal de aviso y vuelve a activar la alarma.
 *
 * Esta función se debe ejecutar obligatoriamente dentro de la función de respuesta del aviso.
 * Borra la bandera de la alarma que ya ocurrió y vuelve a armar el sistema para un próximo aviso.
 *
 * @param grupo Grupo de temporizadores al que pertenece.
 * @param timer El temporizador específico que se va a limpiar.
 */
void timer_clearINTR(timer_grupo_e grupo, timer_e timer)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    
    // Borro el aviso que se quedó guardado en el registro del hardware
    if (timer == TIMER_0) ptr_tmrG->int_clr_timers.t0_int_clr = 1;
    else ptr_tmrG->int_clr_timers.t1_int_clr = 1;

    // Reactivo la alarma del temporizador para que pueda volver a sonar en el futuro
    ptr_tmrG->hw_timer[timer].config.tx_alarm_en = 1;
}

/**
 * @brief Fuerza al contador del temporizador a tomar un valor determinado.
 *
 * Cambia el número de la cuenta actual por cualquier valor que decidamos pasarle,
 * obligando al hardware a actualizarse inmediatamente.
 *
 * @param grupo    Grupo de temporizadores al que pertenece.
 * @param timer    El temporizador específico que se va a modificar.
 * @param cont_val Nuevo valor de 64 bits que tomará el contador.
 */
void timer_setCont(timer_grupo_e grupo, timer_e timer, uint64_t cont_val)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    
    // Divido el valor completo en dos partes de 32 bits y las guardo en los registros de carga
    ptr_tmrG->hw_timer[timer].loadhi.val = GET_HIGH32B(cont_val);
    ptr_tmrG->hw_timer[timer].loadlo.val = GET_LOW32B(cont_val);
    
    // Envío la orden física para que el hardware reemplace la cuenta vieja por este nuevo valor
    ptr_tmrG->hw_timer[timer].load.val = 1;
}

/**
 * @brief Lee y entrega el valor actual del contador del temporizador.
 *
 * Captura de forma segura el valor en tiempo real del conteo y lo devuelve unido
 * en un único formato de 64 bits.
 *
 * @param grupo Grupo de temporizadores al que pertenece.
 * @param timer El temporizador específico que se desea leer.
 * @return uint64_t El número actual de la cuenta del temporizador.
 */
uint64_t timer_getCont(timer_grupo_e grupo, timer_e timer)
{
    timg_dev_t *ptr_tmrG = (grupo == TMR_GRUPO_0)? &TIMERG0 : &TIMERG1;
    
    // Ordeno al hardware que congele y copie el valor actual de la cuenta para poder leerlo sin errores
    ptr_tmrG->hw_timer[timer].update.tx_update = 1; 
    
    // Leo la parte baja y alta de forma independiente desde los registros de captura
    uint64_t low = ptr_tmrG->hw_timer[timer].lo.val;
    uint64_t high = ptr_tmrG->hw_timer[timer].hi.val;

    // Uno ambas partes desplazando los bits de arriba y aplicando una operación OR para reconstruir los 64 bits
    return ((high << 32) | low);
}