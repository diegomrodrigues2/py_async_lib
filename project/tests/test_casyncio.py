import casyncio
import gc


def test_create_and_destroy():
    loop = casyncio.EventLoop()
    assert repr(loop).startswith('<casyncio.EventLoop object')
    del loop
    gc.collect()


def _append(lst, value):
    lst.append(value)


def test_call_soon_and_run_forever():
    loop = casyncio.EventLoop()
    results = []

    loop.call_soon(_append, results, "a")
    loop.call_soon(_append, results, "b")

    loop.run_forever()

    assert results == ["a", "b"]
