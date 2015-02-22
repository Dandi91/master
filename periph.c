#include "periph.h"

#include "flash_structures.h"
#include "init.h"
#include "rft_handlers.h"

typedef struct
{
  uint16_t adc[8];
  uint32_t inputs;
} slave_state_typedef;

// Current system state
slave_state_typedef system_state[MAX_DEVICES];

void GetOutputs(uint16_t *p, data_len_t *len)
{
  uint8_t i;

  len = 0;
  if (GetPeripheralParams().b.adc_enabled)
  {
    for (i = 0; i < 8; i++)
      *p++ = ReadADC(i);
    len += 16;
  }
  if (GetPeripheralParams().b.input_enabled)
  {
    *(uint32_t*)p = GetLogicInputs();
    len += 4;
  }
}

void SetOutputs(uint16_t *p)
{
  if (GetPeripheralParams().b.dac_enabled)
  {
    WriteDACs(p);
    p += 16;
  }
  if (GetPeripheralParams().b.output_enabled)
    SetLogicOutputs(*(uint32_t*)p);
}

uint8_t GetOutputsFor(uint8_t address, uint8_t *array)
{
  slave_state_typedef state;
  conn_typedef conn;
  uint8_t a, d;
  uint16_t i;

  for (i = 0; i < get_wires_count(); i++)
  {
    conn = get_connection(i);
    if (conn.out_addr == address)
    {
      if (conn.out_number < 8)        // [0..7] are for ADCs/DACs
      {
        state.adc[conn.out_number] = system_state[conn.in_addr].adc[conn.in_number];
        a = 1;
      }
      else if (conn.out_number > 7)   // [8..39] are for digital inputs
      {
        if (system_state[conn.in_addr].inputs & (1 << (conn.in_number - 8)))
          state.inputs |= 1 << (conn.out_number - 8);
        else
          state.inputs &= ~(1 << (conn.out_number - 8));
        d = 1;
      }
    }
  }
  if (a == 1)
  {
    for (i = 0; i < 8; i++)
    {
      *(uint16_t*)array = state.adc[i];
      array += 2;
    }
  }
  if (d == 1)
    *(uint32_t*)array = state.inputs;

  if (a + d == 2)
    return 20;
  else if (a == 1)
    return 16;
  else if (d == 1)
    return 4;
  else
    return 0;
}

void SetInputsFor(uint8_t address, uint8_t *array, data_len_t length)
{
  uint8_t i;

  if (length == 4)
    system_state[address].inputs = *(uint32_t*)array;
  else if (length >= 16)
    for (i = 0; i < 8; i++)
      system_state[address].adc[i] = ((uint16_t*)array)[i];
  if (length == 20)
    system_state[address].inputs = *(uint32_t*)(array + 16);
}

void UpdateMasterPeripherals(void)
{
  uint8_t state[20];
  data_len_t len;

  GetOutputsFor(DEFAULT_MASTER_ADDRESS,state);
  SetOutputs((uint16_t*)state);

  GetOutputs((uint16_t*)state,&len);
  SetInputsFor(DEFAULT_MASTER_ADDRESS,state,len);
}
