from scapy.all import *
from scapy.layers.inet6 import IPv6

from py_lora_mac import NETWORK_STACK
import time
from threading import Thread, Lock


def main(max_iter=10):
    count = 0
    while count < max_iter:
        lock.acquire()

def on_packet():
    # print packet
    lock.release()
    pass

def init():
    NETWORK_STACK.init()
    NETWORK_STACK.register_listener(on_packet)
    lock.acquire()


    


if __name__ == "__main__":
    lock = Lock()