Infomap Python API
==================

Infomap is a network community-detection method built on a distinctive idea: it
finds the modules that best compress the flow of a random walk on your network,
instead of counting edges against a null model. That flow-based view, the
`map equation`_, is what sets it apart:

- **It models how a system is used, not only how it is wired.** Link direction
  and weight steer the walk, so citation, transport, and information networks
  cluster by their real dynamics.
- **It finds hierarchy with no resolution parameter to tune.** Infomap reads the
  number of nested levels straight off the data.
- **It spans an unusually broad range of flow models:** multilayer, memory,
  temporal, metadata, and bipartite networks, all through the same objective.

This site is the authoritative home for using Infomap in Python: the ideas behind
the map equation, how to run it on your own networks, the deeper flow models,
common workflows, and the full API.

.. _map equation: https://www.mapequation.org/publications.html

.. image:: /_static/hero-karate.svg
   :alt: Zachary's karate club partitioned by Infomap, nodes coloured by module
   :align: center
   :width: 360px

Quick start
-----------

.. code-block:: bash

    pip install infomap

.. code-block:: python

    import networkx as nx
    import infomap

    graph = nx.karate_club_graph()
    result = infomap.run(graph, seed=123, num_trials=20)

    print(result.num_top_modules, "modules")
    print(result.modules())

Continue to :doc:`installation` for optional integrations and shell completion,
or jump to :doc:`quickstart` for the smallest Python API examples.

Where to go
-----------

.. grid:: 1 2 2 2
   :gutter: 3

   .. grid-item-card:: Concepts
      :link: concepts/index
      :link-type: doc

      What the map equation is and why it works: flow, codelength, and
      hierarchy, taught from the ground up.

   .. grid-item-card:: Working with Infomap
      :link: working-with-infomap/index
      :link-type: doc

      Build networks from your data, run Infomap, tune the options, and read
      and visualise the results.

   .. grid-item-card:: Flow models & representations
      :link: flow-models/index
      :link-type: doc

      Multilayer, memory, temporal, metadata, bipartite, and higher-order
      networks: Infomap's deeper modelling surface.

   .. grid-item-card:: Workflows & integrations
      :link: workflows/index
      :link-type: doc

      Compare with Louvain and Leiden, and use Infomap with Scanpy, GraphRAG,
      and on HPC.

External resources
------------------

- The `Infomap user guide <https://www.mapequation.org/infomap/>`_ on
  mapequation.org is the project portal and documents the command-line tool and
  the **other interfaces (C++, R, JavaScript)**.
- `PyPI project <https://pypi.org/project/infomap/>`_
- `GitHub repository <https://github.com/mapequation/infomap>`_ for
  source, issues, and discussions.
- `CHANGELOG <https://github.com/mapequation/infomap/blob/master/CHANGELOG.md>`_

.. toctree::
   :hidden:
   :maxdepth: 2

   installation
   quickstart
   concepts/index
   working-with-infomap/index
   flow-models/index
   robustness/index
   workflows/index

.. toctree::
   :hidden:
   :caption: Reference
   :maxdepth: 1

   faq
   api/index
   the-infomap-class
   references
   citing
   article-companion/index
