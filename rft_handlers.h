// Event handlers for the wireless module
// Implementation of the tramsmission protocol

#ifndef __RFT_HANDLERS_H__
#define __RFT_HANDLERS_H__

#include <STM32F37x.h>
#include "mrf49xa.h"

// 1 digit = 2 ms
#define TRMS_TIMEOUT_PERIOD   30

// Protocol constants
#define DEFAULT_MASTER_ADDRESS  0x01
#define BROADCAST_ADDRESS       0xFF

#define PACKET_TYPE_INIT      0x00
#define PACKET_TYPE_ANS       0x01
#define PACKET_TYPE_REQ       0x02
#define PACKET_TYPE_TRSMT     0x03
#define PACKET_TYPE_ECHO      0xFD
#define PACKET_TYPE_TIMEOUT   0xFE
#define PACKET_TYPE_OK        0xFF

#define DATA_LENGTH      (sizeof(data_len_t))

// Fields
#define PCKT_TO_OFST    0x00
#define PCKT_FROM_OFST  0x01
#define PCKT_CMD_OFST   0x02
#define PCKT_DATA_OFST  0x03

#define TRMS_TO_OFST    0x00
#define TRMS_CMD_OFST   0x01
#define TRMS_DATA_OFST  0x02

// Functions for making transmission packets
uint8_t* MakeTransPacket(uint8_t *array, data_len_t len, uint8_t to, uint8_t cmd);
void SendProtoPacket(uint8_t to, uint8_t type);
void SendPacket(uint8_t *array, data_len_t length);

// Basic handlers for the RFT library
void TX_Complete(void);
void RX_Complete(void);
uint8_t* RX_Begin(data_len_t length);

#endif
