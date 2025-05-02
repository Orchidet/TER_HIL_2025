/*
 * gpio.h
 *
 *  Created on: Feb 6, 2025
 *      Author: HP
 */

/* Includes -------------------------------------------------------------------------------*/

#include "stm32l476xx.h"
#include "string.h"
#include <stdio.h>

/*-------------------------------------------------------------------------------------------*/


#ifndef INC_GPIO_H_
#define INC_GPIO_H_

#include "TER_software.h"

/**
 * @brief Initialise les GPIO nécessaires (PWM, ADC, UART, LED).
 */
void GPIO_Init(void);

/**
 * @brief Initialise l'horloge système à 32 MHz via MSI.
 */
void init_system_clock(void);

/**
 * @brief Récupère la fréquence du bus APB2.
 * @return Fréquence APB2 en Hz
 */
uint32_t get_APB2_frequency(void);


#endif /* INC_GPIO_H_ */
