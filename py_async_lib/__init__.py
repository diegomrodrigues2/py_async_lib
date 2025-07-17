"""Minimal asynchronous library utilities."""

from .stream_writer import StreamWriter
from .streams import StreamReader
from .subprocess import Subprocess, create_subprocess_exec
from .policy import _CAsyncioPolicy
from .timer_handle import TimerHandle
from .executor import run_in_executor
from .dns import async_getaddrinfo
from .highlevel import open_connection, start_server
from .transports import BaseProtocol, SocketTransport

def install() -> None:
    """Install the `_CAsyncioPolicy` as the default asyncio policy."""
    import asyncio

    asyncio.set_event_loop_policy(_CAsyncioPolicy())

__all__ = [
    "StreamWriter",
    "StreamReader",
    "Subprocess",
    "create_subprocess_exec",
    "install",
    "TimerHandle",
    "run_in_executor",
    "async_getaddrinfo",
    "open_connection",
    "start_server",
    "BaseProtocol",
    "SocketTransport",
]
