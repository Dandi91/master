#include "init.h"

#include "mrf49xa.h"
#include "crypto_functions.h"
#include "rft_handlers.h"

#include "usbd_hid_core.h"
#include "usbd_usr.h"
#include "usbd_desc.h"
#include "pre_config.h"

peripheralParams_TypeDef PeripheralParams;
USB_OTG_CORE_HANDLE USB_OTG_dev;

peripheralParams_TypeDef GetPeripheralParams(void)
{
  return PeripheralParams;
}

SPI_RFT_cb_TypeDef RFT_cb =
{
  NULL,         // on empty buffer
  TX_Complete,  // on TX complete
  RX_Complete,  // on RX complete
  RX_Begin,     // on RX begin
  NULL,         // on event occured
  NULL          // on full buffer
};

void RFTPortsInit(void)
{
  GPIO_InitTypeDef Init;
  EXTI_InitTypeDef InitStructure;

  GPIO_StructInit(&Init);
  Init.GPIO_Mode = GPIO_Mode_AF;
  Init.GPIO_OType = GPIO_OType_PP;
  Init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  Init.GPIO_Speed = GPIO_Speed_10MHz;
  Init.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7; // SDI, SCK
  GPIO_Init(GPIOA,&Init);

  GPIO_PinAFConfig(GPIOA,GPIO_PinSource5,GPIO_AF_5);  // SCK
  GPIO_PinAFConfig(GPIOA,GPIO_PinSource7,GPIO_AF_5);  // SDI

  Init.GPIO_OType = GPIO_OType_OD;
  Init.GPIO_PuPd = GPIO_PuPd_UP;
  Init.GPIO_Pin = GPIO_Pin_6;   // SDO
  GPIO_Init(GPIOA,&Init);

  GPIO_PinAFConfig(GPIOA,GPIO_PinSource6,GPIO_AF_5);  // SDO

  Init.GPIO_Mode = GPIO_Mode_IN;
  Init.GPIO_Pin = GPIO_Pin_15;
  GPIO_Init(GPIOA,&Init);        // IRO

  Init.GPIO_Mode = GPIO_Mode_OUT;
  Init.GPIO_OType = GPIO_OType_PP;
  Init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  Init.GPIO_Pin = GPIO_Pin_9;
  GPIO_Init(GPIOA,&Init);        // CS

  GPIO_SetBits(GPIOA,GPIO_Pin_9);   // CS high

  /* Configure IRO interrupt on PA15 */
  EXTI_StructInit(&InitStructure);

  InitStructure.EXTI_Line = EXTI_Line15;
  InitStructure.EXTI_LineCmd = ENABLE;
  InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
  InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
  EXTI_Init(&InitStructure);

  SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA,EXTI_PinSource15);

  NVIC_EnableIRQ(EXTI15_10_IRQn);
  if (!GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_15))
    EXTI_GenerateSWInterrupt(EXTI_Line15);
}

void RFTInit(void)
{
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1,ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC,ENABLE);

  SPI_RFT_Init(&RFT_cb);

  SPI_RFT_Set_FIFO_Fill_Count(8);
  SPI_RFT_Set_Flags(SPI_RFT_Register_FIFORSTREG,DRSTM);
  SPI_RFT_Restart_Synch_Pattern();
  SPI_RFT_Set_Frequency_Band(SPI_RFT_Freq_Band_433_MHz);                                // 433 MHz base
  SPI_RFT_Set_Load_Capacitance(SPI_RFT_Load_Cap_160);                                   // 16 pF Quarz
  SPI_RFT_Reset_Flags(SPI_RFT_Register_AFCCREG,ARFO_1 | ARFO_0);                        // no restriction for FSK
  SPI_RFT_Set_Modulation_Bandwidth(SPI_RFT_Modulation_Bandwidth_120kHz);                // ±120 kHz for FSK
  SPI_RFT_Set_Center_Frequency(1200);                                                   // centered on 433 MHz
  SPI_RFT_Set_Receiver_Baseband_Bandwidth(SPI_RFT_Receiver_Baseband_Bandwidth_270kHz);  // 270 kHz for FSK 120 kHz * 2
  SPI_RFT_Set_Digital_RSSI_Threshold(SPI_RFT_Digital_RSSI_Threshold_85dB);              // -85 dB threshold for DRSSI
  SPI_RFT_Set_Data_Quality_Threshold(5);
  SPI_RFT_Set_Flags(SPI_RFT_Register_BBFCREG,FTYPE);                                    // RC-low pass filer
  SPI_RFT_Set_Synch_Byte(0x6A);
  SPI_RFT_Set_Data_Rate(0);                                                             // 256 kbps
  SPI_RFT_Set_Flags(SPI_RFT_Register_PMCREG,CLKOEN);                                    // Disable clock output

  SPI_RFT_Enable_Transmitter();                                                         // Tune in antenna
  while (SPI_RFT_Get_Status() != SPI_RFT_TX_On) {};

  SPI_RFT_Start_Polling();                                                                // Start polling

  RFTPortsInit();
}

void Initialization(uint8_t is_config)
{
  TIM_TimeBaseInitTypeDef Init;

  if (is_config)
  {
    // Reset and run backup domain
    RunRTC();
    // USB configuration
    USBD_Init(&USB_OTG_dev,USB_OTG_FS_CORE_ID,&USR_desc,&USBD_HID_cb,&USR_cb);
  }
  else
  {
    /* Normal work */
    // Reading backuped configuration
    PeripheralParams.d8 = *(uint8_t*)RTC_ADDR_FLAGS;
    Initialize_AES_Key((uint8_t*)RTC_ADDR_AESKEY);

    // Configuring interfaces
    if (PeripheralParams.b.adc_enabled)
      ConfigureADCInterface();
    if (PeripheralParams.b.dac_enabled)
      ConfigureDACInterface();
    if (PeripheralParams.b.input_enabled)
      ConfigureInputInterface();
    if (PeripheralParams.b.output_enabled)
      ConfigureOutputInterface();

    // Initializing wireless module
    RFTInit();

    // Initializing interrupts' priorities
    NVIC_SetPriority(SDADC1_IRQn,0);
    NVIC_SetPriority(SPI1_IRQn,1);
    NVIC_SetPriority(EXTI15_10_IRQn,2);
    NVIC_SetPriority(TIM7_IRQn,3);

    // Configuring wakeup pins
    PWR_WakeUpPinCmd(PWR_WakeUpPin_1,DISABLE);
    PWR_WakeUpPinCmd(PWR_WakeUpPin_2,DISABLE);
    PWR_WakeUpPinCmd(PWR_WakeUpPin_3,DISABLE);

    // Configure transmission timeout timer
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7,ENABLE);

    TIM_TimeBaseStructInit(&Init);
    Init.TIM_Prescaler = 48000; // 24000000 / 48000 = 500 Hz
    Init.TIM_Period = TRMS_TIMEOUT_PERIOD;
    TIM_TimeBaseInit(TIM7,&Init);

    TIM_ITConfig(TIM7,TIM_IT_Update,ENABLE);
    NVIC_EnableIRQ(TIM7_IRQn);
  }
}
