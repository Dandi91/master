#include "out_logic.h"

void SetLogicOutputs(uint32_t value)
{
  uint16_t* buffer;

  GPIO_ResetBits(GPIOA,GPIO_Pin_2);    // CS
  buffer = (uint16_t*)&value;

  while (SPI_I2S_GetFlagStatus(SPI3,SPI_I2S_FLAG_RXNE) != SET);
  SPI_I2S_SendData16(SPI3,*buffer++);

  while (SPI_I2S_GetFlagStatus(SPI3,SPI_I2S_FLAG_RXNE) != SET);
  SPI_I2S_SendData16(SPI3,*buffer);

  while (SPI_I2S_GetFlagStatus(SPI3,SPI_I2S_FLAG_RXNE) != SET);

  GPIO_SetBits(GPIOA,GPIO_Pin_2);      // CS
}

void ConfigureOutputInterface(void)
{
  // APB1 Clock 16 MHz
  // Clock < 2 MHz (24/16=1.5 MHz)
  // MSB First
  // SPI(0,0), 16 bit

  SPI_InitTypeDef Init;
  GPIO_InitTypeDef PinInit;

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI3,ENABLE);

  SPI_StructInit(&Init);
  Init.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
  Init.SPI_CPHA = SPI_CPHA_1Edge;
  Init.SPI_CPOL = SPI_CPOL_Low;
  Init.SPI_DataSize = SPI_DataSize_16b;
  Init.SPI_Direction = SPI_Direction_1Line_Tx;
  Init.SPI_FirstBit = SPI_FirstBit_MSB;
  Init.SPI_Mode = SPI_Mode_Master;
  Init.SPI_NSS = SPI_NSS_Soft;

  SPI_Init(SPI3,&Init);
  SPI_NSSInternalSoftwareConfig(SPI3,SPI_NSSInternalSoft_Set);

  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA,ENABLE);

  // Pins
  GPIO_StructInit(&PinInit);
  PinInit.GPIO_Mode = GPIO_Mode_AF;
  PinInit.GPIO_OType = GPIO_OType_PP;
  PinInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
  PinInit.GPIO_Speed = GPIO_Speed_10MHz;
  PinInit.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_3; // SDI, SCK
  GPIO_Init(GPIOA,&PinInit);

  GPIO_PinAFConfig(GPIOA,GPIO_PinSource1,GPIO_AF_6);  // SCK
  GPIO_PinAFConfig(GPIOA,GPIO_PinSource3,GPIO_AF_6);  // SDI

  PinInit.GPIO_Mode = GPIO_Mode_OUT;
  PinInit.GPIO_Pin = GPIO_Pin_2;
  GPIO_Init(GPIOA,&PinInit);        // CS

  SPI_Cmd(SPI3,ENABLE);
}
