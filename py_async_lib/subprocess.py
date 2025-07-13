import asyncio

class Subprocess:
    def __init__(self, proc):
        self._proc = proc

    async def communicate(self):
        return await self._proc.communicate()

    @property
    def returncode(self):
        return self._proc.returncode

async def create_subprocess_exec(*cmd, **kwargs):
    proc = await asyncio.create_subprocess_exec(*cmd, **kwargs)
    return Subprocess(proc)
