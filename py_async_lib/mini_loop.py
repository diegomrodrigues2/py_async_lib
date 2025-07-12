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


def sleep(delay):
    """Coroutine that yields control back to the loop for ``delay`` seconds."""
    yield delay
