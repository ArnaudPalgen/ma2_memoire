import logging
import py_lora_mac.main as loramac

logger = logging.getLogger("RPL ROOT.")

LOG_DIR = "logs"

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
    loramac.run()
    serial_log(PORT_0, ZOLERTIA_BAUDRATE)


if __name__ == "__main__":
    import datetime

    logging.basicConfig(
        format="%(module)s-%(levelname)s-%(asctime)s-%(message)s",
        datefmt="%H:%M:%S",
        filename=LOG_DIR
        + str(datetime.datetime.now().strftime("%Y%m%d-%H:%M:%S"))
        + ".log",
        level=logging.DEBUG,
    )
    main()
