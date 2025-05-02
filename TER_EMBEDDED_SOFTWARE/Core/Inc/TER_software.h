/*
 * TER_software.h
 *
 *  Created on: Feb 6, 2025
 *  Author: HP
 */

/* Includes -------------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------------------------*/

#ifndef INC_TER_SOFTWARE_H_
#define INC_TER_SOFTWARE_H_


#include "stm32l476xx.h"
#include "string.h"
#include <stdio.h>
#include <math.h>
#include "gpio.h"

/*

void Update_PWM(uint16_t digitalVoltage);
void generate_normalized_sin_table(void);
void systickDelayMs(int n);
void systickDelayUs(int n);
//------------------------------------------------------------------------------------//
void USART2_Init(void);
void USART2_SendChar(char value);
void USART2_SendMsg(char *msg);
void UART_Send(uint16_t data);

//------------------------------------------------------------------------------------//
void TIM2_Config_IT(void);
void TIM1_Config_PWM(void);

//------------------------------------------------------------------------------------//
void ADC_Init(void);
uint16_t ADC_Read(void);

*/


/**
 * @file main.c
 * @brief Gestion PWM, ADC, UART et timers pour commande d'une charge via STM32.
 */

/*-----------------------------------------------------------------------------------------------------------*/

/**
 * @brief Met à jour le rapport cyclique PWM en fonction de la tension mesurée.
 * @param digitalVoltage Valeur numérique de la tension mesurée
 */
void Update_PWM(uint16_t digitalVoltage);

/**
 * @brief Génère une table normalisée de sinus entre [0,1].
 */
void generate_normalized_sin_table(void);

/*-----------------------------------------------------------------------------------------------------------*/
/** @defgroup Fonctions_UART
 *  @brief Fonctions de communication UART2
 *  @{
 */

/**
 * @brief Initialise la communication UART2 (115200 bauds).
 */
void USART2_Init(void);

/**
 * @brief Envoie un caractère via UART2.
 * @param value Caractère à envoyer
 */
void USART2_SendChar(char value);

/**
 * @brief Envoie une chaîne de caractères via UART2.
 * @param msg Pointeur sur la chaîne de caractères
 */
void USART2_SendMsg(char *msg);
/** @} */

/*-----------------------------------------------------------------------------------------------------------*/
/** @defgroup Fonctions_Timer
 *  @brief Fonctions de configuration et gestion des timers
 *  @{
 */

/**
 * @brief Configure TIM1 en mode PWM pour générer les signaux de commande.
 */
void TIM1_Config_PWM(void);

/**
 * @brief Configure TIM2 pour générer des interruptions toutes les 1 ms.
 */
void TIM2_Config_IT(void);

/**
 * @brief Handler d'interruption du Timer 2.
 * Effectue la lecture ADC et met à jour le PWM.
 */
void TIM2_IRQHandler(void);
/** @} */

/*-----------------------------------------------------------------------------------------------------------*/
/** @defgroup Fonctions_ADC
 *  @brief Fonctions de configuration et lecture de l'ADC1
 *  @{
 */

/**
 * @brief Initialise l'ADC1 pour la conversion analogique-numérique.
 */
void ADC_Init(void);

/**
 * @brief Lance une conversion ADC et retourne la valeur convertie.
 * @return Valeur numérique de l'ADC
 */
uint16_t ADC_Read(void);
/** @} */

/*-----------------------------------------------------------------------------------------------------------*/
/** @defgroup Fonctions_Delay
 *  @brief Fonctions de génération de délais basés sur SysTick
 *  @{
 */

/**
 * @brief Génère un délai en millisecondes basé sur SysTick.
 * @param n Nombre de millisecondes
 */
void systickDelayMs(int n);

/**
 * @brief Génère un délai en microsecondes basé sur SysTick.
 * @param n Nombre de microsecondes
 */
void systickDelayUs(int n);
/** @} */


#endif /* INC_TER_SOFTWARE_H_ */
