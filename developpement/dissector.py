import sys
from py_lora_mac import LoraFrame


if __name__ == "__main__":
    data = sys.argv[1]
    frame = LoraFrame.build(data)
    print(frame)