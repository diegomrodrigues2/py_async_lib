#!/bin/sh
# Run valgrind massif on the timer benchmark

set -e
valgrind --tool=massif python examples/printer.py
