from py_async_lib.mini_loop import MiniLoop, sleep


def printer(name, delay, count):
    """Coroutine that prints ``name`` every ``delay`` seconds for ``count`` iterations."""
    for _ in range(count):
        print(name)
        # Yield control back to the loop for the specified delay
        yield from sleep(delay)


if __name__ == "__main__":
    loop = MiniLoop()
    loop.create_task(printer("A", 1, 3))
    loop.create_task(printer("B", 0.5, 5))
    loop.run()
