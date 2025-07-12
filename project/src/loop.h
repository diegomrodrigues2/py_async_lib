#ifndef CASYNCIO_LOOP_H
#define CASYNCIO_LOOP_H

#include <Python.h>
#include <sys/epoll.h>

typedef struct {
    char *data;
    Py_ssize_t len;
    Py_ssize_t pos;
    PyObject *waiters;
} OutBuf;

typedef struct {
    PyObject *reader;
    PyObject *writer;
    OutBuf *obuf;
} FDCallback;

typedef struct {
    PyObject_HEAD
    int epfd;
    PyObject *ready_q;
    PyObject *timers;
    FDCallback **fdmap;
    int fdcap;
    int running;
} PyEventLoopObject;

#endif // CASYNCIO_LOOP_H
