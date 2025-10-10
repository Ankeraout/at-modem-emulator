class DHCP:
    def __init__(self, network: bytes, mask: bytes) -> None:
        self._network = network
        self._mask_size = 0
        self._mask = mask

        for byte in mask:
            for bit in range(8):
                if (byte & (1 << (7 - bit))) != 0:
                    self._mask_size += 1

        self._allocated_ips = [False] * (2 ** (32 - self._mask_size))
        
        if self._mask_size < 31:
            self._allocated_ips[0] = True
            self._allocated_ips[-1] = True

    def allocate_ip(self) -> bytes:
        for i in range(2 ** (32 - self._mask_size)):
            if not self._allocated_ips[i]:
                self._allocated_ips[i] = True
                index_bytes = i.to_bytes(4)
                return bytes(
                    ((self._network[j] & self._mask[j]) | index_bytes[j])
                    for j in range(4)
                )

    def free_ip(self, ip: bytes) -> None:
        index_bytes = bytes(ip[i] & ~self._mask[i] for i in range(4))
        index = int.from_bytes(index_bytes)
        self._allocated_ips[index] = False
