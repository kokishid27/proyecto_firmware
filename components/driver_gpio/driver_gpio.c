#include <stdio.h>
#include "driver_gpio.h"

// Arreglo de IO_MUX para los pines disponibles
static volatile uint32_t *IO_MUX_MAP[40] = {
    GPIO_IO_MUX_0,   // 0
    NULL,            // 1
    GPIO_IO_MUX_2,   // 2
    NULL,            // 3
    GPIO_IO_MUX_4,   // 4
    GPIO_IO_MUX_5,   // 5
    NULL, NULL, NULL, NULL, NULL, // 6-10
    NULL,            // 11
    GPIO_IO_MUX_12,  // 12
    GPIO_IO_MUX_13,  // 13
    GPIO_IO_MUX_14,  // 14
    NULL,            // 15
    GPIO_IO_MUX_16,  // 16
    GPIO_IO_MUX_17,  // 17
    GPIO_IO_MUX_18,  // 18
    GPIO_IO_MUX_19,  // 19
    GPIO_IO_MUX_20,  // 20
    GPIO_IO_MUX_21,  // 21
    GPIO_IO_MUX_22,  // 22
    GPIO_IO_MUX_23,  // 23
    GPIO_IO_MUX_24,  // 24
    GPIO_IO_MUX_25,  // 25
    GPIO_IO_MUX_26,  // 26
    GPIO_IO_MUX_27,  // 27
    NULL, NULL, NULL, // 28-30
    NULL,            // 31
    GPIO_IO_MUX_32,  // 32
    GPIO_IO_MUX_33,  // 33
    GPIO_IO_MUX_34,  // 34
    GPIO_IO_MUX_35,  // 35
    GPIO_IO_MUX_36,  // 36
    GPIO_IO_MUX_37,  // 37
    GPIO_IO_MUX_38,  // 38
    GPIO_IO_MUX_39   // 39
};

// Configuracion de PIN como salida, inicializa valor en LOW
void gpio_config_out(uint8_t pin)
{
	if (pin <= 31) {
		GPIO_ENABLE		|= (1 << pin);
		GPIO_OUT_W1TC	|= (1 << pin);	// estado inicial LOW/0
	}
	
	else if (pin >= 32 && pin <= 39) {
		GPIO_ENABLE1	|= (1 << (pin - 32));
		GPIO_OUT1_W1TC	|= (1 << (pin - 32)); // estado inicial LOW
	}
}

// Configuracion de PIN como entrada
// pull_mode = 0: sin pull, 1: pull-up, 2: pull-down
void gpio_config_in(uint8_t pin, pull_mode_e pull_mode)
{	
	if (pin > 39 || IO_MUX_MAP[pin] == NULL) return;
	
	GPIO_ENABLE	&= ~(1 << pin);	// deshabilitamos salida en PIN
	
	*IO_MUX_MAP[pin] &= ~MCU_SEL_MASK;
	*IO_MUX_MAP[pin] |= MCU_SEL_GPIO;
	*IO_MUX_MAP[pin] &= ~(PULL_WPU | PULL_WPD); // limpiar valores previos
	*IO_MUX_MAP[pin] |= FUN_IE;	// habilitamos como entrada
	if (pull_mode == PULLUP) 		*IO_MUX_MAP[pin] |= PULL_WPU; // pull-up
	else if (pull_mode == PULLDOWN) *IO_MUX_MAP[pin] |= PULL_WPD; // pull-down
}

// Funcion de lectura de PIN
bool gpio_read(uint8_t pin)
{
	if (pin <= 31) return (GPIO_IN & (1 << pin))? true : false;
	else if (pin >= 32 && pin <=39) return (GPIO_IN1 & (1 << (pin - 32)))? true : false;
	else return false; 
}

// Funcion de escritura en PIN
void gpio_write(uint8_t pin, uint8_t value)
{	
	if (pin <= 31) {
		if (value) GPIO_OUT_W1TS	|= (1 << pin); // set 1
		else GPIO_OUT_W1TC			|= (1 << pin); // clear 0
	}
	
	else if (pin >= 32 && pin <= 39) {
		if (value) GPIO_OUT1_W1TS	|= (1 << (pin - 32)); // set 1
		else GPIO_OUT1_W1TC			|= (1 << (pin - 32)); // clear 0
	}
}
