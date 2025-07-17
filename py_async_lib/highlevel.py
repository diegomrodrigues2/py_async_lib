import socket
import asyncio
from .streams import StreamReader
from .stream_writer import StreamWriter
from .dns import async_getaddrinfo

async def open_connection(host, port, *, loop=None):
    if loop is None:
        loop = asyncio.get_event_loop()
    infos = await async_getaddrinfo(loop, host, port)
    family, type_, proto, _, sockaddr = infos[0]
    sock = socket.socket(family, type_, proto)
    sock.setblocking(False)
    try:
        sock.connect(sockaddr)
    except BlockingIOError:
        pass

    fut = asyncio.Future()
    def on_connected():
        err = sock.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
        if err != 0:
            fut.set_exception(OSError(err, os.strerror(err)))
        else:
            fut.set_result(None)
        loop.remove_writer(sock.fileno())
    loop.add_writer(sock.fileno(), on_connected)
    await fut
    reader = StreamReader(loop, sock.fileno())
    writer = StreamWriter(loop, sock.fileno())
    return reader, writer

async def start_server(client_connected_cb, host, port, *, loop=None, backlog=100):
    if loop is None:
        loop = asyncio.get_event_loop()
    srv_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv_sock.bind((host, port))
    srv_sock.listen(backlog)
    srv_sock.setblocking(False)

    def accept():
        try:
            client, _ = srv_sock.accept()
        except BlockingIOError:
            return
        client.setblocking(False)
        reader = StreamReader(loop, client.fileno())
        writer = StreamWriter(loop, client.fileno())
        res = client_connected_cb(reader, writer)
        if asyncio.iscoroutine(res):
            # best-effort: run coroutine to completion
            async def runner():
                await res
            loop.call_soon(lambda: asyncio.run(runner()))
    loop.add_reader(srv_sock.fileno(), accept)

    async def close():
        loop.remove_reader(srv_sock.fileno())
        srv_sock.close()

    return close, srv_sock.getsockname()[1]

