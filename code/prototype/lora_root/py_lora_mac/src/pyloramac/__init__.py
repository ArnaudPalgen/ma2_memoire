from pyloramac.lora_ip import LoraIP
from pyloramac.lora_mac import LoraMac
from pyloramac.lora_phy import LoraPhy
from loguru import logger


class NetworkStack:
    def __init__(self):
        self.phy = None
        self.mac = None
        self.ip = None
        self.node_lr_addr = None
        self.node_ip_addr = None
        self.send = None
        self.register_listener = None
    
    def init(self, txBufSize=10, rxBufSize=10, port="/dev/ttyUSB0",
             baudrate="57600", frequence="868100000", bandwidth="125", cr="4/5",
             pwr="1", sf="sf10"):
        
        params = locals()
        params.pop('self')
        self.phy = LoraPhy(listen_on_error=True, **params)
        self.mac = LoraMac(self.phy)
        self.ip = LoraIP(self.mac)
        self.node_lr_addr = self.mac.addr
        self.node_ip_addr = self.ip.lora_to_ipv6(self.node_lr_addr)
        self.send = self.ip.send
        self.register_listener = self.ip.register_listener
        self.ip.init()


logger.disable("pyloramac")
NETWORK_STACK = NetworkStack()
