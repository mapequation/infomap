"""Objective options the input's objective does not act on are warned about, not ignored.

``--entropy-corrected`` and ``--preferred-number-of-modules`` are implemented by
``BiasedMapEquation`` only, which the engine selects for ordinary networks. Any input that
selects another objective -- state, multilayer and meta-data here, but also ``--lossy`` on
an ordinary network -- leaves them inert, so those options were accepted,
echoed in the run banner, recorded in the run metadata as active -- and never applied.
``--meta-data`` combined with higher-order input is the mirror case: meta-data wins the
objective dispatch, so the run is scored without the physical-node codebook that makes it
a memory network. Both were silent, which read as "applied" (#904).

The assertions go through the routed ``infomap`` logger rather than fd-level capture: it
is the supported way to observe engine output from Python, and it carries the level-0
warnings these tests are about.
"""

from __future__ import annotations

import logging

import pytest
from infomap import Infomap

pytestmark = pytest.mark.fast

ENTROPY_WARNING = "--entropy-corrected has no effect on this run"
PREFERRED_WARNING = "--preferred-number-of-modules has no effect on this run"
META_PRECEDENCE_WARNING = "--meta-data takes precedence over higher-order input"


class _ListHandler(logging.Handler):
    def __init__(self) -> None:
        super().__init__(level=logging.DEBUG)
        self.messages: list[str] = []

    def emit(self, record: logging.LogRecord) -> None:
        self.messages.append(record.getMessage())


@pytest.fixture
def engine_messages():
    logger = logging.getLogger("infomap")
    handler = _ListHandler()
    logger.addHandler(handler)
    old_level = logger.level
    logger.setLevel(logging.DEBUG)
    yield handler.messages
    logger.removeHandler(handler)
    logger.setLevel(old_level)


def _run_states(**kwargs) -> None:
    im = Infomap(num_trials=1, **kwargs)
    im.add_state_node(1, 1)
    im.add_state_node(2, 2)
    im.add_state_node(3, 1)
    im.add_state_node(4, 3)
    im.add_link(1, 2)
    im.add_link(2, 3)
    im.add_link(3, 4)
    im.add_link(1, 4)
    im.run()


def _run_ordinary(**kwargs) -> None:
    im = Infomap(num_trials=1, **kwargs)
    im.add_link(1, 2)
    im.add_link(2, 3)
    im.add_link(1, 3)
    im.add_link(3, 4)
    im.add_link(4, 5)
    im.add_link(5, 3)
    im.run()


def _matching(messages: list[str], needle: str) -> list[str]:
    return [message for message in messages if needle in message]


def test_entropy_correction_warns_on_state_input(engine_messages):
    _run_states(entropy_corrected=True)
    assert _matching(engine_messages, ENTROPY_WARNING)


def test_preferred_number_of_modules_warns_on_state_input(engine_messages):
    _run_states(preferred_number_of_modules=2)
    assert _matching(engine_messages, PREFERRED_WARNING)


def test_meta_data_precedence_warns_on_state_input(engine_messages, tmp_path):
    meta = tmp_path / "const.meta"
    # Every physical node in one meta category, so the meta term is exactly 0 and the run
    # cannot be defended as "the metadata was worth the swap" -- the objective changed for
    # nothing. This is the shape the issue used to demonstrate the swap.
    meta.write_text("1 1\n2 1\n3 1\n", encoding="utf-8")

    _run_states(meta_data=str(meta))

    assert _matching(engine_messages, META_PRECEDENCE_WARNING)


def test_ordinary_input_applies_both_options_silently(engine_messages):
    # The objective for an ordinary network implements both terms, so there is nothing to
    # warn about. Guards against the warnings firing on the runs that are actually correct.
    _run_ordinary(entropy_corrected=True, preferred_number_of_modules=2)

    assert not _matching(engine_messages, ENTROPY_WARNING)
    assert not _matching(engine_messages, PREFERRED_WARNING)


def test_meta_data_on_ordinary_input_is_not_a_precedence_warning(
    engine_messages, tmp_path
):
    # Meta-data on a first-order network is what MetaMapEquation is for; only the
    # combination with higher-order input silently discards something.
    meta = tmp_path / "ordinary.meta"
    meta.write_text("1 1\n2 1\n3 2\n4 2\n5 2\n", encoding="utf-8")

    _run_ordinary(meta_data=str(meta))

    assert not _matching(engine_messages, META_PRECEDENCE_WARNING)


def test_engine_emits_something_at_all(engine_messages):
    # The three assertions above are only meaningful if the routed logger is carrying engine
    # output in this environment; an empty stream would make every `not _matching` vacuous.
    _run_ordinary()
    assert engine_messages
