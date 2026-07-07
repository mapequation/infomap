from __future__ import annotations

import os
from typing import TYPE_CHECKING


if TYPE_CHECKING:
    from .._core import Core


class _InfomapWritersMixin:
    # Attribute provided by the composing ``Infomap`` host. Declared under
    # TYPE_CHECKING so pyright resolves these mixin accesses without affecting
    # runtime (annotations only; the host supplies the real value).
    if TYPE_CHECKING:
        _core: Core

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
        self._core.writeClu(os.fspath(filename), states, depth_level)

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
        self._core.writeTree(os.fspath(filename), states)

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
        self._core.writeFlowTree(os.fspath(filename), states)

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
        self._core.writeNewickTree(os.fspath(filename), states)

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
        self._core.writeJsonTree(os.fspath(filename), states)

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
        self._core.writeCsvTree(os.fspath(filename), states)

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
        self._core.network().writeStateNetwork(os.fspath(filename))

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
        self._core.network().writePajekNetwork(os.fspath(filename), flow)
