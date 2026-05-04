# LFR Known Misses

These fixtures are frozen LFR benchmark networks where default Infomap missed
planted structure in local exploratory runs, while the experimental
`--refine-before-aggregation --refine-min-module-size 16` path found lower
codelength partitions that were closer to the planted labels.

They are intended as small, reproducible challenge artifacts for future work.
They are not CI performance assertions.

## Files

- `lfr_n750_mu0.10_seed857000.net`
- `lfr_n750_mu0.10_seed857000.clu`
- `lfr_n1000_mu0.30_seed1047000.net`
- `lfr_n1000_mu0.30_seed1047000.clu`

The `.net` files use Pajek format with 1-based node ids. The `.clu` files store
the planted labels as `node_id module`, also 1-based.

## Generation

The graphs were generated with NetworkX `LFR_benchmark_graph`, converted to
simple undirected graphs, and self-loops were removed.

Common parameters:

- `tau1=3.0`
- `tau2=1.5`
- `max_iters=500`

Case-specific parameters:

| graph | n | mu | average_degree | min_community | seed |
|---|---:|---:|---:|---:|---:|
| `lfr_n750_mu0.10_seed857000` | 750 | 0.10 | 12 | 30 | 857000 |
| `lfr_n1000_mu0.30_seed1047000` | 1000 | 0.30 | 15 | 50 | 1047000 |

## Observed Results

Local exploratory command shape:

```bash
Infomap <network> <out-dir> --seed 123 --num-trials 20 --clu --tree
Infomap <network> <out-dir> --seed 123 --num-trials 20 --clu --tree \
  --refine-before-aggregation --refine-min-module-size 16
```

Observed results:

| graph | default codelength | refined codelength | delta | default AMI vs planted | refined AMI vs planted |
|---|---:|---:|---:|---:|---:|
| `lfr_n750_mu0.10_seed857000` | 9.137460000 | 9.067780000 | -0.069680000 | 0.000000000 | 0.966612032 |
| `lfr_n1000_mu0.30_seed1047000` | 9.507400000 | 9.477390000 | -0.030010000 | 0.779862851 | 0.836598401 |

These results are useful regression context for the refinement experiment, but
future algorithm changes should not be judged by these two cases alone.
