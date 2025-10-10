import socket
import threading
import time

RECEIVE_BUFFER_SIZE = 1500
RETRY_DELAY = 10

def pipe_thread(
    source_socket: socket.socket,
    destination_socket: socket.socket
) -> str:
    print("Pipe thread started.")

    while True:
        try:
            buffer = source_socket.recv(RECEIVE_BUFFER_SIZE)

        except:
            buffer = b""
        
        if len(buffer) == 0:
            return source_socket
        
        try:
            destination_socket.sendall(buffer)
        
        except:
            return destination_socket

def main() -> int:
    TARGET_ADDRESS=("server", 6666)

    server_socket = socket.create_server(
        ("", 6667),
        family=socket.AF_INET6,
        dualstack_ipv6=True
    )

    print("Ready.")

    while True:
        client_socket, client_address = server_socket.accept()
        print("Incoming connection from {:s}.".format(str(client_address)))

        client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        while True:
            print("Connecting to the target...")

            try:
                target_socket = socket.create_connection(TARGET_ADDRESS)

            except:
                print(
                    "Failed to connect to the target. Retrying in {:d} seconds."
                    .format(RETRY_DELAY)
                )
                time.sleep(RETRY_DELAY)
                continue

            target_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

            print("Starting threads...")

            threading.Thread(
                target=pipe_thread,
                args=(client_socket, target_socket)
            ).start()

            closed_socket = pipe_thread(target_socket, client_socket)

            if closed_socket == target_socket:
                print("Connection lost to the target.")

            else:
                print("Connection lost to the client.")
                target_socket.close()
                break

    return 0

if __name__ == "__main__":
    exit(main())
