#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "driver_timer.h"

#define HWREG32VAL(x)	(*((volatile uint32_t *)(x)))
#define HWREG32(x)		((volatile uint32_t *)(x))

/**
 * @brief Mapeo de registros necesarios para la configuracion de gpio
 		  como salida y como entrada y el pull mode
 */
#define GPIO_OUT		(HWREG32VAL(0x3FF44004))	// GPIO 0-31	OUTPUT REG VALUE
#define GPIO_OUT1		(HWREG32VAL(0x3FF44010))	// GPIO 32-39	OUTPUT REG VALUE
#define GPIO_ENABLE		(HWREG32VAL(0x3FF44020))	// GPIO 0-31	OUTPUT ENABLE REG
#define GPIO_ENABLE1	(HWREG32VAL(0x3FF4402C))	// GPIO 32-39	OUTPUT ENABLE REG
#define GPIO_OUT_W1TS	(HWREG32VAL(0x3FF44008))
#define GPIO_OUT_W1TC	(HWREG32VAL(0x3FF4400C))
#define GPIO_OUT1_W1TS	(HWREG32VAL(0x3FF44014))
#define GPIO_OUT1_W1TC	(HWREG32VAL(0x3FF44018))

#define GPIO_IN			(HWREG32VAL(0x3FF4403C))	// GPIO 0-31	INPUT REG
#define GPIO_IN1		(HWREG32VAL(0x3FF44040))	// GPIO 32-39	INPUT REG

#define GPIO_STATUS		(HWREG32VAL(0x3FF44044))	// GPIO 0-31  INTR STATUS REG
#define GPIO_STATUS1	(HWREG32VAL(0x3FF44050))	// GPIO 32-39 INTR STATUS REG
#define GPIO_PIN18_REG	(HWREG32VAL(0x3FF440D0))
#define GPIO_PIN19_REG	(HWREG32VAL(0x3FF440D4))

#define GPIO_IO_MUX_36	(HWREG32(0x3FF49004))
#define GPIO_IO_MUX_37	(HWREG32(0x3FF49008))
#define GPIO_IO_MUX_38	(HWREG32(0x3FF4900C))
#define GPIO_IO_MUX_39	(HWREG32(0x3FF49010))
#define GPIO_IO_MUX_34	(HWREG32(0x3FF49014))
#define GPIO_IO_MUX_35	(HWREG32(0x3FF49018))
#define GPIO_IO_MUX_32	(HWREG32(0x3FF4901C))
#define GPIO_IO_MUX_33	(HWREG32(0x3FF49020))
#define GPIO_IO_MUX_25	(HWREG32(0x3FF49024))
#define GPIO_IO_MUX_26	(HWREG32(0x3FF49028))
#define GPIO_IO_MUX_27	(HWREG32(0x3FF4902C))
#define GPIO_IO_MUX_2	(HWREG32(0x3FF49040))
#define GPIO_IO_MUX_0	(HWREG32(0x3FF49044))
#define GPIO_IO_MUX_4	(HWREG32(0x3FF49048))
#define GPIO_IO_MUX_16	(HWREG32(0x3FF4904C))
#define GPIO_IO_MUX_17	(HWREG32(0x3FF49050))
#define GPIO_IO_MUX_5	(HWREG32(0x3FF4906C))
#define GPIO_IO_MUX_18	(HWREG32(0x3FF49070))
#define GPIO_IO_MUX_19	(HWREG32(0x3FF49074))
#define GPIO_IO_MUX_20	(HWREG32(0x3FF49078))
#define GPIO_IO_MUX_21	(HWREG32(0x3FF4907C))
#define GPIO_IO_MUX_22	(HWREG32(0x3FF49080))
#define GPIO_IO_MUX_23	(HWREG32(0x3FF4908C))
#define GPIO_IO_MUX_24	(HWREG32(0x3FF49090))
#define GPIO_IO_MUX_12	(HWREG32(0x3FF49034))
#define GPIO_IO_MUX_13	(HWREG32(0x3FF49038))
#define GPIO_IO_MUX_14	(HWREG32(0x3FF49030))

#define PULL_WPU		(1 << 8) // pull-up enable
#define PULL_WPD		(1 << 7) // pull-down enable
#define FUN_IE			(1 << 9)
#define FUN_PU			(1 << 7)
#define MCU_SEL_MASK	((1 << 12) | (1 << 13) | (1 << 14))
#define MCU_SEL_GPIO	(1 << 13) // 010 -> 2: Function 2 GPIO

/**
 * @brief Enumeracion de abstraccion de numero de pin
 */
typedef enum {
    PIN0 = 0, PIN1, PIN2, PIN3, PIN4, PIN5, PIN6, PIN7, PIN8, PIN9,
    PIN10, PIN11, PIN12, PIN13, PIN14, PIN15, PIN16, PIN17, PIN18, PIN19,
    PIN20, PIN21, PIN22, PIN23, PIN24, PIN25, PIN26, PIN27, PIN28, PIN29,
    PIN30, PIN31, PIN32, PIN33, PIN34, PIN35, PIN36, PIN37, PIN38, PIN39
} pin_num_e;

/**
 * @brief Enumeracion de abstraccion de pull mode
 */
typedef enum {
	PULL_DISABLE = 0,
	PULLUP,
	PULLDOWN
} pull_mode_e;

//////////////////////////////
//	Prototipos de funciones	//
//////////////////////////////

/**
 * @brief Funcion de configuracion de PIN como salida GPIO
 * 
 * @param pin Pin que se quiere configurar como salida GPIO 
 */
void gpio_config_out(uint8_t pin);

/**
 * @brief Funcion de configuracion de PIN especifico como entrada
 		  digital y define su resistencia interna (PULL-DOWN o PULL-UP)
 * 
 * @param pin Numero del pin GPIO que se desea configurar
 * @param pull_mode Modo de resistencia interna (Pull-up, Pull-down o Pull-disable)
 */
void gpio_config_in(uint8_t pin, pull_mode_e pull_mode);

/**
 * @brief Funcion de lectura logica actual de un pin configurado como salida
 * @pre El numero de pin debe haber sido configurado como entrada
 * 
 * @param pin numero de pin que se quiere leer
 *
 * @return true si el pin se encuentra en estado alto (HIGH / 1)
 * @return false si el pin se encuentra en estado bajo (LOW / 0)
 */
bool gpio_read(uint8_t pin);

/**
 * @brief Funcion de escritura de estado logico (alto o bajo) en el pin espesificado
 * @pre El numero de pin debe haber sido configurado como salida
 * 
 * @param pin numero de pin que al que se quiere escribir
 * @param value valor logico a escribir (0 para bajo, 1 para alto)
 */
void gpio_write(uint8_t pin, uint8_t value);

#endif