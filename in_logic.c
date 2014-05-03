#include "in_logic.h"

uint32_t GetLogicInputs(void)
{
  uint8_t buffer[4] = {0,0,0,0};
  uint8_t input_index, i;

  for (input_index = 0; input_index < 8; input_index++)
  {
    GPIO_Write(GPIOD,input_index << 4);  // set channel index (A-B-C)
    for (i = 0; i < 4; i++)
    {
      if (GPIO_ReadInputDataBit(GPIOD,1 << i))
        buffer[i] &= 1 << input_index;
    }
  }
  return *(uint32_t*)&buffer[0];
}

void ConfigureInputInterface(void)
{
  GPIO_InitTypeDef Init;

  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOD,ENABLE);

  GPIO_StructInit(&Init);
  Init.GPIO_Mode = GPIO_Mode_IN;
  Init.GPIO_OType = GPIO_OType_OD;
  Init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  Init.GPIO_Speed = GPIO_Speed_10MHz;
  Init.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
  GPIO_Init(GPIOD,&Init);   // Input pins

  Init.GPIO_Mode = GPIO_Mode_OUT;
  Init.GPIO_OType = GPIO_OType_PP;
  Init.GPIO_Speed = GPIO_Speed_10MHz;
  Init.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
  GPIO_Init(GPIOD,&Init);   // Output controls
}
