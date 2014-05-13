#include "stm32f37x.h"                  // Keil::Device:Startup
#include "init.h"
#include "rft_handlers.h"

uint8_t is_config = 0;

int main(void)
{
  GPIO_InitTypeDef Init;

  /* Checking jumper */

  /* Initializing PA0 configuration jumper detect */
  GPIO_StructInit(&Init);
  Init.GPIO_Mode = GPIO_Mode_IN;
  Init.GPIO_OType = GPIO_OType_OD;
  Init.GPIO_PuPd = GPIO_PuPd_UP;
  Init.GPIO_Speed = GPIO_Speed_10MHz;
  Init.GPIO_Pin = GPIO_Pin_0;
  GPIO_Init(GPIOA,&Init);

  /* Pin check */
  if (!GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0))
    is_config = 1;

  if (!is_config && !(RCC->BDCR & RCC_BDCR_RTCEN))
    /* Normal mode can't be used without RTC. Enter infinite cycle.
        If it's the first running, you should configure this controller over the USB first. */
    while (1);

  Initialization(is_config);

  if (is_config)
    while (1);  // Configuration through USB interrupts

  /* Actual work */
  InitTopology();   // Net topology
  InitSignal();     // Initialization signal
  while (1)
  {
    SystemPoll();   // Main cycle
  }
}
