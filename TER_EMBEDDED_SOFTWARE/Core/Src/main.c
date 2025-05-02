 /* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes -------------------------------------------------------------------------------*/

#include "stm32l476xx.h"
#include "string.h"
#include <stdio.h>
#include "gpio.h"
#include "TER_software.h"
#include "main.h"

extern uint16_t digitalVoltage;
extern float analogVoltage;
extern uint8_t dataReady;


int main(void)
{

  /*INITIALISATION DES PERIPHERIQUES*/
	init_system_clock();
	GPIO_Init();
	USART2_Init();
	ADC_Init();
	TIM1_Config_PWM();
	TIM2_Config_IT();

	//Autres fonctions
	generate_normalized_sin_table();
	char buffer[100];
	while (1)
	{
		if(dataReady){
			// Envoi UART: Tension recu de LABVIEW
			snprintf(buffer, sizeof(buffer), "Voltage received from LABVIEW : %.2f",analogVoltage);
			USART2_SendMsg(buffer);
			dataReady = 0;
		}
		__WFI();  //Attendre une interruption : Mode economie d'énergie
	}

}
