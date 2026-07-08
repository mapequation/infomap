"""Routing of the engine console log into Python :mod:`logging` (#745).

The engine writes its console output through the C++ ``Log`` class. When the
user has attached handlers to ``logging.getLogger("infomap")``, the package
installs a process-global line sink at the SWIG boundary and every visible
engine log line becomes a log record instead of stdout output: default lines
at ``INFO``, ``-v``/``-vv`` detail lines at ``DEBUG``. Without handlers the
engine writes to stdout exactly as before.

Routing deliberately keys on handlers attached *directly* to the
``"infomap"`` logger, not on propagated root handlers: pytest and
``logging.basicConfig`` both attach root handlers, and rerouting on those
would silently change the stdout behavior of every ``silent=False`` script.

``silent=`` gates emission engine-side, upstream of the sink, in both modes.
The sink is process-global (C++ ``Log`` state already is), matching the
existing one-run-at-a-time constraint.
"""

from __future__ import annotations

import logging
from contextlib import contextmanager

logger = logging.getLogger("infomap")

# Engine message levels at or above this become DEBUG records; below, INFO.
_ENGINE_DEBUG_FROM_LEVEL = 1

_installed = False


def _emit(level: int, line: str) -> None:
    logger.log(
        logging.DEBUG if level >= _ENGINE_DEBUG_FROM_LEVEL else logging.INFO,
        "%s",
        line,
    )


def _sync_engine_routing() -> None:
    """Install or remove the engine-to-logging bridge for the next engine call."""
    # Imported lazily: _core imports _bindings at module load, and this module
    # is imported by the facade/network layers that _core must not depend on.
    from ._core import drain_log_queue, set_log_callback

    global _installed
    should_route = bool(logger.handlers)
    if should_route and not _installed:
        set_log_callback(_emit)
        _installed = True
    elif not should_route and _installed:
        # Deliver anything still queued before the sink goes away.
        drain_log_queue()
        set_log_callback(None)
        _installed = False


def _drain() -> None:
    if _installed:
        from ._core import drain_log_queue

        drain_log_queue()


@contextmanager
def engine_log_routing():
    """Engage/disengage log routing around one engine call.

    Syncs the sink against the current handler configuration on entry and, on
    exit, drains lines queued by non-GIL-holding threads (OpenMP task threads
    emit ``-vv`` detail lines) plus any trailing partial line.
    """
    _sync_engine_routing()
    try:
        yield
    finally:
        _drain()
