import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

import casyncio
import gc
import socket
from py_async_lib.stream_writer import StreamWriter


def test_create_and_destroy():
    loop = casyncio.EventLoop()
    assert repr(loop).startswith('<casyncio.EventLoop object')
    del loop
    gc.collect()


def test_call_soon_and_run_forever():
    loop = casyncio.EventLoop()
    result = []

    def cb():
        result.append(1)

    loop.call_soon(cb)
    loop.run_forever()

    assert result == [1]


def test_multiple_callbacks():
    loop = casyncio.EventLoop()
    order = []

    loop.call_soon(lambda: order.append('a'))
    loop.call_soon(lambda: order.append('b'))
    loop.run_forever()

    assert order == ['a', 'b']


def test_streamwriter_write_and_drain():
    loop = casyncio.EventLoop()
    r, w = socket.socketpair()
    w.setblocking(False)

    writer = StreamWriter(loop, w.fileno())
    writer.write(b'hi')
    fut = loop._c_drain_waiter(w.fileno())
    assert fut.done()

    r.close()
    w.close()
