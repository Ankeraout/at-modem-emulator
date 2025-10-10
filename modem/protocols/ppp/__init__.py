from enum import Enum
from modem.protocols.protocol import Protocol
from modem.protocols.ppp.lcp import LCP

class PPPState(Enum):
    DEAD = 0
    ESTABLISH = 1
    AUTHENTICATE = 2
    NETWORK = 3
    TERMINATE = 4

class PPP(Protocol):
    def __init__(self) -> None:
        super().__init__()
        self._pfc = False
        self._state = PPPState.ESTABLISH
        self._lcp = LCP()
        self._lcp.set_lower_protocol(self)
        self._lcp.add_configuration_acknowledged_handler(
            self._on_lcp_configuration_acknowledged
        )
        self._protocol_handlers = {
            0xc021: self._lcp
        }
        self._state_change_handlers = list()
    
    def receive(self, buffer: bytes) -> None:
        protocol_length = 2 if (buffer[0] & 1) == 0 else 1

        if not self._pfc and protocol_length == 1:
            # Received a frame with the protocol field value compressed without
            # negociating the protocol field compression first.
            return

        protocol = int.from_bytes(buffer[:protocol_length])

        if (protocol & 1) != 1:
            # Discard frames with invalid protocol field value
            return
        
        if protocol in self._protocol_handlers:
            self._protocol_handlers[protocol].receive(buffer[protocol_length:])

        else:
            self._lcp.send_protocol_reject(protocol, buffer[protocol_length:])

    def send(self, buffer: bytes, upper_protocol: Protocol = None) -> None:
        header = bytearray()
        protocol_number = upper_protocol.get_protocol_number()

        if (
            self._pfc
            and protocol_number <= 0xfe
            and (protocol_number & 1) == 1
        ):
            header = protocol_number.to_bytes(1)

        else:
            header = protocol_number.to_bytes(2)

        self._send_lower_protocol(header + buffer)

    def begin_configuration(self) -> None:
        self._lcp.send_configure_request()

    def register_protocol(self, number: int, protocol: Protocol) -> None:
        self._protocol_handlers[number] = protocol

    def register_state_change_handler(self, handler) -> None:
        self._state_change_handlers.append(handler)

    def set_state(self, new_state: PPPState) -> None:
        previous_state = self._state
        self._state = new_state

        for handler in self._state_change_handlers:
            handler(previous_state, new_state)

    def _on_lcp_configuration_acknowledged(self) -> None:
        self.set_state(PPPState.NETWORK)
