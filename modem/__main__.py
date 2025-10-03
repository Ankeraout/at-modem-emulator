import socket
from modem.protocols.hdlc import HDLC
from modem.client import Client
from modem.ip_router import IPRouter

def main():
    ip_router = IPRouter()

    with socket.create_server(("", 6666), family=socket.AF_INET6, dualstack_ipv6=True) as server_socket:
        print("Ready.")

        while True:
            client_socket, client_address = server_socket.accept()
            client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

            print("Accepted client from {:s}".format(str(client_address)))
            
            client = Client(client_socket, ip_router)
            client.run()

if __name__ == "__main__":
    exit(main())
