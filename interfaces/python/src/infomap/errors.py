"""The public error taxonomy for the :mod:`infomap` package.

Engine errors cross the SWIG boundary as bare ``RuntimeError`` with the C++
message string. The package re-raises them as :class:`InfomapError` (or a
subclass) at the ``Core`` boundary, so callers can catch Infomap failures
without also catching unrelated ``RuntimeError``.

Compatibility: :class:`InfomapError` inherits ``RuntimeError`` through 2.x,
so existing ``except RuntimeError`` code keeps working; the base may detach
in 3.0. :class:`NotRunError` additionally keeps ``ValueError`` in its MRO
through 2.x because the results-before-run guard used to raise ``ValueError``.

This module is pure Python and importable without the compiled bindings.
"""

from __future__ import annotations

from contextlib import contextmanager

__all__ = [
    "InfomapError",
    "NetworkParseError",
    "NotRunError",
    "StaleResultError",
]


class InfomapError(RuntimeError):
    """Base class for errors raised by the Infomap engine or package.

    Every error the engine reports -- invalid arguments, option conflicts,
    flow-calculation failures, unwritable output files -- is raised as this
    class or one of its subclasses. Inherits :class:`RuntimeError` through
    2.x for backwards compatibility.
    """


class NetworkParseError(InfomapError):
    """The network input could not be read.

    Raised by :meth:`Infomap.read_file <infomap.Infomap.read_file>`,
    :meth:`Network.from_file <infomap.Network.from_file>`, and
    :func:`infomap.run` with a file path, when the input file cannot be
    opened or its content cannot be parsed. Also raised from ``run()`` when
    an auxiliary input file (e.g. ``cluster_data``) fails to open or parse.
    """


class NotRunError(InfomapError, ValueError):
    """Results were requested before the algorithm has run.

    Raised by the exporters and integration helpers when they are handed an
    :class:`~infomap.Infomap` instance with no module assignments yet.
    Keeps :class:`ValueError` in its MRO through 2.x because this guard
    previously raised ``ValueError``.
    """


class StaleResultError(InfomapError):
    """Node-level data was read from a :class:`~infomap.Result` after a re-run.

    The C++ result tree is rebuilt on every ``run()``, so node-level access
    on a ``Result`` created by an earlier run raises instead of reading the
    rebuilt tree. The eager scalars captured at ``run()`` time (codelength,
    module counts, ...) stay valid on the stale ``Result``.
    """


# Known C++ message patterns that identify an *input* failure surfacing from
# a run() call (the engine reads cluster/network files at run time). Matched
# with str.startswith / substring; first hit wins. Everything read_file-side
# maps to NetworkParseError wholesale, so this table only needs the run-time
# stragglers.
_PARSE_MESSAGE_PREFIXES = (
    "Can't parse",
    "Cannot parse",
    "Cannot open input file",
    "Can't open file",
    "Unrecognized heading",
    "JSON parse error",
)
_PARSE_MESSAGE_SUBSTRINGS = (
    # SafeFile's read-side open error ("... read permissions."); the
    # write-side variant says "write permissions" and stays InfomapError.
    "read permissions",
)


def _classify_run_message(message: str) -> type[InfomapError] | None:
    if message.startswith(_PARSE_MESSAGE_PREFIXES):
        return NetworkParseError
    if any(fragment in message for fragment in _PARSE_MESSAGE_SUBSTRINGS):
        return NetworkParseError
    return None


@contextmanager
def _translate_engine_errors(
    default: type[InfomapError] = InfomapError, *, classify: bool = False
):
    """Re-raise SWIG ``RuntimeError`` as the taxonomy type, message preserved.

    ``default`` is the class used for unrecognized messages; ``classify=True``
    additionally consults the run-time message table above so input failures
    surfacing from ``run()`` become :class:`NetworkParseError`. Errors already
    in the taxonomy pass through untouched.
    """
    try:
        yield
    except InfomapError:
        raise
    except RuntimeError as error:
        message = str(error)
        error_class = (_classify_run_message(message) if classify else None) or default
        raise error_class(message) from None
