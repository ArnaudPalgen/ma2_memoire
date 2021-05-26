#include "contiki.h"

#include "dev/uart.h"
#include "sys/log.h"

#include <stdbool.h>
#include <stdlib.h>

#include "loraphy.h"


#define LOG_MODULE "LoRa PHY"
#define LOG_LEVEL LOG_LEVEL_DBG
#define BUF_SIZE 10


static int (* handler)( lora_frame_t frame) = NULL;
static uart_frame_t buffer [BUF_SIZE];
static uint8_t w_i = 0;// index to write in the buffer
static uint8_t r_i = 0;// index to read in the buffer 
static uint8_t current_size = 0;// current size of the buffer
static bool can_send = true;
static uart_response_t expected_response[UART_EXP_RESP_SIZE];

const char* uart_command[7]={"mac pause", "radio set mod ", "radio set freq ", "radio set wdt ", "radio rx ", "radio tx ", "sys sleep "};
const char* uart_response[8]={"ok", "invalid_param", "radio_err", "radio_rx", "busy", "radio_tx_ok", "4294967245", "none"};

/*---------------------------------------------------------------------------*/
/*private functions*/

void print_uart_frame(uart_frame_t *frame){
    printf("{ cmd:%s ", uart_command[frame->cmd]);
    printf("resp: [");
    uart_response_t *f_expected_response = frame->expected_response;
    for(int i=0;i<UART_EXP_RESP_SIZE-1;i++){
        printf("%s, ",uart_response[f_expected_response[i]]);
    }
    printf("%s] }",uart_response[f_expected_response[UART_EXP_RESP_SIZE-1]]);
    
}
/*build lora_frame_t from char**/
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

    /*extact flags and command*/
    char cmd[2];
    memcpy(cmd, data, 2);
    data = data+2;
    uint8_t i_cmd = (uint8_t)strtol(cmd, NULL, 16);

    uint8_t flag_filter = 0x01;
    uint8_t command_filter = 0x0F;

    bool k    = (bool)((i_cmd >> 7) & flag_filter);
    bool seq  = (bool)((i_cmd >> 6) & flag_filter);
    bool next = (bool)((i_cmd >> 5) & flag_filter);
    
    mac_command_t command = (uint8_t)( i_cmd & command_filter );
    
    result.k = k;
    result.command = command;
    result.seq = seq;
    result.next = next;

    /*extract payload*/
    result.payload = data;
    memcpy(dest, &result, sizeof(lora_frame_t));
    
    return 0;
}

/*convert lora_frame_t to hex*/
int to_frame(lora_frame_t *frame, char *dest){
    
    char result[HEADER_SIZE+PAYLOAD_MAX_SIZE]="";

    /*create src and dest addr*/
    char src_addr[6];
    char dest_addr[6];
    
    sprintf(src_addr, "%02X%04X", frame->src_addr.prefix, frame->src_addr.id);
    sprintf(dest_addr, "%02X%04X", frame->dest_addr.prefix, frame->dest_addr.id);
    
    /*create flags and MAC command*/
    char flags_command[2];

    uint8_t f_c = 0;
    if(frame->k){
        f_c = f_c | K_FLAG;
    }
    if(frame->seq){
        f_c = f_c | SEQ_FLAG;
    }
    if(frame->next){
        f_c = f_c | NEXT_FLAG;
    }
    f_c = f_c | ((uint8_t) frame->command);

    sprintf(flags_command, "%02X", f_c);   
    
    /* concat all computed values to result */
    strcat(result, src_addr);
    strcat(result, dest_addr);
    strcat(result, flags_command);
    
    /* create payload */
    int data_size = HEADER_SIZE;
    if(frame->payload != NULL){
        data_size = data_size + strlen(frame->payload);
    } 
    //int size = HEADER_SIZE+payload_size+9;//todo + 9 why ?
    if(data_size>HEADER_SIZE){
        strcat(result, frame->payload);
    }
    
    /*copy result to dest */
    LOG_DBG("%lu-created frame: %s\n", clock_seconds(),result);
    memcpy(dest, &result, data_size+1);
    return 0;
}

/**
 * Process a received UART command
 */
void process_command(unsigned char *command){
    LOG_INFO("UART response:%s\n", command);
    lora_frame_t frame;
    uart_frame_t response_frame;
    response_frame.type=RESPONSE;

    for(int i=0;i<UART_EXP_RESP_SIZE;i++){
        LOG_DBG("   compare with expected:%s\n",uart_response[expected_response[i]]);
        if(strstr((const char*)command, uart_response[expected_response[i]]) != NULL){
            if(expected_response[i] == RADIO_RX && parse(&frame, (char*)(command+10))==0){
                handler(frame);
            }
            LOG_DBG("%lu-send uart frame type RESPONSE to process\n", clock_seconds());
            process(response_frame);
            //expected_response=NULL;
            break;
        }
    }
}

/**
 * Callback function that receive bytes from UART
 **/
int uart_rx(unsigned char c){
    static unsigned char buf [FRAME_SIZE];
    static unsigned short index = 0;
    //static unsigned char *p=buf;
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
    if((int)c != 254 && (int)c != 248 && c!='\n' && (int)c != 192 && (int)c != 240 && c!='\r'){
        buf[index++] = c;
    }
    return 0;
}

/**
 * Write s to UART
 */
void write_uart(char *s){
    LOG_INFO("%lu-Write UART:%s\n",clock_seconds(), s);
    while(*s != 0){
        uart_write_byte(UART, *s++);
    }
    uart_write_byte(UART, '\r');
    uart_write_byte(UART, '\n');
}

/*---------------------------------------------------------------------------*/
/*public functions*/

void phy_init(){

    LOG_INFO("%lu-Init LoRa PHY\n", clock_seconds());

    /* UART configuration*/
    uart_init(UART);
    uart_set_input(UART, &uart_rx);

    uart_frame_t mac_pause = {MAC_PAUSE, STR, {.s=""}, {U_INT, NONE}};
    uart_frame_t set_freq = {SET_FREQ, STR, {.s="868100000"}, {OK, NONE}};
    process(mac_pause);
    process(set_freq);    
}

void phy_register_listener(int (* listener)(lora_frame_t frame)){
    handler = listener;
}

int phy_tx(lora_frame_t frame){
    uart_frame_t uart_frame = {
        TX,
        LORA,
        {.lora_frame = frame},
        {RADIO_TX_OK,RADIO_ERR}
    };
    process(uart_frame);
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
    process(uart_frame);
    return 0;
}

int phy_sleep(int duration){//todo bug rn2483 ne repond plus
    if(duration < 100 || duration > 4294967296 ){
        return 1;
    }

    uart_frame_t uart_frame = {
        SLEEP,
        INT,
        {.d = duration},
        {OK, NONE}
    };
    process(uart_frame);
    return 0;
}

int phy_rx(){
    uart_frame_t uart_frame = {
        RX,
        STR,
        {.s = "0"},
        {RADIO_ERR, RADIO_RX}
    };
    process(uart_frame);
    return 0;
}

/*---------------------------------------------------------------------------*/
/*LoRa PHY process*/
void process(uart_frame_t uart_frame){
    if(uart_frame.type != RESPONSE){
        LOG_DBG("%lu-append frame ", clock_seconds());
        print_uart_frame(&uart_frame);
        LOG_DBG(" to buffer\n");
        
        buffer[w_i] = uart_frame;
		if (w_i == BUF_SIZE-1){
    		w_i = 0;
		}else{
    		w_i ++;
		}
    	current_size ++;
    }else{
        can_send = true;
        LOG_DBG("%lu-can send -> true\n", clock_seconds());
    }
    LOG_DBG("%lu-values: can_send=%d , current_size=%d\n", clock_seconds(), can_send, current_size);
    if(can_send && current_size>0){
        uart_frame_t uart_frame = buffer[r_i];
        char result[FRAME_SIZE]="";
        
        if(uart_frame.type == STR){
            sprintf(result, "%s%s", uart_command[uart_frame.cmd], uart_frame.data.s);
        }else if(uart_frame.type == LORA){
            strcat(result, uart_command[uart_frame.cmd]);
            to_frame(&(uart_frame.data.lora_frame), result+strlen(result));
        }else if(uart_frame.type == INT){
            sprintf(result, "%s%d", uart_command[uart_frame.cmd], uart_frame.data.d);
        }
        
		if(r_i == BUF_SIZE-1){
    		r_i = 0;
		}else{
    		r_i ++;
		}
    	current_size --;
        can_send = false;
        LOG_DBG("%lu-can send -> false\n", clock_seconds());
        for(int i=0;i<UART_EXP_RESP_SIZE;i++){
            expected_response[i] = uart_frame.expected_response[i];
        }
        write_uart(result);
    }
}
/*
PROCESS_THREAD(ph_rx, ev, data){
    PROCESS_BEGIN();
        mac_init();
    PROCESS_END();
}
*/