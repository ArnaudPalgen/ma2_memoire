import logging
import sys
from py_lora_mac import NETWORK_STACK
import time
from threading import Thread
import serial

logger = logging.getLogger("MAIN")

ZOLERTIA_BAUDRATE = 115200
RN2483_BAUDRATE = 57600

PORT_0 = "/dev/ttyUSB0"
PORT_1 = "/dev/ttyUSB1"
PORT_2 = "/dev/ttyUSB2"


def serial_log(port, baudrate, serial_logger):

    con = serial.Serial(port=port, baudrate=baudrate)
    while True:
        data = con.readline()

        s = data.strip().decode(encoding="utf-8", errors="ignore")[1:]
        serial_logger.debug(s)

def exception_handler(type, value, traceback):
    s = "\n Type:"+str(type)+"\n Value:"+str(value)+"\n Traceback:"+str(traceback)
    logger.exception(s)
    sys.exit(1)

def send_data_test():
    count = 0
    logger.debug("enter send data test")
    print("YOOOOO 2")
    while True:
        time.sleep(7)
        print("YOOOOO 3")
        logger.debug("send data ! HELLO")
        NETWORK_STACK.send_to("2:24859", "hello"+str(count))
        count+=1

def main():
    logger.info("Welcome to LoRaMAC LOGGER")
    sys.excepthook = exception_handler
    NETWORK_STACK.init()

    serial_logger1 = Thread(target=serial_log, args=(PORT_1, ZOLERTIA_BAUDRATE, logging.getLogger("RPL ROOT")))
    serial_logger1.start()

    #serial_logger2 = Thread(target=serial_log, args=(PORT_2, ZOLERTIA_BAUDRATE, logging.getLogger("RPL NODE")))
    #serial_logger2.start()

    #sender = Thread(target=send_data_test)
    #sender.start()


if __name__ == "__main__":

    print(__name__)
    import logging.config

    logging.config.fileConfig("./logging.conf")

    main()
