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
#include <math.h>

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
void gen_coefs(float fc);
int process_sample(int val);

const float PI = 3.141592653589793;
const int COEFS_M = 30;
float coefs[31] = {0};

const int BUFFER_SIZE = 32;
const int BUFFER_LEN = 31;
int bindex = 0;
float buffer[32] = {0};


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

  //printf("starting");

  x = set_80MHz();
 
  RCC->AHB2ENR |= 1 | 1 << 13;
  RCC->APB1ENR1 |= RCC_APB1ENR1_DAC1EN | RCC_APB1ENR1_TIM2EN;
  i=100;
  while (--i) {}

  // Set the system clock as the ADC clock
  RCC->CCIPR |= 0b11 << RCC_CCIPR_ADCSEL_Pos;

  // Set the led on the nucleo board
  //GPIOA->MODER &= ~(0b10 << GPIO_MODER_MODE5_Pos);
  //GPIOA->ODR |= GPIO_ODR_OD5;

  // set gpio pins for adc and dac
  GPIOA->MODER |= GPIO_MODER_MODE4; // Set GPIO4 to analog
  GPIOA->MODER |= 0b11; // Set GPIOA0 to analog mode
  GPIOA->ASCR |= 1; // Connect GPIOA0 to the ADC

  // Set the MCO
  GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEED8;
  GPIOA->MODER &= ~(GPIO_MODER_MODE8);
  GPIOA->MODER |= 0b10 << GPIO_MODER_MODE8_Pos;
  RCC->CFGR |= 1 << RCC_CFGR_MCOSEL_Pos;

  ADC_init(ADC2);

  // CONT, OVR MOD
  //ADC2->CFGR |= 0b11 << 12;

  // select channel 5
  ADC2->SQR1 |= 5 << 6;

  // start ADC
  //ADC2->CR |= 4;

  // ADC oversample
  ADC2->CFGR2 |= 2 << ADC_CFGR2_OVSS_Pos; // oversample shift - 2 bits
  ADC2->CFGR2 |= 0b101 << ADC_CFGR2_OVSR_Pos; // overample ratio - 64x
  ADC2->CFGR2 |= ADC_CFGR2_ROVSE; // oversample enable

  //start DAC
  DAC->CR |= 1;

  // timer for sampling
  TIM1->ARR = 2667; // 80MHz / 2667 = approx. 30kHz, 15kHz Nyquist
  TIM1->CR1 |= TIM_CR1_CEN;

  while (1) {
    if ((TIM1->SR) & TIM_SR_UIF) {
      TIM1->SR &= ~TIM_SR_UIF;
      x = ADC2->DR;
      while(ADC2->CR & ADC_CR_ADSTART);
      ADC2->CR |= ADC_CR_ADSTART;
      DAC->DHR12L1 = process_sample(x) & ~0b1111;
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

// initialize the lse crystal
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

// trim the msi to the lse
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
  RCC->PLLCFGR |= 40 << RCC_PLLCFGR_PLLN_Pos; // 40 * 4MHz = 160MHz. 160MHz / 2 = 80MHz
  RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_MSI_Msk;
  RCC->CR |= RCC_CR_PLLON;
  RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN;
  while (!(RCC->CR & RCC_CR_PLLRDY)) {}
  RCC->CFGR |= RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}
  return 0;
}

int set_80MHz(void) {
  // reference manual 3.3.3
  PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS_Msk) | (1 << PWR_CR1_VOS_Pos);
  while (PWR->SR2 & PWR_SR2_VOSF);
  FLASH->ACR |= 4;
  if ((FLASH->ACR & FLASH_ACR_LATENCY) != 4)
    return -1;
  return LSE_init() || MSIPLL() || PLL_init();
}

// fc is fraction of sampling frequency from 0 to 0.5
// generates coefficients for a windowed-sinc low-pass filter with the given cutoff frequency
// The Scientist & Engineer's Guide to Digital Signal Processing, chapter 16
void gen_coefs(float fc) {
  int i;
  int rel;
  int midpoint = COEFS_M / 2;
  float c1a = 2 * PI * COEFS_M;
  float c2a = 2 * c1a;
  float ang_freq = 2 * PI * fc;
  if (fc < 0 || fc > 0.5)
    return;
  for (i = 0; i <= COEFS_M; ++i) {
    if (i == midpoint)
      coefs[COEFS_M - i] = 2 * PI * fc;
    else {
      rel = i - midpoint;
      coefs[COEFS_M - i] = sinf(midpoint * rel) * (0.42 - 0.5 * cosf(c1a * i) + 0.08 * cosf(c2a * i)) / rel;
    }
  }
}

// adds the sample to the circular buffer and runs the filter to get the next output value
int process_sample(int val) {
  int i;
  float acc = 0;
  float sample = (float)val / 65536;
  buffer[(bindex + BUFFER_LEN) & 32] = sample;
  ++bindex;
  for (i = 0; i < BUFFER_LEN; ++i) {
    acc += coefs[i] * buffer[(bindex + i) % 32];
  }
  return (int)(65536 * acc);
}

/*************************** End of file ****************************/
