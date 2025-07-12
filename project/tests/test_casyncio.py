import casyncio
import gc


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
