import time
from threading import Thread, Lock

import sys
sys.path.insert(1, '..')

from py_lora_mac import *

dest_addr = None

UDP_CLIENT_PORT = 8765
UDP_SERVER_PORT = 5678


def sender(max_iter=10):
    count = 0
    while count < max_iter:
        print("wait PING packet...")
        lock.acquire()

        # Build IPv6 response
        packet = (
            IPv6(src=NETWORK_STACK.node_ip_addr, dst=dest_addr)
            / UDP(sport=UDP_SERVER_PORT, dport=UDP_CLIENT_PORT)
            / Raw(load="PONG "+str(count))
        )

        """
            Allows scapy to add and compute automatically the
            len and chksum fields of the UDP packet
        """
        packet = IPv6(raw(packet))
        print("send PONG to "+dest_addr)
        NETWORK_STACK.send(packet)
        count += 1


def on_packet(packet: IPv6):
    print("Rceive IP packet: ")
    global dest_addr
    dest_addr = packet.src
    packet.show()
    lock.release()


def init():
    NETWORK_STACK.init()
    NETWORK_STACK.register_listener(on_packet)
    lock.acquire()
    t = Thread(target=sender)
    t.start()
    t.join()


if __name__ == "__main__":
    lock = Lock()
    init()
