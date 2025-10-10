from modem.protocols.ip import bytes_to_ip
from modem.protocols.protocol import Protocol

class IPRoutingRule:
    def __init__(
        self,
        network: bytes,
        mask: bytes,
        protocol: Protocol
    ) -> None:
        self._network = network
        self._mask = mask
        self._protocol = protocol

    def is_applicable(self, buffer: bytes) -> bool:
        return bytes(
            network_byte & mask_byte
            for network_byte, mask_byte
            in zip(self._mask, buffer[16:20])
        ) == self._network
    
    def get_protocol(self) -> Protocol:
        return self._protocol
    
    def get_mask(self) -> bytes:
        return self._mask
    
    def get_network(self) -> bytes:
        return self._network

class IPRouter(Protocol):
    def __init__(self) -> None:
        super().__init__()
        self._rules: list[IPRoutingRule] = []

    def receive(self, buffer: bytes) -> None:
        for rule in self._rules:
            if rule.is_applicable(buffer):
                rule.get_protocol().send(buffer)
                break

    def send(self, buffer: bytes, upper_protocol: Protocol = None) -> None:
        pass

    def register_rule(self, rule: IPRoutingRule) -> None:
        self._rules.append(rule)
        self._rules.sort(
            key=lambda rule: rule.get_mask(),
            reverse=True
        )

    def remove_rule(self, network: bytes, mask: bytes) -> None:
        found = False

        for index, rule in enumerate(self._rules):
            if rule.get_network() == network and rule.get_mask() == mask:
                found = True
                break

        if not found:
            raise AttributeError()
        
        del self._rules[index]
