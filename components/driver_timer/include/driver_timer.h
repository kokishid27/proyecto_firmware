#ifndef DRIVER_TIMER_H
#define DRIVER_TIMER_H
/**
 * @brief Componente para la configuracion de timer
 */
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Abstraccion del grupo de temporizadores
 */
typedef enum {TMR_GRUPO_0, TMR_GRUPO_1} timer_grupo_e;

/**
 * @brief Abstraccion del temporizador
 */
typedef enum {TIMER_0, TIMER_1} timer_e;

/**
 * @brief Puntero a funcion para callback desde capa de bajo nivel
 */
typedef void (*timer_intr_callback_t)(void* arg);

/**
 * @brief Funcion de configuracion de un temporizador.
 * ESP32 tiene un total de 4 temporizadores separados en dos grupos de dos temporizadores cada uno,  
 * esta funcion permite la configuracion de alguno en especifico.
 * 
 * @param grupo Grupo de temporizadores correspondiente al temporizador que se quiere configurar
 * @param timer Temporizador especifico que se quiere configurar
 * @param cuenta_desc Argumento para seleccionar si la cuenta es descendente (true) o ascendente (false)
 * @param divisor Divisor de frecuencia aplicado al temporizador que tiene de base 80MHz
 * @param alarm_val Valor que debe alcanzar la cuenta para generar un evento
 * @param autoreload Argumento para seleccionar si el evento es ciclico (true) o se realiza una sola vez (false)
 * 
 * @attention Para configurar y habilitar la interrupcion del temporizador se debe hacer
 * con la funcion: timer_setINTR()
 */
void timer_config(timer_grupo_e grupo, timer_e timer, bool cuenta_desc,uint16_t divisor, uint32_t alarm_val, bool autoreload);

/**
 * @brief Funcion para iniciar la cuenta del temporizador
 * 
 * @note El temporizador debe haber sido previamente configurado con: timer_config()
 * 
 * @param grupo Grupo al que pertenece el temporizador que se quiere iniciar
 * @param timer Temporizador especifico que se quiere iniciar
 */
void timer_start(timer_grupo_e grupo, timer_e timer);

/**
 * @brief Funcion para detener la cuenta del temporizador
 * 
 * @note El temporizador debe haber sido previamente configurador con: timer_config()
 * 
 * @param grupo Grupo al que pertenece el temporizador que se quiere iniciar
 * @param timer Temporizador especifico que se quiere iniciar
 */
void timer_stop(timer_grupo_e grupo, timer_e timer);

/**
 * @brief Configura y habilita la interrupción por hardware del temporizador seleccionado.
 * 
 * Enlaza un callback (puntero a función) que se ejecutará de forma automática desde 
 * la Rutina de Servicio de Interrupción (ISR) cuando el contador del temporizador 
 * alcance el valor de la alarma configurada.
 * 
 * @param grupo Grupo de temporizadores al que pertenece el periférico (TMR_GRUPO_0 o TMR_GRUPO_1).
 * @param timer Temporizador específico dentro del grupo seleccionado (TIMER_0 o TIMER_1).
 * @param callback Puntero a la función (capa superior) que se desea ejecutar al dispararse la alarma.
 * @param arg Puntero genérico opcional con argumentos que se pasarán directamente al callback al ser invocado.
 */
void timer_setINTR(timer_grupo_e grupo, timer_e timer, timer_intr_callback_t callback, void * arg);

/**
 * @brief Limpia la bandera de interrupción por hardware del temporizador seleccionado.
 * 
 * Esta función borra el bit de estado de interrupción (registro INT_CLR) en el periférico. 
 * @note Debe ser llamada de forma estricta dentro de la función ISR (Rutina de 
 * Servicio de Interrupción) para evitar que el microcontrolador se quede atrapado reingresando 
 * al bucle de interrupción indefinidamente.
 * 
 * @param grupo Grupo de temporizadores al que pertenece el periférico (TMR_GRUPO_0 o TMR_GRUPO_1).
 * @param timer Temporizador específico dentro del grupo seleccionado (TIMER_0 o TIMER_1).
 */
void timer_clearINTR(timer_grupo_e grupo, timer_e timer);

/**
 * @brief Fuerza al contador del temporizador seleccionado a iniciar en un valor específico.
 * 
 * Esta función escribe de forma directa en los registros de recarga de hardware (TxLOADLO y TxLOADHI) 
 * y activa el bit de trigger para cargar el valor de forma inmediata. Se utiliza comúnmente para 
 * resetear el contador a cero (0) durante la inicialización o para desfasar el conteo.
 * 
 * @param grupo Grupo de temporizadores al que pertenece el periférico (TMR_GRUPO_0 o TMR_GRUPO_1).
 * @param timer Temporizador específico dentro del grupo seleccionado (TIMER_0 o TIMER_1).
 * @param cont_val Valor de 64 bits que se cargará físicamente en el contador del hardware.
 */
void timer_setCont(timer_grupo_e grupo, timer_e timer, uint64_t cont_val);

/**
 * @brief Obtiene el valor actual del contador del temporizador en tiempo real.
 * 
 * Esta función lee el valor del contador en los registros de lectura forzando una actualizacion sin detener
 * el conteo físico. Luego, une la parte baja (TxLO) y alta (TxHI) en una sola variable de 64 bits.
 * 
 * @param grupo Grupo de temporizadores al que pertenece el periférico (TMR_GRUPO_0 o TMR_GRUPO_1).
 * @param timer Temporizador específico dentro del grupo seleccionado (TIMER_0 o TIMER_1).
 * @return uint64_t El valor numérico de 64 bits del contador en el instante de la lectura (ticks de reloj).
 */
uint64_t timer_getCont(timer_grupo_e grupo, timer_e timer);
#endif