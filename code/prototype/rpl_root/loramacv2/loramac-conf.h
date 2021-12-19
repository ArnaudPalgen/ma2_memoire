
#ifndef __LORAMAC_CONF_H__
#define __LORAMAC_CONF_H__

/*---------------------------------------------------------------------------*/
/*UART port*/
#ifndef LORA_RADIO_UART_PORT
#define LORA_RADIO_UART_PORT 1
#endif

#ifndef LORA_MAC_CONFIRMED
#define LORA_MAC_CONFIRMED 1
#endif

/*---------------------------------------------------------------------------*/
/*Radio configuration*/

/*
 * The operating bandwidth in KHz.
 * Values can be: 125, 250, 500.
 * Default: 125
*/
#ifndef LORA_RADIO_BW
#define LORA_RADIO_BW "125"
#endif

/*
 * The coding rate.
 * Values can be: 4/5, 4/6, 4/7, 4/8.
 * Default: 4/5
*/
#ifndef LORA_RADIO_CR
#define LORA_RADIO_CR "4/5"
#endif

/*
 * The frequency in Hz.
 * From 433050000 to 434790000 or from 863000000 to 870000000
 * Default: 868100000
 */
#ifndef LORA_RADIO_FREQ
#define LORA_RADIO_FREQ "868100000"
#endif

/*
 * The modulation method.
 * Values can be: lora, fsk.
 * Default: lora
 */
#ifndef LORA_RADIO_MODE
#define LORA_RADIO_MODE "lora"
#endif

/*
 * The transceiver output power.
 * From -3 to 15.
 * Default: 1
*/
#ifndef LORA_RADIO_PWR
#define LORA_RADIO_PWR "1"
#endif

/*
 * The spreading factor.
 * Values can be: sf7, sf8, sf9, sf10, sf11 or sf12.
 * Default: sf2
*/
#ifndef LORA_RADIO_SF
#define LORA_RADIO_SF "sf10"
#endif
/*---------------------------------------------------------------------------*/

#endif
