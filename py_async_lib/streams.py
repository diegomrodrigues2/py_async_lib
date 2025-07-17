import os
import asyncio

class StreamReader:
    """Minimal asynchronous stream reader."""

    def __init__(self, loop, fd: int):
        self._loop = loop
        self._fd = fd
        self._buffer = bytearray()
        self._eof = False
        self._waiter = None
        loop.add_reader(fd, self._on_ready)

    def _wakeup(self):
        if self._waiter is not None and not self._waiter.done():
            self._waiter.set_result(True)
            self._waiter = None

    def _on_ready(self):
        data = os.read(self._fd, 4096)
        if not data:
            self._eof = True
            self._loop.remove_reader(self._fd)
        else:
            self._buffer.extend(data)
        self._wakeup()

    async def read(self, n: int = -1) -> bytes:
        while not self._buffer and not self._eof:
            self._waiter = asyncio.Future()
            await self._waiter
        if n == -1 or n >= len(self._buffer):
            data = bytes(self._buffer)
            self._buffer.clear()
        else:
            data = bytes(self._buffer[:n])
            del self._buffer[:n]
        return data

    async def readline(self) -> bytes:
        while True:
            newline = self._buffer.find(b"\n")
            if newline != -1:
                newline += 1
                data = bytes(self._buffer[:newline])
                del self._buffer[:newline]
                return data
            if self._eof:
                data = bytes(self._buffer)
                self._buffer.clear()
                return data
            self._waiter = asyncio.Future()
            await self._waiter

