#ifndef CASYNCIO_LOOP_H
#define CASYNCIO_LOOP_H

#include <Python.h>
#include <sys/epoll.h>

typedef struct {
    PyObject *reader;
    PyObject *writer;
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
