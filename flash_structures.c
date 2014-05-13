#include "flash_structures.h"

// Main FLASH arrays for connections. Written by USB
static const net_typedef topology[MAX_DEVICES];
static const conn_typedef connections[MAX_CONN];
static const sys_typedef system_settings = {0,0};

uint32_t* get_connections_table_address(void)
{
  return (uint32_t*)connections;
}

const net_typedef* get_topology_table_address(void)
{
  return topology;
}

conn_typedef get_connection(uint32_t index)
{
  return connections[index];
}

const sys_typedef* get_sys_settings_address(void)
{
  return &system_settings;
}

uint16_t get_wires_count(void)
{
  return system_settings.wires_count;
}

uint8_t get_dev_count(void)
{
  return system_settings.device_count;
}

void set_topology(net_typedef value, uint32_t index)
{
  uint32_t address;

  address = (uint32_t)&topology[index];
  FLASH_ProgramHalfWord(address,*(uint16_t*)&value);
}
