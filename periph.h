// Common functions for master peripheral and state table

#ifndef __PERIPH_H__
#define __PERIPH_H__

#include <STM32F37x.h>

uint8_t GetOutputsFor(uint8_t address, uint8_t *array);
void SetInputsFor(uint8_t address, uint8_t *array);
void UpdateMasterPeripherals(void);

#endif
