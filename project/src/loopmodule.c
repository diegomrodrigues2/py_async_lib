#include "loop.h"
#include <structmember.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>

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
        PyErr_SetString(PyExc_RuntimeError, "fdslot not allocated");
        return -1;
    }
    if (!self->fdmap[fd]) {
        self->fdmap[fd] = calloc(1, sizeof(FDCallback));
        if (!self->fdmap[fd]) {
            PyErr_NoMemory();
            return -1;
        }
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
    Py_CLEAR(self->fdmap[fd]->reader);
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
    Py_CLEAR(self->fdmap[fd]->writer);
    Py_RETURN_TRUE;
}

static PyObject *
loop_run_forever(PyEventLoopObject *self, PyObject *Py_UNUSED(ignored))
{
    self->running = 1;

    while (self->running && PyList_Size(self->ready_q) > 0) {
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

    self->running = 0;
    Py_RETURN_NONE;
}

static PyMethodDef loop_methods[] = {
    {"call_soon", (PyCFunction)loop_call_soon, METH_O,
     PyDoc_STR("Schedule a callback to run soon")},
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
