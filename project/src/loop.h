#ifndef CASYNCIO_LOOP_H
#define CASYNCIO_LOOP_H

#include <Python.h>
#include <sys/epoll.h>
#include <stdint.h>

typedef struct {
    char *data;
    Py_ssize_t len;
    Py_ssize_t pos;
    PyObject *waiters;
} OutBuf;

#define INITIAL_TIMER_CAPACITY 64

typedef struct {
    int64_t deadline_ns;
    PyObject *callback;
    int canceled;
    int heap_index;
} TimerNode;

typedef struct {
    PyObject *reader;
    PyObject *writer;
    OutBuf *obuf;
} FDCallback;

int socket_write_now(int fd, OutBuf *ob);

typedef struct {
    PyObject_HEAD
    int epfd;
    PyObject *ready_q;
    TimerNode **timer_heap;
    size_t timer_count;
    size_t timer_capacity;
    FDCallback **fdmap;
    int fdcap;
    sigset_t sigmask;
    int sfd;
    PyObject *signal_handlers;
    int running;
    int aw_rfd;
    int aw_wfd;
} PyEventLoopObject;

#endif // CASYNCIO_LOOP_H
