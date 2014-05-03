// DAC outputs functions

#ifndef __DAC_H__
#define __DAC_H__

#include <STM32F37x.h>

void WriteDACs(uint16_t* data);
void ConfigureDACInterface(void);

#endif
