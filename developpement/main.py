import logging
from py_lora_mac import mac_layer

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
        print("data: " + data.strip().decode())
        logger.debug(data.strip().decode())


def main():
    mac_layer.mac_init()
    serial_log(PORT_1, ZOLERTIA_BAUDRATE)


if __name__ == "__main__":

    print(__name__)
    import logging.config
    logging.config.fileConfig('./logging.conf')

    main()
