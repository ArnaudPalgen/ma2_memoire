import logging
import sys
from py_lora_mac import network_stack
import time
from threading import Thread

logger = logging.getLogger("RPL_ROOT")

ZOLERTIA_BAUDRATE = 115200
RN2483_BAUDRATE = 57600

PORT_0 = "/dev/ttyUSB0"
PORT_1 = "/dev/ttyUSB1"


def serial_log(port, baudrate):
    import serial

    con = serial.Serial(port=port, baudrate=baudrate)
    while True:
        data = con.readline()

        s = data.strip().decode(encoding="utf-8", errors="ignore")[1:]
        logger.debug(s)

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
        network_stack.send_to("2:24859", "hello"+str(count))
        count+=1

def main():
    print("YOOOOO 1")
    sys.excepthook = exception_handler
    network_stack.init()
    #serial_log(PORT_1, ZOLERTIA_BAUDRATE)
    serial_logger = Thread(target=serial_log, args=(PORT_1, ZOLERTIA_BAUDRATE))
    serial_logger.start()
    sender = Thread(target=send_data_test)
    sender.start()


if __name__ == "__main__":

    print(__name__)
    import logging.config

    logging.config.fileConfig("./logging.conf")

    main()
