#from py_lora_mac.lora_mac import *
from pyloramac.lora_mac import *
from ipaddress import IPv6Address, AddressValueError
from scapy.all import *

from loguru import logger as log


# Unique Local IPv6 Unicast Addresses (FC00::/7) with to L bit to 1 (c.f. RFC 4193)
IPv6_PREFIX = "FD00"

COMMON_LINK_ADDR_PART = "0212:4B00:060D"


class LoraIP:
    """Network layer for the LoRaMac protocol.

    This class contains also functions to convert Ipv6 addr <-> LoraAddr.
    The conversion use the following format:

    |<-----1----->|<--6-->|<-----1----->|<----------6---------->|<---2--->|
    | IPv6 PREFIX | ZEROS | LORA PREFIX | COMMON_LINK_ADDR_PART | NODE_ID |
     0           0 1     6 7           7 8                    13 14     15

    Attributes:
        mac_layer: The MAC layer to use
        upper_layer: The Callable used to send incoming packet to the upper layer

    """

    def __init__(self, mac_layer: LoraMac):
        self.mac_layer = mac_layer
        self.upper_layer = None

    def init(self):
        """Init the IP layer.
            - Init the MAC layer
            - Register as listener to the MAC layer
        """

        log.info("Init IP")
        self.mac_layer.init()
        self.mac_layer.register_listener(self._on_frame)

    def _on_frame(self, src: LoraAddr, payload: str):
        """Process a frame from the MAC layer and deliver it to the upper layer.

        Args:
            src (LoraAddr): The source address of the frame.
            payload (str): The data of the frame.
        """

        log.info("IP RX: {} from: {}", payload, str(src))
        if self.upper_layer is None:
            log.warning("Upper layer not defined. Please call `register_listener` before.")
        else:
            ip_packet = self.build_ip_packet(payload, src, self.mac_layer.addr)
            self.upper_layer(ip_packet)

    def register_listener(self, listener: Callable[[IPv6], None]):
        """Register the listener for the upper layer.

        Args:
            listener (Callable[[IPv6], None]): The listener.
        """

        log.debug("listener registered !")
        self.upper_layer = listener

    def send(self, ip_packet: IPv6):
        """Send the IPv6 packet.
                - Prepare to IPv6 packet for the MAC layer.
                - Send it to the MAC layer.
        Args:
            ip_packet (IPv6): The packet to send
        """

        payload, _, dest_addr = self.serialize_ip_packet(ip_packet)
        log.info(f"IP TX {ip_packet[UDP][Raw].load.decode()} to {dest_addr}")
        self.mac_layer.mac_send(dest=dest_addr, payload=payload)

    @staticmethod
    def lora_to_ipv6(addr: LoraAddr) -> IPv6Address:
        """Convert a LoRaMAC address to an IPv6 address.

        Args:
            addr (LoraAddr): The address to convert.

        Returns:
            IPv6Address: The converted address.
        """

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
        """Convert an IPv6 address to a LoRaMAC address.

        Args:
            addr (IPv6Address): The address to convert.

        Returns:
            LoraAddr: The converted address.
        """

        addr_binary = addr.packed
        prefix = addr_binary[7]
        node_id = (addr_binary[14] << 8) + addr_binary[15]
        result = LoraAddr(prefix, node_id)
        log.debug(f"Ipv6 addr: {addr.exploded} converted to LoraAddr: {result}")
        return result

    @staticmethod
    def serialize_ip_packet(ip_packet: IPv6)->Tuple[str, LoraAddr, LoraAddr]:
        """Serialize an IPv6 packet that will be sent to the LoRaMAC layer.

        Args:
            ip_packet (IPv6): The IPv6 packet to serialize

        Returns:
            tuple: A tuple containing the payload (str) and the source and
                   destination LoraAddr.

        """

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
    def build_ip_packet(hex_data: str, src_addr: LoraAddr, dest_addr: LoraAddr)->IPv6:
        """Build an IPv6 packet from data extracted from a LoRaMAC frame.

        Args:
            hex_data (str): The payload of the LoRaMAC frame.
            src_addr (LoraAddr): The LoRaMAC source address.
            dest_addr (LoraAddr): The LoRaMAC destination address.

        Returns:
            IPv6: The IPv6 packet built.
        """

        """
        Split the data in two parts:
            - first_part: The first part of 8 bytes from the IPv6 header that contains the VER, TC, FL, LEN, NH and HL fields.
            - second_part: The rest of the IPv6 packet after the src and dest address (that are not carried in a loramac frame).
        """
        first_part = hex_data[0:16]
        second_part = hex_data[16:]

        """Create src and dest IPv6 addr from the LoRaMAC addresses"""
        ip_src_addr = LoraIP.lora_to_ipv6(src_addr).packed.hex().upper()
        ip_dest_addr = LoraIP.lora_to_ipv6(dest_addr).packed.hex().upper()

        """Construct and return the IPv6 packet built from all the previous data"""
        raw_data = bytes.fromhex(first_part + ip_src_addr + ip_dest_addr + second_part)
        result = IPv6(raw_data)
        return result
