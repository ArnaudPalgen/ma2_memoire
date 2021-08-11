from py_lora_mac.lora_phy import *
from threading import Timer, Event, Thread, Lock
from typing import Union, Callable, Type, Tuple
from operator import add, sub
import sys
import time

log = logging.getLogger("LoRa_ROOT.MAC")


MAX_PREFIX = 0xFC  # 252
MIN_PREFIX = 2

#LoRaMAC root address
ROOT_PREFIX = 1
ROOT_ID = 0

MAX_RETRANSMIT = 3

CHILD_TX_BUF_SIZE = 5

MIN_WAIT_TIME = 1  # sec


@unique
class ChildState(Enum):#todo remove
    NEW = auto()
    READY = auto()
    WAIT_RESPONSE = auto()


class LoraChild:
    def __init__(self, addr: LoraAddr):
        self.addr = addr  # child's address

        self.expected_sn = 0  # expected SN for next received frame
        self._next_sn = 0  # SN for next sent frame

        self.last_send_frame: LoraFrame = None  # last sent frame
        self.tx_buf = queue.Queue(CHILD_TX_BUF_SIZE)  # tx buffer

        #todo remove
        #self.ack_lock = Lock()  # used to block the process until an ack is received
        self.tx_process = None  # send data to child

        self.transmit_count = 1 # number of retransmit
        self.retransmit_event = Event()
        self.not_send_count = 0 # for stat

    def clear_transmit_count(self):
        self.transmit_count = 1

    def get_sn(self)->int:
        """Return the sequence number (SN) to use
        and increment it

        Returns:
            int: the sequence number to use to send a LoRaFrame
        """
        sn: int = self._next_sn
        self._next_sn += 1
        return sn

    def is_transmit(self)->bool:
        """ Specifies if the child is transmitting. i.e. if there is a
        TX process for the child and this process is alive.

        Returns:
            bool: True if a TX process exist and is alive. False otherwise
        """
        return self.tx_process is not None and self.tx_process.is_alive()

    def __str__(self):
        return "Child(" + str(self.addr) + ")"


class LoraMac:
    def __init__(self, phy_layer: LoraPhy):
        self.phy_layer = phy_layer  # PHY layer

        # Contains all childs that didn't finish de join procedure(prefix:child)
        self.not_joined_childs = {}
        self.childs = {}  # Contains all childs (prefix:child)

        self.addr = LoraAddr(ROOT_PREFIX, ROOT_ID)  # node address
        self.action_matcher = {
            # contains functions for processing frames according to their MAC command (command:function)
            MacCommand.JOIN: self._on_join,
            MacCommand.ACK: self._on_ack,
            MacCommand.QUERY: self._on_query,
            MacCommand.DATA: self._on_data
        }
        self.next_prefix = MIN_PREFIX  # next prefix to use for new child

        # list contains prefix that can be reused
        # use queue to prevent to reuse a prefix that a child
        # continue to use despite of is was removed
        self.available_prefix = queue.Queue()

        # used not to listen when a tx process is going to send or is sending
        self.can_listen = True
        self.is_listen = False  # tell if listen
        self.listen_lock = Lock()  # lock for can_listen and listen
        self.upper_layer = None

    def init(self):
        """Init the MAC layer.
        
        - Init the PHY layer
        - Listen
        """
        log.info("Init MAC")
        self.phy_layer.init()
        self.phy_layer.phy_timeout(0)
        self.phy_layer.phy_rx()
        self.is_listen = True

        tx_thread = Thread(target=self._rx_process)
        tx_thread.start()

    def mac_send(self, dest:LoraAddr, payload:str, k=False):#todo remove k parameter
        """Send a payload to the destination dest.

        Args:
            dest (LoraAddr): The destination address.
            payload (str): The data to be sent.
            k (bool, optional): Set to true to request an ack. Defaults to False.
        """
        log.info("IP send %s to: %s", payload, str(dest))
        try:
            child = self.childs[dest.prefix]
            child.tx_buf.put(LoraFrame(self.addr, dest, MacCommand.DATA, payload))
        except KeyError:
            log.error("Destination %s unreachable.", dest)

    def register_listener(self, listener: Callable[[LoraAddr, str], None]):
        """Register a listener that will be called when data is available
        for upper layer.

        Args:
            listener (Callable[[LoraAddr, str], None]): The listener.
        """
        self.upper_layer = listener

    def _on_query(self, frame: LoraFrame, child: LoraChild):#current
        """Process a query frame.

        Args:
            frame (LoraFrame): The LoRa frame to process.
            child (LoraChild): The child that send the frame.
        """
        log.debug("In _on_query")
        if child is None:
            log.warning("child not in list")
            return
        #if frame.seq >= child.expected_sn:
        #    child.expected_sn = frame.seq + 1
        #else:  # frame.seq < child.expected_sn -> retransmit
        #    self.phy_layer.phy_send(child.last_send_frame)
        #    
        #    return

        if frame.seq < child.expected_sn:
            self._retransmit(child)
            return
        if frame.seq > child.expected_sn:
            log.debug("frame seq > expected")
            log.warn("Loss of %d frames", (frame.seq - child.expected_sn))
        
        child.expected_sn = frame.seq + 1


        if child.tx_buf.empty():  # no data for this child -> send an ack
            log.debug("child buffer empty -> send ack")
            self._send_ack(child, frame.src_addr, frame.seq)
            ##self.phy_layer.phy_send(
            ##    LoraFrame(
            ##        self.addr,
            ##        frame.src_addr,
            ##        MacCommand.ACK,
            ##        "",
            ##        frame.seq,
            ##        False,
            ##        False,
            ##    )
            ##)
        else:
            # data available for this child
            # start a tx thread if there is no other running
            # set can_listen to False
            if (child.tx_process is None) or (not child.tx_process.is_alive()):
                child.tx_process = Thread(
                    target=self._tx_process, kwargs={"child": child}
                )
                log.debug("start tx process for this child")
                self.listen_lock.acquire()
                self.can_listen = False
                self.listen_lock.release()
                child.tx_process.start()


    def _tx_process(self, *args, **kwargs):
        """Process that sends alls the LoRaFrame in a child buffer
        The child must pas in the **kwargs parameters with the key "child"
        """

        child: LoraChild = kwargs["child"]  # get the child
        while not child.tx_buf.empty():
            try:
                next_frame = child.tx_buf.get_nowait()  # get the next frame to send
                log.debug("next frame to send is: %s", str(next_frame))
                log.debug("tx buffer size:%d", child.tx_buf.qsize())
            except queue.Empty:
                log.warning("no next frame")
                break
            
            #todo: remove
            #if next_frame.k:
            #    # if the frame need an ack, acquire the lock the first time
            #    child.ack_lock.acquire()

            # set the SN and has_next flag of the frame
            next_frame.seq = child.get_sn()
            next_frame.has_next = not child.tx_buf.empty()
            log.debug("send next frame")
            self.phy_layer.phy_send(next_frame)  # send the frame
            child.last_send_frame = next_frame  # set the frame as last frame

            """
            if next_frame.k:  # if frame need an ack
                log.debug("I wait ACK")
                self.listen_lock.acquire()

                self.is_listen = True  # tell that we listen
                self.phy_layer.phy_rx()  # listen
                self.can_listen = True  # tell that we can listen (because we listen)

                self.listen_lock.release()

                # acquire the lock the second time
                # this will block the process until an ack is received
                child.ack_lock.acquire()
                child.ack_lock.release()
            else:
                log.debug("I don't need ack")
            """
            has_retransmit = True
            while has_retransmit:
                log.debug("wait for a possible retransmission query from the child")
                # wait MIN_WAIT_TIME.
                # If during there is a retrasmission (requested by the child),
                # wait MIN_WAIT_TIME again
                log.debug("WAIT retransmit")
                has_retransmit = child.retransmit_event.wait(MIN_WAIT_TIME)
                log.debug("has retransmit: %s", str(has_retransmit))
            
            child.clear_transmit_count()

            if child.transmit_count > MAX_RETRANSMIT:
                # we have done MAX_RETRANSMIT retransmissions
                log.warning("Max retransmit reached")
                child.not_send_count += 1
                break  # stop tx_process and wait next query to transmit child's buffer
        
        self.listen_lock.acquire()
        self.is_listen = True  # tell that we listen
        self.phy_layer.phy_rx()  # listen
        self.can_listen = True  # tell that we can listen (because we listen)
        self.listen_lock.release()

    def _retransmit(self, child:LoraChild):
        # current
        log.info("Retransmit for %s", str(child))
        if child.retransmit_count <= MAX_RETRANSMIT:
            self.phy_layer.phy_send(child.last_send_frame)
            child.retransmit_count += 1

        if child.is_transmit():
            child.retransmit_event.set()


    def _send_ack(self, child:LoraChild, dest_addr:LoraAddr, sn:int):
        #current
        log.debug("send ack %d to %s", sn, str(dest_addr))
        ack = LoraFrame(self.addr, dest_addr, MacCommand.ACK, "", sn)
        self.phy_layer.phy_send(ack)
        child.last_send_frame = ack

    def _on_data(self, frame: LoraFrame, child: LoraChild):#done
        """Process a data frame.

        Args:
            frame (LoraFrame): The LoRa frame to process
            child (LoraChild): The child that send the frame
        """
        log.debug("In _on_data ")
        log.debug("expected sn: %d", child.expected_sn)
        if frame.seq < child.expected_sn:
            log.debug("frame.seq < expected seq")
            self._retransmit(child)
            """
            if child.retransmit_count <= MAX_RETRANSMIT:
                log.info("Frame SN < expected SN -> retransmit")
                self.phy_layer.phy_send(child.last_send_frame)
            
            child.retransmit_count += 1
            if child.is_transmit():
                child.retransmit_event.set()
            """
            return
        
        if frame.seq > child.expected_sn:
            log.debug("frame seq > expected")
            log.warn("Loss of %d frames", (frame.seq - child.expected_sn))
        
        child.expected_sn = frame.seq + 1
        
        if frame.k:
            self._send_ack(child, frame.src_addr, frame.seq)
            """
            log.debug("frame want ack -> send ack")
            ack = LoraFrame(
                    self.addr,
                    frame.src_addr,
                    MacCommand.ACK,
                    "",
                    frame.seq,
                    False,
                    False,
                )
            self.phy_layer.phy_send(ack)
            child.last_send_frame = ack
            """
        self.upper_layer(frame.src_addr, frame.payload) #deliver data to upper layer
    
    """#todo remove
    def _on_ack(self, frame: LoraFrame, child: LoraChild):
        Process an ack frame

        Args:
            frame (LoraFrame): The LoRa frame to process
            child (LoraChild): The child that send the frame
        
        log.debug("receive ack %s", str(frame))

        if frame.seq == child.last_send_frame.seq:
            log.debug("correct ack number")

            if child.ack_lock.locked():  # the child wait an ack
                # unblock the tx process of the child by release his listen lock
                log.debug("child wait an ack")
                self.listen_lock.acquire()
                self.can_listen = False #can't listen because the child will send a frame
                self.listen_lock.release()
                child.ack_lock.release()
            else:
                log.debug("child don't wait an ack")

        elif frame.seq < child.last_send_frame.seq:  # child ask retransmission
            if child.retransmit_count <= MAX_RETRANSMIT:
                self.phy_layer.phy_send(child.last_send_frame)
            child.retransmit_count += 1
            if child.is_transmit():
                child.retransmit_event.set()

        else:
            log.warn("Incorrect SN. Expected: %d", child.last_send_frame.seq)
    """
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
        log.debug("Receive join frame %s", str(frame))

        if frame.seq != 0:
            log.warn("Incorrect SN. Expected: 0")
            return

        if child is not None:
            # it's a node that have already fully joined the network
            # i.e. has send a frame after the joining sequence
            # -> nothing to do
            log.warn("Node has already fully joined the network.")
            return

        # perhaps that it's a node that retransmits the JOIN frame
        child = self.not_joined_childs.get(frame.src_addr.prefix, None)

        if child is not None:  # it is a retransmission
            log.info("Child %s already exist i.e. ask retransmission.", str(child))

            if child.retransmit_count < MAX_RETRANSMIT:
                log.info("Retransmit JOIN_RESPONSE to %s.", child.last_send_frame.dest_addr)
                self.phy_layer.phy_send(child.last_send_frame)
                child.retransmit_count += 1
            else:  # we can no longer retransmit
                log.info("Max retransmission for JOIN reached -> remove child.")
                child.clear_transmit_count()
                r1 = self.not_joined_childs.pop(child.addr.node_id & 255, None)
                if r1 is None:
                    log.warning("child not in not_joined childs lits")
                r2 = self.childs.pop(child.addr.prefix, None)
                if r2 is None:
                    log.warning("child not in childs list")
                self.available_prefix.put_nowait(child.addr.prefix)
                #todo supprimer le fait que on met les préfixe dans une liste
            return

        new_prefix = None
        if self.next_prefix > MAX_PREFIX:
            # if all prefixes have been used, try to reuse a prefix
            if self.available_prefix.qsize() > 0:
                try:
                    new_prefix = self.available_prefix.get_nowait()
                except queue.Empty:
                    new_prefix = None
            if new_prefix is None:
                # can't accept new child
                log.warning("Can't accept new child")
                return
        else:
            new_prefix = self.next_prefix
            self.next_prefix += 1

        # create the child
        new_child = LoraChild(LoraAddr(new_prefix, frame.src_addr.node_id))
        self.childs[new_prefix] = new_child
        self.not_joined_childs[frame.src_addr.prefix] = new_child
        log.debug("new child %s created", str(new_child))

        # send the join response
        log.debug("send join response")
        self.phy_layer.phy_send(
            LoraFrame(
                self.addr,
                frame.src_addr,
                MacCommand.JOIN_RESPONSE,
                "%02X" % new_prefix,
                new_child.get_sn(),
                False,
            )
        )

    def _mac_rx(self, frame: LoraFrame):
        """Receive LoRaFrame frome the PHY layer.
            - checks that the destination address is correct
            - retrieves the child if exist
            - if seq = 1, mark de child as completly joined
            - if wait ack and frame is not ack: retransmit last packet if frame wait a response
            - calls the appropriate function to process the frame

        Args:
            frame (LoraFrame): The frame frome the PHY layer
        """
        log.info("MAC RX: %s", str(frame))

        # feature: use self.prefix instead of self.addr
        #          and use the id part of the LoRaAddr to map to an external Ipv6 addr
        if frame.dest_addr != self.addr:
            log.debug("frame not for me")
            return

        log.debug("look child with prefix: %d", frame.src_addr.prefix)
        child = self.childs.get(frame.src_addr.prefix, None)
        log.debug("child is: %s", str(child))
        if frame.seq == 1 and child is not None:
            # receive the first frame from this child
            # i.e. the join procedure is completed
            self.not_joined_childs.pop(child.addr.node_id & 255, None)

            # resets retransmit_count if there have been retransmissions
            child.clear_transmit_count()

        """#todo remove
        if (child is not None and child.ack_lock.locked() and frame.command != MacCommand.ACK):
            # the child wait an ack and this frame is not an ack
            # -> retransmit last frame
            if child.retransmit_count <= MAX_RETRANSMIT:
                log.info("Retransmit %s to %s", str(child.last_send_frame), str(child.addr))
                self.phy_layer.phy_send(child.last_send_frame)
            child.retransmit_count += 1
            if child.is_transmit():
                child.retransmit_event.set()
            return
        """

        fun = self.action_matcher.get(frame.command, None)
        if fun is not None:
            fun(frame, child)
        else:
            log.warning("Unknown MAC command.")

    def _rx_process(self):
        """Thread that fetches the frames received by the PHY layer.
            - Fetch a frame from the PHY layer
            - Update of if listen
            - If possible, listen
            - Send the frame to _mac_rx
        """
        while True:
            # the frame received by the PHY layer
            # this call block until a frame is available
            frame = self.phy_layer.getFrame()
            log.debug("RX process receive a frame")
            self.listen_lock.acquire()
            self.is_listen = False
            log.debug("RX process set is_listen=False")
            self.listen_lock.release()

            log.debug("RX process: call mac_rx")
            self._mac_rx(frame)
            log.debug("RX process: mac_rx end")

            self.listen_lock.acquire()
            log.debug("RX process acquire lock for can_listen")
            log.debug("can listen: %s", str(self.can_listen))
            log.debug("is_listen: %s", str(self.is_listen))
            if self.can_listen and not self.is_listen:
                self.phy_layer.phy_rx()
                self.is_listen = True
            self.listen_lock.release()
