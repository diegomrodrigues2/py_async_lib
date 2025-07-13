import asyncio
import casyncio

class _CAsyncioPolicy(asyncio.DefaultEventLoopPolicy):
    """EventLoopPolicy that creates casyncio.EventLoop instances."""

    def _loop_factory(self):
        return casyncio.EventLoop()
