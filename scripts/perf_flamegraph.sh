#!/bin/sh
# Generate a perf-based flamegraph for the echo benchmark

set -e
perf record -F 99 -g -- python examples/printer.py
perf script | stackcollapse-perf.pl > out.folded
flamegraph.pl out.folded > flamegraph.svg
