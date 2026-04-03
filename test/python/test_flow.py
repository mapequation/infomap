import re

import pytest


def test_precomputed_requirements(make_infomap, example_network_path):
    im = make_infomap(flow_model="precomputed")
    im.read_file(str(example_network_path("twotriangles.net")))
    with pytest.raises(
        RuntimeError,
        match=re.escape(
            "Missing node flow in input data. Should be passed as a third field under a *Vertices section."
        ),
    ):
        im.run()


def test_precomputed_states_requirements(make_infomap, example_network_path):
    im = make_infomap(flow_model="precomputed")
    im.read_file(str(example_network_path("states.net")))
    with pytest.raises(
        RuntimeError,
        match=re.escape(
            "Missing node flow in input data. Should be passed as a third field under a *States section."
        ),
    ):
        im.run()


def test_precomputed_from_file(make_infomap, fixture_path, output_dir, assert_pajek_matches_fixture):
    im = make_infomap(flow_model="precomputed")
    im.read_file(str(fixture_path("twotriangles_flow.net")))
    im.run()
    output_path = output_dir / "twotriangles_flow_output.net"
    im.write_pajek(str(output_path), flow=True)
    assert_pajek_matches_fixture(output_path, fixture_path("twotriangles_flow_output.net"))


def test_precomputed_states_from_file(make_infomap, fixture_path, output_dir, assert_pajek_matches_fixture):
    im = make_infomap(flow_model="precomputed")
    im.read_file(str(fixture_path("states_flow.net")))
    im.run()
    output_path = output_dir / "states_flow_output.net"
    im.write_pajek(str(output_path), flow=True)
    assert_pajek_matches_fixture(output_path, fixture_path("states_flow_output.net"))
