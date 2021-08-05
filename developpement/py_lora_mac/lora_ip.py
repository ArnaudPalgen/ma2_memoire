from ipaddress import IPv6Address, AddressValueError
from py_lora_mac.lora_phy import LoraAddr
from py_lora_mac.lora_mac import LoraMac
from typing import Union, Callable, Type
import logging
from scapy.all import *
from scapy.layers.inet6 import IPv6

log = logging.getLogger("LoRa_ROOT.IP")


# Unique Local IPv6 Unicast Addresses (FC00::/7) with to L bit to 1 (c.f. RFC 4193)
IPv6_PREFIX = "FD00"
COMMON_LINK_ADDR_PART = "02124B00060D"


class LoraIP:
    """Network layer for the LoRaMac protocol.

    This class contains also two functions to convert Ipv6 <-> LoraAddr.
    The conversion use the following format:

    |<-----1----->|<--5-->|<-----1----->|<--1-->|<----------6---------->|<---2--->|
    | IPv6 PREFIX | ZEROS | LORA PREFIX | ZEROS | COMMON_LINK_ADDR_PART | NODE_ID |
     0           0 1     5 6           6 7     7 8                    13 14     15

    Attributes:
        mac_layer: The mac mayer to use
        upper_layer: The Callable used to send incoming packet to the upper layer

    """

    def __init__(self, mac_layer: LoraMac):
        self.mac_layer = mac_layer
        self.upper_layer = None

    def init(self):
        log.info("Init IP layer")
        self.mac_layer.init()
        self.mac_layer.register_listener(self._on_frame)

    def _on_frame(self, src: LoraAddr, payload: str):
        log.debug("Incomming frame from LoRaMAC")
        if self.upper_layer is None:
            log.error("Upper layer not defined. Please call `register_listener` before")
        else:
            self.upper_layer(self.build_ip_packet(payload, src, self.mac_layer.addr))

    def register_listener(self, listener: Callable[[IPv6], None]):
        log.debug("listener registered !")
        self.upper_layer = listener

    def send(self, ip_packet: IPv6):
        log.debug("Send %s", ip_packet.show())
        payload, _, dest_addr = self.serialize_ip_packet(ip_packet)
        log.debug("Send to RPL ROOT: " + str(dest_addr))
        self.mac_layer.mac_send(dest=dest_addr, payload=payload, k=False)

    @staticmethod
    def lora_to_ipv6(addr: LoraAddr) -> IPv6Address:
        result = IPv6Address(
            IPv6_PREFIX
            + "::"
            + ("%02X:" % addr.prefix)
            + COMMON_LINK_ADDR_PART
            + ":"
            + ("%04x" % addr.node_id)
        )
        log.debug("LoraAddr: " + str(addr) + " converted to IPv6: " + result.exploded)
        return result

    @staticmethod
    def ipv6_to_lora(addr: IPv6Address) -> LoraAddr:
        addr_binary = addr.packed
        prefix = addr_binary[6]
        node_id = (addr_binary[14] << 8) + addr_binary[15]
        result = LoraAddr(prefix, node_id)
        log.debug(
            "Ipv6 addr: " + addr.exploded + " converted to LoraAddr: " + str(addr)
        )
        return result

    @staticmethod
    def serialize_ip_packet(ip_packet: IPv6):
        hex_packet = (
            chexdump(ip_packet, dump=True)
            .replace("0x", "")
            .replace(",", "")
            .replace(" ", "")
            .upper()
        )

        """remove adresses from the packet"""
        f1 = hex_packet[0:16]
        f2 = hex_packet[80:]
        result = f1 + f2

        src_addr = IPv6Address(ip_packet.dest).packed.hex().upper()
        dest_addr = IPv6Address(ip_packet.dest).packed.hex().upper()
        return (result, src_addr, dest_addr)

    @staticmethod
    def build_ip_packet(hex_data: str, src_addr: LoraAddr, dest_addr: LoraAddr):
        """
        Split the data in two parts:
            - first_part: The first part of 8 bytes from the IPv6 header that contains: VER, TC, FL, LEN, NH, HL
            - second_part: The rest of the IPv6 packet after the src and dest address (that are not carried in a loramac frame)
        """
        first_part = hex_data[0:16]
        second_part = hex_data[80:]

        """Create src and dest IPv6 addr from the LoRaMAC addresses"""
        ip_src_addr = LoraIP.lora_to_ipv6(src_addr).packed.hex().upper()
        ip_dest_addr = LoraIP.lora_to_ipv6(dest_addr).packed.hex().upper()

        """Construct and return the IPv6 packet built from all the previous data"""
        raw_data = bytes.fromhex(first_part + ip_src_addr + ip_dest_addr + second_part)
        result = IPv6(raw_data)
        return result
