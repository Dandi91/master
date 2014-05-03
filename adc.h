// ADC inputs functions

#ifndef __ADC_H__
#define __ADC_H__

#include <STM32F37x.h>

uint16_t ReadADC(uint8_t channel);
void ConfigureADCInterface(void);

#endif
