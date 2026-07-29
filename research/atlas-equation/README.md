# The atlas equation: a block-autonomous sibling of the map equation

*Research note and prototype. Status: proposal, validated numerically at
prototype scale; not integrated into the Infomap core.*

## Summary

The atlas equation is an information-theoretic, flow-based objective for
community detection, built from the same coding philosophy as the map
equation but with one deliberate change: the adaptive index codebook is
replaced by **static module addresses** whose lengths are set by module flow
mass, $\ell_i = -\log_2 P_i$. The consequences:

1. **Same philosophy.** The objective is still an achievable per-step
   description rate of the random walk under a two-level codebook structure
   (in the same idealized coding sense as the map equation's entropy terms,
   see §2): minimizing it finds modules in which flow persists.
2. **Exactly characterized bias.** For every partition $\mathsf{M}$,
   $$L_A(\mathsf{M}) = L_M(\mathsf{M}) + q\, D_{KL}(\hat Q \,\|\, \hat P),$$
   where $L_M$ is the two-level map equation, $q$ is the total boundary
   flow, $\hat Q$ the distribution of module entries and $\hat P$ the
   distribution of module flow masses. The two objectives coincide exactly
   when module usage is proportional to module mass, and the gap is at most
   $q \log_2(1/\min_i P_i)$ — vanishing with the boundary flow on any
   family of partitions whose module masses stay bounded below (not
   uniformly over all partitions; see §3).
3. **Additive separability.** $L_A = \sum_i g(P_i, Q_i^{in}, Q_i^{out})$ with
   no global term. A node move touches exactly two module terms. This is the
   property the map equation lacks (its single coupling term
   $\operatorname{plogp}(q)$ is repriced by *every* move), and it is what
   makes the top-level search parallelize with per-module locks instead of a
   serial commit pass: commits become **linearizable** — every committed move
   improves the true objective, at any thread count.

In the prototype benchmark (undirected planted partition, 200k nodes, 4
cores, every mode driven to a verified single-move local optimum), the
fused parallel sweep under the atlas equation cuts the top-level phase from
7.4 s to 1.4 s on 4 threads **with sequential-grade bookkeeping
consistency at every thread count** ($|L - \sum\delta| \lesssim 10^{-12}$),
while the map equation with the faithful propose-parallel/commit-serial
scheme reaches 2.3× against the sequential baseline (its commit pass stays
a serial floor as cores grow, and snapshot staleness doubles its sweep
count), and the map equation under the same two-lock scheme as the atlas
equation loses consistency (drift $\sim 10^{-8}$, i.e. it no longer
optimizes its own objective). Partition quality between the two objectives is statistically
indistinguishable on the test suites (karate club: identical partition;
ring of cliques: identical resolution behavior up to 256 cliques; planted
partitions: equal recovery within noise).

The name: a *map* has one global legend that must be kept in sync with how
often every region is visited; an *atlas* is a book of self-contained charts,
each decodable on its own, with page numbers proportional to territory.

## 1. Motivation: the map equation has exactly one global term

The two-level map equation over a partition $\mathsf{M}$ of nodes
$\alpha$ with visit rates $p_\alpha$, module flow masses
$P_i = \sum_{\alpha \in i} p_\alpha$, module enter flows $Q_i^{in}$, exit
flows $Q_i^{out}$, and total boundary flow $q = \sum_i Q_i^{in}$, expands
(with $\operatorname{plogp}(x) = x \log_2 x$) to

$$
L_M(\mathsf{M}) =
\underbrace{\operatorname{plogp}(q) - \sum_i \operatorname{plogp}(Q_i^{in})}_{\text{index codebook}}
\; + \;
\underbrace{\sum_i \Big[ \operatorname{plogp}(P_i + Q_i^{out}) - \operatorname{plogp}(Q_i^{out}) \Big] - \sum_\alpha \operatorname{plogp}(p_\alpha)}_{\text{module codebooks}} .
$$

Every term is a sum of per-module quantities **except**
$\operatorname{plogp}(q)$. In the implementation this is explicit:
`calculateCodelengthFromCodelengthTerms()` in `src/core/MapEquation.h`
assembles the codelength from per-module sums plus the single global
`enterFlow_log_enterFlow = plogp(enterFlow)`, and
`updateCodelengthOnMovingNode()` ends by recomputing exactly that scalar
after *every* accepted move.

That one nonlinear global term is the entire reason the move loop resists
parallelization:

- The move delta (`getDeltaCodelengthOnMovingNode`) needs the current global
  `enterFlow`; any concurrently committed move anywhere in the network
  invalidates it.
- Infomap's inner parallelization
  (`InfomapOptimizer::tryMoveEachNodeIntoBestModuleInParallel`) therefore
  splits each sweep into a **parallel, read-only proposal pass** against a
  sweep-start snapshot and a **strictly serial commit pass** that rechecks
  each proposal against the live state. The serial pass is Amdahl's-law
  overhead that grows precisely when the sweep is productive (early sweeps
  from singletons, i.e. the expensive top-level phase), and proposals go
  stale across the barrier, costing extra sweeps.

Conceptually the coupling is no accident: the index codebook is the
*adaptive* (Shannon-optimal) code for module entries, and optimal adaptive
codes need global usage statistics. The statistical coupling in the
objective and the synchronization bottleneck in the search are the same
thing. That suggests the fix belongs in the objective, not the algorithm.

## 2. Definition

**Setup.** Ergodic random walk with stationary visit rates $p_\alpha$,
$\sum_\alpha p_\alpha = 1$ (for directed networks the usual
teleportation-smoothed flows). Two-level partition $\mathsf{M}$; per module
$i$: flow mass $P_i$, enter flow $Q_i^{in}$, exit flow $Q_i^{out}$.

**Coding scheme.** As in the map equation, each module has a local codebook
over its member nodes plus one exit word, used $P_i^{\circlearrowright} =
P_i + Q_i^{out}$ per step, entropy-coded from module-local statistics. The
change is the navigation between modules: instead of one adaptive index
codebook over entries, module $i$ has a **static address**, a codeword of
length

$$\ell_i = -\log_2 P_i .$$

These are idealized per-event information lengths, not literal binary
codeword lengths: an individual prefix codeword cannot have length
$-\log_2 0.3$. Since $\sum_i P_i = 1$ (Kraft equality for the address
distribution), the *rate* $\sum_i Q_i^{in} \ell_i$ is achievable in the
usual asymptotic sense — block or arithmetic coding over the event stream
with per-event overhead vanishing in the block length. This is the same
idealization the map equation already makes for all of its entropy terms;
the atlas equation neither gains nor loses anything by it. Operationally:
after each exit word the decoder reads the entered module's address,
decodable from the module-mass table alone.

**Objective** (the atlas equation):

$$
L_A(\mathsf{M})
= \sum_i Q_i^{in} \, \ell_i \; + \; \sum_i P_i^{\circlearrowright} H(\mathcal{P}^i)
= \sum_i g\big(P_i, Q_i^{in}, Q_i^{out}\big) \; - \; \sum_\alpha \operatorname{plogp}(p_\alpha),
$$

with the per-module functional

$$
g(P, Q^{in}, Q^{out}) = -\,Q^{in} \log_2 P
\; + \; \operatorname{plogp}(P + Q^{out}) - \operatorname{plogp}(Q^{out}),
$$

and the constant $-\sum_\alpha \operatorname{plogp}(p_\alpha)$ (node-level
entropy, invariant under moves) kept for reporting so that $L_A$ is a true
description length in bits per step. The module codebooks are *unchanged*
from the map equation — they were already module-local, both statistically
and algorithmically. Only cross-module navigation is made static.

**Interpretation.** $-\log_2 P_i$ is the Shannon codeword length for the
event "the walker is in module $i$" under the stationary distribution.
Equivalently: the static code is the optimal entry code for a walker that,
whenever it crosses a border, relocates according to the stationary
distribution ("border teleportation"). The atlas equation is the codelength
paid by a reader who models within-module dynamics exactly but treats
cross-module transitions as memoryless relocation. What it deliberately
forgoes is the information in *where* the walk crosses borders — and Theorem
1 shows that is exactly the KL gap.

## 3. Properties

**Theorem 1 (validity and exact relation to the map equation).**
$L_A$ is an achievable description rate in the block-coding sense of §2 —
the same sense in which $L_M$ is one — and

$$
L_A(\mathsf{M}) - L_M(\mathsf{M})
= q \, D_{KL}\big(\hat Q \,\|\, \hat P\big) \;\ge\; 0,
\qquad \hat Q_i = Q_i^{in}/q, \quad \hat P_i = P_i .
$$

*Proof.* The module codebook terms are identical. The index terms differ by
$-\sum_i Q_i^{in} \log_2 P_i - \big(\operatorname{plogp}(q) - \sum_i
\operatorname{plogp}(Q_i^{in})\big) = q \sum_i \hat Q_i \log_2 (\hat Q_i /
\hat P_i)$. $\blacksquare$

Corollaries:

- $L_A \ge L_M$ pointwise, with equality iff module usage is proportional
  to module flow mass ($\hat Q = \hat P$). On the symmetric two-triangle
  example the gap is exactly $0$; on `ninetriangles.net` it is $0.0025$
  bits.
- Since $\hat Q_i \le 1$, the gap obeys $q\,D_{KL}(\hat Q \| \hat P) \le
  q \log_2(1/\min_i P_i)$. On any family of partitions whose smallest module
  flow mass is bounded below, the disagreement therefore vanishes linearly
  in the boundary flow $q$. This is a *conditional* guarantee: it is not
  uniform over arbitrary partitions, because the mass of some module can
  shrink as $q$ does — with $P_{min} \sim 2^{-1/q}$ the product stays
  $\Theta(1)$ — so small boundary flow alone does not bound the gap.
- The bias is interpretable and arguably useful: partitions in which a
  module's entry rate is disproportionate to its size (small, heavily
  trafficked pass-through modules) pay a surcharge of
  $\log_2(\hat Q_i / \hat P_i)$ bits per entry.

**Theorem 2 (locality).** Moving node $v$ from module $a$ to module $b$
changes only $g(a)$ and $g(b)$. The delta is computable from
$(P, Q^{in}, Q^{out})$ of $a$ and $b$ plus $v$'s link flows to members of
$a$ and $b$, in $O(\deg v)$; commit updates the two triples in $O(1)$.
No global quantity exists.

*Proof sketch.* For $c \notin \{a, b\}$: $P_c$ is unchanged, and
$Q_c^{in/out}$ counts flow crossing $c$'s boundary, which depends only on
membership of $c$ — unchanged by the move. The index term of $c$ is
$-Q_c^{in} \log_2 P_c$, a function of $c$'s own aggregates only (this is
where the map equation instead has the shared $\operatorname{plogp}(q)$).
$\blacksquare$

**Theorem 3 (lock-local, linearizable parallel search).** Run the local
moving sweep concurrently; to commit a move $v: a \to b$, acquire the two
module locks in id order, recheck the delta, apply, release. Then every
committed move improves $L_A$ by more than the threshold $\varepsilon$, at
any thread count, so the parallel search is monotone and terminates.

*Proof sketch.* While holding both locks, membership of $a$ and $b$ cannot
change (any move with an endpoint in $\{a,b\}$ needs one of the held locks).
A concurrent move $u: c \to d$ with $c, d \notin \{a, b\}$ changes neither
the aggregates of $a, b$ nor the classification of any of $v$'s neighbours
as "in $a$", "in $b$", or "elsewhere" — a neighbour flipping between two
"elsewhere" modules leaves the delta unchanged. Hence the recheck under the
two locks is exact, the committed schedule is equivalent to some sequential
order of strictly improving moves, and $L_A$ is bounded below.
$\blacksquare$

For the map equation the same argument fails at one point: the delta
contains $\operatorname{plogp}(q_{after}) - \operatorname{plogp}(q)$, and
$q$ is written by every committed move everywhere. The benchmark's negative
control (`par-map-twolock`) makes this concrete: identical locking
discipline, but the summed committed deltas drift from the true codelength
change by $\sim 10^{-8}$ (and growing with contention), i.e. the search
silently stops optimizing its own objective.

**Extreme partitions.** One module: $q = 0$, so
$L_A = L_M = -\sum_\alpha \operatorname{plogp}(p_\alpha)$ (the one-level
codelength). All singletons (undirected, no self-links): $L_A = L_M + q\,
D_{KL}$ as always; both are maximally penalized by boundary terms. There is
no degenerate optimum introduced by the static addresses: growing a module
shortens its address but inflates its module codebook, the same trade-off
that balances the map equation.

**Resolution behavior.** For $r$ equal modules the static address costs
$\log_2 r$ bits per entry — the same $\log r$ growth as the map equation's
index entropy $H(\hat Q) = \log_2 r$; the two objectives' merge conditions
differ only through the KL term, which vanishes for symmetric structures.
So the atlas equation inherits the map equation's mild, logarithmic
resolution characteristics rather than modularity's square-root-of-total-
weight limit.
Empirically (prototype, ring of 5-cliques with unit links): both objectives
keep one clique per module for $r = 4$ up to $r = 256$ without a single
merge.

## 4. Parallel search designs enabled

Because all move-relevant state is the per-module triple
$(P_i, Q_i^{in}, Q_i^{out})$ — exactly the `FlowData{flow, enterFlow,
exitFlow}` Infomap already maintains per module — three sweep designs become
available, in increasing order of throughput:

1. **Serial commit (status quo).** The existing
   propose-parallel/commit-serial pass works unchanged, but every recheck
   loses its global dependency (per candidate: two evaluations of $g$ after
   hoisting the old-module side, versus a 7-wide plogp batch *plus* the live
   `enterFlow` read) and the "fast accept" condition (neither module touched
   this sweep) can be tracked per module pair instead of per sweep. Fully
   deterministic given the seed, independent of thread count.
2. **Colored parallel commit.** Since a commit touches two modules only,
   accepted proposals form a conflict graph over module pairs; color it and
   apply colors in parallel batches. Deterministic *and* parallel commits,
   at the cost of a coloring pass. Impossible for the map equation (every
   pair of commits conflicts through $q$).
3. **Fused two-lock sweep (maximum throughput).** Each thread proposes and
   commits immediately under ordered per-module spinlocks with an exact
   recheck (Theorem 3). No barriers, no proposal staleness across a commit
   pass, monotone at any thread count. The committed *schedule* depends on
   thread interleaving, so runs are not bit-reproducible across thread
   counts — the same trade the current inner parallelization already makes
   for proposals, but with the guarantee that every commit improves the true
   objective.

Design 2 has a second life as **vectorization**: a batch of accepted moves
with pairwise-disjoint module pairs is exactly a "color", and applying it is
a handful of numpy scatter-assignments — the after-aggregates of the touched
modules are already known from the proposal step, so commit is `P[old] =
old_p; P[new] = new_p; ...`. `prototype/atlas_numpy.py` implements the full
search this way (sparse-matmul proposals over dirty nodes, greedy-maximal
disjoint batches): the accepted deltas of every batch sum to the true
codelength change to machine precision, while the identical batch scheme
under the map equation drifts by $\sim 10^{-3}$ bits per sweep — the
vectorized twin of the two-lock-vs-global-term result in the C++ benchmark.

A convergence caveat that applies to every design (and to dirty-node
tracking in general, including the neighbour marking used by Infomap's move
loops): marking only the mover's neighbours dirty is not sufficient for
local optimality, because a move changes the aggregates of both endpoint
modules and thereby stales the deltas of *non-adjacent* nodes that border
them. Both prototype implementations therefore treat dirty tracking purely
as an accelerator and confirm convergence with full verification sweeps: a
level terminates only when a sweep over all nodes accepts nothing, so every
reported partition is a genuine single-move local optimum.

Two further structural benefits, independent of threading:

- **No shared read-modify-write per move.** Even the sequential sweep stops
  funneling every accepted move through one global scalar.
- **Aggregation and deeper levels inherit the property.** Module aggregates
  sum exactly under coarse-graining, so super-node moves at every
  aggregation level have the same two-module locality.

## 5. Hierarchical extension

Two natural variants; both keep every leaf-module codebook adaptive:

- **Atlas at the root only.** Replace just the top index codebook with
  static addresses. This is the minimal change that removes the global
  coupling: in the multilevel map equation, all module-of-modules codebooks
  below the root are already local to their supermodule (their users are the
  walkers inside it); only the root codebook is shared by everyone. Search
  at every level then touches at most two supermodule chains, and the
  top-level partition search — the phase that dominates wall time on large
  networks — decouples entirely.
- **Atlas everywhere (nested charts).** Give each submodule $s$ with parent
  $m$ a static address of length $-\log_2 (P_s / P_m)$ within the parent's
  chart. Full addresses then telescope: naming a leaf module from the root
  costs $-\log_2 P_{leaf}$ regardless of depth, like arithmetic coding of
  nested intervals. Conceptually cleaner; the treatment of exit words at
  intermediate levels (adaptive, as in the map equation, or static against
  the parent's residual mass) is a design detail to settle when formalizing
  the multilevel version.

The two-level core defined in §2 is the same under both variants.

## 6. Directed networks and teleportation

Nothing in §2 assumed undirectedness: enter and exit flows are tracked
separately ($Q^{in}$ prices addresses, $Q^{out}$ prices exit words), and
$p_\alpha$ comes from the usual teleportation-smoothed stationary flows. The
`DeltaFlow` bookkeeping and `addTeleportationFlow` hooks in the optimizer
apply verbatim — the delta corrections that Infomap applies symmetrically to
module enter and exit flows carry over unchanged (validated numerically for
directed flows in the prototype). Kraft-validity of the static addresses
only needs $\sum_i P_i = 1$.

## 7. Empirical validation

All numbers below are reproducible with the two programs in this directory
(pure-stdlib Python prototype; standalone C++17/OpenMP benchmark). Bits
throughout; `plogp` uses log2 as in `src/utils/infomath.h`.

### 7.1 Correctness of the math (`prototype/atlas_prototype.py`)

- **Move deltas exact** for both objectives, undirected and directed random
  networks, random partitions, 8 trials × 40 moves: max error $1.4 \times
  10^{-15}$ against recompute-from-scratch.
- **Theorem 1 identity** $L_A - L_M = q\,D_{KL}(\hat Q \| \hat P)$: residual
  $< 10^{-15}$ in every trial.
- **Baseline validated against the Infomap binary** (v2.15.0,
  `--two-level`): prototype $L_M$ on the binary's partition matches the
  reported codelength on `twotriangles.net` and `ninetriangles.net` to the
  printed precision ($< 5 \times 10^{-6}$).

### 7.2 Partition quality, atlas vs map equation

- **Karate club** (20 seeds, best-of): both objectives select the *same*
  3-module partition (NMI = 1.0), $L_M = 4.3118$, $L_A = 4.3227$ bits
  (one-level reference 4.7044).
- **Ring of 5-cliques**, $r \in \{4, \dots, 256\}$: optimal block size is
  one clique per module for both objectives at every $r$ (no resolution
  divergence).
- **Planted partitions** (10 blocks × 100 nodes, $k_{in} = 8$, 3 seeds,
  best-of, undirected):

  | $k_{out}/k_{in}$ | NMI map | NMI atlas | modules map/atlas |
  |---:|---:|---:|:---:|
  | 0.0625 | 0.993 | 0.993 | 10 / 10 |
  | 0.125  | 0.993 | 0.990 | 10 / 10 |
  | 0.25   | 0.985 | 0.990 | 11 / 10 |
  | 0.375  | 0.939 | 0.953 | 10 / 10 |
  | 0.5    | 0.944 | 0.940 | 11 / 10 |

  Directed planted partitions behave the same way (experiment 5 in the
  prototype output), with measured KL gaps at the atlas optimum of
  $0.002$–$0.035$ bits.

### 7.3 Parallel top-level search (`bench/parallel_bench.cpp`)

Planted partition, 200 blocks × 1000 nodes ($n = 2 \times 10^5$, ~1.2M
edges, $k_{in} = 8$, nominal $k_{out} = 2$; the generator's two cross-block
draws per node land closer to 4), Louvain from singletons, 4 cores. Every
level converges only when a full verification sweep over all nodes accepts
nothing (§4), so all modes end at single-move local optima. "L0 time" is
the wall time of the top-level (leaf) local-moving phase — the phase that
dominates large runs — including the verification sweeps; consistency is
$|L_{after} - (L_{before} + \sum \delta_{committed})|$ at the top level.

| mode | threads | L0 time | L0 commit | final $L_M$ | modules | consistency |
|:--|--:|--:|--:|--:|--:|--:|
| seq-map                  | 1 | 6.43 s | — | 14.745 | 278 | $2 \times 10^{-12}$ |
| seq-atlas                | 1 | 7.42 s | — | 14.743 | 250 | $3 \times 10^{-13}$ |
| par-map (serial commit)  | 1 | 14.31 s | 0.34 s | 15.337 | 289 | $1 \times 10^{-12}$ |
| par-map (serial commit)  | 2 | 6.55 s | 0.37 s | 15.337 | 289 | $1 \times 10^{-12}$ |
| par-map (serial commit)  | 4 | 2.80 s | 0.32 s | 15.337 | 289 | $1 \times 10^{-12}$ |
| par-atlas (two-lock)     | 1 | 7.43 s | — | 14.743 | 250 | $3 \times 10^{-13}$ |
| par-atlas (two-lock)     | 2 | 2.42 s | — | 14.748 | 251 | $8 \times 10^{-13}$ |
| par-atlas (two-lock)     | 4 | 1.36 s | — | 14.752 | 252 | $8 \times 10^{-13}$ |
| par-map-twolock (unsound)| 2 | 2.88 s | — | 14.749 | 285 | $5 \times 10^{-9}$ |
| par-map-twolock (unsound)| 4 | 1.42 s | — | 14.743 | 291 | $2 \times 10^{-8}$ |

Readings:

- **par-atlas**: 7.43 s → 1.36 s from 1 to 4 threads (the 1-thread run
  matches seq-atlas — the locking discipline is free when uncontended),
  with consistency at the sequential level for every thread count and
  equal final quality. The whole sweep — proposal, commit, and the
  verification sweeps — parallelizes; there is no serial floor. Wall-clock
  ratios above ~4× fold in trajectory effects: the asynchronous schedule
  changes which moves are proposed, so thread count alters the search path,
  not just its speed.
- **par-map (serial commit)**: the faithful scheme scales its proposal pass
  but keeps a ~0.33 s serial commit pass as a hard Amdahl floor, and
  snapshot staleness roughly doubles the sweeps to convergence (68 vs 32).
  Net effect at 4 threads: 2.3× over seq-map. (The final-quality gap of
  this mode is an artifact of this benchmark's plain Louvain pipeline
  lacking Infomap's tuning iterations, not a claim about Infomap.)
- **par-map-twolock**: scales like par-atlas but is unsound — the committed
  deltas drift from the true codelength change by $10^{-8}$, four orders of
  magnitude above everyone else's floating-point floor, and growing with
  contention. This isolates the cause: the same commit machinery is exact
  for the atlas equation and inexact for the map equation, so the obstacle
  is the objective's global term, nothing else.

## 8. Integration path into Infomap

The objective drops into the existing static-dispatch pattern
(`InfomapOptimizer<Objective>`; see the class comment in
`src/core/MapEquation.h`):

1. **`src/core/AtlasEquation.h`** — `class AtlasEquation final : private
   MapEquation<>`, following `LossyMapEquation`'s structure, gated by an
   `INFOMAP_FEATURE_ATLAS_EQUATION` macro while experimental.
   - Per-module state: none beyond the existing `m_moduleFlowData`
     (`FlowData.flow/enterFlow/exitFlow` are exactly $P, Q^{in}, Q^{out}$).
   - `calculateCodelengthTerms`/`FromCodelengthTerms`: index term becomes
     $\sum_i -Q_i^{in} \log_2 P_i$; drop `enterFlow_log_enterFlow`.
   - `getDeltaCodelengthOnMovingNode(+Hoisted)`: four evaluations of $g$;
     the old-side pair is hoistable per node exactly like the base class's
     `hoistOldSide`. Reuses `DeltaFlow` and `addTeleportationFlow` verbatim.
   - `updateCodelengthOnMovingNode`: two-module aggregate update only — the
     method stops writing any shared scalar, which is the hook the parallel
     commit needs.
   - `calcCodelength(parent)`: per-module reporting; for the hierarchical
     variant choose atlas-root or nested charts (§5).
2. **Factory**: one branch in `InfomapBase::initOptimizer()`
   (`src/core/InfomapBase.cpp`, next to the `--lossy` branch).
3. **Option surface**: a flag (e.g. `--atlas`) in
   `src/io/ParameterCatalog.cpp`, then `make build-binding-options` and the
   freshness check, per the documented generator workflow.
4. **Parallel sweeps**: phase 1, reuse the existing inner parallelization
   unchanged (correct for any objective). Phase 2, add the two-lock commit
   path for objectives that declare no global state — a natural trait to
   express in the optimizer template (e.g. a `constexpr bool
   hasGlobalCodelengthState` on the objective).

Until then, everything here runs standalone; nothing in `src/` is touched by
this note.

## 9. Relation to prior work

- **Map equation** (Rosvall & Bergstrom 2008): the parent framework; the
  atlas equation changes only the index codebook, from adaptive to static.
- **RelaxMap** (Bae, Halperin, West, Rosvall, Howe 2017) and **GossipMap**
  (Bae & Howe 2015): parallel/distributed map-equation search that *relaxes
  consistency* of the shared state. The atlas equation is the complementary
  move: change the objective so no shared state exists, keeping exactness.
- **Markov stability** (Delvenne, Yaliraki, Barahona 2010) and **CPM/Leiden**
  (Traag, Waltman, van Eck 2019): additively separable objectives — which is
  why parallel Louvain-family algorithms thrive on them — but not
  description-length-based. The atlas equation is an MDL-semantics member of
  the separable family.
- **Smart teleportation** (Lambiotte & Rosvall 2012): changes the *process*
  to tame directed flows; the atlas equation changes the *code*, and its
  static addresses are the coding twin of stationary relocation at borders
  (§2).
- **Regularized/Bayesian map equation** (Smiljanić, Edler, Rosvall 2020) and
  the in-repo **lossy map equation** (rate-distortion): orthogonal
  variations (priors; lossy node identity) that keep the adaptive index
  codebook; in principle composable with static addresses.
- **MapSim** (Blöcker, Smiljanić, Scholtes, Rosvall 2024): uses
  map-equation coding rates as inter-node similarities — kindred
  address-based reading of the coding hierarchy.

To our knowledge the specific construction — cross-entropy index coding
against the module-mass distribution, chosen to make the objective
additively separable — has not been proposed; a proper literature check
should precede any publication claim.

## 10. Open questions

1. **Systematic bias characterization.** The KL term penalizes
   entry-rate/size disproportion. On which real network families does that
   change partitions materially (e.g. hub-and-spoke, strongly heterogeneous
   module sizes, flow bottlenecks), and is the change ever preferable?
2. **Hierarchical formalization** (§5): exit-word treatment for nested
   charts; whether the telescoping property yields a cheaper multilevel
   search than recursive two-level.
3. **Alternative static references.** Addresses from node counts
   ($-\log_2(n_i/n)$) give a structure-only variant; addresses from
   teleportation weights connect to smart teleportation. Same separability,
   different biases.
4. **Deterministic parallel commits** via module-pair coloring (§4, design
   2): is the coloring overhead worth bit-reproducibility?
5. **Combination with Bayesian regularization**: the static address book has
   no usage parameters to estimate, which may simplify the prior story for
   sparse data.

## Reproducing

```bash
# Math validation + quality experiments (pure stdlib, ~4 min; add --quick for ~40 s)
python3 research/atlas-equation/prototype/atlas_prototype.py

# Vectorized batch search (needs numpy + scipy; add --quick for ~45 s)
python3 research/atlas-equation/prototype/atlas_numpy.py

# Parallel benchmark (standalone, needs OpenMP)
make -C research/atlas-equation/bench run
```

The prototype validates its map-equation baseline against the built
`./Infomap` binary when present (skips that section otherwise).
