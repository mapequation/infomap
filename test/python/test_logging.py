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

import logging

import pytest

from infomap import Infomap


pytestmark = pytest.mark.fast


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
    capfd.readouterr()

    _run_triangle(silent=False)

    records = infomap_log_handler.records
    assert records, "expected engine log records on the 'infomap' logger"
    assert all(record.name == "infomap" for record in records)
    messages = [record.getMessage() for record in records]
    assert any("Infomap" in message for message in messages)
    # Routed mode replaces stdout output entirely.
    assert capfd.readouterr().out == ""


def test_default_lines_are_info_and_detail_lines_are_debug(infomap_log_handler):
    _run_triangle(silent=False, verbosity_level=2)

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


def test_silent_default_emits_no_records(infomap_log_handler):
    _run_triangle()

    assert infomap_log_handler.records == []


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
        capfd.readouterr()

        _run_triangle(silent=False)

        # Back to the classic stdout path, no records.
        assert infomap_log_handler.records == []
        assert capfd.readouterr().out != ""
    finally:
        logger.addHandler(infomap_log_handler)  # fixture teardown removes it


def test_propagated_root_handlers_do_not_engage_routing(capfd):
    # pytest attaches a capture handler to the root logger; the "infomap"
    # logger itself has none here. The engine must keep writing to stdout.
    assert not logging.getLogger("infomap").handlers
    capfd.readouterr()

    _run_triangle(silent=False)

    assert capfd.readouterr().out != ""


def test_read_file_routes_through_logging(infomap_log_handler, capfd, test_paths):
    capfd.readouterr()
    im = Infomap(silent=False)

    im.read_file(str(test_paths.example_networks / "ninetriangles.net"))

    assert capfd.readouterr().out == ""


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
