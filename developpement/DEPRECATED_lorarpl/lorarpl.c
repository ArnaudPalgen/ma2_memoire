#include "contiki.h"
#include "sys/log.h"

#include "dev/button-hal.h"



#include "loramac.h"

#define LOG_MODULE "LoRa RPL"
#define LOG_LEVEL LOG_LEVEL_DBG

PROCESS(lorarpl_process, "LoRa-RPL process");
AUTOSTART_PROCESSES(&lorarpl_process);

PROCESS_THREAD(lorarpl_process, ev, data){
    PROCESS_BEGIN();

        LOG_INFO("Welcome !\n");
        PROCESS_WAIT_EVENT_UNTIL(ev == button_hal_press_event);
        LOG_INFO("Button pushed\n");
        mac_init();
    PROCESS_END();
}