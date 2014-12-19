#!/usr/bin/env python

"""
setup.py file for SWIG Infomap module
"""

from distutils.core import setup, Extension


infomap_module = Extension('_infomap',
	sources=['infomap_wrap.cpp',
	'src/Infomap.cpp',
	'src/infomap/FlowNetwork.cpp',
	'src/infomap/InfomapBase.cpp',
	'src/infomap/InfomapContext.cpp',
	'src/infomap/MemFlowNetwork.cpp',
	'src/infomap/MemNetwork.cpp',
	'src/infomap/MultiplexNetwork.cpp',
	'src/infomap/Network.cpp',
	'src/infomap/NetworkAdapter.cpp',
	'src/infomap/Node.cpp',
	'src/infomap/TreeData.cpp',
	'src/io/ClusterReader.cpp',
	'src/io/HierarchicalNetwork.cpp',
	'src/io/ProgramInterface.cpp',
	'src/io/TreeDataWriter.cpp',
	'src/io/version.cpp',
	'src/utils/FileURI.cpp',
	'src/utils/Logger.cpp'
	],

	)

setup (name = 'infomap',
	version = '0.1',
	author      = "SWIG Docs",
	description = """Infomap clustering algorithm""",
	ext_modules = [infomap_module],
	py_modules = ["infomap"],
	)