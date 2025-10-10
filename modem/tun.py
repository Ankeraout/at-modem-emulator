import fcntl
import os
import struct
from modem.protocols.protocol import Protocol
from modem.ip_router import IPRouter

TUNSETIFF = 0x400454ca
IFF_TUN = 0x0001
ETHERTYPE_IPV4 = 0x0800
ETHERTYPE_IPV6 = 0x86dd

class Tun(Protocol):
    def __init__(self, interface: str, ip_router: IPRouter) -> None:
        super().__init__()
        self._tun = os.open("/dev/net/tun", os.O_RDWR)
        self._ip_router = ip_router
    
        if self._tun < 0:
            raise Exception("Failed to open /dev/net/tun.")
        
        ifr = struct.pack("16sH", interface.encode(), IFF_TUN)
        fcntl.ioctl(self._tun, TUNSETIFF, ifr)

    def run(self) -> None:
        while True:
            buffer = os.read(self._tun, 1504)
            ethertype = struct.unpack(">H", buffer[2:4])[0]

            if ethertype == ETHERTYPE_IPV4:
                self._ip_router.receive(buffer[4:])

    def receive(self, buffer: bytes) -> None:
        pass

    def send(self, buffer: bytes, upper_protocol: Protocol = None) -> None:
        os.write(self._tun, b"\x00\x00\x08\x00" + buffer)
