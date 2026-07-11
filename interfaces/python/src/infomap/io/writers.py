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

    # File extension -> writer method. The available set is host-dependent: a
    # ``Result`` carries only the result-artifact writers, while ``.net``
    # (Pajek) serializes the *input network* and lives on ``Network`` /
    # ``Infomap``. The messages below are built from what THIS host can write
    # (filtered by ``hasattr``), so a ``Result`` never advertises ``.net``.
    _EXT_WRITERS = {
        "clu": "write_clu",
        "tree": "write_tree",
        "ftree": "write_flow_tree",
        "nwk": "write_newick",
        "net": "write_pajek",
        "json": "write_json",
        "csv": "write_csv",
    }

    def write(self, filename: str | os.PathLike[str], *args, **kwargs) -> None:
        """Write results to file, inferring the format from the extension.

        An existing file at ``filename`` is overwritten.

        Raises
        ------
        ValueError
            If ``filename`` has no extension to infer the format from.
        NotImplementedError
            If the file format is not supported on this host.

        Parameters
        ----------
        filename : str or os.PathLike
            The filename.
        """
        filename = os.fspath(filename)
        _, ext = os.path.splitext(filename)
        ext = ext[1:]  # remove the dot

        available = [
            f".{extension}"
            for extension, method in self._EXT_WRITERS.items()
            if hasattr(self, method)
        ]

        if not ext:
            raise ValueError(
                f"Cannot infer an output format from {filename!r}: the filename "
                f"has no extension. Add one of {', '.join(available)}, or call a "
                "specific writer such as write_clu()."
            )

        writer = self._EXT_WRITERS.get(ext)
        if writer is not None and hasattr(self, writer):
            return getattr(self, writer)(filename, *args, **kwargs)
        if writer is not None:
            # A known format this host cannot write: ``.net`` (Pajek) describes
            # the input network, so it lives on Network, not on the Result.
            raise NotImplementedError(
                f"a {ext!r} file serializes the input network, not the "
                "partition: write it from the Network with "
                "network.write_pajek(...) / network.write_state_network(...). "
                f"The Result writes partition artifacts: {', '.join(available)}."
            )
        raise NotImplementedError(f"No method found for writing {ext} files")

    def write_clu(
        self,
        filename: str | os.PathLike[str],
        states: bool = False,
        depth: int | None = None,
        *,
        depth_level: int | None = None,
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
        depth : int, optional
            The depth in the hierarchical tree to write. Accepted positionally,
            matching ``result.modules(depth=...)`` and
            ``result.to_dataframe(depth=...)``. ``1`` (default) is the top
            level, ``-1`` the bottom; it overrides ``depth_level`` when given.
        depth_level : int, optional
            Legacy keyword alias of ``depth`` (the historical ``write_clu``
            keyword); still accepted.
        """
        if depth is None:
            depth = depth_level if depth_level is not None else 1
        core = self._writer_core()
        with _translate_engine_errors():
            core.writeClu(os.fspath(filename), states, depth)

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

    @staticmethod
    def _no_partition_writer(name: str) -> NotImplementedError:
        # A Network holds only input; the partition artifacts come from a run.
        # These stubs turn a bare AttributeError into a pointer. They live on
        # the Network-only mixin; Infomap resolves the real writers first via
        # MRO (_ResultWritersMixin precedes _NetworkWritersMixin), so it keeps
        # its full historical writer set.
        return NotImplementedError(
            f"a Network serializes the input, not a partition, so it has no "
            f"{name}(). Run it and write from the Result -- e.g. "
            f"result = infomap.run(network); result.{name}(path). The Network "
            f"itself writes the input via write_pajek(...) / "
            f"write_state_network(...)."
        )

    def write_tree(self, *args, **kwargs):
        raise self._no_partition_writer("write_tree")

    def write_flow_tree(self, *args, **kwargs):
        raise self._no_partition_writer("write_flow_tree")

    def write_clu(self, *args, **kwargs):
        raise self._no_partition_writer("write_clu")

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
