"""File writers for the engine's native text formats.

Three hosts share these mixins (#760):

- :class:`~infomap.Result` carries the *result-artifact* writers
  (``.tree``/``.ftree``/``.clu``/Newick/JSON/CSV): the artifacts are derived
  from the partition, so the immutable result is their natural home. Access
  is generation-guarded like all node-level ``Result`` access.
- :class:`~infomap.Network` carries the *network-describing* writers
  (``write_pajek``, ``write_state_network``): they serialize the input, not
  the partition.
- :class:`~infomap.Infomap` keeps the full historical set through 2.x.

Each host provides ``_writer_core()`` returning the engine core to write
from; the default resolves ``self._core`` (Infomap, Network), and ``Result``
overrides it with its stale-guard.
"""

from __future__ import annotations

import os
from typing import TYPE_CHECKING

from ..errors import _translate_engine_errors


if TYPE_CHECKING:
    from .._core import Core


class _WritersBase:
    # Empty slots: Result composes these mixins and is slots-based; a mixin
    # without __slots__ would silently add a __dict__ to every Result.
    __slots__ = ()

    # Attribute provided by the composing host. Declared under TYPE_CHECKING
    # so pyright resolves the mixin accesses without affecting runtime.
    if TYPE_CHECKING:
        _core: Core

    def _writer_core(self) -> "Core":
        return self._core


class _ResultWritersMixin(_WritersBase):
    """Writers for the result-derived artifacts (tree/ftree/clu/...)."""

    __slots__ = ()

    def write(self, filename: str | os.PathLike[str], *args, **kwargs) -> None:
        """Write results to file, inferring the format from the extension.

        An existing file at ``filename`` is overwritten.

        Raises
        ------
        ValueError
            If ``filename`` has no extension to infer the format from.
        NotImplementedError
            If the file format is not supported.

        Parameters
        ----------
        filename : str or os.PathLike
            The filename.
        """
        filename = os.fspath(filename)
        _, ext = os.path.splitext(filename)

        # remove the dot
        ext = ext[1:]

        if not ext:
            raise ValueError(
                f"Cannot infer an output format from {filename!r}: the filename "
                "has no extension. Add one of .clu, .tree, .ftree, .nwk, .net, "
                ".json or .csv, or call a specific writer such as write_clu()."
            )

        if ext == "ftree":
            ext = "flow_tree"
        elif ext == "nwk":
            ext = "newick"
        elif ext == "net":
            ext = "pajek"

        writer = f"write_{ext}"

        if hasattr(self, writer):
            return getattr(self, writer)(filename, *args, **kwargs)

        raise NotImplementedError(f"No method found for writing {ext} files")

    def write_clu(
        self,
        filename: str | os.PathLike[str],
        states: bool = False,
        depth_level: int = 1,
    ) -> None:
        """Write result to a clu file.

        An existing file at ``filename`` is overwritten.

        See Also
        --------
        write_tree
        write_flow_tree

        Parameters
        ----------
        filename : str or os.PathLike
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        depth_level : int, optional
            The depth in the hierarchical tree to write.
        """
        core = self._writer_core()
        with _translate_engine_errors():
            core.writeClu(os.fspath(filename), states, depth_level)

    def write_tree(
        self, filename: str | os.PathLike[str], states: bool = False
    ) -> None:
        """Write result to a tree file.

        An existing file at ``filename`` is overwritten.

        See Also
        --------
        write_clu
        write_flow_tree

        Parameters
        ----------
        filename : str or os.PathLike
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        """
        core = self._writer_core()
        with _translate_engine_errors():
            core.writeTree(os.fspath(filename), states)

    def write_flow_tree(
        self, filename: str | os.PathLike[str], states: bool = False
    ) -> None:
        """Write result to a ftree file.

        An existing file at ``filename`` is overwritten.

        See Also
        --------
        write_clu
        write_tree

        Parameters
        ----------
        filename : str or os.PathLike
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        """
        core = self._writer_core()
        with _translate_engine_errors():
            core.writeFlowTree(os.fspath(filename), states)

    def write_newick(
        self, filename: str | os.PathLike[str], states: bool = False
    ) -> None:
        """Write result to a Newick file.

        An existing file at ``filename`` is overwritten.

        See Also
        --------
        write_clu
        write_tree

        Parameters
        ----------
        filename : str or os.PathLike
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        """
        core = self._writer_core()
        with _translate_engine_errors():
            core.writeNewickTree(os.fspath(filename), states)

    def write_json(
        self, filename: str | os.PathLike[str], states: bool = False
    ) -> None:
        """Write result to a JSON file.

        An existing file at ``filename`` is overwritten.

        See Also
        --------
        write_clu
        write_tree

        Parameters
        ----------
        filename : str or os.PathLike
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        """
        core = self._writer_core()
        with _translate_engine_errors():
            core.writeJsonTree(os.fspath(filename), states)

    def write_csv(
        self, filename: str | os.PathLike[str], states: bool = False
    ) -> None:
        """Write result to a CSV file.

        An existing file at ``filename`` is overwritten.

        See Also
        --------
        write_clu
        write_tree

        Parameters
        ----------
        filename : str or os.PathLike
        states : bool, optional
            If the state nodes should be included. Default ``False``.
        """
        core = self._writer_core()
        with _translate_engine_errors():
            core.writeCsvTree(os.fspath(filename), states)


class _NetworkWritersMixin(_WritersBase):
    """Writers that serialize the network input, not the partition."""

    __slots__ = ()

    def write_state_network(self, filename: str | os.PathLike[str]) -> None:
        """Write internal state network to file.

        An existing file at ``filename`` is overwritten.

        See Also
        --------
        write_pajek

        Parameters
        ----------
        filename : str or os.PathLike
        """
        core = self._writer_core()
        with _translate_engine_errors():
            core.network().writeStateNetwork(os.fspath(filename))

    def write_pajek(
        self, filename: str | os.PathLike[str], flow: bool = False
    ) -> None:
        """Write network to a Pajek file.

        An existing file at ``filename`` is overwritten.

        See Also
        --------
        write_state_network

        Parameters
        ----------
        filename : str or os.PathLike
        flow : bool, optional
            If the flow should be included. Default ``False``.
        """
        core = self._writer_core()
        with _translate_engine_errors():
            core.network().writePajekNetwork(os.fspath(filename), flow)


class _InfomapWritersMixin(_ResultWritersMixin, _NetworkWritersMixin):
    """The stateful ``Infomap``'s full historical writer set (kept through 2.x)."""

    __slots__ = ()
