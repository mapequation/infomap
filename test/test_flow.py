from infomap import Infomap
import math
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


def test_initial_partition_runs_optimization_before_first_level_consolidation():
    links = [
        (1, 2),
        (1, 3),
        (2, 3),
        (3, 4),
        (4, 5),
        (4, 6),
        (5, 6),
    ]
    params = dict(silent=True, num_trials=1, core_level_limit=1, tune_iteration_limit=1)

    baseline = Infomap(**params)
    baseline.add_links(links)
    baseline.run()

    im = Infomap(**params)
    im.add_links(links)
    im.initial_partition = {1: 1, 2: 1, 3: 2, 4: 2, 5: 1, 6: 1}
    im.run()

    assert im.num_top_modules == baseline.num_top_modules == 2
    assert math.isclose(im.codelength, baseline.codelength)


def test_repeated_run_with_metadata_after_initial_partition_optimization_change():
    im = Infomap(silent=True, num_trials=10)
    im.add_links(
        (
            (1, 2),
            (1, 3),
            (2, 3),
            (3, 4),
            (4, 5),
            (4, 6),
            (5, 6),
        )
    )
    im.set_meta_data(1, 0)
    im.set_meta_data(2, 0)
    im.set_meta_data(3, 1)
    im.set_meta_data(4, 1)
    im.set_meta_data(5, 0)
    im.set_meta_data(6, 0)

    im.run(meta_data_rate=0)
    assert im.num_top_modules == 2

    im.run(meta_data_rate=2)
    assert im.num_top_modules == 3
