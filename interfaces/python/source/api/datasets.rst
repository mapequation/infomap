Datasets
========

.. currentmodule:: infomap.datasets

The canonical example networks from the format documentation on
mapequation.org, bundled with the package. Each loader reads its file with the
same parser the Infomap CLI uses and returns a fresh
:class:`~infomap.Network`, ready to run::

    import infomap

    net = infomap.datasets.two_triangles()
    result = infomap.run(net)

:func:`modular_wd` and :func:`states` encode directed flow, so those loaders
return a ``Network`` pre-configured with ``--flow-model directed``; a
``flow_model`` passed at :func:`infomap.run` still wins.

Basic examples
--------------

.. autofunction:: two_triangles
.. autofunction:: nine_triangles

Weighted and directed
---------------------

.. autofunction:: modular_w
.. autofunction:: modular_wd

Bipartite
---------

.. autofunction:: bipartite

Multilayer
----------

.. autofunction:: multilayer
.. autofunction:: multilayer_intra_inter
.. autofunction:: multilayer_intra

Memory and state
----------------

.. autofunction:: states
