import re

import infomap
import pytest

pytestmark = pytest.mark.fast


def test_degenerate_flow_raises_instead_of_reporting_zero(example_network_path):
    # The canonical bipartite shape (every link primary -> feature) leaves this
    # flow model with no flow to distribute. It used to return codelength 0.0 --
    # lower than any true codelength, so a script comparing flow models would
    # pick the broken run as the best model.
    net = infomap.Network.from_file(str(example_network_path("bipartite.net")))

    with pytest.raises(infomap.InfomapError, match="Degenerate flow"):
        net.run(directed=True, recorded_teleportation=True, seed=7, num_trials=1)


def test_skip_adjust_bipartite_flow_still_runs(example_network_path):
    # The documented opt-out from distributing flow to the primary nodes: flow
    # deliberately sums to less than one, so the run must complete.
    net = infomap.Network.from_file(str(example_network_path("bipartite.net")))

    result = net.run(
        directed=True,
        skip_adjust_bipartite_flow=True,
        two_level=True,
        seed=7,
        num_trials=1,
    )

    # Raised from 0.04164969778 when #899's dangling redistribution was restored:
    # the link flows now carry the 1/(1 - danglingRank) scaling the node flows
    # already had, so the exit flows are no longer deflated. The flow still sums to
    # less than one, which is what this test is about.
    assert result.codelength == pytest.approx(0.2776646519)


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


def test_precomputed_from_file(
    make_infomap,
    network_fixture_path,
    expected_fixture_path,
    output_dir,
    assert_pajek_matches_fixture,
):
    im = make_infomap(flow_model="precomputed")
    im.read_file(str(network_fixture_path("twotriangles_flow.net")))
    im.run()
    output_path = output_dir / "twotriangles_flow_output.net"
    im.write_pajek(str(output_path), flow=True)
    assert_pajek_matches_fixture(
        output_path, expected_fixture_path("twotriangles_flow_output.net")
    )


def test_precomputed_states_from_file(
    make_infomap,
    network_fixture_path,
    expected_fixture_path,
    output_dir,
    assert_pajek_matches_fixture,
):
    im = make_infomap(flow_model="precomputed")
    im.read_file(str(network_fixture_path("states_flow.net")))
    im.run()
    output_path = output_dir / "states_flow_output.net"
    im.write_pajek(str(output_path), flow=True)
    assert_pajek_matches_fixture(
        output_path, expected_fixture_path("states_flow_output.net")
    )
