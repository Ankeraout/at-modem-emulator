from abc import ABC, abstractmethod

class Protocol(ABC):
    def __init__(self) -> None:
        self._lower_protocol = None
        self._upper_protocol = None

    def set_lower_protocol(self, protocol: "Protocol") -> None:
        self._lower_protocol = protocol

    def set_upper_protocol(self, protocol: "Protocol") -> None:
        self._upper_protocol = protocol

    def _send_lower_protocol(self, buffer: bytes) -> None:
        if self._lower_protocol is not None:
            self._lower_protocol.send(buffer)

    def _receive_upper_protocol(self, buffer: bytes) -> None:
        if self._upper_protocol is not None:
            self._upper_protocol.receive(buffer)

    @abstractmethod
    def receive(buffer: bytes) -> None:
        """
        This method receives a message from the connected device.
        """
        pass

    @abstractmethod
    def send(buffer: bytes) -> None:
        """
        This method sends a message to the connected device.
        """
        pass
