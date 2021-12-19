#include "lorabuf.h"
#include "loraaddr.h"
#include "sys/cc.h"
#include "sys/log.h"
/*---------------------------------------------------------------------------*/
/*logging configuration*/
#define LOG_MODULE "LoRa BUF"
#define LOG_LEVEL LOG_LEVEL_NONE
/*---------------------------------------------------------------------------*/
lorabuf_attr_t lorabuf_attrs[LORABUF_NUM_ATTRS];
lora_addr_t lorabuf_addrs[LORABUF_NUM_ADDRS];

static uint8_t lorabuf[LORA_PAYLOAD_BYTE_MAX_SIZE];
static char lorabuf_c[LORA_FRAME_CHAR_MAX_SIZE+LORA_UART_CHAR_SIZE];
static char* lorabuf_field[LORABUF_NUM_ATTRS]={"LORABUF_ATTR_UART_CMD", "LORABUF_ATTR_UART_EXP_RESP1", "LORABUF_ATTR_UART_EXP_RESP2",
                                        "LORABUF_ATTR_MAC_CONFIRMED", "LORABUF_ATTR_MAC_SEQNO", "LORABUF_ATTR_MAC_NEXT",
                                        "LORABUF_ATTR_MAC_CMD"};

static uint16_t datalen;
static uint16_t datalen_c;
/*---------------------------------------------------------------------------*/
void
lorabuf_c_write_char(char c, unsigned int pos)
{
    lorabuf_c[pos] = c;
}
/*---------------------------------------------------------------------------*/
void
lorabuf_c_copy_from(const char* data, unsigned int size)
{
    memcpy(lorabuf_c, data, size);
    datalen_c = size;
}
/*---------------------------------------------------------------------------*/
char*
lorabuf_c_get_buf(void)
{
    return (char *) &lorabuf_c;
}
/*---------------------------------------------------------------------------*/
uint16_t
lorabuf_get_data_c_len(void)
{
    return datalen_c;
}
/*---------------------------------------------------------------------------*/
uint16_t
lorabuf_set_data_c_len(uint16_t len)
{
    datalen_c = len;
    return len;
}
/*---------------------------------------------------------------------------*/
void
lorabuf_clear(void)
{
    datalen = 0;
    memset(lorabuf_attrs, 0, sizeof(lorabuf_attrs));
}

void
lorabuf_c_clear(void)
{
    datalen_c = 0;
}
/*---------------------------------------------------------------------------*/
void
lorabuf_set_data_len(uint16_t len)
{
    datalen = len;
}
/*---------------------------------------------------------------------------*/
uint16_t
lorabuf_get_data_len(void)
{
    return datalen;
}
/*---------------------------------------------------------------------------*/
int
lorabuf_copy_from(const void* from, uint16_t len)
{

    uint16_t l;
    l = MIN(LORA_PAYLOAD_BYTE_MAX_SIZE, len);
    memcpy(lorabuf, from, l);
    datalen = l;
    return l;
}
/*---------------------------------------------------------------------------*/
void
lorabuf_set_attr(uint8_t type, lorabuf_attr_t val)
{
    lorabuf_attrs[type] = val;
}
/*---------------------------------------------------------------------------*/
lorabuf_attr_t
lorabuf_get_attr(uint8_t type)
{
    lorabuf_attr_t val = lorabuf_attrs[type];
    return val;
}
/*---------------------------------------------------------------------------*/
void
lorabuf_set_addr(uint8_t type, const lora_addr_t *addr)
{
    loraaddr_copy(&lorabuf_addrs[type - LORABUF_ADDR_FIRST], addr);
}
/*---------------------------------------------------------------------------*/
lora_addr_t *
lorabuf_get_addr(uint8_t type)
{
    return &lorabuf_addrs[type - LORABUF_ADDR_FIRST];
}
/*---------------------------------------------------------------------------*/
void
print_lorabuf(void)
{
    printf("\n|___________________________________________|\n");
    printf("|               lorabuf_attrs               |\n");
    printf("|-------------------------------------------|\n");
    printf("| %-30s | %-8s |\n","ATTRIBUTE", "VALUE");
    printf("|--------------------------------|----------|\n");
    for(int i=0;i<LORABUF_NUM_ATTRS;i++){
        printf("| %-30s | %-8d |\n",lorabuf_field[i], lorabuf_attrs[i]);
    }
    printf("| %-30s | %-8d |\n","DATA LEN", datalen);
    printf("|-------------- lorabuf_addrs --------------|\n");
    printf("| %-30s | ", "LORABUF_ADDR_SENDER");
    loraaddr_print(lorabuf_get_addr(LORABUF_ADDR_SENDER));
    printf(" |\n");
    printf("| %-30s | ", "LORABUF_ADDR_RECEIVER");
    loraaddr_print(lorabuf_get_addr(LORABUF_ADDR_RECEIVER));
    printf(" |\n");
    printf("|___________________________________________|\n\n");


}
/*---------------------------------------------------------------------------*/
uint8_t*
lorabuf_get_buf(void)
{
    return (uint8_t *) &lorabuf;
}
/*---------------------------------------------------------------------------*/
uint8_t*
lorabuf_mac_param_ptr(void)
{
    return &lorabuf_attrs[LORABUF_MAC_PARAMS_FIRST];
}
/*---------------------------------------------------------------------------*/
