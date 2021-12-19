/**
 * The driver for the RN2483.
 */

#ifndef LORAPHY_H_
#define LORAPHY_H_

#include "contiki.h"
#include "loramac-conf.h"

#define LORAPHY_PARAM_VALUE_MAX_SIZE 4
#define LORAPHY_COMMMAND_VALUE_MAX_SIZE 10

#define LORAPHY_NUM_RADIO_PARAM 6

/*---------------------------------------------------------------------------*/
/* the status sended to the mac layer after a UART command is sent */
typedef enum loraphy_sent_status{
    LORAPHY_SENT_DONE,
    LORAPHY_INPUT_DATA,
}loraphy_sent_status_t;


/* the parameters of the radio */
typedef enum loraphy_param{
    LORAPHY_PARAM_BW,
    LORAPHY_PARAM_CR,
    LORAPHY_PARAM_FREQ,
    LORAPHY_PARAM_MODE,
    LORAPHY_PARAM_PWR,
    LORAPHY_PARAM_SF,
    LORAPHY_PARAM_WDT,
    LORAPHY_PARAM_NONE,
}loraphy_param_t;

/* the UART commands used */
typedef enum loraphy_command{
    LORAPHY_CMD_MAC_PAUSE,
    LORAPHY_CMD_RADIO_SET,
    LORAPHY_CMD_RADIO_RX,
    LORAPHY_CMD_RADIO_TX,
    LORAPHY_CMD_SYS_SLEEP
}loraphy_command_t;

/* possible responses to UART commands */
typedef enum loraphy_cmd_response{
    LORAPHY_CMD_RESPONSE_OK,
    LORAPHY_CMD_RESPONSE_INVALID_PARAM,
    LORAPHY_CMD_RESPONSE_RADIO_ERR,
    LORAPHY_CMD_RESPONSE_RADIO_RX,
    LORAPHY_CMD_RESPONSE_BUSY,
    LORAPHY_CMD_RESPONSE_RADIO_TX_OK,
    LORAPHY_CMD_RESPONSE_U_INT,
    LORAPHY_CMD_RESPONSE_NONE
}loraphy_cmd_response_t;
/*---------------------------------------------------------------------------*/

/**
 * \brief Initialize the LoRaPHY layer
 * 
 */
void loraphy_init(void);

/**
 * \brief Send the lorabuf_c to the radio
 * 
 */
int loraphy_send(void);

/**
 * \brief Prepare a LoRaMAC frame to be sent to the radio
 * \param command the UART command
 * \param parameter the parameter of the command
 * \param value the value of the parameter
 * \param len the length of the value
 * \param exp1 the first UART response expected
 * \param exp2 the second UART response expected
 * \return 0
 * 
 */
int loraphy_prepare_data(loraphy_command_t command, loraphy_param_t parameter, char* value, int16_t len, loraphy_cmd_response_t exp1, loraphy_cmd_response_t exp2);

/**
 * \brief Set a callback function for the upper layer
 * \param callback the callback function
 * 
 */
void loraphy_set_callback(void (* callback)( loraphy_sent_status_t status));
/*---------------------------------------------------------------------------*/
#define LORAPHY_TX(data, len){\
    loraphy_prepare_data(LORAPHY_CMD_RADIO_TX, LORAPHY_PARAM_NONE, data, len, LORAPHY_CMD_RESPONSE_RADIO_TX_OK, LORAPHY_CMD_RESPONSE_RADIO_ERR);\
    loraphy_send();\
}

#define LORAPHY_SET_PARAM(param, value){\
    loraphy_prepare_data(LORAPHY_CMD_RADIO_SET, param, value, -1,LORAPHY_CMD_RESPONSE_OK, LORAPHY_CMD_RESPONSE_NONE);\
    loraphy_send();\
}

#define LORAPHY_RX(){\
    loraphy_prepare_data(LORAPHY_CMD_RADIO_RX, LORAPHY_PARAM_NONE, "0", -1, LORAPHY_CMD_RESPONSE_RADIO_RX, LORAPHY_CMD_RESPONSE_RADIO_ERR);\
    loraphy_send();\
}

#define LORAPHY_SLEEP(duration){\
    loraphy_prepare_data(LORAPHY_CMD_SYS_SLEEP, LORAPHY_PARAM_NONE, duration, -1, LORAPHY_CMD_RESPONSE_OK, LORAPHY_CMD_RESPONSE_NONE);\
    loraphy_send();\
}

#endif /* LORAPHY_H_ */
