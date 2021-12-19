/*
 * Contains code to test the LoRaMAC stack in 3 different ways
 * depending on the mode of operation.
 * 
 *  - PINGPONG: Send the first PING and respond by PING when a PONG is received
 *  - RECEIVER: Send a SYN and receive others messages
 *  - SENDER: Send messages
 *  - SLEEP: Do nothing (mainly for the RPL root)
 * 
 */

#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"
#include "random.h"
#include "arch/cpu/cc2538/lpm.h"
/*-------------------------------------------------------------------------------------*/
/* Macros definition */

/* LOG configuration */
#define LOG_MODULE "APP"
#define LOG_LEVEL LOG_LEVEL_INFO

/* UDP ports configuration */
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

/* Differents intervals */
#define TEST_INTERVAL (CLOCK_SECOND * 3)
#define SEND_INTERVAL (CLOCK_SECOND * 5)
//#define SEND_INTERVAL_WITH_JITTER

/* Maximum numbers of packets to send */
#define MAX_PAQUET_COUNT 500

/* Mode of operation */

#define PINGPONG 0
#define RECEIVER 1
#define SENDER 2
#define SLEEP 3

#define NODE_MODE 0
/*-------------------------------------------------------------------------------------*/
/* Process configuration */
PROCESS(app_process, "APP process");
AUTOSTART_PROCESSES(&app_process);
/*-------------------------------------------------------------------------------------*/
/* Global variables */
#if NODE_MODE != SLEEP
static struct simple_udp_connection udp_conn;
static uint16_t count = 0;
static char payload[15];
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
    LOG_INFO("Received '%.*s' from ", datalen, (char *) data);
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");

    #if NODE_MODE == PINGPONG
        if (count < MAX_PAQUET_COUNT){
            LOG_INFO("Sending PING %d to ", count);
            LOG_INFO_6ADDR(sender_addr);
            LOG_INFO_("\n");

            snprintf(payload, sizeof(payload), "PING %d", count);
            simple_udp_sendto(&udp_conn, payload, strlen(payload), sender_addr);
            count ++;
        }else{
            LOG_INFO("END\n");
        }
    #endif
}
#endif
/*-------------------------------------------------------------------------------------*/
PROCESS_THREAD(app_process, ev, data)
{
    lpm_set_max_pm(LPM_PM0);
    #if NODE_MODE != SLEEP
        static struct etimer periodic_timer;
        uip_ipaddr_t dest_ipaddr;
    #endif
    
    PROCESS_BEGIN();
    LOG_INFO("APP started\n");
    LOG_INFO("MODE: %d\n", NODE_MODE);
    #ifdef LORAMAC_IS_ROOT
        LOG_INFO("LORAMAC_IS_ROOT is defined\n");
    #endif
    
    /* If the node must do nothing */
    #if NODE_MODE == SLEEP
        while(1) {
            PROCESS_YIELD();
            LOG_INFO("APP PROCESS\n");
        }
    #else
        /* Initialize UDP connection */
        simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                            UDP_SERVER_PORT, udp_rx);

        /* Wait that the RPL network is reachable */
        etimer_set(&periodic_timer, TEST_INTERVAL);
        while(!NETSTACK_ROUTING.node_is_reachable()){
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
            LOG_INFO("Network not joined\n");
            etimer_set(&periodic_timer, TEST_INTERVAL);
        }
        LOG_INFO("Network joined\n");

        /* 
         * Set an IPv6 destination address that does not belong to the RPL network
         * so send the packet to the LoRa root
         */
        uip_ip6addr_u8(&dest_ipaddr, 0, '_','u','m','o','n','s', '_', 'l','o','r','a','m','a','c', 0);

        #if ((NODE_MODE == PINGPONG) || (NODE_MODE == RECEIVER))
            if(NODE_MODE == PINGPONG){
                snprintf(payload, sizeof(payload), " PING %d", count);
                count ++;
            }else{
                snprintf(payload, sizeof(payload), "SYN");
            }
            
            LOG_INFO("Sending %s to ", payload);
            LOG_INFO_6ADDR(&dest_ipaddr);
            LOG_INFO_("\n");
            
            simple_udp_sendto(&udp_conn, payload, strlen(payload), &dest_ipaddr);
        #else
            #ifdef SEND_INTERVAL_WITH_JITTER
                etimer_set(&periodic_timer, SEND_INTERVAL - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
            #else
                etimer_set(&periodic_timer, SEND_INTERVAL);
            #endif
            while(count < MAX_PAQUET_COUNT){
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
                LOG_INFO("Sending hello %d to", count);
                LOG_INFO_6ADDR(&dest_ipaddr);
                LOG_INFO_("\n");
                
                snprintf(payload, sizeof(payload), "hello %d", count);
                simple_udp_sendto(&udp_conn, payload, strlen(payload), &dest_ipaddr);
                count ++;
                etimer_reset(&periodic_timer);
            }
        #endif

    #endif

    LOG_INFO("END\n");
    PROCESS_END();
}
