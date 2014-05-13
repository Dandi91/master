#include "rft_handlers.h"
#include "flash_structures.h"
#include "topology.h"
#include "periph.h"

// Work packet
uint8_t packet[MAX_PACKET_LOAD + PROTO_BYTES_CNT];
data_len_t packet_len = 0;
uint8_t is_topology_construct = 0, is_initialization = 0;

const net_typedef *net;
static uint8_t runs;

typedef enum
{
  RFT_ANS_OK = 0,
  RFT_ANS_TIMEOUT,
  RFT_ANS_WAIT
} rft_ans_typedef;

rft_ans_typedef rft_answer;

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
  rft_answer = RFT_ANS_WAIT;
  SPI_RFT_Write_Packet(array,length);
  StartTimeoutTimer();
}

void DisTransPacket(uint8_t *packet)
{
  data_len_t sub_len;
  uint8_t count, i;

  count = packet[0];
  packet++;
  for (i = 0; i < count; i++)
  {
    sub_len = *(data_len_t*)packet;
    packet += DATA_LENGTH;
    if (packet[TRMS_CMD_OFST] == PACKET_TYPE_ANS)
    {
      SetInputsFor(packet[TRMS_TO_OFST],&packet[TRMS_DATA_OFST],sub_len);
      packet += sub_len + TRMS_DATA_OFST;
    }
    else if (packet[TRMS_CMD_OFST] == PACKET_TYPE_TRSMT)
    {
      packet += TRMS_DATA_OFST;
      DisTransPacket(packet);
      packet += sub_len;
    }
  }
}

void DisassemblePacket(uint8_t *packet)
{
  if (packet[PCKT_CMD_OFST] == PACKET_TYPE_ANS)
    SetInputsFor(packet[PCKT_FROM_OFST],&packet[PCKT_DATA_OFST],packet_len - PROTO_BYTES_CNT);
  else if (packet[PCKT_CMD_OFST] == PACKET_TYPE_TRSMT)
    DisTransPacket(&packet[PCKT_DATA_OFST]);
}

uint8_t CheckTransmitter(uint8_t root_idx)
{
  uint8_t i;

  for (i = root_idx + 1; i < get_dev_count(); i++)
  {
    if (net[i].from == net[root_idx].to)
      return 1;
  }
  return 0;
}

data_len_t BuildRequest(uint8_t *packet, uint8_t target_idx)
{
  uint8_t i, sub_count;
  data_len_t len, sub_len, *p_len;
  uint8_t *p;

  // Build request for itself
  sub_count = 1;                      // First sub-packet
  len = 1;                            // One byte for sub-packets count
  if (is_initialization)
  {
    p = MakeTransPacket(&packet[1],0,net[target_idx].to,PACKET_TYPE_INIT);
    sub_len = 0;
  }
  else
  {
    p_len = (data_len_t*)&packet[1];  // Pointer to current sub-packet's data length
    p = MakeTransPacket(&packet[1],0,net[target_idx].to,PACKET_TYPE_REQ);
    sub_len = GetOutputsFor(net[target_idx].to,p);
    *p_len = sub_len;                 // Write data length
  }
  p += sub_len;                       // Move pointer foward
  len += sub_len + TRMS_PROTO_CNT;    // Count result length
  runs++;
  if (runs > MAX_TRSM_CHAIN)
    return len;
  // Build transmissions
  for (i = target_idx + 1; i < get_dev_count(); i++)
  {
    if (net[i].from == net[target_idx].to)
    {
      sub_count++;
      if (CheckTransmitter(i))
      {
        // If slave is a transmitter
        p_len = (data_len_t*)p;
        p = MakeTransPacket(p,0,net[i].to,PACKET_TYPE_TRSMT);
        sub_len = BuildRequest(p,i);
        *p_len = sub_len;
        p += sub_len;
        len += sub_len + TRMS_PROTO_CNT;
      }
      else
      {
        // If slave isn't a transmitter - make simple request packet
        if (is_initialization)
        {
          sub_len = 0;
          p = MakeTransPacket(p,0,net[i].to,PACKET_TYPE_INIT);
        }
        else
        {
          p_len = (data_len_t*)p;           // Pointer to current sub-packet's data length
          p = MakeTransPacket(p,0,net[i].to,PACKET_TYPE_REQ);
          sub_len = GetOutputsFor(net[i].to,p);
          *p_len = sub_len;                 // Write data length
        }
        p += sub_len;                     // Move pointer foward
        len += sub_len + TRMS_PROTO_CNT;  // Count result length
      }
    }
  }
  packet[0] = sub_count;
  return len;
}

void InitSignal(void)
{
  is_initialization = 1;
  SystemPoll();
  is_initialization = 0;
}

void SystemPoll(void)
{
  uint8_t i = 0;
  data_len_t len;

  if (!is_initialization)
    UpdateMasterPeripherals();
  net = get_topology_table_address();
  while (net[i].from == DEFAULT_MASTER_ADDRESS)
  {
    // Cycle until first non-direct connection is reached
    packet[PCKT_TO_OFST] = net[i].to;
    packet[PCKT_FROM_OFST] = DEFAULT_MASTER_ADDRESS;
    if (CheckTransmitter(i))
    {
      // If net[i] is a transmitter
      packet[PCKT_CMD_OFST] = PACKET_TYPE_TRSMT;
      runs = 0;
      len = BuildRequest(&packet[PCKT_DATA_OFST],i);
    }
    else
    {
      // If net[i] is not a transmitter
      if (is_initialization)
      {
        packet[PCKT_CMD_OFST] = PACKET_TYPE_INIT;
        len = 0;
      }
      else
      {
        packet[PCKT_CMD_OFST] = PACKET_TYPE_REQ;
        len = GetOutputsFor(net[i].to,&packet[PCKT_DATA_OFST]);
      }
    }
    SendPacket(packet,len + PROTO_BYTES_CNT);
    // Wait for result
    while (rft_answer == RFT_ANS_WAIT);
    if (rft_answer == RFT_ANS_OK)
    {
      // Retrieve result from packet
      if (!is_initialization)
        DisassemblePacket(packet);
    }
    else if (rft_answer == RFT_ANS_TIMEOUT)
    {
      // Direct-connected slave doesn't respond
    }
    i++;
  }
}

void InitTopology(void)
{
  is_topology_construct = 1;
  RebuildTopology(get_dev_count(),packet);
  is_topology_construct = 0;
}

void TIM7_IRQHandler(void)
{
  if (TIM_GetFlagStatus(TIM7,TIM_FLAG_Update) == SET)
  {
    TIM_ClearFlag(TIM7,TIM_FLAG_Update);
    StopTimeoutTimer();
    rft_answer = RFT_ANS_TIMEOUT;
    if (is_topology_construct)
      SetTopologyAnswer(TOP_ANS_TIMEOUT);
  }
}

void RX_Complete(void)
{
  if (packet[PCKT_TO_OFST] == DEFAULT_MASTER_ADDRESS)
  {
    rft_answer = RFT_ANS_OK;
    switch (packet[PCKT_CMD_OFST])
    {
      case PACKET_TYPE_ECHO:
      {
        if (is_topology_construct)
          SetTopologyAnswer(TOP_ANS_OK);
        break;
      }
      case PACKET_TYPE_TRSMT:    // Incapsulated packets
      {
        if (is_topology_construct)
          // Returned echo from a transmitter
          SetTopologyAnswer(TOP_ANS_OK);
        break;
      }
    }
  }
}

uint8_t* RX_Begin(data_len_t length)
{
  StopTimeoutTimer();
  packet_len = length;
  if ((length < PROTO_BYTES_CNT) || (length > sizeof(packet)))
    return NULL;
  else
    return packet;
}
