#!/bin/sh
# Run valgrind massif on the timer benchmark

set -e
valgrind --tool=massif python -m benchmarks.throughput
