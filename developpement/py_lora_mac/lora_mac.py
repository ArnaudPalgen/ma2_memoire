import queue
from threading import Timer, Event, Thread
from lora_phy import LoraAddr, LoraFrame, LoraPhy, MacCommand
from dataclasses import dataclass
import logging as log


MAX_PREFIX = 0xFC  # 252
MIN_PREFIX = 2

ROOT_PREFIX = 1
ROOT_ID = 0

RETRANSMIT_TIMEOUT = 5  # sec
MAX_RETRANSMIT = 3
RX_TIME = 3000 # 3 sec

CHILD_TX_BUF_SIZE = 5
CHILD_QUEUE_SIZE = 10


class LoraChild:
    def __init__(self, addr: LoraAddr, retransmit_fun: callable):
        self.addr = addr
        self.ack = False
        self.retransmit_fun = retransmit_fun
        self.timer = Timer(RETRANSMIT_TIMEOUT, self.retransmit_fun, [self])
        self.last_send_frame = None
        self.transmit_count = 1
        self.can_send = True
        self.tx_buf = queue.Queue(CHILD_TX_BUF_SIZE)
        self.lost_packet = 0
        #self.tx_buf.put(LoraFrame(LoraAddr(ROOT_PREFIX, ROOT_ID), LoraAddr(2, 24859), MacCommand.DATA, "48656c6C6F", False, True, False))

    def restart_timer(self):
        self.timer = Timer(RETRANSMIT_TIMEOUT, self.retransmit_fun, [self])
        self.timer.start()


class LoraMac:
    def __init__(self):
        self.phy_layer = LoraPhy()
        self.childs = {}# prefix:child
        self.addr = LoraAddr(ROOT_PREFIX, ROOT_ID)
        self.action_matcher = {MacCommand.JOIN: self._on_join, MacCommand.ACK: self._on_ack, MacCommand.QUERY: self._on_query}
        self.last_prefix = -1
        self.childs_buf = queue.Queue(CHILD_QUEUE_SIZE)

    def mac_init(self):
        log.info('Init MAC')
        self.phy_layer.phy_register_listener(self._mac_rx)
        self.phy_layer.phy_init()
        self.phy_layer.phy_timeout(0)
        self.phy_layer.phy_rx()

        tx_thread = Thread(target=self._tx_process)
        tx_thread.start()

    def _retransmit_timeout(self, child):
        log.info("retransmit timeout from child %s", str(child))
        if child.transmit_count >= MAX_RETRANSMIT:
            if child.last_send_frame.command == MacCommand.JOIN_RESPONSE:
                r=self.childs.pop(child.addr.node_id & 255, None)
                if r is None:
                    log.warning("child not in list")
                return
            log.warning("unable to send frame %s", str(child.last_send_frame))
            child.lost_packet += 1
            log.info("lost frames:%d", child.lost_packet)
            child.ack = not child.ack
            child.last_send_frame = None
            child.transmit_count = 1
            child.can_send = True
            if child.last_send_frame.has_next:
                self.childs_buf.put(child)

        else:
            log.info("retransmit frame: %s", str(child.last_send_frame))
            child.transmit_count += 1
            self.phy_layer.phy_tx(child.last_send_frame)
            child.restart_timer()

    def _on_query(self, frame: LoraFrame):
        log.debug("In _on_query")
        child:LoraChild = self.childs.get(frame.src_addr.prefix, None)
        if child is None:
            log.warning("child not in list")
            return
        if child.tx_buf.empty():
            log.debug("child buffer empty -> send ack")
            self.phy_layer.phy_tx(LoraFrame(self.addr, frame.src_addr, MacCommand.ACK, "", child.ack, False, False))
            child.ack = not child.ack
        else:
            self.childs_buf.put(child)


    def _on_ack(self, frame: LoraFrame):
        """ get the childs from self.childs
            try already joined child first, after try not already joined child
            raise key error if child is not found
            &255 because initial prefix is unint8(node_id)
        """
        log.info("receive ack %s", str(frame))
        
        child = self.childs.get(frame.src_addr.prefix, self.childs.get(frame.src_addr.node_id & 255, None))
        
        if child is None:
            log.error("child %s don't exist", str(frame.src_addr))
        
        if frame.seq == child.ack:
            log.debug("correct ack number")
            if child.last_send_frame.command == MacCommand.JOIN_RESPONSE:
                log.debug("ack a JOIND_RESPONSE")
                """ ack for JOIN_RESPONSE -> update child.addr.prefix
                    and self.last_prefix
                """
                new_prefix = int(child.last_send_frame.payload, 16)
                child.addr = LoraAddr(new_prefix, frame.dest_addr.node_id)
                log.debug("update child with prefix %d", child.addr.prefix)
                self.childs[child.addr.prefix] = child
                del self.childs[frame.src_addr.node_id& 255]
                self.last_prefix = new_prefix
            
            child.ack = not child.ack
            child.timer.cancel()
            child.can_send = True

            self.phy_layer.phy_timeout(0)
            self.phy_layer.phy_rx()

            if child.last_send_frame.has_next:
                self.childs_buf.put(child)

    def _on_join(self, frame: LoraFrame):
        ch = self.childs.get(frame.src_addr.prefix)
        if ch is not None:
            log.info("Child %s already exist", str(ch))
            return
        
        log.info("reveive join frame %s", str(frame))
        if len(self.childs) == 0:
            new_prefix = MIN_PREFIX
        elif len(self.childs) > 0 and self.last_prefix < MAX_PREFIX:
            new_prefix = self.last_prefix + 1

        new_child = LoraChild(frame.src_addr, self._retransmit_timeout)

        self.childs[frame.src_addr.prefix] = new_child
        log.debug("new child:%s", str(frame.src_addr))

        log.debug("send join response")
        self.mac_tx(
            LoraFrame(self.addr, frame.src_addr, MacCommand.JOIN_RESPONSE, "%02X" % new_prefix, new_child.ack, True)
        )
        self.childs_buf.put(new_child)

    def mac_tx(self, frame: LoraFrame) -> bool:
        child = self.childs[frame.dest_addr.prefix]
        log.debug("in mac tx. FULL:%s", str(child.tx_buf.full()))
        if not child.tx_buf.full():
            child.tx_buf.put(frame, block=False)
            log.debug("put for frame to buf of %s", str(child))
            return True
        return False

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
                if child.tx_buf.qsize() > 0 and frame.command != MacCommand.JOIN_RESPONSE:
                    frame.has_next=True
                frame.seq = child.ack
                self.phy_layer.phy_tx(frame)
                log.info("send %s", str(frame))
                if frame.k or frame.command==MacCommand.JOIN_RESPONSE:
                    child.timer.start()
                    log.info("start retransmit timer")
                    child.can_send = False
                    if frame.has_next:
                        self.childs_buf.put(child)
                    self.phy_layer.phy_timeout(RX_TIME)
                    self.phy_layer.phy_rx()
                else:
                    child.ack = not child.ack

    def _mac_rx(self, frame: LoraFrame):
        log.debug("MAC RX: %s", str(frame))
        child = self.childs.get(frame.src_addr.prefix, None)
        log.debug("look child with prefix: %d", frame.src_addr.prefix)
        
        if frame.dest_addr == self.addr:
            if child is not None and frame.seq != child.ack:
                log.warning("incorrect SN -> retransmit last frame")
                self.phy_layer.phy_tx(child.last_send_frame)
            
            elif child is None and (frame.command != MacCommand.JOIN and frame.command !=MacCommand.ACK):
                log.warning("node %s wants to exchange without JOIN", str(frame.src_addr))
            else:
                fun = self.action_matcher.get(frame.command, None)
                if fun is not None:
                    fun(frame)
                else:
                    log.warning("unknown MAC command")


if __name__ == '__main__':
    mac = LoraMac()
    mac.mac_init()
    #mac.mac_tx(LoraFrame(LoraAddr(ROOT_PREFIX, ROOT_ID), LoraAddr(2, 24859), MacCommand.DATA, "48656c6C6F", False, True, False))
    # mac._mac_rx(LoraFrame.build("radio_rx  00611B01000000\r\n"))
