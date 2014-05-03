// Initialization functions

#ifndef __INIT_H__
#define __INIT_H__

#include <STM32F37x.h>

#include "in_logic.h"
#include "out_logic.h"
#include "dac.h"
#include "adc.h"

#define RTC_ADDR_FLAGS  (RTC_BASE + 0x50 + 0)
#define RTC_ADDR_SLADR  (RTC_BASE + 0x50 + 1)
#define RTC_ADDR_AESKEY (RTC_BASE + 0x50 + 2)

typedef union
{
  uint8_t d8;
  struct
  {
    uint8_t adc_enabled:     1;
    uint8_t dac_enabled:     1;
    uint8_t input_enabled:   1;
    uint8_t output_enabled:  1;
    uint8_t reserved:        1;
  } b;
} peripheralParams_TypeDef;

peripheralParams_TypeDef GetPeripheralParams(void);
void Initialization(uint8_t is_config);

#endif
