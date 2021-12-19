/**
 * Contains utility functions for LoRa addresses and
 * the definitions of the node address, the LoRa root address and the null address.
 */

#include "loraaddr.h"
#include "sys/log.h"
/*---------------------------------------------------------------------------*/
/*logging configuration*/
#define LOG_MODULE "LoRa ADDR"
#define LOG_LEVEL LOG_LEVEL_NONE
/*---------------------------------------------------------------------------*/
lora_addr_t lora_node_addr;

const lora_addr_t lora_root_addr = {1, 0};
const lora_addr_t lora_null_addr = {0,0};
/*---------------------------------------------------------------------------*/
void
loraaddr_copy(lora_addr_t *dest, const lora_addr_t *from)
{
    memcpy(dest, from, LORA_ADDR_SIZE);
}
/*---------------------------------------------------------------------------*/
int
loraaddr_compare(const lora_addr_t *addr1, const lora_addr_t *addr2)
{
    LOG_DBG("Compare ");
    LOG_DBG_LORA_ADDR(addr1);
    LOG_DBG(" and ");
    LOG_DBG_LORA_ADDR(addr2);
    bool r = ((addr1->prefix == addr2->prefix) && (addr1->id == addr2->id));
    LOG_DBG("addresses are %s\n", r ? "equal":"not equal");
    return r;
}
/*---------------------------------------------------------------------------*/
void
loraaddr_set_node_addr(lora_addr_t *addr)
{
    memcpy(&lora_node_addr, addr, LORA_ADDR_SIZE);
}
/*---------------------------------------------------------------------------*/
void
loraaddr_print(const lora_addr_t *addr)
{
    printf("%-2d:%-5d", addr->prefix, addr->id);
}
/*---------------------------------------------------------------------------*/
bool
loraaddr_is_in_dag(lora_addr_t *addr)
{
    LOG_DBG("ADDR ");
    LOG_DBG_LORA_ADDR(addr);
    LOG_DBG(" is%s in dag\n", (addr->prefix == lora_node_addr.prefix) ? " ":" not");
    return addr->prefix == lora_node_addr.prefix;
}
/*---------------------------------------------------------------------------*/
void
lora2ipv6(lora_addr_t *src_addr, uip_ip6addr_t *dest_addr)
{
    uip_ip6addr_u8(dest_addr, 0xFD, 0, 0, 0, 0, 0, 0, src_addr->prefix, 0x02, 0x12, 0x4B, 0x00, 0x06, 0x0D, src_addr->id>>8, src_addr->id);
}
/*---------------------------------------------------------------------------*/
void
ipv62lora(uip_ip6addr_t *src_addr, lora_addr_t *dest_addr)
{
  dest_addr->prefix = src_addr->u8[7];
  dest_addr->id = (src_addr->u8[14] <<8) + src_addr->u8[15];
}
