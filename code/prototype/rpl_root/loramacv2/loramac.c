/**
 * The LoRaMAC implementation.
 */

#include "sys/log.h"
#include "loramac.h"
#include "loraaddr.h"
#include "lorabuf.h"
#include "framer.h"
#include "loraphy.h"
#include "sys/node-id.h"
#include "lorabridge.h"
#include "random.h"
//#include "dev/gpio.h"
/*---------------------------------------------------------------------------*/
//#define BASE GPIO_PORT_TO_BASE(GPIO_A_NUM)
//#define MASK GPIO_PIN_MASK(5)

/* Log configuration */
#define LOG_MODULE "LoRa MAC"
#define LOG_LEVEL LOG_LEVEL_INFO
static char* mac_states_str[3] = {"ALONE", "READY", "WAIT_RESPONSE"};
static char* mac_command_str[5] = {"JOIN", "JOIN_RESPONSE", "DATA", "ACK", "QUERY"};
/*---------------------------------------------------------------------------*/
static loramac_state_t state;

/*Retransmit and query timer*/
static struct ctimer retransmit_timer;
static struct ctimer query_timer;

static process_event_t loramac_event_output;
static process_event_t loramac_phy_done;

/*Counters*/
static uint8_t next_seq = 0;
static uint8_t expected_seq = 0;
static uint8_t retransmit_attempt=0;

static lora_frame_hdr_t last_sent_frame;
static uint16_t last_sent_datalen;
static uint8_t last_sent_data[LORA_PAYLOAD_BYTE_MAX_SIZE];

static bool pending = false;
static bool pending_query = false;
static bool is_retransmission = false;

PROCESS(loramac_process, "LoRa-MAC process");

/*---------------------------------------------------------------------------*/
void
loramac_print_hdr(lora_frame_hdr_t *hdr)
{
    printf("src: ");
    loraaddr_print(&(hdr->src_addr));
    printf("|dest: ");
    loraaddr_print(&(hdr->dest_addr));
    printf("|K: %s|next: %s", hdr->confirmed ? "true":"false", hdr->next ? "true":"false");
    printf("|cmd: %s", mac_command_str[hdr->command]);
    printf("|seq: %d", hdr->seqno);
}
/*---------------------------------------------------------------------------*/
void
send_frame()
{
    pending = true;
    process_post(&loramac_process, loramac_event_output, NULL);
}
/*---------------------------------------------------------------------------*/
void
send_query()
{   
    lorabuf_set_addr(LORABUF_ADDR_SENDER, &lora_node_addr);
    lorabuf_set_addr(LORABUF_ADDR_RECEIVER, &lora_root_addr);
    lorabuf_set_data_len(0);
    lorabuf_set_attr(LORABUF_ATTR_MAC_CMD, QUERY);
    send_frame();
}
/*---------------------------------------------------------------------------*/
void
set_state(loramac_state_t new_state)
{    
    if (state == WAIT_RESPONSE && new_state == READY && pending_query){
        LOG_INFO("Pending QUERY\n");
        pending_query = false;
        send_query();

    }else{
        LOG_DBG("STATE %s -> %s \n", mac_states_str[state], mac_states_str[new_state]);
        state = new_state;
    }
}
/*---------------------------------------------------------------------------*/
int
loramac_send(void)
{
    LOG_DBG("State is ready: %s\n", (state == READY) ? "true" : "false");
    if(state == READY){
        lorabuf_set_attr(LORABUF_ATTR_MAC_CMD, DATA);
        lorabuf_set_addr(LORABUF_ADDR_RECEIVER, &lora_root_addr);
        send_frame();
        return 0;
    }
    return -1;
}
/*---------------------------------------------------------------------------*/
void
on_query_timeout(void *ptr)
{
    LOG_INFO("Query timeout\n");
    if(state == READY){
        send_query();
    }else{
        pending_query = true;
    }
}
/*---------------------------------------------------------------------------*/
void
on_retransmit_timeout(void *ptr)
{
    LOG_INFO("Retransmit timeout\n");
    if(retransmit_attempt < LORAMAC_MAX_RETRANSMIT){
        is_retransmission = true;
        
        LOG_DBG("Copy last sent frame to lorabuf\n");
        lorabuf_clear();
        memcpy(lorabuf_mac_param_ptr(), &last_sent_frame, sizeof(lora_frame_hdr_t)-(2* sizeof(lora_addr_t)));
        memcpy(lorabuf_get_addr(LORABUF_ADDR_FIRST), &last_sent_frame.src_addr, 2*sizeof(lora_addr_t));
        memcpy(lorabuf_get_buf(), &last_sent_data, last_sent_datalen);
        lorabuf_set_data_len(last_sent_datalen);

        retransmit_attempt ++;

        LOG_INFO("Attempts: %d\n", retransmit_attempt);
        send_frame();
    }else{
        retransmit_attempt = 0;
        LOG_WARN("Sending failed\n");
        if(last_sent_frame.command == JOIN){
            LOG_DBG("For JOIN -> sleep LoRa radio during %s\n",LORAMAC_JOIN_SLEEP_TIME_c);
            
            clock_time_t interval = (LORAMAC_JOIN_SLEEP_TIME+random_rand())%LORAMAC_MAX_JOIN_SLEEP_TIME;
            LORAPHY_SLEEP(LORAMAC_JOIN_SLEEP_TIME_c);
            ctimer_set(&retransmit_timer, interval, on_retransmit_timeout, NULL);
        }else{
            if(last_sent_frame.command == QUERY){
                LOG_WARN("restart query timer\n");
                ctimer_restart(&query_timer);
            }
            set_state(READY);
        }
    }
}
/*---------------------------------------------------------------------------*/
void
on_join_response(void)
{
    LOG_INFO("Receive JOIN RESPONSE (sn: %d, len: %d)\n", lorabuf_get_attr(LORABUF_ATTR_MAC_SEQNO), lorabuf_get_data_len());
    if( state == ALONE &&
        loraaddr_compare(lorabuf_get_addr(LORABUF_ADDR_RECEIVER), &lora_node_addr) &&
        lorabuf_get_data_len()==1 &&
        lorabuf_get_attr(LORABUF_ATTR_MAC_SEQNO)==0)
    {
        LOG_DBG("Valid JOIN RESPONSE. Stop retransmit timer\n");
        ctimer_stop(&retransmit_timer);
        retransmit_attempt = 0;

        lora_addr_t new_addr;
        new_addr.id = node_id;
        memcpy(&(new_addr.prefix), lorabuf_get_buf(), 2);
        LOG_INFO("New node address: ");
        LOG_INFO_LORA_ADDR(&new_addr);
        loraaddr_set_node_addr(&new_addr);

        LOG_DBG("Start query timer\n");
        ctimer_set(&query_timer, LORAMAC_QUERY_TIMEOUT, on_query_timeout, NULL);
        expected_seq ++;

        set_state(READY);
        lora_network_joined();
    }else
    {
        LOG_WARN("Invalid JOIN response expected values: (sn=0, len=1)\n");
    }
}
/*---------------------------------------------------------------------------*/
void
on_data(void)
{
    uint8_t seq = lorabuf_get_attr(LORABUF_ATTR_MAC_SEQNO);
    LOG_INFO("Receive DATA (sn: %d, len: %d)\n", seq, lorabuf_get_data_len());
    if(seq < expected_seq){
        LOG_INFO("SN smaller than expected (%d). Drop frame\n", expected_seq);
        return;
    }
    
    ctimer_stop(&retransmit_timer);
    ctimer_stop(&query_timer);
    retransmit_attempt = 0;
    
    if(seq > expected_seq){
        LOG_INFO("SN greater than expected(%d)\n", expected_seq);
    }
    expected_seq = seq+1;

    if(lorabuf_get_attr(LORABUF_ATTR_MAC_NEXT)){
        bridge_input();
        LOG_DBG("Waiting for another frame. Send QUERY\n");
        send_query();
    }else{
        LOG_DBG("Last frame. Restart QUERY timer\n");
        ctimer_restart(&query_timer);
        set_state(READY);
        bridge_input();
    }
}
/*---------------------------------------------------------------------------*/
void
on_ack(void)
{
    if (!loraaddr_compare(&lora_node_addr, lorabuf_get_addr(LORABUF_ADDR_RECEIVER))){
        LOG_DBG("Wrong ACK destination address\n");
        return;
    }
    
    uint8_t seq = lorabuf_get_attr(LORABUF_ATTR_MAC_SEQNO);
    LOG_INFO("Receive ACK (sn: %d, len: %d)\n", seq, lorabuf_get_data_len());
    
    if(seq != last_sent_frame.seqno){
        LOG_INFO("Incorrect ACK . Drop frame. Expected: %d\n", last_sent_frame.seqno);
        return;
    }
    
    LOG_DBG("Stop retransmit timer\n");
    ctimer_stop(&retransmit_timer);
    retransmit_attempt = 0;
    
    if(last_sent_frame.command == QUERY){
        LOG_DBG("ACK is the response of a QUERY\n");
        LOG_DBG("Restart QUERY timer\n");
        ctimer_restart(&query_timer);
    }
    set_state(READY);
}
/*---------------------------------------------------------------------------*/
void
loramac_input(void)
{
    lora_addr_t *dest_addr = lorabuf_get_addr(LORABUF_ADDR_RECEIVER);
    if(!loraaddr_is_in_dag(dest_addr)){
        LOG_INFO("Received frame with dest addr, ");
        LOG_INFO_LORA_ADDR(lorabuf_get_addr(LORABUF_ADDR_RECEIVER));
        LOG_INFO("is not for this DAG\n");
        return;
    }
    loramac_command_t command = lorabuf_get_attr(LORABUF_ATTR_MAC_CMD);
    switch (command) {
        case JOIN_RESPONSE:
            if(state == ALONE){
                on_join_response();
            }
            break;
        case DATA:
            if(state != ALONE){
                on_data();
            }
            break;
        case ACK:
            if(state != ALONE){
                on_ack();
            }
            break;
        default:
            LOG_WARN("Unknown MAC command\n");
    }
}
/*---------------------------------------------------------------------------*/
void
send_join_request(void)
{
    /*
     *The joining sequence is described by de diagrame below
     *
     *    RPL_ROOT                                                                 LORA_ROOT
     *        | -----------------JOIN[(prefix=node_id[0:8], node_id)]----------------> |
     *        | <-- JOIN_RESPONSE[(prefix=node_id[0:8], node_id), data=new_prefix] --> |
    **/
    LOG_DBG("Send JOIN request\n");
    lorabuf_set_addr(LORABUF_ADDR_SENDER, &lora_node_addr);
    lorabuf_set_addr(LORABUF_ADDR_RECEIVER, &lora_root_addr);
    lorabuf_set_data_len(0);
    lorabuf_set_attr(LORABUF_ATTR_MAC_CMD, JOIN);
    pending = true;
    process_post(&loramac_process, loramac_event_output, NULL);
}
/*---------------------------------------------------------------------------*/
void
set_conf()
{
    static uint8_t i = 0;
    loraphy_param_t radio_config[LORAPHY_NUM_RADIO_PARAM] = {LORAPHY_PARAM_BW, LORAPHY_PARAM_CR, LORAPHY_PARAM_FREQ,
                                                 LORAPHY_PARAM_MODE, LORAPHY_PARAM_PWR, LORAPHY_PARAM_SF};
    char* initial_radio_config[LORAPHY_NUM_RADIO_PARAM] = {LORA_RADIO_BW, LORA_RADIO_CR, LORA_RADIO_FREQ,
                                                           LORA_RADIO_MODE, LORA_RADIO_PWR, LORA_RADIO_SF};
    if(i<LORAPHY_NUM_RADIO_PARAM) {
        LORAPHY_SET_PARAM(radio_config[i], initial_radio_config[i]);
        i++;
    }else if(i==LORAPHY_NUM_RADIO_PARAM){ // all radio parameters are set
        i++;
        /*Start the LoRaMAC process and send the JOIN request*/
        LOG_INFO("LoRaMAC configuration done\n");
        process_start(&loramac_process, NULL);
        send_join_request();
    }else{ // the sending of the JOIN request is done
        process_post(&loramac_process, loramac_phy_done, NULL);
    }
}
/*---------------------------------------------------------------------------*/
void
phy_callback(loraphy_sent_status_t status)
{
    switch (status) {
        case LORAPHY_SENT_DONE:
            LOG_DBG("LoRaPHY sent done\n");
            set_conf();
            break;
        case LORAPHY_INPUT_DATA:
            //GPIO_SET_PIN(BASE, MASK);
            LOG_DBG("LoRaPHY input data\n");
            parse(lorabuf_c_get_buf(), lorabuf_get_data_c_len(), 10); // 10 is the size of 'radio rx '
            //GPIO_CLR_PIN(BASE, MASK);
            process_post(&loramac_process, loramac_phy_done, NULL);
            loramac_input();
            break;
        default:
            LOG_WARN("Unknown LoRaPHY status\n");
    }
}
/*---------------------------------------------------------------------------*/
void
loramac_root_start(void)
{
    LOG_INFO("Start LoRaMAC root\n");
    state = ALONE; // initial state
    //GPIO_SET_OUTPUT(BASE, MASK);
    //GPIO_CLR_PIN(BASE, MASK);

    /* Create events for LoRaMAC */
    loramac_event_output = process_alloc_event();
    loramac_phy_done = process_alloc_event();

    /* Set initial node addr */
    lora_addr_t lora_init_node_addr = {node_id, node_id};
    LOG_INFO("Initial node address: ");
    LOG_INFO_LORA_ADDR(&lora_init_node_addr);
    loraaddr_set_node_addr(&lora_init_node_addr);

    /* Init PHY layer */
    loraphy_set_callback(&phy_callback);
    loraphy_init();
}
/*---------------------------------------------------------------------------*/
void
prepare_last_sent_frame()
{
    LOG_DBG("Preparation of a frame %s\n", is_retransmission ? "that is retransmitted":"");
    if(!is_retransmission) {
        /* SET SEQNO */
        lorabuf_set_attr(LORABUF_ATTR_MAC_SEQNO, next_seq);
        next_seq++;

        /*copy packet from buffer to last_send_frame*/
        memcpy(&last_sent_frame, lorabuf_mac_param_ptr(), sizeof(lora_frame_hdr_t) - (2 * sizeof(lora_addr_t)));
        memcpy(&last_sent_frame.src_addr, lorabuf_get_addr(LORABUF_ADDR_FIRST), 2 * sizeof(lora_addr_t));
        last_sent_datalen = lorabuf_get_data_len();
        memcpy(&last_sent_data, lorabuf_get_buf(), last_sent_datalen);
    }

    /* create str packet to lorabuf_c */
    lorabuf_c_clear();
    int size = create(lorabuf_c_get_buf());
    lorabuf_set_data_c_len(size);
    is_retransmission = false;
}
/*---------------------------------------------------------------------------*/
#define WAIT_PHY() \
{ \
    ev = PROCESS_EVENT_NONE; \
    while(ev != loramac_phy_done){ \
        PROCESS_WAIT_EVENT(); \
    } \
}
#define PHY_ACTION(...) \
{ \
    __VA_ARGS__ \
    WAIT_PHY(); \
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(loramac_process, ev, data)
{
    
    PROCESS_BEGIN();
    LOG_DBG("BEGIN LORAMAC process\n");
    PHY_ACTION(LORAPHY_SET_PARAM(LORAPHY_PARAM_WDT, LORAMAC_RETRANSMIT_TIMEOUT_c);)

    while(true){
        while(!pending){
            LOG_DBG("Waiting ...\n");
            PROCESS_WAIT_EVENT_UNTIL(ev == loramac_event_output);
        }
        //GPIO_SET_PIN(BASE, MASK);
        pending = false;
        LOG_DBG("Receive packet to send\n");
        /*------------------------------------------------------------------*/
        /*prepare the packet for transmission*/
        prepare_last_sent_frame();
        if(last_sent_frame.command != JOIN){
            set_state(WAIT_RESPONSE);
        }
        /*------------------------------------------------------------------*/
        /*send packet to PHY layer*/
        //GPIO_CLR_PIN(BASE, MASK);
        LOG_INFO("Send ");
        LOG_INFO_LORA_HDR(&last_sent_frame);
        //GPIO_SET_PIN(BASE, MASK);
        PHY_ACTION(LORAPHY_TX(lorabuf_c_get_buf(), lorabuf_get_data_c_len());)
        //GPIO_CLR_PIN(BASE, MASK);
        /*------------------------------------------------------------------*/
        /*actions depending on if a response is expected or not */
        if(last_sent_frame.confirmed || last_sent_frame.command == QUERY || last_sent_frame.command == JOIN){
            //GPIO_CLR_PIN(BASE, MASK);
            //GPIO_SET_PIN(BASE, MASK);
            LOG_INFO("Frame need a response\n");
            //GPIO_CLR_PIN(BASE, MASK);
            LOG_DBG("Set retransmit timer\n");
            PHY_ACTION(LORAPHY_SET_PARAM(LORAPHY_PARAM_WDT, LORAMAC_RETRANSMIT_TIMEOUT_c);)
            ctimer_set(&retransmit_timer, LORAMAC_RETRANSMIT_TIMEOUT, on_retransmit_timeout, NULL);
            PHY_ACTION(LORAPHY_RX();)
            //LORAPHY_RX();
        }else{
            LOG_DBG("Frame don't need a response\n");
            set_state(READY);
        }
    }
    PROCESS_END();
}
