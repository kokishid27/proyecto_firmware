#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"

// Inclusión del BSP para el hardware de la placa (LEDs, Botones)
#include "bsp_board.h"
// Inclusión del HAL para los periféricos internos del chip (Timers)
#include "hal_board.h" 

/* --- 1. VARIABLES GLOBALES VOLÁTILES (Banderas para ISR) --- */
volatile bool flag_btn1 = false;
volatile bool flag_timer0 = false;


bsp_led_t leds[4] = {BSP_LED_1, BSP_LED_2, BSP_LED_3, BSP_LED_4}; // Arreglo de LEDs para iterar fácilmente

/* --- 2. RUTINAS DE SERVICIO DE INTERRUPCIÓN (ISR) --- */

// --- ISR para el Timer de Hardware ---
static void IRAM_ATTR isr_timer0(void *arg) 
{
    // 1. Limpiamos la bandera física del registro del Timer para que no haga "Panic"
    hal_timer_clear_intr(HAL_TIMER_0);
    
    // 2. Levantamos la bandera de software para la aplicación principal
    flag_timer0 = true; 
}

// ISR para el Botón 1 
static void IRAM_ATTR isr_btn1(void *arg) 
{
    static uint32_t ultimo_disparo = 0; 
    uint32_t tiempo_actual = xTaskGetTickCountFromISR();

    if ((tiempo_actual - ultimo_disparo) > pdMS_TO_TICKS(600)) {
        if (!flag_btn1) flag_btn1 = true;
        ultimo_disparo = tiempo_actual;
    }
}


/* --- 3. FUNCIÓN PRINCIPAL --- */
void app_main(void) 
{
    printf("--- Iniciando Firmware --\n");

    /* --- A. INICIALIZACIÓN DEL BSP (Placa Base) --- */
    if (!bsp_board_init()) {
        printf("Error critico: Fallo al inicializar la placa base.\n");
        return; 
    }
    
    // Encender el LED de sistema para indicar que el microcontrolador arrancó bien
    bsp_led_set(BSP_LED_SISTEMA, true);
    
    //state_old = bsp_button_is_pressed(BSP_BUTTON_1);
    // Registrar los callbacks de los botones en el BSP
    bsp_button_register_callback(BSP_BUTTON_1, isr_btn1, NULL);


    /* --- B. INICIALIZACIÓN DEL HAL TIMER --- */
    hal_timer_cfg_t timer_cfg = {
        .id = HAL_TIMER_0,                  // Grupo 0, Timer 0
        .count_down = false,                // Cuenta ascendente
        .divisor = 80,                      // 80MHz / 80 = 1MHz (1 tick = 1 us)
        .alarm_val = 500000ULL,             // Alarma cada 500,000 us (500 ms)
        .autoreload = true,                 // Reiniciar el contador al llegar a la alarma
        .alarm_en = true,                   // Activar disparo de alarma
        .callback = isr_timer0,             // Nuestra ISR registrada arriba
        .int_type = HAL_TIMER_INT_LEVEL,    // Interrupción por nivel (estándar en ESP32)
        .cb_arg = NULL                      // Sin argumentos adicionales
    };

    if (hal_timer_init(&timer_cfg) != HAL_OK) {
        printf("Error critico: Fallo al inicializar Timer 0.\n");
        return;
    }

    // Por seguridad, forzamos el contador a cero antes de arrancar
    hal_timer_set_count(HAL_TIMER_0, 0);
    // ¡Arrancamos el motor de tiempo!
    hal_timer_start(HAL_TIMER_0);

    printf("Hardware inicializado. Timer corriendo. Esperando eventos...\n");

    /* --- 4. BUCLE PRINCIPAL (Super-loop) --- */
    while (1) 
    {
        // 1. Procesar evento del Timer (Ocurrirá rigurosamente cada 500ms)
        if (flag_timer0) {
            bsp_led_toggle(BSP_LED_SISTEMA);
            flag_timer0 = false;
        }

        // 2. Procesar evento asíncrono del Botón 1
        if (flag_btn1) {
            printf("Evento atrapado: Boton 1. Alternando LED Sistema.\n");
            
            bsp_led_toggle(BSP_LED_2);
            //bsp_led_rgb_set(true, true, true); // Enciende el rojo del RGB
            flag_btn1 = false; 
        }

        // 3. Procesar evento asíncrono del Botón 2
        if (bsp_button_is_pressed(BSP_BUTTON_2)) {
            printf("Evento atrapado: Boton 2");
            for(int i = 0; i < 4; i++) {
                 bsp_led_set(leds[i], true);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
             for(int i = 0; i < 4; i++) {
                bsp_led_set(leds[i], false);
                vTaskDelay(pdMS_TO_TICKS(100));
             } 
            
        }

        // Ceder control a FreeRTOS por 10ms (Protección del Watchdog Timer)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}