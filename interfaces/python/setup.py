import sys

import codecs
import fnmatch
import os
import shutil
import subprocess
import tempfile
from pkg_resources import parse_version
from setuptools import setup, Extension
from sysconfig import get_config_var

import package_meta


def have_openmp():
    # Create a temporary directory
    tmpdir = tempfile.mkdtemp()
    curdir = os.getcwd()
    os.chdir(tmpdir)

    # Get compiler invocation
    compiler = os.environ.get('CC', get_config_var('CC'))

    # make sure to use just the compiler name without flags
    compiler = compiler.split()[0]

    # Attempt to compile a test script.
    # See http://openmp.org/wp/openmp-compilers/
    filename = "test_omp.c"
    with open(filename, 'w') as f:
        f.write(
            "#include <omp.h>\n"
            "#include <stdio.h>\n"
            "int main() {\n"
            "#pragma omp parallel\n"
            "printf(\"Hello from thread %d, nthreads %d\\n\", omp_get_thread_num(), omp_get_num_threads());\n"
            "}"
        )

    try:
        with open(os.devnull, 'w') as fnull:
            exit_code = subprocess.call([compiler, '-fopenmp', filename], stdout=fnull, stderr=fnull)
    except OSError:
        exit_code = 1

    # Clean up
    os.chdir(curdir)
    shutil.rmtree(tmpdir)

    return exit_code == 0


here = os.path.abspath(os.path.dirname(__file__))

# Get the long description from the relevant file
with codecs.open(os.path.join(here, 'README.rst'), encoding='utf-8') as f:
    long_description = f.read()

sources = ['./infomap_wrap.cpp']
headers = []
for root, dirnames, filenames in os.walk('./src'):
    for filename in fnmatch.filter(filenames, '*.cpp'):
        sources.append(os.path.join(root, filename))
    for filename in fnmatch.filter(filenames, '*.h'):
        headers.append(os.path.join(root, filename))

# Set minimum Mac OS X version to 10.9 to pick up C++ standard library
if get_config_var('MACOSX_DEPLOYMENT_TARGET') and not 'MACOSX_DEPLOYMENT_TARGET' in os.environ:
    if parse_version(get_config_var('MACOSX_DEPLOYMENT_TARGET')) < parse_version("10.9"):
        os.environ['MACOSX_DEPLOYMENT_TARGET'] = "10.9"
    else:
        os.environ['MACOSX_DEPLOYMENT_TARGET'] = get_config_var('MACOSX_DEPLOYMENT_TARGET')

compiler_args = [
    '-DAS_LIB',
    '-DPYTHON',
    '-Wno-deprecated-register',
    '-Wno-deprecated-declarations',
    '-std=c++14',
]

link_args = []

if have_openmp():
    compiler_args.append('-fopenmp')
    link_args.append('-fopenmp')
else:
    print("Warning: building without OMP support")

if sys.platform == 'win32':
    # Not executed if we are on WSL
    compiler_args = [
        '/DAS_LIB',
        '/DPYTHON',
        '/DNOMINMAX',
    ]

infomap_module = Extension(
    '_infomap',
    sources=sources,
    include_dirs=['headers', 'headers/src', 'headers/src/core', 'headers/src/io', 'headers/src/utils'],
    language='c++',
    extra_compile_args=compiler_args,
    extra_link_args=link_args)

setup(
    # This is the name of your project. The first time you publish this
    # package, this name will be registered for you. It will determine how
    # users can install this project, e.g.:
    #
    # $ pip install sampleproject
    #
    # And where it will live on PyPI: https://pypi.org/project/sampleproject/
    #
    # There are some restrictions on what makes a valid project name
    # specification here:
    # https://packaging.python.org/specifications/core-metadata/#name
    name=package_meta.__name__,  # Required

    # Versions should comply with PEP 440:
    # https://www.python.org/dev/peps/pep-0440/
    #
    # For a discussion on single-sourcing the version across setup.py and the
    # project code, see
    # https://packaging.python.org/en/latest/single_source_version.html
    # version='1.0.0-beta.39',  # Required
    version=package_meta.__version__,  # Required

    # This is a one-line description or tagline of what your project does. This
    # corresponds to the "Summary" metadata field:
    # https://packaging.python.org/specifications/core-metadata/#summary
    description=package_meta.__description__,  # Required

    # This is an optional longer description of your project that represents
    # the body of text which users will see when they visit PyPI.
    #
    # Often, this is the same as your README, so you can just read it in from
    # that file directly (as we have already done above)
    #
    # This field corresponds to the "Description" metadata field:
    # https://packaging.python.org/specifications/core-metadata/#description-optional
    long_description=long_description,  # Optional

    # Denotes that our long_description is in Markdown; valid values are
    # text/plain, text/x-rst, and text/markdown
    #
    # Optional if long_description is written in reStructuredText (rst) but
    # required for plain-text or Markdown; if unspecified, "applications should
    # attempt to render [the long_description] as text/x-rst; charset=UTF-8 and
    # fall back to text/plain if it is not valid rst" (see link below)
    #
    # This field corresponds to the "Description-Content-Type" metadata field:
    # https://packaging.python.org/specifications/core-metadata/#description-content-type-optional
    #long_description_content_type='text/markdown',  # Optional (see note above)

    # This should be a valid link to your project's main homepage.
    #
    # This field corresponds to the "Home-Page" metadata field:
    # https://packaging.python.org/specifications/core-metadata/#home-page-optional
    url=package_meta.__url__,  # Optional

    # This should be your name or the name of the organization which owns the
    # project.
    author=package_meta.__author__,  # Optional

    # This should be a valid email address corresponding to the author listed
    # above.
    author_email=package_meta.__email__,  # Optional

    # Choose your license
    license=package_meta.__license__,

    # Classifiers help users find your project by categorizing it.
    #
    # For a list of valid classifiers, see https://pypi.org/classifiers/
    classifiers=package_meta.__classifiers__,

    # This field adds keywords for your project which will appear on the
    # project page. What does your project relate to?
    #
    # Note that this is a string of words separated by whitespace, not a list.
    keywords=package_meta.__keywords__,

    # You can just specify package directories manually here if your project is
    # simple. Or you can use find_packages().
    #
    # Alternatively, if you just want to distribute a single Python file, use
    # the `py_modules` argument instead as follows, which will expect a file
    # called `my_module.py` to exist:
    #
    #   py_modules=["my_module"],
    #
    # packages=find_packages(exclude=['contrib', 'docs', 'tests']),  # Required
    # packages=['infomap'],
    py_modules=["infomap", "package_meta"],

    # This field lists other packages that your project depends on to run.
    # Any package you put here will be installed by pip when your project is
    # installed, so they must be valid existing projects.
    #
    # For an analysis of "install_requires" vs pip's requirements files see:
    # https://packaging.python.org/en/latest/requirements.html
    # install_requires=['peppercorn'],  # Optional

    # List additional groups of dependencies here (e.g. development
    # dependencies). Users will be able to install these using the "extras"
    # syntax, for example:
    #
    #   $ pip install sampleproject[dev]
    #
    # Similar to `install_requires` above, these must be valid existing
    # projects.
    # extras_require={  # Optional
    #     'dev': ['check-manifest'],
    #     'test': ['coverage'],
    # },

    # If there are data files included in your packages that need to be
    # installed, specify them here.
    #
    # If using Python 2.6 or earlier, then these have to be included in
    # MANIFEST.in as well.
    # package_data={  # Optional
    #     'sample': ['package_data.dat'],
    # },

    # Although 'package_data' is the preferred approach, in some case you may
    # need to place data files outside of your packages. See:
    # http://docs.python.org/3.4/distutils/setupscript.html#installing-additional-files
    #
    # In this case, 'data_file' will be installed into '<sys.prefix>/my_data'
    # data_files=[('my_data', ['data/data_file'])],  # Optional

    # To provide executable scripts, use entry points in preference to the
    # "scripts" keyword. Entry points provide cross-platform support and allow
    # `pip` to create the appropriate form of executable for the target
    # platform.
    #
    # For example, the following would provide a command called `sample` which
    # executes the function `main` from this package when invoked:
    # entry_points={  # Optional
    #     'console_scripts': [
    #         'sample=sample:main',
    #     ],
    # },
    entry_points={  # Optional
        'console_scripts': [
            'infomap=infomap:main',
        ],
    },

    # List additional URLs that are relevant to your project as a dict.
    #
    # This field corresponds to the "Project-URL" metadata fields:
    # https://packaging.python.org/specifications/core-metadata/#project-url-multiple-use
    #
    # Examples listed include a pattern for specifying where the package tracks
    # issues, where the source is hosted, where to say thanks to the package
    # maintainers, and where to support the project financially. The key is
    # what's used to render the link text on PyPI.
    project_urls={  # Optional
        'Bug Reports': package_meta.__issues__,
        'Source': package_meta.__repo__,
    },

    # Check MANIFEST.in
    include_package_data=True,

    # C++ source files defined with the Extension module
    ext_modules=[infomap_module],

    python_requires='>=3',
)
