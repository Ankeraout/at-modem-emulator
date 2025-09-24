from modem.protocols.protocol import Protocol

class Modem(Protocol):
    def __init__(self) -> None:
        super().__init__()

    def receive(self, buffer: bytes) -> None:
        self._receive_upper_protocol(buffer)

    def send(self, buffer: bytes) -> None:
        pass
