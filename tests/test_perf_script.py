import os


def test_perf_script_exists():
    assert os.path.exists('scripts/perf_flamegraph.sh')
