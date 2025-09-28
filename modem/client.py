import socket
from modem.protocols.hdlc import HDLC
from modem.protocols.protocol import Protocol
from modem.protocols.ppp import PPP
from modem.modem import Modem

class Client:
    class _ClientSendProxy(Protocol):
        def __init__(self, client: "Client") -> None:
            self._client = client
        
        def receive(self, buffer: bytes) -> None:
            pass

        def send(
            self,
            buffer: bytes,
            upper_protocol: "Protocol" = None
        ) -> None:
            self._client._socket.send(buffer)

    def __init__(self, client_socket: socket.socket):
        self._socket = client_socket
        self._modem = Modem()

        hdlc = HDLC()
        ppp = PPP()

        self._modem.set_lower_protocol(Client._ClientSendProxy(self))
        self._modem.set_upper_protocol(hdlc)
        hdlc.set_lower_protocol(self._modem)
        hdlc.set_upper_protocol(ppp)
        ppp.set_lower_protocol(hdlc)

    def run(self) -> None:
        while True:
            self._modem.receive(self._socket.recv(1500))
