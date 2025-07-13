import time
from py_async_lib.mini_loop import MiniLoop


def spin(iterations: int):
    for _ in range(iterations):
        yield 0


def bench(iterations: int = 1000) -> float:
    loop = MiniLoop()
    loop.create_task(spin(iterations))
    start = time.time()
    loop.run()
    return time.time() - start


if __name__ == "__main__":
    dur = bench()
    print(f"{dur:.6f}")
