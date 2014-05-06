#include "flash_structures.h"

// Main FLASH arrays for connections. Written by USB
static const net_type_def topology[MAX_DEVICES];
static const conn_type_def connections[MAX_CONN];

uint32_t* get_connections_table_address(void)
{
  return (uint32_t*)connections;
}

uint32_t* get_topology_table_address(void)
{
  return (uint32_t*)topology;
}

conn_type_def get_connection(uint32_t index)
{
  return connections[index];
}

net_type_def get_topology(uint32_t index)
{
  return topology[index];
}
