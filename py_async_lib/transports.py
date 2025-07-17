import os
import asyncio

class BaseProtocol:
    def connection_made(self, transport):
        pass

    def data_received(self, data: bytes):
        pass

    def connection_lost(self, exc):
        pass

class SocketTransport:
    def __init__(self, loop, sock, protocol):
        self._loop = loop
        self._sock = sock
        self._protocol = protocol
        sock.setblocking(False)
        loop.add_reader(sock.fileno(), self._on_read)
        protocol.connection_made(self)

    def _on_read(self):
        data = os.read(self._sock.fileno(), 4096)
        if not data:
            self.close()
        else:
            self._protocol.data_received(data)

    def write(self, data: bytes):
        self._loop._c_write(self._sock.fileno(), data)

    def close(self):
        self._loop.remove_reader(self._sock.fileno())
        self._sock.close()
        self._protocol.connection_lost(None)

