import queue
from threading import Timer, Event, Thread
from lora_phy import LoraAddr, LoraFrame, LoraPhy, MacCommand
from dataclasses import dataclass


MAX_PREFIX = 0xFC#252
MIN_PREFIX = 2

ROOT_PREFIX = 1
ROOT_ID = 0

RETRANSMIT_TIMEOUT = 5#sec
MAX_RETRANSMIT = 3

CHILD_TX_BUF_SIZE = 5
CHILD_QUEUE_SIZE = 10


class LoraChild():
    def __init__(self, addr:LoraAddr, retransmit_fun:callable):
        self.addr = addr
        self.ack = False
        self.timer = Timer(RETRANSMIT_TIMEOUT, retransmit_fun, [self])
        self.last_send_frame = None
        self.transmit_count = 0
        self.can_send = True
        self.tx_buf = queue.Queue(CHILD_TX_BUF_SIZE)
        self.lost_packet = 0

class LoraMac():
    def __init__(self):
        self.phy_layer = LoraPhy()
        self.childs = {}
        self.addr = LoraAddr(ROOT_PREFIX,ROOT_ID)
        self.action_matcher = {MacCommand.JOIN:self._on_join, MacCommand.ACK:self._on_ack}
        self.last_prefix = -1
        self.childs_buf=queue.Queue(CHILD_QUEUE_SIZE)

    def mac_init(self):
        self.phy_layer.phy_register_listener(self._mac_rx)
        self.phy_layer.phy_init()
        self.phy_layer.phy_timeout(0)
        self.phy_layer.phy_rx()

        tx_thread = Thread(target=self._tx_process)
        tx_thread.start()


    def _retransmit_timeout(self, child):
        print("retransmit timeout:", child)
        if child.transmit_count >= MAX_RETRANSMIT:
            print("unable to send frame", child.last_send_frame)
            child.lost_packet +=1
            print("lost packets:" ,child.lost_packet)
            child.ack = not child.ack
            child.last_send_frame = None
            child.transmit_count = 0
            child.can_send = True
            self.childs_buf.put(child)

        else:
            child.transmit_count +=1
            self.phy_layer.phy_tx(child.last_send_frame)
            #child.timer.start()
    
    def _on_ack(self, frame:LoraFrame):
        oldAddr = LoraAddr(0,frame.src_addr.node_id)
        child = self.childs[oldAddr]
        if frame.seq == child.ack:
            if child.last_send_frame.command==MacCommand.JOIN_RESPONSE:
                child.addr=LoraAddr(int(child.last_send_frame.payload, 16), frame.dest_addr.node_id)
                self.childs[child.addr] = child
                del self.childs[oldAddr]
            child.ack = not child.ack
            child.timer.cancel()
            child.can_send = True
            self.childs_buf.put(child)

    def _on_join(self, frame:LoraFrame):
        print("enter on_join")
        if len(self.childs) == 0:
            new_prefix = MIN_PREFIX
        elif len(self.childs) > 0 and self.last_prefix < MAX_PREFIX:
            new_prefix = self.last_prefix+1
        
        new_child_addr = LoraAddr(0, frame.src_addr.node_id)
        new_child = LoraChild(new_child_addr, self._retransmit_timeout)
        
        self.childs[new_child_addr] = new_child
        print("new child:", new_child_addr)
        
        print("send join response")
        self.mac_tx(LoraFrame(self.addr, frame.src_addr, True, MacCommand.JOIN_RESPONSE, "%02X"%new_prefix))#todo ack?

    def mac_tx(self, frame: LoraFrame)->bool:
        child = self.childs[frame.dest_addr]
        print("in mac tx. FULL:", child.tx_buf.full())
        if not child.tx_buf.full():
            child.tx_buf.put(frame, block=False)
            print("putted for :", child, child.tx_buf)
            if child.can_send:
                self.childs_buf.put(child)
            return True
        return False
    
    def _tx_process(self):
        while True:
            child = self.childs_buf.get()
            while child.can_send and child.tx_buf.qsize()>0:
                try:
                    frame = child.tx_buf.get(block=False)
                except queue.Empty:
                    break
                child.last_send_frame = frame
                self.phy_layer.phy_tx(frame)
                if frame.k:
                    child.timer.start()
                    self.can_send=False

    def _mac_rx(self, frame:LoraFrame):
        print("MAC RX:", frame)
        if frame.dest_addr == self.addr:
            fun = self.action_matcher.get(frame.command, None)
            if fun is not None:
                fun(frame)
            else:
                print("unknown mac command")


if __name__ == '__main__':
    mac = LoraMac()
    mac.mac_init()
    #mac._mac_rx(LoraFrame.build("radio_rx  00611B01000000\r\n"))