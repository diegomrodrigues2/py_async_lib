# py_async_lib
My own custom implementation of the Ansycio library from scratch using C Wrappers and Python

## Example

Run the small loop demo in `examples/printer.py`:

```bash
PYTHONPATH=. python examples/printer.py
```

To use the high performance C loop with asyncio:

```python
import py_async_lib

py_async_lib.install()
```

### Development progress

The C event loop now includes an `OutBuf` structure for managing pending writes.
Signalfd integrated
