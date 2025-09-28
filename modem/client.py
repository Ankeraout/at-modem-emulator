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
        self._modem.set_lower_protocol(Client._ClientSendProxy(self))
        self._modem.add_dial_handler(self._on_dial)

    def run(self) -> None:
        while True:
            buffer = self._socket.recv(1500)

            if len(buffer) == 0:
                break

            else:
                self._modem.receive(buffer)

    def _on_dial(self, modem: Modem) -> None:
        hdlc = HDLC()
        ppp = PPP()

        modem.set_upper_protocol(hdlc)
        hdlc.set_lower_protocol(modem)
        hdlc.set_upper_protocol(ppp)
        ppp.set_lower_protocol(hdlc)

        ppp._lcp.send_configure_request()
