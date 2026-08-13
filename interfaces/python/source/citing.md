# Citing Infomap

When you use Infomap in published work, cite **both** the 2008 PNAS paper and
the MapEquation software package. The paper introduced the map equation; the
software entry identifies the implementation and version, for reproducibility.

The survey {cite:p}`smiljanic2026survey` is the best single reference for the
wider map-equation framework, its flow models, and its extensions. Cite it too
when your work builds on that broader treatment, and as the primary reference for
results that originate in the survey itself.

## BibTeX

```bibtex
@article{rosvall2008maps,
  title   = {Maps of random walks on complex networks reveal community structure},
  author  = {Rosvall, Martin and Bergstrom, Carl T.},
  journal = {Proceedings of the National Academy of Sciences},
  volume  = {105},
  number  = {4},
  pages   = {1118--1123},
  year    = {2008},
  doi     = {10.1073/pnas.0706851105},
}

@misc{mapequation2026software,
  title        = {{The MapEquation software package}},
  author       = {Daniel Edler and Anton Holmgren and Martin Rosvall},
  howpublished = {\url{https://mapequation.org}},
  year         = {2026},
}
```

For the version-stamped software entry (recommended for reproducibility), visit
<https://www.mapequation.org> and copy the BibTeX from the "How to cite" section
there. It includes the current release version.

## Citing extensions

If your analysis uses a specific flow model or network type, also cite the
paper that introduced that extension. Each chapter under
{doc}`/flow-models/index` and {doc}`/robustness/index` names its source paper in
its "Going deeper" section. The {doc}`References page </references>` collects
these papers.

## Going deeper

Full publication list: <https://www.mapequation.org/publications.html>
