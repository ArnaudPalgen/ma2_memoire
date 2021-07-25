#include <stdlib.h>
#include <stdbool.h>


#define NULL_PREFIX 0
#define ROOT_PREFIX 1
#define ROOT_ID 0

typedef enum state{
    ALONE, //initial state
    JOINED, // when node has received the prefix
    READY, // when node has start RPL
    WAIT_RESPONSE
}state_t;

void send_next();

void mac_init();
//int send_to_root(lora_addr_t *frame);

//void mac_register_listener(int (* listener)(char *data));
