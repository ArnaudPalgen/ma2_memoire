import queue
from threading import Timer, Event, Thread
from py_lora_mac.lora_phy import LoraAddr, LoraFrame, LoraPhy, MacCommand
from dataclasses import dataclass
from enum import Enum, auto, unique
import logging
import sys


log = logging.getLogger("LoRa_ROOT.MAC")

def exception_handler(type, value, traceback):
    s = "\n Type:"+str(type)+"\n Value:"+str(value)+"\n Traceback:"+str(traceback)
    log.exception(s)
    sys.exit(1)

sys.excepthook = exception_handler


MAX_PREFIX = 0xFC  # 252
MIN_PREFIX = 2

ROOT_PREFIX = 1
ROOT_ID = 0

RETRANSMIT_TIMEOUT = 5  # sec
MAX_RETRANSMIT = 3
RX_TIME = 3000  # 3 sec

CHILD_TX_BUF_SIZE = 5
CHILD_QUEUE_SIZE = 10


@unique
class ChildState(Enum):
    NEW = auto()
    READY = auto()
    WAIT_RESPONSE = auto()


class LoraChild:
    def __init__(self, addr: LoraAddr):
        self.addr = addr  # child's address

        self.expected_sn = 0  # expected SN for next received frame
        self._next_sn = 0  # SN for next sent frame

        self.last_send_frame: LoraFrame = None # last sent frame
        self.tx_buf = queue.Queue(CHILD_TX_BUF_SIZE) # tx buffer

        # -------------------------------------------------------------
        self.can_send = True
        # self.state = ChildState.NEW  # child's state
        # self.transmit_count = 1
        
        # self.lost_packet = 0
        # self.tx_buf.put(LoraFrame(LoraAddr(ROOT_PREFIX, ROOT_ID), LoraAddr(2, 24859), MacCommand.DATA, "48656c6C6F", False, True, False))

    def get_sn(self):
        """Return the sequence number (SN) to use
        and increment it

        Returns:
            int: the sequence number to use to send a LoRaFrame
        """
        sn: int = self._next_sn
        self._next_sn += 1
        return sn

    def __str__(self):
        return "Child("+str(self.addr)+")"

    """
    def restart_timer(self):
        self.timer = Timer(RETRANSMIT_TIMEOUT, self.retransmit_fun, [self])
        self.timer.start()
    """


class LoraMac:
    def __init__(self):
        self.phy_layer = LoraPhy()  # PHY layer

        # Contains all childs that didn't finish de join procedure(prefix:child)
        self.not_joined_childs = {}
        self.childs = {}  # Contains all childs (prefix:child)

        self.addr = LoraAddr(ROOT_PREFIX, ROOT_ID)  # node address
        self.action_matcher = {
            # contains functions for processing frames according to their MAC command (command:function)
            MacCommand.JOIN: self._on_join,
            MacCommand.ACK: self._on_ack,
            MacCommand.QUERY: self._on_query,
        }
        self.next_prefix = MIN_PREFIX  # next prefix to use for new child
        # -----------------------------------------------------------------------
        # self.childs_buf = queue.Queue(CHILD_QUEUE_SIZE)

    def mac_init(self):
        print("Init MAC")
        log.info("Init MAC")
        self.phy_layer.phy_register_listener(self._mac_rx)
        self.phy_layer.phy_init()
        self.phy_layer.phy_timeout(0)
        self.phy_layer.phy_rx()

        # tx_thread = Thread(target=self._tx_process)
        # tx_thread.start()

    
    def _on_query(self, frame: LoraFrame, child: LoraChild):
        log.debug("In _on_query")
        if child is None:
            log.warning("child not in list")
            return
        if frame.seq >= child.expected_sn:
            child.expected_sn = frame.seq+1
        else: # frame.seq < child.expected_sn -> retransmit
            self.phy_layer.phy_tx(child.last_send_frame)
            return

        if child.tx_buf.empty():
            log.debug("child buffer empty -> send ack")
            self.phy_layer.phy_tx(
                LoraFrame(
                    self.addr,
                    frame.src_addr,
                    MacCommand.ACK,
                    "",
                    frame.seq,
                    False,
                    False,
                )
            )
        else:
            pass #todo
    

    def _on_ack(self, frame: LoraFrame, child: LoraChild):
        """Process an ack frame

        Args:
            frame (LoraFrame): The LoRa frame to process
            child (LoraChild): The child that send the frame
        """
        log.info("receive ack %s", str(frame))

        if frame.seq == child.last_send_frame.seq:
            log.debug("correct ack number")

            child.can_send = True

            self.phy_layer.phy_timeout(0)
            self.phy_layer.phy_rx()
        else:
            log.warn("incorrect sn. expected: %d", child.last_send_frame.seq)

    def _on_join(self, frame: LoraFrame, child: LoraChild):
        """Process a join frame.
        The joining sequence is described by de diagrame below

        RPL_ROOT                                                                 LORA_ROOT
            | -----------------JOIN[(prefix=node_id[0:8], node_id)]----------------> |
            | <-- JOIN_RESPONSE[(prefix=node_id[0:8], node_id), data=new_prefix] --> |

        Args:
            frame (LoraFrame): The LoRa frame to process
            child (LoraChild): The child that send the frame

        """
        log.info("reveive join frame %s", str(frame))

        if frame.seq != 0:
            log.warn("incorrect sn")

        if child is not None:
            # it's a node that have already fully joined the network -> nothing to do
            return

        # perhaps that it's a node that retransmits the JOIN frame
        child = self.not_joined_childs.get(frame.src_addr.prefix, None)

        if child is not None:
            # it is a retransmission
            log.info("Child %s already exist->retransmit", str(child))
            self.phy_layer.phy_tx(child.last_send_frame)
            return


        if self.next_prefix > MAX_PREFIX:
            # can't accept new child
            log.warning("can't accept new child")
            return

        new_prefix = self.next_prefix
        self.next_prefix += 1

        # create the child
        new_child = LoraChild(
            LoraAddr(new_prefix, frame.src_addr.node_id))
        self.childs[new_prefix] = new_child
        self.not_joined_childs[frame.src_addr.prefix] = new_child
        log.debug("new child %s created", str(new_child))

        # send the join response
        log.debug("send join response")
        self.phy_layer.phy_tx(
            LoraFrame(
                self.addr,
                frame.src_addr,
                MacCommand.JOIN_RESPONSE,
                "%02X" % new_prefix,
                new_child.get_sn(),
                False,
            )
        )
        self.phy_layer.phy_timeout(0) # disable watchdog timer
        self.phy_layer.phy_rx() # listen

    def _mac_rx(self, frame: LoraFrame):
        """Receive LoRaFrame frome the PHY layer.
            - checks that the destination address is correct
            - retrieves the child if exist
            - calls the appropriate function to process the frame

        Args:
            frame (LoraFrame): The frame frome the PHY layer
        """
        log.debug("MAC RX: %s", str(frame))

        if frame.dest_addr != self.addr:
            log.debug("frame not for me")
            return

        log.debug("look child with prefix: %d", frame.src_addr.prefix)
        child: LoraChild = self.childs.get(frame.src_addr.prefix, None)
        log.debug("child is: %s", str(child))
        if frame.seq == 1 and child is not None:
            # receive the first frame from this child
            # i.e. the join procedure is completed
            self.not_joined_childs.pop(child.addr.node_id & 255, None)

        fun = self.action_matcher.get(frame.command, None)
        if fun is not None:
            fun(frame, child)
        else:
            log.warning("unknown MAC command")

    """
    def mac_tx(self, frame: LoraFrame) -> bool:
        child = self.childs[frame.dest_addr.prefix]
        log.debug("in mac tx. FULL:%s", str(child.tx_buf.full()))
        if not child.tx_buf.full():
            child.tx_buf.put(frame, block=False)
            log.debug("put for frame to buf of %s", str(child))
            return True
        return False
    """

    """
    def _tx_process(self):
        while True:

            try:
                # get  next child for wich frame are available to send
                child = self.childs_buf.get(block=False)
            except queue.Empty:
                # if there is no child, we have send all frame
                # so we listen
                self.phy_layer.phy_timeout(0)
                self.phy_layer.phy_rx()
                child = self.childs_buf.get(block=True)

            while child.can_send and child.tx_buf.qsize() > 0:
                try:
                    frame = child.tx_buf.get(block=False)
                    log.warning("TX send frame: %s", str(frame))
                except queue.Empty:
                    break
                child.last_send_frame = frame
                if (
                    child.tx_buf.qsize() > 0
                    and frame.command != MacCommand.JOIN_RESPONSE
                ):
                    frame.has_next = True
                frame.seq = child.ack
                self.phy_layer.phy_tx(frame)
                log.info("send %s", str(frame))
                if frame.k or frame.command == MacCommand.JOIN_RESPONSE:
                    child.timer.start()
                    log.info("start retransmit timer")
                    child.can_send = False
                    if frame.has_next:
                        self.childs_buf.put(child)
                    self.phy_layer.phy_timeout(RX_TIME)
                    self.phy_layer.phy_rx()
                else:
                    child.ack = not child.ack
    """


# if __name__ == "__main__":
#    mac = LoraMac()
#    mac.mac_init()
# mac.mac_tx(LoraFrame(LoraAddr(ROOT_PREFIX, ROOT_ID), LoraAddr(2, 24859), MacCommand.DATA, "48656c6C6F", False, True, False))
# mac._mac_rx(LoraFrame.build("radio_rx  00611B01000000\r\n"))
