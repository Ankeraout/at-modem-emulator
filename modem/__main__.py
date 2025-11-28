import socket
import threading
from modem.protocols.hdlc import HDLC
from modem.client import Client
from modem.ip_router import IPRouter, IPRoutingRule
from modem.tun import Tun
from modem.protocols.dhcp import DHCP
from modem.protocols.fsk import FSK
from modem.sample_converter import SampleConverter_U8_FLOAT, SampleConverter_FLOAT_U8
import math
from modem.protocols.protocol import Protocol
import wave
import argparse

def test():
    class TestProtocol(Protocol):
        def __init__(self) -> None:
            super().__init__()
            self._already_written = False

        def receive(self, buffer: bytes) -> None:
            print("R: {:s}".format(" ".join("{:02x}".format(x) for x in buffer)))
    
        def send(self, buffer: bytes, upper_protocol: "Protocol" = None) -> None:
            if not self._already_written:
                with wave.open("test.wav", "wb") as f:
                    f.setframerate(8000)
                    f.setnchannels(1)
                    f.setnframes(len(buffer))
                    f.setsampwidth(1)
                    f.writeframesraw(buffer)
                    
                self._already_written = True

    v21 = FSK(8000, 300, [1850, 1650], 1600, SampleConverter_U8_FLOAT(), SampleConverter_FLOAT_U8())
    tp = TestProtocol()
    v21.set_upper_protocol(tp)
    v21.set_lower_protocol(tp)
    v21.send(b"Hello")
    v21.receive([])

    with wave.open("test.wav", "rb") as f:
        v21.receive(f.readframes(f.getnframes()))

def main_modem():
    network = b"\xc0\xa8\x0a\x00"
    mask = b"\xff\xff\xff\x00"
    ip_router = IPRouter()
    tun = Tun("tun1", ip_router)
    dhcp = DHCP(network, mask)
    local_ip = dhcp.allocate_ip()
    ip_router.register_rule(IPRoutingRule(local_ip, b"\xff\xff\xff\xff", tun))
    ip_router.register_rule(
        IPRoutingRule(b"\x00\x00\x00\x00", b"\x00\x00\x00\x00", tun)
    )

    tun_thread = threading.Thread(target=tun.run)
    tun_thread.start()

    with socket.create_server(("", 6666), family=socket.AF_INET6, dualstack_ipv6=True) as server_socket:
        print("Ready.")

        while True:
            client_socket, client_address = server_socket.accept()
            client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

            print("Accepted client from {:s}".format(str(client_address)))
            
            remote_ip = dhcp.allocate_ip()

            client = Client(client_socket, ip_router, local_ip, remote_ip)
            client.run()

            dhcp.free_ip(remote_ip)

            try:
                ip_router.remove_rule(remote_ip, b"\xff\xff\xff\xff")
            
            except:
                pass

def main():
    argument_parser = argparse.ArgumentParser(
        
    )

if __name__ == "__main__":
    exit(main())
