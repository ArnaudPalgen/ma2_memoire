/*
 * The LoRaMac address.
 * This kind of address is composed of
 * a 1 byte prefix followed by a 2 bytes identifier.
 */

#ifndef LORAADDR_H_
#define LORAADDR_H_

#include "contiki.h"
#include "net/ipv6/uip.h"
/*---------------------------------------------------------------------------*/
#define LORA_ADDR_PREFIX_SIZE 1
#define LORA_ADDR_ID_SIZE 2
#define LORA_ADDR_PADDING_SIZE 1
#define LORA_ADDR_SIZE (LORA_ADDR_PREFIX_SIZE + LORA_ADDR_PADDING_SIZE +LORA_ADDR_ID_SIZE)
/*---------------------------------------------------------------------------*/
typedef struct lora_addr{
    uint8_t prefix;
    uint16_t id;
}lora_addr_t;
/*---------------------------------------------------------------------------*/
/**
 * \brief Copy a lora address to another
 * \param dest The destination address
 * \param from The source address
 * 
 */
void loraaddr_copy(lora_addr_t *dest, const lora_addr_t *from);

/**
 * \brief Compare two lora addresses
 * \param addr1 The first address
 * \param addr2 The second address
 * \return 1 if the addresses are equal, 0 otherwise
 * 
 */
int loraaddr_compare(const lora_addr_t *addr1, const lora_addr_t *addr2);

/**
 * \brief Set the LoRa address of the node
 * \param addr The address to set
 * 
 */
void loraaddr_set_node_addr(lora_addr_t *addr);

/**
 * \brief Print a LoRa address
 * \param addr The address to print
 * 
 */
void loraaddr_print(const lora_addr_t *addr);

/**
 * \brief Check if a LoRa address is in the DAG
 * \param addr The address to check
 * \return 1 if the address is in the DAG, 0 otherwise
 * 
 */
bool loraaddr_is_in_dag(lora_addr_t *addr);
/*---------------------------------------------------------------------------*/
/*
Conversion function between lora_addr_t and uip_ip6addr_t.
An IPv6 address is build as follow:

|<-----1----->|<--6-->|<-----1----->|<----------6---------->|<---2--->|
| IPv6 PREFIX | ZEROS | LORA PREFIX | COMMON_LINK_ADDR_PART | NODE_ID |
 0           0 1     6 7           7 8                    13 14     15

Where:
  - IPv6 PREFIX is 0xFD
  - LORA PREFIX is The prefix of the lora_addr_t
  - COMMON_LINK_ADDR_PART is the 6 first bytes of the link layer address and defined as follow: 0x02, 0x12, 0x4B, 0x00, 0x06, 0x0D
  - NODE_ID is the node-id of the node
*/

/**
 * \brief Convert a LoRa to an IPv6 address
 * \param src_addr The LoRa address to convert
 * \param dest_addr The IPv6 address to fill
 * 
 */
void lora2ipv6(lora_addr_t *src_addr, uip_ip6addr_t *dest_addr);

/**
 * \brief Convert an IPv6 address to a LoRa address
 * \param src_addr The IPv6 address to convert
 * \param dest_addr The LoRa address to fill
 * 
 */
void ipv62lora(uip_ip6addr_t *src_addr, lora_addr_t *dest_addr);
/*---------------------------------------------------------------------------*/
extern const lora_addr_t lora_root_addr; // The root address
extern lora_addr_t lora_node_addr; // The node address
extern const lora_addr_t lora_null_addr; // The null address
/*---------------------------------------------------------------------------*/

#define LOG_LORA_ADDR(level, lora_addr) do {  \
    if((level) <= (LOG_LEVEL)) { \
        loraaddr_print(lora_addr); \
    } \
    } while (0)

#define LOG_INFO_LORA_ADDR(...)    LOG_LORA_ADDR(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DBG_LORA_ADDR(...)    LOG_LORA_ADDR(LOG_LEVEL_DBG, __VA_ARGS__)

#endif /* LORAADDR_H_ */
