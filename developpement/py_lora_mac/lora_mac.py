import queue
import threading
from lora_phy import LoraAddr, LoraFrame, LoraPhy, MacCommand

MAX_PREFIX = 0xFC#252
MIN_PREFIX = 2

ROOT_PREFIX = 1
ROOT_ID = 0

RETRANSMIT_TIMEOUT = 5#sec


class LoraMac():
    def __init__(self):
        self.phy_layer = LoraPhy()
        self.childs = []
        self.addr = LoraAddr(ROOT_PREFIX,ROOT_ID)
        self.action_matcher = {MacCommand.JOIN:self._on_join, MacCommand.ACK:self._on_ack}
        self.tx_buf = queue.Queue(10)
        self.retransmit_timer = threading.Timer(RETRANSMIT_TIMEOUT, self._retransmit_timeout)
        self.last_send_frame = None
        self.can_send = True
        self.expected_ack = False
        self.can_send_cond = threading.Condition()

    def mac_init(self):
        self.phy_layer.phy_register_listener(self._mac_rx)
        self.phy_layer.phy_init()
        self.phy_layer.phy_timeout(0)
        self.phy_layer.phy_rx()

        tx_thread = threading.Thread(target=self._tx_process)
        tx_thread.start()

        with self.can_send_cond:
            self.can_send_cond.notify_all()

    def _retransmit_timeout(self):
        print("retransmit timeout")
    
    def _on_ack(self, frame:LoraFrame):
        if frame.payload == self.expected_ack:
            with self.can_send_cond:
                self.can_send = True
                self.can_send_cond.notify_all()

    def _on_join(self, frame:LoraFrame):
        if len(self.childs) == 0:
            new_child = MIN_PREFIX
        elif len(self.childs) > 0 and self.childs[-1] < MAX_PREFIX:
            new_child = self.childs[-1]+1
        
        self.childs.append(LoraAddr(new_child, frame.src_addr.node_id))
        
        self.mac_tx(LoraFrame(self.addr, frame.src_addr, True, MacCommand.JOIN_RESPONSE, "%02X"%new_child))

    def mac_tx(self, frame: LoraFrame)->bool:
        if not self.tx_buf.full:
            self.tx_buf.put(frame, block=False)
            return True
        return False

    
    def _tx_process(self):
        while True:
            while not self.can_send:
                while self.can_send_cond:
                    self.can_send_cond.wait()
        self.last_send_frame = self.tx_buf.get(block=True)
        self.phy_layer.phy_tx(self.last_send_frame)
        if self.last_send_frame.k:
            self.retransmit_timer.start()
            self.expected_ack = not self.expected_ack
            self.can_send = False

    def _mac_rx(self, frame:LoraFrame):
        print("MAC ! ", frame)
        if frame.dest_addr == self.addr:
            fun = self.action_matcher.get(frame.command, None)
            if fun is not None:
                fun(frame)
            else:
                print("unknown mac command")


if __name__ == '__main__':
    mac = LoraMac()
    mac.mac_init()