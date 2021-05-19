#include <stdlib.h>


#define UART_EXP_RESP_SIZE 2


typedef enum uart_response{
    OK,
    INVALID_PARAM,
    RADIO_ERR,
    RADIO_RX,
    BUSY,
    RADIO_TX_OK,
    U_INT,
    NONE
}uart_response_t;

typedef enum uart_command{
    MAC_PAUSE,// pause mac layer
    SET_MOD,//set radio mode (fsk or lora)
    SET_FREQ,//set radio freq from 433050000 to 434790000 or from 863000000 to 870000000, in Hz.
    SET_WDT,//set watchdog timer
    RX,//receive mode
    TX,//transmit data
    SLEEP//system sleep
}uart_command_t;

typedef enum uart_frame_type{
    STR,
    LORA,
    INT,
    RESPONSE,
}uart_type;

typedef enum mac_command {
    JOIN,
    JOIN_RESPONSE,
    DATA,
    ACK,
    PING,
    PONG,
    QUERY,
    CHILD,
    CHILD_RESPONSE 
}mac_command_t;

typedef struct lora_addr{
    uint8_t prefix;
    uint16_t id;
}lora_addr_t;

typedef struct lora_frame{
    lora_addr_t src_addr;
    lora_addr_t dest_addr;
    bool k;
    mac_command_t command;
    char* payload;

}lora_frame_t;

typedef struct uart_frame{
    uart_command_t cmd;
    uart_type type;
    union uart_data {
        char *s;
        int d;
        lora_frame_t lora_frame;
    } data;
    uart_response_t expected_response[UART_EXP_RESP_SIZE];
}uart_frame_t;


void print_uart_frame(uart_frame_t *frame);

int parse(lora_frame_t *dest, char *data);

//void to_frame(lora_frame_t *frame, char *dest){
