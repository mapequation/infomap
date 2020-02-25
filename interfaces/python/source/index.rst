Infomap Python API
==================

Infomap is a network clustering algorithm based on the `Map equation`_.

.. _Map equation: http://www.mapequation.org/publications.html#Rosvall-Axelsson-Bergstrom-2009-Map-equation


Installation
------------
Infomap is available on PyPI. To install, run::

    pip install infomap

If you want to upgrade Infomap, run::

    pip install --upgrade infomap


Usage
-----

Command line
^^^^^^^^^^^^

When the Python package is installed, an executable called ``infomap`` (with small i)
is available from any directory. To verify your installation, run::

    infomap -v

Command line usage is as follows::

    infomap [options] network_data destination

The various options are documented in the `Infomap documentation`_.

.. _Infomap documentation: http://mapequation.org/code.html

Python interface
^^^^^^^^^^^^^^^^

:doc:`/infomap`


.. * :ref:`genindex`

