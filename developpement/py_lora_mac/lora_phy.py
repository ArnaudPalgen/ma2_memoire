import serial
import time
import queue
import threading
import logging

log = logging.getLogger(__name__)
log.setLevel(logging.DEBUG)


class LoraPhy:
    def __init__(self, port="/dev/ttyUSB0", baudrate=57600):
        self.con = None
        self.port=port
        self.baudrate=baudrate
        self.buffer = queue.Queue(10)
        self.listener = None

    def init(self):
        log.info("Init PHY")
        self.con = serial.Serial(port=self.port, baudrate=self.baudrate)
        thread = threading.Thread(target=self.uart_rx)
        self.send_phy("sys get ver")
        self.send_phy("mac pause")
        self.send_phy("radio set freq 868100000")
        self.send_phy("radio tx 48656c6C6F")
        thread.start()

    def send_phy(self, data):
        if type(data) == bytes:
            data = data + "\r\n".encode()
        elif type(data) == str:
            data = (data + "\r\n").encode()
        else:
            raise TypeError("Data must be bytes or str")
        
        if len(data > 255):
            raise ValueError("Data too big")
        
        try:
            self.buffer.put(data, block=False)
        except queue.Full:
            return False

        return True

    def phy_register_listener(self, listener):
        self.listener = listener

    def process_response(self, data):
        decode_data = data.decode()
        log.info("UART DATA: "+decode_data)
        if "radio_rx" in decode_data:
            log.debug("LoRa frame:"+decode_data)
            return False
        elif "radio_err" in decode_data:
            log.debug("radio error")
            return False
        elif "radio_rx" in decode_data:
            self.listener(decode_data)
            return False

        return True

    def uart_rx(self):
        can_send = True
        while self.con is not None:
            if can_send and not self.buffer.empty():
                data = self.buffer.get(block=False)
                log.info("Send UART data:"+ data)
                self.con.write(data)
                can_send = False
            if self.con.in_waiting > 0:
                data = self.con.readline()
                can_send = self.process_response(data)


if __name__ == '__main__':
    phy = LoraPhy()
    phy.init()
