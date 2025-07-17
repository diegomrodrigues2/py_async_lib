import socket
from .executor import run_in_executor

def async_getaddrinfo(
    loop,
    host: str,
    port: int,
    family: int = 0,
    type: int = 0,
    proto: int = 0,
    flags: int = 0,
):
    """Resolve host asynchronously using run_in_executor."""
    return run_in_executor(
        loop, socket.getaddrinfo, host, port, family, type, proto, flags
    )
