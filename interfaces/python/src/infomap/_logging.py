"""Routing of the engine console log into Python :mod:`logging` (#745).

The engine writes its console output through the C++ ``Log`` class. When the
user has attached handlers to ``logging.getLogger("infomap")`` (one line:
:func:`infomap.enable_log`), the package installs a process-global line sink
at the SWIG boundary and every visible engine log line becomes a log record
instead of stdout output: default lines at ``INFO``, ``-v``/``-vv`` detail
lines at ``DEBUG``. Without handlers the engine writes to stdout exactly as
before.

In routed mode, logging is the control: the engine is un-silenced regardless
of the ``silent`` option (silence with logger levels or by removing the
handlers), and a DEBUG-enabled logger raises the engine verbosity so detail
records exist to filter. In classic (stdout) mode ``silent=`` keeps gating
emission as always.

Routing deliberately keys on handlers attached *directly* to the
``"infomap"`` logger, not on propagated root handlers: pytest and
``logging.basicConfig`` both attach root handlers, and rerouting on those
would silently change the stdout behavior of every ``silent=False`` script.

The sink is process-global (C++ ``Log`` state already is), matching the
existing one-run-at-a-time constraint.
"""

from __future__ import annotations

import logging
import sys
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


def is_routed() -> bool:
    """Whether engine log routing is engaged for the next engine call."""
    return bool(logger.handlers)


def apply_engine_log_overrides(kwargs: dict) -> dict:
    """Make logging the emission control when routing is engaged.

    Applied to the option kwargs right before they are rendered to engine
    args (both at ``Infomap`` construction and per run): strip ``silent`` so
    the engine emits — the user asked for the log by configuring the logger;
    silence with logger levels instead — and, when the logger is enabled for
    DEBUG, raise the engine verbosity to ``-vv`` (unless the user already
    chose a higher ``verbosity_level``) so detail records exist to filter.
    Returns ``kwargs`` untouched when routing is not engaged.
    """
    if not is_routed():
        return kwargs
    kwargs = dict(kwargs)
    kwargs["silent"] = False
    if logger.isEnabledFor(logging.DEBUG):
        current = kwargs.get("verbosity_level") or 1
        if current <= 1:
            kwargs["verbosity_level"] = 2
    return kwargs


_helper_handler: logging.Handler | None = None


def enable_log(level: int = logging.INFO) -> logging.Handler:
    """Show the engine log as Python log records, in one line.

    Attaches a plain ``%(message)s`` stream handler (stdout) to the
    ``"infomap"`` logger and sets the logger level, which engages the log
    routing: every engine run emits records — no ``silent=False`` needed —
    and stdout output is replaced by the records. Pass
    ``level=logging.DEBUG`` for the engine's ``-vv`` detail lines.

    Idempotent: repeated calls reuse the same handler and only adjust the
    level. Undo with :func:`disable_log`. For full control (formatting,
    files, propagation), skip this helper and configure
    ``logging.getLogger("infomap")`` with standard :mod:`logging` instead.

    Turns off propagation while active so records are not *also* emitted by a
    root handler installed via ``logging.basicConfig`` (which would print each
    line twice); :func:`disable_log` restores propagation.

    Parameters
    ----------
    level : int, optional
        The logger level, by default ``logging.INFO``. Use
        ``logging.DEBUG`` to include the engine's detail lines.

    Returns
    -------
    logging.Handler
        The installed handler (useful for reformatting or removal).

    Examples
    --------
    >>> import infomap
    >>> handler = infomap.enable_log()
    >>> infomap.disable_log()
    """
    global _helper_handler
    if _helper_handler is None:
        handler = logging.StreamHandler(sys.stdout)
        handler.setFormatter(logging.Formatter("%(message)s"))
        _helper_handler = handler
    if _helper_handler not in logger.handlers:
        logger.addHandler(_helper_handler)
    # Don't double-print through a root handler (e.g. logging.basicConfig());
    # disable_log() puts propagation back.
    logger.propagate = False
    logger.setLevel(level)
    return _helper_handler


def disable_log() -> None:
    """Remove the handler installed by :func:`enable_log`.

    Handlers the user attached themselves are left untouched; with no
    handlers remaining, the engine goes back to classic stdout output
    (gated by ``silent=``).
    """
    global _helper_handler
    if _helper_handler is not None:
        logger.removeHandler(_helper_handler)
        _helper_handler = None
        # Restore the default so records reach ancestor handlers again.
        logger.propagate = True
