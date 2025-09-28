from modem.protocols.ppp.configuration_protocol import ConfigurationProtocol, Option

class OptionMRU(Option):
    def __init__(self, protocol: "LCP") -> None:
        super().__init__(protocol)
        self.reset()
    
    def on_configure_request(self, data: bytes) -> bool:
        if len(data) != 4:
            return False
        
        value = int.from_bytes(data[2:4])

        if value <= self._value:
            self._value = value
            return True
        
        else:
            return False

    def on_configure_ack(self, data: bytes) -> None:
        print("PPP MRU set to {:d} bytes.".format(self._value))
        self._protocol._upper_protocol._mru = self._value

    def reset(self) -> None:
        self._value = 1500

    def to_bytes(self) -> bytes:
        return b"\x01\x04" + self._value.to_bytes(2)

class LCP(ConfigurationProtocol):
    def __init__(self) -> None:
        super().__init__()
        self._code_handlers.update(
            {
                8: self._code_handler_protocol_reject,
                9: self._code_handler_echo_request,
                10: self._code_handler_echo_reply,
                11: self._code_handler_discard_request
            }
        )
        self._options.update(
            {
                1: OptionMRU(self)
            }
        )
        
    def _code_handler_protocol_reject(identifier: int, data: bytes) -> None:
        pass

    def _code_handler_echo_request(identifier: int, data: bytes) -> None:
        pass

    def _code_handler_echo_reply(identifier: int, data: bytes) -> None:
        pass

    def _code_handler_discard_request(identifier: int, data: bytes) -> None:
        pass

    def get_protocol_number(self) -> int:
        return 0xc021
