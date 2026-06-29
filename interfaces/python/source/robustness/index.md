# Robustness & reliability

How far should you trust a partition? Real network data are noisy and
incomplete, and Infomap will always return *some* partition. This section covers
the tools for telling a result that reflects real structure from one driven by
sampling noise.

- {doc}`Incomplete data and regularization <incomplete-data>`: when links are
  missing, standard Infomap overfits into many small spurious modules; the
  Bayesian regularized map equation adds a prior network so that only
  well-supported communities survive.

```{toctree}
:hidden:
:maxdepth: 1

incomplete-data
```
