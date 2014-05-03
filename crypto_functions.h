// Cryptographic functions for wireless protocol

#ifndef __CRYPTO_FUNCTIONS_H__
#define __CRYPTO_FUNCTIONS_H__

#include <STM32F37x.h>
#include "crypto.h"

void Initialize_AES_Key(uint8_t* AES_key);
int32_t Crypt_Bytes(uint8_t* input_buffer, uint8_t* output_buffer, uint8_t length);

#endif
