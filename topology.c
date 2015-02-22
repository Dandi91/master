#include "topology.h"
#include "flash_structures.h"

topology_answer_typedef topology_answer = TOP_ANS_WAIT;

void SetTopologyAnswer(topology_answer_typedef value)
{
  topology_answer = value;
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

uint8_t CountUnconnected(net_typedef *array, uint32_t length)
{
  uint32_t i;
  uint8_t result = 0;

  for (i = 0; i < length; i++)
    if (array[i].from == 0xFF)
      result++;

  return result;
}

uint8_t* GetPathTo(net_typedef *net, uint8_t *packet, uint16_t needed_idx, uint16_t load_length)
{
  uint8_t from;
  int32_t i;
  uint8_t chain[MAX_DEVICES], count;
  uint8_t *pos;
  
  from = net[needed_idx].from;
  chain[0] = needed_idx;
  count = 1;
  i = needed_idx;
  while ((i > 0) && (from != DEFAULT_MASTER_ADDRESS))
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
  packet[PCKT_FROM_OFST] = DEFAULT_MASTER_ADDRESS;
  packet[PCKT_CMD_OFST] = PACKET_TYPE_TRSMT;
  pos = &packet[PCKT_DATA_OFST];
  if (count > 1)
    for (i = count - 2; i > -1; i--)
    {
      *pos++ = 1; // Re-transmit - only one included packet
      pos = MakeTransPacket(pos,load_length + (5 * i),net[chain[i]].to,PACKET_TYPE_TRSMT);
    }
  return pos;
}

void DisassembleTransmitEcho(net_typedef *array, uint32_t length, uint8_t *packet)
{
  uint32_t i, count;
  uint8_t current_address, current_idx, *pos;
  net_typedef new_top;

  pos = &packet[PCKT_DATA_OFST];
  count = *pos++;
  for (i = 0; i < count; i++)
  {
    pos += DATA_LENGTH;
    current_address = *(pos + TRMS_TO_OFST);
    current_idx = FindTopology(array,length,current_address);
    if (*(pos + TRMS_CMD_OFST) == PACKET_TYPE_ECHO)
    {
      // Catch!
      new_top.from = packet[PCKT_FROM_OFST];
      new_top.to = current_address;
      array[current_idx] = new_top;
    }
    pos += TRMS_DATA_OFST;
  }
}

void RebuildTopology(uint8_t dev_count, uint8_t *packet)
{
  uint32_t i, j;
  uint8_t curr_address, direct_connect_count = 0, asked, *pos;
  net_typedef new_topology[MAX_DEVICES];

  for (i = 0; i < dev_count; i++)
  {
    curr_address = i + 0x02;
    new_topology[i].to = curr_address;
    topology_answer = TOP_ANS_WAIT;
    // Send packet
    SendProtoPacket(curr_address,PACKET_TYPE_ECHO);
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
    pos = GetPathTo(new_topology,packet,i++,4*asked + 1);  // Search path to needed transmitter
    j = dev_count - asked;
    *pos++ = asked;
    while (j < dev_count)
      pos = MakeTransPacket(pos,0,new_topology[j++].to,PACKET_TYPE_ECHO);
    // Send packet to i-device
    topology_answer = TOP_ANS_WAIT;
    SendPacket(packet,pos - packet);
    // Wait
    while (topology_answer == TOP_ANS_WAIT);
    // Check result
    switch (topology_answer)
    {
      case TOP_ANS_OK:
      {
        DisassembleTransmitEcho(new_topology,dev_count,packet);    // Modify array
        SortArray(new_topology,dev_count);                         // Sort array again
        break;
      }
      case TOP_ANS_TIMEOUT:
      {
        // Oh... Something went wrong!
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
