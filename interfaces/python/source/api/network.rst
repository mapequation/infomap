Network
=======

.. currentmodule:: infomap

:class:`Network` is the first-class input builder. Construct it from a graph
library with a ``from_*`` constructor, or build it incrementally with the
``add_*`` verbs, then partition it with :func:`infomap.run` or
:meth:`Network.run`.

.. autoclass:: Network
   :members:
   :inherited-members:
   :show-inheritance:
