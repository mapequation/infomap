# Thread-scaling the two-level search — discussion sketch

*Status: sketch for discussion, 2026-07-29. Nothing here is implemented in
`src/`. Numbers marked [4c] were measured this session on a 4-core/no-SMT
container with ±10–24% wall-clock noise (documented below); numbers marked
[DE] are from Daniel's interleaved measurements in `columnar_wip/` on the
`columnar-hierarchical-core` branch.*

## 1. Goal and regime

Two-level partitions of networks large enough that a single search should
occupy ~100 threads on one shared-memory machine. Infomap is already fast at
moderate scale; the target is the regime where it is not. Explicitly out of
scope here: distributed memory (GossipMap/InfoFlow territory), hierarchical
search (later, same machinery applies per level), and objective changes (the
atlas-equation track is closed — measured decision-flips from a stale global
`q` are zero even under a full sweep of drift, so the map equation itself is
not the blocker; see §4).

## 2. Why base this on the columnar engine, not master

Decision this sketch assumes (to be confirmed): **thread-scaling work targets
`ColumnarTwoLevel`, not the OO core.**

- `--inner-parallelization` is the experiment already run on the OO core:
  parallel proposal pass, serial commit pass, gated to leaf levels ≥ 10k
  nodes. On a 500k-node/2.5M-edge planted two-level run it buys 1.12× on 4
  threads with a slightly worse codelength [4c]; on other machines it loses
  outright. The obstacle is the data layout (pointer-linked `InfoNode`/
  `InfoEdge`, 48 B/link, ObjectPool) more than the algorithm, and any thread
  work there is discarded the day columnar becomes the default engine.
- `ColumnarLevel` (`src/core/ColumnarLevel.h`) is already the layout a
  lock-local or batch-parallel sweep needs: SoA unit aggregates
  (`flow/enter/exit` as separate arrays) + CSR adjacency. The move loop
  (`ColumnarTwoLevel::moveLoop`, `src/core/ColumnarMapEquation.cpp:713`) has
  exactly the shape of the standalone harness in
  `research/atlas-equation/bench/parallel_bench.cpp`, where the candidate
  designs below are already implemented and verified — they port 1:1.
- Cost of tracking Daniel's branch is ordinary feature-branch maintenance:
  fork point is v2.15.0 (2026-07-13), 53 own commits, and a trial merge of
  current master produced 5 conflicted files [4c].
- Columnar `-C` at **one** thread already beats master OO at four
  (18.7 s vs 22.1 s end-to-end on the 500k run) [4c], and `--two-level` is
  already wired to the columnar engine.

Reference numbers, 500k nodes / 2.49M edges, `-2 -N1 --seed 123` [4c]:

| config | end-to-end | search part | codelength |
|---|--:|--:|--:|
| master OO, serial | 24.7 s | ~18.8 s | 12.4693 |
| master OO, `--inner-parallelization`, 4t | 22.1 s | ~16.2 s | 12.4725 |
| columnar-branch OO, serial | 22.8 s | ~16.9 s | 12.4693 |
| columnar `-C`, serial | 18.7 s | ~12.8 s | 12.4693 |
| (both binaries, `--no-infomap`) | 5.9 s | — | — |

## 3. The core design: batch-parallel move sweep

Two candidate sweep designs, both verified in the standalone harness. They
share all infrastructure (proposal kernel, dirty tracking, verification
sweeps) and differ only in how commits are ordered.

### D1 — deterministic disjoint batches (proposed default)

Per sweep round:

1. **Propose** (parallel, read-only): every dirty unit computes its best move
   against the round-start state. Per unit this is the existing per-candidate
   delta with `enterFlow`/`enterFlow_log_enterFlow` passed in — the columnar
   delta functions already take them as arguments, so "price against the
   round-start `q`" is the current call signature, unchanged.
2. **Select** (deterministic): order proposals by `(Δ, unit id)` and take the
   greedy-maximal subset whose `(old, new)` module pairs are pairwise
   disjoint. A serial marking pass over the sorted proposals suffices
   (memory-bound, ~ms at millions of proposals; parallelizable later if it
   ever shows up in a profile).
3. **Apply** (parallel): disjointness makes the commit a scatter-assign of
   precomputed after-aggregates — no locks, no read-modify-write. Reprice the
   global term once: `ΔL = Σ per-pair parts + plogp(q + Σδq) − plogp(q)`.
   This is **exact** for the map equation over a disjoint batch (verified to
   2.5e-15 over random batches [4c]); the objective needs no approximation
   and no relaxation.
4. Movers + their neighbourhoods become the next round's dirty set.

Properties:

- **Bit-reproducible for a given seed, independent of thread count.**
  Proposals are functions of round-start state, selection is order-based,
  application is order-independent. For a scientific tool this is the
  headline property, and it is what RelaxMap-style designs give up.
- Exact codelength bookkeeping at every round (no drift; `|L − ΣΔ|` at the
  floating-point floor at any thread count in the harness [4c]).
- Cost: batch (Jacobi-style) acceptance needs more rounds than sequential
  Gauss–Seidel sweeps. In the harness this was the difference between 32
  and ~50 rounds at the top level; each round is fully parallel, so the
  trade is favourable once threads ≥ a handful. **Measure the round
  inflation on real networks early** — it is the main unknown constant.

### D2 — asynchronous two-lock sweep (opt-in throughput dial)

Fused propose+commit per unit under ordered per-module spinlocks, exact
recheck of the two-module part under the locks, `q` read at sweep start with
per-thread `δq` accumulators reduced and repriced exactly at the sweep
barrier. Monotone in the true objective; **not** reproducible across thread
counts (schedule-dependent trajectory). Justified only if D1's round
inflation turns out expensive on real networks. The stale-`q` risk is
measured and negligible: pricing against a sweep-start `q` flipped **0 of
27 581** acceptance decisions on the 200k instance even with a full sweep's
drift (per-move error ~3e-8, shrinking as 1/n) [4c].

### Convergence and the dirty set (applies to both, and to serial)

Neighbour-of-mover dirty marking alone can stop short of a single-move local
optimum: a move changes the aggregates of both endpoint modules, which
stales the deltas of *non-adjacent* units bordering them. In the harness
this was worth up to a full planted-block resolution (35–58 modules → 20/20
on a small instance) [4c]. Fix that keeps the accelerator: converge only
when a **full verification sweep** over all units accepts nothing.
**To check:** whether `ColumnarTwoLevel::moveLoop`'s dirty set has the same
property today — if so this fix is independent of threading and worth
landing first, with its own quality measurement.

### Scratch memory at high thread counts

The current move loop uses dense per-module scratch (`dEnter/dExit`, size =
active units) with a touched-list reset — fine serially, but per-thread
copies do not scale: 100 threads × 2 × 8 B × active units is untenable at
leaf level on large networks. Options, in preference order: (a) per-thread
open-addressing map sized by max degree (candidates per unit are its
neighbours' modules only); (b) dense per-thread scratch only above the first
aggregation (module space is small there) and hashed scratch at leaf level;
(c) blocked unit ranges with block-local module remapping. Decide at M1.

## 4. Why not an objective change

Investigated first (see `research/atlas-equation/`): a separable objective
makes per-move commits exact without any global term. Verdict after
measurement: the map equation's global term costs ~1.4% serial share in a
faithful propose/commit implementation (Amdahl ceiling ~70×), the stale-`q`
decision error is zero in practice, and D1 above removes even that by exact
batch repricing — while the separable objective's bias (dwell-time
dispersion) misbehaves precisely on heterogeneous real networks. The
parallelization problem is data layout and scheduling, not the objective.

## 5. End-to-end budget: the search is not the whole run

On the 500k config, read + flow (power iteration) + write is **5.9 s of
18.7 s** [4c]. Perfect search scaling alone caps end-to-end at ~3× on that
shape. For the 100-thread goal the non-search pipeline must scale too:

| stage | today | plan |
|---|---|---|
| ingest/parse | serial | mmap + chunked parallel parse (later; often amortized across runs) |
| flow calculation | serial power iteration | parallel SpMV + reduction — embarrassingly parallel, do early |
| leaf CSR build | serial | parallel counting sort (standard) |
| move sweeps | serial | D1/D2 above |
| aggregation between passes | serial | per-thread partials + parallel CSR build |
| fine-tune / coarse-tune loops | serial | same sweep machinery, module-level |
| output write | serial | keep serial (rarely dominant) unless profiles say otherwise |

## 6. Composition with existing parallelism and gating

- **Trials**: `--parallel-trials` (trial-level OpenMP) composes with inner
  threading via a thread budget (inner threads × workers ≤ budget; the
  branch already carries thread-budget plumbing — align with it). Best-of-N
  with N ≥ threads is embarrassingly parallel and memory-hungry; inner
  threading is for when N is small and the network is huge — exactly the
  target regime.
- **Gating v1**: base objective only (`corr.empty()` in `moveLoop`), behind
  a flag, default off. Corrections (tele/meta/memory/regularized) each need
  their own concurrent-state story; the tele aggregates can get the same
  barrier treatment as `q`. Later milestones.
- **Determinism contract** (decide explicitly): D1 default keeps results
  identical across thread counts; D2 opt-in documents that it does not.

## 7. Milestones with go/no-go measurements

- **M0 — big-machine baseline** (before deep integration): port the harness
  skeleton (D1 + D2 on synthetic + one real 1e8-edge network) to a 32–128
  thread machine. Measures: per-round scaling, selection-pass cost, lock
  contention (D2), NUMA sensitivity of the SoA arrays (first-touch policy).
  *Go/no-go: D1 per-round scaling ≥ 0.7 × ideal at 32 threads.*
- **M1 — D1 in `ColumnarTwoLevel`**: leaf-level sweep first, gated as above;
  verification-sweep convergence; bit-reproducibility tests (same seed, 1
  vs T threads); quality parity on Daniel's 13-config set + large
  synthetics. *Go/no-go: codelength parity within seed noise, round
  inflation ≤ ~2×.*
- **M2 — aggregation + module-level loops parallel** (removes the next
  serial stages inside the search).
- **M3 — parallel flow calculation** (SpMV power iteration; biggest
  non-search win, independent of M1/M2 — can go first if easier to land).
- **M4 — D2 dial**, only if M1 shows round inflation hurts on real networks.
- **M5 — corrections**, per-correction, tele first.

## 8. Open questions for this discussion

1. Is cross-thread-count bit-reproducibility a requirement (D1-only) or is a
   documented nondeterministic fast mode (D2) acceptable as opt-in?
2. Where in the columnar PR chain does this insert — after the leaf-CSR
   single-owner PR? Who owns the moveLoop refactor that splits
   propose/select/apply so both engines' loops stay maintainable?
3. Does the current columnar dirty-set converge to single-move local optima
   (§3, "to check")? If not, land the verification sweep independently?
4. Hardware for M0 — what machine, and is a 1e8–1e9-edge reference network
   with ground truth available, or do we standardize on a synthetic?
5. Target topology: one NUMA node at ~100 threads, or multi-socket? (Decides
   whether module-array sharding/first-touch enters M1 or waits.)
6. Memory model for parallel trials vs inner threads on the same budget —
   who wins when both are requested?
7. Scratch-memory choice at leaf level (§3 options a/b/c).

## Appendix: measurement provenance

- [4c] this session: 4 physical cores, no SMT, wall-clock spread up to 24%
  on bit-identical runs (documented in `research/atlas-equation/README.md`
  §7.3–7.4); controlled comparisons use identical-state single sweeps,
  best-of-N. Engine matrix: planted partition 500×1000, k_in 8, k_out 2,
  `--two-level --seed 123 -N1`, binaries: master @ 5aa854f3, columnar @
  f8634ae3.
- [DE] `columnar_wip/columnar-pr-performance-section.md` (13-network set,
  interleaved same-session A/B, min-of-k CPU under load) and
  `columnar_wip/columnar-rethink-notes.md` (F1–F22).
- Stale-`q` and batch-exactness measurements:
  `research/atlas-equation/prototype/atlas_numpy.py` and the session log —
  decision-flip test priced every proposal of a full sweep against the
  sweep-start vs post-sweep `q`.
