#include "periph.h"
#include "flash_structures.h"

typedef struct
{
  uint16_t adc[8];
  uint32_t inputs;
} slave_state_typedef;

// Current system state
slave_state_typedef system_state[MAX_DEVICES];

uint8_t GetOutputsFor(uint8_t address, uint8_t *array)
{
  
}

void SetInputsFor(uint8_t address, uint8_t *array)
{
  
}

void UpdateMasterPeripherals(void)
{
  
}
