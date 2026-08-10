"""The thread budget's effect on the process it runs in.

A run publishes its budget as the OpenMP ``nthreads-var`` ICV so every parallel
region inherits it. That ICV belongs to the process, not to Infomap, so a run has
to give it back -- an embedding host that limits OpenMP programmatically
(threadpoolctl, a BLAS wrapper, torch) must not have its limit rewritten by
calling us.

These live in Python rather than in the C++ suite on purpose: the C++ test targets
are compiled without OpenMP flags, so ``_OPENMP`` is undefined there and a guarded
test case would silently compile to nothing. Here the extension module's own libomp
is loaded and observable.
"""

import infomap
import pytest

threadpoolctl = pytest.importorskip(
    "threadpoolctl",
    reason="threadpoolctl reads the loaded OpenMP runtime's thread count",
)


def openmp_threads():
    """The loaded OpenMP runtime's current thread limit, or None if absent."""
    for pool in threadpoolctl.threadpool_info():
        if pool.get("user_api") == "openmp":
            return pool["num_threads"]
    return None


def run_something(**kwargs):
    im = infomap.Infomap(silent=True, seed=123, **kwargs)
    im.add_links([(0, 1), (1, 2), (2, 0), (2, 3), (3, 4), (4, 5), (5, 3)])
    im.run()
    return im


@pytest.fixture
def openmp_present():
    if openmp_threads() is None:
        pytest.skip("built without OpenMP, so there is no thread ICV to preserve")


def test_run_restores_the_hosts_openmp_limit(openmp_present):
    """A run inside a host's thread limit leaves that limit alone."""
    with threadpoolctl.threadpool_limits(limits=2):
        assert openmp_threads() == 2
        # num_threads above the host's limit, so a run that ignored the restore
        # leaves an observably different value rather than coincidentally the same.
        run_something(num_threads=4, num_trials=2)
        assert openmp_threads() == 2


def test_repeated_runs_do_not_drift_the_limit(openmp_present):
    """Restoring per run means the count cannot creep across runs."""
    with threadpoolctl.threadpool_limits(limits=1):
        for _ in range(3):
            run_something(num_threads=4)
            assert openmp_threads() == 1


def test_limit_survives_a_failing_run(openmp_present):
    """The guard unwinds, so a run that raises still hands the ICV back."""
    with threadpoolctl.threadpool_limits(limits=2):
        im = infomap.Infomap(
            silent=True, num_threads=4, cluster_data="does-not-exist.clu"
        )
        im.add_links([(0, 1), (1, 2)])
        with pytest.raises(infomap.NetworkParseError):
            im.run()
        assert openmp_threads() == 2


def test_the_limit_returns_to_the_process_default_with_no_host_limit(openmp_present):
    """With no host limit set, the ICV still has to come back to what it was.

    Distinct from the tests above: it catches a guard that restores the wrong value
    -- for instance its own budget -- which a run inside an explicit limit cannot
    show, since there the budget and the restored value differ either way.
    """
    baseline = openmp_threads()
    assert baseline is not None
    for requested in (1, 2, 3):
        run_something(num_threads=requested)
        assert openmp_threads() == baseline
