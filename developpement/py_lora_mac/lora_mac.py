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

    def restart_timer(self):
        self.timer = Timer(RETRANSMIT_TIMEOUT, self.retransmit_fun, [self])
        self.timer.start()


class LoraMac:
    def __init__(self):
        self.phy_layer = LoraPhy()
        self.childs = {}# prefix:child
        self.addr = LoraAddr(ROOT_PREFIX, ROOT_ID)
        self.action_matcher = {MacCommand.JOIN: self._on_join, MacCommand.ACK: self._on_ack}
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
            # TODO si cela concerne une JOIN_RESPONSE, supprimer le child de self.childs
            log.warning("unable to send frame %s", str(child.last_send_frame))
            child.lost_packet += 1
            log.info("lost frames:%d", child.lost_packet)
            child.ack = not child.ack
            child.last_send_frame = None
            child.transmit_count = 1
            child.can_send = True
            self.childs_buf.put(child)

        else:
            log.info("retransmit frame: %s", str(child.last_send_frame))
            child.transmit_count += 1
            self.phy_layer.phy_tx(child.last_send_frame)
            child.restart_timer()

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
                self.childs[child.addr.prefix] = child
                del self.childs[frame.src_addr.node_id& 255]
                self.last_prefix = new_prefix
            
            child.ack = not child.ack
            child.timer.cancel()
            child.can_send = True
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

    def _on_data(self, frame):
        pass#TODO

    def mac_tx(self, frame: LoraFrame) -> bool:
        child = self.childs[frame.dest_addr.prefix]
        log.debug("in mac tx. FULL:%s", str(child.tx_buf.full()))
        if not child.tx_buf.full():
            child.tx_buf.put(frame, block=False)
            log.debug("put for frame to buf of %s", str(child))
            if child.can_send:
                self.childs_buf.put(child)
            return True
        return False

    def _tx_process(self):
        while True:
            child = self.childs_buf.get()
            while child.can_send and child.tx_buf.qsize() > 0:
                try:
                    frame = child.tx_buf.get(block=False)
                except queue.Empty:
                    break
                child.last_send_frame = frame
                if child.tx_buf.qsize() > 0:
                    frame.has_next=True
                self.phy_layer.phy_tx(frame)
                log.info("send %s", str(frame))
                if frame.k:
                    child.timer.start()
                    log.info("start retransmit timer")
                    self.can_send = False
                self.phy_layer.phy_rx()

    def _mac_rx(self, frame: LoraFrame):
        log.debug("MAC RX: %s", str(frame))
        if frame.dest_addr == self.addr:
            fun = self.action_matcher.get(frame.command, None)
            if fun is not None:
                fun(frame)
            else:
                log.warning("unknown MAC command")


if __name__ == '__main__':
    mac = LoraMac()
    mac.mac_init()
    # mac._mac_rx(LoraFrame.build("radio_rx  00611B01000000\r\n"))
