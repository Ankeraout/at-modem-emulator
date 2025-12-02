from abc import ABC, abstractmethod
import json
import sys

class Object(ABC):
    def __init__(self, configuration: dict):
        pass

    @abstractmethod
    def init(self) -> None:
        pass

class Protocol(ABC):
    def __init__(self, configuration: dict):
        pass

    @abstractmethod
    def init(self) -> None:
        pass

    @abstractmethod
    def instantiate(self) -> "Protocol":
        pass

    @abstractmethod
    def receive(self, buffer: bytes) -> None:
        pass

    @abstractmethod
    def send(self, buffer: bytes, upper_protocol: "Protocol" = None) -> None:
        pass

    def send_lower_protocol(self, buffer: bytes) -> None:
        if self._lower_protocol is not None:
            self._lower_protocol.send(buffer, upper_protocol=self)

    def receive_upper_protocol(self, buffer: bytes) -> None:
        if self._upper_protocol is not None:
            self._upper_protocol.receive(buffer)

class Application:
    def __init__(self):
        self._objects = dict()
        self._protocols = dict()
        self._stack = list()

def main(config_file_name: str):
    with open(config_file_name) as f:
        config = json.load(f)

    print(config)

if __name__ == "__main__":
    exit(main(sys.argv[1]))
