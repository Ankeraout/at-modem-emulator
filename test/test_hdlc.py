from modem.protocols.hdlc import HDLC
from modem.protocols.protocol import Protocol
import unittest

class SendProtocol(Protocol):
    def __init__(self) -> None:
        super().__init__()
        self._last_sent = None

    def send(self, buffer: bytes, upper_protocol: Protocol = None) -> None:
        self._last_sent = buffer
    
    def receive(self, buffer: bytes) -> None:
        raise AttributeError()
    
    def get_last_sent(self) -> bytes:
        return self._last_sent

class TestHDLC(unittest.TestCase):
    def test_send_nominal(self) -> None:
        hdlc = HDLC()
        send_protocol = SendProtocol()
        hdlc.set_lower_protocol(send_protocol)
        hdlc.send(b"\x55\xaa")

        self.assertEqual(
            send_protocol.get_last_sent(),
            b"\x7e\xff\x7d\x23\x55\xaa\x77\x71\x7e"
        )

    def test_send_accm(self) -> None:
        hdlc = HDLC()
        send_protocol = SendProtocol()
        hdlc.set_lower_protocol(send_protocol)
        hdlc.send(b"\x01")

        self.assertEqual(
            send_protocol.get_last_sent(),
            b"\x7e\xff\x7d\x23\x7d\x21\xde\x3b\x7e"
        )

    def test_send_accm_2(self) -> None:
        hdlc = HDLC()
        send_protocol = SendProtocol()
        hdlc.set_lower_protocol(send_protocol)
        hdlc._accm[1] = False
        hdlc.send(b"\x01")

        self.assertEqual(
            send_protocol.get_last_sent(),
            b"\x7e\xff\x7d\x23\x01\xde\x3b\x7e"
        )
