import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import casyncio
import socket
import asyncio
from py_async_lib import StreamReader, StreamWriter


def test_stream_reader_writer():
    loop = casyncio.EventLoop()
    r, w = socket.socketpair()
    r.setblocking(False)
    w.setblocking(False)

    reader = StreamReader(loop, r.fileno())
    writer = StreamWriter(loop, w.fileno())

    writer.write(b"hello\nworld")
    loop.call_later(0.01, loop.stop)
    loop.run_forever()

    async def consume():
        line = await reader.readline()
        rest = await reader.read()
        return line, rest

    line, rest = asyncio.run(consume())
    assert line == b"hello\n"
    assert rest == b"world"

    r.close(); w.close()


