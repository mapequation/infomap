#!/usr/bin/env python

"""
setup.py file for compiling Infomap module
"""

from distutils.core import setup, Extension
import fnmatch
import os
import re

cppSources = []
for root, dirnames, filenames in os.walk('.'):
    if root == 'src': cppSources.append(os.path.join(root, 'Infomap.cpp'))
    else:
        for filename in fnmatch.filter(filenames, '*.cpp'):
            cppSources.append(os.path.join(root, filename))

# Extract Infomap version
infomapVersion = ''
with open(os.path.join('src', 'io', 'version.cpp')) as f:
    for line in f:
        m = re.match( r'.+INFOMAP_VERSION = \"(.+)\"', line)
        if m: infomapVersion = m.groups()[0]

infomap_module = Extension('_infomap',
    sources=cppSources,
    extra_compile_args=['-DAS_LIB']
    )

setup (name = 'infomap',
    version = infomapVersion,
    author      = "Team at mapequation.org",
    description = """Infomap clustering algorithm""",
    url         = "www.mapequation.org",
    ext_modules = [infomap_module],
    py_modules = ["infomap"],
    )