from modem.protocols.protocol import Protocol

class IPV6(Protocol):
    def __init__(self) -> None:
        super().__init__()

    def receive(self, buffer: bytes) -> None:
        pass

    def send(self, buffer: bytes, upper_protocol: Protocol = None) -> None:
        pass
