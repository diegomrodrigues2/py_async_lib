import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

import asyncio
from py_async_lib import create_subprocess_exec


def test_subprocess_echo():
    async def run():
        proc = await create_subprocess_exec(
            "echo", "hello", stdout=asyncio.subprocess.PIPE
        )
        out, _ = await proc.communicate()
        assert out.strip() == b"hello"
        assert proc.returncode == 0

    asyncio.run(run())
