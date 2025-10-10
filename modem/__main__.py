import socket
import threading
from modem.protocols.hdlc import HDLC
from modem.client import Client
from modem.ip_router import IPRouter, IPRoutingRule
from modem.tun import Tun
from modem.protocols.dhcp import DHCP

def main():
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

if __name__ == "__main__":
    exit(main())
