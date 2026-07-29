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
   when module usage is proportional to module mass, and because
   stationarity forces $Q_i^{in} \le P_i$, the gap is at most
   $q \log_2(1/q)$ — vanishing with the boundary flow uniformly over all
   partitions (§3).
3. **Additive separability.** Up to a move-invariant constant,
   $L_A = \sum_i g(P_i, Q_i^{in}, Q_i^{out})$ — a sum of per-module terms
   with no term coupling different modules. A node move touches exactly two
   of them. The map equation instead carries the coupling term
   $\operatorname{plogp}(q)$, which *every* move reprices, so an individual
   move's exact delta is never a function of local state alone. That is what
   lets the atlas equation commit under per-module locks instead of a serial
   pass: commits become **linearizable** — every committed move improves the
   true objective, at any thread count (§4 draws the precise line, which is
   about per-move exactness, not about batching).

In the prototype benchmark (undirected planted partition, 200k nodes, 4
physical cores, every mode driven to a verified single-move local optimum),
the fused two-lock sweep keeps $|L - \sum\delta|$ at the sequential
floating-point level ($\lesssim 10^{-12}$) **at every thread count**, and
converges in 32 sweeps against 53 for propose-parallel/commit-serial, which
has a sweep-start snapshot to go stale. Running the *same* two-lock scheme
under the map equation scales just as well but drifts by $\sim 10^{-8}$ —
it stops optimizing its own objective — which isolates the objective rather
than the machinery as the obstacle.

Be careful about the throughput half of the argument, though: measured
properly (identical state, one sweep, best of 15), per-sweep scaling on 4
cores is **indistinguishable between the two designs**, 3.7–4.4× for both.
The map equation's serial commit is only ~1.4% of a sweep, which caps its
speedup near $70\times$ asymptotically — real, but far beyond 4 cores. The
claim this note can support on this hardware is exactness and sweep count,
not wall-clock throughput (§7.4). Partition quality tracks the map equation
closely on every test used here — karate club: identical partition; ring of
cliques: identical resolution behavior up to 256 cliques; planted partitions:
recovery differences within seed-to-seed spread — though these are small
suites, not a systematic quality study.

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
after *every* accepted move. (The default objective is actually
`BiasedMapEquation`, which keeps a second global — `currentNumModules` — but
it is inert unless `--preferred-number-of-modules` or `--entropy-corrected`
is set, and with default options its codelength and deltas reduce exactly to
the base map equation. The argument below concerns the unbiased objective.)

That one nonlinear global term is the entire reason the move loop resists
parallelization:

- The move delta (`getDeltaCodelengthOnMovingNode`) needs the current global
  `enterFlow`; any concurrently committed move anywhere in the network
  invalidates it.
- Infomap's inner parallelization
  (`InfomapOptimizer::tryMoveEachNodeIntoBestModuleInParallel`) therefore
  splits each sweep into a **parallel, read-only proposal pass** against a
  sweep-start snapshot and a **strictly serial commit pass**. That pass
  re-gathers edge sums and recomputes the delta for any proposal whose old or
  new module was already touched earlier in the same pass, and *fast-accepts*
  the rest on their snapshot deltas
  (`src/core/InfomapOptimizer.h:887-909`). Codelength bookkeeping stays exact
  either way, because `updateCodelengthOnMovingNode` reprices the global term
  from the live `enterFlow` at commit; what the fast path gives up is the
  guarantee that the accepted move is still an improvement, since $q$ may
  have moved under it. The serial pass is Amdahl's-law overhead that grows
  precisely when the sweep is productive (early sweeps from singletons, i.e.
  the expensive top-level phase), and proposals go stale across the barrier,
  costing extra sweeps.

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
event "the walker is in module $i$" under the stationary distribution. The
static address book is thus the code that would be *optimal* for a walker
which, on crossing any border, relocates according to the stationary
distribution ("border teleportation"); applied to the real walk it is a
mismatched code, and the index cost is the cross-entropy
$q\,H(\hat Q, \hat P)$ rather than that reference process's own entropy
$q\,H(\hat P)$. Theorem 1 identifies the excess as exactly the KL
divergence: what the atlas equation forgoes is the information in *which*
module the walk enters, over and above how large that module is. As in the
map equation, "models the module interior" means an i.i.d. code over the
module's own usage distribution, not a model of its internal dynamics; the
change here is confined to cross-module navigation.

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
- **The gap vanishes with the boundary flow, uniformly over all
  partitions.** Stationarity forces $Q_i^{in} \le P_i$ for every module:
  $Q_i^{in} = \Pr[X_t \notin i,\, X_{t+1} \in i] \le \Pr[X_{t+1} \in i] =
  P_i$ (for the unrecorded-teleportation directed model the link flows omit
  teleportation steps, so the inequality only gains slack). Hence
  $\hat Q_i / P_i \le 1/q$ for every $i$, and since $\sum_i \hat Q_i = 1$,
  $$0 \;\le\; L_A - L_M \;=\; q\,D_{KL}(\hat Q \| \hat P) \;\le\; q \log_2(1/q)
  \;\xrightarrow[q \to 0]{}\; 0 .$$
  No assumption on module masses is needed. The complementary bound
  $q \log_2(1/\min_i P_i)$ also holds, so the gap is at most
  $q \min\{\log_2(1/q),\, \log_2(1/\min_i P_i)\}$. Both are tight enough to
  matter: over 720 random partitions the largest observed
  $\max_i Q_i^{in}/P_i$ is exactly $1$, and the largest observed
  $\text{gap} / (q \log_2(1/q))$ is $0.96$.

  So the objectives agree increasingly well as the partition improves by
  either one's standards, and they can only disagree materially when the
  boundary flow is substantial. (Note what this does *not* say: a large
  gap requires large $q$, but large $q$ does not by itself make a partition
  bad — the full codelength has other terms.)
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
moving sweep concurrently, with each node assigned to exactly one thread per
sweep. To commit a move $v: a \to b$, acquire the two module locks in id
order, re-read $v$'s current module and its link sums to $a$ and $b$,
recheck the delta, apply, release — abandoning the move if $v$ is no longer
in $a$. Then every committed move improves $L_A$ by more than the threshold
$\varepsilon$, at any thread count, so the parallel search is monotone and
terminates.

*Proof sketch.* While holding both locks, membership of $a$ and $b$ cannot
change (any move with an endpoint in $\{a,b\}$ needs one of the held locks),
so the re-read of $v$'s module and of $v$'s link sums to $a$ and $b$ is
stable for the duration. A concurrent move $u: c \to d$ with
$c, d \notin \{a, b\}$ changes neither the aggregates of $a, b$ nor the
classification of any of $v$'s neighbours as "in $a$", "in $b$", or
"elsewhere" — a neighbour flipping between two "elsewhere" modules leaves
the delta unchanged. Hence the recheck under the two locks is exact, the
committed schedule is equivalent to some sequential order of strictly
improving moves, and $L_A$ is bounded below. $\blacksquare$

The membership re-read is not redundant bookkeeping: without the
one-thread-per-node assignment (or an equivalent claim on $v$), a thread
could hold the $\{a,b\}$ locks while $v$ has already been moved to some
third module, and "recheck the delta" would be evaluating a move that no
longer starts where it thinks. The benchmark implements both guards.

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
   `enterFlow` read), and the existing fast-accept condition — neither
   endpoint module touched yet in this commit pass — becomes sufficient for
   *exactness*, not just for cheapness: with no global term there is nothing
   left that a third module's commit could have staled. Fully deterministic
   given the seed, independent of thread count.
2. **Colored parallel commit.** Since a commit touches two modules only,
   accepted proposals form a conflict graph over module pairs; color it and
   apply colors in parallel batches. Deterministic *and* parallel commits,
   at the cost of a coloring pass.
3. **Fused two-lock sweep (maximum throughput).** Each thread proposes and
   commits immediately under ordered per-module spinlocks with an exact
   recheck (Theorem 3). No barriers, no proposal staleness across a commit
   pass, monotone at any thread count. The committed *schedule* depends on
   thread interleaving, so runs are not bit-reproducible across thread
   counts — the same trade the current inner parallelization already makes
   for proposals, but with the guarantee that every commit improves the true
   objective. Note what this does *not* fix: contention is per module pair,
   so a network with a few very heavy modules (early sweeps from singletons
   are the opposite — many tiny modules) will serialize on those locks. The
   design trades a guaranteed serial fraction for a data-dependent one.

**Where the objectives actually part company.** Design 2 is *not* the
dividing line: the map equation can be batched exactly too. Over a set of
moves with pairwise-disjoint module pairs, its exact change is the
per-module part plus a single global correction,
$$\Delta L_M = \sum_{j} \Delta g_M(\text{pair } j) \; + \;
\operatorname{plogp}\Big(q + \sum_j \delta q_j\Big) - \operatorname{plogp}(q),$$
i.e. one reduction and one $\operatorname{plogp}$ per batch, no
approximation (verified numerically to $2.5 \times 10^{-15}$ over 60 random
batches). Closed-form bulk operations of this kind are what InfoFlow already
does for the map equation. What the map equation cannot have is **design 3's
property**: an individual move's delta that is exact *at commit time* from
locally held state, with no global read-modify-write and no batch barrier.
There, $q$ is a genuine serialization point.

Design 2 also has a second life as **vectorization**: a batch of accepted
moves with pairwise-disjoint module pairs is exactly a "color", and applying
it is a handful of numpy scatter-assignments — the after-aggregates of the
touched modules are already known from the proposal step, so commit is
`P[old] = old_p; P[new] = new_p; ...`. `prototype/atlas_numpy.py` implements
the full search this way (sparse-matmul proposals over dirty nodes,
greedy-maximal disjoint batches). Its `demo_map_batch_drift` measures what
happens when *per-move* deltas priced against the pre-batch $q$ are summed
over a batch: exact for the atlas equation, off by $\sim 10^{-3}$ bits for
the map equation. Read that as the cost of per-move attribution under a
global term — not as a claim that exact batch bookkeeping is unavailable to
the map equation, which the formula above provides.

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
apply verbatim, since the atlas equation consumes exactly the same
per-module enter/exit aggregates the map equation does. The prototype's
directed experiments confirm the algebra (deltas exact, KL identity exact)
on a *simplified* directed flow model — raw PageRank node flow with
link-following link flows — which matches neither of Infomap's teleportation
models exactly: the default unrecorded model re-derives node flow as the
in-aggregate of one teleportation-free link step
(`src/utils/FlowCalculator.cpp:508-525`), and the recorded model books
teleportation mass separately into enter/exit flow. That difference is
orthogonal to the objective (both objectives read whatever flows the flow
calculator produces), but it does mean the directed results here are not
binary-validated — only the undirected examples in §7.1 are. Validity of the
static addresses only needs $\sum_i P_i = 1$.

## 7. Empirical validation

All numbers below are reproducible with the two programs in this directory
(pure-stdlib Python prototype; standalone C++17/OpenMP benchmark). Bits
throughout; `plogp` uses log2 as in `src/utils/infomath.h`.

### 7.1 Correctness of the math (`prototype/atlas_prototype.py`)

The residuals quoted below are *measured* values; the assertions that gate the
script's exit code use a looser $10^{-10}$ so it stays stable across platforms
and BLAS-free stdlib arithmetic. Reference codelengths in the delta checks are
rebuilt from the network by the `Partition` constructor rather than read off the
incrementally-updated object, so a wrong incremental formula cannot cancel out
of both sides of the comparison (verified: a 37% error injected into
`_module_terms_after` fails 6 checks).

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
draws per node land closer to 4), Louvain from singletons, 4 physical cores
(no SMT). Every level converges only when a full verification sweep over all
nodes accepts nothing (§4), so all modes end at single-move local optima.
"L0 time" is the wall time of the top-level (leaf) local-moving phase — the
phase that dominates large runs — including the verification sweeps;
consistency is $|L_{after} - (L_{before} + \sum \delta_{committed})|$ at the
top level.

Two fairness notes on the `par-map` baseline, so its serial floor is not an
artifact of a weak implementation. It carries Infomap's fast-accept path:
proposals whose two endpoint modules are both still untouched in the commit
pass are taken in $O(1)$ without re-walking the node's edges (43% of commits
here), and because the map equation's delta splits into a per-module part and
one $\operatorname{plogp}(q)$ term, that fast path is implemented *exactly*
rather than on a stale snapshot — strictly stronger than Infomap's. It also
takes no per-sweep snapshot copies: the proposal pass precedes all commits, so
it reads live state directly, as Infomap does.

**Read the thread columns as end-to-end phase times, not as parallel
speedup.** Two things make them unsuitable as speedup measurements. First,
concurrency changes the search trajectory: at higher thread counts proposals
see fresher state and the level converges in a different number of sweeps
(par-atlas: 32 sweeps at 1 thread, 27 at 4). Second, this machine is noisy —
repeating a *bit-identical deterministic* run (par-atlas at 1 thread, same
seed, same 32 sweeps, same final codelength) gave 6.63 s and 5.37 s on two
consecutive runs, a 24% spread. Ratios computed from these columns can and do
exceed the core count, which is by itself proof that they are not speedups.
§7.4 measures scaling properly instead.

| mode | threads | L0 time | L0 commit | fast | sweeps | final $L_M$ | modules | consistency |
|:--|--:|--:|--:|--:|--:|--:|--:|--:|
| seq-map                  | 1 | 4.98 s | — | — | 28 | 14.746 | 279 | $4 \times 10^{-12}$ |
| seq-atlas                | 1 | 6.19 s | — | — | 32 | 14.743 | 250 | $3 \times 10^{-13}$ |
| par-map (serial commit)  | 1 | 10.54 s | 0.36 s | 43% | 53 | 15.317 | 268 | $6 \times 10^{-12}$ |
| par-map (serial commit)  | 2 | 4.60 s | 0.34 s | 43% | 53 | 15.317 | 268 | $6 \times 10^{-12}$ |
| par-map (serial commit)  | 4 | 2.37 s | 0.35 s | 43% | 53 | 15.317 | 268 | $6 \times 10^{-12}$ |
| par-atlas (two-lock)     | 1 | 6.63 s | — | — | 32 | 14.743 | 250 | $3 \times 10^{-13}$ |
| par-atlas (two-lock)     | 2 | 2.36 s | — | — | 26 | 14.746 | 258 | $8 \times 10^{-13}$ |
| par-atlas (two-lock)     | 4 | 0.95 s | — | — | 27 | 14.742 | 251 | $8 \times 10^{-13}$ |
| par-map-twolock (unsound)| 1 | 5.36 s | — | — | 28 | 14.748 | 275 | $4 \times 10^{-12}$ |
| par-map-twolock (unsound)| 2 | 2.99 s | — | — | 32 | 14.746 | 273 | $5 \times 10^{-9}$ |
| par-map-twolock (unsound)| 4 | 0.99 s | — | — | 27 | 14.750 | 277 | $1 \times 10^{-8}$ |

Readings:

- **par-atlas**: consistency stays at the sequential floating-point level at
  *every* thread count, and final quality matches seq-atlas. The 1-thread run
  reproduces seq-atlas exactly — the locking discipline is free when
  uncontended. It converges in 32 sweeps versus par-map's 53: the fused sweep
  has no snapshot to go stale, so a proposal accepted late in a sweep already
  accounts for everything committed earlier in it.
- **par-map (serial commit)**: the commit pass does not shrink with threads —
  0.36 / 0.34 / 0.35 s at 1 / 2 / 4 — and snapshot staleness costs it 53
  sweeps to par-atlas's 32. (The final-quality gap of this mode is an
  artifact of this benchmark's plain Louvain pipeline lacking Infomap's tuning
  iterations, not a claim about Infomap.)
- **par-map-twolock**: the negative control. It runs at par-atlas speed but is
  unsound — the committed deltas drift from the true codelength change by
  $10^{-8}$, four orders of magnitude above everyone else's floating-point
  floor, and growing with contention. This isolates the cause: the same commit
  machinery is exact for the atlas equation and inexact for the map equation,
  so the obstacle is the objective's global term, nothing else. (RelaxMap's
  authors report the same outcome from the same experiment — see §9.)

### 7.4 Controlled strong scaling, and what it does *not* show

To separate sweep throughput from trajectory, `--sweep-scaling` advances to a
fixed state (2 sequential sweeps from singletons), snapshots it, then runs one
sweep from that identical state at each thread count, keeping the best of 15
repetitions. Work is then identical across rows. Two invocations:

| mode | threads | sweep | vs 1 thread | serial part | share |
|:--|--:|--:|--:|--:|--:|
| par-atlas   | 1 | 0.187 / 0.184 s | 1.00× | — | — |
| par-atlas   | 2 | 0.112 / 0.092 s | 1.67× / 2.00× | — | — |
| par-atlas   | 4 | 0.051 / 0.042 s | 3.70× / 4.42× | — | — |
| par-map     | 1 | 0.164 / 0.198 s | 1.00× | 0.0027 / 0.0024 s | 1.6% / 1.2% |
| par-map     | 2 | 0.082 / 0.095 s | 1.99× / 2.09× | 0.0025 / 0.0027 s | 3.0% / 2.8% |
| par-map     | 4 | 0.039 / 0.047 s | 4.16× / 4.23× | 0.0023 / 0.0025 s | 5.8% / 5.4% |

**This is the honest result, and it is narrower than a throughput claim.** On
4 cores the two designs' per-sweep scaling is indistinguishable — both land
between 3.7× and 4.4×, inside the measurement spread. The serial commit is
only about 1.4% of a 1-thread sweep here, so by Amdahl it caps speedup at
roughly $70\times$ asymptotically: ~13× of a possible 16 at 16 threads, ~34×
of 64 at 64 threads. Real but distant. **A 4-core machine cannot demonstrate
the throughput half of this note's motivation**, and the earlier framing of
the serial commit as a near-term bottleneck overstated it.

What the benchmark does establish, and what does not depend on core count:

1. **Exactness under concurrent commits.** par-atlas keeps
   $|L - \sum \delta|$ at the sequential floating-point level at every thread
   count; the identical scheme under the map equation drifts by $10^{-8}$.
   Every accepted move provably improves the objective (Theorem 3) — the
   search cannot silently optimize something other than what it reports.
2. **Fewer sweeps to converge**: 32 versus 53, because there is no
   sweep-start snapshot to go stale.
3. **No barrier and no shared scalar**, so the design's scaling ceiling is
   set by lock contention on individual module pairs rather than by a fixed
   serial section — which is the property that would matter at high core
   counts, on hardware not available here.

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

**The map equation itself.** Rosvall & Bergstrom, *PNAS* 105(4):1118–1123
(2008). The atlas equation changes only the index codebook, from adaptive to
static. Its predecessor is worth noting too: the two-part MDL formulation in
Rosvall & Bergstrom, *PNAS* 104(18):7327–7331 (2007) *does* pay for the
module assignment explicitly, so charging for the partition has precedent
inside this lineage.

**Parallel and distributed map-equation search.** This is the literature the
note's payoff claim lands in, and it is substantial:

- **RelaxMap** — Bae, Halperin, West, Rosvall, Howe, "Scalable and Efficient
  Flow-Based Community Detection for Large-Scale Graph Analysis," *ACM TKDD*
  11(3):32 (2017); originally "Scalable Flow-Based Community Detection for
  Large-Scale Network Analysis," *ICDMW* 2013, pp. 303–310. Its abstract's
  "relaxes concurrency assumptions to avoid lock overhead" is easy to
  misread: RelaxMap keeps the codelength and the per-module statistics
  **exactly consistent** under a *global* lock around the update step. What
  it relaxes is the strict sequential order — proposals are evaluated
  concurrently against possibly stale module state, justified by a rarity
  argument (collision probability $\approx p/N$ per pass). Most relevant
  here: the authors report **testing the lock-free variant and finding it
  unworkable** — convergence slower than the sequential algorithm in some
  cases, plus incorrect active-module counts causing race conditions and
  memory faults. That is the same experiment as this note's
  `par-map-twolock` negative control, and it points the same way. The atlas
  equation's contribution is complementary: rather than relax consistency or
  pay for a lock that serializes, remove the shared quantity from the
  objective so per-module locks suffice.
- **GossipMap** — Bae & Howe, *SC '15*, Article 27: distributed-memory
  extension of RelaxMap on PowerGraph, billion-edge graphs.
- **Distributed Infomap** — Zeng & Yu, *ICPP '18*, Article 4: synchronized
  exchange of community state, explicitly targeting RelaxMap's shared-memory
  limits.
- **InfoFlow** — Fung, *Big Data Cogn. Comput.* 3(3):42 (2019): Spark
  implementation whose closed-form multi-module merge formulas let it commit
  a *bulk* merge per iteration, giving logarithmic rather than linear
  iteration count. This is direct evidence that batching is available to the
  map equation (§4) — with the nuance that InfoFlow's closed form is exact
  for the batch it commits while the *selection* is greedy pairwise.
- **HyPC-Map** — Faysal, Arifuzzaman, Chan, Bremer, Popovici, Shalf, *IEEE
  HPEC* 2021: hybrid MPI+OpenMP with cache-optimized structures, reporting
  ~25× over prior map-equation implementations at 41M-node scale. Also
  Faysal & Arifuzzaman, *IEEE BigData* 2019, pp. 4773–4782, and Faysal,
  Bremer, Arifuzzaman, Popovici, Shalf, Chan, *IPDPSW* 2023, pp. 601–610
  (sparse-accumulation acceleration).
- **Distributed map equation and modularity** — Hamann, Strasser, Wagner,
  Zeitz, *Euro-Par 2018*, LNCS 11014.

Read together, this body of work attacks the same bottleneck from the
algorithm side: locking, relaxation, synchronization schedules, and bulk
operations. None of it changes the objective, which is what this note
proposes.

**Separable objectives.** Markov stability (Delvenne, Yaliraki, Barahona
2010) and the Constant Potts Model (Traag, Van Dooren, Nesterov, *Phys. Rev.
E* 84:016114, 2011 — the CPM *objective*; the 2019 *Sci. Rep.* Leiden paper
is the *algorithm*) are additively separable, which is exactly why
Louvain-family parallelizations thrive on them — but they are not
description-length-based. The atlas equation is an MDL-semantics member of
that family.

**Map-equation variants.** Smart teleportation (Lambiotte & Rosvall 2012)
changes the *process* to tame directed flows; the atlas equation changes the
*code*, and its static addresses are the coding counterpart of stationary
relocation at borders (§2). The regularized/Bayesian map equation
(Smiljanić, Blöcker, Edler, Rosvall, *J. Complex Netw.* 9(6):cnab044, 2021)
and the in-repo lossy map equation (rate–distortion) are orthogonal
variations that keep the adaptive index codebook; in principle both compose
with static addresses. For the broader landscape see the map-equation survey
by Smiljanić, Blöcker, Holmgren, Edler, Neuman & Rosvall (*ACM Computing
Surveys*, online-first; arXiv:2311.04036).

**Mismatched map-equation codebooks.** One precedent is directly on point:
*flow divergence* (Blöcker & Scholtes, arXiv:2401.09052) scores a network's
flow with **another partition's** codebooks and frames the excess bits as a
relative entropy. That is the same move the atlas equation makes — price
events with a code built for a different distribution and read the penalty as
a KL term — applied there to comparing partitions rather than to defining an
objective.

**Novelty caveat.** What appears not to have been proposed is the specific
combination: pricing module *entries* by the module-mass distribution
$-\log_2 P_i$, chosen so that the objective becomes additively separable,
together with the identity $L_A = L_M + q\,D_{KL}(\hat Q \| \hat P)$. Both
ingredients have precedent, though — the mismatched-codebook relative entropy
in flow divergence, and explicit partition-cost terms in the 2007 two-part
MDL and in the SBM/MDL line (§10). Any publication claim needs a careful
check against those; the citations here were assembled from search metadata
rather than from reading every paper end to end.

## 10. Open questions

1. **Systematic bias characterization.** The KL term penalizes
   entry-rate/size disproportion. On which real network families does that
   change partitions materially (e.g. hub-and-spoke, strongly heterogeneous
   module sizes, flow bottlenecks), and is the change ever preferable?
2. **Hierarchical formalization** (§5): exit-word treatment for nested
   charts; whether the telescoping property yields a cheaper multilevel
   search than recursive two-level.
3. **Alternative static references.** Addresses from teleportation weights
   would connect to smart teleportation; same separability, different bias.
   Note that the obvious other candidate — addresses from node counts,
   $-\log_2(n_i/n)$ — is *not* unexplored: summed over nodes it is
   $\sum_i n_i \log_2(n/n_i)$, which is (via Stirling) exactly the
   group-label term $-\log P(b \mid n) = \log(N! / \prod_r n_r!)$ of the
   nonparametric SBM description length (Peixoto, *Phys. Rev. X* 4:011047,
   2014; *Phys. Rev. E* 95:012317, 2017). A node-count variant of the atlas
   equation would therefore be closer to inferential SBM/MDL community
   detection than to the map equation — interesting, but it should be
   developed against that literature, and it omits the SBM's further terms
   for $\{n_r\}$ and the number of groups.
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

# Parallel benchmark, end-to-end phase times (standalone, needs OpenMP)
make -C research/atlas-equation/bench run

# Controlled strong scaling from an identical state (§7.4)
research/atlas-equation/bench/parallel_bench --blocks 200 --block-size 1000 \
    --threads 1,2,4 --sweep-scaling --reps 15
```

The prototype validates its map-equation baseline against the built
`./Infomap` binary when present; pass `--require-binary` to make a missing
binary an error rather than a skip.
