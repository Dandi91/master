// Low-level SPI functions for RFT library

#include "spi_low.h"

SPI_RFT_low_cb_TypeDef Spi_cb;

void SPI_RFT_Configure(SPI_RFT_low_cb_TypeDef pSpi_cb)
{
	SPI_InitTypeDef InitStructure;

  Spi_cb = pSpi_cb;

	SPI_StructInit(&InitStructure);

	InitStructure.SPI_CPHA = SPI_CPHA_1Edge;												// SPI(0,0)
	InitStructure.SPI_CPOL = SPI_CPOL_Low;
	InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;	// 1.5 MHz
	InitStructure.SPI_DataSize = SPI_DataSize_16b;									// 16 bit
	InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;	// Full-duplex
	InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;									// MSB first
	InitStructure.SPI_Mode = SPI_Mode_Master;
	InitStructure.SPI_NSS = SPI_NSS_Soft;
	SPI_Init(SPIx,&InitStructure);
	SPI_NSSInternalSoftwareConfig(SPIx,SPI_NSSInternalSoft_Set);
	SPI_Cmd(SPIx,ENABLE);

  SPI_I2S_ITConfig(SPIx,SPI_I2S_IT_RXNE,ENABLE);
  NVIC_EnableIRQ(SPIx_IRQn);

	*CS = 1;
}

void SPI_RFT_Raw_Write(uint16_t data)
{
  while (SPI_I2S_GetFlagStatus(SPIx,SPI_I2S_FLAG_TXE) != SET);  // Foal check
	*CS = 0;
	SPI_I2S_SendData16(SPIx,data);
}

void SPI1_IRQHandler(void)
{
	uint16_t result;
  if (SPI_I2S_GetFlagStatus(SPIx,SPI_I2S_FLAG_RXNE) == SET) // Word has been received
	{
		*CS = 1;
		result = SPI_I2S_ReceiveData16(SPIx);
    Spi_cb(result);
	}
}
