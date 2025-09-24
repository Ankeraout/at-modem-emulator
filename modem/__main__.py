import socket
from modem.protocols.hdlc import HDLC
from modem.client import Client

def main():
    with socket.create_server(("", 6666), family=socket.AF_INET6, dualstack_ipv6=True) as server_socket:
        print("Ready.")

        while True:
            client_socket, client_address = server_socket.accept()

            print("Accepted client from {:s}".format(str(client_address)))
            
            client = Client(client_socket)
            client.run()

if __name__ == "__main__":
    exit(main())
