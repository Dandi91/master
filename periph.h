// Common functions for master peripheral and state table

#ifndef __PERIPH_H__
#define __PERIPH_H__

#include <STM32F37x.h>

#include "in_logic.h"
#include "out_logic.h"
#include "dac.h"
#include "adc.h"

#include "mrf49xa.h"

uint8_t GetOutputsFor(uint8_t address, uint8_t *array);
void SetInputsFor(uint8_t address, uint8_t *array, data_len_t length);
void UpdateMasterPeripherals(void);

#endif
