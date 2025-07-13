import os

def test_massif_script_exists():
    assert os.path.exists('scripts/massif.sh')
