Result
======

.. currentmodule:: infomap

:class:`Result` is the immutable snapshot returned by :func:`infomap.run` and
:meth:`Infomap.run`. Scalar metrics are properties; node and tree collections
are methods with defaults (``modules(depth=1)``, ``nodes()``, ``to_dataframe()``).

.. autoclass:: Result
   :members:
   :inherited-members:

:class:`TreeNode` is the lightweight, immutable per-node view yielded by
:meth:`Result.nodes`.

.. autoclass:: TreeNode
   :members:
