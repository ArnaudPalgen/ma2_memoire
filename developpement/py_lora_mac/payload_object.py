from typing import Any


class PayloadObject:
    """Base class of object that can be sent and received with LoRaMAC."""

    def serialize(self) -> str:
        """Serialize the object so that it can be sent by radio via the LoRaMAC protocol.

        Raises:
            NotImplementedError: If method is not implemented.

        Returns:
            str: The object serialized
        """
        raise NotImplementedError

    @staticmethod
    def deserialize(data: str) -> Any:
        """Deserialize the object so that it can be used.

        Raises:
            NotImplementedError: If method is not implemented.

        Returns:
            Any: The Object built.
        """
        raise NotImplementedError


class StrPayload(PayloadObject):
    """Example PayloadObject.

    Simple example of PayloadObject with str.
    """

    def __init__(self, data: str):
        self.data = data

    def serialize(self):
        result = ""
        for character in self.data:
            result += "%02X" % ord(character)
        return result

    @staticmethod
    def deserialize(data: str):
        result = ""
        i = 0
        while i < data:
            char_hex = data[i, i + 2]
            new_char = chr(int(char_hex, 16))
            result += new_char
            i += 2
        return StrPayload(result)


class People(StrPayload):
    """Another example PayloadObject."""

    def __init__(self, firstname, lastname):
        super().__init__(firstname + lastname)
        self.firstname = firstname
        self.lastname = lastname

    def sayHello(self):
        print("hello " + self.firstname + " " + self.lastname)
