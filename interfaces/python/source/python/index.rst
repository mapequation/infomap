:orphan:

Infomap Python API
==================

Infomap is a network clustering algorithm based on the `Map equation`_.

.. _Map equation: https://www.mapequation.org/publications.html#Rosvall-Axelsson-Bergstrom-2009-Map-equation

API Reference
-------------
.. toctree::
   :maxdepth: 2

   infomap

Installation
------------
Infomap is available on `PyPI`_. Install it with::

    pip install infomap

Upgrade an existing installation with::

    pip install --upgrade infomap

.. _PyPI: https://pypi.org/project/infomap/

Usage
-----

Command line
^^^^^^^^^^^^

Installing the Python package also provides an ``infomap`` executable.
To verify that the CLI is available, run::

    infomap -v

Command line usage is as follows::

    infomap [options] network_data destination

For a list of available options, run::

    infomap --help


Python package
^^^^^^^^^^^^^^

Import the package with::

    import infomap

A simple example:

.. code-block:: python

    from infomap import Infomap

    # Command line flags can be provided as a string
    im = Infomap("--two-level --directed")

    # Add weight as optional third argument
    im.add_link(0, 1)
    im.add_link(0, 2)
    im.add_link(0, 3)
    im.add_link(1, 0)
    im.add_link(1, 2)
    im.add_link(2, 1)
    im.add_link(2, 0)
    im.add_link(3, 0)
    im.add_link(3, 4)
    im.add_link(3, 5)
    im.add_link(4, 3)
    im.add_link(4, 5)
    im.add_link(5, 4)
    im.add_link(5, 3)

    # Run the search
    im.run()

    print(f"Found {im.num_top_modules} modules with codelength: {im.codelength}")

    print("Result")
    print("\n#node module")
    for node in im.tree:
        if node.is_leaf:
            print(node.node_id, node.module_id)

Please read the :doc:`/python/infomap` reference to learn more.

.. * :ref:`genindex`
