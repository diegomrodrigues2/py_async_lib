from collections import deque

class MiniLoop:
    """A minimal event loop implementation."""

    def __init__(self):
        # Queue for ready-to-run callbacks
        self.ready = deque()
        # Timers managed as a min-heap of (when, callback) tuples
        self.timers = []
