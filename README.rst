Infomap
=======

Infomap is a network clustering algorithm based on the `Map equation`_.

For more info, see `mapequation.org/infomap`_.

For a list of recent changes, see `CHANGELOG.md`_ in the source directory.

.. _Map equation: https://www.mapequation.org/publications.html#Rosvall-Axelsson-Bergstrom-2009-Map-equation
.. _mapequation.org/infomap: https://www.mapequation.org/infomap
.. _Github: https://www.github.com/mapequation/infomap
.. _CHANGELOG.md: https://github.com/mapequation/infomap/blob/master/CHANGELOG.md
.. _Github issues: https://www.github.com/mapequation/infomap/issues

Getting started
---------------

In a terminal with the GNU Compiler Collection installed,
just run ``make`` in the current directory to compile the
code with the included ``Makefile``.

Call ``./Infomap`` to run.

Run ``./Infomap --help`` for a list of available options.

Infomap can be used both as a standalone program and as a library.
See the examples folder for examples.


Using Pip
---------

Infomap is available on the Python Package Index PyPi. To install, run::

    pip install infomap


To upgrade, run::

    pip install --upgrade infomap


When the Python infomap package is installed, a binary called ``infomap`` is
available on the command line from any directory.

Check the `API documentation`_ to get started.

.. _API documentation: https://mapequation.github.io/infomap/

Using Git
---------

To download and compile the newest version from Github, clone the repository
by running::

    git clone git@github.com:mapequation/infomap.git
    cd infomap
    make


Migrating from v0.x to v1.0
^^^^^^^^^^^^^^^^^^^^^^^^^^^

In v1.0 we have moved the old master branch to ``v0.x``.
If you have cloned Infomap before v1.0, get the master branch up-to-date by running::

    git checkout v0.x
    git branch -D master
    git checkout master


Using NPM
---------

An experimental web worker implementation is available on NPM.
To install it, run::

    npm install @mapequation/infomap


Authors
-------

Daniel Edler, Anton Eriksson, Martin Rosvall

For contact information, see `mapequation.org/about.html`_.

.. _mapequation.org/about.html: https://www.mapequation.org/about.html

Terms of use
------------

Infomap is released under a dual licence.

To give everyone maximum freedom to make use of Infomap
and derivative works, we make the code open source under
the GNU Affero General Public License version 3 or any
later version (see `LICENSE_AGPLv3.txt`_).

For a non-copyleft license, please contact us.

.. _LICENSE_AGPLv3.txt: https://github.com/mapequation/infomap/blob/master/LICENSE_AGPLv3.txt

