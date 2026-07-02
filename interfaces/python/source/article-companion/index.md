# Article companion

```{admonition} What these are
:class: note
These notebooks accompany the survey article *Community Detection with the Map
Equation and Infomap: Theory and Applications* ([doi:10.1145/3779648](https://doi.org/10.1145/3779648)),
section by section. They are more academic and article-aligned than the rest of
this site. **For a gentler, Python-first path, start with the chapters**:
{doc}`Concepts </concepts/index>`, {doc}`Working with Infomap </working-with-infomap/index>`,
{doc}`Flow models </flow-models/index>`, and {doc}`Workflows </workflows/index>`.
Treat these notebooks as deeper companion reading.
```

The notebooks live in the Infomap repository under
[`examples/notebooks/`](https://github.com/mapequation/infomap/tree/master/examples/notebooks)
and are numbered to match the survey's sections. Some require additional
research code or data-processing packages and are not executed as part of this
documentation.

## Foundations

- [3.1 The two-level map equation](https://github.com/mapequation/infomap/blob/master/examples/notebooks/3.1%20The%20two-level%20map%20equation.ipynb):
  the core objective. Gentler path: {doc}`The map equation </concepts/the-map-equation>`.
- [4.1 The two-level phase](https://github.com/mapequation/infomap/blob/master/examples/notebooks/4.1%20The%20Two-level%20Phase.ipynb):
  the search algorithm. Related: {doc}`Running Infomap </working-with-infomap/running-and-options>`.
- [4.3 Solution landscape](https://github.com/mapequation/infomap/blob/master/examples/notebooks/4.3%20Solution%20Landscape.ipynb):
  degenerate solutions and reliability. Related: {doc}`Reading results and iterating </working-with-infomap/results-and-iteration>`.

## Higher-order representations

- [5.1 Memory networks](https://github.com/mapequation/infomap/blob/master/examples/notebooks/5.1%20Memory%20Networks.ipynb).
  See also: {doc}`Memory and state networks </flow-models/memory-and-state>`.
- [5.2 Multilayer networks](https://github.com/mapequation/infomap/blob/master/examples/notebooks/5.2%20Multilayer%20Networks.ipynb).
  See also: {doc}`Multilayer networks </flow-models/multilayer>`.
- [5.3 Modeling temporal data](https://github.com/mapequation/infomap/blob/master/examples/notebooks/5.3%20Modeling%20Temporal%20Data.ipynb).
  See also: {doc}`Temporal networks </flow-models/temporal>`.
- [5.4 Modeling multi-body interactions](https://github.com/mapequation/infomap/blob/master/examples/notebooks/5.4%20Modeling%20Multi-body%20Interactions.ipynb).

## Metadata, bipartite, and incomplete data

- [6.1 Networks with metadata](https://github.com/mapequation/infomap/blob/master/examples/notebooks/6.1%20Networks%20with%20Metadata.ipynb).
  See also: {doc}`Networks with metadata </flow-models/metadata>`.
- [6.2 Bipartite networks](https://github.com/mapequation/infomap/blob/master/examples/notebooks/6.2%20Bipartite%20Networks.ipynb).
  See also: {doc}`Bipartite networks </flow-models/bipartite>`.
- [7 Networks with incomplete data](https://github.com/mapequation/infomap/blob/master/examples/notebooks/7%20Networks%20with%20incomplete%20data.ipynb).
  See also: {doc}`Incomplete data and regularization </robustness/incomplete-data>`.

## Applications

```{admonition} Not yet covered by a narrative chapter
:class: tip
These four application topics do not have a dedicated chapter on this site yet.
For now, the companion notebooks below are the best Python starting point.
```

- [9.1 Map equation centrality](https://github.com/mapequation/infomap/blob/master/examples/notebooks/9.1%20Map%20Equation%20Centrality.ipynb):
  a community-aware centrality from the map equation's coding scheme.
- [9.2 Map equation similarity](https://github.com/mapequation/infomap/blob/master/examples/notebooks/9.2%20Map%20Equation%20Similarity.ipynb):
  MapSim node similarity and link prediction.
- [9.3 Infomap bioregions](https://github.com/mapequation/infomap/blob/master/examples/notebooks/9.3%20Infomap%20Bioregions.ipynb):
  delineating biogeographical regions from species occurrences.
- [9.4 Model selection with correlational data](https://github.com/mapequation/infomap/blob/master/examples/notebooks/9.4%20Model%20Selection%20with%20Correlational%20Data.ipynb):
  choosing a correlation threshold by codelength savings.

## Citing this work

If you use Infomap, cite the 2008 PNAS paper and the MapEquation software package;
see {doc}`How to cite Infomap </citing>`. To cite the survey itself,
use [doi:10.1145/3779648](https://doi.org/10.1145/3779648).
