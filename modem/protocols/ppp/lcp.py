import random
import struct
from enum import Enum
from modem.protocols.ppp.configuration_protocol import ConfigurationProtocol, Option

class LCPCode(Enum):
    PROTOCOL_REJECT = 8
    ECHO_REQUEST = 9
    ECHO_REPLY = 10
    DISCARD_REQUEST = 11

class OptionMRU(Option):
    def __init__(self, protocol: "LCP") -> None:
        super().__init__(protocol)
        self.reset()
    
    def on_configure_request(self, data: bytes) -> bool:
        if len(data) != 2:
            return False
        
        value = int.from_bytes(data)

        if value <= self._value:
            self._value = value
            return True
        
        else:
            return False

    def on_configure_ack(self) -> None:
        print("PPP MRU set to {:d} bytes.".format(self._value))
        self._protocol._lower_protocol._mru = self._value

    def reset(self) -> None:
        self._value = 1500

    def to_bytes(self) -> bytes:
        return b"\x01\x04" + self._value.to_bytes(2)
    
class OptionACCM(Option):
    def __init__(self, protocol: "LCP") -> None:
        super().__init__(protocol)
        self.reset()

    def on_configure_request(self, data: bytes) -> bool:
        if len(data) != 4:
            return False
        
        value = int.from_bytes(data)

        self._value = [(value & (1 << i)) != 0 for i in range(32)]

        return True
    
    def on_configure_ack(self) -> None:
        hdlc = self._protocol._lower_protocol._lower_protocol

        for i in range(32):
            hdlc._accm[i] = self._value[i]

    def reset(self) -> None:
        self._value = 32 * [False]

    def to_bytes(self):
        value = 0

        for i in range(32):
            if self._value[i]:
                value |= 1 << i

        return b"\x02\x06" + value.to_bytes(4)
    
class OptionMagicNumber(Option):
    def __init__(self, protocol: "LCP") -> None:
        super().__init__(protocol)
        self.reset()

    def on_configure_request(self, data: bytes) -> bool:
        if len(data) != 4:
            return False
        
        self._remote = int.from_bytes(data)

        return True
    
    def on_configure_ack(self) -> None:
        pass

    def reset(self) -> None:
        self._local = random.randint(0, 2 ** 32)
        self._remote = 0

    def to_bytes(self):
        return b"\x05\x06" + self._local.to_bytes(4)
    
class OptionPFC(Option):
    def __init__(self, protocol: "LCP") -> None:
        super().__init__(protocol)
        self.reset()

    def on_configure_request(self, data: bytes) -> bool:
        return len(data) == 0
    
    def on_configure_ack(self) -> None:
        self._protocol._lower_protocol._pfc = True

    def reset(self) -> None:
        pass

    def to_bytes(self):
        return b"\x07\x02"
    
class OptionACFC(Option):
    def __init__(self, protocol: "LCP") -> None:
        super().__init__(protocol)
        self.reset()

    def on_configure_request(self, data: bytes) -> bool:
        return len(data) == 0
    
    def on_configure_ack(self) -> None:
        self._protocol._lower_protocol._lower_protocol._acfc = True

    def reset(self) -> None:
        pass

    def to_bytes(self):
        return b"\x08\x02"

class LCP(ConfigurationProtocol):
    def __init__(self) -> None:
        super().__init__()
        self._code_handlers.update(
            {
                LCPCode.PROTOCOL_REJECT.value:
                    self._code_handler_protocol_reject,
                LCPCode.ECHO_REQUEST.value: self._code_handler_echo_request,
                LCPCode.ECHO_REPLY.value: self._code_handler_echo_reply,
                LCPCode.DISCARD_REQUEST.value:
                    self._code_handler_discard_request
            }
        )
        self._options.update(
            {
                1: OptionMRU(self),
                2: OptionACCM(self),
                5: OptionMagicNumber(self),
                7: OptionPFC(self),
                8: OptionACFC(self)
            }
        )
        self._protocol_reject_identifier = 0

    def get_protocol_number(self) -> int:
        return 0xc021
    
    def send_protocol_reject(self, protocol: int, data: bytes) -> None:
        self._send_lower_protocol(
            struct.pack(
                ">BBHH",
                LCPCode.PROTOCOL_REJECT.value,
                self._protocol_reject_identifier,
                len(data) + 6,
                protocol
            ) + data
        )

        self._protocol_reject_identifier += 1
        
    def _code_handler_protocol_reject(identifier: int, data: bytes) -> None:
        pass

    def _code_handler_echo_request(identifier: int, data: bytes) -> None:
        pass

    def _code_handler_echo_reply(identifier: int, data: bytes) -> None:
        pass

    def _code_handler_discard_request(identifier: int, data: bytes) -> None:
        pass
