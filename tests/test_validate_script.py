import os


def test_validate_script_exists():
    assert os.path.exists('benchmarks/validate.py')
