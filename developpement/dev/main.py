import logging
import sys
import time
from threading import Thread
import serial
import sys
from ipaddress import IPv6Address, AddressValueError
from scapy.all import *
from scapy.layers.inet6 import IPv6

sys.path.insert(1, '/home/arnaud/Documents/cours/universite/cours2020-2021/memoire/ma2_memoire/developpement')

from py_lora_mac import NETWORK_STACK


logger = logging.getLogger("MAIN")

ZOLERTIA_BAUDRATE = 115200
RN2483_BAUDRATE = 57600

PORT_0 = "/dev/ttyUSB0"
PORT_1 = "/dev/ttyUSB1"
PORT_2 = "/dev/ttyUSB2"

UDP_CLIENT_PORT = 8765
UDP_SERVER_PORT = 5678


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
    time.sleep(10)
    count = 0
    logger.debug("enter send data test")
    while True:
        time.sleep(7)
        logger.debug("send data ! HELLO")
        packet = (
            IPv6(src=NETWORK_STACK.node_ip_addr, dst=dest_addr)
            / UDP(sport=UDP_SERVER_PORT, dport=UDP_CLIENT_PORT)
            / Raw(load="HELLO "+str(count))
        )

        """
            Allows scapy to add and compute automatically the
            len and chksum fields of the UDP packet
        """
        packet = IPv6(raw(packet))
        print("send PONG to "+dest_addr)
        NETWORK_STACK.send(packet)

        count+=1

def on_ip_packet():
    print("On ip packet !")

def main():
    logger.info("Welcome to LoRaMAC LOGGER")
    sys.excepthook = exception_handler
    NETWORK_STACK.init()
    NETWORK_STACK.register_listener(on_ip_packet)

    serial_logger1 = Thread(target=serial_log, args=(PORT_1, ZOLERTIA_BAUDRATE, logging.getLogger("RPL ROOT")))
    serial_logger1.start()

    #serial_logger2 = Thread(target=serial_log, args=(PORT_2, ZOLERTIA_BAUDRATE, logging.getLogger("RPL NODE")))
    #serial_logger2.start()

    sender = Thread(target=send_data_test)
    sender.start()


if __name__ == "__main__":

    print(__name__)
    import logging.config

    logging.config.fileConfig("./logging.conf")

    main()
