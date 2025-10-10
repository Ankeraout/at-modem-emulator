from modem.protocols.ppp.configuration_protocol import ConfigurationProtocol, Option
from modem.protocols.ip import IP

class OptionIPAddress(Option):
    def __init__(
        self,
        protocol: "IPCP",
        local: bytes,
        remote: bytes
    ) -> None:
        super().__init__(protocol)
        self._local = local
        self._remote = remote

    def on_configure_request(self, data: bytes) -> bool:
        if len(data) != 4:
            return False
        
        return data == self._remote

    def on_configure_nak(self, data: bytes) -> bool:
        return True

    def to_bytes(self, remote: bool = False) -> bytes:
        return b"\x03\x06" + (self._remote if remote else self._local)

class IPCP(ConfigurationProtocol):
    def __init__(self, local: bytes, remote: bytes) -> None:
        super().__init__()
        self._options.update(
            {
                3: OptionIPAddress(self, local, remote)
            }
        )

    def get_protocol_number(self) -> int:
        return 0x8021
