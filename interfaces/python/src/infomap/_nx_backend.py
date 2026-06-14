"""NetworkX backend entry point for Infomap.

Registers ``infomap`` as a NetworkX backend so that
``nx.community.infomap_communities(G, backend="infomap")`` -- or via
``nx.config.backend_priority = ["infomap"]`` -- dispatches here.

NetworkX defines ``infomap_communities`` as a backend-only API (it has no
default NetworkX implementation). This module provides the implementation,
delegating to :func:`infomap.find_communities`.

Any extra Infomap option may be passed as a backend-only keyword argument and
is forwarded to :class:`infomap.Infomap`, e.g.::

    nx.community.infomap_communities(
        G, backend="infomap", two_level=True, markov_time=1.5, flow_model="directed",
    )
"""

from __future__ import annotations

from typing import Any

from ._networkx import find_communities


class BackendInterface:
    @staticmethod
    def infomap_communities(
        G, weight="weight", seed=None, num_trials=1, **infomap_kwargs
    ):
        if G.is_multigraph():
            import networkx as nx

            raise nx.NetworkXNotImplemented(
                "the infomap backend does not support multigraphs"
            )

        options: dict[str, Any] = dict(infomap_kwargs)
        if seed is not None:
            options.setdefault("seed", seed)
        options.setdefault("num_trials", num_trials)
        # Constrain to standard-graph mode. ``find_communities`` auto-detects
        # state/multilayer networks from ``node_id``/``layer_id`` node
        # attributes; point those at sentinel names that no node carries so a
        # plain Graph/DiGraph is never silently treated as a state or
        # multilayer network (those are out of scope for this backend).
        # Advanced users can still opt in by passing ``node_id``/``layer_id``
        # explicitly as backend-only keyword arguments.
        options.setdefault("node_id", "__infomap_nx_no_state__")
        options.setdefault("layer_id", "__infomap_nx_no_layer__")
        return find_communities(G, weight=weight, **options)

    # Infomap consumes NetworkX graphs directly (it has no native graph type),
    # so conversion in both directions is the identity.
    @staticmethod
    def convert_from_nx(graph, **kwargs):
        return graph

    @staticmethod
    def convert_to_nx(obj, **kwargs):
        return obj


def get_info():
    return {
        "backend_name": "infomap",
        "project": "infomap",
        "package": "infomap",
        "url": "https://www.mapequation.org/infomap",
        "short_summary": "Flow-based community detection with the map equation.",
        "functions": {
            "infomap_communities": {
                "url": "https://www.mapequation.org/infomap",
                "additional_docs": (
                    "Minimizes the map equation. Extra Infomap options may be "
                    "passed as backend-only keyword arguments."
                ),
                "additional_parameters": {
                    "**infomap_kwargs : Any": (
                        "Options forwarded to infomap.Infomap, e.g. two_level, "
                        "flow_model, markov_time, regularized, variable_markov_time."
                    ),
                },
            },
        },
    }
