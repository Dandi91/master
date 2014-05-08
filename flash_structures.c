#include "flash_structures.h"

// Main FLASH arrays for connections. Written by USB
static const net_typedef topology[MAX_DEVICES];
static const conn_typedef connections[MAX_CONN];

uint32_t* get_connections_table_address(void)
{
  return (uint32_t*)connections;
}

uint32_t* get_topology_table_address(void)
{
  return (uint32_t*)topology;
}

conn_typedef get_connection(uint32_t index)
{
  return connections[index];
}

net_typedef get_topology(uint32_t index)
{
  return topology[index];
}

void set_topology(net_typedef value, uint32_t index)
{
  uint32_t address;

  address = (uint32_t)&topology[index];
  FLASH_ProgramHalfWord(address,*(uint16_t*)&value);
}
