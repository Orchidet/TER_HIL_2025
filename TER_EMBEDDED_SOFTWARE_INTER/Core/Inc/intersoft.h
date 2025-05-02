/*
 * intersoft.h
 *
 *  Created on: Mar 24, 2025
 *      Author: HP
 */

#ifndef INC_INTERSOFT_H_
#define INC_INTERSOFT_H_

//Structure gestion de capture PWM

typedef struct PWM{

    uint32_t period_us;    // Période mesurée
    uint32_t high_time_us; // Temps à l'état HIGH
    float duty_cycle;      // Rapport cyclique (0-100%)
    uint8_t error_flags;   // Bitmask d'erreurs (voir defines)

}PWM;

void buff(void);
void TIM1_Init_Slave(void);
void EXTI_PA0_Init(void);
//--------------------------------------------------
void init_system_clock(void);
void GPIO_Init(void);
void systickDelayMs(int n);
void systickDelayUs(int n);
void TIM1_Config_PWMInputCaptureMode(void);
void TIM2_Init(void);
void DAC_Init();
void DAC_Write(uint16_t value);
void DMA_Init(void);
void USART2_Init(void);
void USART2_SendMsg(char *msg);
int string_to_int(char *s);
void USART2_SendChar(char value);
void USART2_receive_string(char *buffer, uint16_t buffer_size);


#endif /* INC_INTERSOFT_H_ */
