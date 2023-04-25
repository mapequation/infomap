from infomap import Infomap
import re
from pathlib import Path
import filecmp
import pytest


def test_precomputed_requirements():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("examples/networks/twotriangles.net")
    with pytest.raises(RuntimeError, match=re.escape("Missing node flow in input data. Should be passed as a third field under a *Vertices section.")):
        im.run()


def test_precomputed_states_requirements():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("examples/networks/states.net")
    with pytest.raises(RuntimeError, match=re.escape("Missing node flow in input data. Should be passed as a third field under a *States section.")):
        im.run()


def test_precomputed_from_file():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("test/data/twotriangles_flow.net")
    im.run()
    output_path = Path("test/data/output/twotriangles_flow_output.net")
    output_path.parent.mkdir(exist_ok=True)
    im.write_pajek(str(output_path), flow=True)
    assert (filecmp.cmp(output_path, "test/data/twotriangles_flow_output.net"))


def test_precomputed_states_from_file():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("test/data/states_flow.net")
    im.run()
    output_path = Path("test/data/output/states_flow_output.net")
    output_path.parent.mkdir(exist_ok=True)
    im.write_pajek(str(output_path), flow=True)
    assert (filecmp.cmp(output_path, "test/data/states_flow_output.net"))
