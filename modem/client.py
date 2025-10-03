import socket
from modem.protocols.hdlc import HDLC
from modem.protocols.protocol import Protocol
from modem.protocols.ppp import PPP, PPPState
from modem.protocols.ip import IP
from modem.protocols.ppp.ipcp import IPCP
from modem.modem import Modem
from modem.ip_router import IPRouter

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

    def __init__(self, client_socket: socket.socket, ip_router: IPRouter):
        self._socket = client_socket
        self._modem = Modem()
        self._modem.set_lower_protocol(Client._ClientSendProxy(self))
        self._modem.add_dial_handler(self._on_dial)
        self._ppp = PPP()
        self._ip_router = ip_router

    def run(self) -> None:
        while True:
            buffer = self._socket.recv(1500)

            if len(buffer) == 0:
                break

            else:
                self._modem.receive(buffer)

    def _on_dial(self, modem: Modem) -> None:
        hdlc = HDLC()

        modem.set_upper_protocol(hdlc)
        hdlc.set_lower_protocol(modem)
        hdlc.set_upper_protocol(self._ppp)
        self._ppp.set_lower_protocol(hdlc)
        self._ppp.register_state_change_handler(self._on_ppp_state_change)
        self._ppp.begin_configuration()

    def _on_ppp_state_change(
        self,
        previous_state: PPPState,
        new_state: PPPState
    ) -> None:
        print(
            "client: PPP state change from {:s} to {:s}.".format(
                previous_state.name,
                new_state.name
            )
        )

        if new_state == PPPState.NETWORK:
            ipcp = IPCP()
            ipcp.set_lower_protocol(self._ppp)
            ipcp.add_configuration_acknowledged_handler(
                self._on_ipcp_configuration_acknowledged
            )
            self._ppp.register_protocol(ipcp.get_protocol_number(), ipcp)
            ipcp.send_configure_request()

    def _on_ipcp_configuration_acknowledged(self) -> None:
        ip = IP()
        ip.set_lower_protocol(self._ip_router)
        ip.set_upper_protocol(self._ip_router)
        self._ppp.register_protocol(ip.get_protocol_number(), ip)
