"""Keep an unreachable intersphinx inventory from failing the docs build.

``make build-docs`` passes ``-W``, so every Sphinx warning is an error -- which is
what we want for a broken reference or a stale toctree, and not what we want for
six external documentation sites that have to answer over the network on every
build. When one of them times out, intersphinx logs

    WARNING: failed to reach any of the inventories with the following issues:
    intersphinx inventory 'https://docs.scipy.org/doc/scipy/objects.inv' not
    fetchable due to <class 'requests.exceptions.ConnectTimeout'>

and that one warning ends the build. It reddened two unrelated pull requests
inside the same half hour (#1055, #1060), where it reads as a docs regression in
the branch until someone opens the log, and re-running is a coin flip on someone
else's uptime.

``suppress_warnings`` cannot reach it: Sphinx 9.1 logs this one through
``LOGGER.warning`` with no type or subtype (``sphinx/ext/intersphinx/_load.py``),
and only typed warnings can be suppressed by name. The record is intercepted here
instead, on the intersphinx logger alone, and demoted to INFO -- still in the
log, no longer fatal.

The alternative was a committed ``objects.inv`` per project as a fallback
location, which is intersphinx's own mechanism for this (a failure with a working
alternative logs at INFO). That was 612 KiB of binaries with a refresh chore
attached, and a stale inventory resolves references to URLs that no longer exist
-- silently wrong links, where this degrades to no link at all.

Losing an inventory costs only link resolution: with ``nitpicky`` off (the
default, and unset in ``conf.py``), an unresolved cross-reference renders as
plain text without a warning of its own. A build that hits this therefore leaves
``:class:`scipy.sparse.csr_matrix`` as unlinked code text and stays green, while
every other warning class remains an error.
"""

from __future__ import annotations

import logging

from sphinx.util import logging as sphinx_logging

__all__ = ["setup"]

# The logger intersphinx warns on. Resolved through Sphinx's own getLogger so the
# "sphinx." namespace prefix it adds cannot drift out from under us; the module
# path is the part that would have to be updated if upstream moved the logger,
# and if it ever does this filter stops matching and the build goes back to
# failing on the warning -- loud, not silent.
_INTERSPHINX_LOGGER = "sphinx.ext.intersphinx"

# The message as passed to LOGGER.warning, before %-interpolation. Matched on the
# format string so the URL, the exception class, and which site happened to be
# down do not enter into it. Sphinx runs it through gettext, so a translated
# docs build would not match and would keep failing as it does today; this build
# is English (no `language` set in conf.py).
_UNREACHABLE_INVENTORIES = "failed to reach any of the inventories"


class _DemoteUnreachableInventories(logging.Filter):
    """Demote the unreachable-inventory warning to INFO, leaving the rest alone.

    A filter on the logger rather than on the handler: Sphinx's
    ``WarningIsErrorFilter`` lives on the warning handler and raises from inside
    its own ``filter()``, so a handler filter added later would never get to run.
    Logger filters all run before any handler sees the record.
    """

    def filter(self, record: logging.LogRecord) -> bool:
        if record.levelno == logging.WARNING and _UNREACHABLE_INVENTORIES in str(
            record.msg
        ):
            record.levelno = logging.INFO
            record.levelname = "INFO"
        return True


def setup(app):
    # Installed at extension-load time, which is before intersphinx fetches
    # anything (it does that on builder-inited), so no ordering hazard.
    sphinx_logging.getLogger(_INTERSPHINX_LOGGER).logger.addFilter(
        _DemoteUnreachableInventories()
    )
    # Only a log filter: no doctree read or write, so parallel builds are fine.
    return {
        "version": "1.0",
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
