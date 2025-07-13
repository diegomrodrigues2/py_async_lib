import time
import asyncio
import casyncio


def bench_casyncio(iterations: int = 1000) -> float:
    """Run the workload on the casyncio event loop."""
    loop = casyncio.EventLoop()
    count = 0

    def cb():
        nonlocal count
        count += 1
        if count < iterations:
            loop.call_soon(cb)
        else:
            loop.stop()

    loop.call_soon(cb)
    start = time.time()
    loop.run_forever()
    return time.time() - start


def bench_asyncio(iterations: int = 1000) -> float:
    """Run the workload on Python's default event loop."""
    loop = asyncio.new_event_loop()
    count = 0

    def cb():
        nonlocal count
        count += 1
        if count < iterations:
            loop.call_soon(cb)
        else:
            loop.stop()

    loop.call_soon(cb)
    start = time.time()
    loop.run_forever()
    loop.close()
    return time.time() - start


def bench(iterations: int = 1000) -> tuple[float, float]:
    """Return runtimes for (casyncio, asyncio)."""
    return bench_casyncio(iterations), bench_asyncio(iterations)


if __name__ == "__main__":
    ctime, atime = bench()
    print(f"casyncio: {ctime:.6f}")
    print(f"asyncio: {atime:.6f}")
