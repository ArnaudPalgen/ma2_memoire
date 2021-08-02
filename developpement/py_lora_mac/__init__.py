from py_lora_mac.lora_mac import *
from py_lora_mac.lora_ip import *
from py_lora_mac.payload_object import StrPayload


class NetworkStack:
    def __init__(self):
        self.phy = LoraPhy()
        self.mac = LoraMac(self.phy)
        self.ip = LoraIP(self.mac, StrPayload)
        self.send_to = self.ip.send_to
        self.register_listener = self.ip.register_listener

    def init(self):
        self.ip.init()

network_stack = NetworkStack()

#print(__name__)
# import logging
# logger = logging.getLogger("LoRa ROOT."+__name__)
