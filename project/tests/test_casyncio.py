import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

import casyncio
import gc
import signal
import asyncio
import socket
from py_async_lib.stream_writer import StreamWriter
from py_async_lib import install


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

def test_add_reader_is_called():
    loop = casyncio.EventLoop()
    r, w = socket.socketpair()
    r.setblocking(False)
    w.setblocking(False)
    results = []

    def reader():
        data = r.recv(1)
        results.append(data)
        loop.stop()

    loop.add_reader(r.fileno(), reader)
    w.send(b'X')
    loop.run_forever()

    assert results == [b'X']
    r.close()
    w.close()


def test_signal_handler():
    loop = casyncio.EventLoop()
    called = []

    def handler():
        called.append(True)
        loop.stop()

    loop.add_signal_handler(signal.SIGTERM, handler)
    # Add a dummy reader so the loop waits for epoll events
    r, w = socket.socketpair()
    loop.add_reader(r.fileno(), lambda: None)

    os.kill(os.getpid(), signal.SIGTERM)
    loop.run_forever()

    loop.remove_reader(r.fileno())
    r.close()
    w.close()

    assert called == [True]


def test_install_sets_event_loop_policy():
    old = asyncio.get_event_loop_policy()
    install()
    try:
        loop = asyncio.new_event_loop()
        assert isinstance(loop, casyncio.EventLoop)
    finally:
        asyncio.set_event_loop_policy(old)

