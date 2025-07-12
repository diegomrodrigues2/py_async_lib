#include "loop.h"
#include <structmember.h>
#include <sys/epoll.h>

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

    return 0;
}

static void
loop_dealloc(PyEventLoopObject *self)
{
    if (self->epfd != -1)
        close(self->epfd);
    Py_XDECREF(self->ready_q);
    Py_XDECREF(self->timers);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyTypeObject PyEventLoop_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "casyncio.EventLoop",
    .tp_basicsize = sizeof(PyEventLoopObject),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)loop_init,
    .tp_dealloc = (destructor)loop_dealloc,
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
