import logging as log
import serial


ZOLERTIA_BAUDRATE = 115200
ZOLERTIA_PORT = "/dev/ttyUSB1"

RN2483_BAUDRATE = 57600
RN2483_PORT = "/dev/ttyUSB0"

log.basicConfig(format='%(module)s-%(levelname)s-%(asctime)s-%(message)s', 
datefmt='%H:%M:%S', filename='log/loramacC.log', level=log.DEBUG)


con = serial.Serial(port=ZOLERTIA_PORT, baudrate=ZOLERTIA_BAUDRATE)