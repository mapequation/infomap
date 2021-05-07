Infomap
=======

Infomap is a network clustering algorithm based on the `Map equation`_.

In this branch, we implement the bipartite map equation with varying node-type memory (`DOI:10.1103/PhysRevE.102.052305`_, `arXiv:2007.01666`_).

.. _Map equation: https://www.mapequation.org/publications.html#Rosvall-Axelsson-Bergstrom-2009-Map-equation
.. _`mapequation.org/infomap`: https://www.mapequation.org/infomap
.. _`DOI:10.1103/PhysRevE.102.052305`: https://journals.aps.org/pre/abstract/10.1103/PhysRevE.102.052305
.. _`arXiv:2007.01666`: https://arxiv.org/abs/2007.01666


Compiling from source
---------------------

Installing Infomap from source requires a working ``gcc`` or ``clang`` compiler.

To download and compile the newest version from `Github`_, clone the repository, switch to the `feature/bipartite-mapequation` branch, and compile the code by running

.. code-block:: shell

    git clone git@github.com:mapequation/infomap.git
    cd infomap
    git checkout feature/bipartite-mapequation
    make

This creates the binary ``Infomap`` (you might want to rename it to InfomapBipartite or so), run it using::

    ./Infomap [options] network_data destination

For a list of options, run::

    ./Infomap --help


Running it:
-----------
To run the bipartite version of Infomap, you need to encode your network in the following format

.. code-block:: shell

    # comments...
    *Vertices n
    1 "node 1 name"
    2 "node 2 name"
    ...
    n "node n name"
    *Bipartite m
    a x 42
    b y 3.14
    ...

where
  - ``n`` is the number of nodes
  - ``m`` is the ID of the first right node
  - ``a`` is a left node with ``a < m``
  - ``b`` is a left node with ``b < m``
  - ``x`` is a right node with ``x ≥ m``
  - ``y`` is a right node with ``y ≥ m``
  - ``42`` and ``3.14`` are carefully chosen random weights for the two edges ``(a,x)`` and ``(b,y)``

Assuming your network is stored in a file called ``bipartite.net`` and you have renamed the binary to ``InfomapBipartite``, you can analyse it by running ``InfomapBipartite bipartite.net out`` where ``out`` is the folder where Infomap will place the result.

You can set the rate at which node types are forgotten with ``--node-type-flipping-rate <number>`` where ``<number>`` is a value between ``0`` and ``1``.



Feedback
--------

If you have any questions, suggestions or issues regarding the software,
please add them to `GitHub issues`_.

.. _Github issues: http://www.github.com/mapequation/infomap/issues

Authors
-------

Christopher Blöcker, Daniel Edler, Anton Eriksson, Martin Rosvall

For contact information, see `mapequation.org/about.html`_.

.. _`mapequation.org/about.html`: https://www.mapequation.org/about.html

Terms of use
------------

Infomap is released under a dual licence.

To give everyone maximum freedom to make use of Infomap
and derivative works, we make the code open source under
the GNU Affero General Public License version 3 or any
later version (see `LICENSE_AGPLv3.txt`_).

For a non-copyleft license, please contact us.

.. _LICENSE_AGPLv3.txt: https://github.com/mapequation/infomap/blob/master/LICENSE_AGPLv3.txt
