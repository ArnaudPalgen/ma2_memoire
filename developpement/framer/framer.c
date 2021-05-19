#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "framer.h"



const char* uart_command[7]={"mac pause", "radio set mod ", "radio set freq ", "radio set wdt ", "radio rx ", "radio tx ", "sys sleep "};
const char* uart_response[8]={"ok", "invalid_param", "radio_err", "radio_rx", "busy", "radio_tx_ok", "4294967245", "none"};

#define HEADER_SIZE 14
#define PAYLOAD_MAX_SIZE 20//todo ajuster
#define FRAME_SIZE (HEADER_SIZE+PAYLOAD_MAX_SIZE)

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

void to_frame(lora_frame_t *frame, char *dest){
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
    
    memcpy(dest, &result, size+1);
}

int main(){
    printf("hello\n");
}