---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
---

# Running at scale (HPC)

{bdg-primary-line}`Workflow`

```{admonition} At a glance
:class: tip
Infomap trials are independent work units: run many in parallel on a
single node with `parallel_trials`, or split them across a scheduler job
array with `trial_offset` and merge the best result afterwards with
`python -m infomap.merge`.
```

## When one node isn't enough

Infomap is a stochastic optimiser, so more independent trials give a more
reliable partition (see {doc}`/working-with-infomap/running-and-options` for
why). That is cheap on small networks — tens of trials on a laptop — but on
real-world networks with millions of nodes and links each trial can take minutes,
and the hundreds of trials you may want are more than one node can finish in
reasonable wall-clock time.

Two strategies cover most HPC use cases:

1. **Single-node parallelism.** Your scheduler gives you a node with many
   cores. Run all trials on that node with `parallel_trials=True`.
   `num_threads` controls the thread budget; left unset (its default, the same
   as `"auto"`) it reads scheduler-set variables (`SLURM_CPUS_PER_TASK`,
   `OMP_NUM_THREADS`, cpuset) automatically.

2. **Job-array sharding.** Your network is large, or you want more trials
   than one node can finish in time. Divide the total trial budget across
   array tasks with `trial_offset`. Each task runs its slice of trials and
   writes a shard result file (`trial_results`). A lightweight
   post-processing step merges the shards and picks the global best
   without rerunning Infomap.

Both strategies produce the same result as a sequential run with the same
seed, total trial count, and algorithm settings.

## Trials as independent work units

Each task runs `num_trials` trials of its own (the *per-shard* count), and
`trial_offset` positions those trials within the global budget. If you want
100 trials in total and split them across four array tasks, each task gets
`num_trials=25`:

- task 0: trials 0–24, offset 0
- task 1: trials 25–49, offset 25
- task 2: trials 50–74, offset 50
- task 3: trials 75–99, offset 75

Each task uses seed `base_seed + (offset + i)` for its *i*-th local
trial. The ranges never overlap, so no two tasks duplicate work. After all
tasks finish, `infomap.merge` reads the four result JSON files, checks
that they came from the same network and algorithm configuration, and
writes the tree from the lowest-codelength trial.

The merge is an offline post-processing step. It reads JSON, copies one tree
file, and exits. It does not need the network, does not rerun
Infomap, and takes negligible time compared with the runs themselves.

## Threading and scheduler awareness

The `num_threads` option accepts a positive integer or the string `"auto"`, and
defaults to `"auto"` when left unset. In `"auto"` mode Infomap resolves the
thread budget from the first source that provides a value in this priority order:

1. `--num-threads` / `num_threads` explicit integer
2. Environment variable `INFOMAP_NUM_THREADS`
3. `SLURM_CPUS_PER_TASK` (set automatically by SLURM when you use
   `--cpus-per-task`)
4. `OMP_NUM_THREADS`
5. The process cpuset (cgroup limit)
6. Hardware thread count

Because `"auto"` is the default, your job script does not need to set
`num_threads` or forward scheduler environment variables manually: if the
scheduler sets `SLURM_CPUS_PER_TASK=8`, Infomap will use 8 threads. This prevents
the common mistake of allocating 8 cores but Infomap running with 64 threads
and fighting every other job on the node.

`parallel_trials=True` runs independent trials concurrently using OpenMP.
The number of concurrent workers is clamped to `min(num_trials, OpenMP
thread count)`, which `num_threads` sets. Peak memory scales with the worker
count, so check your memory allocation if you raise thread counts significantly. (The
{doc}`benchmark-performance <../examples/benchmark-performance>`
notebook has run-time and memory scaling curves to help plan allocations.)

`inner_parallelization=True` is an experimental alternative that
parallelises the node-move loop inside a single trial. It can improve
wall-clock time on very large graphs but may produce a slightly different
partition. If you set both, `parallel_trials` takes precedence and inner
parallelisation is disabled inside the trial workers.

## Sharding trials across jobs

The following cells demonstrate the sharding pattern locally in Python.
On a cluster, each shard would be a separate job-array task running the
native `Infomap` binary; here, we run all shards in the same process to
make the pattern self-contained and executable.

### Build the graph

```{code-cell} python
import infomap

# A small network: two dense groups connected by a single bridge.
# On a real network this would be a large file loaded with im.read_file().
edges = [
    # group A: nodes 0-3
    (0, 1), (1, 2), (2, 3), (3, 0), (0, 2), (1, 3),
    # group B: nodes 4-7
    (4, 5), (5, 6), (6, 7), (7, 4), (4, 6), (5, 7),
    # single cross-group bridge
    (3, 4),
]
```

### Run three shards and keep the best

```{code-cell} python
# On a cluster each entry below would be a separate job-array task.
# Each shard uses a distinct base seed so their trial seeds never overlap;
# the SLURM recipe below instead shares one seed and offsets with trial_offset.
shard_specs = [
    {"shard_id": 0, "seed": 10, "num_trials": 4},
    {"shard_id": 1, "seed": 20, "num_trials": 4},
    {"shard_id": 2, "seed": 30, "num_trials": 4},
]

from infomap import Network, run

best_codelength = float("inf")
best_result = None
best_shard_id = None

for spec in shard_specs:
    net = Network()
    for u, v in edges:
        net.add_link(u, v)
    result = run(
        net,
        num_trials=spec["num_trials"],
        seed=spec["seed"],
        silent=True,
    )

    print(
        f"shard {spec['shard_id']}: seed={spec['seed']}"
        f"  codelength={result.codelength:.6f}"
        f"  modules={result.num_top_modules}"
    )

    if result.codelength < best_codelength:
        best_codelength = result.codelength
        best_result = result
        best_shard_id = spec["shard_id"]

print()
print(f"Best shard: {best_shard_id}")
print(f"Best codelength: {best_codelength:.6f}")
print(f"Partition: {best_result.modules()}")
```

The two dense groups (`{0,1,2,3}` and `{4,5,6,7}`) form separate modules
regardless of seed. On large networks with ambiguous structure, trials can
produce meaningfully different codelengths, and the minimum across all
shards is the result you keep.

### What the merge step does at scale

The code cell above mimics the selection logic in memory. At cluster scale you do
not hold all `Infomap` objects at once — each job writes its shard JSON and exits
— so the real workflow uses the CLI or the Python helper shown next.

## Using `infomap.merge`

After all shard jobs finish, merge their result files into a final
`tree`/`clu` pair. You can do this from the shell:

```bash
python -m infomap.merge results_*.json \
    --out-name final \
    --output tree,clu \
    --require-complete-trials
```

Or programmatically from Python:

```python
from infomap.merge import merge_trial_results

summary = merge_trial_results(
    ["results_*.json"],
    out_name="final",
    formats=("tree", "clu"),
    require_complete=True,
)
print("winner trial:", summary["trial"])
print("codelength:  ", summary["codelength"])
print("outputs:     ", summary["outputs"])
```

`merge_trial_results` verifies that all shard files share the same
network and configuration fingerprint (so you cannot accidentally mix
results from different runs), selects the trial with the lowest
codelength, and writes the corresponding tree. Ties are broken by the
lowest global trial index to make the result independent of how you
partitioned the trials across shards.

`--require-complete-trials` / `require_complete=True` causes the merge to
fail if any global trial index in `[0, max]` is missing. Use this when
reproducibility matters; leave it off if one shard failed and you are
happy with the subset that completed.

## SLURM array-job recipe

The following SLURM batch script distributes 100 trials across four
array tasks (25 per task). Each task writes a shard result JSON. A small
post-processing step merges the shards.

```bash
#!/usr/bin/env bash
#SBATCH --job-name=infomap-shards
#SBATCH --array=0-3
#SBATCH --cpus-per-task=8
#SBATCH --mem=32G
#SBATCH --time=02:00:00
#SBATCH --output=logs/infomap_%A_%a.out
#SBATCH --error=logs/infomap_%A_%a.err

set -euo pipefail

INFOMAP=/path/to/Infomap          # native binary, built with OpenMP
NETWORK=/path/to/graph.net
OUT=/path/to/out
TRIALS_PER_SHARD=25
OFFSET=$((SLURM_ARRAY_TASK_ID * TRIALS_PER_SHARD))

mkdir -p "$OUT/shards/$SLURM_ARRAY_TASK_ID"

srun "$INFOMAP" "$NETWORK" "$OUT/shards/$SLURM_ARRAY_TASK_ID" \
    --num-trials "$TRIALS_PER_SHARD"  \
    --trial-offset "$OFFSET"           \
    --seed 123                         \
    --num-threads auto                 \
    --parallel-trials                  \
    --trial-results "$OUT/results_${SLURM_ARRAY_TASK_ID}.json" \
    --no-final-output                  \
    --silent
```

Key points about this script:

- `--num-threads auto` picks up `SLURM_CPUS_PER_TASK`, so the Infomap process
  uses exactly the cores the scheduler allocated.
- `--parallel-trials` runs the 25 per-shard trials concurrently across
  those cores.
- `--trial-results` writes the shard JSON along with a per-shard best-result
  tree; `--no-final-output` skips the aggregate default output (the merge
  produces the final result).
- All shards use the same `--seed 123`. Trial `i` in shard `k` uses seed
  `123 + offset_k + i`, which guarantees globally unique seeds.

After the array completes, submit a short post-processing job:

```bash
python -m infomap.merge "$OUT"/results_*.json \
    --out-name "$OUT/final"  \
    --output tree,clu        \
    --require-complete-trials
```

Monitor the array while it runs:

```bash
squeue -j <job-id>
tail -f logs/infomap_<job-id>_<task-id>.out
sacct -j <job-id> --format=JobID,State,ExitCode,Elapsed,MaxRSS
```

## Before you scale: a checklist

- Use `--num-threads auto` so the process respects scheduler core
  allocation without manual forwarding of environment variables.
- Use the same `--seed`, network file, and algorithm flags for all shards.
  Shard files with different network or config fingerprints will be
  rejected by the merge.
- Make `--num-trials` the *per-shard* count. Choose non-overlapping
  `--trial-offset` ranges.
- Run a short reference job (a single shard without `--no-final-output`)
  to confirm your build, options, and file paths work before submitting a
  large array.
- Check exit codes in your batch script (`set -euo pipefail`). A failed
  Infomap run that exits 0 would produce a missing shard file and silently
  pollute the merge result.
- `infomap.merge` can write only `tree` and `clu` output; link-bearing formats
  (`ftree`) require a full Infomap run with the original network.
- Use `--require-complete-trials` when reproducibility matters; the merge
  warns about gaps without it.

## API pointers

- {func}`infomap.run` is the main entry point; it accepts `num_trials`, `seed`,
  `num_threads`, `parallel_trials`, `trial_offset`, and `trial_results` as
  engine options.
- `result.codelength` is the codelength of the best solution found.
- `result.modules()` returns a `{node_id: module_id}` mapping for the top-level
  partition.
- {func}`infomap.merge.merge_trial_results` is the programmatic equivalent of
  `python -m infomap.merge`.
- {doc}`/working-with-infomap/running-and-options` covers the search and
  flow-model options (`seed`, `num_trials`, `converge`, `directed`, …); the
  parallelism options used here (`parallel_trials`, `inner_parallelization`,
  `num_threads`, `trial_offset`) are documented on this page and in the
  {doc}`Options reference </api/options>`.

## Going deeper

- **Companion notebook** `examples/notebooks/run-infomap-on-hpc.ipynb` is a
  complete end-to-end HPC walkthrough using the native binary, with distributed
  trial verification and SLURM monitoring commands.
- **Performance planning**: `examples/notebooks/benchmark-performance.ipynb` has
  empirical run-time and memory scaling curves to help you choose trial counts,
  thread budgets, and memory allocations before submitting large jobs.
- {doc}`/working-with-infomap/running-and-options` is the options guide for the
  search and flow-model settings that shape each trial before you scale it out.
