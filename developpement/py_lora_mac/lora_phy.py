import serial
import time
import queue
import threading
import logging as log
from enum import Enum, auto, unique
from dataclasses import dataclass

log.basicConfig(format='%(module)s-%(levelname)s-%(asctime)s-%(message)s', 
datefmt='%H:%M:%S', filename='log/loramacC.log', level=log.DEBUG)

HEADER_SIZE = 14

K_FLAG_SHIFT = 7
NEXT_FLAG_SHIFT = 6
#
# - - - - - - - -


@unique
class MacCommand(Enum):
    JOIN = 0
    JOIN_RESPONSE = 1
    DATA = 2
    ACK = 3
    PING = 4
    PONG = 5
    QUERY = 6
    CHILD = 7
    CHILD_RESPONSE = 8


@unique
class UartCommand(Enum):
    MAC_PAUSE = "mac pause"  # pause mac layer
    SET_MOD = "radio set mod "  # set radio mode (fsk or lora)
    # set radio freq from 433050000 to 434790000 or from 863000000 to 870000000, in Hz.
    SET_FREQ = "radio set freq "
    SET_WDT = "radio set wdt "  # set watchdog timer
    RX = "radio rx "  # receive mode
    TX = "radio tx "  # transmit data
    SLEEP = "sys sleep "  # system sleep


@unique
class UartResponse(Enum):
    OK = "ok"
    INVALID_PARAM = "invalid_param"
    RADIO_ERR = "radio_err"
    RADIO_RX = "radio_rx"
    BUSY = "busy"
    RADIO_TX_OK = "radio_tx_ok"
    U_INT = "4294967245"
    NONE = "none"


@dataclass(frozen=True)
class LoraAddr:
    prefix: int
    node_id: int

    def toHex(self):
        return "%02X" % self.prefix + "%04X" % self.node_id


"""
LoRa frame: 
|<---24---->|<----24--->|<-1->|<-1-->|<---2--->|<--4--->|<--8--->|<(2040-64=1976)>|
| dest addr |  src addr |  k  | next | reserved|command |  seq   |     payload    |

"""
@dataclass
class LoraFrame:
    src_addr: LoraAddr
    dest_addr: LoraAddr
    command: MacCommand
    payload: str  # must be to hex
    seq: int    #sequence number
    k: bool = False #need ack ?
    has_next: bool = False

    def toHex(self):
        """ create flags and MAC command"""
        f_c = 0
        f_c |= self.k << K_FLAG_SHIFT
        f_c |= self.has_next << NEXT_FLAG_SHIFT
        f_c |= self.command.value
        

        return (
            self.src_addr.toHex()
            + self.dest_addr.toHex()
            + ("%02X" % f_c)
            + ("%02X" % self.seq)
            + (self.payload if self.payload else "")
        )

    @staticmethod
    def build(data: str):
        if len(data) < HEADER_SIZE:
            return None

        """ extract src addr: most significant 24 bits"""
        prefix_src = int(data[0:2], 16)
        node_id_src = int(data[2:6], 16)

        """ extract dest addr: next 24 bits """
        prefix_dest = int(data[6:8], 16)
        node_id_dest = int(data[8:12], 16)

        """ extract flags an MAC command: 8 bits """
        f_c = int(data[12:14], 16)

        flag_filter = 0x01 # 1 bit
        cmd_filter = 0x0F # 4 bits

        k = bool((f_c >> K_FLAG_SHIFT) & flag_filter)
        has_next = bool((f_c >> NEXT_FLAG_SHIFT) & flag_filter)

        cmd = MacCommand(f_c & cmd_filter)

        """ extract sequence number: 8 bits """
        seq = int(data[14:16])

        """ extract payload """
        payload = data[16:]

        """ create LoraFrame with computed values"""
        return LoraFrame(
            LoraAddr(prefix_src, node_id_src),
            LoraAddr(prefix_dest, node_id_dest),
            cmd,
            payload,
            seq,
            k,
            has_next,
        )


@dataclass
class UartFrame:
    expected_response: list
    data: str
    cmd: UartCommand


class LoraPhy:
    def __init__(self, port="/dev/ttyUSB0", baudrate=57600):
        self.con = None
        self.port = port
        self.baudrate = baudrate
        self.buffer = queue.Queue(10)
        self.listener = None
        self.can_send = True
        self.can_send_cond = threading.Condition()
        self.last_sended = None

    def phy_init(self):
        # set serial connection, call send_phy for mac pause et radio set freq
        log.info('Init PHY')
        self.con = serial.Serial(port=self.port, baudrate=self.baudrate)
        tx_thread = threading.Thread(target=self.uart_tx)
        rx_thread = threading.Thread(target=self.uart_rx)
        self._send_phy(UartFrame([UartResponse.U_INT], "", UartCommand.MAC_PAUSE))
        self._send_phy(UartFrame([UartResponse.OK], "868100000", UartCommand.SET_FREQ))
        rx_thread.start()
        tx_thread.start()

        with self.can_send_cond:
            self.can_send_cond.notify_all()

    def phy_register_listener(self, listener):
        self.listener = listener

    def phy_tx(self, loraFrame: LoraFrame):
        if loraFrame is None:
            return
        f = UartFrame([UartResponse.RADIO_TX_OK, UartResponse.RADIO_ERR], loraFrame.toHex(), UartCommand.TX)
        self._send_phy(f)

    def phy_timeout(self, timeout: int):
        f = UartFrame([UartResponse.OK], str(timeout), UartCommand.SET_WDT)
        self._send_phy(f)

    def phy_rx(self):
        f = UartFrame([UartResponse.RADIO_ERR, UartResponse.RADIO_RX], "0", UartCommand.RX)
        self._send_phy(f)

    # --------------------------------------------------------------------------------
    def _send_phy(self, data):  # append data to buffer
        if type(data) != UartFrame:
            raise TypeError("Data must be UartFrame. actual type: ", type(data))

        try:
            log.debug("append %s to tx_buf", str(data))
            self.buffer.put(data, block=False)
        except queue.Full:
            log.warning("  buffer full")
            return False

        return True

    def process_response(self, data: str):
        decode_data = data.decode()
        log.info("process uart response: " + decode_data)
        for resp in self.last_sended.expected_response:
            if resp is None:
                continue
            if resp.value in decode_data:
                if resp==UartResponse.RADIO_RX:
                    self.listener(LoraFrame.build(decode_data[10:].strip()))
                return True
        log.debug("unexpected response")
        return False

    def uart_rx(self):
        while True:
            data = self.con.readline()
            if self.process_response(data.strip()):
                with self.can_send_cond:
                    self.can_send = True
                    self.can_send_cond.notify_all()

    def uart_tx(self):
        while True:
            while self.con is None or not self.can_send:
                with self.can_send_cond:
                    self.can_send_cond.wait()
            self.last_sended = self.buffer.get(block=True)
            log.info("Send UART data:" + str(self.last_sended))
            self.con.write((self.last_sended.cmd.value + self.last_sended.data + "\r\n").encode())
            self.can_send = False


if __name__ == '__main__':
    print("=== test frame parsing/building ===\n")
    a = LoraFrame(LoraAddr(178, 45797), LoraAddr(179, 49878), MacCommand.PING, '', True, k=True)
    print(" inital frame     :", a)
    to_hex = a.toHex()
    print(" to hex           : "+to_hex)
    b = LoraFrame.build(to_hex)
    print(" rebuild from hex :", b)
    print(" frames are equals:", a==b)
    print("\n===================================")
