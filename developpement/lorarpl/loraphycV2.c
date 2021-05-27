#include "contiki.h"

#include "dev/uart.h"
#include "sys/log.h"

#include <stdbool.h>
#include <stdlib.h>

#include "loraphy.h"
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "LoRa PHY"
#define LOG_LEVEL LOG_LEVEL_DBG

static int (* handler)( lora_frame_t frame) = NULL;

//static bool can_send = true;
static uart_response_t expected_response[UART_EXP_RESP_SIZE];

const char* uart_command[7]={"mac pause", "radio set mod ", "radio set freq ", "radio set wdt ", "radio rx ", "radio tx ", "sys sleep "};
const char* uart_response[8]={"ok", "invalid_param", "radio_err", "radio_rx", "busy", "radio_tx_ok", "4294967245", "none"};

/*---------------------------------------------------------------------------*/
/*correct functions*/

void print_uart_frame(uart_frame_t *frame){
    printf("{ cmd:%s ", uart_command[frame->cmd]);
    printf("resp: [");
    uart_response_t *f_expected_response = frame->expected_response;
    for(int i=0;i<UART_EXP_RESP_SIZE-1;i++){
        printf("%s, ",uart_response[f_expected_response[i]]);
    }
    printf("%s] }",uart_response[f_expected_response[UART_EXP_RESP_SIZE-1]]);
    
}

int parse(lora_frame_t *dest, char *data){

    if(strlen(data) < HEADER_SIZE){
        return 1;
    }
    
    lora_frame_t result;
    
    char prefix_c[2];
    char id_c[4];
    
    uint8_t prefix;
    uint16_t id;
    lora_addr_t src_addr;
    lora_addr_t dest_addr;

    /*extract src addr*/
    memcpy(prefix_c, data, 2);
    data = data+2;
    memcpy(id_c, data, 4);
    data = data+4;

    prefix = (uint8_t) strtol(prefix_c, NULL, 16);
    id = (uint16_t) strtol(id_c, NULL, 16);

    src_addr.prefix = prefix;
    src_addr.id=id; 
    result.src_addr = src_addr;

    /*extract dest addr*/
    memcpy(prefix_c, data, 2);
    data = data+2;
    memcpy(id_c, data, 4);
    data = data+4;

    prefix = (uint8_t) strtol(prefix_c, NULL, 16);
    id = (uint16_t) strtol(id_c, NULL, 16);

    dest_addr.prefix = prefix;
    dest_addr.id = id;
    result.dest_addr = dest_addr;

    /*extact ack flag anc command*/
    char cmd[2];
    memcpy(cmd, data, 2);
    data = data+2;

    bool ack = (bool)(((int)strtol(cmd, NULL, 16)) >> 7);
    uint8_t filter = 0x0F;
    mac_command_t command = (uint8_t)( ((uint8_t)strtol(cmd, NULL, 16)) & filter );
    result.k = ack;
    result.command = command;

    /*extract payload*/
    result.payload = data;
    memcpy(dest, &result, sizeof(lora_frame_t));
    
    return 0;
}

int to_frame(lora_frame_t *frame, char *dest){
    int payload_size=0;
    if(frame->payload != NULL){
        payload_size = strlen(frame->payload);
    }
    int size = HEADER_SIZE+payload_size+9;
    char result[HEADER_SIZE+PAYLOAD_MAX_SIZE]="";
    
    char src_addr[6];
    char dest_addr[6];
    char flags_command[2];
    
    sprintf(src_addr, "%02X%04X", frame->src_addr.prefix, frame->src_addr.id);
    sprintf(dest_addr, "%02X%04X", frame->dest_addr.prefix, frame->dest_addr.id);
    uint8_t k;
    if(frame->k){
        k = 0x80;
    }else{
        k = 0x00;
    }
    uint8_t f_c = ((uint8_t) frame->command) | k;
    sprintf(flags_command, "%02X", f_c);

    strcat(result, src_addr);
    strcat(result, dest_addr);
    strcat(result, flags_command);
    if(payload_size>0){
        strcat(result, frame->payload);
    }
    
    LOG_DBG("[%lu]created frame: %s\n", clock_seconds(),result);
    memcpy(dest, &result, size+1);
    return 0;
}

void write_uart(char *s){
    LOG_INFO("[%lu]Write UART:%s\n",clock_seconds(), s);
    while(*s != 0){
        uart_write_byte(UART, *s++);
    }
    uart_write_byte(UART, '\r');
    uart_write_byte(UART, '\n');
}

void phy_register_listener(int (* listener)(lora_frame_t frame)){
    handler = listener;
}


/*---------------------------------------------------------------------------*/
mutex_t response_mutex;
mutex_t tx_buf_mutex;
static process_event_t new_tx_frame_event;
static process_event_t can_send_event;
static bool can_send = true;

#define TX_BUF_SIZE 10
static uart_frame_t tx_buffer [TX_BUF_SIZE];
static uint8_t w_i = 0;// index to write in the buffer
static uint8_t r_i = 0;// index to read in the buffer 
static uint8_t tx_buf_size = 0;// current size of the buffer

void process_command(char *command){
    LOG_INFO("UART response:%s\n", command);

    lora_frame_t frame;

    while(!mutex_try_lock(&response_mutex)){}
    for(int i=0;i<UART_EXP_RESP_SIZE;i++){
        LOG_DBG("compare response with:%s\n",uart_response[expected_response[i]]);
        if(strstr((const char*)command, uart_response[expected_response[i]]) != NULL){
            /*the UART response is the exoetced response*/
            if(expected_response[i] == RADIO_RX && parse(&frame, (char*)(command+10))==0){
                /*receive data -> transmit to MAC layer*/
                handler(frame);
            }
            /* signal to the tx process that the next frame can be sent*/
            process_post(&ph_tx, can_send_event, NULL);
            break;
        }
    }
    mutex_unlock(&response_mutex);

}

/**
 * Callback function that receive bytes from UART
 **/
int uart_rx(unsigned char c){
    static unsigned char buf [FRAME_SIZE];
    static unsigned short index = 0;
    static bool cr = false;

    if(c == '\r'){
      cr = true;
    }else if(c == '\n'){
      if(cr==true){
        process_command(buf);
        index = 0;
        cr = false;
        memset(buf, 0, FRAME_SIZE*sizeof(char));
      }
    }
    if((int)c != 254 && (int)c != 248 && c!='\n' && (int)c != 192 && (int)c != 240){
        buf[index++] = c;
    }
    return 0;
}

int uart_tx(uart_frame_t uart_frame){
    while(!mutex_try_lock(&tx_buf_mutex)){}
    if(tx_buf_size < TX_BUF_SIZE){
        tx_buffer[w_i] = uart_frame;
        tx_buf_size ++;
        w_i = (w_i+1)%TX_BUF_SIZE;
    }
    mutex_unlock(&tx_buf_mutex);
}

void phy_init(){
    //add config command to tx_buf
    //send event to process
    LOG_INFO("[%lu]Init LoRa PHY\n", clock_seconds());

    /* create events*/    
    new_tx_frame_event = process_alloc_event();
    can_send_event = process_alloc_event();
    
    /* start process*/
    process_start(&ph_rx);
    process_start(&ph_tx);

    /*send initialisation UART commands*/
    uart_frame_t mac_pause = {MAC_PAUSE, STR, {.s=""}, {U_INT, NONE}};
    uart_frame_t set_freq = {SET_FREQ, STR, {.s="868100000"}, {OK, NONE}};

    uart_tx(mac_pause);
    uart_tx(set_freq);

    process_post(&ph_tx, new_tx_frame_event, NULL);

}

int phy_tx(lora_frame_t frame){
    uart_frame_t uart_frame = {
        TX,
        LORA,
        {.lora_frame = frame},
        {RADIO_TX_OK,RADIO_ERR}
    };
    uart_tx(uart_frame);
    return 0;

}

int phy_timeout(int timeout){
    if(timeout < 0 || timeout > 4294967295 ){
        return 1;
    }

    uart_frame_t uart_frame = {
        SET_WDT,
        INT,
        {.d = timeout},
        {OK, NONE}
    };
    uart_tx(uart_frame);
    return 0;
}

int phy_sleep(int duration){
    if(duration < 100 || duration > 4294967296 ){
        return 1;
    }

    uart_frame_t uart_frame = {
        SLEEP,
        INT,
        {.d = duration},
        {OK, NONE}
    };
    uart_tx(uart_frame);
    return 0;
}

int phy_rx(){
    uart_frame_t uart_frame = {
        RX,
        STR,
        {.s = "0"},
        {RADIO_ERR, RADIO_RX}
    };
    uart_tx(uart_frame);
    return 0;
}

/*---------------------------------------------------------------------------*/
/* process */
PROCESS_THREAD(ph_rx, ev, data){
    PROCESS_BEGIN();
    
    /* UART configuration*/
    uart_init(UART);
    uart_set_input(UART, &uart_rx);
    
    PROCESS_END();
}

PROCESS_THREAD(ph_tx, ev, data){
    
    uart_frame_t uart_frame;
    
    PROCESS_BEGIN();

    /*main process*/

    PROCESS_WAIT_EVENT_UNTIL(ev==new_tx_frame_event);
    if can_send{
        while(!mutex_try_lock(&tx_buf_mutex)){}
        if(tx_buf_size >0){
            /* get next uart frame to send*/
            uart_frame = tx_buffer[r_i];
            tx_buf_size --;
            r_i = (r_i+1)%TX_BUF_SIZE;
            mutex_unlock(&tx_buf_mutex);

            /* update expected response */
            while(!mutex_try_lock(&response_mutex)){}
            for(int i=0;i<UART_EXP_RESP_SIZE;i++){
                expected_response[i] = uart_frame.expected_response[i];
            }
            mutex_unlock(&response_mutex);

        }else{
            mutex_unlock(&tx_buf_mutex);
        }
        can_send = false;

        char result[FRAME_SIZE]="";
        if(uart_frame.type == STR){
            sprintf(result, "%s%s", uart_command[uart_frame.cmd], uart_frame.data.s);
        }else if(uart_frame.type == LORA){
            strcat(result, uart_command[uart_frame.cmd]);
            to_frame(&(uart_frame.data.lora_frame), result+strlen(result));
        }else if(uart_frame.type == INT){
            sprintf(result, "%s%d", uart_command[uart_frame.cmd], uart_frame.data.d);
        }

        write_uart(result);
    }

    PROCESS_WAIT_EVENT_UNTIL(ev==can_send_event);
    can_send = true;
    //process_post(&ph_tx, new_tx_frame_event, NULL);

    PROCESS_END();
}