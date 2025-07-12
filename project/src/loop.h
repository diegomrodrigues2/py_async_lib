#ifndef CASYNCIO_LOOP_H
#define CASYNCIO_LOOP_H

#include <Python.h>
#include <sys/epoll.h>

typedef struct {
    PyObject_HEAD
    int epfd;
    PyObject *ready_q;
    PyObject *timers;
    int running;
} PyEventLoopObject;

#endif // CASYNCIO_LOOP_H
