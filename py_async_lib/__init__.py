"""Minimal asynchronous library utilities."""

from .stream_writer import StreamWriter
from .subprocess import Subprocess, create_subprocess_exec
from .policy import _CAsyncioPolicy

def install() -> None:
    """Install the `_CAsyncioPolicy` as the default asyncio policy."""
    import asyncio

    asyncio.set_event_loop_policy(_CAsyncioPolicy())

__all__ = [
    "StreamWriter",
    "Subprocess",
    "create_subprocess_exec",
    "install",
]
