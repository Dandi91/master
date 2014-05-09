#include "rft_handlers.h"
#include "init.h"
#include "flash_structures.h"
#include "topology.h"

// Number of samples
#define TIME_PERIODS_COUNT 3
// Multiplication constant (2048 / 2047.95)
#define TIMER_SHIFTING_VALUE (float)1.000024414658561
// Additional number of cycles for startup
#define WARM_UP_TIME 10

typedef struct
{
  uint16_t adc[8];
  uint32_t inputs;
} slave_state_typedef;

// Current system state
slave_state_typedef system_state[MAX_DEVICES];

// Work packet
uint8_t packet[MAX_PACKET_LOAD + PROTO_BYTES_CNT];
// In-going transmission
uint8_t in_trans[MAX_PACKET_LOAD + PROTO_BYTES_CNT];
// Out-going transmission
uint8_t out_trans[MAX_PACKET_LOAD + PROTO_BYTES_CNT];
data_len_t packet_length;
uint8_t dev_count;
uint8_t is_topology_construct = 0;

uint8_t current_transmission_from, current_transmission_to, is_transmit = 0;
uint8_t transmt_count, transmt_index;
uint8_t *curr_transmt_pos, *curr_pack_pos;

void CopyBuffer(uint8_t *source, uint8_t *dest, uint32_t length)
{
  uint32_t i;

  i = 0;
  while (i++ < length)
    *dest++ = *source++;
}

void EXTI15_10_IRQHandler(void)
{
  if (EXTI_GetITStatus(EXTI_Line15) == SET)
  {
    EXTI_ClearITPendingBit(EXTI_Line15);
    SPI_RFT_IRO_IRQHandler();
  }
}

void StartTimeoutTimer(void)
{
  TIM_ITConfig(TIM7,TIM_IT_Update,DISABLE);
  TIM_GenerateEvent(TIM7,TIM_EventSource_Update);
  TIM_Cmd(TIM7,ENABLE);
  TIM_ITConfig(TIM7,TIM_IT_Update,ENABLE);
}

void StopTimeoutTimer(void)
{
  TIM_Cmd(TIM7,DISABLE);
}

void TX_Complete(void)
{
  SPI_RFT_Start_Polling();
}

void MakePacket(uint8_t to, uint8_t from, uint8_t cmd)
{
  packet[PCKT_TO_OFST] = to;
  packet[PCKT_FROM_OFST] = from;
  packet[PCKT_CMD_OFST] = cmd;
}

void GetOutputs(uint16_t *p, data_len_t *len)
{
  uint8_t i;

  len = 0;
  if (GetPeripheralParams().b.adc_enabled)
  {
    for (i = 0; i < 8; i++)
      *p++ = ReadADC(i);
    len += 16;
  }
  if (GetPeripheralParams().b.input_enabled)
  {
    *(uint32_t*)p = GetLogicInputs();
    len += 4;
  }
}

void SetOutputs(uint16_t *p)
{
  if (GetPeripheralParams().b.dac_enabled)
  {
    WriteDACs(p);
    p += 16;
  }
  if (GetPeripheralParams().b.output_enabled)
    SetLogicOutputs(*(uint32_t*)p);
}

uint8_t* MakeTransPacket(uint8_t *array, data_len_t len, uint8_t to, uint8_t cmd)
{
  *(data_len_t*)array = len;
  array += DATA_LENGTH;
  *(array + TRMS_TO_OFST) = to;
  *(array + TRMS_CMD_OFST) = cmd;
  return array + TRMS_DATA_OFST;
}

void SendProtoPacket(uint8_t to, uint8_t type)
{
  MakePacket(to,DEFAULT_MASTER_ADDRESS,type);
  SendPacket(packet,PROTO_BYTES_CNT);
}

void SendPacket(uint8_t *array, data_len_t length)
{
  SPI_RFT_Write_Packet(array,length);
  StartTimeoutTimer();
}

void HandlePacket(uint8_t *h_packet)
{
  data_len_t *temp;

  switch (h_packet[TRMS_CMD_OFST])
  {
    case PACKET_TYPE_REQ:
    {
      /* setting outputs */
      SetOutputs((uint16_t*)(h_packet + TRMS_DATA_OFST));

      /* getting outputs */
      temp = (data_len_t*)curr_pack_pos;
      curr_pack_pos = MakeTransPacket(curr_pack_pos,0,DEFAULT_MASTER_ADDRESS,PACKET_TYPE_ANS);
      GetOutputs((uint16_t*)curr_pack_pos,temp);

      curr_pack_pos += *temp;
      break;
    }
  }
}

void TransmitNextFromBuffer(void)
{
  uint32_t curr_length;

  // Check with ourselves
  if (*(curr_transmt_pos + DATA_LENGTH + TRMS_TO_OFST) == DEFAULT_MASTER_ADDRESS)
  {
    // Recipient is we!
    curr_transmt_pos += DATA_LENGTH;
    HandlePacket(curr_transmt_pos);
    return;
  }

  // Length
  curr_length = *(data_len_t*)curr_transmt_pos;
  curr_transmt_pos += DATA_LENGTH;

  // Protocol fields
  current_transmission_to = *(curr_transmt_pos + TRMS_TO_OFST);
  MakePacket(current_transmission_to,DEFAULT_MASTER_ADDRESS,
             *(curr_transmt_pos + TRMS_CMD_OFST));

  // Data
  curr_transmt_pos += TRMS_DATA_OFST;
  CopyBuffer(curr_transmt_pos,&packet[PCKT_DATA_OFST],curr_length);
  curr_transmt_pos += curr_length;

  // Send
  SPI_RFT_Write_Packet(packet,curr_length + PROTO_BYTES_CNT);

  // Start timeout timer
  StartTimeoutTimer();
}

void InitTopology(void)
{
  is_topology_construct = 1;
  RebuildTopology(dev_count,packet);
  is_topology_construct = 0;
}

void TIM7_IRQHandler(void)
{
  if (TIM_GetFlagStatus(TIM7,TIM_FLAG_Update) == SET)
  {
    TIM_ClearFlag(TIM7,TIM_FLAG_Update);
    StopTimeoutTimer();
    if (is_transmit)
    {
      curr_pack_pos = MakeTransPacket(curr_pack_pos,0,current_transmission_to,PACKET_TYPE_TIMEOUT);
      TransmitNextFromBuffer();
    }
    if (is_topology_construct)
    {
      SetTopologyAnswer(TOP_ANS_TIMEOUT);
    }
  }
}

void RX_Complete(void)
{
  uint8_t sender;
  uint16_t curr_length;

  if (packet[PCKT_TO_OFST] == DEFAULT_MASTER_ADDRESS)
  {
    sender = packet[PCKT_FROM_OFST];
    if (is_transmit)
    {
      // Transmission in progress
      // All received packets should be packed to output packet
      curr_length = packet_length - PROTO_BYTES_CNT;
      curr_pack_pos = MakeTransPacket(curr_pack_pos,curr_length,sender,packet[PCKT_CMD_OFST]);
      CopyBuffer(&packet[PCKT_DATA_OFST],curr_pack_pos,curr_length);
      curr_pack_pos += curr_length;
      // Check next packet
      transmt_index++;
      if (transmt_index < transmt_count)
      {
        // Still have something to transmit
        TransmitNextFromBuffer();
      }
      else
      {
        // Nothing to transmit; send packet to main sender
        out_trans[PCKT_TO_OFST] = current_transmission_from;
        out_trans[PCKT_FROM_OFST] = DEFAULT_MASTER_ADDRESS;
        out_trans[PCKT_CMD_OFST] = PACKET_TYPE_TRSMT;
        out_trans[PCKT_DATA_OFST] = transmt_count;

        curr_length = curr_pack_pos - out_trans;
        SPI_RFT_Write_Packet(out_trans,curr_length);

        is_transmit = 0;
      }
      return;
    }
    switch (packet[PCKT_CMD_OFST])
    {
      case PACKET_TYPE_ECHO:
      {
        if (is_topology_construct)
          SetTopologyAnswer(TOP_ANS_OK);
        break;
      }
      case PACKET_TYPE_REQ:  // output data
      {
        SetOutputs((uint16_t*)(packet + PCKT_DATA_OFST));
        GetOutputs((uint16_t*)(packet + PCKT_DATA_OFST),&packet_length);

        MakePacket(sender,DEFAULT_MASTER_ADDRESS,PACKET_TYPE_ANS);
        SPI_RFT_Write_Packet(packet,packet_length + PROTO_BYTES_CNT);
        break;
      }
      case PACKET_TYPE_TRSMT:    // Incapsulated packets
      {
        if (is_topology_construct)
        {
          // Returned echo from a transmitter
          SetTopologyAnswer(TOP_ANS_OK);
          break;
        }
        if (!is_transmit)    // First request
        {
          current_transmission_from = sender;
          is_transmit = 1;
          // Copy to independent buffer
          CopyBuffer(packet,in_trans,packet_length);

          // Start first transmission
          transmt_count = in_trans[PCKT_DATA_OFST];
          transmt_index = 0;
          curr_pack_pos = &out_trans[PCKT_DATA_OFST + 1];
          curr_transmt_pos = &in_trans[PCKT_DATA_OFST + 1];

          TransmitNextFromBuffer();
        }
        break;
      }
    }
  }
}

uint8_t* RX_Begin(data_len_t length)
{
  StopTimeoutTimer();
  packet_length = length;
  if ((length < PROTO_BYTES_CNT) || (length > sizeof(packet)))
    return NULL;
  else
    return packet;
}
