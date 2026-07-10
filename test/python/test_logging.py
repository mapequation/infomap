"""Engine-log routing into Python logging (#745).

Routing engages per engine call, only when handlers are attached directly to
``logging.getLogger("infomap")``: then every visible engine log line becomes
a record (default lines at INFO, ``-v``/``-vv`` detail at DEBUG) and stdout
stays clean. Without handlers the engine writes to stdout exactly as before
(pinned by test_run_functional.py). ``silent=`` gates emission upstream in
both modes.

Propagated root handlers deliberately do NOT engage routing — pytest itself
attaches one, so these tests double as the guard for that rule.
"""

from __future__ import annotations

import ctypes
import logging
import sys
import warnings

import pytest

from infomap import Infomap, disable_log, enable_log


# These tests drive the engine log -> Python logging routing, deliberately
# using silent=False to produce engine output; the pending deprecation on the
# silent keyword is expected here (it is asserted in test_deprecations.py).
pytestmark = [
    pytest.mark.fast,
    pytest.mark.filterwarnings("ignore::PendingDeprecationWarning"),
]


def _engine_stdout(capfd) -> str:
    """Read the engine's stdout from the fd-level capture.

    The engine writes via ``std::cout``; under pytest's fd-level capture
    stdout is a pipe, so the C runtime fully buffers the log and it must be
    flushed before reading (mirrors test_run_functional._engine_log —
    silence/emptiness assertions would otherwise pass or fail on stale
    buffer contents).
    """
    if sys.platform == "win32":
        ctypes.cdll.msvcrt.fflush(None)
    else:
        ctypes.CDLL(None).fflush(None)
    return capfd.readouterr().out


class ListHandler(logging.Handler):
    def __init__(self):
        super().__init__(level=logging.DEBUG)
        self.records: list[logging.LogRecord] = []

    def emit(self, record: logging.LogRecord) -> None:
        self.records.append(record)


@pytest.fixture
def infomap_log_handler():
    logger = logging.getLogger("infomap")
    handler = ListHandler()
    logger.addHandler(handler)
    old_level = logger.level
    logger.setLevel(logging.DEBUG)
    yield handler
    logger.removeHandler(handler)
    logger.setLevel(old_level)


def _run_triangle(**kwargs):
    im = Infomap(**kwargs)
    im.add_link(1, 2)
    im.add_link(2, 3)
    im.add_link(1, 3)
    return im.run()


def test_engine_log_routes_to_logging_and_not_stdout(infomap_log_handler, capfd):
    _engine_stdout(capfd)  # drain log buffered by earlier (unflushed) tests

    _run_triangle(silent=False)

    records = infomap_log_handler.records
    assert records, "expected engine log records on the 'infomap' logger"
    assert all(record.name == "infomap" for record in records)
    messages = [record.getMessage() for record in records]
    assert any("Infomap" in message for message in messages)
    # Routed mode replaces stdout output entirely.
    assert _engine_stdout(capfd) == ""


def test_routed_mode_emits_without_silent_false(infomap_log_handler, capfd):
    # Logging is the control in routed mode: configuring the logger is the
    # whole opt-in, no silent=False needed (and silent=True is ignored).
    _engine_stdout(capfd)

    _run_triangle()

    assert infomap_log_handler.records
    assert _engine_stdout(capfd) == ""

    infomap_log_handler.records.clear()
    _run_triangle(silent=True)

    assert infomap_log_handler.records


def test_routed_mode_is_silenced_with_logger_levels(infomap_log_handler):
    logger = logging.getLogger("infomap")
    logger.setLevel(logging.WARNING)

    _run_triangle()

    # The engine emits, but logging drops the INFO records: the pythonic
    # silence in routed mode.
    assert infomap_log_handler.records == []


def test_debug_logger_raises_engine_verbosity_automatically(infomap_log_handler):
    # The fixture leaves the logger enabled for DEBUG, so the run gets -vv
    # detail without any verbosity_level kwarg.
    _run_triangle()

    levels = {record.levelno for record in infomap_log_handler.records}
    assert logging.INFO in levels
    assert logging.DEBUG in levels
    # The banner is a default (INFO) line; indented detail lines are DEBUG.
    debug_messages = [
        record.getMessage()
        for record in infomap_log_handler.records
        if record.levelno == logging.DEBUG
    ]
    assert any("run Infomap" in message for message in debug_messages)


def test_info_logger_keeps_default_engine_verbosity(infomap_log_handler):
    logging.getLogger("infomap").setLevel(logging.INFO)

    _run_triangle()

    levels = {record.levelno for record in infomap_log_handler.records}
    assert levels == {logging.INFO}


def test_records_are_plain_lines_without_ansi(infomap_log_handler):
    _run_triangle(silent=False)

    for record in infomap_log_handler.records:
        message = record.getMessage()
        assert "\x1b[" not in message
        assert "\r" not in message
        assert "\n" not in message


def test_routing_disengages_when_handlers_are_removed(infomap_log_handler, capfd):
    _run_triangle(silent=False)
    assert infomap_log_handler.records

    logger = logging.getLogger("infomap")
    logger.removeHandler(infomap_log_handler)
    try:
        infomap_log_handler.records.clear()
        _engine_stdout(capfd)

        _run_triangle(silent=False)

        # Back to the classic stdout path, no records.
        assert infomap_log_handler.records == []
        assert _engine_stdout(capfd) != ""
    finally:
        logger.addHandler(infomap_log_handler)  # fixture teardown removes it


def test_propagated_root_handlers_do_not_engage_routing(capfd):
    # pytest attaches a capture handler to the root logger; the "infomap"
    # logger itself has none here. The engine must keep writing to stdout.
    assert not logging.getLogger("infomap").handlers
    _engine_stdout(capfd)

    _run_triangle(silent=False)

    assert _engine_stdout(capfd) != ""


def test_read_file_routes_through_logging(infomap_log_handler, capfd, test_paths):
    _engine_stdout(capfd)
    im = Infomap(silent=False)

    im.read_file(str(test_paths.example_networks / "ninetriangles.net"))

    assert _engine_stdout(capfd) == ""


def test_instance_constructed_before_logging_config_warns(infomap_log_handler):
    logger = logging.getLogger("infomap")
    logger.removeHandler(infomap_log_handler)
    im = Infomap()  # constructed silent: no handlers at this point
    im.add_link(1, 2)
    logger.addHandler(infomap_log_handler)

    with pytest.warns(UserWarning, match="before logging was configured"):
        im.run()

    # The engine baked --silent in at construction, so no records exist.
    assert infomap_log_handler.records == []


def test_disable_log_restores_user_propagate_choice():
    logger = logging.getLogger("infomap")
    original = logger.propagate
    try:
        logger.propagate = False  # a deliberate user choice
        enable_log()
        assert logger.propagate is False  # off while active
        disable_log()
        # Restored to the user's choice, not forced back to the True default.
        assert logger.propagate is False

        logger.propagate = True
        enable_log()
        disable_log()
        assert logger.propagate is True
    finally:
        disable_log()
        logger.propagate = original


def test_stale_silent_advisory_warns_at_most_once(infomap_log_handler):
    logger = logging.getLogger("infomap")
    logger.removeHandler(infomap_log_handler)
    im = Infomap()  # constructed silent before logging was configured
    im.add_link(1, 2)
    logger.addHandler(infomap_log_handler)

    with warnings.catch_warnings(record=True) as records:
        warnings.simplefilter("always")
        im.run()
        im.run()
        im.run()

    stale = [
        r for r in records if "before logging was configured" in str(r.message)
    ]
    assert len(stale) == 1


def test_enable_log_is_a_one_line_opt_in(capfd):
    handler = enable_log()
    try:
        assert handler in logging.getLogger("infomap").handlers
        # Idempotent: a second call reuses the handler.
        assert enable_log() is handler
        assert logging.getLogger("infomap").handlers.count(handler) == 1
        _engine_stdout(capfd)

        _run_triangle()

        # The helper's stdout handler prints the records: one opt-in line,
        # engine progress visible, no silent=False anywhere.
        out = _engine_stdout(capfd)
        assert "Infomap v" in out
        assert "\x1b[" not in out
    finally:
        disable_log()

    assert handler not in logging.getLogger("infomap").handlers
    _engine_stdout(capfd)

    _run_triangle()

    # Classic quiet default is back.
    assert _engine_stdout(capfd) == ""


@pytest.mark.filterwarnings("ignore::pytest.PytestUnraisableExceptionWarning")
def test_raising_log_handler_does_not_kill_the_run(infomap_log_handler):
    class RaisingHandler(logging.Handler):
        def emit(self, record: logging.LogRecord) -> None:
            raise RuntimeError("handler boom")

    logger = logging.getLogger("infomap")
    raising = RaisingHandler()
    logger.addHandler(raising)
    try:
        # A handler that raises straight out of emit() propagates through
        # logger.log into the C++ sink's callback, which swallows it with
        # PyErr_WriteUnraisable (hence the ignored unraisable warning): a
        # broken log handler must not kill the engine run.
        result = _run_triangle(silent=False)
        assert result.num_top_modules >= 1
    finally:
        logger.removeHandler(raising)
