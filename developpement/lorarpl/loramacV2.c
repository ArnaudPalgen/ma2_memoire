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
static struct ctimer query_timer;

//buffers
static lora_frame_t buffer[BUF_SIZE];
static uint8_t w_i = 0;// index to write in the buffer
static uint8_t r_i = 0;// index to read in the buffer 
static uint8_t buf_len = 0;// current size of the buffer
mutex_t tx_buf_mutex;// mutex for tx_buffer

static lora_frame_t last_send_frame;
static bool seq = false;
static uint8_t retransmit_attempt=0;

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

void setState(state_t new_state){
    state = new_state;
    process_post(&mac_tx, state_change_event, NULL);
}

void phy_send(lora_frame_t frame){
    phy_timeout(0);//disable watchdog timer
    phy_tx(frame);
}

int mac_send(lora_frame_t frame){
    LOG_WARN("MAC_SEND A\n");
    //acquire mutex for buffer
    while(!mutex_try_lock(&tx_buf_mutex)){}
    LOG_WARN("MAC_SEND B\n");

    if(buf_len <= BUF_SIZE){
        LOG_WARN("MAC_SEND C\n");
        buffer[w_i] = frame;
        LOG_WARN("MAC_SEND D\n");
        buf_len++;
        LOG_WARN("MAC_SEND E\n");
        w_i = (w_i+1)%BUF_SIZE;
        LOG_WARN("MAC_SEND F\n");
        mutex_unlock(&tx_buf_mutex);
        LOG_WARN("MAC_SEND G\n");
        process_post(&mac_tx, new_tx_frame_event, NULL);        
        LOG_WARN("MAC_SEND H\n");
        return 0;
    }else{
        LOG_WARN("MAC_SEND I\n");
        mutex_unlock(&tx_buf_mutex);
        LOG_WARN("MAC_SEND J\n");
        return 1;
    }
}

void send_ack(lora_addr_t ack_dest_addr, bool ack_seq){
    static lora_frame_t ack_frame;
    
    ack_frame.src_addr = node_addr;
    ack_frame.seq = ack_seq;
    ack_frame.command=ACK;
    ack_frame.next=false;
    ack_frame.k=false;
    ack_frame.dest_addr = ack_dest_addr;

    last_send_frame = ack_frame;
    phy_send(ack_frame);
}

/*---------------------------------------------------------------------------*/

//timeout callback functions
void retransmit_timeout(void *ptr){
    LOG_INFO("retransmit timeout !\n");
    LOG_ERR("STOP retransmit timer thanks to retransmit_timeout\n");
    ctimer_stop(&retransmit_timer);
    if(retransmit_attempt < MAX_RETRANSMIT){
        LOG_DBG("retransmit frame: ");
        LOG_DBG_LR_FRAME(&last_send_frame);
        LOG_DBG("\n");
        phy_send(last_send_frame);
        retransmit_attempt ++;
        LOG_ERR("RESTART retransmit timer because retransmit frame");
        ctimer_restart(&retransmit_timer);
    }else{
        retransmit_attempt = 0;
        seq = !seq;
        setState(READY);
    }
}

void query_timeout(void *ptr){
    LOG_INFO("query timeout !\n");
    LOG_ERR("STOP query timer thaks to query_timeout\n");
    ctimer_stop(&query_timer);
    lora_frame_t query_frame;
    query_frame.src_addr = node_addr;
    query_frame.dest_addr=root_addr;
    query_frame.k=false;
    query_frame.seq=seq;
    query_frame.next=false;
    query_frame.command=QUERY;
    query_frame.payload="";
    // = {node_addr, root_addr, false, seq, false, QUERY, ""};
    LOG_DBG("query frame built\n");
    mac_send(query_frame);
}
/*---------------------------------------------------------------------------*/


void on_join_response(lora_frame_t* frame){
    LOG_DBG("JOIN RESPONSE\n");
    if(state == ALONE && forRoot(&(frame->dest_addr)) && strlen(frame->payload) == 2){
        node_addr.prefix = (uint8_t) strtol(frame->payload, NULL, 16);
        //state = JOINED;
        setState(READY);
        LOG_ERR("STOP retransmit timer\n");
        ctimer_stop(&retransmit_timer);
        
        LOG_INFO("Lora Root joined\n");
        LOG_INFO("Node addr: ");
        LOG_INFO_LR_ADDR(&node_addr);
        LOG_INFO("\n");

        send_ack(root_addr, seq);
        seq = !seq;
        
        process_start(&mac_tx, NULL);
        LOG_ERR("SET query timer\n");
        ctimer_set(&query_timer, QUERY_TIMEOUT, query_timeout, NULL);

    }
    else if(state != ALONE && forRoot(&(frame->dest_addr)) && strlen(frame->payload) == 2){
        send_ack(root_addr, false);
    }
}

void on_data(lora_frame_t* frame){
    LOG_INFO("DATA!\n");

    //ctimer_stop(&retransmit_timer);
    //ctimer_stop(&query_timer);

    if(forRoot(&(frame->dest_addr))){
        LOG_DBG("Data for root\n");//todo send payload to upper layer

    }else if(state == READY && forChild(&(frame->dest_addr))){
        LOG_DBG("Data for child\n");//todo send to RPL
    }

    if(frame->k){
        send_ack(frame->dest_addr, seq);
    }
    seq = !seq;
    if(frame->next){
        phy_timeout(RX_TIME);
        phy_rx();
    }else{
        //ctimer_restart(&query_timer);
        setState(READY);
    }
}

void on_ack(lora_frame_t* frame){
    LOG_INFO("ACK!\n");
    LOG_ERR("STOP retransmit timer thanks to ACK");
    ctimer_stop(&retransmit_timer);
    seq=!seq;
    setState(READY);
}

int lora_rx(lora_frame_t frame){
    
    LOG_INFO("RX LoRa frame: ");
    LOG_INFO_LR_FRAME(&frame);
    LOG_INFO("\n");
    
    if(!forDag(&(frame.dest_addr)) || frame.seq != seq){//dest addr is not the node -> drop frame
        if(frame.seq != seq){
            phy_send(last_send_frame);
        }
        LOG_DBG("drop frame\n");
        return 0;
    }

    mac_command_t command = frame.command;

    switch (command){
        case JOIN_RESPONSE:
            on_join_response(&frame);
            break;
        case DATA:
            if(state != ALONE){
                on_data(&frame);
            }
            break;
        case ACK:
            on_ack(&frame);
            break;
        default:
            LOG_WARN("Unknown MAC command %d\n", command);
    }
    return 0;
}

int send(lora_addr_t to, bool need_ack,void* data){
    lora_frame_t frame = {node_addr, to, need_ack, false, false, DATA, data};
    return mac_send(frame);
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

    new_tx_frame_event = process_alloc_event();
    state_change_event = process_alloc_event();

    /* start phy layer */
    phy_init();
    phy_register_listener(&lora_rx);

    lora_frame_t join_frame = {node_addr, root_addr, false, false, false, JOIN, ""};
    last_send_frame = join_frame;
    phy_send(join_frame);
    phy_timeout(RX_TIME);
    phy_rx();
    LOG_ERR("set retransmit timer\n");
    ctimer_set(&retransmit_timer, RETRANSMIT_TIMEOUT, retransmit_timeout, NULL);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mac_tx, ev, data){
    
    
    bool buf_not_empty = true;
    
    PROCESS_BEGIN();
    
    while(true){
        LOG_WARN("YO 2\n");
        LOG_DBG("TEST1 is new tx frame: %d\n",ev==new_tx_frame_event);
        LOG_DBG("TEST2 is state change: %d\n",ev==state_change_event);
        LOG_WARN("YO 2 B !\n");
        PROCESS_WAIT_EVENT_UNTIL(ev==new_tx_frame_event);
        LOG_WARN("YO 3\n");
        LOG_DBG("false: %d\n", false);
        LOG_DBG("event is new tx frame: %d\n",ev==new_tx_frame_event);
        LOG_DBG("event is state change: %d\n",ev==state_change_event);
        do
        {   LOG_WARN("PROCESS A\n");
            
            //acquire mutex for buffer
            while(!mutex_try_lock(&tx_buf_mutex)){}
            LOG_WARN("PROCESS B\n");
            if(buf_len==0){
                LOG_WARN("YO!\n");
                break;
            }
            last_send_frame = buffer[r_i];
            LOG_DBG("--- buf size: %d\n", buf_len);
            LOG_WARN("PROCESS C\n");
            buf_len = buf_len-1;
            LOG_DBG("--- buf size: %d\n", buf_len);
            LOG_WARN("PROCESS D\n");
            r_i = (r_i+1)%BUF_SIZE;
            LOG_WARN("PROCESS E\n");
            buf_not_empty = (buf_len>0);
            LOG_DBG("--- buf size: %d\n", buf_len);
            LOG_WARN("PROCESS F\n");
            mutex_unlock(&tx_buf_mutex);
            LOG_WARN("PROCESS G\n");
            last_send_frame.seq = seq;
            LOG_WARN("PROCESS H\n");
            phy_timeout(0);//disable watchdog timer
            LOG_WARN("PROCESS I\n");
            phy_tx(last_send_frame);
            LOG_WARN("PROCESS J\n");
            if(last_send_frame.k || last_send_frame.command == QUERY){
                state = WAIT_RESPONSE;
                LOG_ERR("RESTART retransmit timer in mac_tx process\n");
                ctimer_restart(&retransmit_timer);
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