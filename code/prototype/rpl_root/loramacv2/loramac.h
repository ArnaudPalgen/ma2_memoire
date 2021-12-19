/**
 * The LoRaMAC implementation.
 */

#ifndef LORAMAC_H_
#define LORAMAC_H_

#include "contiki.h"
#include "loraaddr.h"
#include "sys/rtimer.h"
/*---------------------------------------------------------------------------*/
// macros definitions
#define LORAMAC_QUERY_TIMEOUT (30*CLOCK_SECOND)
#define LORAMAC_MAX_RETRANSMIT 3
#define LORAMAC_RETRANSMIT_TIMEOUT (CLOCK_SECOND*12)
#define LORAMAC_RETRANSMIT_TIMEOUT_c "12000"//ms
#define LORAMAC_JOIN_SLEEP_TIME_c "60000" // in ms. 5000 ms -> 5s
#define LORAMAC_JOIN_SLEEP_TIME (CLOCK_SECOND*60)
#define LORAMAC_MAX_JOIN_SLEEP_TIME (CLOCK_SECOND*180)
#define LORAMAC_DISABLE_WDT "0"
/*---------------------------------------------------------------------------*/
/*The supported LoRaMAC commands*/
typedef enum loramac_command {
    JOIN,
    JOIN_RESPONSE,
    DATA,
    ACK,
    QUERY,
}loramac_command_t;

/*The different MAC states*/
typedef enum loramac_state{
    ALONE, // initial state
    READY, // when node has start RPL
    WAIT_RESPONSE // when the node wait a response
}loramac_state_t;

typedef struct lora_frame_hdr{
    bool confirmed; // true if the frame need an ack in return, false otherwise
    uint8_t seqno; // The sequence number of the frame
    bool next; // true if another frame follow this frame. Only for downward traffic
    loramac_command_t command; // The MAC command of the frame
    lora_addr_t src_addr; // The source Address
    lora_addr_t dest_addr; // The destination Address
}lora_frame_hdr_t;
/*---------------------------------------------------------------------------*/
/**
 * \brief Initialize the LoRaMAC layer
 * 
 */
void loramac_root_start(void);

/**
 * \brief function called for an incoming LoRaMAC frame
 * 
 */
void loramac_input(void);

/**
 * \brief Function to use to send a LoRaMAC frame from the lorabuf
 * 
 */
int loramac_send(void);

/**
 * \brief print the a LoRa header
 * \param hdr the header to print
 * 
 */
void loramac_print_hdr(lora_frame_hdr_t *hdr);

/*---------------------------------------------------------------------------*/
#define LOG_LORA_HDR(level, lora_hdr) do {  \
    if((level) <= (LOG_LEVEL)) { \
        loramac_print_hdr(lora_hdr); \
        printf("\n"); \
    } \
    } while (0)

#define LOG_INFO_LORA_HDR(...)    LOG_LORA_HDR(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DBG_LORA_HDR(...)    LOG_LORA_HDR(LOG_LEVEL_DBG, __VA_ARGS__)


#endif /* LORAMAC_H_ */
