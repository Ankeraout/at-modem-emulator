from modem.protocols.synchronous_protocol import SynchronousProtocol
import math

FREQ_RX_HIGH = 1850
FREQ_RX_LOW = 1650
FREQ_TX_HIGH = 1180
FREQ_TX_LOW = 980
SAMPLE_RATE = 8000
MODULATION_RATE = 300
SAMPLES_PER_SYMBOL = SAMPLE_RATE / MODULATION_RATE

class V21(SynchronousProtocol):
    def __init__(self) -> None:
        super().__init__(8000, 800)
        self._rx_symbol_timing = 0.
        self._rx_symbol_buffer = ([0., 0.], [0., 0.])
        self._rx_bit_buffer = []
        self._tx_bit_buffer = []
        self._tx_symbol_timing = 0.
        self._tx_current_symbol = None
        self._tx_next_symbol = None
        
    def receive(self, buffer: bytes) -> None:
        super().receive(buffer)

        while len(self._receive_buffer) > 0:
            sample = self._receive_buffer.pop(0)
            
            if sample >= 128:
                sample = -256 + sample

            sample /= 128

            sample_timing = math.pi * 2 * self._rx_symbol_timing / MODULATION_RATE
            high_sample_timing = sample_timing * FREQ_RX_HIGH
            high_sample_i = math.cos(high_sample_timing) * sample
            high_sample_q = math.sin(high_sample_timing) * sample
            low_sample_timing = sample_timing * FREQ_RX_LOW
            low_sample_i = math.cos(low_sample_timing) * sample
            low_sample_q = math.sin(low_sample_timing) * sample

            self._rx_symbol_buffer[0][0] += high_sample_i
            self._rx_symbol_buffer[0][1] += high_sample_q
            self._rx_symbol_buffer[1][0] += low_sample_i
            self._rx_symbol_buffer[1][1] += low_sample_q

            self._rx_symbol_timing += 1 / SAMPLES_PER_SYMBOL

            if self._rx_symbol_timing >= 1:
                self._rx_symbol_timing -= 1

                # Compensate last sample
                last_sample_overtime = self._rx_symbol_timing * SAMPLES_PER_SYMBOL
                self._rx_symbol_buffer[0][0] -= last_sample_overtime * high_sample_i
                self._rx_symbol_buffer[0][1] -= last_sample_overtime * high_sample_q
                self._rx_symbol_buffer[1][0] -= last_sample_overtime * low_sample_i
                self._rx_symbol_buffer[1][1] -= last_sample_overtime * low_sample_q

                power_high = math.sqrt(sum(x ** 2 for x in self._rx_symbol_buffer[0]))
                power_low = math.sqrt(sum(x ** 2 for x in self._rx_symbol_buffer[1]))

                if power_high > power_low:
                    received_bit = 0
                
                else:
                    received_bit = 1

                if len(self._rx_bit_buffer) > 0 or received_bit == 0:
                    self._rx_bit_buffer.append(received_bit)

                    if len(self._rx_bit_buffer) == 10:
                        # Stop bit must be 1
                        if self._rx_bit_buffer[9] == 1:
                            value = 0

                            for i, n in enumerate(self._rx_bit_buffer[1:9]):
                                value += n * (2 ** i)
                        
                            self._receive_upper_protocol(bytes([value]))
                        
                        self._rx_bit_buffer.clear()

                # Compensate first sample
                self._rx_symbol_buffer[0][0] = last_sample_overtime * high_sample_i
                self._rx_symbol_buffer[0][1] = last_sample_overtime * high_sample_q
                self._rx_symbol_buffer[1][0] = last_sample_overtime * low_sample_i
                self._rx_symbol_buffer[1][1] = last_sample_overtime * low_sample_q
