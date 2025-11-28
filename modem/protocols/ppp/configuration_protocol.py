from abc import ABC, abstractmethod
import struct
from enum import Enum
from modem.protocols.protocol import Protocol
import time
import threading

class ConfigurationProtocolCode(Enum):
    CONFIGURE_REQUEST = 1
    CONFIGURE_ACK = 2
    CONFIGURE_NAK = 3
    CONFIGURE_REJECT = 4
    TERMINATE_REQUEST = 5
    TERMINATE_ACK = 6
    CODE_REJECT = 7

class Option(ABC):
    def __init__(self, protocol: "ConfigurationProtocol") -> None:
        self._protocol = protocol

    @abstractmethod
    def on_configure_request(self, data: bytes) -> bool:
        """
        Returns a boolean that indicates whether or not the configuration
        should be acknowledged (True) or not (False).
        """
        pass

    def on_configure_ack(self, data: bytes) -> bool:
        """
        Returns a boolean that indicates whether the acknowledged option is
        good (True) or not (False).
        """
        return True

    def on_configure_nak(self, data: bytes) -> bool:
        """
        Returns a boolean that indicates whether the negociation shall be
        terminated (True) or not (False).
        """
        return False

    def reset(self) -> None:
        pass

    @abstractmethod
    def to_bytes(self, remote: bool = False) -> bytes:
        """
        The "remote" parameter indicates whether the bytes to obtain are for the
        remote or the local configuration. This is useful for options that
        need different parameters on both ends, for example IPCP IP-Address.
        """
        pass

class ConfigurationProtocol(Protocol):
    def __init__(self) -> None:
        super().__init__()
        self._code_handlers = {
            ConfigurationProtocolCode.CONFIGURE_REQUEST.value:
                self._code_handler_configure_request,
            ConfigurationProtocolCode.CONFIGURE_ACK.value:
                self._code_handler_configure_ack,
            ConfigurationProtocolCode.CONFIGURE_NAK.value:
                self._code_handler_configure_nak,
            ConfigurationProtocolCode.CONFIGURE_REJECT.value:
                self._code_handler_configure_reject,
            ConfigurationProtocolCode.TERMINATE_REQUEST.value:
                self._code_handler_terminate_request,
            ConfigurationProtocolCode.TERMINATE_ACK.value:
                self._code_handler_terminate_ack,
            ConfigurationProtocolCode.CODE_REJECT.value:
                self._code_handler_code_reject
        }
        self._options: dict[int, Option] = {}
        self._rejected_options: dict[int, Option] = {}
        self._configuration_acknowledged_handlers = []
        self._restart_task_lock = threading.RLock()
        self.reset()

    def receive(self, buffer: bytes) -> None:
        code = buffer[0]
        identifier = buffer[1]
        length = int.from_bytes(buffer[2:4])
        data = buffer[4:4 + length]

        if code in self._code_handlers.keys():
            self._code_handlers[code](identifier, data)

        else:
            self._send_code_reject(buffer)

    def send(self) -> None:
        raise AttributeError()

    def reset(self) -> None:
        self._rejected_options.clear()
        
        for option in self._options:
            option.reset()

        self._configure_ack_received = False
        self._configure_ack_sent = False
        self._code_reject_identifier = 0
        self._configure_request_identifier = 0
        self._terminate_request_identifier = 0

        with self._restart_task_lock:
            self._restart_task = {
                "last-execution": 0,
                "timeout": 3,
                "done": False,
                "thread": None,
                "exited": True
            }

    def send_configure_request(self) -> None:
        self._configure_ack_received = False

        data = bytearray()
        options = { code: option for code, option in self._options.items() if code not in self._rejected_options }

        for option in options.values():
            data.extend(option.to_bytes())

        self._send_lower_protocol(
            struct.pack(
                ">BBH",
                ConfigurationProtocolCode.CONFIGURE_REQUEST.value,
                self._configure_request_identifier,
                len(data) + 4
            ) + bytes(data)
        )

        with self._restart_task_lock:
            self._restart_task["last-execution"] = time.time()
            self._restart_task["done"] = False

            if self._restart_task["exited"]:
                self._restart_task["exited"] = False
                self._restart_task["thread"] = threading.Thread(
                    target=self._restart_thread_main
                )
                self._restart_task["thread"].start()

    def add_configuration_acknowledged_handler(self, handler) -> None:
        self._configuration_acknowledged_handlers.append(handler)

    def remove_configuration_acknowledged_handler(self, handler) -> None:
        self._configuration_acknowledged_handlers.remove(handler)

    def _restart_thread_main(self) -> None:
        while True:
            with self._restart_task_lock:
                next_execution = (
                    self._restart_task["last-execution"]
                    + self._restart_task["timeout"]
                )
            
            remaining_time = next_execution - time.time()

            if remaining_time > 0:
                time.sleep(remaining_time)

            with self._restart_task_lock:
                if self._restart_task["done"]:
                    break
                
                else:
                    self.send_configure_request()
                    self._restart_task["last-execution"] = time.time()
        
        with self._restart_task_lock:
            self._restart_task["exited"] = True

    def _send_code_reject(
        self,
        information: bytes
    ) -> None:
        self._send_lower_protocol(
            struct.pack(
                ">BBH",
                ConfigurationProtocolCode.CODE_REJECT.value,
                self._code_reject_identifier,
                len(information) + 6
            )
            + information
        )
        self._code_reject_identifier += 1

    def _decode_options(
        self,
        data: bytes
    ) -> list[dict]:
        return_data = []
        read_index = 0

        # Construct the option array
        while read_index < len(data):
            max_option_length = len(data) - read_index

            if max_option_length < 2:
                # TODO: error case: cannot read type and length.
                break

            type = data[read_index]
            length = data[read_index + 1]
            option_data = data[read_index + 2:read_index + length]
            option = self._options[type] if type in self._options else None

            if option is None:
                print("{:s}: Unknown option {:d}.".format(self.__class__.__name__, type))

            if max_option_length < length:
                # TODO: error case: invalid option length
                break

            return_data.append(
                {
                    "type": type,
                    "data": option_data,
                    "option": option
                }
            )

            read_index += length
        
        return return_data
    
    def _code_handler_configure_request(
        self,
        identifier: int,
        data: bytes
    ) -> None:
        def option_data_to_bytes(option_data: list[dict], copy: bool) -> bytes:
            result = bytearray()

            for option in option_data:
                if copy:
                    data = struct.pack(
                        "BB",
                        option["type"],
                        len(option["data"]) + 2
                    ) + option["data"]

                else:
                    data = option["option"].to_bytes(True)

                result.extend(data)

            return bytes(result)

        option_data = self._decode_options(data)

        reject = []
        ack = []
        nak = []

        for option in option_data:
            if option["option"] is None:
                reject.append(option)
            
            else:
                result = option["option"].on_configure_request(option["data"])
                
                if result:
                    ack.append(option)

                else:
                    nak.append(option)

        if len(reject) != 0:
            result_code = ConfigurationProtocolCode.CONFIGURE_REJECT
            result_options = reject

        elif len(nak) != 0:
            result_code = ConfigurationProtocolCode.CONFIGURE_NAK
            result_options = nak

        else:
            result_code = ConfigurationProtocolCode.CONFIGURE_ACK
            result_options = ack
            self._acknowledge_remote_configuration()
        
        data = option_data_to_bytes(
            result_options,
            result_code == ConfigurationProtocolCode.CONFIGURE_REJECT
        )
        self._send_lower_protocol(
            struct.pack(
                ">BBH",
                result_code.value,
                identifier,
                len(data) + 4
            ) + data
        )

    def _acknowledge_remote_configuration(self) -> None:
        if not self._configure_ack_sent:
            self._configure_ack_sent = True
            self._check_configuration_acknowledged()
        
    def _acknowledge_local_configuration(self) -> None:
        if not self._configure_ack_received:
            self._configure_ack_received = True
            self._check_configuration_acknowledged()
        
    def _check_configuration_acknowledged(self) -> None:
        if self._configure_ack_received and self._configure_ack_sent:
            with self._restart_task_lock:
                self._restart_task["done"] = True
                
            self._fire_configuration_acknowledged()
    
    def _fire_configuration_acknowledged(self) -> None:
        for handler in self._configuration_acknowledged_handlers:
            handler()

    def _code_handler_configure_ack(self, identifier: int, data: bytes) -> None:
        option_data = self._decode_options(data)

        # If an option is not recognized, the packet is invalid and therefore
        # silently discarded.
        if any(option["option"] is None for option in option_data):
            return
        
        if all(
            option["option"].on_configure_ack(option["data"])
            for option in option_data
        ):
            self._acknowledge_local_configuration()

        else:
            for option in option_data:
                if not option["option"].on_configure_ack(option["data"]):
                    pass

            self.send_configure_request()

    def _code_handler_configure_nak(self, identifier: int, data: bytes) -> None:
        option_data = self._decode_options(data)

        if any(option["option"] is None for option in option_data):
            return
        
        if any(
            option["option"].on_configure_nak(
                option["data"]
            ) for option in option_data
        ):
            # Terminate negociation
            self._send_lower_protocol(
                struct.pack(
                    ">BBH",
                    ConfigurationProtocolCode.TERMINATE_REQUEST.value,
                    self._terminate_request_identifier,
                    4
                )
            )
            self._terminate_request_identifier += 1

        else:
            self.send_configure_request()

    def _code_handler_configure_reject(
        self,
        identifier: int,
        data: bytes
    ) -> None:
        option_data = self._decode_options(data)

        # If an option is not recognized, the packet is invalid and therefore
        # silently discarded.
        if any(option["option"] is None for option in option_data):
            return
        
        for option in option_data:
            self._rejected_options[option["type"]] = option["option"]
        
        self.send_configure_request()

    def _code_handler_terminate_request(
        self,
        identifier: int,
        data: bytes
    ) -> None:
        self._send_lower_protocol(
            struct.pack(
                ">BBH",
                ConfigurationProtocolCode.TERMINATE_ACK.value,
                identifier,
                0
            )
        )

    def _code_handler_terminate_ack(self, identifier: int, data: bytes) -> None:
        pass

    def _code_handler_code_reject(self, identifier: int, data: bytes) -> None:
        self._send_lower_protocol(
            struct.pack(
                ">BBH",
                ConfigurationProtocolCode.TERMINATE_REQUEST.value,
                identifier,
                0
            )
        )
