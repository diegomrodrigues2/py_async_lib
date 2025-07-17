import casyncio
import socket
import casyncio
from py_async_lib import run_in_executor, async_getaddrinfo


def test_run_in_executor_executes():
    loop = casyncio.EventLoop()
    results = []

    fut = run_in_executor(loop, lambda: 40 + 2)
    r, w = socket.socketpair()
    r.setblocking(False)
    loop.add_reader(r.fileno(), lambda: None)

    def check():
        if fut.done():
            results.append(fut.result())
            loop.remove_reader(r.fileno())
            r.close(); w.close()
            loop.stop()
        else:
            loop.call_later(0.01, check)

    loop.call_soon(check)
    loop.run_forever()

    assert results == [42]


def test_async_getaddrinfo():
    loop = casyncio.EventLoop()
    results = []

    fut = async_getaddrinfo(loop, "localhost", 80)
    r, w = socket.socketpair()
    r.setblocking(False)
    loop.add_reader(r.fileno(), lambda: None)

    def check():
        if fut.done():
            results.append(fut.result())
            loop.remove_reader(r.fileno())
            r.close(); w.close()
            loop.stop()
        else:
            loop.call_later(0.01, check)

    loop.call_soon(check)
    loop.run_forever()

    assert results and len(results[0]) > 0
