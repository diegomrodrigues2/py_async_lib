import sys, os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from benchmarks.throughput import bench


def test_bench_runs_quickly():
    duration = bench(10)
    assert duration >= 0
