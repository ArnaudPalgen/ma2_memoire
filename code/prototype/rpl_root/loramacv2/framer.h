/*
 * Framer for LoRaMAC frames.
 */

#ifndef LORAFRAMER_H_
#define LORAFRAMER_H_

#include "contiki.h"

/**
 * \brief        Parse the received data to a LoRaMAC frame in the lorabuf
 * \param data   The received ASCII data
 * \param len    The length of the received data
 * \param offset The offset of the received data
 * \return       0
 * 
 */ 
int parse(char *data, int len, int offset);

/**
 * \brief              Convert the lorabuf to its'ASCII representation
 * \param destination  The destination of the conversion
 * \return             The length of the string
 * 
 */
int create(char *destination);

#endif /* LORAFRAMER_H_ */
