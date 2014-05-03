// Low-level SPI functions for RFT library
#ifndef __SPI_LOW_H__
#define __SPI_LOW_H__

#include <STM32F37x.h>
#include "spi_conf.h"

#define CS_ADDR   (PERIPH_BB_BASE + (CS_PORT_BASE + 0x14 - PERIPH_BASE)*32 + CS_PIN*4)
#define IRO_ADDR  (PERIPH_BB_BASE + (IRO_PORT_BASE + 0x10 - PERIPH_BASE)*32 + IRO_PIN*4)

static uint32_t *CS  	= (uint32_t*)CS_ADDR;
static uint32_t *IRO 	= (uint32_t*)IRO_ADDR;

typedef void (*SPI_RFT_low_cb_TypeDef)(uint16_t);

void SPI_RFT_Configure(SPI_RFT_low_cb_TypeDef pSpi_cb);
void SPI_RFT_Raw_Write(uint16_t data);

#endif
