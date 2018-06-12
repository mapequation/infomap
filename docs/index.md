# Infomap Software Package

Infomap is a network clustering algorithm based on the
[Map equation](http://www.mapequation.org/publications.html#Rosvall-Axelsson-Bergstrom-2009-Map-equation).
 
For more info, see [www.mapequation.org](http://www.mapequation.org).

Note: This is the page for the Infomap v1.0, which is currently in a beta testing phase. For the 0.x version, checkout [www.mapequation.org](http://www.mapequation.org)

## Getting started

### Command line version
To download and install, run
```
git clone git@github.com:mapequation/infomap.git
cd infomap
git checkout v1-beta
make
```

Run `./Infomap --help` or visit [options](http://www.mapequation.org/code.html#Options)
for a list of available options.

Infomap can be used both as a standalone program and as a library.
See the `examples` folder for examples.

We also include a [Jupyter notebook](examples/python/infomap-examples.ipynb)
with the examples that can be viewed online.


### Python:
Infomap v1.0.0-beta.1 is available on the Python Package Index PyPi. To install, run

```
pip install infomap
```

#### Simple example

```python
import infomap

# Command line flags can be added as a string to Infomap
infomapSimple = infomap.Infomap("--two-level --directed")

# Access the default network to add links programmatically
network = myInfomap.network()

# Add weight as optional third argument
network.addLink(0, 1)
network.addLink(0, 2)
network.addLink(0, 3)
network.addLink(1, 0)
network.addLink(1, 2)
network.addLink(2, 1)
network.addLink(2, 0)
network.addLink(3, 0)
network.addLink(3, 4)
network.addLink(3, 5)
network.addLink(4, 3)
network.addLink(4, 5)
network.addLink(5, 4)
network.addLink(5, 3)

# Run the Infomap search algorithm to find optimal modules
myInfomap.run()

print("Found {} modules with codelength: {}".format(myInfomap.numTopModules(), myInfomap.codelength()))

print("Result")
print("\n#node module")
for node in myInfomap.iterTree():
  if node.isLeaf():
    print("{} {}".format(node.physicalId, node.moduleIndex()))
```

#### Python API

#### Infomap

Create
```
"""
Create an Infomap instance.

Parameters
----------
flags : string
    Optional flags, same as in the command line version, e.g. "--directed"

Returns
-------
Infomap
    The Infomap instance

"""
myInfomap = infomap.Infomap(flags)
```

Run
```
"""
Run default search algorithm to find modules.
"""
myInfomap.run()
```

Iterate result tree

```
"""
Tree node iterator

Parameters
----------
moduleIndexLevel : int
    The depth from the root on which to advance the moduleIndex accessed from the iterator for a tree with multiple levels
    Set to 1 to have moduleIndex() return the coarsest level (top modules), set to 2 for second level modules, and -1 (default) for the finest level of modules (bottom level)

Returns
-------
InfomapIterator
    The InfomapIterator instance, merged with the Node instance, see InfomapIterator and InfoNode

"""
myInfomap.iterTree(moduleIndexLevel = -1)

# Example
for node in myInfomap.iterTree():
  print("{} {}".format(node.path(), node.physicalId))
```


Network
```
"""
Access default underlying network.

Returns
-------
Network
    The underlying network, configured as the infomap instance it belongs to

"""
network = myInfomap.network()
```


#### Network

Add nodes
```
"""
Add node.

Parameters
----------
id : unsigned int
    Node id
name : string
    Optional name
weight : float
    Optional teleportation weight
"""
network.addNode(id)
network.addNode(id, name)
network.addNode(id, weight)
network.addNode(id, name, weight)
```

Add links
```

"""
Add node.

Parameters
----------
source : unsigned int
    Source node id
target : unsigned int
    Target node id
weight : float
    Optional link weight, default 1.0
"""
network.addLink(source, target)
network.addLink(source, target, weight)
```


#### InfomapIterator
```

"""
The tree path for the iterator as a tuple of child node indices taken from the root to arrive at current node

Returns
-------
tuple
    The current tree path

"""
node.path()
```

```
"""
Module index of the node collected by the iterator, default finest level (see `InfomapIterator`).

Returns
-------
unsigned int
    The module index

"""
node.moduleIndex()

```

```
"""
The current depth from the start node in the iterator

Returns
-------
unsigned int
    The current depth

"""
node.depth()

```


#### InfoNode
```

"""
The state node id, equals physicalId for ordinary networks

Returns
-------
unsigned int
    The state node id

"""
node.stateId
```

```
"""
The physical node id

Returns
-------
unsigned int
    The id of the physical node

"""
node.physicalId
```

```
"""
The flow data of the node that defines:
node.data.flow
node.data.enterFlow
node.data.exitFlow

Returns
-------
FlowData
    The flow data

"""
node.data
```


## Authors:
Daniel Edler
Martin Rosvall

For contact information, see 
http://www.mapequation.org/about.html

For a list of recent feature updates, see
CHANGES.txt in the source directory.

## Terms of use:
The Infomap software is released under a dual licence.

To give everyone maximum freedom to make use of Infomap 
and derivative works, we make the code open source under 
the GNU Affero General Public License version 3 or any 
later version (see LICENSE_AGPLv3.txt.)

For a non-copyleft license, please contact us.
