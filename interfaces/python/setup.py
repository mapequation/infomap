#!/usr/bin/env python
"""
setup.py file for compiling Infomap module
"""

from distutils.core import setup, Extension
from distutils.file_util import copy_file
from pkg_resources import parse_version
import sysconfig
import fnmatch
import os
import re
from sysconfig import get_config_var

cppSources = []
for root, dirnames, filenames in os.walk('.'):
    for filename in fnmatch.filter(filenames, '*.cpp'):
        cppSources.append(os.path.join('.', root, filename))

# Extract Infomap version
infomapVersion = ''
with open(os.path.join('src', 'io', 'version.cpp')) as f:
    for line in f:
        m = re.match(r'.+INFOMAP_VERSION = \"(.+)\"', line)
        if m: infomapVersion = m.groups()[0]

# Set minimum Mac OS X version to 10.9 to pick up C++ standard library
if get_config_var('MACOSX_DEPLOYMENT_TARGET') and not 'MACOSX_DEPLOYMENT_TARGET' in os.environ:
    if parse_version(get_config_var('MACOSX_DEPLOYMENT_TARGET')) < parse_version("10.9"):
        os.environ['MACOSX_DEPLOYMENT_TARGET'] = "10.9"
    else:
        os.environ['MACOSX_DEPLOYMENT_TARGET'] = get_config_var('MACOSX_DEPLOYMENT_TARGET')


infomap_module = Extension(
    '_infomap',
    sources=cppSources,
    extra_compile_args=[
        '-DAS_LIB',
        '-DPYTHON',
        '-Wno-deprecated-register',
        '-std=c++14'
        # '-stdlib=libc++', # Fix error: no member named 'unique_ptr' in namespace 'std'
    ])
        # '-mmacosx-version-min=10.10' # Fix clang: error: invalid deployment target for -stdlib=libc++ (requires OS X 10.7 or later)
    # ],
    # extra_link_args=[
    #     '-mmacosx-version-min=10.10'
    # ])

setup(
    name='infomap',
    version=infomapVersion,
    author="Team at mapequation.org",
    description="""Infomap clustering algorithm""",
    url="www.mapequation.org",
    ext_modules=[infomap_module],
    py_modules=["infomap"])

# Clean ABI Version Tagged .so Files
ext_suffix = sysconfig.get_config_var('EXT_SUFFIX')
if ext_suffix is None:
    ext_suffix = sysconfig.get_config_var('SO')
if ext_suffix is not None:
    copy_file('_infomap{}'.format(ext_suffix), '_infomap.so')