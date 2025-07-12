import casyncio
import gc


def test_create_and_destroy():
    loop = casyncio.EventLoop()
    assert repr(loop).startswith('<casyncio.EventLoop object')
    del loop
    gc.collect()
