# Infomap Software Package

Infomap is a network clustering algorithm based on the
[Map equation](http://www.mapequation.org/publications.html#Rosvall-Axelsson-Bergstrom-2009-Map-equation).

For more info, see [www.mapequation.org](http://www.mapequation.org).

For a list of recent changes, see [CHANGELOG.md](CHANGELOG.md) in the source directory.

## Getting started

In a terminal with the GNU Compiler Collection installed,
just run `make` in the current directory to compile the
code with the included `Makefile`. Call `./Infomap` to run.

Run `./Infomap --help` or visit [options](http://www.mapequation.org/code.html#Options)
for a list of available options.

Infomap can be used both as a standalone program and as a library.
See the [examples](examples) folder for examples.


### Using Pip

Infomap is available on the Python Package Index PyPi. To install, run
```
pip install infomap
```

To upgrade, run
```
pip install --upgrade infomap
```

When the Python infomap package is installed, a binary called `infomap` is
available on the command line from any directory.

### Using Git

To download and compile the newest version from Github, clone the repository
by running
```
git clone git@github.com:mapequation/infomap.git
cd infomap
make
```

### Using NPM

An experimental web worker implementation is available on NPM.
To install it, run
```
npm install @mapequation/infomap
```


## Authors

Daniel Edler, Anton Eriksson, Martin Rosvall

For contact information, see http://www.mapequation.org/about


## Terms of use

Infomap is released under a dual licence.

To give everyone maximum freedom to make use of Infomap
and derivative works, we make the code open source under
the GNU Affero General Public License version 3 or any
later version (see LICENSE_AGPLv3.txt.)

For a non-copyleft license, please contact us.
