import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import pytest
import time

from py_async_lib.mini_loop import MiniLoop, sleep


def printer(name: str, delay: float, count: int):
    """Simple coroutine that prints a name and yields delays."""
    for i in range(count):
        print(f"{name}{i}")
        yield from sleep(delay)


def test_printers_interleave(capsys):
    """Printers with different delays should interleave their output."""
    loop = MiniLoop()
    loop.create_task(printer("A", 0.05, 2))
    loop.create_task(printer("B", 0.02, 2))
    loop.run()

    captured = capsys.readouterr().out.splitlines()
    assert captured == ["A0", "B0", "B1", "A1"]


def test_loop_exits_when_done(capsys):
    """The loop should finish with no pending tasks or timers."""
    loop = MiniLoop()
    loop.create_task(printer("A", 0.01, 1))
    loop.create_task(printer("B", 0.01, 1))
    loop.run()

    first_run = capsys.readouterr().out.splitlines()
    assert len(first_run) == 2
    assert not loop.ready
    assert not loop.timers

    loop.run()
    second_run = capsys.readouterr().out
    assert second_run == ""

def test_call_later_runs_after_delay():
    """call_later should schedule a coroutine after the given delay."""
    loop = MiniLoop()
    events = []

    def coro():
        events.append("done")
        if False:
            yield  # pragma: no cover

    loop.call_later(0.01, coro())
    start = time.time()
    loop.run()
    elapsed = time.time() - start
    assert events == ["done"]
    assert elapsed >= 0.01
