import ctypes

_py_get_ptr = ctypes.pythonapi.PyCapsule_GetPointer
_py_get_ptr.restype = ctypes.c_void_p
_py_get_ptr.argtypes = [ctypes.py_object, ctypes.c_char_p]

class _CNode(ctypes.Structure):
    _fields_ = [
        ("deadline_ns", ctypes.c_longlong),
        ("callback", ctypes.py_object),
        ("canceled", ctypes.c_int),
        ("heap_index", ctypes.c_int),
    ]

class TimerHandle:
    """Handle returned by call_later to allow cancellation."""

    def __init__(self, capsule):
        self._capsule = capsule

    def cancel(self):
        ptr = _py_get_ptr(self._capsule, b"TimerNode")
        if ptr:
            node = ctypes.cast(ptr, ctypes.POINTER(_CNode))
            node.contents.canceled = 1

