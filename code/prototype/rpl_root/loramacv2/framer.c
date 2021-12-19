/*
 * Framer for LoRaMAC frames.
 * The format of a LoRaMAC frame is the following:
 *
 * |<---24---->|<----24--->|<-1->|<-1-->|<---2--->|<--4--->|<--8--->|<(2040-64=1976)>|
 * | src addr  | dest addr |  k  | next | reserved|command |  seq   |     payload    |
 *
 * Attributes:
 *     src_addr: The source address
 *     dest_addr: The Destination address
 *     command: The MAC command
 *     payload: The payload. Must be a string in hexadecimal
 *     seq: The sequence number
 *     k: true if the frame need an ACK, false otherwise
 *     has_next: true if another frame follows it, false otherwise. Only used for downward traffic
 */
#include <stdlib.h>
#include "lorabuf.h"
#include "framer.h"
#include "loraaddr.h"
#include "loramac.h"
#include "sys/log.h"
/*---------------------------------------------------------------------------*/
/*logging configuration*/
#define LOG_MODULE "LoRa FRAMER"
#define LOG_LEVEL LOG_LEVEL_NONE
/*---------------------------------------------------------------------------*/
int
parse(char *data, int len, int offset)
{
    /*skip the first characters like 'radio_rx'*/
    int increment = offset;
    len -= offset;

    LOG_DBG("ENTER parse\n");
    LOG_DBG("   > data:{%s}\n", data+increment);
    LOG_DBG("   > len: %d\n", len);

    char prefix_c[2];
    uint8_t prefix;
    
    char id_c[4];
    uint16_t id;

    /*extract src addr*/
    memcpy(prefix_c, data+increment, 2);
    increment += 2;
    memcpy(id_c, data+increment, 4);
    increment += 4;

    prefix = (uint8_t) strtol(prefix_c, NULL, 16);
    id = (uint16_t) strtol(id_c, NULL, 16);

    lora_addr_t addr = {prefix, id};

    lorabuf_set_addr(LORABUF_ADDR_SENDER, &addr);

    /*extract dest addr*/
    memcpy(prefix_c, data+increment, 2);
    increment = increment+2;
    memcpy(id_c, data+increment, 4);
    increment +=4;

    prefix = (uint8_t) strtol(prefix_c, NULL, 16);
    id = (uint16_t) strtol(id_c, NULL, 16);

    lora_addr_t addr2 = {prefix, id};
    lorabuf_set_addr(LORABUF_ADDR_RECEIVER, &addr2);

    /*extact flags and MAC command*/
    char cmd[2];
    memcpy(cmd, data+increment, 2);
    increment += 2;
    uint8_t i_cmd = (uint8_t)strtol(cmd, NULL, 16);

    uint8_t flag_filter = 0x01;
    uint8_t command_filter = 0x0F;

    bool k    = (bool)((i_cmd >> 7) & flag_filter);
    bool next = (bool)((i_cmd >> 6) & flag_filter);
    
    loramac_command_t command = (uint8_t)( i_cmd & command_filter );
    lorabuf_set_attr(LORABUF_ATTR_MAC_CMD, command);
    lorabuf_set_attr(LORABUF_ATTR_MAC_NEXT, next);
    lorabuf_set_attr(LORABUF_ATTR_MAC_CONFIRMED, k);

    /*extract SN*/
    char sn_c[2];
    memcpy(sn_c, data+increment, 2);
    increment += 2;
    uint8_t sn = (uint8_t)strtol(sn_c, NULL, 16);
    lorabuf_set_attr(LORABUF_ATTR_MAC_SEQNO, sn);

    /*extract payload*/
    int payload_size = 0;
    char current_byte_c[2];
    uint8_t current_byte = 0;

    while((increment-10)<len){
        memcpy(current_byte_c, data+increment, 2);
        current_byte = (uint8_t)strtol(current_byte_c, NULL, 16);
        memcpy(lorabuf_get_buf()+payload_size, &current_byte, 1);
        payload_size+=1;
        increment = increment+2;

    }
    lorabuf_set_data_len(payload_size);
    return 0;
}
/*---------------------------------------------------------------------------*/
int
create(char* destination)
{
    char* dest = destination;
    int size = 0;
    LOG_DBG("ENTER create\n");

    char addr_c[6];
    lora_addr_t *addr_p;
    
    /*create src addr*/
    addr_p = lorabuf_get_addr(LORABUF_ADDR_SENDER);
    sprintf(addr_c, "%02X%04X", addr_p->prefix, addr_p->id);
    memcpy(dest,addr_c,6);
    dest = dest+6;
    size = size+6;

    /*create dest addr*/
    addr_p = lorabuf_get_addr(LORABUF_ADDR_RECEIVER);
    sprintf(addr_c, "%02X%04X", addr_p->prefix, addr_p->id);
    memcpy(dest,addr_c,6);
    dest = dest+6;
    size = size+6;

    /*create flags and MAC command*/
    char flags_command[2];
    uint16_t k_flag =  0x80;
    uint16_t next_flag =  0x40;
    
    uint8_t f_c = 0;
    lorabuf_attr_t k = lorabuf_get_attr(LORABUF_ATTR_MAC_CONFIRMED);
    lorabuf_attr_t next = lorabuf_get_attr(LORABUF_ATTR_MAC_NEXT);
    lorabuf_attr_t command = lorabuf_get_attr(LORABUF_ATTR_MAC_CMD);

    if(k){
        f_c = f_c | k_flag;
    }
    if(next){
        f_c = f_c | next_flag;
    }
    f_c = f_c | ((uint8_t) command);

    sprintf(flags_command, "%02X", f_c);
    memcpy(dest, flags_command, 2);
    dest = dest+2;
    size = size+2;

    /* create SN */
    char sn[2];
    lorabuf_attr_t seq = lorabuf_get_attr(LORABUF_ATTR_MAC_SEQNO);
    sprintf(sn, "%02X", seq);
    memcpy(dest, sn, 2);
    dest = dest+2;
    size = size+2;

    /* create payload */
    uint16_t datalen = lorabuf_get_data_len();
    if (datalen> 0){
        char char_byte[2];
        uint8_t* lorabuf = lorabuf_get_buf();
        for(int i=0;i<datalen;i++){
            sprintf(char_byte,"%02X", lorabuf[i]);
            memcpy(dest, char_byte,2);
            dest = dest+2;
            size = size+2;
        }
    }
    LOG_DBG("created frame:{%s}\n", destination);
    return size;
}
