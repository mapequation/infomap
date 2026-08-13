# Robustness & reliability

Real network data are noisy and incomplete, and Infomap always returns *some*
partition. The techniques here keep sparse or under-sampled data from
fragmenting into small, spurious modules.

- {doc}`Incomplete data and regularization <incomplete-data>`: when links are
  missing, standard Infomap overfits into many small spurious modules. The
  Bayesian regularized map equation adds a prior network so that only
  well-supported communities survive.

```{toctree}
:hidden:
:maxdepth: 1

incomplete-data
```
