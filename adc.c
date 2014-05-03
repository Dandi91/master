#include "adc.h"

uint16_t channels[8];

uint16_t ReadADC(uint8_t channel)
{
  return channels[channel];
}

void SDADC1_IRQHandler(void)
{
  uint32_t channel;
  uint16_t result;

  if (SDADC_GetITStatus(SDADC1,SDADC_IT_JEOC) == SET)
  {
    result = SDADC_GetInjectedConversionValue(SDADC1,&channel);
    channels[channel] = result;
  }
}

void ConfigureADCInterface(void)
{
  SDADC_AINStructTypeDef AINInit;
  GPIO_InitTypeDef Init;

  /* Clocks */
  RCC_SDADCCLKConfig(RCC_SDADCCLK_SYSCLK_Div4);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SDADC1,ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB,ENABLE);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOE,ENABLE);

  GPIO_StructInit(&Init);
  Init.GPIO_Mode = GPIO_Mode_AN;
  Init.GPIO_OType = GPIO_OType_OD;
  Init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  Init.GPIO_Speed = GPIO_Speed_10MHz;
  Init.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
  GPIO_Init(GPIOB,&Init);   // Input pins

  Init.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12;
  GPIO_Init(GPIOB,&Init);   // Input pins

  SDADC_AINStructInit(&AINInit);

  SDADC_VREFSelect(SDADC_VREF_Ext);   // Select VREFSD+ as reference

  SDADC_InitModeCmd(SDADC1,ENABLE);   // Initialization begins

  /* Configuration 0 for all channels */
  AINInit.SDADC_CommonMode = SDADC_CommonMode_VSSA;
  AINInit.SDADC_Gain = SDADC_Gain_1;
  AINInit.SDADC_InputMode = SDADC_InputMode_SEOffset;
  SDADC_AINInit(SDADC1,SDADC_Conf_0,&AINInit);

  /* Select using channels (0 - 7) */
  SDADC_InjectedChannelSelect(SDADC1,SDADC_Channel_0 | SDADC_Channel_1 |
                                     SDADC_Channel_2 | SDADC_Channel_3 |
                                     SDADC_Channel_4 | SDADC_Channel_5 |
                                     SDADC_Channel_6 | SDADC_Channel_7);

  /* Configure each channel to configuration 0 */
  SDADC_ChannelConfig(SDADC1,SDADC_Channel_0,SDADC_Conf_0);
  SDADC_ChannelConfig(SDADC1,SDADC_Channel_1,SDADC_Conf_0);
  SDADC_ChannelConfig(SDADC1,SDADC_Channel_2,SDADC_Conf_0);
  SDADC_ChannelConfig(SDADC1,SDADC_Channel_3,SDADC_Conf_0);
  SDADC_ChannelConfig(SDADC1,SDADC_Channel_4,SDADC_Conf_0);
  SDADC_ChannelConfig(SDADC1,SDADC_Channel_5,SDADC_Conf_0);
  SDADC_ChannelConfig(SDADC1,SDADC_Channel_6,SDADC_Conf_0);
  SDADC_ChannelConfig(SDADC1,SDADC_Channel_7,SDADC_Conf_0);

  SDADC_InjectedContinuousModeCmd(SDADC1,ENABLE);
  SDADC_ITConfig(SDADC1,SDADC_IT_JEOC,ENABLE);

  SDADC_InitModeCmd(SDADC1,DISABLE);   // Initialization ends
  NVIC_EnableIRQ(SDADC1_IRQn);
  SDADC_Cmd(SDADC1,ENABLE);

  /* Calibration */
  SDADC_CalibrationSequenceConfig(SDADC1,SDADC_CalibrationSequence_1);
  SDADC_StartCalibration(SDADC1);
  while (!SDADC_GetFlagStatus(SDADC1,SDADC_FLAG_EOCAL));

  /* Start conversion */
  SDADC_SoftwareStartInjectedConv(SDADC1);
}
