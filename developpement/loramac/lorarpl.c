#include "contiki.h"

#include "loramac.h"

PROCESS_THREAD(lorarpl_process, ev, data){
    PROCESS_BEGIN();
        mac_init();
    PROCESS_END();
}