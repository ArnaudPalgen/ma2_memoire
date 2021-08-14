import serial
import queue
import threading
from enum import Enum, auto, unique
from dataclasses import dataclass
import logging
import time
import csv
import random

log = logging.getLogger("LoRa_ROOT.PHY")

HEADER_SIZE = 16 #NUmber of hexadecimal character in the header

K_FLAG_SHIFT = 7
NEXT_FLAG_SHIFT = 6


ENABLE_STAT = True
TEST_LOSS = False
LOSS_PROBABILITY = 0 #in [0, 1]

@unique
class MacCommand(Enum):
    """The MAC commands available for LoRaMAC."""

    JOIN = 0
    JOIN_RESPONSE = 1
    DATA = 2
    ACK = 3
    QUERY = 4


@unique
class UartCommand(Enum):
    """The UART commands used with the RN2383."""

    MAC_PAUSE = "mac pause"  # pause mac layer
    RX = "radio rx "  # receive mode
    TX = "radio tx "  # transmit data
    SLEEP = "sys sleep "  # system sleep

    """
    The modulation method.
    Values can be: lora, fsk.
    Default: lora
    """
    SET_MOD = "radio set mod "  # set radio mode (fsk or lora)

    """
    The frequency in Hz.
    From 433050000 to 434790000 or from 863000000 to 870000000
    Default: 868100000
    """
    SET_FREQ = "radio set freq "

    """
    The operating bandwidth in KHz.
    Values can be: 125, 250, 500.
    Default: 125
    """
    SET_BW = "radio set bw "

    """
    The transceiver output power.
    From -3 to 15.
    Default: 1
    """
    SET_PWR = "radio set pwr "

    """
    The spreading factor.
    Values can be: sf7, sf8, sf9, sf10, sf11 or sf12.
    Default: sf12
    """
    SET_SF = "radio set sf "

    """
    The coding rate.
    Values can be: 4/5, 4/6, 4/7, 4/8.
    Default: 4/5
    """
    SET_CR = "radio set cr "

    """
    The time-out length for the Watchdog Timer in milliseconds.
    From 0 to 4294967295. 0 disable this functionality.
    Default: 15000
    """
    SET_WDT = "radio set wdt "


@unique
class UartResponse(Enum):
    """The possible UART responses for the RN2483."""

    OK = "ok"
    INVALID_PARAM = "invalid_param"
    RADIO_ERR = "radio_err"
    RADIO_RX = "radio_rx"
    BUSY = "busy"
    RADIO_TX_OK = "radio_tx_ok"
    U_INT = "4294967245" # The response to MAC PAUSE
    NONE = "none"


@dataclass(frozen=True)
class LoraAddr:
    """A LoRaMAC address

    The format of a LoRaMAC adress is the following (size in bits):
                |<---8-->|<---16-->|
                | prefix | node-id |

    Attributes:
        prefix: An integer which is the prefix of the address.
        node_id: An integer chich is the node id of the node.
    """

    prefix: int
    node_id: int

    def toHex(self) -> str:
        """Serialize the adress to a string in hexadecimal.

        Returns:
            str: The serialized address
        """
        return "%02X" % self.prefix + "%04X" % self.node_id

    def __str__(self):
        return str(self.prefix) + ":" + str(self.node_id)


@dataclass
class LoraFrame:
    """A LoRaMAC frame

        The format of a LoRaMAC frame is the following (size in bits):

    |<---24---->|<----24--->|<-1->|<-1-->|<---2--->|<--4--->|<--8--->|<(2040-64=1976)>|
    | dest addr |  src addr |  k  | next | reserved|command |  seq   |     payload    |

        Attributes:
            src_addr: The source address
            dest_addr: The Destination address
            command: The MAC command (c.f. MacCommand)
            payload: The payload. Must be a string in hexadecimal
            seq: The sequence number
            k: True if the frame need an ack, False otherwise
            has_next: True true if another frame follows it, False otherwise. Only used for downward traffic
    """

    src_addr: LoraAddr
    dest_addr: LoraAddr
    command: MacCommand
    payload: str
    seq: int = 0
    k: bool = False
    has_next: bool = False

    def toHex(self) -> str:
        """Serialize the frame.

        Returns:
            str: The serialized frame
        """

        # create flags and MAC command
        f_c = 0
        f_c |= self.k << K_FLAG_SHIFT
        f_c |= self.has_next << NEXT_FLAG_SHIFT
        f_c |= self.command.value

        # check that the size of the payload is even
        if len(self.payload) % 2 != 0:
            # The size must be even because one character is 4 bits and we
            # can't send a half a byte
            # if not even add a zero
            self.payload = "0" + self.payload

        return (
            self.src_addr.toHex()
            + self.dest_addr.toHex()
            + ("%02X" % f_c)
            + ("%02X" % self.seq)
            + (self.payload if self.payload else "")
        )

    @staticmethod
    def build(data: str):
        """Deserialize (or build) a LoRaFrame.

        Returns:
            LoraFrame: The frame built from the data.

        """

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

        flag_filter = 0x01  # 1 bit
        cmd_filter = 0x0F  # 4 bits

        k = bool((f_c >> K_FLAG_SHIFT) & flag_filter)
        has_next = bool((f_c >> NEXT_FLAG_SHIFT) & flag_filter)

        cmd = MacCommand(f_c & cmd_filter)

        """ extract sequence number: 8 bits """
        seq = int(data[14:16], 16)

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
    """An UART paquet

    Attributes:
        expected_response: The expected UART response to this paquet
        data: The sent data
        cmd: The UART command (c.f. UartCommand)
    """

    expected_response: list
    data: str
    cmd: UartCommand
    stat_id: int = -1


class PhyMeter:

    instance = None

    def __init__(self, dest_file):
        self.data_list=[]#[[data, put_time, send_time, diff]]
        self.counter = -1
        self.dest_file = dest_file

    def put(self,data):
        data.stat_id = self._get_id()
        self.data_list.append([data, time.perf_counter_ns(), -1, -1])

    def send(self,data):
        r = self.data_list[data.stat_id]
        r[2]=time.perf_counter_ns()
        r[3]=r[2]-r[1]

    @staticmethod
    def getMeter(dest_file='stat.txt'):
        if PhyMeter.instance == None:
            PhyMeter.instance = PhyMeter(dest_file)
        return PhyMeter.instance

    def _get_id(self):
        self.counter+=1
        return self.counter

    def export_data(self):
        #with open(self.dest_file, "w", newline='') as f:
        writer = csv.writer(open(self.dest_file, "w", newline=''), 'unix')
        writer.writerow(['put time', 'send time', 'delta', 'data'])
        writer.writerow([self.data_list[0][0], self.data_list[-1][1], len(self.data_list), 'general info'])
        for data in self.data_list:
            writer.writerow([data[1], data[2], data[3], str(data[0])])



class LoraPhy:
    """The LoRaMAC PHY layer.

    The PHY layer is the driver for the RN2483.

    """

    def __init__(self, **params):
        self._con = None  # The serial conenction
        #self._port = port  # The serial port
        #self._baudrate = baudrate  # The serial baudrate
        self._params = params
        self._buffer = queue.Queue(self._params.get('txBufSize', 10))  # The TX buffer
        self._rx_buffer = queue.Queue(self._params.get('rxBufSize', 10))  # the RX buffer
        self._can_send = True  # True if the tx process can send, False otherwise
        self._can_send_cond = threading.Condition()  # condition used by the tx_process
        self._last_sended = None  # the last sended frame
        self._tx_lock = threading.Lock()  # lock used for phy_tx()
        self.listen_lock = threading.Lock()
        self._is_listen = False

    def init(self):
        """Init the PHY layer.

        - Set the serial connection
        - Prepare the RN2483 for communications (mac pause, radio set)
        """
        # set serial connection, call send_phy for mac pause et radio set freq
        log.info("Init PHY")
        try:
            self._con = serial.Serial(port=self._params.get('port', "/dev/ttyUSB0"), baudrate=self._params.get('baudrate', 57600))
        except serial.serialutil.SerialException as e:
            log.error(str(e))
            exit()
        tx_thread = threading.Thread(target=self._uart_tx)
        rx_thread = threading.Thread(target=self._uart_rx)

        log.info("Radio configuration: %s", str(self._params))

        # Radio configuration
        self._send_phy(UartFrame([UartResponse.U_INT], "", UartCommand.MAC_PAUSE))
        self._send_phy(UartFrame([UartResponse.OK], self._params.get('mode',"lora"), UartCommand.SET_MOD))
        self._send_phy(UartFrame([UartResponse.OK], self._params.get('frequence',"868100000"), UartCommand.SET_FREQ))
        self._send_phy(UartFrame([UartResponse.OK], self._params.get('bandwidth',"125"), UartCommand.SET_BW))
        self._send_phy(UartFrame([UartResponse.OK], self._params.get('cr',"4/5"), UartCommand.SET_CR))
        self._send_phy(UartFrame([UartResponse.OK], self._params.get('pwr',"1"), UartCommand.SET_PWR))
        self._send_phy(UartFrame([UartResponse.OK], self._params.get('sf',"sf2"), UartCommand.SET_SF))
        
        rx_thread.start()
        tx_thread.start()

        with self._can_send_cond:
            self._can_send_cond.notify_all()

    def phy_send(self, loraFrame: LoraFrame):
        """Method to use to send LoraFrame.

        Prepare the UART paquet from the LoRa frame.

        Args:
            loraFrame (LoraFrame): The frame to sent.
        """

        log.info("MAC send:%s", str(loraFrame))
        self._tx_lock.acquire()
        if TEST_LOSS:
            if not random.random() < LOSS_PROBABILITY:
                log.warning("LOSS TEST: don't send %s", str(loraFrame))
                return
        if loraFrame is None:
            return
        f = UartFrame(
            [UartResponse.RADIO_TX_OK, UartResponse.RADIO_ERR],
            loraFrame.toHex(),
            UartCommand.TX,
        )
        self._send_phy(f)
        self._tx_lock.release()

    def phy_timeout(self, timeout: int):
        """Set the RN2483 radio watchdog timer.

        Args:
            timeout (int): The timeout (in milliseconds)
        """

        log.info("MAC set watchdog timer to %d ms", timeout)
        f = UartFrame([UartResponse.OK], str(timeout), UartCommand.SET_WDT)
        self._send_phy(f)

    def phy_rx(self):
        """Set the radio the reception mode"""

        self.listen_lock.acquire()
        self._is_listen = True
        self.listen_lock.release()
        log.info("MAC switch to reception mode")
        f = UartFrame(
            [UartResponse.RADIO_ERR, UartResponse.RADIO_RX], "0", UartCommand.RX
        )
        self._send_phy(f)

    def listen(self) -> bool:
        result = None
        self.listen_lock.acquire()
        result = self._is_listen
        self.listen_lock.release()
        return result

    def getFrame(self) -> LoraFrame:
        """Get the next received frame.

        This method block until a frame is available.

        Returns:
            LoraFrame: A received frame.
        """
        frame = self._rx_buffer.get()
        return frame

    def _send_phy(self, data: UartFrame) -> bool:
        """Append data to the TX buffer.

        Args:
            data (UartFrame): The UART frame to put in the tx buffer.

        Raises:
            TypeError: If the data type is not UartFrame

        Returns:
            bool: True if the data has been added to the buffer, False otherwise
        """

        if type(data) != UartFrame:
            raise TypeError("Data must be UartFrame. actual type: ", type(data))

        try:
            log.debug("append %s to tx_buf", str(data))
            self._buffer.put(data, block=False)
            if ENABLE_STAT:
                PhyMeter.getMeter().put(data)
        except queue.Full:
            log.warning("TX buffer full")
            return False

        return True

    def _process_response(self, decode_data: str) -> bool:
        """Process an UART response.

        Args:
            data (str): The UART response.

        Returns:
            bool: True if the answer is the one expected, False otherwise.
        """
        #decode_data = data
        log.debug("process uart response: " + decode_data)
        for resp in self._last_sended.expected_response:
            if resp is None:
                continue
            if resp.value in decode_data:  # the response is the one expected
                if resp == UartResponse.RADIO_RX:  # the response is DATA
                    log.info("PHY RX:" + decode_data[10:].strip())
                    #if ENABLE_STAT:
                    #    log.info("STAT: PHY RX: %d", time.monotonic_ns())
                    try:
                        self._rx_buffer.put(
                            LoraFrame.build(decode_data[10:].strip()), block=False
                        )
                    except queue.Full:
                        log.warning("RX buffer full")

                return True
        log.debug("unexpected response")
        return False

    def _uart_rx(self):
        """Method used as Thread to read data from the serial connection."""
        while True:
            data = self._con.readline().strip().decode()
            if ("radio_rx" in data) or ("radio_err" in data):
                self.listen_lock.acquire()
                self._is_listen = False
                self.listen_lock.release()

            log.debug("UART response: %s", data)
            if self._process_response(data):
                # It is the expected response
                # Notify threads waiting for the response
                with self._can_send_cond:
                    self._can_send = True
                    self._can_send_cond.notify_all()

    def _uart_tx(self):
        """Method used as Thread to send data to the serial connection."""
        while True:
            while self._con is None or not self._can_send:
                with self._can_send_cond:
                    self._can_send_cond.wait()
            self._last_sended = self._buffer.get(block=True)
            log.info("PHY TX:" + self._last_sended.cmd.value + self._last_sended.data)
            if ENABLE_STAT:
                PhyMeter.getMeter().send(self._last_sended)
                #if self._last_sended.cmd == UartCommand.TX:
                #    log.info("STAT: PHY TX: %d", time.monotonic_ns())
            self._con.write(
                (self._last_sended.cmd.value + self._last_sended.data + "\r\n").encode()
            )
            self._can_send = False
