#include "rft_handlers.h"
#include "init.h"
#include "flash_structures.h"

// Number of samples
#define TIME_PERIODS_COUNT 3
// Multiplication constant (2048 / 2047.95)
#define TIMER_SHIFTING_VALUE (float)1.000024414658561
// Additional number of cycles for startup
#define WARM_UP_TIME 10

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

typedef struct
{
  uint16_t adc[8];
  uint32_t inputs;
} slave_state_typedef;

typedef enum
{
  TOP_ANS_WAIT = 0,
  TOP_ANS_OK,
  TOP_ANS_TIMEOUT
} topology_answer_typedef;

topology_answer_typedef topology_answer = TOP_ANS_WAIT;

// Current system state
slave_state_typedef system_state[MAX_DEVICES];

// Work packet
uint8_t packet[MAX_PACKET_LOAD + PROTO_BYTES_CNT];
// In-going transmission
uint8_t in_trans[MAX_PACKET_LOAD + PROTO_BYTES_CNT];
// Out-going transmission
uint8_t out_trans[MAX_PACKET_LOAD + PROTO_BYTES_CNT];
data_len_t packet_length;
uint8_t address, dev_count;
uint8_t is_topology_construct = 0;

uint8_t current_transmission_from, current_transmission_to, is_transmit = 0;
uint8_t transmt_count, transmt_index;
uint8_t *curr_transmt_pos, *curr_pack_pos;

void Set_Address(uint8_t value)
{
  address = value;
}

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

void MakeTransPacket(data_len_t len, uint8_t to, uint8_t cmd)
{
  *(data_len_t*)curr_pack_pos = len;
  curr_pack_pos += DATA_LENGTH;
  *(curr_pack_pos + TRMS_TO_OFST) = to;
  *(curr_pack_pos + TRMS_CMD_OFST) = cmd;
  curr_pack_pos += TRMS_DATA_OFST;
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
      MakeTransPacket(0,address,PACKET_TYPE_ANS);
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
  if (*(curr_transmt_pos + DATA_LENGTH + TRMS_TO_OFST) == address)
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
  MakePacket(current_transmission_to,address,
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

uint32_t FindMin(net_typedef* array, uint32_t start, uint32_t end)
{
  uint32_t i, result;
  net_typedef min_item;

  min_item = array[start];
  result = start;
  for (i = start + 1; i < end + 1; i++)
  {
    if (min_item.from > array[i].from)
    {
      min_item = array[i];
      result = i;
    }
  }
  return result;
}

void SortArray(net_typedef* array, uint32_t length)
{
  net_typedef buf;
  uint32_t i, min;

  for (i = 0; i < length; i++)
  {
    min = FindMin(array,i,length - 1);
    buf = array[i];
    array[i] = array[min];
    array[min] = buf;
  }
}

uint8_t FindTopology(net_typedef *array, uint32_t length, uint8_t address)
{
  uint32_t i;

  for (i = 0; i < length; i++)
  {
    if (array[i].to == address)
      return i;
  }
  return 0xFF;
}

void DisassembleTransmitEcho(net_typedef *array, uint32_t length)
{
  uint32_t i, count;
  uint8_t current_address, current_idx;
  net_typedef new_top;

  curr_transmt_pos = &packet[PCKT_DATA_OFST];
  count = *curr_transmt_pos++;
  for (i = 0; i < count; i++)
  {
    curr_transmt_pos += DATA_LENGTH;
    current_address = *(curr_transmt_pos + TRMS_TO_OFST);
    current_idx = FindTopology(array,length,current_address);
    if (*(curr_transmt_pos + TRMS_CMD_OFST) == PACKET_TYPE_ECHO)
    {
      // Catch!
      new_top.from = packet[PCKT_FROM_OFST];
      new_top.to = current_address;
      array[current_idx] = new_top;
    }
    curr_transmt_pos += TRMS_DATA_OFST;
  }
}

void GetPathTo(net_typedef *net, uint32_t net_length, uint8_t *packet, uint16_t needed_idx, uint32_t load_length)
{
  uint8_t from;
  int32_t i;
  uint8_t chain[MAX_DEVICES], count;
  
  from = net[needed_idx].from;
  chain[0] = needed_idx;
  count = 1;
  i = needed_idx;
  while ((i > 0) || (from != address))
  {
    // Go from needed element up
    i--;
    if (net[i].to == from)
    {
      chain[count] = i;
      count++;
      from = net[i].from;
    }
  }
  // Now chain has all indexes of connectors between master and needed slave
  // Start forming transmission packet
  packet[PCKT_TO_OFST] = net[chain[count - 1]].to;
  packet[PCKT_FROM_OFST] = address;
  packet[PCKT_CMD_OFST] = PACKET_TYPE_TRSMT;
  curr_pack_pos = &packet[PCKT_DATA_OFST];
  if (count > 1)
    for (i = count - 2; i > -1; i--)
    {
      *curr_pack_pos++ = 1; // Re-transmit - only one included packet
      MakeTransPacket(load_length + (5 * i),net[chain[i]].to,PACKET_TYPE_TRSMT);
    }
}

uint8_t CountUnconnected(net_typedef *array, uint32_t length)
{
  uint32_t i;
  uint8_t result = 0;

  for (i = 0; i < length; i++)
    if (array[i].from == 0xFF)
      result++;

  return result;
}

void BuildTopology(void)
{
  uint32_t i, j;
  uint8_t curr_address, direct_connect_count = 0, asked;
  net_typedef new_topology[MAX_DEVICES];

  is_topology_construct = 1;
  for (i = 0; i < dev_count; i++)
  {
    curr_address = i + 0x02;
    new_topology[i].to = curr_address;
    topology_answer = TOP_ANS_WAIT;
    // Send packet
    MakePacket(curr_address,address,PACKET_TYPE_ECHO);
    SPI_RFT_Write_Packet(packet,PROTO_BYTES_CNT);
    StartTimeoutTimer();
    // Wait for answer
    while (topology_answer == TOP_ANS_WAIT);
    switch (topology_answer)
    {
      case TOP_ANS_OK:
      {
        new_topology[i].from = DEFAULT_MASTER_ADDRESS;
        direct_connect_count++;
        break;
      }
      case TOP_ANS_TIMEOUT:
      {
        new_topology[i].from = 0xFF;
        break;
      }
      case TOP_ANS_WAIT:
        // KERNEL PANIC!
        while(1);
    }
  }
  SortArray(new_topology,dev_count);
  // Test each direct connected slave to be a transmitter
  i = 0;
  while (i < dev_count)
  {
    asked = CountUnconnected(new_topology,dev_count);
    if (asked == 0)
      break;
    GetPathTo(new_topology,dev_count,packet,i++,4*asked + 1);  // Search path to needed transmitter
    j = dev_count - asked;
    *curr_pack_pos++ = asked;
    while (j < dev_count)
      MakeTransPacket(0,new_topology[j++].to,PACKET_TYPE_ECHO);
    // Send packet to i-device
    topology_answer = TOP_ANS_WAIT;
    SPI_RFT_Write_Packet(packet,curr_pack_pos - packet);
    StartTimeoutTimer();
    // Wait
    while (topology_answer == TOP_ANS_WAIT);
    // Check result
    switch (topology_answer)
    {
      case TOP_ANS_OK:
      {
        DisassembleTransmitEcho(new_topology,dev_count);    // Modify array
        SortArray(new_topology,dev_count);                  // Sort array again
        break;
      }
      case TOP_ANS_TIMEOUT:
      {
        // Oh... Something gone wrong!
        // Slave is not responding now
        break;
      }
      case TOP_ANS_WAIT:
      {
        while (1); // KERNEL PANIC!
      }
    }
  }
  // Write new topology to FLASH
  for (i = 0; i < dev_count; i++)
    set_topology(new_topology[i],i);
}

void TIM7_IRQHandler(void)
{
  if (TIM_GetFlagStatus(TIM7,TIM_FLAG_Update) == SET)
  {
    TIM_ClearFlag(TIM7,TIM_FLAG_Update);
    StopTimeoutTimer();
    if (is_transmit)
    {
      MakeTransPacket(0,current_transmission_to,PACKET_TYPE_TIMEOUT);
      TransmitNextFromBuffer();
    }
    if (is_topology_construct)
    {
      topology_answer = TOP_ANS_TIMEOUT;
    }
  }
}

void RX_Complete(void)
{
  uint8_t sender;
  uint16_t curr_length;

  if ((packet[PCKT_TO_OFST] == address) || (packet[PCKT_TO_OFST] == BROADCAST_ADDRESS))
  {
    sender = packet[PCKT_FROM_OFST];
    if (is_transmit)
    {
      // Transmission in progress
      // All received packets should be packed to output packet
      curr_length = packet_length - PROTO_BYTES_CNT;
      MakeTransPacket(curr_length,sender,packet[PCKT_CMD_OFST]);
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
        out_trans[PCKT_FROM_OFST] = address;
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
          topology_answer = TOP_ANS_OK;
        break;
      }
      case PACKET_TYPE_REQ:  // output data
      {
        SetOutputs((uint16_t*)(packet + PCKT_DATA_OFST));
        GetOutputs((uint16_t*)(packet + PCKT_DATA_OFST),&packet_length);

        MakePacket(sender,address,PACKET_TYPE_ANS);
        SPI_RFT_Write_Packet(packet,packet_length + PROTO_BYTES_CNT);
        break;
      }
      case PACKET_TYPE_TRSMT:    // Incapsulated packets
      {
        if (is_topology_construct)
        {
          // Returned echo from a transmitter
          topology_answer = TOP_ANS_OK;
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
