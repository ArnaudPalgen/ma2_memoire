/*---------------------------------------------------------------------------*/
//ROOT ADDR
#define ROOT_PREFIX 1
#define ROOT_ID 0

//timeout and rx_time
#define QUERY_TIMEOUT (CLOCK_SECOND * 30) //10 sec
#define RETRANSMIT_TIMEOUT (CLOCK_SECOND * 35) //3 sec
#define RX_TIME 30000 // 2 sec

#define MAX_RETRANSMIT 3

#define BUF_SIZE 10

//logging macros
#define LOG_INFO_LR_FRAME(...)    LOG_LR_FRAME(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DBG_LR_FRAME(...)    LOG_LR_FRAME(LOG_LEVEL_DBG, __VA_ARGS__)

#define LOG_LR_FRAME(level, lora_frame) do {  \
                           if(level <= (LOG_LEVEL)) { \
                                print_lora_frame(lora_frame); \
                           } \
                         } while (0)

#define LOG_INFO_LR_ADDR(...)    LOG_LR_ADDR(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_DBG_LR_ADDR(...)    LOG_LR_ADDR(LOG_LEVEL_DBG, __VA_ARGS__)

#define LOG_LR_ADDR(level, lora_addr) do {  \
                           if(level <= (LOG_LEVEL)) { \
                                print_lora_addr(lora_addr); \
                           } \
                         } while (0)

/*---------------------------------------------------------------------------*/
typedef enum state{
    ALONE, //initial state
    JOINED, // when node has received the prefix
    READY, // when node has start RPL
    WAIT_RESPONSE
}state_t;