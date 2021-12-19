/**
 * The buffer for the LoRa stack.
 */

#ifndef LORABUF_H_
#define LORABUF_H_

#include "contiki.h"
#include "loraaddr.h"

/*---------------------------------------------------------------------------*/
// definition of size
#define LORA_HDR_BYTE_SIZE 8
#define LORA_PAYLOAD_BYTE_MAX_SIZE 247
#define LORA_FRAME_BYTE_MAX_SIZE (LORA_HDR_BYTE_SIZE + LORA_PAYLOAD_BYTE_MAX_SIZE)

#define LORA_HDR_CHAR_SIZE (2*LORA_HDR_BYTE_SIZE)
#define LORA_PAYLOAD_CHAR_MAX_SIZE (2*LORA_PAYLOAD_BYTE_MAX_SIZE)
#define LORA_FRAME_CHAR_MAX_SIZE (LORA_HDR_CHAR_SIZE + LORA_PAYLOAD_CHAR_MAX_SIZE)

#define LORA_UART_CHAR_SIZE 15

#define LORABUF_NUM_ATTRS 7
#define LORABUF_NUM_ADDRS 2
#define LORABUF_NUM_EXP_UART_RESP 2
/*---------------------------------------------------------------------------*/
typedef uint8_t lorabuf_attr_t;
/*---------------------------------------------------------------------------*/
/**
 * \brief write a char to the lorabuf
 * \param c the char to write
 * \param pos the position to write the char
 * 
 */
void lorabuf_c_write_char(char c, unsigned int pos);

/**
 * \brief copy the data to the lorabuf_c
 * \param data the data to copy
 * \param size the size of the data
 * 
 */
void lorabuf_c_copy_from(const char* data, unsigned int size);

/**
 * \brief get the lorabuf_c
 * \return the lorabuf_c
 * 
 */
char* lorabuf_c_get_buf(void);

/**
 * \brief get the length of the lorabuf_c
 * \return the length of the lorabuf_c
 * 
 */
uint16_t lorabuf_get_data_c_len(void);

/**
 * \brief set the length of the lorabuf_c
 * \param len the length of the lorabuf_c
 * 
 */
uint16_t lorabuf_set_data_c_len(uint16_t len);

/**
 * \brief clear the lorabuf
 * 
 */
void lorabuf_clear(void);

/**
 * \brief set the length of the lorabuf
 * \param len the length of the lorabuf
 * 
 */
void lorabuf_set_data_len(uint16_t len);

/**
 * \brief get the length of the lorabuf
 * \return the length of the lorabuf
 * 
 */
uint16_t lorabuf_get_data_len(void);

/**
 * \brief copy the data to the lorabuf
 * \param from the data to copy
 * \param len the size of the data
 * 
 */
int lorabuf_copy_from(const void* from, uint16_t len);

/**
 * \brief set the value of an attribute
 * \param attr the attribute to set
 * \param value the value to set
 * 
 */
void lorabuf_set_attr(uint8_t type, lorabuf_attr_t val);

/**
 * \brief get the value of an attribute
 * \param attr the attribute to get
 * \return the value of the attribute
 * 
 */
lorabuf_attr_t lorabuf_get_attr(uint8_t type);

/**
 * \brief set the value of an address
 * \param addr the address to set
 * \param value the value to set
 * 
 */
void lorabuf_set_addr(uint8_t type, const lora_addr_t *addr);

/**
 * \brief get the value of an address
 * \param addr the address to get
 * \return the value of the address
 * 
 */
lora_addr_t * lorabuf_get_addr(uint8_t type);

/**
 * \brief print the lorabuf
 * 
 */
void print_lorabuf(void);

/**
 * \brief get the lorabuf
 * \return the lorabuf
 * 
 */
uint8_t* lorabuf_get_buf(void);

/**
 * \brief get a pointer to the lorabuf MAC parameters
 * \return the pointer to the lorabuf MAC parameters
 * 
 */
uint8_t* lorabuf_mac_param_ptr(void);

/**
 * \brief clear the lorabuf_c
 * 
 */
void lorabuf_c_clear(void);
/*---------------------------------------------------------------------------*/
enum {
    /*UART attributes*/
    LORABUF_ATTR_UART_CMD,
    LORABUF_ATTR_UART_EXP_RESP1,
    LORABUF_ATTR_UART_EXP_RESP2,

    /*frame parameters*/
    LORABUF_ATTR_MAC_CONFIRMED,
    LORABUF_ATTR_MAC_SEQNO,
    LORABUF_ATTR_MAC_NEXT,
    LORABUF_ATTR_MAC_CMD,

    /*frame addresses*/
    LORABUF_ADDR_SENDER,
    LORABUF_ADDR_RECEIVER
};
/*---------------------------------------------------------------------------*/
#define LORABUF_ADDR_FIRST LORABUF_ADDR_SENDER
#define LORABUF_UART_RESP_FIRST LORABUF_ATTR_UART_EXP_RESP1
#define LORABUF_MAC_PARAMS_FIRST LORABUF_ATTR_MAC_CONFIRMED
/*---------------------------------------------------------------------------*/
#endif /* LORABUF_H_ */
