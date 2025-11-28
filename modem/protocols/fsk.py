from modem.protocols.synchronous_protocol import SynchronousProtocol
from modem.sample_converter import SampleConverter
import math

class FSK(SynchronousProtocol):
    def __init__(
        self,
        sample_rate: int,
        baud_rate: int,
        frequencies: list[int],
        buffer_size: int,
        sample_converter_rx: SampleConverter,
        sample_converter_tx: SampleConverter
    ) -> None:
        super().__init__(sample_rate, buffer_size)
        self._rx_sample_timing = 0.
        self._rx_symbol_timing = 0.
        self._rx_symbol_buffer = [[0., 0.] for _ in frequencies]
        self._rx_bit_buffer = []
        self._tx_bit_buffer = []
        self._tx_symbol_timing = 0.
        self._tx_phase = 0.
        self._tx_current_symbol = 1
        self._sample_converter_rx = sample_converter_rx
        self._sample_converter_tx = sample_converter_tx
        self._baud_rate = baud_rate
        self._frequencies = frequencies
        self._samples_per_symbol = sample_rate / baud_rate
        self._symbol_sample_count = 0
        self._sample_rate = sample_rate

        if len(frequencies) not in (2, 4, 8, 16):
            raise Exception("Invalid frequency count")
        
        self._bits_per_symbol = int(math.log2(len(frequencies)))
        
    def receive(self, buffer: bytes) -> None:
        super().receive(buffer)

        data = []
        self._sample_converter_rx.convert(self._receive_buffer, data)

        for sample in data:
            sample_timing = math.pi * 2 * self._rx_sample_timing

            for index, frequency in enumerate(self._frequencies):
                frequency_sample_timing = sample_timing * frequency
                frequency_sample_i = math.cos(frequency_sample_timing) * sample
                frequency_sample_q = math.sin(frequency_sample_timing) * sample
                self._rx_symbol_buffer[index][0] += frequency_sample_i
                self._rx_symbol_buffer[index][1] += frequency_sample_q

            self._rx_symbol_timing += 1 / self._samples_per_symbol
            self._rx_sample_timing += 1 / self._sample_rate
            self._symbol_sample_count += 1

            if self._rx_sample_timing >= 1:
                self._rx_sample_timing -= 1

            if self._rx_symbol_timing >= 1:
                self._rx_symbol_timing -= 1

                powers = [
                    math.sqrt(sum(x ** 2 for x in self._rx_symbol_buffer[i])) / self._symbol_sample_count
                    for i in range(len(self._rx_symbol_buffer))
                ]

                index_max = 0
                value_max = 0

                for index, power in enumerate(powers):
                    if power > value_max:
                        index_max = index
                        value_max = power

                received_bits = [
                    1 if (index_max & (1 << bit_index)) != 0 else 0
                    for bit_index in range(self._bits_per_symbol)
                ]

                for bit in received_bits:
                    # Are we reading a byte or receiving a start bit?
                    if len(self._rx_bit_buffer) > 0 or bit == 0:
                        self._rx_bit_buffer.append(bit)

                        # Start bit + byte + stop bit = 10 bits
                        while len(self._rx_bit_buffer) >= 10:
                            # Stop bit must be 1
                            if self._rx_bit_buffer[9] == 1:
                                value = sum(
                                    n << i
                                    for i, n in enumerate(self._rx_bit_buffer[1:9])
                                )
                            
                                self._receive_upper_protocol(bytes([value]))
                                del self._rx_bit_buffer[:10]

                            else:
                                try:
                                    next_start_index = self._rx_bit_buffer[1:].index(0) + 1
                                    del self._rx_bit_buffer[:next_start_index]

                                except ValueError:
                                    self._rx_bit_buffer.clear()
                
                for i in range(len(self._frequencies)):
                    for j in range(2):
                        self._rx_symbol_buffer[i][j] = 0.
                
                self._symbol_sample_count = 0

        send_buffer = []

        while self._buffer_size > len(send_buffer) - self._delay:
            if len(self._tx_bit_buffer) == 0:
                if len(self._send_buffer) > 0:
                    for n in self._send_buffer:
                        self._tx_bit_buffer.append(0) # Start bit
                        self._tx_bit_buffer += [(n >> i) & 0x01 for i in range(8)]
                        self._tx_bit_buffer.append(1) # Stop bit

                    self._send_buffer.clear()
                
                else:
                    self._tx_bit_buffer.append(1)

            self._tx_phase += 2 * math.pi * self._frequencies[self._tx_current_symbol] / self._sample_rate
            self._sample_converter_tx.convert([math.sin(self._tx_phase)], send_buffer)
            self._tx_symbol_timing += 1 / self._samples_per_symbol

            if self._tx_symbol_timing >= 1:
                self._tx_symbol_timing -= 1
                self._tx_current_symbol = self._tx_bit_buffer.pop(0)
        
        self._delay -= len(send_buffer)
        self._send_lower_protocol(bytes(send_buffer))
