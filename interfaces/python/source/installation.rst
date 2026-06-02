Installation
============

Infomap is available on `PyPI`_. Install it with:

.. code-block:: bash

    pip install infomap

Upgrade an existing installation with:

.. code-block:: bash

    pip install --upgrade infomap

.. _PyPI: https://pypi.org/project/infomap/

Optional integrations
---------------------

Install optional integrations for common research workflows:

.. code-block:: bash

    pip install "infomap[networkx]"
    pip install "infomap[igraph]"
    pip install "infomap[pandas]"
    pip install "infomap[scipy]"
    pip install "infomap[anndata]"

Or install all optional integrations at once:

.. code-block:: bash

    pip install "infomap[all]"

Command line interface
----------------------

The package also installs the ``infomap`` executable. Verify that the CLI is
available with:

.. code-block:: bash

    infomap -v

Command line usage:

.. code-block:: bash

    infomap [options] network_data destination

For a list of available options, run:

.. code-block:: bash

    infomap --help

Interactive shell
-----------------

Start a Python shell with Infomap preloaded:

.. code-block:: bash

    infomap-shell

The shell imports ``Infomap``, ``InfomapOptions``, and ``MultilayerNode``.
It also creates ``im = Infomap(pretty=True)`` and provides ``summary()`` and
``options()`` helpers. Pass one network file to preload it:

.. code-block:: bash

    infomap-shell examples/networks/twotriangles.net

Shell completion
----------------

Install shell completion scripts manually with:

.. code-block:: bash

    mkdir -p ~/.zfunc
    infomap --completion zsh > ~/.zfunc/_infomap

    mkdir -p ~/.local/share/bash-completion/completions
    infomap --completion bash > ~/.local/share/bash-completion/completions/infomap

For Zsh, make sure ``~/.zfunc`` is in ``fpath`` and ``compinit`` is loaded from
``~/.zshrc``. For Bash, make sure ``bash-completion`` is sourced from
``~/.bashrc``.
