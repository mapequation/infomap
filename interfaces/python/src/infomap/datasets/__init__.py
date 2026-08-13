"""Bundled example networks.

The canonical Infomap example networks (``examples/networks/`` in the source
repository, mirrored from the format documentation on mapequation.org) packaged
so they are loadable from an installed package. Each loader reads its file with
the same parser the Infomap CLI uses and returns a fresh
:class:`~infomap.Network`, ready to run:

>>> import infomap
>>> net = infomap.datasets.two_triangles()
>>> net.run().num_top_modules
2

The loaders whose file encodes directed flow (:func:`modular_wd` and
:func:`states`) return a ``Network`` pre-configured with
``--flow-model directed`` -- the canonical way to run those examples. A
``flow_model`` passed at :meth:`~infomap.Network.run` still wins, because
per-run options are applied after the construction options.
"""

from __future__ import annotations

from importlib.resources import as_file as _as_file
from importlib.resources import files as _files

from .._core import Core as _Core
from ..network import Network as _Network  # public return type of every loader below

__all__ = [
    "bipartite",
    "modular_w",
    "modular_wd",
    "multilayer",
    "multilayer_intra",
    "multilayer_intra_inter",
    "nine_triangles",
    "states",
    "two_triangles",
]


def _load(filename: str, *, extra_args: str = "") -> _Network:
    args = "--silent --no-file-output"
    if extra_args:
        args = f"{args} {extra_args}"
    net = _Network(core=_Core(args))
    with _as_file(_files(__name__) / "data" / filename) as path:
        net.read_file(path)
    return net


def two_triangles() -> _Network:
    """Two triangles joined by a single bridge.

    The minimal example of community structure: 6 nodes named ``A``--``F`` and
    7 unweighted, undirected links forming two triangles connected by one
    bridge link. A random walker stays long within each triangle and rarely
    crosses the bridge, so the map equation finds the two triangles as
    modules.

    Returns
    -------
    Network
        A fresh network with 6 nodes and 7 links.

    Examples
    --------
    >>> import infomap
    >>> net = infomap.datasets.two_triangles()
    >>> net.num_nodes, net.num_links
    (6, 7)
    >>> net.run().num_top_modules
    2
    """
    return _load("twotriangles.net")


def nine_triangles() -> _Network:
    """Nine triangles with two-level hierarchical structure.

    27 nodes and 39 links: nine triangles grouped three-and-three into three
    super-groups, with unit-weight links inside each super-group and weaker
    0.8 links between them. The optimal solution is hierarchical: three
    super-group modules at the top level, each nesting its three triangles.
    Use it to explore :attr:`~infomap.Result.num_levels` and the multilevel
    tree.

    Returns
    -------
    Network
        A fresh network with 27 nodes and 39 links.

    Examples
    --------
    >>> import infomap
    >>> net = infomap.datasets.nine_triangles()
    >>> net.num_nodes, net.num_links
    (27, 39)
    >>> net.run(options={"seed": 123, "num_trials": 10}).num_top_modules
    3
    """
    return _load("ninetriangles.net")


def bipartite() -> _Network:
    """A small bipartite network with named nodes.

    Figure 2 in the input-formats documentation on mapequation.org: 3 ordinary
    nodes and 2 feature nodes with 4 weighted links between the node types,
    declared with a ``*Bipartite`` section (the feature nodes start at id 4).
    Infomap models bipartite flow directly on the two-mode structure.

    Returns
    -------
    Network
        A fresh bipartite network with 5 nodes and 4 links.

    Examples
    --------
    >>> import infomap
    >>> net = infomap.datasets.bipartite()
    >>> net.bipartite_start_id
    4
    """
    return _load("bipartite.net")


def multilayer() -> _Network:
    """A multilayer network in the explicit ``*Multilayer`` format.

    Figure 3 in the input-formats documentation on mapequation.org: 5 physical
    nodes named ``i``--``m`` in 2 layers, with all 12 intra-layer and 4
    inter-layer links given explicitly as ``layer node layer node weight``
    rows. The explicit format gives full control over movement between
    layers; compare :func:`multilayer_intra_inter` and
    :func:`multilayer_intra` for the constrained and relax-rate variants of
    the same network.

    Returns
    -------
    Network
        A fresh multilayer network with 6 state nodes (5 physical nodes
        across 2 layers) and 16 links.

    Examples
    --------
    >>> import infomap
    >>> net = infomap.datasets.multilayer()
    >>> net.num_nodes
    6
    """
    return _load("multilayer.net")


def multilayer_intra_inter() -> _Network:
    """A multilayer network in the ``*Intra``/``*Inter`` format.

    Figure 4 in the input-formats documentation on mapequation.org: the same 5
    physical nodes and 2 layers as :func:`multilayer`, but written as 12
    intra-layer links plus 2 ``*Inter`` rows (``layer node layer weight``)
    that constrain how a random walker switches layer at a physical node.
    Infomap expands the inter-layer weights into explicit multilayer links.

    Returns
    -------
    Network
        A fresh multilayer network with 5 physical nodes across 2 layers.

    Examples
    --------
    >>> import infomap
    >>> net = infomap.datasets.multilayer_intra_inter()
    >>> net.num_nodes
    6
    """
    return _load("multilayer_intra_inter.net")


def multilayer_intra() -> _Network:
    """A multilayer network with intra-layer links only.

    Figure 5 in the input-formats documentation on mapequation.org: the same 5
    physical nodes and 2 layers as :func:`multilayer`, but with only the 12
    intra-layer links. Inter-layer movement is generated by the relax rate
    (``multilayer_relax_rate``, default 0.15): the walker relaxes to a random
    layer at the current physical node with that probability.

    Returns
    -------
    Network
        A fresh multilayer network with 5 physical nodes across 2 layers.

    Examples
    --------
    >>> import infomap
    >>> net = infomap.datasets.multilayer_intra()
    >>> net.num_nodes
    6
    """
    return _load("multilayer_intra.net")


def states() -> _Network:
    """A memory network in the ``*States`` format.

    Figure 6 in the input-formats documentation on mapequation.org: 5 physical
    nodes named ``i``--``m`` carrying 6 state nodes with 16 weighted, directed
    links between the states. Two state nodes share the physical node ``i``,
    so where the walker goes next depends on where it came from -- the memory
    effect that state networks capture and that overlapping modules reveal.

    The returned ``Network`` is pre-configured with ``--flow-model directed``
    (the canonical way to run this example); pass ``flow_model`` at
    :meth:`~infomap.Network.run` to override.

    Returns
    -------
    Network
        A fresh state network with 6 state nodes on 5 physical nodes and 16
        links, pre-configured for directed flow.

    Examples
    --------
    >>> import infomap
    >>> net = infomap.datasets.states()
    >>> net.num_nodes
    6
    """
    return _load("states.net", extra_args="--flow-model directed")


def modular_w() -> _Network:
    """A weighted network with four modules.

    25 nodes (ids 0--24) and 43 weighted, undirected links forming four
    naturally connected groups. This is the network behind the hero figure of
    this documentation, sized so that module structure is visible yet
    non-trivial.

    Returns
    -------
    Network
        A fresh network with 25 nodes and 43 links.

    Examples
    --------
    >>> import infomap
    >>> net = infomap.datasets.modular_w()
    >>> net.num_nodes, net.num_links
    (25, 43)
    """
    return _load("modular_w.net")


def modular_wd() -> _Network:
    """A weighted, directed network.

    The directed variant of :func:`modular_w`: 25 nodes (ids 1--25) and 48
    weighted links declared as ``*Arcs``, including asymmetric pairs where
    the two directions carry different weights. Directed flow changes the
    module structure compared with the undirected variant, which is the point
    of comparing the two.

    The returned ``Network`` is pre-configured with ``--flow-model directed``
    (the canonical way to run this example); pass ``flow_model`` at
    :meth:`~infomap.Network.run` to override.

    Returns
    -------
    Network
        A fresh network with 25 nodes and 48 links, pre-configured for
        directed flow.

    Examples
    --------
    >>> import infomap
    >>> net = infomap.datasets.modular_wd()
    >>> net.num_nodes, net.num_links
    (25, 48)
    """
    return _load("modular_wd.net", extra_args="--flow-model directed")
