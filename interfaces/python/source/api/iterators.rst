Tree nodes and iterators
========================

.. currentmodule:: infomap

Walking an :class:`Infomap` partition yields nodes via these iterator types.
Each step yields an iterator positioned at the current node, proxying the
attributes of the underlying :class:`InfoNode` (available via ``current()``).

.. autoclass:: InfoNode
   :members:

State-node iterators
--------------------

.. autoclass:: InfomapIterator
   :members:

.. autoclass:: InfomapLeafModuleIterator
   :members:

.. autoclass:: InfomapLeafIterator
   :members:

Physical-node iterators
-----------------------

.. autoclass:: InfomapIteratorPhysical
   :members:

.. autoclass:: InfomapLeafIteratorPhysical
   :members:
