Infomap Python documentation
============================

Infomap is a network community-detection method: it finds the modules that best
compress the flow of a random walk on your network. Three properties follow from
that flow-based view, the `map equation`_:

- **It models how a system is used, not just how it is wired.** Link direction
  and weight steer the walk, so a citation or transport network clusters by where
  flow actually goes.
- **It finds hierarchy with no resolution parameter to tune.** The number of
  nested levels is inferred directly from the data.
- **It spans a broad range of flow models:** multilayer, memory,
  temporal, metadata, and bipartite networks, all through the same objective.

.. _map equation: https://www.mapequation.org/publications.html

.. image:: /_static/hero-modular.svg
   :alt: A weighted example network partitioned by Infomap into four modules,
         nodes coloured by module and sized by flow
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
    result = infomap.run(graph, seed=123, num_trials=20, silent=True)

    print(result.num_top_modules, "modules")
    print(result.modules())

Continue to :doc:`installation` for the command-line tool and shell completion,
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

      Use Infomap with Scanpy and GraphRAG, and run it at scale on HPC
      schedulers.

   .. grid-item-card:: Robustness & reliability
      :link: robustness/index
      :link-type: doc

      Keep sparse or incompletely sampled networks from fragmenting into
      spurious modules with the regularized map equation.

External resources
------------------

- The `Infomap user guide <https://www.mapequation.org/infomap/>`_ on
  mapequation.org is the project portal and documents the command-line tool and
  the other interfaces (C++, R, JavaScript).
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
   workflows/index
   robustness/index

.. toctree::
   :hidden:
   :caption: Reference
   :maxdepth: 1

   faq
   api/index
   the-infomap-class
   examples/index
   references
   citing
   article-companion/index
