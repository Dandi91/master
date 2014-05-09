// Funtions for work on the topology of the wireless network

#ifndef __TOPOLOGY_H__
#define __TOPOLOGY_H__

#include <STM32F37x.h>
#include "rft_handlers.h"

typedef enum
{
  TOP_ANS_WAIT = 0,
  TOP_ANS_OK,
  TOP_ANS_TIMEOUT
} topology_answer_typedef;

void SetTopologyAnswer(topology_answer_typedef value);
void RebuildTopology(uint8_t dev_count, uint8_t *packet);

#endif
