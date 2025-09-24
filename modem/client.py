import socket
from modem.protocols.hdlc import HDLC
from modem.modem import Modem

class Client:
    def __init__(self, client_socket: socket.socket):
        self._socket = client_socket
        self._modem = Modem()

        hdlc = HDLC()

        self._modem.set_upper_protocol(hdlc)
        hdlc.set_lower_protocol(self._modem)

    def run(self) -> None:
        while True:
            self._modem.receive(self._socket.recv(1500))
