/*
 * Simple PING-PONG example that use LoRaMAC.
 */

#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"
/*-------------------------------------------------------------------------------------*/
#define LOG_MODULE "APP"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT	2102
#define UDP_SERVER_PORT	2511

#define TEST_INTERVAL 3

static struct simple_udp_connection udp_conn;
static uint16_t count=0;

PROCESS(udp_process, "UDP process");
AUTOSTART_PROCESSES(&udp_process);
/*-------------------------------------------------------------------------------------*/
static void
udp_rx(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
    static char payload[16];
    LOG_INFO("Received PONG %s from ", (char *) data);
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");
    snprintf(payload, sizeof(payload), "PING %d", count);

    LOG_INFO("Send PING\n");
    simple_udp_sendto(&udp_conn, payload, strlen(payload), sender_addr);

}
/*-------------------------------------------------------------------------------------*/
PROCESS_THREAD(udp_process, ev, data)
{
    static char payload[16];
    static uip_ipaddr_t dest_ipaddr;
    static struct etimer timer;
    bool network_joined = false;


    PROCESS_BEGIN();

    /*Init UDP connection*/
    simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                    UDP_SERVER_PORT, udp_rx);

    /* 
     * set an IPv6 destination address that does not belong to the RPL network
     * so send the packet to the LoRa root
     */
    uip_ip6addr_u8(&dest_ipaddr, 0, '_','u','m','o','n','s', '_', 'l','o','r','a','m','a','c', 0);
    while(! network_joined){
        if(NETSTACK_ROUTING.node_is_reachable()) {
            /* network  joined so send the first PING */
            network_joined = true;
            snprintf(payload, sizeof(payload), "PING %d", count);
            LOG_INFO("Send PING\n");
            simple_udp_sendto(&udp_conn, payload, strlen(payload), &dest_ipaddr);
            count ++;
        }else{
            LOG_INFO("Network not joined\n");
            /* RPL network not joined wait before try again */
            etimer_set(&timer, (TEST_INTERVAL*CLOCK_SECOND));
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
        }
    }

    PROCESS_END();
}
