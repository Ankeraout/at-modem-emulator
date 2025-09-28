from modem.protocols.ppp.configuration_protocol import ConfigurationProtocol, Option



class IPCP(ConfigurationProtocol):
    def __init__(self) -> None:
        super().__init__()

    def get_protocol_number(self) -> int:
        return 0x8021
