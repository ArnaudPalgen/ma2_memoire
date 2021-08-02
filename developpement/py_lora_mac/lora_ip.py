from ipaddress import IPv6Address
from py_lora_mac.lora_phy import PayloadObject, StrPayload, LoraAddr
from py_lora_mac.lora_mac import LoraMac
from typing import Union, Callable, Type
import logging

log = logging.getLogger("LoRa_ROOT.IP")


# Unique Local IPv6 Unicast Addresses (FC00::/7) with to L bit to 1 (c.f. RFC 4193)
IPv6_PREFIX = "FD00"
COMMON_LINK_ADDR_PART = "5F756D6F6E73"


class LoraIP:
    """Network layer for the LoRaMac protocol.

    This class contains also two functions to convert Ipv6 <-> LoraAddr.
    The conversion use the following format:

    |<-----1----->|<--5-->|<-----1----->|<--1-->|<----------6---------->|<---2--->|
    | IPv6 PREFIX | ZEROS | LORA PREFIX | ZEROS | COMMON_LINK_ADDR_PART | NODE_ID |
     0           0 1     5 6           6 7     7 8                    13 14     15

    Attributes:
        mac_layer: The mac mayer to use
        listener: The Callable used for incoming packet

    """

    def __init__(self, mac_layer: LoraMac, payloadType: Type[PayloadObject]):
        self.mac_layer = mac_layer
        self.listener = None
        self.payload_type = payloadType

    def init(self):
        log.info("Init IP layer")
        self.mac_layer.init()
        self.mac_layer.register_listener(self._on_frame)

    def _on_frame(self, src: LoraAddr, payload: str):
        log.debug("Incomming frame")
        if self.listener is None:
            log.error("Listener not defined. Please call `register_listener` before")
        else:
            self.listener(self.lora_to_ipv6(src), self.payload_type(payload))

    def send_to(self, dest_addr: IPv6Address, payload: Union[str, PayloadObject]):
        log.debug("Send " + str(payload) + " to " + dest_addr.exploded)
        if type(payload) == str:
            payload = StrPayload(payload)
        self.mac_layer.mac_send(
            dest=self.ipv6_to_lora(dest_addr), payload=payload, k=False
        )

    def register_listener(self, listener: Callable[[IPv6Address, PayloadObject], None]):
        log.debug("listener registered !")
        self.listener = listener

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
