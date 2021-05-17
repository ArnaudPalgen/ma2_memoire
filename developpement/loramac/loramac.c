#include "contiki.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "sys/node-id.h"
#include "loraphy.h"
#include "loramac.h"
#include "net/linkaddr.h"
#include "sys/log.h"

#define LOG_MODULE "LoRa MAC"
#define LOG_LEVEL LOG_LEVEL_DBG

//node adress
static lora_addr_t node_addr;

//current state
static state_t state;
//static lora_frame_t last_send_frame;
//static uint8_t expected_ack_num = 0;

lora_addr_t root_addr={ROOT_PREFIX, ROOT_ID};

PROCESS(loramac_process, "lora-mac-process");
AUTOSTART_PROCESSES(&loramac_process);

void printLoraAddr(lora_addr_t *addr){
    printf("%d:%d", addr->prefix, addr->id);
}

void printLoraFrame(lora_frame_t *frame){
    printf("{src:");
    printLoraAddr(&(frame->src_addr));
    printf(" dest:");
    printLoraAddr(&(frame->dest_addr));
    printf(" k:%d cmd:%d data: %s}", frame->k, frame->command, frame->payload);
}

bool checkDest(lora_addr_t *dest_addr){
    // if frame is for this RPL root or for RPL child of this root
    return dest_addr->prefix == node_addr.prefix || dest_addr->id == node_addr.id;
}

bool isForRoot(lora_addr_t *dest_addr){
    return dest_addr->prefix == node_addr.prefix && dest_addr->id == node_addr.id;
}

bool isForChild(lora_addr_t *dest_addr){
    return dest_addr->prefix == node_addr.prefix && dest_addr->id != node_addr.id;
}

void process_frame(lora_frame_t *frame){
    mac_command_t command = frame->command;

    switch (command)
    {
    case JOIN_RESPONSE:
        if(state == ALONE && isForRoot(&(frame->dest_addr)) && strlen(frame->payload) == 2){
            node_addr.prefix = (uint8_t) strtol(frame->payload, NULL, 16);
            state = JOINED;
        }
        break;
    
    case DATA:
        if(state != ALONE && isForRoot(&(frame->dest_addr))){
            //todo send payload to upper layer
        }else if(state == READY && isForChild(&(frame->dest_addr))){
            //todo send to RPL
        }
        break;

    case PING:
        if(state != ALONE && isForRoot(&(frame->dest_addr))){
            //todo send ping back to src
        }else if(state == READY && isForChild(&(frame->dest_addr))){
            //todo check if child exist 
        }
        
        break;

    case PONG:
        //todo
        break;
    case ACK:
        //todo
        break;

    default:
        LOG_WARN("Unknown command %d\n", command);
    }
}

int lora_rx(lora_frame_t frame){
    LOG_DBG("Lora frame: ");
    printLoraFrame(&frame);
    LOG_DBG("\n");

    if(checkDest(&(frame.dest_addr))){
        process_frame(&frame);
    }
    
    return 0;
}

void mac_init(){
    LOG_INFO("Init LoRa MAC");

    /* set custom link_addr */
    unsigned char new_linkaddr[8] = {'_','u','m','o','n','s',linkaddr_node_addr.u8[LINKADDR_SIZE - 2],linkaddr_node_addr.u8[LINKADDR_SIZE - 1]};
    linkaddr_t new_addr;
    memcpy(new_addr.u8, new_linkaddr, 8*sizeof(unsigned char));
    linkaddr_set_node_addr(&new_addr);
    
    LOG_INFO("Node ID: %u\n", node_id);
    LOG_INFO("New Link-layer address: ");
    LOG_INFO_LLADDR(&linkaddr_node_addr);
    LOG_INFO_("\n");

    
    /* set initial LoRa address */
    node_addr.prefix = NULL_PREFIX;
    node_addr.id = node_id;

    state = ALONE;//initial state

    /* start phy layer */
    phy_init();
    phy_register_listener(&lora_rx);

    
    //lora_frame_t join_frame={node_addr, root_addr, false, JOIN, NULL};
    //uart_tx(join_frame);
    //phy_timeout()
    //phy_rx()

    //join_request = toto
    //PHY_send(join_reQUEST)
    //last_send = join_request
    //expected frame = ack0
    //start process(max retransmit=3, timeout=1)
}


PROCESS_THREAD(loramac_process, ev, data){
    PROCESS_BEGIN();
    mac_init();
    lora_frame_t join_frame={node_addr, root_addr, false, JOIN, NULL};
    phy_tx(join_frame);
    phy_tx(join_frame);
    phy_timeout(3000);
    phy_rx();
    phy_tx(join_frame);


    //start timer
    //on timeout: retransmit
    //if retransmit > max_retransmit: drop packet

    //
    //printf("real frame: ");
    //printLoraFrame(&join_frame);
    //printf("\n");
    //printf("POINTEUR: %p\n", &join_frame);//toto
    //send_phy(join_frame);

    PROCESS_END();
}