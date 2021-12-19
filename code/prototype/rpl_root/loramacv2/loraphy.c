#include "loraphy.h"
#include "lorabuf.h"
#include "dev/uart.h"
#include "sys/log.h"
#include "sys/rtimer.h"
#include "framer.h"
#include "loramac.h"
#include "sys/log.h"
#include <stdlib.h>
/*---------------------------------------------------------------------------*/
/* Log configuration */
#define LOG_MODULE "LoRa PHY"
#define LOG_LEVEL LOG_LEVEL_NONE
/*---------------------------------------------------------------------------*/
const char* loraphy_params_values[8]={"bw ", "cr ", "freq ", "mod ", "pwr ", "sf ", "wdt ", ""};
const char* loraphy_commands_values[5]={"mac pause", "radio set ", "radio rx ", "radio tx ", "sys sleep "};
const char* uart_response[8]={"ok", "invalid_param", "radio_err", "radio_rx", "busy", "radio_tx_ok", "4294967245", "none"};
static bool ready = true;
static void (* c)( loraphy_sent_status_t status) = NULL;
/*---------------------------------------------------------------------------*/
void
loraphy_input()
{
    LOG_INFO("Receive UART data {%s} (len=%d)\n", lorabuf_c_get_buf(), lorabuf_get_data_c_len());

    int i = LORABUF_UART_RESP_FIRST;
    loraphy_cmd_response_t uart_resp=LORAPHY_CMD_RESPONSE_NONE;

    while(i<LORABUF_UART_RESP_FIRST+LORABUF_NUM_EXP_UART_RESP && !ready){
        uart_resp = (loraphy_cmd_response_t)lorabuf_get_attr(i);
        LOG_DBG("Compare {%s} WITH {%s}\n", lorabuf_c_get_buf(), uart_response[uart_resp]);
        if(strstr((const char*)lorabuf_c_get_buf(), uart_response[uart_resp])){
            ready = true;
        }else{
            LOG_DBG("Not this response\n");
        }
        i++;
    }
    if(strstr((const char*)lorabuf_c_get_buf(), uart_response[2])){
        ready = true;
        LOG_INFO("Unexpected radio_err. Radio is ready.\n");
    }
    if(i>=LORABUF_UART_RESP_FIRST+LORABUF_NUM_EXP_UART_RESP && ready==false){
        LOG_INFO("Response is not the expected.\n");
        for(int j=LORABUF_UART_RESP_FIRST; j<LORABUF_UART_RESP_FIRST+LORABUF_NUM_EXP_UART_RESP; j++){
            LOG_DBG("Expected %d: %s\n", j-LORABUF_UART_RESP_FIRST, uart_response[(loraphy_cmd_response_t)lorabuf_get_attr(j)]);
        }
        LOG_DBG("----\n");
    }else if(c != NULL){
        LOG_INFO("Expected response\n");
        c((uart_resp==LORAPHY_CMD_RESPONSE_RADIO_RX) ? LORAPHY_INPUT_DATA:LORAPHY_SENT_DONE);
    }
}
/*---------------------------------------------------------------------------*/
int
uart_rx(unsigned char c)
{
    static bool cr = false;
    static unsigned int index = 0;
    static bool start = true;

    if(start){
        start=false;
        lorabuf_c_clear();
    }
    if(c == '\r'){
        cr = true;
    }else if(c == '\n' && cr){
        lorabuf_set_data_c_len(index);
        loraphy_input();
        cr = false;
        start = true;
        index = 0;
    }else if((int)c != 254 && (int)c != 248 && c!='\n' && (int)c != 192 && (int)c != 240 && c!='\r'){
        //review simplify test with ascii value
        lorabuf_c_write_char(c, index);
        index ++;
    }
    return 0;
}
/*---------------------------------------------------------------------------*/
/*write a char* to the UART connection*/
void
write_uart(char *s, int len)
{
    LOG_INFO("Write to UART {%s} len=%d\n", s, len);

    for(int i=0;i<len;i++){
        uart_write_byte(LORA_RADIO_UART_PORT, *s++);
    }

    uart_write_byte(LORA_RADIO_UART_PORT, '\r');
    uart_write_byte(LORA_RADIO_UART_PORT, '\n');
}
/*---------------------------------------------------------------------------*/
void
loraphy_init(void)
{
    LOG_INFO("Init LoRaPHY\n");

    /* UART configuration */
    uart_init(LORA_RADIO_UART_PORT);
    uart_set_input(LORA_RADIO_UART_PORT, &uart_rx);

    /*SEND mac pause*/
    loraphy_prepare_data(LORAPHY_CMD_MAC_PAUSE, LORAPHY_PARAM_NONE, "", -1,LORAPHY_CMD_RESPONSE_U_INT, LORAPHY_CMD_RESPONSE_NONE);
    loraphy_send();
    RTIMER_BUSYWAIT_UNTIL(ready, RTIMER_SECOND/4);
}
/*---------------------------------------------------------------------------*/
void
loraphy_set_callback(void (* callback)( loraphy_sent_status_t status))
{
    c = callback;
}
/*---------------------------------------------------------------------------*/
int
loraphy_send(void)
{
    LOG_DBG("Call to PHY send for {%s} (len=%d)\n", lorabuf_c_get_buf(), lorabuf_get_data_c_len());

    if(!ready){
        LOG_WARN("Unable to send. A response is expected\n");
        return 1;
    }
    char* buf = lorabuf_c_get_buf();
    ready = false;
    write_uart(buf, lorabuf_get_data_c_len());
    return 0;
}
/*---------------------------------------------------------------------------*/
int
loraphy_prepare_data(loraphy_command_t command, loraphy_param_t parameter, char* value, int16_t len,loraphy_cmd_response_t exp1, loraphy_cmd_response_t exp2)
{
    LOG_DBG("Prepare data\n");
    LOG_DBG("   > phy cmd:{%s}\n", loraphy_commands_values[command]);
    LOG_DBG("   > phy param:{%s}\n", loraphy_params_values[parameter]);
    LOG_DBG("   > value:{%s}\n", value);
    LOG_DBG("   > value len:{%d}\n", len);
    LOG_DBG("   > phy resp1:{%s}\n", uart_response[exp1]);
    LOG_DBG("   > phy resp2:{%s}\n", uart_response[exp2]);

    const char* cmd = loraphy_commands_values[command];
    uint8_t cmd_len = strlen(cmd);

    const char* param = loraphy_params_values[parameter];
    uint8_t param_len = strlen(param);

    unsigned int value_len = ((len > -1) ? len:strlen(value));
    unsigned int total_len = cmd_len+param_len+value_len;
    char data[total_len];
    memcpy(&data, cmd, cmd_len);
    memcpy(&(data[cmd_len]), param, param_len);
    memcpy(&(data[cmd_len+param_len]), value, value_len);

    lorabuf_set_attr(LORABUF_ATTR_UART_EXP_RESP1, exp1);
    lorabuf_set_attr(LORABUF_ATTR_UART_EXP_RESP2, exp2);
    lorabuf_c_clear();
    lorabuf_c_copy_from(data, total_len);
    lorabuf_set_data_c_len(total_len);
    return 0;
}
