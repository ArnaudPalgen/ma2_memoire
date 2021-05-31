#include "contiki.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "sys/node-id.h"
#include "loraphy.h"
#include "loramacV2.h"
#include "net/linkaddr.h"
#include "sys/log.h"
#include "sys/mutex.h"

/*---------------------------------------------------------------------------*/
//variables definition

#define LOG_MODULE "LoRa MAC"
#define LOG_LEVEL LOG_LEVEL_DBG

//LoRa addr
static const lora_addr_t root_addr={ROOT_PREFIX, ROOT_ID};
static lora_addr_t node_addr;

//MAC state
static state_t state;

//timers
static struct ctimer retransmit_timer;
//static struct ctimer query_timer;

//buffers
static lora_frame_t buffer[BUF_SIZE];
static uint8_t w_i = 0;// index to write in the buffer
static uint8_t r_i = 0;// index to read in the buffer 
static uint8_t buf_size = 0;// current size of the buffer
mutex_t tx_buf_mutex;// mutex for tx_buffer

static lora_frame_t last_send_frame;
static bool seq = false;
//static uint8_t retransmit_attempt=0;

static process_event_t new_tx_frame_event;//event that signals to the TX process that a new frame is available to be sent 
static process_event_t state_change_event;//event that signals to the TX process that ..TODO

PROCESS(mac_tx, "LoRa-MAC tx process");

/*---------------------------------------------------------------------------*/
//logging functions
void print_lora_addr(lora_addr_t *addr){
    printf("%d:%d", addr->prefix, addr->id);
}

void print_lora_frame(lora_frame_t *frame){
    printf("{src:");
    print_lora_addr(&(frame->src_addr));
    printf(" dest:");
    print_lora_addr(&(frame->dest_addr));
    printf(" k:%d seq:%d next:%d cmd:%d data:%s", frame->k, frame->seq,\
        frame->next, frame->command, frame->payload);
}
/*---------------------------------------------------------------------------*/
//functions that check dest of a lora_addr
bool forDag(lora_addr_t *dest_addr){
    // if frame is for this RPL root or for RPL child of this root
    return dest_addr->prefix == node_addr.prefix || dest_addr->id == node_addr.id;
}

bool forRoot(lora_addr_t *dest_addr){
    return dest_addr->prefix == node_addr.prefix && dest_addr->id == node_addr.id;
}

bool forChild(lora_addr_t *dest_addr){
    return dest_addr->prefix == node_addr.prefix && dest_addr->id != node_addr.id;
}

/*---------------------------------------------------------------------------*/
//timeout callback functions
void retransmit_timeout(void *ptr){
    LOG_INFO("retransmit timeout !\n");
}

void query_timeout(void *ptr){
    LOG_INFO("query timeout !\n");
}
/*---------------------------------------------------------------------------*/

void phy_send(lora_frame_t frame){
    phy_timeout(0);//disable watchdog timer
    phy_tx(frame);
}

int lora_rx(lora_frame_t frame){
    
    LOG_INFO("RX LoRa frame: ");
    LOG_INFO_LR_FRAME(&frame);
    LOG_INFO("\n");
    
    if(!forDag(&(frame.dest_addr)) || frame.seq != seq){//dest addr is not the node -> drop frame
       LOG_DBG("drop frame\n");
        return 0;
    }

    mac_command_t command = frame.command;

    switch (command)
    {
        case JOIN_RESPONSE:
            LOG_DBG("HEEEEEEEEEEEEEEEEEEEEEEEEEEEEEERE\n");
            if(state == ALONE && forRoot(&(frame.dest_addr)) && strlen(frame.payload) == 2){
                node_addr.prefix = (uint8_t) strtol(frame.payload, NULL, 16);
                state = JOINED;
                ctimer_stop(&retransmit_timer);
                
                LOG_INFO("Lora Root joined\n");
                LOG_INFO("Node addr: ");
                LOG_INFO_LR_ADDR(&node_addr);
                LOG_INFO("\n");

                lora_frame_t ack_frame = {node_addr, root_addr, false, seq, false, ACK, ""};
                phy_send(ack_frame);
                seq = !seq;
                
                process_start(&mac_tx, NULL);
                //init query timer

            }
            else if(state != ALONE && forRoot(&(frame.dest_addr)) && strlen(frame.payload) == 2){
                lora_frame_t ack_frame = {node_addr, root_addr, false, false, false, ACK, ""};
                phy_send(ack_frame);
            }
            break;

        case DATA:
            LOG_INFO("DATA!\n");
            if(state != ALONE && forRoot(&(frame.dest_addr))){
                //todo send payload to upper layer
                LOG_DBG("Data for root\n");
            }else if(state == READY && isForChild(&(frame->dest_addr))){
                //todo send to RPL
                LOG_DBG("Data for child\n");
            }
            if(frame.k){
                lora_frame_t ack_frame = {node_addr, frame.dest_addr, false, seq, false, ACK, ""};
                phy_send(ack_frame);
            }
            seq = !seq;
            if(frame.next){
                phy_timeout(RX_TIME);
                phy_rx();
            }
            break;

        case ACK:
            LOG_INFO("ACK!\n");
            ctimer_stop(&retransmit_timer);
            seq=!seq;
            break;

        default:
            LOG_WARN("Unknown MAC command %d\n", command);
    }
    return 0;

}

int mac_send(lora_frame_t frame){
    //acquire mutex for buffer
    while(!mutex_try_lock(&tx_buf_mutex)){}

    if(buf_size <= BUF_SIZE){
        buffer[w_i] = frame;
        buf_size++;
        w_i = (w_i+1)%BUF_SIZE;
        mutex_unlock(&tx_buf_mutex);
        return 0;
    }
    return 1;
    

}

int send(lora_addr_t to, bool need_ack,void* data){
    lora_frame_t frame = {node_addr, to, need_ack, false, false, DATA, data};
    mac_send(frame);
}

void mac_init(){
    LOG_INFO("Init LoRa MAC\n");
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
    node_addr.prefix = node_id;//first 8 bits of the node_id
    node_addr.id = node_id;

    /* set initial state*/
    state = ALONE;
    LOG_DBG("initial state: %d\n", state);

    /* start phy layer */
    phy_init();
    phy_register_listener(&lora_rx);

    lora_frame_t join_frame = {node_addr, root_addr, false, false, false, JOIN, ""};
    phy_send(join_frame);
    phy_timeout(RX_TIME);
    phy_rx();
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mac_tx, ev, data){
    
    
    bool buf_not_empty = true;
    
    PROCESS_BEGIN();
    
    while(true){
        PROCESS_WAIT_EVENT_UNTIL(ev==new_tx_frame_event);
        do
        {
            if(state != READY){continue;}
            
            //acquire mutex for buffer
            while(!mutex_try_lock(&tx_buf_mutex)){}
            
            last_send_frame = buffer[r_i];
            buf_size --;
            r_i = (r_i+1)%BUF_SIZE;
            buf_not_empty = (buf_size>0);
            mutex_unlock(&tx_buf_mutex);

            last_send_frame.seq = seq;
            phy_timeout(0);//disable watchdog timer
            phy_tx(last_send_frame);
            if(last_send_frame.k){
                state = WAIT_RESPONSE;
                ctimer_start(&retransmit_timer);
                phy_timeout(RX_TIME);
                phy_rx();
                
                while(state != READY){
                    PROCESS_WAIT_EVENT_UNTIL(ev == state_change_event);
                }

            }else{
                seq = !seq;
            }

            
        } while (buf_not_empty);
        
        
    }
    
    
    PROCESS_END();
}