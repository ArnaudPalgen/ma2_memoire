#include "contiki.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "sys/node-id.h"
#include "loraphy.h"
#include "loramac.h"
#include "net/linkaddr.h"
#include "sys/log.h"
#include "mybuffer.h"


#define LOG_MODULE "LoRa MAC"
#define LOG_LEVEL LOG_LEVEL_DBG

#define QUERY_TIMEOUT (CLOCK_SECOND * 5)
#define RETRANSMIT_TIMEOUT CLOCK_SECOND

#define MAX_RETRANSMIT 3
#define BUF_SIZE 10

static lora_addr_t node_addr;

static state_t state;

static struct ctimer retransmit_timer;
static struct ctimer query_timer;

static lora_frame_t buffer[BUF_SIZE];
static myqueue queue;
static mac_command_t resp_buffer[BUF_SIZE];
static myqueue resp_queue;

static lora_frame_t last_send_frame;
static uint8_t expected_ack_num = 0;
static mac_command_t expected_response = JOIN_RESPONSE;
static uint8_t retransmit_attempt=0;

static const lora_addr_t root_addr={ROOT_PREFIX, ROOT_ID};


/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/

void process_frame(lora_frame_t *frame){
    LOG_DBG("process frame");
    printLoraFrame(frame);
    LOG_DBG("\n");
    mac_command_t command = frame->command;
    uint8_t ack_num;

    if(command != expected_response && expected_response>-1){
        command = -1;//match noting in the swith
        LOG_DBG("unexpected mac command %d\n", command);
    }

    switch (command)
    {
    case JOIN_RESPONSE:
        if(state == ALONE && isForRoot(&(frame->dest_addr)) && strlen(frame->payload) == 2){
            node_addr.prefix = (uint8_t) strtol(frame->payload, NULL, 16);
            state = JOINED;
            ctimer_stop(&retransmit_timer);
            LOG_INFO("Lora Root joined. \n");
            LOG_INFO("Node addr: ");
            printLoraAddr(&node_addr);
            LOG_INFO("\n");
        }
        break;
    
    case DATA:
        if(state != ALONE && isForRoot(&(frame->dest_addr))){
            //todo send payload to upper layer
            LOG_DBG("Data for root");
        }else if(state == READY && isForChild(&(frame->dest_addr))){
            //todo send to RPL
            LOG_DBG("Data for child");
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
        
        ack_num = (uint8_t) strtol(frame->payload, NULL, 16);
        LOG_DBG("received ack_num: %d\n", ack_num);
        if(ack_num == expected_ack_num){
            ctimer_stop(&retransmit_timer);
            state = READY;
        }
        break;

    default:
        LOG_WARN("Unknown command %d\n", command);
    }
}

void rpl_on(){
    state = READY;
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

void retransmit_timeout(void *ptr){
    LOG_DBG("retransmit TIMEOUT\n");
    if(retransmit_attempt>=MAX_RETRANSMIT){
        LOG_WARN("Unable to send frame\n");
        retransmit_attempt = 0;
        send_next();
    }else{
        retransmit_attempt ++;
        phy_tx(last_send_frame);
        ctimer_reset(&retransmit_timer);
    }
}

int mac_process(lora_frame_t *frame, mac_command_t f_expected_response){
    LOG_DBG("mac_process: frame:");
    printLoraFrame(frame);
    LOG_DBG(" expected response: %d\n", expected_response);
    if(queue.current_item<BUF_SIZE && frame !=NULL && f_expected_response>0){//append frame to buffer
        LOG_DBG("append to buffer");
        queue_append(&queue, frame);
        queue_append(&resp_queue, &f_expected_response);
    }
    if(queue.current_item>0 && state != WAIT_RESPONSE){//send next
        queue_pop(&queue, &last_send_frame);
        queue_pop(&resp_queue, &expected_response);
        LOG_DBG("send ");
        printLoraFrame(&last_send_frame);
        LOG_DBG("with expected response: %d\n", expected_response);

        LOG_DBG("disable watchdog timer and send\n");
        phy_timeout(0);//disable watchdog timer
        phy_tx(last_send_frame);
        if(expected_response>=0){
            LOG_DBG("waiting for an answer: %d\n", expected_response);
            state = WAIT_RESPONSE;
            LOG_DBG("state is: %d\n", state);
            LOG_DBG("set watchdog timer and start rx\n");
            phy_timeout(10000);
            phy_rx();
            LOG_DBG("start retransmit timer\n");
            ctimer_set(&retransmit_timer, RETRANSMIT_TIMEOUT, retransmit_timeout, NULL);
        }
    }
    return 0;
}

void send_next(){
    LOG_DBG("send next\n");
    mac_process(NULL, -1);
}

void mac_tx(lora_frame_t frame){
    LOG_DBG("mac tx: ");
    printLoraFrame(&frame);
    LOG_DBG(" ");
    if(frame.k){
        LOG_DBG("need ACK\n");
        mac_process(&frame, ACK);
    }else{
        LOG_DBG("don't need ACK\n");
        mac_process(&frame, -1);
    }
    
}

void query_timeout(void *ptr){
    LOG_DBG("Query timeout\n");
    lora_frame_t query_frame={node_addr, root_addr, false, QUERY, ""};
    mac_process(&query_frame, DATA);
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
    LOG_DBG("initial state: %d\n", state);

    /* start phy layer */
    phy_init();
    phy_register_listener(&lora_rx);

    /*init query timer*/
    ctimer_set(&query_timer, QUERY_TIMEOUT, query_timeout, NULL);
    LOG_DBG("query timer started\n");
    //ctimer_set(&timer_ctimer, CLOCK_SECOND, ctimer_callback, NULL);

    /*init queue*/
    queue_init(&queue, sizeof(lora_frame_t), BUF_SIZE, buffer);
    queue_init(&resp_queue, sizeof(mac_command_t), BUF_SIZE, resp_buffer);
    LOG_DBG("buffer init\n");


    /*send join request to LoRa root*/
    lora_frame_t join_frame={node_addr, root_addr, false, JOIN, NULL};
    mac_process(&join_frame, JOIN_RESPONSE);
    LOG_DBG("JOIN_REQUEST sended\n");
    
    
    //uart_tx(join_frame);
    //phy_timeout()
    //phy_rx()

    //join_request = toto
    //PHY_send(join_reQUEST)
    //last_send = join_request
    //expected frame = ack0
}