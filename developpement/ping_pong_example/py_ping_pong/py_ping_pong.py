###### import ######
import time
from threading import Thread, Lock
import sys
import logging.config
import os, os.path


###### Configuration ######
PY_LORAMAC_PATH = '../..'
UDP_CLIENT_PORT = 8765
UDP_SERVER_PORT = 5678

###### import py_lora_mac ######
sys.path.insert(1, PY_LORAMAC_PATH)
from py_lora_mac import *

###### logger configuration ######
if not os.path.exists("./logs/"):
    os.makedirs("./logs/")
logging.config.fileConfig("./logging.conf")


###### PING-PONG APP ######

dest_addr = None # destination address

def sender(max_iter=10):
    """ Thread that send PONG to the des_addr for max_iter times."""

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
    """IP layer listener: receive incomming IPv6 packets.

    Args:
        packet (IPv6): The incomming IPv6 packet.
    """
    print("Rceive IP packet: ")
    global dest_addr
    dest_addr = packet.src
    packet.show()
    lock.release()


def init():
    """Init the ping-pong app."""
    
    NETWORK_STACK.init()
    NETWORK_STACK.register_listener(on_packet)
    lock.acquire()
    t = Thread(target=sender)
    t.start()
    t.join()


if __name__ == "__main__":
    try:
        lock = Lock()
        init()
    except KeyboardInterrupt:
        print("\nGoodbye ...")
        os._exit(0)

