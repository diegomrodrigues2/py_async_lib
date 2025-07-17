import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import casyncio
from py_async_lib import SocketTransport, BaseProtocol

class EchoProtocol(BaseProtocol):
    def __init__(self, loop):
        self.loop = loop
        self.data = []

    def connection_made(self, transport):
        self.transport = transport

    def data_received(self, data):
        self.data.append(data)
        self.loop.call_soon(self.loop.stop)

def test_socket_transport():
    loop = casyncio.EventLoop()
    import socket
    r, w = socket.socketpair()
    r.setblocking(False)
    w.setblocking(False)

    prot = EchoProtocol(loop)
    transport = SocketTransport(loop, r, prot)
    w.send(b'hi')
    loop.run_forever()
    assert prot.data == [b'hi']
    w.close(); r.close()

