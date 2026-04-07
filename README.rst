|ci| |docs| |docker| |nightly|

Infomap
=======

Infomap is a network clustering algorithm based on the `Map equation`_.
This repository contains the native CLI, the Python package, the JavaScript web
worker, the Docker images, and the source for the published Python
documentation.

Start with `mapequation.org/infomap`_ for the user guide and
`CHANGELOG.md`_ for release notes.

.. |ci| image:: https://github.com/mapequation/infomap/actions/workflows/ci.yml/badge.svg
   :target: https://github.com/mapequation/infomap/actions/workflows/ci.yml
   :alt: CI

.. |docs| image:: https://github.com/mapequation/infomap/actions/workflows/docs.yml/badge.svg
   :target: https://github.com/mapequation/infomap/actions/workflows/docs.yml
   :alt: Docs

.. |docker| image:: https://github.com/mapequation/infomap/actions/workflows/docker-smoke.yml/badge.svg
   :target: https://github.com/mapequation/infomap/actions/workflows/docker-smoke.yml
   :alt: Docker smoke

.. |nightly| image:: https://github.com/mapequation/infomap/actions/workflows/nightly-quality.yml/badge.svg
   :target: https://github.com/mapequation/infomap/actions/workflows/nightly-quality.yml
   :alt: Nightly quality

.. _Map equation: https://www.mapequation.org/publications.html#Rosvall-Axelsson-Bergstrom-2009-Map-equation
.. _`mapequation.org/infomap`: https://www.mapequation.org/infomap
.. _`CHANGELOG.md`: https://github.com/mapequation/infomap/blob/master/CHANGELOG.md

Install
-------

Python package
^^^^^^^^^^^^^^

Install from `PyPI`_:

.. code-block:: bash

    pip install infomap

Upgrades use the usual `pip` flow:

.. code-block:: bash

    pip install --upgrade infomap

The package also installs the ``infomap`` CLI entry point.
The Python API reference lives at `Infomap Python API`_.

Quick start with Python:

.. code-block:: python

    from infomap import Infomap, InfomapOptions

    options = InfomapOptions(two_level=True, silent=True, num_trials=20)
    im = Infomap.from_options(options)
    im.add_link(0, 1)
    im.add_link(1, 2)
    im.run()

    print(im.num_top_modules, im.codelength)

.. _PyPI: https://pypi.org/project/infomap/
.. _`Infomap Python API`: https://mapequation.github.io/infomap/python/

JavaScript package
^^^^^^^^^^^^^^^^^^

The browser worker package is published on `NPM`_:

.. code-block:: bash

    npm install @mapequation/infomap

.. _NPM: https://www.npmjs.com/package/@mapequation/infomap

Docker
^^^^^^

Supported images are published on `Docker Hub`_:

- ``mapequation/infomap``
- ``mapequation/infomap:notebook``

Run the CLI image with:

.. code-block:: bash

    docker run -it --rm \
        -v "$(pwd)":/data \
        mapequation/infomap \
        [infomap arguments]

Or use the local Compose file:

.. code-block:: bash

    docker compose run --rm infomap

Start the notebook image with:

.. code-block:: bash

    docker run \
        -v "$(pwd)":/home/jovyan/work \
        -p 8888:8888 \
        mapequation/infomap:notebook \
        start.sh jupyter lab

.. _`Docker Hub`: https://hub.docker.com/r/mapequation/infomap

Build from source
-----------------

Building locally requires a working ``gcc`` or ``clang`` toolchain.

.. code-block:: bash

    git clone git@github.com:mapequation/infomap.git
    cd infomap
    make build-native

This creates the ``Infomap`` binary in the repository root.
Show the available CLI options with:

.. code-block:: bash

    ./Infomap --help

Maintainers should use:

- ``BUILD.md`` for local build and verification commands
- ``RELEASING.md`` for the release flow
- ``ARCHITECTURE.md`` for ownership and source-of-truth rules
- ``AGENTS.md`` for repo-local maintenance guidance

Feedback
--------

Questions, bug reports, and feature requests belong in `GitHub issues`_.

.. _`GitHub issues`: https://github.com/mapequation/infomap/issues

Authors
-------

Daniel Edler, Anton Holmgren, Martin Rosvall

For contact information, see `mapequation.org/about.html`_.

.. _`mapequation.org/about.html`: https://www.mapequation.org/about.html

Terms of use
------------

Infomap is released under a dual licence.

The code is available under the GNU General Public License version 3 or any
later version; see `LICENSE_GPLv3.txt`_.
For a non-copyleft license, please contact us.

.. _`LICENSE_GPLv3.txt`: https://github.com/mapequation/infomap/blob/master/LICENSE_GPLv3.txt
