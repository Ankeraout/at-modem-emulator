from modem.protocols.protocol import Protocol

def bytes_to_ip(data: bytes) -> str:
    return "{:d}.{:d}.{:d}.{:d}".format(*data)

class IP(Protocol):
    def __init__(self) -> None:
        super().__init__()

    def receive(self, buffer: bytes) -> None:
        # Check IP version
        if (buffer[0] >> 4) != 4:
            return
        
        self._receive_upper_protocol(buffer)

    def send(self, buffer: bytes, upper_protocol: Protocol = None) -> None:
        self._send_lower_protocol(buffer)

    def get_protocol_number(self) -> int:
        return 0x0021
