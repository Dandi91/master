#include "dac.h"

#define DAC_ADDR_NOP  0x00
#define DAC_ADDR_DR   0x01
#define DAC_ADDR_RB   0x02
#define DAC_ADDR_CTR  0x55
#define DAC_ADDR_RST  0x56

#define DAC_READ_ADDR_SR  0x00
#define DAC_READ_ADDR_DR  0x01
#define DAC_READ_ADDR_CR  0x02

typedef struct
{
  uint8_t address;
  uint16_t data;
} DAC_Frame;

void Write_DAC_Low(DAC_Frame* buffer)
{
  uint8_t i, *temp;

  GPIO_ResetBits(GPIOA,GPIO_Pin_4);  // LATCH
  while (SPI_I2S_GetFlagStatus(SPI2,SPI_I2S_FLAG_TXE) != SET) {};

  temp = (uint8_t*)buffer;
  for (i = 0; i < 24; i++)
  {
    SPI_SendData8(SPI2,*temp++);
    while (SPI_I2S_GetFlagStatus(SPI2,SPI_I2S_FLAG_RXNE) != SET);
  }

  GPIO_SetBits(GPIOA,GPIO_Pin_4); // LATCH
}

void WriteDACs(uint16_t* data)
{
  // Data 16-byte length
  DAC_Frame buffer[8];
  uint8_t i;

  for (i = 0; i < 8; i++)
  {
    buffer[i].address = DAC_ADDR_DR;
    buffer[i].data = *data++;
  }
  Write_DAC_Low(buffer);
}

void InitializeDACs(void)
{
  DAC_Frame buffer[8];
  uint8_t i;

  for (i = 0; i < 8; i++)
  {
    buffer[i].address = DAC_ADDR_CTR;
    buffer[i].data = 0x100E;
  }
  Write_DAC_Low(buffer);
}

void ConfigureDACInterface(void)
{
  // APB1 Clock 16 MHz
  // Clock < 12 MHz (24/2=12 MHz)
  // MSB First
  // SPI(0,0), 8 bit

  SPI_InitTypeDef Init;
  GPIO_InitTypeDef PinInit;

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2,ENABLE);

  SPI_StructInit(&Init);
  Init.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
  Init.SPI_CPHA = SPI_CPHA_1Edge;
  Init.SPI_CPOL = SPI_CPOL_Low;
  Init.SPI_DataSize = SPI_DataSize_8b;
  Init.SPI_Direction = SPI_Direction_1Line_Tx;
  Init.SPI_FirstBit = SPI_FirstBit_MSB;
  Init.SPI_Mode = SPI_Mode_Master;
  Init.SPI_NSS = SPI_NSS_Soft;

  SPI_Init(SPI2,&Init);
  SPI_NSSInternalSoftwareConfig(SPI2,SPI_NSSInternalSoft_Set);

  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA,ENABLE);

  // Pins
  GPIO_StructInit(&PinInit);
  PinInit.GPIO_Mode = GPIO_Mode_AF;
  PinInit.GPIO_OType = GPIO_OType_PP;
  PinInit.GPIO_PuPd = GPIO_PuPd_NOPULL;
  PinInit.GPIO_Speed = GPIO_Speed_10MHz;
  PinInit.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_10; // SDI, SCK
  GPIO_Init(GPIOA,&PinInit);

  GPIO_PinAFConfig(GPIOA,GPIO_PinSource8,GPIO_AF_5);  // SCK
  GPIO_PinAFConfig(GPIOA,GPIO_PinSource10,GPIO_AF_5); // SDI

  PinInit.GPIO_Mode = GPIO_Mode_OUT;
  PinInit.GPIO_Pin = GPIO_Pin_4;
  GPIO_Init(GPIOA,&PinInit);        // LATCH

  SPI_Cmd(SPI2,ENABLE);

  InitializeDACs();
}
