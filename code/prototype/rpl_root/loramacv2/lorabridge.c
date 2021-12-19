#include "lorabridge.h"
#include "loramac.h"
#include "lorabuf.h"
#include "net/ipv6/uip.h"
#include "net/netstack.h"
#include "net/routing/routing.h"
#include "dev/button-hal.h"
#include "sys/log.h"
#include "loraaddr.h"
#include "loramac-conf.h"
#include <stdio.h>


/*---------------------------------------------------------------------------*/
/*logging configuration*/
#define LOG_MODULE "LoRa BRIDGE"
#define LOG_LEVEL LOG_LEVEL_INFO
/*---------------------------------------------------------------------------*/
static void
init(void)
{
    LOG_INFO("LoRa Stack bridge start\n");
    
    /* Turn off the radio until the node has joined a LoRaMAC network */
    NETSTACK_MAC.off();
    
    /* Start the LoRaMAC network */
    loramac_root_start();
}
/*---------------------------------------------------------------------------*/
/*
 * Send fallback IP packets to the LoRa ROOT with the LoRaMAC protocol.
 * UIP_CONF_BUFFER_SIZE must be set to 279. This  is the 247 bytes of the LoRaMAC
 * payload to which we add 32 bytes of the src/dest IP addresses that are not used.
 */
static int
output(void)
{
    LOG_INFO("Receive packet for LoRa Stack\n");
    int uip_index = 0;
    int datalen = 0;
    uint8_t* buf_p = lorabuf_get_buf();
    lorabuf_set_data_len(0);
    while(uip_index<uip_len){
        if(uip_index==8){
            // skip src and dest ipv6 addr
            // UIP_IPH_LEN is the ipv6 header size
            uip_index= UIP_IPH_LEN;
        }
        memcpy(buf_p, &(uip_buf[uip_index]), 1);
        buf_p++;
        uip_index++;
        datalen++;
    }
    LOG_DBG("UIP datalen: %d\n", datalen);
    lorabuf_set_data_len(datalen);
    lora_addr_t src_addr;
    ipv62lora(&(UIP_IP_BUF->srcipaddr), &src_addr);//convert src address 
    lorabuf_set_addr(LORABUF_ADDR_SENDER, &src_addr);
    LOG_DBG("CONVERTED SRC ADDR: ");
    LOG_DBG_LORA_ADDR(&src_addr);
    lorabuf_set_attr(LORABUF_ATTR_MAC_CONFIRMED, LORA_MAC_CONFIRMED);
    return loramac_send();
}
/*---------------------------------------------------------------------------*/
void
bridge_input(void)
{
    LOG_INFO("Receive packet for UIP Stack\n");
    uip_ip6addr_t ip_src, ip_dest;
    lora2ipv6(lorabuf_get_addr(LORABUF_ADDR_SENDER), &ip_src);
    lora2ipv6(lorabuf_get_addr(LORABUF_ADDR_RECEIVER), &ip_dest);
    memcpy(&(uip_buf), lorabuf_get_buf(), 8);//ipv6 hdr
    memcpy(&(uip_buf[8]), &(ip_src.u8), 16);//ipv6 src addr
    memcpy(&(uip_buf[24]), &(ip_dest.u8), 16);//ipv6 dest addr

    memcpy(&(uip_buf[UIP_IPH_LEN]), lorabuf_get_buf()+8, lorabuf_get_data_len()-8);//ipv6 payload
    uip_len = lorabuf_get_data_len()+32;
    tcpip_input();
}
/*---------------------------------------------------------------------------*/
const struct uip_fallback_interface loramac_interface = {
        init, output
};
/*---------------------------------------------------------------------------*/
void
lora_network_joined()
{
    LOG_INFO("LoRa network joined\n");
    
    uip_ipaddr_t prefix;
    uip_ip6addr_u8(&prefix, 0xFD, 0, 0, 0, 0, 0, 0, lora_node_addr.prefix, 0,0,0,0,0,0,0,0);
    NETSTACK_ROUTING.root_set_prefix(&prefix, NULL);

    NETSTACK_ROUTING.root_start();
    NETSTACK_MAC.on();

}
