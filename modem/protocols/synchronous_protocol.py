from modem.protocols.protocol import Protocol

class SynchronousProtocol(Protocol):
    def __init__(self, data_rate: int, buffer_size: int):
        self._buffer_size = buffer_size
        self._data_rate = data_rate
        self._send_buffer = []
        self._receive_buffer = []
        self._delay = 0

    def receive(self, buffer: bytes) -> None:
        self._receive_buffer.extend(buffer)
        self._delay += len(buffer)

    def send(self, buffer: bytes, upper_protocol: Protocol = None) -> None:
        self._send_buffer.extend(buffer)
