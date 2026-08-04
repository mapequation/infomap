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


def test_bipartite_flow_adjustment_leaves_no_negative_enter_flow(example_network_path):
    # The adjustment that moves flow off the feature nodes left their teleport flow and
    # weight behind, and the self-teleportation removed from enter/exit flow then ran past
    # the zero it had left: both feature nodes came out at flow 0 and enter/exit
    # -(0.1232969585 * 0.2352941176) = -0.02901104907. Every module containing one inherited
    # a negative enter flow, and the two-level index codelength evaluated to 1.1e-16 --
    # charging nothing at all for entering either of two modules (#957).
    #
    # Asserted on the index term rather than on the total, because that is where the
    # cancellation showed: the total stayed plausible while the index term vanished.
    net = infomap.Network.from_file(str(example_network_path("bipartite.net")))
    result = net.run(regularized=True, two_level=True, seed=7, num_trials=1)

    assert len(set(result.modules().values())) == 2
    assert result.index_codelength == pytest.approx(0.01551663112)
    assert result.codelength == pytest.approx(0.7991539547)


def test_bipartite_regularized_partition_scores_the_same_when_read_back(
    example_network_path,
):
    # Two independent evaluations of one partition: the optimizer's tracked value, and a
    # fresh recompute over the tree from the module enter/exit flow. Negative flow drove
    # them apart -- 0.5939114118 against 0.4252773622 -- while they agree on every input
    # whose flow is sound.
    path = str(example_network_path("bipartite.net"))
    options = {"regularized": True, "two_level": True, "seed": 7, "num_trials": 1}

    im = infomap.Infomap(silent=True, **options)
    im.read_file(path)
    result = im.run()

    rescored = infomap.Infomap(silent=True, no_infomap=True, **options)
    rescored.read_file(path)
    rescored.run(initial_partition=result.modules())

    assert rescored.codelength == pytest.approx(result.codelength)


def test_negative_module_flow_is_an_error(network_fixture_path):
    # One level up from the leaf check: a module's enter/exit flow is not the sum of its
    # leaves', because the optimizer maintains it incrementally and removes the teleportation
    # internal to the module. That removal can over-subtract even when every leaf is sound,
    # and on this network under --regularized it leaves a module at enter/exit -0.00121 with
    # a negative codelength. Before the check that surfaced only as a 0.0007 disagreement
    # between two evaluations of the same partition (#958).
    net = infomap.Network.from_file(str(network_fixture_path("bipartite_uneven.net")))

    with pytest.raises(infomap.InfomapError, match=r"Negative enter flow on a module"):
        net.run(regularized=True, two_level=True, seed=7, num_trials=1)


def test_negative_module_flow_is_an_error_under_parallel_trials(network_fixture_path):
    # The trial runs on a worker instance under --parallel-trials, so a check reading the main
    # instance would walk an unpartitioned tree and pass every time. That is the path where a
    # silent wrong codelength is worst -- it reported 0.9210660968 here -- so cover it
    # separately from the serial one above.
    net = infomap.Network.from_file(str(network_fixture_path("bipartite_uneven.net")))

    with pytest.raises(infomap.InfomapError, match=r"Negative enter flow on a module"):
        net.run(
            regularized=True,
            two_level=True,
            seed=7,
            num_trials=4,
            parallel_trials=True,
        )


@pytest.mark.xfail(
    strict=True,
    reason="#958: module enter/exit flow still goes negative on bipartite input under "
    "--regularized, so the run is refused rather than reporting a codelength built on it. "
    "Drop this marker when the accumulation is fixed -- an unexpected pass means it is.",
)
def test_bipartite_uneven_regularized_yields_a_codelength(network_fixture_path):
    # Stated as what should happen rather than as the current error, so this turns red the
    # moment #958 is fixed instead of having to be rewritten.
    net = infomap.Network.from_file(str(network_fixture_path("bipartite_uneven.net")))
    result = net.run(regularized=True, two_level=True, seed=7, num_trials=1)

    assert result.codelength > 0.0


def test_flow_convergence_is_visible_on_the_result():
    # A failed power iteration means the flow is not the network's stationary
    # distribution, so the codelength describes something else. It used to be reported
    # only as a console line, and --summary-json requires --silent, so no automated
    # consumer could see it at all.
    links = ((1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 3))

    im = infomap.Infomap(silent=True, directed=True, max_flow_iterations=2)
    im.add_links(links)
    stopped_early = im.run()

    converging = infomap.Infomap(silent=True, directed=True)
    converging.add_links(links)
    converged = converging.run()

    assert converged.flow_converged is True
    assert converged.flow_iterations > 0
    # This network reaches tolerance within two iterations, so a truncated run is not a
    # failure -- "converged" means the error is within tolerance, not that the loop
    # stopped before the limit.
    assert stopped_early.flow_iterations == 2
    assert stopped_early.flow_converged is True


def test_flow_convergence_reports_a_real_failure(example_network_path):
    # Same options on a network that genuinely cannot reach tolerance in two iterations.
    net = infomap.Network.from_file(str(example_network_path("ninetriangles.net")))
    result = net.run(directed=True, max_flow_iterations=2, seed=7, num_trials=1)

    assert result.flow_converged is False
    assert result.flow_iterations == 2
    assert result.flow_error > 0.0


def test_undirected_flow_reports_no_power_iteration():
    # Nothing to converge, so the flag must not read as a failure.
    im = infomap.Infomap(silent=True)
    im.add_links(((1, 2), (2, 3), (3, 1), (3, 4), (4, 5), (5, 3)))
    result = im.run()

    assert result.flow_converged is True
    assert result.flow_iterations == 0


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
