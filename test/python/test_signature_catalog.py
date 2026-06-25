"""Guard: the ``Infomap.__init__`` / ``Infomap.run`` keyword signatures must stay
in sync with the GENERATED ``InfomapOptions`` field catalog (the single source of
truth). This closes the divergence risk of hand-maintaining the ~90-param lists.
"""

import inspect

import pytest

from infomap import Infomap
from infomap._options import _OPTION_FIELD_NAMES

# Facade-only kwargs intentionally NOT in the generated option catalog, kept for
# backward compatibility. `pretty` is a deprecated no-op in the C++ ParameterCatalog
# ("pretty console output is always on"); it is accepted-and-ignored so old user code
# keeps working, hence absent from InfomapOptions. Any *new* extra parameter still fails.
BACKWARD_COMPAT_EXTRA = {"pretty"}

NON_OPTION_INIT = {"self", "args"} | BACKWARD_COMPAT_EXTRA
NON_OPTION_RUN = {"self", "args", "initial_partition"} | BACKWARD_COMPAT_EXTRA


@pytest.mark.fast
def test_init_signature_matches_option_catalog():
    params = set(inspect.signature(Infomap.__init__).parameters) - NON_OPTION_INIT
    assert params == set(_OPTION_FIELD_NAMES), (
        "Infomap.__init__ drifted from InfomapOptions. "
        f"In init not catalog={sorted(params - set(_OPTION_FIELD_NAMES))}; "
        f"in catalog not init={sorted(set(_OPTION_FIELD_NAMES) - params)}"
    )


@pytest.mark.fast
def test_run_signature_matches_option_catalog():
    params = set(inspect.signature(Infomap.run).parameters) - NON_OPTION_RUN
    assert params == set(_OPTION_FIELD_NAMES), (
        "Infomap.run drifted from InfomapOptions. "
        f"In run not catalog={sorted(params - set(_OPTION_FIELD_NAMES))}; "
        f"in catalog not run={sorted(set(_OPTION_FIELD_NAMES) - params)}"
    )
