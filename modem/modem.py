from enum import Enum
from modem.protocols.protocol import Protocol

class ModemState(Enum):
    COMMAND = 1
    COMMAND_A = 2
    COMMAND_AT = 3
    DATA = 4
    DATA_PLUS1 = 5
    DATA_PLUS2 = 6

class ModemReturnCode(Enum):
    OK = (0, "OK")
    CONNECT = (1, "CONNECT")
    RING = (2, "RING")
    NO_CARRIER = (3, "NO CARRIER")
    ERROR = (4, "ERROR")
    CONNECT_1200 = (5, "CONNECT 1200")
    NO_DIALTONE = (6, "NO DIALTONE")
    BUSY = (7, "BUSY")
    NO_ANSWER = (8, "NO ANSWER")
    CONNECT_2400 = (10, "CONNECT 2400")
    CONNECT_4800 = (11, "CONNECT 4800")
    CONNECT_9600 = (12, "CONNECT 9600")
    CONNECT_14400 = (13, "CONNECT 14400")
    CONNECT_19200 = (14, "CONNECT 19200")
    CONNECT_1200_75 = (22, "CONNECT 1200/75")
    CONNECT_75_1200 = (23, "CONNECT 75/1200")
    CONNECT_7200 = (24, "CONNECT 7200")
    CONNECT_12000 = (25, "CONNECT 12000")
    CONNECT_38400 = (28, "CONNECT 38400")

IDENTIFICATION_CODES = [
    "at-modem-emulator",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    ""
]

class Modem(Protocol):
    def __init__(self) -> None:
        super().__init__()
        self._state = ModemState.COMMAND
        self._command_buffer = bytearray(80)
        self._command_length = 0
        self._error_command_too_long = False
        self._command_handlers = {
            "D": self._command_handler_D,
            "E": self._command_handler_E,
            "H": self._command_handler_H,
            "I": self._command_handler_I,
            "O": self._command_handler_O,
            "Q": self._command_handler_Q,
            "S": self._command_handler_S,
            "V": self._command_handler_V,
            "Z": self._command_handler_Z
        }
        self._dial_handlers = set()
        self._hang_handlers = set()
        self._reset()

    def receive(self, buffer: bytes) -> None:
        for byte in buffer:
            self._receive(byte)

    def send(self, buffer: bytes, upper_protocol: Protocol = None) -> None:
        if self._state in (
            ModemState.DATA,
            ModemState.DATA_PLUS1,
            ModemState.DATA_PLUS2
        ):
            self._send_lower_protocol(buffer)

    def add_dial_handler(self, handler) -> None:
        self._dial_handlers.add(handler)

    def add_hang_handler(self, handler) -> None:
        self._hang_handlers.add(handler)

    def remove_dial_handler(self, handler) -> None:
        self._dial_handlers.remove(handler)

    def remove_hand_handler(self, handler) -> None:
        self._hang_handlers.remove(handler)

    def _fire_dial(self) -> None:
        for handler in self._dial_handlers:
            handler(self)

    def _fire_hang(self) -> None:
        for handler in self._hang_handlers:
            handler(self)

    def _receive(self, byte: int) -> None:
        if self._echo:
            self._send_lower_protocol(byte.to_bytes(1))

        if self._state == ModemState.COMMAND:
            if byte == ord('A'):
                self._state = ModemState.COMMAND_A
        
        elif self._state == ModemState.COMMAND_A:
            if byte == ord('T'):
                self._state = ModemState.COMMAND_AT
                self._command_length = 0

            else:
                self._state = ModemState.COMMAND

        elif self._state == ModemState.COMMAND_AT:
            if byte == 13:
                self._execute_command()

            elif self._command_length < len(self._command_buffer):
                self._command_buffer[self._command_length] = byte
                self._command_length += 1

            else:
                self._error_command_too_long = True
        
        elif self._state == ModemState.DATA:
            if byte == ord("+"):
                self._state = ModemState.DATA_PLUS1

            else:
                self._receive_upper_protocol(byte.to_bytes(1))
        
        elif self._state == ModemState.DATA_PLUS1:
            if byte == ord("+"):
                self._state = ModemState.DATA_PLUS2

            else:
                self._state = ModemState.DATA
                self._receive_upper_protocol(b"+" + byte.to_bytes(1))
        
        elif self._state == ModemState.DATA_PLUS2:
            if byte == ord("+"):
                self._state = ModemState.COMMAND

            else:
                self._state = ModemState.DATA
                self._receive_upper_protocol(b"++" + byte.to_bytes(1))

    def _reset_command_buffer(self) -> None:
        self._command_length = 0
        self._error_command_too_long = False
    
    def _execute_command(self) -> None:
        if self._error_command_too_long:
            self._send_response(ModemReturnCode.ERROR)
            self._error_command_too_long = False

        else:
            self._command_response = ModemReturnCode.OK
            self._command_read_index = 0

            while self._peek_character() != "":
                command = self._peek_character().upper()

                if command not in self._command_handlers.keys():
                    self._command_response = ModemReturnCode.ERROR
                    break

                self._advance()
                self._command_handlers[command]()

            self._command_length = 0
            self._send_response(self._command_response)

            if self._state == ModemState.COMMAND_AT:
                self._state = ModemState.COMMAND

        while len(self._execute_after_command) != 0:
            self._execute_after_command[0]()
            del self._execute_after_command[0]

    def _reset(self) -> None:
        self._s_register = [
            0,
            0,
            43,
            13,
            10,
            8,
            2,
            50,
            2,
            6,
            14,
            95,
            50
        ]
        self._echo = True
        self._quiet = False
        self._verbose = True
        self._connected = False
        self._execute_after_command = []

    def _send_response(self, response_code: ModemReturnCode) -> None:
        if not self._quiet:
            if self._verbose:
                self._send_lower_protocol(
                    (response_code.value[1] + "\r\n").encode()
                )
            
            else:
                self._send_lower_protocol(
                    "{:d}\r".format(response_code.value[0]).encode()
                )

    def _peek_character(self) -> str:
        if self._command_read_index == self._command_length:
            return ""
        
        else:
            return chr(self._command_buffer[self._command_read_index]) 

    def _advance(self) -> str:
        if self._command_read_index != self._command_length:
            self._command_read_index += 1

    def _read_character(self) -> str:
        if self._command_read_index == self._command_length:
            return ""
        
        else:
            return_value = self._peek_character()
            self._advance()
            return return_value
        
    def _command_handler_D(self) -> None:
        if self._peek_character() in ("T", "P"):
            self._advance()
        
        while self._peek_character().isdigit():
            self._advance()
        
        if self._peek_character() == ";":
            self._advance()
            go_to_data_state = False

        else:
            go_to_data_state = True

        if self._connected:
            self._command_response = ModemReturnCode.ERROR

        else:
            if go_to_data_state:
                self._state = ModemState.DATA

            self._connected = True
            self._command_response = ModemReturnCode.CONNECT_1200

            self._execute_after_command.append(self._fire_dial)

    def _command_handler_E(self) -> None:
        parameter = self._peek_character()
        self._echo = parameter == "1"

        if parameter in "01":
            self._advance()

    def _command_handler_H(self) -> None:
        if not self._connected:
            self._command_response = ModemReturnCode.ERROR

        elif self._connected:
            self._connected = False
            self._fire_hang()

    def _command_handler_I(self) -> None:
        parameter = self._peek_character()

        if parameter.isdigit():
            self._advance()
            self._send_lower_protocol(
                "{:s}\r".format(IDENTIFICATION_CODES[int(parameter)]).encode()
            )

        else:
            self._command_response = ModemReturnCode.ERROR

    def _command_handler_O(self) -> None:
        if not self._connected:
            self._command_response = ModemReturnCode.ERROR

        self._state = ModemState.DATA

    def _command_handler_Q(self) -> None:
        parameter = self._peek_character()
        self._quiet = parameter == "1"

        if parameter in "01":
            self._advance()

    def _command_handler_S(self) -> None:
        def is_register_number_valid(register_number: int) -> bool:
            return register_number < len(self._s_register)

        register_number = 0

        while self._peek_character().isdigit():
            register_number *= 10
            register_number += int(self._peek_character())
            self._advance()

        if self._peek_character() == "=":
            value = 0
            self._advance()

            while self._peek_character().isdigit():
                value *= 10
                value += int(self._peek_character())
                self._advance()

            if is_register_number_valid(register_number):
                self._s_register[register_number] = value

            else:
                self._command_response = ModemReturnCode.ERROR

        elif self._peek_character() == "?":
            if is_register_number_valid(register_number):
                self._send_lower_protocol(
                    "{:d}\r\n".format(self._s_register[register_number])
                )

            else:
                self._command_response = ModemReturnCode.ERROR
        
    def _command_handler_V(self) -> None:
        parameter = self._peek_character()
        self._verbose = parameter == "1"

        if parameter in "01":
            self._advance()

    def _command_handler_Z(self) -> None:
        self._reset()
