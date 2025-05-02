
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/

#include "stm32l476xx.h"
#include "string.h"
#include <stdio.h>
#include "intersoft.h"
#include "main.h"

extern uint8_t dataReady;
//extern uint8_t ADC_Set;
//extern uint32_t pwmPeriod;
extern uint32_t pwmDuty;

int main(void)
{

	init_system_clock();
	GPIO_Init();
	DAC_Init();
	USART2_Init();
	//DMA_Init();
	EXTI_PA0_Init();
	TIM1_Config_PWMInputCaptureMode();
	//TIM1_Init_Slave();
	char buffer[100];

	while (1){
		//Transmission des donnees
		if(dataReady){
			USART2_SendMsg(buffer);
			snprintf(buffer, sizeof(buffer), "%lu", pwmDuty);
			USART2_SendMsg(buffer);
			dataReady = 0;
		}

	   //__WFI(); // Mode basse consommation : Attente d'une interruption
	}

}
