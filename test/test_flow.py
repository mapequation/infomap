from infomap import Infomap
import re
from pathlib import Path
import itertools as it
import pytest


def are_files_identical(filename1, filename2, start=0):
    with open(filename1, "rt") as a:
        with open(filename2, "rt") as b:
            return all(
                line_a == line_b
                for line_a, line_b in it.zip_longest(
                    it.islice(a, start, None), it.islice(b, start, None)
                )
            )


def test_precomputed_requirements():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("examples/networks/twotriangles.net")
    with pytest.raises(
        RuntimeError,
        match=re.escape(
            "Missing node flow in input data. Should be passed as a third field under a *Vertices section."
        ),
    ):
        im.run()


def test_precomputed_states_requirements():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("examples/networks/states.net")
    with pytest.raises(
        RuntimeError,
        match=re.escape(
            "Missing node flow in input data. Should be passed as a third field under a *States section."
        ),
    ):
        im.run()


def test_precomputed_from_file():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("test/data/twotriangles_flow.net")
    im.run()
    output_path = Path("test/data/output/twotriangles_flow_output.net")
    output_path.parent.mkdir(exist_ok=True)
    im.write_pajek(str(output_path), flow=True)
    assert are_files_identical(
        output_path, "test/data/twotriangles_flow_output.net", start=1
    )


def test_precomputed_states_from_file():
    im = Infomap(silent=True, flow_model="precomputed")
    im.read_file("test/data/states_flow.net")
    im.run()
    output_path = Path("test/data/output/states_flow_output.net")
    output_path.parent.mkdir(exist_ok=True)
    im.write_pajek(str(output_path), flow=True)
    assert are_files_identical(output_path, "test/data/states_flow_output.net", start=1)
