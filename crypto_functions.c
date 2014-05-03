#include "crypto_functions.h"

const static uint8_t IV[CRL_AES_BLOCK] =
{
  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
  0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

uint8_t Key[CRL_AES128_KEY];

void Initialize_AES_Key(uint8_t* AES_key)
{
  uint8_t i;
  for (i = 0; i < CRL_AES128_KEY; i++)
    Key[i] = AES_key[i];
}

int32_t Crypt_Bytes(uint8_t* input_buffer, uint8_t* output_buffer, uint8_t length)
{
  AESCTRctx_stt AESctx;
  uint32_t error_status = AES_SUCCESS;
  int32_t output_length;

  Crypto_DeInit();

  AESctx.mFlags = E_SK_DEFAULT;
  AESctx.mKeySize = 16;
  AESctx.mIvSize = sizeof(IV);

  error_status = AES_CTR_Encrypt_Init(&AESctx,Key,IV);
  if (error_status == AES_SUCCESS)
  {
    /* Crypt Data */
    error_status = AES_CTR_Encrypt_Append(&AESctx,input_buffer,length,output_buffer,&output_length);
    if (error_status == AES_SUCCESS)
    {
      /* Do the Finalization */
      error_status = AES_CTR_Encrypt_Finish(&AESctx,output_buffer + output_length,&output_length);
    }
  }
  return error_status;
}
