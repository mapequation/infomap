# Configuration file for the Sphinx documentation builder.
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import importlib.util
import os
import sys
from pathlib import Path

_ext_dir = str(Path(__file__).resolve().parent / "_ext")
sys.path.insert(0, _ext_dir)
os.environ["PYTHONPATH"] = os.pathsep.join(
    [p for p in (_ext_dir, os.environ.get("PYTHONPATH", "")) if p]
)

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
    "sphinx.ext.autosummary",
    "sphinx.ext.napoleon",
    "sphinx.ext.intersphinx",
    "sphinx.ext.githubpages",
    "sphinx_copybutton",
    "sphinxext.opengraph",
    "sphinx_design",
    "sphinx_togglebutton",
    "myst_nb",
    "sphinx_reredirects",
    "sphinxcontrib.bibtex",
]

# Consolidated bibliography (see references.bib and the References page).
bibtex_bibfiles = ["references.bib"]
bibtex_reference_style = "author_year"

# Keep old URLs alive after the documentation overhaul. Each key is a retired
# page; the value is the new location, relative to the retired page's own
# directory. sphinx-reredirects writes a small meta-refresh stub at the old URL.
redirects = {
    "usage": "working-with-infomap/index.html",
    "export": "working-with-infomap/visualizing-and-exporting.html",
    "scanpy": "workflows/scanpy.html",
    "examples/quickstart": "../quickstart.html",
    "examples/compare-infomap-louvain-networkx": "../concepts/choosing-a-method.html",
    "examples/compare-infomap-louvain-leiden-igraph": "../concepts/choosing-a-method.html",
    "examples/compare-infomap-scanpy-workflow": "../workflows/scanpy.html",
    "examples/run-infomap-on-graphrag-tables": "../workflows/graphrag.html",
    "examples/run-infomap-on-hpc": "../workflows/hpc.html",
    # R1 re-architecture: merged/moved Concepts pages.
    "concepts/map-equation": "the-map-equation.html",
    "concepts/codelength-and-the-two-level-map": "the-map-equation.html",
    "concepts/how-to-cite": "../citing.html",
    "workflows/comparisons": "../concepts/choosing-a-method.html",
    # R2 re-architecture: incomplete-data moved to its own Robustness section.
    "flow-models/incomplete-data": "../robustness/incomplete-data.html",
}

# DOIs are persistent identifiers, but the publishers behind them (APS, PNAS,
# OUP, Science, ACM, MDPI) return 403 to the linkcheck bot. Trust the DOI rather
# than flag those as broken links.
linkcheck_ignore = [r"https://doi\.org/.*"]

myst_enable_extensions = [
    "colon_fence",   # ::: fenced directives (cards, admonitions)
    "dollarmath",    # $...$ and $$...$$ inline/block math
    "deflist",       # definition lists for option glossaries
    "attrs_inline",  # inline attributes on links/spans
]
myst_heading_anchors = 3  # auto slugs for h1-h3 so "going deeper" deep-links resolve

templates_path = ["_templates"]
# The hypergraphs chapter is kept in the source tree but excluded from the build
# on request. Remove it from this list to render it again.
exclude_patterns = ["flow-models/hypergraphs.md"]

# Number figures so captions read "Fig. N", matching the textbook feel.
numfig = True

# -- Options for HTML output -------------------------------------------------

html_theme = "furo"
html_title = f"Infomap {release}"
html_static_path = ["_static"]
html_css_files = ["custom.css"]
html_js_files = ["analytics.js"]
html_logo = "https://mapequation.github.io/assets/img/twocolormapicon_whiteboarder.svg"
html_favicon = "https://mapequation.github.io/assets/img/twocolormapicon_whiteboarder.svg"

# Hide Furo's "view page source" header icon (the eye); with the source in a
# different repo there is nothing useful to link to.
html_show_sourcelink = False

html_theme_options = {
    # No source_repository/branch/directory: the docs deploy from a separate repo,
    # so Furo's "view source" / "edit this page" header links 404 on GitHub.
    # Omitting these keys hides those two icons (the light/dark toggle stays).
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
autodoc_member_order = "groupwise"  # methods, then attributes; not one flat A-Z list

# Summary tables on the API pages link to the autodoc entries on the same page;
# we never want autosummary to generate stub pages.
autosummary_generate = False

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
