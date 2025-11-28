from abc import ABC, abstractmethod
from enum import Enum
import struct

class SampleType(Enum):
    PCM_U8 = 10
    PCM_FLOAT = 80

class SampleConverter(ABC):
    def __init__(self):
        pass

    @abstractmethod
    def convert(self, input: list, output: list) -> None:
        pass

class SampleConverter_Passive(SampleConverter):
    def __init__(self):
        super().__init__()

    def convert(self, input: list, output: list) -> None:
        output.extend(input)

class SampleConverter_U8_FLOAT(SampleConverter):
    def __init__(self):
        super().__init__()

    def convert(self, input: list, output: list) -> None:
        for sample in input:
            output.append((sample / 127.5) - 1.)

class SampleConverter_FLOAT_U8(SampleConverter):
    def __init__(self):
        super().__init__()

    def convert(self, input: list, output: list) -> None:
        for sample in input:
            output.append(int((sample + 1.) * 127.5))
