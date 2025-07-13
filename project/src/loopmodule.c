#include "loop.h"
#include <structmember.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <pthread.h>

int
socket_write_now(int fd, OutBuf *ob)
{
    while (ob->pos < ob->len) {
        ssize_t n = send(fd, ob->data + ob->pos, ob->len - ob->pos,
                          MSG_NOSIGNAL);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 1; /* pending */
            return -1;     /* fatal */
        }
        ob->pos += n;
    }
    return 0; /* complete */
}

static OutBuf *
outbuf_new(void)
{
    OutBuf *ob = calloc(1, sizeof(OutBuf));
    if (!ob)
        return NULL;
    ob->waiters = PyList_New(0);
    if (!ob->waiters) {
        free(ob);
        return NULL;
    }
    return ob;
}

static int
loop_init(PyEventLoopObject *self, PyObject *args, PyObject *kwds)
{
    self->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (self->epfd == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    self->ready_q = PyList_New(0);
    if (!self->ready_q)
        return -1;
    self->timers = PyList_New(0);
    if (!self->timers)
        return -1;

    self->fdmap = NULL;
    self->fdcap = 0;

    sigemptyset(&self->sigmask);
    sigaddset(&self->sigmask, SIGINT);
    sigaddset(&self->sigmask, SIGTERM);
    sigaddset(&self->sigmask, SIGCHLD);
    pthread_sigmask(SIG_BLOCK, &self->sigmask, NULL);
    self->sfd = signalfd(-1, &self->sigmask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (self->sfd == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    self->signal_handlers = PyDict_New();
    if (!self->signal_handlers)
        return -1;

    struct epoll_event ev = {.events = EPOLLIN, .data.u32 = (uint32_t)self->sfd};
    if (epoll_ctl(self->epfd, EPOLL_CTL_ADD, self->sfd, &ev) == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    self->running = 0;

    return 0;
}

static void
loop_dealloc(PyEventLoopObject *self)
{
    if (self->epfd != -1)
        close(self->epfd);
    if (self->fdmap) {
        for (int i = 0; i < self->fdcap; i++) {
            FDCallback *slot = self->fdmap[i];
            if (!slot)
                continue;
            Py_XDECREF(slot->reader);
            Py_XDECREF(slot->writer);
            if (slot->obuf) {
                free(slot->obuf->data);
                Py_XDECREF(slot->obuf->waiters);
                free(slot->obuf);
            }
            free(slot);
        }
        free(self->fdmap);
    }
    if (self->sfd != -1)
        close(self->sfd);
    Py_XDECREF(self->signal_handlers);
    Py_XDECREF(self->ready_q);
    Py_XDECREF(self->timers);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int
ensure_fdslot(PyEventLoopObject *self, int fd)
{
    if (fd >= self->fdcap) {
        int newcap = self->fdcap ? self->fdcap : 8;
        while (newcap <= fd)
            newcap *= 2;
        FDCallback **newmap = realloc(self->fdmap, newcap * sizeof(FDCallback *));
        if (!newmap) {
            PyErr_NoMemory();
            return -1;
        }
        for (int i = self->fdcap; i < newcap; i++)
            newmap[i] = NULL;
        self->fdmap = newmap;
        self->fdcap = newcap;
    }
    if (!self->fdmap[fd]) {
        self->fdmap[fd] = calloc(1, sizeof(FDCallback));
        if (!self->fdmap[fd]) {
            PyErr_NoMemory();
            return -1;
        }
        self->fdmap[fd]->obuf = NULL;
    }
    return 0;
}

static PyObject *
loop_call_soon(PyEventLoopObject *self, PyObject *arg)
{
    if (PyList_Append(self->ready_q, arg) < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
loop_call_later(PyEventLoopObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *delay_obj, *cb;
    static char *kwlist[] = {"delay", "callback", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO:call_later", kwlist,
                                     &delay_obj, &cb))
        return NULL;
    double delay = PyFloat_AsDouble(delay_obj);
    if (PyErr_Occurred())
        return NULL;
    if (delay <= 0.0)
        return loop_call_soon(self, cb);
    PyErr_SetString(PyExc_NotImplementedError, "timers not implemented");
    return NULL;
}

static PyObject *
loop_create_task(PyEventLoopObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *coro;
    static char *kwlist[] = {"coro", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O:create_task", kwlist, &coro))
        return NULL;
    PyObject *tasks = PyImport_ImportModule("asyncio.tasks");
    if (!tasks)
        return NULL;
    PyObject *func = PyObject_GetAttrString(tasks, "create_task");
    Py_DECREF(tasks);
    if (!func)
        return NULL;
    PyObject *res = PyObject_CallFunction(func, "OO", coro, self);
    Py_DECREF(func);
    return res;
}

static PyObject *
loop_add_reader(PyEventLoopObject *self, PyObject *args)
{
    int fd;
    PyObject *cb;
    if (!PyArg_ParseTuple(args, "iO:add_reader", &fd, &cb))
        return NULL;
    if (!PyCallable_Check(cb)) {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return NULL;
    }
    if (ensure_fdslot(self, fd) < 0)
        return NULL;
    FDCallback *slot = self->fdmap[fd];
    int op = (slot->reader || slot->writer) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    uint32_t events = EPOLLET | EPOLLIN | (slot->writer ? EPOLLOUT : 0);
    struct epoll_event ev = {.events = events, .data.u32 = (uint32_t)fd};
    if (epoll_ctl(self->epfd, op, fd, &ev) == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Py_XINCREF(cb);
    Py_XDECREF(slot->reader);
    slot->reader = cb;
    Py_RETURN_NONE;
}

static PyObject *
loop_remove_reader(PyEventLoopObject *self, PyObject *arg)
{
    int fd = PyLong_AsLong(arg);
    if (fd == -1 && PyErr_Occurred())
        return NULL;
    if (fd >= self->fdcap || !self->fdmap[fd] || !self->fdmap[fd]->reader)
        Py_RETURN_FALSE;
    FDCallback *slot = self->fdmap[fd];
    Py_CLEAR(slot->reader);
    uint32_t events = EPOLLET | (slot->writer ? EPOLLOUT : 0);
    int op = events & (EPOLLIN | EPOLLOUT) ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    struct epoll_event ev = {.events = events, .data.u32 = (uint32_t)fd};
    if (epoll_ctl(self->epfd, op, fd, &ev) == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *
loop_add_writer(PyEventLoopObject *self, PyObject *args)
{
    int fd;
    PyObject *cb;
    if (!PyArg_ParseTuple(args, "iO:add_writer", &fd, &cb))
        return NULL;
    if (!PyCallable_Check(cb)) {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return NULL;
    }
    if (ensure_fdslot(self, fd) < 0)
        return NULL;
    FDCallback *slot = self->fdmap[fd];
    int op = (slot->reader || slot->writer) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    uint32_t events = EPOLLET | EPOLLOUT | (slot->reader ? EPOLLIN : 0);
    struct epoll_event ev = {.events = events, .data.u32 = (uint32_t)fd};
    if (epoll_ctl(self->epfd, op, fd, &ev) == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Py_XINCREF(cb);
    Py_XDECREF(slot->writer);
    slot->writer = cb;
    Py_RETURN_NONE;
}

static PyObject *
loop_remove_writer(PyEventLoopObject *self, PyObject *arg)
{
    int fd = PyLong_AsLong(arg);
    if (fd == -1 && PyErr_Occurred())
        return NULL;
    if (fd >= self->fdcap || !self->fdmap[fd] || !self->fdmap[fd]->writer)
        Py_RETURN_FALSE;
    FDCallback *slot = self->fdmap[fd];
    Py_CLEAR(slot->writer);
    uint32_t events = EPOLLET | (slot->reader ? EPOLLIN : 0);
    int op = events & (EPOLLIN | EPOLLOUT) ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    struct epoll_event ev = {.events = events, .data.u32 = (uint32_t)fd};
    if (epoll_ctl(self->epfd, op, fd, &ev) == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    Py_RETURN_TRUE;
}

static PyObject *
loop_add_signal_handler(PyEventLoopObject *self, PyObject *args)
{
    int signo;
    PyObject *cb;
    if (!PyArg_ParseTuple(args, "iO:add_signal_handler", &signo, &cb))
        return NULL;
    if (!PyCallable_Check(cb)) {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return NULL;
    }
    PyObject *key = PyLong_FromLong(signo);
    int r = PyDict_SetItem(self->signal_handlers, key, cb);
    Py_DECREF(key);
    if (r < 0)
        return NULL;
    Py_RETURN_NONE;
}

static PyObject *
loop_c_write(PyEventLoopObject *self, PyObject *args)
{
    int fd;
    Py_buffer buf;
    if (!PyArg_ParseTuple(args, "iy*:write", &fd, &buf))
        return NULL;
    if (ensure_fdslot(self, fd) < 0) {
        PyBuffer_Release(&buf);
        return NULL;
    }
    FDCallback *slot = self->fdmap[fd];
    if (!slot->obuf) {
        slot->obuf = outbuf_new();
        if (!slot->obuf) {
            PyBuffer_Release(&buf);
            return NULL;
        }
    }
    OutBuf *ob = slot->obuf;
    char *newdata = realloc(ob->data, ob->len + buf.len);
    if (!newdata) {
        PyBuffer_Release(&buf);
        PyErr_NoMemory();
        return NULL;
    }
    memcpy(newdata + ob->len, buf.buf, buf.len);
    ob->data = newdata;
    ob->len += buf.len;
    PyBuffer_Release(&buf);

    int res = socket_write_now(fd, ob);
    if (res == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }
    if (res == 1) {
        int op = (slot->reader || slot->writer) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
        uint32_t events = EPOLLET | EPOLLOUT | (slot->reader ? EPOLLIN : 0);
        struct epoll_event ev = {.events = events, .data.u32 = (uint32_t)fd};
        if (epoll_ctl(self->epfd, op, fd, &ev) == -1) {
            PyErr_SetFromErrno(PyExc_OSError);
            return NULL;
        }
    }
    Py_RETURN_NONE;
}

static PyObject *
loop_c_drain_waiter(PyEventLoopObject *self, PyObject *arg)
{
    int fd = PyLong_AsLong(arg);
    if (fd == -1 && PyErr_Occurred())
        return NULL;

    PyObject *asyncio = PyImport_ImportModule("asyncio");
    if (!asyncio)
        return NULL;
    PyObject *Future = PyObject_GetAttrString(asyncio, "Future");
    Py_DECREF(asyncio);
    if (!Future)
        return NULL;
    PyObject *fut = PyObject_CallNoArgs(Future);
    Py_DECREF(Future);
    if (!fut)
        return NULL;

    if (fd >= self->fdcap || !self->fdmap[fd] || !self->fdmap[fd]->obuf ||
        self->fdmap[fd]->obuf->pos >= self->fdmap[fd]->obuf->len) {
        PyObject *res = PyObject_CallMethod(fut, "set_result", "O", Py_None);
        Py_XDECREF(res);
        if (!res) {
            Py_DECREF(fut);
            return NULL;
        }
        return fut;
    }

    OutBuf *ob = self->fdmap[fd]->obuf;
    if (PyList_Append(ob->waiters, fut) < 0) {
        Py_DECREF(fut);
        return NULL;
    }
    return fut;
}

static PyObject *
loop_run_forever(PyEventLoopObject *self, PyObject *Py_UNUSED(ignored))
{
    self->running = 1;
    struct epoll_event evs[64];

    while (self->running) {
        while (PyList_Size(self->ready_q) > 0) {
            PyObject *callback = PyList_GetItem(self->ready_q, 0);
            if (!callback)
                return NULL;
            Py_INCREF(callback);
            if (PySequence_DelItem(self->ready_q, 0) < 0) {
                Py_DECREF(callback);
                return NULL;
            }

            PyObject *res = PyObject_CallNoArgs(callback);
            Py_DECREF(callback);
            if (!res)
                return NULL;
            Py_DECREF(res);
        }

        if (!self->running)
            break;

        int have_watchers = 0;
        for (int i = 0; i < self->fdcap; i++) {
            FDCallback *s = self->fdmap[i];
            if (s && (s->reader || s->writer)) {
                have_watchers = 1;
                break;
            }
        }
        if (!have_watchers)
            break;

        int n;
        Py_BEGIN_ALLOW_THREADS
        n = epoll_wait(self->epfd, evs, 64, -1);
        Py_END_ALLOW_THREADS
        if (n == -1) {
            PyErr_SetFromErrno(PyExc_OSError);
            self->running = 0;
            return NULL;
        }

        for (int i = 0; i < n; i++) {
            int fd = (int)evs[i].data.u32;
            if (fd == self->sfd) {
                struct signalfd_siginfo si;
                while (read(self->sfd, &si, sizeof(si)) == sizeof(si)) {
                    PyObject *key = PyLong_FromLong((long)si.ssi_signo);
                    PyObject *cb = PyDict_GetItemWithError(self->signal_handlers, key);
                    Py_DECREF(key);
                    if (cb) {
                        if (PyList_Append(self->ready_q, cb) < 0)
                            return NULL;
                    } else if (PyErr_Occurred()) {
                        return NULL;
                    }
                }
                continue;
            }
            if (fd >= self->fdcap)
                continue;
            FDCallback *slot = self->fdmap[fd];
            if (!slot)
                continue;
            if ((evs[i].events & EPOLLIN) && slot->reader) {
                if (PyList_Append(self->ready_q, slot->reader) < 0)
                    return NULL;
            }
            if (evs[i].events & EPOLLOUT) {
                if (slot->obuf) {
                    int r = socket_write_now(fd, slot->obuf);
                    if (r == -1) {
                        PyErr_SetFromErrno(PyExc_OSError);
                        return NULL;
                    }
                    if (slot->obuf->pos >= slot->obuf->len) {
                        free(slot->obuf->data);
                        slot->obuf->data = NULL;
                        slot->obuf->len = slot->obuf->pos = 0;

                        Py_ssize_t nw = PyList_Size(slot->obuf->waiters);
                        for (Py_ssize_t j = 0; j < nw; j++) {
                            PyObject *fut = PyList_GetItem(slot->obuf->waiters, j);
                            if (!fut)
                                return NULL;
                            PyObject *res = PyObject_CallMethod(fut, "set_result", "O", Py_None);
                            Py_XDECREF(res);
                        }
                        if (PyList_SetSlice(slot->obuf->waiters, 0, nw, NULL) < 0)
                            return NULL;

                        if (!slot->writer) {
                            uint32_t events = EPOLLET | (slot->reader ? EPOLLIN : 0);
                            int op = events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
                            struct epoll_event ev = {.events = events, .data.u32 = (uint32_t)fd};
                            if (epoll_ctl(self->epfd, op, fd, &ev) == -1) {
                                PyErr_SetFromErrno(PyExc_OSError);
                                return NULL;
                            }
                        }
                    }
                }
                if (slot->writer) {
                    if (PyList_Append(self->ready_q, slot->writer) < 0)
                        return NULL;
                }
            }
        }
    }

    self->running = 0;
    Py_RETURN_NONE;
}

static PyObject *
loop_stop(PyEventLoopObject *self, PyObject *Py_UNUSED(ignored))
{
    self->running = 0;
    Py_RETURN_NONE;
}

static PyMethodDef loop_methods[] = {
    {"call_soon", (PyCFunction)loop_call_soon, METH_O,
     PyDoc_STR("Schedule a callback to run soon")},
    {"call_later", (PyCFunction)(PyCFunctionWithKeywords)loop_call_later,
     METH_VARARGS | METH_KEYWORDS,
     PyDoc_STR("Schedule a callback to run after a delay")},
    {"create_task", (PyCFunction)(PyCFunctionWithKeywords)loop_create_task,
     METH_VARARGS | METH_KEYWORDS,
     PyDoc_STR("Create a Task object")},
    {"add_reader", (PyCFunction)loop_add_reader, METH_VARARGS,
     PyDoc_STR("Register a reader callback for a file descriptor")},
    {"remove_reader", (PyCFunction)loop_remove_reader, METH_O,
     PyDoc_STR("Remove reader callback for a file descriptor")},
    {"add_writer", (PyCFunction)loop_add_writer, METH_VARARGS,
     PyDoc_STR("Register a writer callback for a file descriptor")},
    {"remove_writer", (PyCFunction)loop_remove_writer, METH_O,
     PyDoc_STR("Remove writer callback for a file descriptor")},
    {"add_signal_handler", (PyCFunction)loop_add_signal_handler, METH_VARARGS,
     PyDoc_STR("Register a callback for a signal")},
    {"_c_write", (PyCFunction)loop_c_write, METH_VARARGS,
     PyDoc_STR("Low level write with buffering")},
    {"_c_drain_waiter", (PyCFunction)loop_c_drain_waiter, METH_O,
     PyDoc_STR("Return Future resolved when buffer drained")},
    {"run_forever", (PyCFunction)loop_run_forever, METH_NOARGS,
     PyDoc_STR("Run callbacks until queue is empty")},
    {"stop", (PyCFunction)loop_stop, METH_NOARGS,
     PyDoc_STR("Stop the running loop")},
    {NULL, NULL, 0, NULL},
};

static PyTypeObject PyEventLoop_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "casyncio.EventLoop",
    .tp_basicsize = sizeof(PyEventLoopObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)loop_init,
    .tp_dealloc = (destructor)loop_dealloc,
    .tp_methods = loop_methods,
};

static PyMethodDef casyncio_methods[] = {
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef casyncio_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "casyncio",
    .m_size = -1,
    .m_methods = casyncio_methods,
};

PyMODINIT_FUNC
PyInit_casyncio(void)
{
    PyObject *m;
    if (PyType_Ready(&PyEventLoop_Type) < 0)
        return NULL;

    m = PyModule_Create(&casyncio_module);
    if (!m)
        return NULL;

    Py_INCREF(&PyEventLoop_Type);
    if (PyModule_AddObject(m, "EventLoop", (PyObject *)&PyEventLoop_Type) < 0) {
        Py_DECREF(&PyEventLoop_Type);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
