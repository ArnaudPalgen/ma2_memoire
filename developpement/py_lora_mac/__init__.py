from py_lora_mac.lora_ip import *


class NetworkStack:
    def __init__(self):
        self.phy = LoraPhy()
        self.mac = LoraMac(self.phy)
        self.ip = LoraIP(self.mac)
        self.send = self.ip.send
        self.register_listener = self.ip.register_listener
        self.node_lr_addr = self.mac.addr
        self.node_ip_addr = self.ip.lora_to_ipv6(self.node_lr_addr)

    def init(self):
        self.ip.init()

NETWORK_STACK = NetworkStack()

#print(__name__)
# import logging
# logger = logging.getLogger("LoRa ROOT."+__name__)
