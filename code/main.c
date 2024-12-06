/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*                        The Embedded Experts                        *
**********************************************************************

-------------------------- END-OF-HEADER -----------------------------

File    : main.c
Purpose : Generic application start

*/

#include "stm32l476xx.h"
#include <stdio.h>

/*
  ADC_ISR
  Bit3: EOS
  Bit2: EOC
  Bit0: ADRDY
*/

int ADC_init(ADC_TypeDef *ADC);
int LSE_init(void);
int MSIPLL(void);
int PLL_init(void);
int set_80MHz(void);


/*********************************************************************
*
*       main()
*
*  Function description
*   Application entry point.
*/
int main(void) {
  int i;
  int x = 0;

  i = 100000;
  while (--i) {
  
  }

  //printf("starting");

  x = LSE_init() || MSIPLL();
  PLL_init();
 
  RCC->AHB2ENR |= 1 | 1 << 13;
  RCC->APB1ENR1 |= 1 << 29;
  i=100;
  while (--i) {}

  // Set the system clock as the ADC clock
  RCC->CCIPR |= 0b11 << RCC_CCIPR_ADCSEL_Pos;

  GPIOA->MODER |= 0b11;
  GPIOA->MODER |= 0b11 << GPIO_MODER_MODE4_Pos;

  GPIOA->MODER |= (0b11 << 8); // Set GPIO4 to analog
  GPIOA->MODER &= ~(0b10 << GPIO_MODER_MODE5_Pos);
  GPIOA->ODR |= GPIO_ODR_OD5;
  GPIOA->MODER |= 0b11; // Set GPIOA0 to analog mode
  GPIOA->ASCR |= 1; // Connect GPIOA0 to the ADC

  // Set the MCO
  GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED8;
  GPIOA->MODER &= ~(GPIO_MODER_MODE8);
  GPIOA->MODER |= 0b10 << GPIO_MODER_MODE8_Pos;
  RCC->CFGR |= 1 << RCC_CFGR_MCOSEL_Pos;

  //ADC123_COMMON->CCR |= ADC_CCR_CKMODE_Pos;

  ADC_init(ADC2);

  // CONT, OVR MOD
  //ADC2->CFGR |= 0b11 << 12;

  // select channel 5
  ADC2->SQR1 |= 5 << 6;

  // start ADC
  //ADC2->CR |= 4;

  ADC2->CFGR2 |= 2 << ADC_CFGR2_OVSS_Pos;
  ADC2->CFGR2 |= 0b101 << ADC_CFGR2_OVSR_Pos;
  ADC2->CFGR2 |= ADC_CFGR2_ROVSE;

  //start DAC
  DAC->CR |= 1;

  while (1) {
    if (ADC2->ISR & ADC_ISR_EOS) {
      ADC2->ISR |= ADC_ISR_EOS;
      x = ADC2->DR;
      DAC->DHR12L1 = x;
    }
    if ((ADC2->CR & (ADC_CR_ADSTART | ADC_CR_ADDIS | ADC_CR_ADEN)) == ADC_CR_ADEN) {
      ADC2->CR |= ADC_CR_ADSTART;
    }
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

int LSE_init(void) {
  int i;
  if (!(RCC->APB1ENR1 & RCC_APB1ENR1_PWREN)) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    i = 2;
    while (i--) {}
  }
  if (!(PWR->CR1 & PWR_CR1_DBP))
    PWR->CR1 |= PWR_CR1_DBP;
  if (!(RCC->BDCR & RCC_BDCR_LSEON) || ((RCC->BDCR & RCC_BDCR_LSEDRV) != 0b11000)) {
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    while (RCC->BDCR & RCC_BDCR_LSERDY) {}
    
    RCC->BDCR |= 0b11 << RCC_BDCR_LSEDRV_Pos;
    RCC->BDCR |= RCC_BDCR_LSEON;
    while (!(RCC->BDCR & RCC_BDCR_LSERDY)) {}
  }
  return 0;
}

int MSIPLL(void) {
  if (!(RCC->BDCR & (RCC_BDCR_LSEON | RCC_BDCR_LSERDY)))
    return -1;
  RCC->CR |= RCC_CR_MSIPLLEN;
  return 0;
}

// If the MSI is set to 4MHz (default), set the pll to 80MHz and chose PLL as system clock source
int PLL_init(void) {
  if (RCC->CR & (RCC_CR_PLLSAI1ON | RCC_CR_PLLSAI2ON)) return -1;
  if ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL) return -1;
  RCC->CR &= ~RCC_CR_PLLON;
  while (RCC->CR & RCC_CR_PLLRDY) {}
  RCC->PLLCFGR = 0; // Hopefully this is fine, since PLLN gets set to a valid state before the PLL is reactivated
  RCC->PLLCFGR |= 24 << RCC_PLLCFGR_PLLN_Pos; // 40 * 4MHz = 160MHz. 160MHz / 2 = 80MHz
  RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_MSI_Msk;
  RCC->CR |= RCC_CR_PLLON;
  RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN;
  while (!(RCC->CR & RCC_CR_PLLRDY)) {}
  RCC->CFGR |= RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
  return 0;
}

int set_80MHz(void) {
  PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS_Msk) | (1 << PWR_CR1_VOS_Pos);
  while (!(PWR->SR2 & PWR_SR2_VOSF));
  FLASH->ACR |= 


/*************************** End of file ****************************/
