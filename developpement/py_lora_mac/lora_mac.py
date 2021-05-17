from lora_phy import LoraPhy
from enum import Enum, unique

childs = []

phy_layer = LoraPhy()

def on_join():
    pass

def on_data():
    pass

def on_ack():
    pass

def on_ping():
    pass

def on_pong():
    pass

def on_query():
    pass


def on_frame(frame):
    print("MAC ! ", frame)

@unique
class Command(Enum):
    def __init__(self, value, fun):
        self._value_ = value
        self.fun = fun
    JOIN = (1, on_join)
    JOIN_RESPONSE = (2, None)
    DATA = (3, on_data)
    ACK = (4, on_ack)
    PING = (5, on_ping)
    PONG = (6, on_pong)
    QUERY = (7, on_query)
    #CHILD,
    #CHILD_RESPONSE



phy_layer.phy_register_listener(on_frame)
#phy_layer.init()

if __name__ == '__main__':
    print(Command.JOIN.fun)
    Command.JOIN.fun()