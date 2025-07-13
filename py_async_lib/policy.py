import asyncio

try:
    import casyncio
except ModuleNotFoundError:  # pragma: no cover - optional C extension
    casyncio = None

class _CAsyncioPolicy(asyncio.DefaultEventLoopPolicy):
    """EventLoopPolicy that creates casyncio.EventLoop instances."""

    def _loop_factory(self):
        if casyncio is None:
            raise RuntimeError("casyncio extension is not available")
        return casyncio.EventLoop()
