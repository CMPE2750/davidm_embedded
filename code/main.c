/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*                        The Embedded Experts                        *
**********************************************************************

-------------------------- END-OF-HEADER -----------------------------

File    : main.c
Purpose : Generic application start

*/

#include "stm32l476xx.h"

/*
  ADC_ISR
  Bit3: EOS
  Bit2: EOC
  Bit0: ADRDY
*/

int ADC_init(ADC_TypeDef *ADC);


/*********************************************************************
*
*       main()
*
*  Function description
*   Application entry point.
*/
int main(void) {
  int i;
  int x;
 
  RCC->AHB2ENR |= 1 | 1 << 13;
  RCC->APB1ENR1 |= 1 << 29;
  i=100;
  while (--i) {}

  // Set the system clock as the ADC clock
  RCC->CCIPR |= 0b11 << RCC_CCIPR_ADCSEL_Pos;

  GPIOA->MODER |= (0b11 << 8);
  GPIOA->MODER &= ~(0b10 << GPIO_MODER_MODE5_Pos);
  GPIOA->ODR |= GPIO_ODR_OD5;
  GPIOA->ASCR |= 1<<4;

  //ADC123_COMMON->CCR |= ADC_CCR_CKMODE_Pos;

  ADC_init(ADC2);

  // CONT, OVR MOD
  ADC2->CFGR |= 0b11 << 12;

  // select channel 5
  ADC2->SQR1 |= 5 << 6;

  // start ADC
  ADC2->CR |= 4;

  //start DAC
  DAC->CR |= 1;

  while (1) {
    x = ADC2->DR;
    DAC->DHR12L1 = x;
  }

}


int ADC_init(ADC_TypeDef *ADC) {
  int i;

  if ((ADC->CR) & ((1 << 31) | 0b111111)) return -1;

  

  // ADC power on
  ADC->CR &= ~(1<<29);
  ADC->CR |= (1<<28);
  for (i=0; i<1600; ++i) {}

  // ADC calibration
  ADC->CR &= ~(1<<30);
  ADC->CR |= 1<<31;
  while (ADC->CR & (1<<31));
  for (i=0; i<4; ++i);

  // Enable ADC
  ADC->ISR |= 1;
  ADC->CR |= 1;
  while(!(ADC->ISR & 1));
  ADC->ISR |= 1;

  return 0;
}


/*************************** End of file ****************************/
