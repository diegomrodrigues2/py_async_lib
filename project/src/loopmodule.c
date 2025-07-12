#include "loop.h"
#include <structmember.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

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
            if (fd >= self->fdcap)
                continue;
            FDCallback *slot = self->fdmap[fd];
            if (!slot)
                continue;
            if ((evs[i].events & EPOLLIN) && slot->reader) {
                if (PyList_Append(self->ready_q, slot->reader) < 0)
                    return NULL;
            }
            if ((evs[i].events & EPOLLOUT) && slot->writer) {
                if (PyList_Append(self->ready_q, slot->writer) < 0)
                    return NULL;
            }
        }
    }

    self->running = 0;
    Py_RETURN_NONE;
}

static PyMethodDef loop_methods[] = {
    {"call_soon", (PyCFunction)loop_call_soon, METH_O,
     PyDoc_STR("Schedule a callback to run soon")},
    {"add_reader", (PyCFunction)loop_add_reader, METH_VARARGS,
     PyDoc_STR("Register a reader callback for a file descriptor")},
    {"remove_reader", (PyCFunction)loop_remove_reader, METH_O,
     PyDoc_STR("Remove reader callback for a file descriptor")},
    {"add_writer", (PyCFunction)loop_add_writer, METH_VARARGS,
     PyDoc_STR("Register a writer callback for a file descriptor")},
    {"remove_writer", (PyCFunction)loop_remove_writer, METH_O,
     PyDoc_STR("Remove writer callback for a file descriptor")},
    {"run_forever", (PyCFunction)loop_run_forever, METH_NOARGS,
     PyDoc_STR("Run callbacks until queue is empty")},
    {NULL, NULL, 0, NULL}
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
