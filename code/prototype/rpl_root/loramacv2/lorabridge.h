#ifndef LORABRIDGE_H
#define LORABRIDGE_H

#include "contiki.h"

/**
 * \brief Function to use when a packet is available
 *        for the LoRa stack.
 * 
 */
void bridge_input(void);

/**
 * \brief Function used by the loramac when the LoRa
 *        network is joined.
 * 
 */
void lora_network_joined(void);

#endif /* LORABRIDGE_H */
