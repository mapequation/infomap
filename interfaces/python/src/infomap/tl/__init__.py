from __future__ import annotations

from collections.abc import Mapping
from typing import Any

# The GraphRAG tools are conceptually part of the ``tl`` namespace, so their
# public names are re-exported here. tl.graphrag has no module-scope
# third-party imports, so this keeps ``import infomap`` dependency-free.
from .graphrag import (
    GraphRAGGraph as GraphRAGGraph,
    GraphRAGRunResult as GraphRAGRunResult,
    read_graphrag as read_graphrag,
    run_graphrag_communities as run_graphrag_communities,
    write_graphrag_communities as write_graphrag_communities,
)

# A curated __all__: without it, the module-level imports (Any/Mapping/...)
# would leak into the ``infomap.tl`` namespace. ``infomap`` is the
# Scanpy-style entry point defined below.
__all__ = [
    "GraphRAGGraph",
    "GraphRAGRunResult",
    "infomap",
    "read_graphrag",
    "run_graphrag_communities",
    "write_graphrag_communities",
]


def _import_pandas() -> Any:
    # Thin delegate; the shared guard lives in infomap._optional. Only pandas
    # is imported here -- the entry point works with any AnnData-like object
    # without the anndata package -- so the hint names the pandas extra.
    from .._optional import require_pandas

    return require_pandas("AnnData support")


def _validate_anndata_like(adata: Any) -> int:
    missing = [
        attribute
        for attribute in ("obs", "obsp", "uns", "obs_names")
        if not hasattr(adata, attribute)
    ]
    if missing:
        raise ValueError(
            "`adata` must be an AnnData-like object with `.obs`, `.obsp`, "
            f"`.uns`, and `.obs_names`; missing `.{missing[0]}`."
        )

    obs_names = list(adata.obs_names)
    n_obs = getattr(adata, "n_obs", len(obs_names))
    if n_obs != len(obs_names):
        raise ValueError("`adata.n_obs` must match the length of `adata.obs_names`.")
    return n_obs


def _require_mapping(value: Any, *, name: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise ValueError(f"`{name}` must be a mapping.")
    return value


def _resolve_adjacency(
    adata: Any,
    *,
    adjacency: Any,
    neighbors_key: str | None,
    obsp: str | None,
) -> tuple[Any, str | None]:
    if adjacency is not None and (neighbors_key is not None or obsp is not None):
        raise ValueError("Pass only one of `adjacency`, `neighbors_key`, or `obsp`.")
    if neighbors_key is not None and obsp is not None:
        raise ValueError("Pass only one of `neighbors_key` and `obsp`.")

    if adjacency is not None:
        return adjacency, None

    if neighbors_key is not None:
        uns = _require_mapping(adata.uns, name="adata.uns")
        if neighbors_key not in uns:
            raise ValueError(f"`adata.uns[{neighbors_key!r}]` does not exist.")
        neighbors = _require_mapping(
            uns[neighbors_key], name=f"adata.uns[{neighbors_key!r}]"
        )
        connectivities_key = neighbors.get("connectivities_key")
        if not isinstance(connectivities_key, str) or not connectivities_key:
            raise ValueError(
                f"`adata.uns[{neighbors_key!r}]` must contain a non-empty "
                "`'connectivities_key'` string."
            )
        obsp = connectivities_key
    elif obsp is None:
        obsp = "connectivities"

    if obsp not in adata.obsp:
        raise ValueError(f"`adata.obsp[{obsp!r}]` does not exist.")
    return adata.obsp[obsp], obsp


def _validate_adjacency_shape(matrix: Any, *, n_obs: int) -> None:
    shape = getattr(matrix, "shape", None)
    if shape is None or len(shape) != 2:
        raise ValueError("`adjacency` must be a two-dimensional adjacency matrix.")
    if shape[0] != shape[1]:
        raise ValueError("`adjacency` must be a square adjacency matrix.")
    if shape[0] != n_obs:
        raise ValueError("`adjacency` shape must match `adata.n_obs`.")


def _copy_anndata_like(adata: Any) -> Any:
    copy_method = getattr(adata, "copy", None)
    if not callable(copy_method):
        raise ValueError("`adata` must provide a `.copy()` method when `copy=True`.")
    return copy_method()


def _module_labels(im: Any, mapping: dict[int, Any], obs_names: list[Any]) -> list[str]:
    modules = im._result.modules()
    modules_by_obs_name = {
        mapping[internal_id]: module_id for internal_id, module_id in modules.items()
    }
    try:
        return [str(modules_by_obs_name[obs_name]) for obs_name in obs_names]
    except KeyError as exc:
        raise RuntimeError(
            "Could not align Infomap modules with `adata.obs_names`."
        ) from exc


def infomap(
    adata: Any,
    *,
    adjacency: Any = None,
    directed: bool = False,
    use_weights: bool = True,
    key_added: str = "infomap",
    neighbors_key: str | None = None,
    obsp: str | None = None,
    copy: bool = False,
    args: str | None = None,
    **infomap_options: Any,
) -> Any:
    """Run Infomap on an AnnData observation graph.

    This function follows Scanpy ``tl`` conventions: by default it reads
    ``adata.obsp["connectivities"]``, writes categorical module labels to
    ``adata.obs[key_added]``, and stores run metadata in ``adata.uns[key_added]``.
    Scanpy itself is not imported.

    Parameters
    ----------
    adata : AnnData
        An AnnData-like object with ``.obs``, ``.obsp``, ``.uns``, and
        ``.obs_names``.
    adjacency : sparse matrix, optional
        Square observation-by-observation adjacency matrix to use instead of
        reading one from ``adata.obsp``. Mutually exclusive with
        ``neighbors_key`` and ``obsp``.
    directed : bool, optional
        Treat the adjacency as directed. Default ``False``.
    use_weights : bool, optional
        Use the adjacency values as link weights. If ``False``, every nonzero
        entry is treated as weight 1. Default ``True``.
    key_added : str, optional
        Key under which module labels are written to ``adata.obs`` and run
        metadata to ``adata.uns``. Default ``"infomap"``.
    neighbors_key : str, optional
        Read the adjacency from the ``obsp`` key named by
        ``adata.uns[neighbors_key]["connectivities_key"]``. Mutually
        exclusive with ``adjacency`` and ``obsp``.
    obsp : str, optional
        Read the adjacency from ``adata.obsp[obsp]``. Default
        ``"connectivities"``. Mutually exclusive with ``adjacency`` and
        ``neighbors_key``.
    copy : bool, optional
        Operate on (and return) a copy of ``adata`` instead of modifying it
        in place. Default ``False``.
    args : str, optional
        Raw Infomap CLI arguments passed to :class:`~infomap.Infomap`.
    **infomap_options
        Keyword arguments passed to :class:`~infomap.Infomap`. By default,
        ``silent=True`` and ``no_file_output=True`` are used unless
        explicitly overridden.

    Returns
    -------
    AnnData or None
        The annotated copy when ``copy=True``, otherwise ``None`` and
        ``adata`` is modified in place. In both cases,
        ``adata.obs[key_added]`` holds the module labels as a categorical
        (string categories sorted by integer value) and
        ``adata.uns[key_added]`` holds a dict with the run ``params``, the
        number of top modules ``n_modules``, and the ``codelength``.
    """
    from .._facade import Infomap

    pd = _import_pandas()
    n_obs = _validate_anndata_like(adata)
    if copy:
        adata = _copy_anndata_like(adata)
        n_obs = _validate_anndata_like(adata)
    matrix, resolved_obsp = _resolve_adjacency(
        adata,
        adjacency=adjacency,
        neighbors_key=neighbors_key,
        obsp=obsp,
    )
    _validate_adjacency_shape(matrix, n_obs=n_obs)

    # dict[str, Any]: the caller's infomap_options carry arbitrary option
    # value types, not just the bools of the two seeds.
    options: dict[str, Any] = {"silent": True, "no_file_output": True}
    options.update(infomap_options)
    im = Infomap(args=args, **options)
    obs_names = list(adata.obs_names)
    mapping = im.add_scipy_sparse_matrix(
        matrix,
        directed=directed,
        weighted=use_weights,
        node_ids=obs_names,
    )
    result = im.run()

    labels = _module_labels(im, mapping, obs_names)
    try:
        categories = sorted({int(label) for label in labels})
    except (TypeError, ValueError) as exc:
        # Infomap module labels are always ints; this is defensive so a
        # malformed label surfaces a clear message rather than a raw cast error.
        raise ValueError(
            "Infomap module labels must be integer-castable to build the "
            f"`adata.obs[{key_added!r}]` categories; got {labels!r}."
        ) from exc
    adata.obs[key_added] = pd.Categorical(
        labels,
        categories=[str(category) for category in categories],
    )
    adata.uns[key_added] = {
        "params": {
            "directed": directed,
            "use_weights": use_weights,
            "neighbors_key": neighbors_key,
            "obsp": resolved_obsp,
            "args": args,
            **options,
        },
        "n_modules": result.num_top_modules,
        "codelength": result.codelength,
    }

    if copy:
        return adata
    return None
