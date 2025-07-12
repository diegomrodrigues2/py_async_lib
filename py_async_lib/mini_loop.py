from collections import deque
import heapq
import time

class MiniLoop:
    """A minimal event loop implementation."""

    def __init__(self):
        # Queue for ready-to-run callbacks
        self.ready = deque()
        # Timers managed as a min-heap of (when, callback) tuples
        self.timers = []

    def create_task(self, coro):
        """Schedule a coroutine to be run on the next loop iteration."""
        self.ready.append(coro)

    def call_later(self, delay, coro):
        """Schedule a coroutine to be run after a given delay."""
        when = time.time() + delay
        heapq.heappush(self.timers, (when, coro))

    def run(self):
        """Run tasks and timers until none remain."""
        while self.ready or self.timers:
            if not self.ready:
                when, coro = heapq.heappop(self.timers)
                delay = when - time.time()
                if delay > 0:
                    time.sleep(delay)
                self.ready.append(coro)

            coro = self.ready.popleft()
            try:
                result = coro.send(None)
            except StopIteration:
                continue

            if isinstance(result, (int, float)):
                self.call_later(result, coro)
            else:
                if result is not None:
                    self.create_task(result)
                self.ready.append(coro)


def sleep(delay):
    """Coroutine that yields control back to the loop for ``delay`` seconds."""
    yield delay
