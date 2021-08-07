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
COMMON_LINK_ADDR_PART = "0212:4B00:060D"


class LoraIP:
    """Network layer for the LoRaMac protocol.

    This class contains also two functions to convert Ipv6 <-> LoraAddr.
    The conversion use the following format:

    |<-----1----->|<--6-->|<-----1----->|<----------6---------->|<---2--->|
    | IPv6 PREFIX | ZEROS | LORA PREFIX | COMMON_LINK_ADDR_PART | NODE_ID |
     0           0 1     6 7           7 8                    13 14     15

    Attributes:
        mac_layer: The mac mayer to use
        upper_layer: The Callable used to send incoming packet to the upper layer

    """

    def __init__(self, mac_layer: LoraMac):#done
        self.mac_layer = mac_layer
        self.upper_layer = None

    def init(self):#done
        log.info("Init IP layer")
        self.mac_layer.init()
        self.mac_layer.register_listener(self._on_frame)

    def _on_frame(self, src: LoraAddr, payload: str):#done
        log.debug("Incomming frame from LoRaMAC")
        if self.upper_layer is None:
            log.warning("Upper layer not defined. Please call `register_listener` before")
        else:
            ip_packet = self.build_ip_packet(payload, src, self.mac_layer.addr)
            packet_info = ip_packet.show(dump=True)
            log.debug("Rebuilt IP packet: \n%s\n", packet_info)
            self.upper_layer(ip_packet)

    def register_listener(self, listener: Callable[[IPv6], None]):#done
        log.debug("listener registered !")
        self.upper_layer = listener

    def send(self, ip_packet: IPv6):#review
        log.debug("Send %s", ip_packet.show())
        payload, _, dest_addr = self.serialize_ip_packet(ip_packet)
        log.debug("Send to RPL ROOT: " + str(dest_addr))
        self.mac_layer.mac_send(dest=dest_addr, payload=payload, k=False)

    @staticmethod
    def lora_to_ipv6(addr: LoraAddr) -> IPv6Address:#DONE
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
    def ipv6_to_lora(addr: IPv6Address) -> LoraAddr:#DONE
        addr_binary = addr.packed
        prefix = addr_binary[7]
        node_id = (addr_binary[14] << 8) + addr_binary[15]
        result = LoraAddr(prefix, node_id)
        log.debug(
            "Ipv6 addr: " + addr.exploded + " converted to LoraAddr: " + str(addr)
        )
        return result

    @staticmethod
    def serialize_ip_packet(ip_packet: IPv6):#review
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
        payload = f1 + f2

        src_addr = LoraIP.ipv6_to_lora(IPv6Address(ip_packet.src))
        dest_addr = LoraIP.ipv6_to_lora(IPv6Address(ip_packet.dst))
        return (payload, src_addr, dest_addr)

    @staticmethod
    def build_ip_packet(hex_data: str, src_addr: LoraAddr, dest_addr: LoraAddr):#done
        log.debug("enter build ip packet with hex_data:%s | lora_src: %s | lora_dest: %s", hex_data, str(src_addr), str(dest_addr))
        """
        Split the data in two parts:
            - first_part: The first part of 8 bytes from the IPv6 header that contains: VER, TC, FL, LEN, NH, HL
            - second_part: The rest of the IPv6 packet after the src and dest address (that are not carried in a loramac frame)
        """
        first_part = hex_data[0:16]
        second_part = hex_data[16:]
        log.debug("first part: %s", first_part)
        log.debug("second part: %s", second_part)

        """Create src and dest IPv6 addr from the LoRaMAC addresses"""
        ip_src_addr = LoraIP.lora_to_ipv6(src_addr).packed.hex().upper()
        ip_dest_addr = LoraIP.lora_to_ipv6(dest_addr).packed.hex().upper()

        """Construct and return the IPv6 packet built from all the previous data"""
        raw_data = bytes.fromhex(first_part + ip_src_addr + ip_dest_addr + second_part)
        log.debug("hex data for ip packet:%s", raw_data)
        result = IPv6(raw_data)
        return result
