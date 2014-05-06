#include "pre_config.h"
#include "flash_structures.h"

#include "usbd_ioreq.h"
#include "usbd_hid_core.h"

/* Backup structure:
  uint8_t       Flags
  uint8_t       Slave Address
  uint8_t [16]  AES Key
*/

/* Wires structure
  4 * 40 * 255 bytes
*/

uint8_t buffer[BCKP_STRUCT_LENGTH + 1];

uint32_t current_wire;

void Process_USB_Get_Request(USB_SETUP_REQ *req)
{
  uint16_t i, *dest, *source;

  if ((req->wValue & 0xFF) == REPORT_MEMORY)
  {
    source = (uint16_t*)(RTC_BASE + 0x50);
    dest = (uint16_t*)&buffer[1];

    for (i = 0; i < BCKP_STRUCT_LENGTH; i++)
      *dest++ = *source++;

    buffer[0] = REPORT_MEMORY;
    USBD_CtlSendData(&USB_OTG_dev,buffer,sizeof(buffer));
  }
}

void EP0_Data_Ready(void)
{
  uint16_t i, *dest, *source;

  switch (buffer[0])
  {
    case REPORT_MEMORY:
    {
      dest = (uint16_t*)(RTC_BASE + 0x50);
      source = (uint16_t*)&buffer[1];

      for (i = 0; i < BCKP_STRUCT_LENGTH; i++)
        *dest++ = *source++;

      current_wire = (uint32_t)get_connections_table_address();
      break;
    }
    case REPORT_WIRES:
    {
      FLASH_ProgramWord(current_wire,*(uint32_t*)&buffer[1]);
      current_wire += 4;
      break;
    }
  }
  buffer[0] = 0;
}

void Process_USB_Set_Request(USB_SETUP_REQ *req)
{
  if (((req->wValue & 0xFF) == REPORT_MEMORY) ||
      ((req->wValue & 0xFF) == REPORT_WIRES))
    USBD_CtlPrepareRx(&USB_OTG_dev,buffer,req->wLength);
}

void USB_HP_IRQHandler(void)
{
  USBD_OTG_ISR_Handler(&USB_OTG_dev);
}

void RunRTC(void)
{
  PWR->CR |= PWR_CR_DBP;  // Disable backup domain security

  /* Software backup domain reset */
  RCC->BDCR |= RCC_BDCR_BDRST;
  RCC->BDCR &= ~RCC_BDCR_BDRST;

  /* Start LSE */
  RCC_LSEConfig(RCC_LSE_ON);
  RCC_LSEDriveConfig(RCC_LSEDrive_MediumHigh);
  while (!(RCC->BDCR & RCC_BDCR_LSERDY));

  /* Start RTC */
  RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
  RCC_RTCCLKCmd(ENABLE);

  PWR->CR &= ~PWR_CR_DBP;  // Enable backup domain security
}
