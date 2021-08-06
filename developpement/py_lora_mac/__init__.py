from py_lora_mac.lora_mac import *
from py_lora_mac.lora_ip import *


class NetworkStack:
    def __init__(self):
        self.phy = LoraPhy()
        self.mac = LoraMac(self.phy)
        self.ip = LoraIP(self.mac)
        self.send_to = self.ip.send
        self.register_listener = self.ip.register_listener

    def init(self):
        self.ip.init()

NETWORK_STACK = NetworkStack()

#print(__name__)
# import logging
# logger = logging.getLogger("LoRa ROOT."+__name__)
