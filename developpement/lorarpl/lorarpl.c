#include "contiki.h"
#include "sys/log.h"

#include "loramac.h"

#define LOG_MODULE "LoRa RPL"
#define LOG_LEVEL LOG_LEVEL_DBG

PROCESS(lorarpl_process, "LoRa-RPL process");
AUTOSTART_PROCESSES(&lorarpl_process);

PROCESS_THREAD(lorarpl_process, ev, data){
    PROCESS_BEGIN();
        mac_init();
    PROCESS_END();
}