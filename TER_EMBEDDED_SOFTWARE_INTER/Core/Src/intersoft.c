/*
 * intersoft.c
 *
 *  Created on: Mar 24, 2025
 *      Author: HP
 */

#include "stm32l4xx.h"
#include "stm32l476xx.h"
#include "intersoft.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Pour l'ADC

// Configuration pour CPU à 32 MHz
#define CPU_FREQ_MHZ 32
#define PWM_PERIOD_US 100

// Définition des erreurs PWM
#define PWM_ERR_OVF     (1 << 0)  									// Débordement timer
#define PWM_ERR_SHORT   (1 << 1)  									// Période trop courte (<5µs)
#define PWM_ERR_INVALID (1 << 2)
// Signal incohérent

volatile int test_dac = 0;
char buffer[100];

volatile uint32_t pwmPeriod = 0;
volatile uint32_t pwmDuty = 0;
volatile uint8_t dataReady = 0;
volatile uint8_t ADC_Set = 0;
char ctrl_value[100];										// Lire et efface RXNE automatiquement

//Zone des Variables pour le DMA

#define BUFFER_SIZE 100												// Taille des buffers (ajustable)
#define RX_BUF_SIZE 10
volatile char rx_buffer[RX_BUF_SIZE];
volatile uint8_t rx_index = 0;
volatile uint8_t rx_ready = 0;

// Buffers circulaires pour CCR1 et CCR2
volatile uint32_t ccr1_buffer[BUFFER_SIZE] __attribute__((aligned(4)));
volatile uint32_t ccr2_buffer[BUFFER_SIZE] __attribute__((aligned(4)));

volatile uint32_t capture_count = 0; 	 							// Index de capture actuel
volatile uint8_t  DMA_captureCCR1_set = 0;
volatile uint8_t  DMA_captureCCR2_set = 0;

//----------------------------------------------------------------------------------------
void init_system_clock(void)
{

	RCC->CR &= ~(1UL<<7);											//Reset bits de configuration valeur MSI
	RCC->CR &= ~(1UL<<6);
	RCC->CR &= ~(1UL<<5);
	RCC->CR &= ~(1UL<<4);

	RCC->CR |= (0b1010UL<<4);   									// 1010 MSI à 32 MHz
	RCC->CR |= (1UL<<3);   											//MSI clock range in RCC_CR
	RCC->CR |= ( 1UL<< 0); 											//MSI clock on, only once MSI range configured

	RCC->CFGR &= 0xffffff0f;   										//HPRE = 0000
	RCC->CFGR &= ~(7UL << 8);  										// PPRE1 = 000 (Prescaler APB1 = 1) => PCLK1 = 80 MHz
	RCC->CFGR &= ~(7UL << 11); 										// PPRE2 = 000 (Prescaler APB2 = 1) => PCLK1 = 80 MHz

	while (!(RCC->CR & RCC_CR_MSIRDY)); 							// Attendre que MSI soit stable

	SystemCoreClock = 32000000; 									// 32 MHz //Mise à jour de la variable SystemCoreClock
}



void GPIO_Init(void)
{

	RCC->AHB2ENR  |= (1 << 0) | (1 << 1); 							//Active l'horloge du GPIOA	et GPIOB
	RCC->APB1ENR1 |= (1 << 17);										//Activer l'horloge pour USART2

	GPIOA->MODER  &= ~(3 << 5*2);									//Reset bits de configuration
	GPIOA->MODER  |=  (1 << 5*2);									//PA5 output

	//Config LED Temoins PB4 et PB5
	GPIOB->MODER &= ~(3 << 2*4);									//Efface les bits de configuration	PB4
	GPIOB->MODER |= (1 << 2*1);										//PB4 output

	GPIOB->MODER &= ~(3 << 2*5);									//Efface les bits de configuration	PB5
	GPIOB->MODER |= (1 << 2*5);										//PB5 output

	GPIOA->MODER |= (3 << (4 * 2));   								// MODER4 = 11 (analog mode)
	GPIOA->PUPDR &= ~(3 << (4 * 2));  								// Pas de pull-up/pull-down

	//Config UART pour le debug
	//Configurer PA2 (TX) et PA3 (RX) en mode Alternate Function (AF7)
	GPIOA->MODER &= ~(3 << (2 * 2));  								//PA2 en mode alternatif (10)
	GPIOA->MODER |=  (2 << (2 * 2));

	GPIOA->MODER &= ~(3 << (3 * 2)); 								//PA3 en mode alternatif (10)
	GPIOA->MODER |=  (2 << (3 * 2));

	//Configuration de la vitesse (optionnel)
	GPIOA->OSPEEDR |=   (2 << (2 * 2)) | (2 << (3 * 2)); 			//Vitesse haute
	GPIOA->PUPDR   &= ~((3 << (2 * 2)) | (3 << (3 * 2)));  			//Desactiver les résistances pull-up/pull-down


	//onfigurer PA8, PA9, PA10 et PA11 en mode Alternate Function (AF7)
	GPIOA->MODER &= ~(3 << (8 * 2));  								//PA8 en mode alternatif (10)
	GPIOA->MODER |=  (2 << (8 * 2));

	GPIOA->MODER &= ~(3 << (9 * 2)); 								//PA9 en mode alternatif (10)
	GPIOA->MODER |=  (2 << (9 * 2));

	//Selectionner TIM1 pour utiliser les 4 sorties en mode Alternative
	GPIOA->AFR[0] &= ~(0xF << 0);      							//Efface les config des modes alternatifs actuels ~(0xF << 0) &~(0xF << 4) &~(0xF << 8) &~(0xF << ) ; 			/* PA8 -> AF7 (USART2_TX)					*/
	GPIOA->AFR[1] &= ~(0xFFFF << 0);      							//Efface les config des modes alternatifs actuels

	GPIOA->AFR[1] |= (0x1 << 4*0); 									//PA8 -> AF1 (TIM1_CH1)
	GPIOA->AFR[1] |= (0x1 << 4*1); 									//PA9 -> AF1 (TIM1_CH2)


	//Selection de AF7 pour USART2 (Activation USART2 sur PA2 et PA3)
	GPIOA->AFR[0] |= (7 << (2 * 4)); 								// PA2 -> AF7 (USART2_TX)
	GPIOA->AFR[0] |= (7 << (3 * 4)); 								//PA3 -> AF7 (USART2_RX)
}

/*--------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------*/

void TIM1_Config_PWMInputCaptureMode(void){

	//--------------------------------------------------------------------------------------------//
	// Activation horloge TIM1
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
	//__DSB(); // Barrière mémoire pour s'assurer que l'horloge est active

	// Prescaler pour 1 MHz (32MHz/32)
	TIM1->PSC = CPU_FREQ_MHZ - 1;  									// 32MHz / 32 = 1 MHz

	//  Auto-reload max
	TIM1->ARR = 0xFFFF;

	// Configuration Input Capture
	// Canal 1 (front Montant) sur TI1 (Timer input 1)
	TIM1->CCMR1 |= TIM_CCMR1_CC1S_0;  								// bits 1-0 CC1S=01
	TIM1->CCMR1 |= TIM_CCMR1_IC1F_0;  								//bits 7-4  IC1F=0001  filtrage basic des signaux à l'entree

	// Canal 2 (front descendant) sur TI1 (Timer input 1)
	TIM1->CCMR1 |= TIM_CCMR1_CC2S_1;  								// bits 9-8 CC2S=10
	TIM1->CCMR1 |= TIM_CCMR1_IC2F_0;  								// bits 15-12 CC2S=10	filtrage basic des signaux à l'entree

	// Polarités
	TIM1->CCER |= TIM_CCER_CC1E;	  								//Enable capture compare mode
	TIM1->CCER |= TIM_CCER_CC2E;	  								//Enable capture compare mode
	TIM1->CCER &= ~TIM_CCER_CC1P;									// CH1 = front montant
	TIM1->CCER |= TIM_CCER_CC2P;									// CH2 = front descendant

	// Mode reset sur TI1FP1
	TIM1->SMCR &= ~TIM_SMCR_TS;
	TIM1->SMCR |= TIM_SMCR_TS_2 | TIM_SMCR_TS_0;					//Trigger Selection : 101 (000 - 111) trigger on TI1FP1 entrée filtrée du Timer Input 1 	//Trigger Selection : 100 = Edge Detector sur TI1

	TIM1->SMCR &= ~TIM_SMCR_SMS;
	TIM1->SMCR |= TIM_SMCR_SMS_2;									//Configurer le mode reset (le déclenchement réinitialise le compteur)
	//------------------------------------------------------------//
	// Activation interruptions
	//TIM1->DIER = TIM_DIER_CC1IE | TIM_DIER_CC2IE;

	// Priorité NVIC (plus haute pour sécurité)
    //NVIC_SetPriority(TIM1_CC_IRQn, 0);
	//NVIC_EnableIRQ(TIM1_CC_IRQn);

	//Configuration TIM1 pour trigger DMA sur capture
	//TIM1->DIER |= TIM_DIER_CC1DE | TIM_DIER_CC2DE;  				// DMA Enable sur CC1/CC2

	// Advanced timer requirement
	//TIM1->BDTR |= TIM_BDTR_MOE;
	//Start timer
	//------------------------------------------------------------//

	TIM1->CR1 = TIM_CR1_CEN;

}

//Interruption sur capture non utilisé ici
void TIM1_CC_IRQHandler(void)
{
    // Capture CCR1 = Période (front montant -> front montant)
    if (TIM1->SR & TIM_SR_CC1IF) {
        TIM1->SR &= ~TIM_SR_CC1IF;

        uint32_t periode_us = TIM1->CCR1;
        sprintf(buffer, "Période : %lu us", periode_us);
        USART2_SendMsg(buffer);
    }

    // Capture CCR2 = Durée High (front montant -> front descendant)
    if (TIM1->SR & TIM_SR_CC2IF) {
        TIM1->SR &= ~TIM_SR_CC2IF;

        uint32_t high_us = TIM1->CCR2;
        sprintf(buffer, "%lu", high_us);
        USART2_SendMsg(buffer);
    }
}



void systickDelayMs(int n)
{
	/*pour avoir une fonction qui fabrique un délai en ms*/

    SysTick->LOAD = 32000; 											//Recharger avec le nombre de ticks par milliseconde
    SysTick->VAL = 0;     											//Effacer la valeur actuelle du registrer
    SysTick->CTRL = 0x5;  											//Activer Systick

    for(int i = 0; i < n; i++)
    {
        //Attendre jusqu'a l'actualisation du drapeau COUNT flag
        while((SysTick->CTRL & 0x10000) == 0);
    }
    SysTick->CTRL = 0;
}

void systickDelayUs(int n)
{
    SysTick->LOAD = 32 - 1;      // 32 ticks pour 1 µs (32 MHz)
    SysTick->VAL = 0;            // Réinitialiser la valeur actuelle
    SysTick->CTRL = 0x5;         // Activer SysTick (horloge processeur)

    for(int i = 0; i < n; i++)
    {
        // Attendre le flag COUNT
        while((SysTick->CTRL & 0x10000) == 0);
    }

    SysTick->CTRL = 0;           // Désactiver SysTick
}

void USART2_Init(void){

	//Configurer USART2 : 115200 baud, 8 bits, 1 stop bit, pas de parité

	USART2->BRR = (32000000 / 115200); 								// Baudrate pour PCLK = 32 MHz
	USART2->CR1 = 0x00;                    							// Remise à zéro du registre CR1
	USART2->CR2 = 0x00;                    							// 1 stop bit (par défaut)
	USART2->CR3 = 0x00;                    							// Pas de contrôle de flux (par défaut)
	USART2->CR1 |= USART_CR1_TE;									// Activer la transmission
	USART2->CR1 |= USART_CR1_RE; 									// Activer la réception
	USART2->CR1 |= USART_CR1_UE; 									//Activer USART

    USART2->CR1 |= USART_CR1_RXNEIE;  								// Activer l'interruption sur réception (RXNEIE=1)

    NVIC_SetPriority(USART2_IRQn, 0);  								// (Optionnel) priorité 1 pour USART2
    NVIC_EnableIRQ(USART2_IRQn);       								// Autorise les IT USART2 dans le NVIC

	//Attendre que l'USART soit prêt
	while (!(USART2->ISR & USART_ISR_TEACK));
}


void USART2_SendChar(char value) {
    while (!(USART2->ISR & (1 << 7))); 								//Attendre que TXE=1 (buffer vide)
    USART2->TDR = value; 											//Envoyer le caractère
}

/*---------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------*/

void USART2_SendMsg(char *msg){

	for(uint8_t i = 0; i < strlen(msg); i++){
		USART2_SendChar(msg[i]);									//Envoie des caracteres en boucle
	}
	//USART2_SendChar('\r');											//Retour chariot
	USART2_SendChar('\n');											//Retour à ligne

	// Attendre que la transmission soit complète
	while (!(USART2->ISR & USART_ISR_TC));
}

void USART2_receive_string(char *buffer, uint16_t buffer_size) {
    uint16_t i = 0;
    char received_char;
    do {
        // Attentte que des données soient reçues
        while (!(USART2->ISR & USART_ISR_RXNE));
        // Lire le caractère reçu
        received_char = (char)(USART2->RDR);
        // Stockage du caractère dans le buffer si la taille le permet
        if (i < (buffer_size - 1)) {
            buffer[i++] = received_char;
        }
    } while (received_char != '\n'); 								// Fin de la réception
    buffer[i] = '\0'; 												// Terminer la chaîne
}


void USART2_IRQHandler(void){


    if (USART2->ISR & USART_ISR_RXNE) {

        USART2_receive_string(ctrl_value, sizeof(ctrl_value));		//Reception des valeurs de tensions via UART
        while (!(USART2->ISR & USART_ISR_TXE));						//Attendre la fin de la lecture
        ADC_Set = 1;
        int voltageCRTL = atoi(ctrl_value);
        if(voltageCRTL > 0 && voltageCRTL <= 4095){
        //Envoie des donnees reçues du système controlé vers le système de commande
        	GPIOA->ODR ^= (1 << 5);
        	DAC_Write((uint16_t)voltageCRTL);
        }

    }
}

/*--------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------*/
void DAC_Init(){

	RCC->APB1ENR1 |= (1 << 29); 									//Active l'horloge du DAC1
	DAC1->CR |= (1 << 0);				        					// Activer DAC (CH1_PA4)
}
/*--------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------*/

void DAC_Write(uint16_t value){
    DAC1->DHR12R1 = value;               							// Écrire la valeur sur 12 bits
}

/*--------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------*/
//------------------------NON UTILISE---------------------------------------------------------*/
//DMA non utilisé dans cette configuration
void DMA_Init(void) {

    // Désactiver les canaux avant configuration
    DMA1_Channel2->CCR &= ~DMA_CCR_EN;
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;

    // Activation horloge DMA
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    __DSB(); // Barrière mémoire

    // Efface les 4 bits de sélection du canal 2 (C2S)
    DMA1_CSELR->CSELR &= ~(0xF << (4 * 1));
    DMA1_CSELR->CSELR |=  (0x7 << (4 * 1));  // TIM1_CH1 on Channel2 (C2S)

    // Efface les 4 bits de sélection du canal 3 (C3S)
    DMA1_CSELR->CSELR &= ~(0xF << (4 * 2));
    DMA1_CSELR->CSELR |=  (0x7 << (4 * 2));  // TIM1_CH2 on Channel3 (C3S)


    // Réinitialisation complète des canaux
    DMA1_Channel2->CCR = 0;
    DMA1_Channel3->CCR = 0;

    // Configuration Channel 2 (CCR1 -> buffer)
    DMA1_Channel2->CPAR = (uint32_t)&(TIM1->CCR1);  // Source: registre CCR1
    DMA1_Channel2->CMAR = (uint32_t)ccr1_buffer;    // Destination: buffer
    DMA1_Channel2->CNDTR = BUFFER_SIZE;             // Nombre d'éléments

    DMA1_Channel2->CCR = DMA_CCR_PL_1 |            // Priorité haute
                         DMA_CCR_MSIZE_1 |         // 32-bit mémoire
                         DMA_CCR_PSIZE_1 |         // 32-bit périphérique
                         DMA_CCR_MINC |            // Incrément mémoire
                         DMA_CCR_CIRC |            // Mode circulaire
                         DMA_CCR_TCIE;             // Interruption fin transfert

    DMA1_Channel2->CCR &= ~DMA_CCR_DIR;            // Périphérique -> Mémoire (0)

    // Configuration Channel 3 (CCR2 -> buffer)
    DMA1_Channel3->CPAR = (uint32_t)&(TIM1->CCR2);
    DMA1_Channel3->CMAR = (uint32_t)ccr2_buffer;
    DMA1_Channel3->CNDTR = BUFFER_SIZE;

    DMA1_Channel3->CCR = DMA_CCR_PL_1 |
                         DMA_CCR_MSIZE_1 |
                         DMA_CCR_PSIZE_1 |
                         DMA_CCR_MINC |
                         DMA_CCR_CIRC |
                         DMA_CCR_TCIE;

    DMA1_Channel3->CCR &= ~DMA_CCR_DIR;            // Périphérique -> Mémoire (0)


    // Activation des interruptions
    //NVIC_SetPriority(DMA1_Channel2_IRQn, 0);
    //NVIC_SetPriority(DMA1_Channel3_IRQn, 1);
    //NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    //NVIC_EnableIRQ(DMA1_Channel3_IRQn);

    // Activation des canaux (doit être fait APRÈS TIM1_Config)
    DMA1_Channel2->CCR |= DMA_CCR_EN;
    DMA1_Channel3->CCR |= DMA_CCR_EN;
}


void DMA1_Channel2_IRQHandler(void) {

    if (DMA1->ISR & DMA_ISR_TCIF2) {
        DMA1->IFCR |= DMA_IFCR_CTCIF2; 	// Clear flag
        GPIOA->ODR ^= (1 << 5); 		// Toggle LED

        // Traitement des données
        for (int i = 0; i < BUFFER_SIZE; i++) {
            sprintf(buffer, "CCR1[%d]: %lu", i, ccr1_buffer[i]);
            USART2_SendMsg(buffer);
        }
    }

    if (DMA1->ISR & DMA_ISR_TEIF2) {
        DMA1->IFCR |= DMA_IFCR_CTEIF2; // Clear error flag
        USART2_SendMsg("DMA1 Ch2 Error!\n");
    }
}

void DMA1_Channel3_IRQHandler(void) {

    if (DMA1->ISR & DMA_ISR_TCIF3) {
        DMA1->IFCR |= DMA_IFCR_CTCIF3;
        DMA_captureCCR2_set = 0;

        // Traitement des données
        for (int i = 0; i < BUFFER_SIZE; i++) {
        	sprintf(buffer, "%lu", ccr2_buffer[i]);
            USART2_SendMsg(buffer);
        }
    }
}


//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//MES TESTS

void EXTI_PA0_Init(void) {
    //Activer l'horloge pour GPIOA et SYSCFG
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    //Configurer PA0 en entrée avec pull-down
    GPIOA->MODER &= ~(3 << (0 * 2));       // MODE0 = 00 (entrée)
    GPIOA->PUPDR &= ~(3 << (0 * 2));       // Clear PUPDR
    GPIOA->PUPDR |=  (2 << (0 * 2));       // Pull-down = 10

    //Connecter EXTI0 à PA0 via SYSCFG_EXTICR1
    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0_Msk; // EXTICR1 pour EXTI0
    // Pas besoin de changer la valeur car 0 = PA

    //Configurer EXTI0
    EXTI->IMR1 |= EXTI_IMR1_IM0;      // Activer interruption ligne 0
    EXTI->RTSR1 |= EXTI_RTSR1_RT0;    // Front montant
    EXTI->FTSR1 &= ~EXTI_FTSR1_FT0;   // Désactiver front descendant

    //Activer EXTI0 dans le NVIC
    NVIC_SetPriority(EXTI0_IRQn, 1);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

void EXTI0_IRQHandler(void) {

	uint32_t ccr1_a, ccr2_a;
	uint32_t ccr1_b, ccr2_b;

	if (EXTI->PR1 & EXTI_PR1_PIF0) {
	    EXTI->PR1 = EXTI_PR1_PIF0; // Effacer le drapeau d'interruption

        //Lecture coherente
		do {
			ccr1_a = TIM1->CCR1;
			ccr2_a = TIM1->CCR2;

			ccr1_b = TIM1->CCR1;
			ccr2_b = TIM1->CCR2;

		}while (ccr1_a != ccr1_b || ccr2_a != ccr2_b);

		pwmPeriod = TIM1->CCR1;
		pwmDuty   = TIM1->CCR2;

		dataReady = 1;       // Autoriser l'envoie des données

    }
}





