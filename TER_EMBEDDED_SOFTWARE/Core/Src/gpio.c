/*
 * gpio.c
 *
 *  Created on: Feb 6, 2025
 *      Author: HP
 */

/**Includes ---------------------------------------------------------------------------*/
#include "gpio.h"
#include "stm32l476xx.h"

/*Implementation des fonctions---------------------------------------------------------*/
/*-------------------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------------------*/

void GPIO_Init(void)
{

	RCC->AHB2ENR  |= (1 << 0) | (1 << 1); 									//Active l'horloge du GPIOA	et GPIOB

	GPIOA->MODER  &= ~(3 << 5*2);
	GPIOA->MODER  |=  (1 << 5*2);

	//Config entrees acquisition PAO et PA1 en mode analogique
	GPIOA->MODER &= ~(3 << 2*0);											//Efface les bits de configuration	PA0
	GPIOA->MODER |= (3 << 2*0);												//Config PA0 en mode 11 analogique
	GPIOA->ASCR  |= (1<<0);      											//Connecter un commutateur analogique à l'entrée de l'ADC (PORT A entrée 0)

	GPIOA->MODER &= ~(3 << 2*1);											//Efface les bits de configuration	PA1
	GPIOA->MODER |= (3 << 2*1);												//Config PA1 en mode 11 analogique
	GPIOA->ASCR  |= (1<<1);      											//Connecter un commutateur analogique à l'entrée de l'ADC (PORT A entrée 1)

	//Config LED Temoins PB5
	GPIOB->MODER &= ~(3 << 2*5);											//Efface les bits de configuration	PB5
	GPIOB->MODER |= (1 << 2*5);												//Config PB5 en mode 01 sortie

	//Config UART pour le debug
	//Configurer PA2 (TX) et PA3 (RX) en mode Alternate Function (AF7)
	GPIOA->MODER &= ~(3 << (2 * 2));  										//PA2 en mode alternatif (10)
	GPIOA->MODER |=  (2 << (2 * 2));

	GPIOA->MODER &= ~(3 << (3 * 2)); 										//PA3 en mode alternatif (10)
	GPIOA->MODER |=  (2 << (3 * 2));

	//Configuration de la vitesse (optionnel)
	GPIOA->OSPEEDR |=   (2 << (2 * 2)) | (2 << (3 * 2)); 					//Vitesse haute
	GPIOA->PUPDR   &= ~((3 << (2 * 2)) | (3 << (3 * 2)));  					//Desactiver les résistances pull-up/pull-down


	//Configurer PA7, PA8, PA9 et PB0 en mode Alternate Function (AF7)
	GPIOA->MODER &= ~(3 << (8 * 2));  										//PA8 en mode alternatif (10)
	GPIOA->MODER |=  (2 << (8 * 2));

	GPIOA->MODER &= ~(3 << (7 * 2));  										//PA7 en mode alternatif (10)
	GPIOA->MODER |=  (2 << (7 * 2));

	//-----------------------------------------------------------------------------------

	GPIOA->MODER &= ~(3 << (9 * 2)); 										//PA9 en mode alternatif (10)
	GPIOA->MODER |=  (2 << (9 * 2));

	GPIOB->MODER &= ~(3 << (2 * 0));										//PB0 en mode alternatif (10)
	GPIOB->MODER |=  (2 << (2 * 0));

	//Selection TIM1 pour utiliser les 4 sorties en mode Alternative
	GPIOA->AFR[0] &= ~(0xFFFF << 0);      									//Efface les config des modes alternatifs actuels ~(0xF << 0) &~(0xF << 4) &~(0xF << 8) &~(0xF << ) ; 			/* PA8 -> AF7 (USART2_TX)					*/
	GPIOA->AFR[1] &= ~(0xFFFF << 0);      									//Efface les config des modes alternatifs actuels
	GPIOB->AFR[0] &= ~(0xFFFF << 0);      									//Efface les config des modes alternatifs actuels

	GPIOA->AFR[1] |= (0x1 << 4*0); 											//PA8 -> AF1 (TIM1_CH1)
	GPIOA->AFR[0] |= (0x1 << 4*7); 											//PA7 -> AF1 (TIM1_CH1N)

	GPIOA->AFR[1] |= (0x1 << 4*1); 											//PA9 -> AF1 (TIM1_CH2)
	GPIOB->AFR[0] |= (0x1 << 4*0); 											//PB0 -> AF1 (TIM1_CH2N)

	//Selection de AF7 pour USART2 (Activation USART2 sur PA2 et PA3)
	GPIOA->AFR[0] |= (7 << (2 * 4)); 										//PA2 -> AF7 (USART2_TX)
	GPIOA->AFR[0] |= (7 << (3 * 4)); 										//PA3 -> AF7 (USART2_RX)
}

/*--------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------*/

void init_system_clock(void)
{

/* Configuration de l'horloge système SYSCLK, par défaut MSI à 4MHz */
	RCC->CR &= ~(1UL << 0); 		//MSI clock off, for MSI range configuration

  //MSI freq. bits[7654]=(1010->32MHz,1000->16MHz,default 0110->4MHz, min 0000->100kHz, max 1011->48MHz)
	RCC->CR &= ~(1UL<<7);
	RCC->CR &= ~(1UL<<6);
	RCC->CR &= ~(1UL<<5);
	RCC->CR &= ~(1UL<<4);


	RCC->CR |= (1UL<<3);   													//MSI clock range in RCC_CR

	while (!(RCC->CR & RCC_CR_MSIRDY)); 									// Attendre que MSI soit stable

	RCC->CR |= (0b1010UL<<4);   											// 1010 MSI à 32 MHz

	RCC->CR |= ( 1UL<< 0); 													//MSI clock on, only once MSI range configured

	RCC->CFGR &= 0xffffff0f;   												//HPRE = 0000
	RCC->CFGR &= ~(7UL << 8);  												// PPRE1 = 000 (Prescaler APB1 = 1) => PCLK1 = 80 MHz
	RCC->CFGR &= ~(7UL << 11); 												// PPRE2 = 000 (Prescaler APB2 = 1) => PCLK1 = 80 MHz


	SystemCoreClock = 32000000;	 											// 32 MHz //Mise à jour de la variable SystemCoreClock
}


//Fonction pour checker la configuration des Horloges
uint32_t get_APB2_frequency(){

	uint32_t sysclk, hpre, ppre2, apb2_freq;

	//Frequence du systeme ( HCLK)
	sysclk = SystemCoreClock;

	//Lecture du prescaler AHB (HPRE) dans RCC-CFGR
	hpre = (RCC->CFGR & RCC_CFGR_HPRE_Msk) >> RCC_CFGR_HPRE_Pos;

	//Facteur de division HPRE
	uint32_t hpre_div[16] = {1, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512};

	uint32_t hclk = sysclk / hpre_div[hpre]; //Frequence HCLK
	//Lire le prescaler APB2 (ppre) dans RCC->CFGR

	ppre2 = (RCC->CFGR & RCC_CFGR_PPRE2_Msk) >> RCC_CFGR_PPRE2_Pos;

	//Facteur de division PPRE2
	uint32_t ppre2_div[8] = {1, 1, 2, 4, 8, 16};

	apb2_freq = hclk / ppre2_div[ppre2];

	return apb2_freq;

}



