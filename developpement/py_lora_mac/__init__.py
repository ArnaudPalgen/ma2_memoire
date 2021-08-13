from py_lora_mac.lora_ip import *
from py_lora_mac.lora_mac import *


class NetworkStack:
    def __init__(self):
        self.phy = LoraPhy(txBufSize=10, rxBufSize=10, port="/dev/ttyUSB0", baudrate="57600", frequence="868100000", bandwidth="125", cr="4/5", pwr="1", sf="sf12")
        self.mac = LoraMac(self.phy)
        self.ip = LoraIP(self.mac)
        self.node_lr_addr = self.mac.addr
        self.node_ip_addr = self.ip.lora_to_ipv6(self.node_lr_addr)
        self.send = self.ip.send
        self.register_listener = self.ip.register_listener
        self.init = self.ip.init
        if ENABLE_STAT:
            self.phy_meter = PhyMeter.getMeter()


NETWORK_STACK = NetworkStack()
