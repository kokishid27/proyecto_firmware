#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"

// Inclusión de la capa de la placa para manejar LEDs y Botones
#include "bsp_board.h"
// Inclusión de la capa de abstracción para manejar Temporizadores
#include "hal_board.h" 

/* --- VARIABLES GLOBALES VOLÁTILES (Banderas de comunicación) --- */
volatile bool flag_btn1 = false;
volatile bool flag_timer0 = false;
int contador_btn1 = 0; // Lleva la secuencia de colores para el LED RGB

// Arreglo para manipular los LEDs del 1 al 4 mediante bucles
bsp_led_t leds[4] = {BSP_LED_1, BSP_LED_2, BSP_LED_3, BSP_LED_4}; 

/* --- RUTINAS DE SERVICIO DE INTERRUPCIÓN (ISR) --- */

// Función que responde a la alarma del Temporizador
static void IRAM_ATTR isr_timer0(void *arg) 
{
    // Limpio el aviso en el hardware para que pueda volver a sonar
    hal_timer_clear_intr(HAL_TIMER_0);
    
    // Aviso al programa principal que ocurrió el evento
    flag_timer0 = true; 
}

// Función que responde al presionar el Botón 1
static void IRAM_ATTR isr_btn1(void *arg) 
{
    static uint32_t ultimo_disparo = 0; 
    uint32_t tiempo_actual = xTaskGetTickCountFromISR();

    // Filtro antideslizante: Solo acepto el clic si pasaron 600ms desde el anterior
    if ((tiempo_actual - ultimo_disparo) > pdMS_TO_TICKS(600)) {
        if (!flag_btn1) flag_btn1 = true;
        ultimo_disparo = tiempo_actual;
    }
}

/* --- FUNCIÓN PRINCIPAL --- */
void app_main(void) 
{
    printf("--- Iniciando Firmware --\n");

    /* --- FASE 1: INICIALIZACIÓN DE LA PLACA (BSP) --- */
    if (!bsp_board_init()) {
        printf("Error critico: Fallo al inicializar la placa base.\n");
        return; 
    }
    
    // Enciendo el LED principal fijo para confirmar que el arranque fue exitoso
    bsp_led_set(BSP_LED_SISTEMA, true);
    
    // Conecto el aviso del Botón 1 con mi función de respuesta
    bsp_button_register_callback(BSP_BUTTON_1, isr_btn1, NULL);

    /* --- FASE 2: INICIALIZACIÓN DEL TEMPORIZADOR (HAL) --- */
    hal_timer_cfg_t timer_cfg = {
        .id = HAL_TIMER_0,                  // Usaré el Temporizador 0 del Grupo 0
        .count_down = false,                // Cuenta hacia adelante
        .divisor = 80,                      // Frecuencia ajustada para que 1 paso = 1 microsegundo
        .alarm_val = 500000,                // La alarma sonará a los 500,000 microsegundos (500 ms)
        .autoreload = true,                 // Vuelve a contar desde cero automáticamente
        .alarm_en = true,                   // Enciendo el sistema de alarmas
        .callback = isr_timer0,             // Conecto la función que responderá a la alarma
        .int_type = HAL_TIMER_INT_LEVEL,    // Tipo de señal eléctrica requerida por el ESP32
        .cb_arg = NULL                      // No necesito pasarle datos extra
    };

    if (hal_timer_init(&timer_cfg) != HAL_OK) {
        printf("Error critico: Fallo al inicializar Timer 0.\n");
        return;
    }

    // Aseguro que el contador arranque desde cero limpio
    hal_timer_set_count(HAL_TIMER_0, 0);
    // Enciendo el motor del temporizador
    hal_timer_start(HAL_TIMER_0);

    printf("Hardware inicializado. Timer corriendo. Esperando eventos...\n");

    /* --- FASE 3: BUCLE PRINCIPAL (Super-loop) --- */
    while (1) 
    {
        // 1. Tarea del Temporizador (Medio segundo)
        if (flag_timer0) {
            // Cambio el estado del LED para crear un parpadeo
            bsp_led_toggle(BSP_LED_SISTEMA);
            flag_timer0 = false; // Bajo la bandera hasta el próximo aviso
        }

        // 2. Tarea del Botón 1 (Eventos de interrupción)
        if (flag_btn1) {
            printf("Evento atrapado: Boton 1. Alternando LED RGB.\n");
            
            // Alterno entre Rojo, Verde y Azul dependiendo de cuántas veces presioné
            if(contador_btn1 == 0) {
                contador_btn1++;
                bsp_led_rgb_set(true, false, false); // Rojo
            }
            else if(contador_btn1 == 1) {
                contador_btn1++;
                bsp_led_rgb_set(false, true, false); // Verde
            }
            else if(contador_btn1 == 2) {
                bsp_led_rgb_set(false, false, true); // Azul
                contador_btn1 = 0; // Reinicio la secuencia
            }
            
            flag_btn1 = false; // Bajo la bandera para esperar el siguiente clic
        }

        // 3. Tarea del Botón 2 (Lectura manual por sondeo)
        if (bsp_button_is_pressed(BSP_BUTTON_2)) {
            printf("Evento atrapado: Boton 2\n");
            
            // Animación de barrido: Enciendo los LEDs uno por uno
            for(int i = 0; i < 4; i++) {
                 bsp_led_set(leds[i], true);
                 vTaskDelay(pdMS_TO_TICKS(300)); // Espera visual
            }
            // Animación de barrido: Apago los LEDs uno por uno
             for(int i = 0; i < 4; i++) {
                 bsp_led_set(leds[i], false);
                 vTaskDelay(pdMS_TO_TICKS(300));
             } 
        }

        // 4. Descanso del sistema
        // Obligatorio para que FreeRTOS pueda gestionar la placa sin colapsar
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}