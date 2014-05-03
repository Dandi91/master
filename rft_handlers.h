// Event handlers for the wireless module
// Implementation of the tramsmission protocol

#ifndef __RFT_HANDLERS_H__
#define __RFT_HANDLERS_H__

#include <STM32F37x.h>
#include "mrf49xa.h"

// 1 digit = 2 ms
#define TRMS_TIMEOUT_PERIOD   30

// Wrap above the address value
void Set_Address(uint8_t value);

// Basic handlers for the RFT library
void TX_Complete(void);
void RX_Complete(void);
uint8_t* RX_Begin(data_len_t length);

#endif
