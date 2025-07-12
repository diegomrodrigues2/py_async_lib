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

static PyObject *
loop_call_soon(PyEventLoopObject *self, PyObject *args)
{
    if (PyTuple_Size(args) < 1) {
        PyErr_SetString(PyExc_TypeError, "call_soon requires a callback");
        return NULL;
    }

    PyObject *callback = PyTuple_GET_ITEM(args, 0);
    PyObject *cb_args = PyTuple_GetSlice(args, 1, PyTuple_Size(args));
    if (!cb_args)
        return NULL;

    PyObject *entry = PyTuple_Pack(2, callback, cb_args);
    Py_DECREF(cb_args);
    if (!entry)
        return NULL;

    if (PyList_Append(self->ready_q, entry) < 0) {
        Py_DECREF(entry);
        return NULL;
    }
    Py_DECREF(entry);
    Py_RETURN_NONE;
}

static PyObject *
loop_run_forever(PyEventLoopObject *self, PyObject *Py_UNUSED(ignored))
{
    while (PyList_Size(self->ready_q) > 0) {
        PyObject *entry = PyList_GetItem(self->ready_q, 0); /* borrowed */
        if (!entry)
            return NULL;
        Py_INCREF(entry);
        if (PySequence_DelItem(self->ready_q, 0) < 0) {
            Py_DECREF(entry);
            return NULL;
        }

        PyObject *callback = PyTuple_GET_ITEM(entry, 0);
        PyObject *cb_args = PyTuple_GET_ITEM(entry, 1);
        PyObject *result = PyObject_CallObject(callback, cb_args);
        Py_DECREF(entry);
        if (!result)
            return NULL;
        Py_DECREF(result);
    }

    Py_RETURN_NONE;
}

static PyMethodDef eventloop_methods[] = {
    {"call_soon", (PyCFunction)loop_call_soon, METH_VARARGS,
     PyDoc_STR("Schedule a callback to run soon")},
    {"run_forever", (PyCFunction)loop_run_forever, METH_NOARGS,
     PyDoc_STR("Run callbacks until none remain")},
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
    .tp_methods = eventloop_methods,
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
