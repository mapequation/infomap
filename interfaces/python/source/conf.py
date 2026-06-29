# Configuration file for the Sphinx documentation builder.
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import importlib.util
from pathlib import Path

# -- Project information -----------------------------------------------------

project = "Infomap"
copyright = "2020, Daniel Edler, Anton Holmgren, Martin Rosvall"
author = "Daniel Edler, Anton Holmgren, Martin Rosvall"

version_module_path = (
    Path(__file__).resolve().parents[1] / "src" / "infomap" / "_version.py"
)
spec = importlib.util.spec_from_file_location("infomap_version", version_module_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
release = module.__version__
version = release

# -- General configuration ---------------------------------------------------

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.napoleon",
    "sphinx.ext.intersphinx",
    "sphinx.ext.githubpages",
    "sphinx_copybutton",
    "sphinxext.opengraph",
    "myst_nb",
]

templates_path = ["_templates"]
exclude_patterns = []

# -- Options for HTML output -------------------------------------------------

html_theme = "furo"
html_title = f"Infomap {release}"
html_static_path = ["_static"]
html_css_files = ["custom.css"]
html_js_files = ["analytics.js"]
html_logo = "https://mapequation.github.io/assets/img/twocolormapicon_whiteboarder.svg"
# TODO: add a real favicon (.ico or .svg) and re-enable:
# html_favicon = "_static/favicon.ico"

html_theme_options = {
    "source_repository": "https://github.com/mapequation/infomap/",
    "source_branch": "master",
    "source_directory": "interfaces/python/source/",
    "footer_icons": [
        {
            "name": "GitHub",
            "url": "https://github.com/mapequation/infomap",
            "html": "",
            "class": "fa-brands fa-github",
        },
    ],
}

# -- Autodoc / Napoleon ------------------------------------------------------

napoleon_numpy_docstring = True
napoleon_google_docstring = False
napoleon_include_init_with_doc = True

autodoc_default_options = {"exclude-members": "thisown"}

# -- Intersphinx -------------------------------------------------------------

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
    "pandas": ("https://pandas.pydata.org/docs/", None),
    "scipy": ("https://docs.scipy.org/doc/scipy/", None),
    "networkx": ("https://networkx.org/documentation/stable/", None),
    "igraph": ("https://python.igraph.org/en/main/", None),
}

# -- Copy button -------------------------------------------------------------

copybutton_prompt_text = r">>> |\.\.\. |\$ "
copybutton_prompt_is_regexp = True

# Execute rendered notebook pages and cache outputs between documentation builds.
nb_execution_mode = "cache"

# Fail the build if any rendered notebook raises during execution, so broken
# example notebooks surface as CI errors instead of silent warnings.
nb_execution_raise_on_error = True

# -- Open Graph --------------------------------------------------------------

ogp_site_url = "https://mapequation.org/infomap-python-docs/"
ogp_image = "https://mapequation.github.io/assets/img/twocolormapicon_whiteboarder.svg"
ogp_site_name = "Infomap Python API"
ogp_description_length = 200
