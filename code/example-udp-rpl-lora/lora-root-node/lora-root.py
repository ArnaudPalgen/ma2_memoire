###### import ######
from threading import Thread, Lock
from enum import Enum
import sys
from loguru import logger
from pyloramac import *
from ipaddress import IPv6Address, AddressValueError
from scapy.all import *
from time import *
###### LOG Configuration ######
LOG_FORMAT = "<green>("+str(perf_counter_ns())+"){time: HH:mm:ss.SSS}</green> | "+\
             "<level>{level: <8}</level> |"+\
             "{level.icon}  <cyan>{name}</cyan>"+\
             ":<cyan>{function}</cyan>:<cyan>{line}</cyan> "+\
             "- <level>{message}</level>"+\
             "\n{exception}"
LOG_CONFIG = {
    "handlers": [
        {"sink": sys.stdout, "format": lambda record: LOG_FORMAT, "level": "INFO"},
        #{"sink":"file_{time}.log", "format": lambda record: LOG_FORMAT, "level": "INFO"},
    ]
}
###### UDP Configuration ######
UDP_CLIENT_PORT = 8765
UDP_SERVER_PORT = 5678

SEND_INTERVAL = 5 # s
MAX_PAQUET_COUNT = 100
###### MAIN APP ######
class Mode(Enum):
    PINGPONG = 0
    RECEIVER = 1
    SENDER = 2

class Child:
    def __init__(self, mode: Mode, first_data: IPv6):
        self.mode = mode
        self.addr = first_data.src
        self.count = 0
        self.sender = None
        
        if mode == Mode.PINGPONG:
            self.receive(first_data)
        if mode == Mode.SENDER:
            self.sender = Thread(target=self.send)
            self.sender.start()

    def send(self):
        while self.count < MAX_PAQUET_COUNT:
            data = IPv6(src=NETWORK_STACK.node_ip_addr, dst=self.addr)/UDP(sport=UDP_SERVER_PORT, dport=UDP_CLIENT_PORT)/Raw(load=f"hello {self.count}")
            logger.log("APP", f"Send {data[UDP][Raw].load.decode()} to {data.dst}")
            NETWORK_STACK.send(data)
            sleep(SEND_INTERVAL)
            self.count += 1

    def receive(self, data):
        logger.log("APP", f"Receive {data[UDP][Raw].load.decode()} from {data.src}")
        new_data = IPv6(src=NETWORK_STACK.node_ip_addr, dst=self.addr)/UDP(sport=UDP_SERVER_PORT, dport=UDP_CLIENT_PORT)/Raw(load=f"PONG {self.count}")
        logger.log("APP", f"send {data[UDP][Raw].load.decode()} to {new_data.dst}")
        NETWORK_STACK.send(new_data)
        self.count +=1

class LoRaRoot:
    def __init__(self, mode:Mode):
        self.childs = {}
        self.mode = mode

    def init(self, port:str):
        NETWORK_STACK.init(port=port)
        NETWORK_STACK.register_listener(self.on_data)

    def on_data(self, data: IPv6):
        if mode == Mode.RECEIVER:
            logger.log("APP", f"Receive {data[UDP][Raw].load.decode()} from {data.src}")
        else:
            child = self.childs.get(data.src, None)
            if child is None:
                child = Child(self.mode, data)
                self.childs[data.src] = child
            elif self.mode == Mode.PINGPONG:
                child.receive(data)

if __name__ == "__main__":
    logger.configure(**LOG_CONFIG)
    logger.enable("pyloramac")
    logger.level("APP", no=26, color="<fg #00ABCC><i>", icon="\U0001F3D3")

    if len(sys.argv) != 3:
        logger.error("Usage: lora-root.py <port> <mode>")
        sys.exit(1)
    
    port = sys.argv[1]
    mode = Mode(int(sys.argv[2]))
    logger.log("APP", f"Mode: {mode}")

    try:    
        LoRaRoot(mode).init(port)
    except KeyboardInterrupt:
        logger.info("KeyboardInterrupt")
        logger.info("Exiting...")
        sys.exit(0)
