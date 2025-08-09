/*
 * TER_software.c
 *
 *  Created on: Feb 6, 2025
 *      Author: HP
 */

/**Includes -----------------------------------------------------------------------------------------------------------------*/

#include "TER_software.h"
#include "gpio.h"
#include "stm32l4xx.h"
#include "stm32l476xx.h"


/*-----------------------------------------------------------------------------------------------------------*/
/** @defgroup Macros_PWM
 *  @brief Paramètres pour la génération du signal PWM
 *  @{
 */
#define PWM_PERIOD 				 50						/**< Période PWM 									*/
#define N 						 100					/**< Nombre de points dans la table du sinus 		*/
#define PWM_MAX 				 50						/**< Valeur maximale d'amplitude du sinus (duty 100%)*/
#define DUTY_MIN 				 0.1f					/**< Duty cycle minimum limité à 10% 				*/
#define DUTY_MAX 				 0.9f					/**< Duty cycle maximum limité à 90% 				*/
#define MOD_MIN                  0.10f                 	/**< Modulation minimale 							*/
#define MOD_MAX                  0.90f                 	/**< Modulation maximale 							*/
/** @} */

/*-----------------------------------------------------------------------------------------------------------*/
/** @defgroup Macros_ADC
 *  @brief Paramètres pour la conversion ADC
 *  @{
 */
#define ADC_RESOLUTION     		  4096.0f   			/**< Résolution ADC 12 bits */
#define VREF               		  3.3f      			/**< Tension de référence ADC */
/**
 * @brief Convertit une valeur ADC brute en tension analogique.
 * @param adc_val Valeur ADC
 * @return Tension correspondante en Volts
 */
#define ADC_TO_VOLT(adc_val)   	  (((adc_val) * VREF) / (ADC_RESOLUTION - 1))

/** @brief Retourne la plus petite des deux valeurs. */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/** @brief Retourne la plus grande des deux valeurs. */
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/** @brief Contraint une valeur entre deux bornes. */
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
/** @} */

/*-----------------------------------------------------------------------------------------------------------*/
/** @defgroup Macros_Charge
 *  @brief Paramètres de contrôle de la charge
 *  @{
 */
#define VREF_LOAD 10.0									/**< Tension cible de la charge */
#define VREF_ALIM 33.0									/**< Tension d'alimentation du pont DC-AC */
/** @} */

/*-----------------------------------------------------------------------------------------------------------*/
/** @defgroup Variables
 *  @brief Déclarations de variables globales
 *  @{
 */
volatile uint16_t digitalVoltage = 0;      				/**< Tension numérique mesurée */
volatile float analogVoltage 	 = 0;       			/**< Tension analogique convertie */
volatile uint32_t dutyCycle      = 0;       			/**< Rapport cyclique PWM */
float sin_table[N];					      				/**< Table de valeurs normalisées du sinus */
volatile uint8_t dataReady = 0;             			/**< Drapeau indiquant que les données sont prêtes */
volatile uint32_t system_micros = 0;


/** @} */


/*----------------------------------------------------------------------------------------------------------------------------*/
/*Implementation des fonctions------------------------------------------------------------------------------------------------*/


void Update_PWM(uint16_t digitalVoltage) {

    static uint16_t i = 0;
    static float k = 0.342; 							// valeur initiale de k =0.1+(0.8*10)/33

    // Conversion ADC vers tension mesurée
    analogVoltage = ADC_TO_VOLT(digitalVoltage)*10; 	// Gain x10 → Vmes max = 33V


    // Sécurité sur la tension mesurée : éviter valeurs absurdes
    if (analogVoltage < 0.1f || analogVoltage > 33.0f) {
    	analogVoltage = VREF_ALIM;						// Valeur par défaut sécurisée
    }

    //Calcul du facteur de correction dynamique k
    //Non appliqué ici
    // limitation de k pour éviter dérives trop fortes
    k = MIN(0.9f, MAX(0.1f, k));

    //Modulation amplitude sinus corrigée : ici le correcteur n'est pas appliqué
    float modulated = sin_table[i];

    // Clamp modulated
    if (modulated > 1.0f || isnan(modulated)) modulated = MOD_MAX;
    if (modulated < 0.0f) modulated = MOD_MIN;

    // Mise en plage du rapport cyclique (10% à 90%)
    float duty = DUTY_MIN + (DUTY_MAX - DUTY_MIN) * modulated;
    uint16_t dutyCycle = (uint16_t)(duty * PWM_MAX);

    // Application PWM
    TIM1->CCR1 = dutyCycle;
    TIM1->CCR2 = 50;
    systickDelayUs(50);
    TIM1->CCR2 = 0;
    TIM1->EGR |= (1 << 0); 								// Mise à jour immédiate


    dataReady = 1;
    // Avancement sinus circulaire
    i = (i + 1) % N;
}


/*----------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------*/

void generate_normalized_sin_table(void) {

	//sin(x) = [-1;1]
	//(sin(x) + 1)/2 = [0; 1]  : Normalisation entre O et 1

    for (int i = 0; i < N; i++) {
        float angle = (2.0f * 3.14159f * i) / N;
        sin_table[i] = (sinf(angle) + 1.0f) / 2.0f;  	// entre 0 et 1
    }
}

/*----------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------*/
//Fonctions UART

void USART2_Init(void){

	RCC->APB1ENR1 |= (1 << 17);							//Activer l'horloge pour USART2

	/*Configurer USART2 : 115200 baud, 8 bits, 1 stop bit, pas de parité*/
	USART2->BRR = (32000000 / 115200); 					//Baudrate pour PCLK = 32 MHz
	USART2->CR1 |= USART_CR1_TE;
	USART2->CR1 |= USART_CR1_UE; 						//Activer TX et USART

	/*Attendre que l'USART soit prêt*/
	while (!(USART2->ISR & USART_ISR_TEACK));
}


void USART2_SendChar(char value) {
    while (!(USART2->ISR & (1 << 7))); 					//Attendre que TXE=1 (buffer vide)
    USART2->TDR = value; 								//Envoyer le caractère
}


void USART2_SendMsg(char *msg){

	for(uint8_t i = 0; i < strlen(msg); i++){
		USART2_SendChar(msg[i]);						//Envoie des caracteres en boucle
	}
	//USART2_SendChar('\r');							//Retour chariot
	USART2_SendChar('\n');								//Retour à ligne
}

/*---------------------------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------------------------*/
//TIMER1

void TIM1_Config_PWM(void){

	RCC->APB2ENR |= (1 << 11); 							//Active l'horloge du TIMER1
	TIM1->CR1  &= ~(1 << 0); 		 					//Desactive le timer
	TIM1->BDTR &= ~TIM_BDTR_MOE;  						//Desactivation du MOE

	TIM1->PSC = 31;							 			//config prescalaire pour 50us (20Khz)
	TIM1->ARR = 49;										//config valeur d'autoreload adaptée pour 50us (20Khz)

	TIM1->CR1 &= ~(3 << 5);								//Efface les bits du mode : Mode Edge-Aligned 1 activé (00) Tpwm = periode simple (pas de doublage)
	//TIM1->CR1 |=  (1 << 5);								//Mode Center-Aligned 1 activé (01) Tpwm = 2Tpwm(effective) : double la période

	TIM1->CR1 &= ~(1 << 4);								//Compteur montant DIR = 0

	//Configuration du mode PWM1
	//Configurer TIM1_CH1 (PA8) en mode PWM1
	TIM1->CCMR1 &= ~(7 << 4);      						// Réinitialiser OC1M (bits 3-5)
	TIM1->CCMR1 |= 	(6 << 4);      						// Configurer OC1M en mode PWM1 (0110)


	//Configurer TIM1_CH2 (PA9) en mode PWM1
	TIM1->CCMR1 &= ~(7 << 12);      					// Réinitialiser OC2M (bits 8-10)
	TIM1->CCMR1 |= (6 << 12);       					// Configurer OC2M en mode PWM1 (110)

	//Définir les valeurs des rapports cycliques initials
	TIM1->CCR1 =  1;  									// 10% pour TIM1_CH1 (et TIM1_CH1N)
	TIM1->CCR2 =  0;  									// 10% pour TIM1_CH2 (et TIM1_CH2N)

	//Activer la précharge des CCR pour changer proprement le rapport cyclique pendant que le timer tourne
	TIM1->CCMR1 |= TIM_CCMR1_OC1PE;  					// Précharge pour TIM1_CH1 (PA8) (et TIM1_CH1N : PA7)
	TIM1->CCMR1 |= TIM_CCMR1_OC2PE;  					// Précharge pour TIM1_CH2 (PA9) (et TIM1_CH2N : PB0)

	//Activer les sorties PWM (canaux)
	TIM1->CCER |= TIM_CCER_CC1E;  						// Activer la sortie pour TIM1_CH1 (PA8)
	TIM1->CCER |= TIM_CCER_CC1NE;  						// Activer la sortie pour TIM1_CHN1 (PA7)

	TIM1->CCER |= TIM_CCER_CC2E;  						// Activer la sortie pour TIM1_CH2 (PA9)
	TIM1->CCER |= TIM_CCER_CC2NE;  						// Activer la sortie pour TIM1_CHN2 (PB0)


	//Configuration du Deadtime
	TIM1->BDTR &= ~(0xFF << 0);   						//Efface les bits du DTG = 0000000
	//TIM1->BDTR |= (0x0 << ); 	  						// DTG[7:5] = 111
	TIM1->BDTR |= (0x9 << 0);   						// DTG[7:0] = 0000100 = 4 -> DT = 281ns

	//Activer le Main Output Enable (MOE) dans le registre BDTR
	TIM1->CR1  |= TIM_CR1_ARPE;  						// Permet de modifier proprement la periode (ARR) pendant que le timer tourne
	TIM1->BDTR |= TIM_BDTR_MOE;  						// Main Output Enable
	//TIM1->RCR   = 0;									//Desactive le mode période etendue (Reset value = 0)
	TIM1->CR1  |= (1 << 0); 		 					//config demarre le compteur
}

//Config Tache périodique : 1ms
void TIM2_Config_IT(void){

	RCC->APB1ENR1 	|= (1 << 0); 						//Active l'horloge du TIMER 2

	TIM2->PSC =   31;								 	//config prescalaire pour 500ms
	TIM2->ARR =  999;									//config valeur d'autoreload
	TIM2->DIER |= (1 << 0);								//config active l'interruption
	TIM2->CR1 |= (1 << 0); 								//config demarre le compteur

	NVIC_SetPriority(TIM2_IRQn, 2);
	NVIC_EnableIRQ(TIM2_IRQn);							//Controleur Active l'interruption  TIM2

}

//TIM2 IRQ Handler : Tache principale de l'algorithme
//Tache périodique : 1ms
void TIM2_IRQHandler(void) {

    if (TIM2->SR &   (1 << 0)){  						// Vérifier le flag d'interruption
        TIM2->SR &= ~(1 << 0);  						// Effacer le flag d'interruption

        digitalVoltage = ADC_Read();					//Lecture des valeurs de tensions via ADC
        Update_PWM(digitalVoltage);						//Traitement et Mise à jour du Rapport cyclique
    }
}

/*----------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------*/
//Fonctions ADC

void ADC_Init(void)
{
	  RCC->AHB2ENR  |= (1 << 13); 						//Activer horloge pour l'ADC1
	  ADC123_COMMON->CCR |= (1<<16);					//HCLK horloge d'entree du convertisseur, CK_Mode =01
	  ADC123_COMMON->CCR &= ~(1<<17);	
	  //ADC1->CCR |= (1<<16);							//Alternative de configuration
	  				
	  //Prediviseur si ADC clock = SystemClock et CK_Mode = 00
	  //ADC123_COMMON->CCR &= ~(1<<21) & ~(1<<20) & ~(1<<19) & ~(1<<18); /*prediviseur d'horloge ADC :  1 = 0000							*/
	
	  ADC1->CR |= (1<<1);         					    //Desactivation de l'ADC 

	  ADC1->CR &= ~(1<<29);   							//Desactivation du mode sommeil
	  ADC1->SQR1 |= (0 << 0);   						//Nombre de conversion N = 2*/
	  ADC1->SQR1 |= (1 << 6) | (1<<8);   				//Premiere conversion ADC1_IN5 (PA0 connecte a ADC1 Input5
	  //ADC1->SQR1 |= (1 << 13)| (1 << 14);   			//Deuxieme conversion ADC1_IN6 (PA1 connecte a ADC1 Input6
	  //ADC1->SQR1 &= 0xfffffff0; 						//~(1<<0) & ~(1<<1) & ~(1<<2) & ~(1<<3); //une conversion

	  ADC1->CR |= 0x10000000;  							//Active le regulateur de tensin de l'ADC : ADVREGEN to 1
	  while (!(ADC1->CR & (1<<28)));

	  /*ADC calibration*/
	  ADC1->CR &= ~(1UL<<30); 							//Select calibration mode : ADCALDIF = 0 (single-ended) 
	  ADC1->CR |= (1UL<<31);  							//Demarre la calibration de  ADC : ADCAL to 1
	
	  while ((ADC1->CR & ADC_CR_ADCAL)); 				//Attendre ADCAL=0
	  systickDelayMs(10);    							//Attendre 10 ms la fin de calibration de l'ADC

	  //Active ADC si configuree (calibration, deep power mode off, voltage regulator on)
	  ADC1->ISR |= ADC_ISR_ADRDY;          			    //Effacer l'état prêt de l'ADC : ADRDY = 1

	  ADC1->CR |= ADC1_CR_ADEN;           			    //Activation de l'ADC ADEN = 1
	  while (!(ADC1->ISR & ADC_ISR_ADRDY)); 			//Attendre que ADRDY=1  (1UL<<0)

	  systickDelayMs(10);     							//Attendre 10ms : stabilisation de ADC
}

uint16_t ADC_Read(void)
{
    ADC1->CR |= (1 << 2);        						//Démarrer la conversion
    while ((ADC1->CR & (1 << 2))); 				//Attendre la fin de la conversion
    return (uint16_t)ADC1->DR;        					//Lire la valeur stockee
}

/*----------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------------------------*/
//Fonctions delay

void systickDelayMs(int n)
{
	//pour avoir une fonction qui fabrique un délai en ms

    SysTick->LOAD = 32000; 								//Recharger avec le nombre de ticks par milliseconde
    SysTick->VAL = 0;     								//Effacer la valeur actuelle du registrer
    SysTick->CTRL = 0x5;  								//Activer Systick

    for(int i = 0; i < n; i++)
    {
        //Attendre jusqu'a l'actualisation du drapeau COUNT flag
        while((SysTick->CTRL & 0x10000) == 0){;;}
    }
    SysTick->CTRL = 0;
}


void systickDelayUs(int n)
{
    SysTick->LOAD = 32 - 1;      						// 32 ticks pour 1 µs (32 MHz)
    SysTick->VAL = 0;            						// Réinitialiser la valeur actuelle
    SysTick->CTRL = 0x5;        	 					// Activer SysTick (horloge processeur)

    for(int i = 0; i < n; i++)
    {
        // Attendre le flag COUNT
        while((SysTick->CTRL & 0x10000) == 0);
    }

    SysTick->CTRL = 0;           						// Désactiver SysTick
}


