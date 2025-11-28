import queue
import socket
import threading
import time

class Client:
    def __init__(self, client_socket: socket.socket) -> None:
        self._client_socket = client_socket
        self._client_to_server_thread = (
            threading.Thread(target=self._client_to_server_thread_main)
        )
        self._server_to_client_thread = (
            threading.Thread(target=self._server_to_client_thread_main)
        )
        self._client_to_server_thread.start()
        self._server_to_client_thread.start()
        self._lock = threading.Lock()

    def _client_to_server_thread_main(self) -> None:
        lock_acquired = False

        while True:
            try:
                buffer = self._client_socket.recv(2000)

                if len(buffer) == 0:
                    break

                self._lock.acquire()
                lock_acquired = True

                if self._server_socket is not None:
                    self._server_socket.send(buffer)
                
                self._lock.release()
                lock_acquired = False

            except:
                break
        
        if not lock_acquired:
            self._lock.acquire()

        try:
            self._client_socket.close()

        except:
            pass

        try:
            self._server_socket.close()

        except:
            pass

        self._client_socket = None
        self._lock.release()

    def _server_to_client_thread_main(self) -> None:
        while True:
            try:
                server_socket = socket.create_connection(("server", 6666))
                server_socket.setsockopt(
                    socket.IPPROTO_TCP,
                    socket.TCP_NODELAY,
                    1
                )

            except:
                time.sleep(1)
                continue

            self._lock.acquire()
            self._server_socket = server_socket
            self._lock.release()

            lock_acquired = False

            while True:
                try:
                    buffer = self._server_socket.recv(2000)

                    if len(buffer) == 0:
                        break

                    self._lock.acquire()
                    lock_acquired = True

                    if self._client_socket is None:
                        break

                    self._client_socket.send(buffer)
                    self._lock.release()
                    lock_acquired = False

                except:
                    break

            if not lock_acquired:
                self._lock.acquire()

            if self._client_socket is None:
                self._lock.release()
                break

            self._lock.release()

def main():
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

        Client(client_socket)

if __name__ == "__main__":
    exit(main())
