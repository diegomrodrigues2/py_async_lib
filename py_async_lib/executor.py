import asyncio
from concurrent.futures import ThreadPoolExecutor
from typing import Any, Callable, Optional

_DEFAULT_EXECUTOR: Optional[ThreadPoolExecutor] = None


def run_in_executor(loop, func: Callable[..., Any], *args: Any, executor: Optional[ThreadPoolExecutor] = None):
    """Execute *func* in a thread and return an asyncio Future."""
    global _DEFAULT_EXECUTOR
    if executor is None:
        if _DEFAULT_EXECUTOR is None:
            _DEFAULT_EXECUTOR = ThreadPoolExecutor()
        executor = _DEFAULT_EXECUTOR

    fut = asyncio.Future()

    def _work():
        try:
            res = func(*args)
        except Exception as exc:  # pragma: no cover - error path
            loop.call_soon_threadsafe(lambda: fut.set_exception(exc))
        else:
            loop.call_soon_threadsafe(lambda: fut.set_result(res))

    executor.submit(_work)
    return fut
