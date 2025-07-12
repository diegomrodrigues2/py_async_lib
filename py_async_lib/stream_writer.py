class StreamWriter:
    """Simple wrapper around low-level write APIs."""

    def __init__(self, loop, fd):
        self._loop = loop
        self._fd = fd

    def write(self, data: bytes) -> None:
        self._loop._c_write(self._fd, data)

    async def drain(self):
        fut = self._loop._c_drain_waiter(self._fd)
        await fut
