import serial
import time
import queue
import threading
import logging
from enum import Enum, auto, unique
from dataclasses import dataclass

log = logging.getLogger("LoRa PHY")
log.setLevel(logging.DEBUG)

@dataclass
class LoraAddr:
    prefix: int
    node_id: int

@dataclass
class LoraFrame:
    src_addr: LoraAddr
    des_addr: LoraAddr
    k: bool
    command: MacCommand
    

@unique
class MacCommand(Enum):
    JOIN=auto(),
    JOIN_RESPONSE=auto(),
    DATA=auto(),
    ACK=auto(),
    PING=auto(),
    PONG=auto(),
    QUERY=auto(),
    CHILD=auto(),
    CHILD_RESPONSE=auto() 

@unique
class UartResponse(Enum):
    OK = "ok",
    INVALID_PARAM = "invalid_param",
    RADIO_ERR = "radio_err",
    RADIO_RX = "radio_rx",
    BUSY = "busy",
    RADIO_TX_OK = "radio_tx_ok",
    U_INT = "4294967245",
    NONE = "none"

class LoraPhy:
    def __init__(self, port="/dev/ttyUSB0", baudrate=57600):
        self.con = None
        self.port=port
        self.baudrate=baudrate
        self.buffer = queue.Queue(10)
        self.listener = None
        self.can_send = True
        self.can_send_cond = threading.Condition()

    def phy_init(self):
        #set serial connection, call send_phy for mac pause et radio set freq
        logging.debug('Init PHY')
        self.con = serial.Serial(port=self.port, baudrate=self.baudrate)
        tx_thread = threading.Thread(target=self.uart_tx)
        rx_thread = threading.Thread(target=self.uart_rx)
        self.send_phy("mac pause")
        self.send_phy("radio set freq 868100000")
        rx_thread.start()
        tx_thread.start()
        
        with self.can_send_cond:
            self.can_send_cond.notify_all()

    def phy_register_listener(self, listener):
        self.listener = listener

    def phy_tx(self):#todo loraframe
        pass#todo

    def phy_timeout(self, timeout:int):
        pass#todo

    def phy_rx(self):
        pass #todo
#--------------------------------------------------------------------------------
    def send_phy(self, data):#append data to buffer
        if type(data) == bytes:
            data = data + "\r\n".encode()
        elif type(data) == str:
            data = (data + "\r\n").encode()
        else:
            raise TypeError("Data must be bytes or str")
        
        if len(data) > 255:
            raise ValueError("Data too big")
        
        try:
            self.buffer.put(data, block=False)
        except queue.Full:
            return False

        return True

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
        while True:
            data = self.con.readline()
            if self.process_response(data):
                with self.can_send_cond:
                    self.can_send = True
                    self.can_send_cond.notify_all()
    
    def uart_tx(self):
        while True:
            while self.con is None or not self.can_send:
                with self.can_send_cond:
                    self.can_send_cond.wait()
            data = self.buffer.get(block=True)
            log.info("Send UART data:"+ str(data))
            self.con.write(data)
            self.can_send = False


if __name__ == '__main__':
    phy = LoraPhy()
    phy.phy_init()
