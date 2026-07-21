# Columnar search rethink — experiment log

Working notes + scratchpad for making the columnar hierarchical search faster/more
flexible. Tracked WIP doc under `columnar_wip/` on branch `columnar-hierarchical-core`,
baseline commit `d5b17d12`. Experiments run in isolated worktrees under `scratchpad/`;
the shipped core changes are committed on the branch.

Convention: **always report codelength (bits) AND time**. Time that matters = the
`Done trial` optimize timer (build+optimize+materialize) and end-to-end `real`.
Primary signal network: `web-NotreDame.net -d` (325k nodes, directed — where time hurts).
Generality checks: `science2001.net -d`, `netscicoauthor2010.net` (undir), `politicalblogs.net`.

Single-thread (`MODE=release OPENMP=0`), `--seed 123 -N1`.

---

## TL;DR (what to do)

**The one shipped win: raise the refine knee 1e-3 → 5e-3** (committed `36efd697`). webND -d ~18% faster
search for +0.06% codelength (seed-stable); every other benchmarked net bit-identical (they run one
sweep). Simple, robust, and it turned out to make the fancier ideas redundant.

Everything else was tested and NOT shipped, with evidence:
- **gpcache** (grandparent-incremental k=0 cache): bit-exact and −15% *at knee 1e-3*, but a SUBSTITUTE
  for the knee — at the shipped 5e-3 it's neutral on webND and −20% *slower* on shallow/memory nets
  (overhead with too few re-runs to repay). Dropped (F13). Would only pay off at a tight knee / full
  convergence.
- **organic build + bubble (A/B)**: valid schedule (matches converge single-trial with a from-singletons
  tune, F10), but on the fair best-of-N metric it's 1.7–2.5× slower and no better (F11); limiting the
  bubble depth (leaf-once etc.) helps but stays dominated (F12).
- **flex / `-F`**: webND-specific fast dial — catastrophic on memory (air30k +14/15%), neutral/slower on
  shallow nets. Keep as a dial, not a default.
- Ruled out earlier: seeded re-tune / C1 (basin-trapped, F7); B1 incremental gate (negligible when
  gated); D1 queue-moving (moveLoop already has a dirty-set).

**Overarching lesson (Daniel's methodology): compare fairly — best-of-N + total time — against the
CURRENT default.** Several ideas looked good single-trial or at a stale knee and lost when measured
properly. Follow-up worth a look: **on memory nets the interior refine can HURT** (coarsening
interaction, F3). See findings F1–F13 below.

---

## Baselines (commit d5b17d12, this machine)

| network | engine | codelength | lvl | trial s | e2e s |
|---|---|--:|--:|--:|--:|
| web-NotreDame -d | OO | 5.566937317 | 8..12 | — | ~15.9 |
| web-NotreDame -d | columnar converge (`-C`) | 5.569547986 | 6 | ~2.6 | ~3.9 |
| web-NotreDame -d | columnar fast (`-C -F`) | 5.626580431 | 5 | ~1.8 | ~3.1 |

(other networks filled in below as measured)

### Anatomy of the default converge search (measured earlier, native path)
- read+flow(power-iter)+write ≈ 1.3s (shared infra)
- native SoA build (`buildColumnarLeafInput`) 0.015s
- ingest 0.014s
- bottom two-level 0.57s
- screen (2× up-build) 0.21s
- **refineHierarchy (interior refinement) ≈ 1.45s = DOMINANT** (coarsen ≈0 for base)
- materialize 0.31–0.47s

Per gated refine step: full `m_hierLevels`+`m_hierAssign` COPY (save), per-grandparent
sub-network CSR rebuilt from scratch, re-cluster from SINGLETONS, full O(n_leaves)
`hierarchicalCodelengthFromStack` recompute, copy-back on revert.

Known duplication: `refineBottomWithinParents` (flex) ≈ `refineLayerWithinGrandparent(0)`
(converge) — same op, differ only in acceptance policy.

---

## Experiment plan
- **A** understand: (A1) split refineHierarchy time; (A2) ablate converge's +1% quality
  (screen count / k>0 layers / gating).
- **B** bit-exact efficiency: (B1) incremental gate — save/recompute only touched levels;
  (B2) cache sub-network topology across passes.
- **C** cheaper refinement: (C1) seed grandparent re-cluster from current partition + fine-tune
  (vs singletons); (C2) best-of-both — flex bottom + interior refine of higher layers only.
- **D** primitive rethink: (D1) queue-based local moving (Leiden-style); (D2) interleave up-build+refine.
- **E** unify into one configurable refine (flex/converge as presets).
- **N** (user idea) grow organically: refine after each new level, bubble up/down, not always to
  the bottom, adapt to max benefit.

Lab strategy: ONE instrumented "lab" build with env-gated knobs (timers, screen count, max
refined layer, gate on/off, seed policy, incremental gate) so the A/B/C matrix runs without
per-variant rebuilds.

---

## Findings log
(newest insights appended here)

### F1 — Baselines across networks + KEY DIRECTIONAL INSIGHT (commit d5b17d12)

| network | OO cl | converge cl (trial s) | fast cl (trial s) | fast vs converge |
|---|--:|--:|--:|--|
| web-NotreDame -d | 5.566937 | 5.569548 (2.6) | 5.626580 (1.8) | +1.0% cl, **~30% faster** |
| science2001 -d | — | 7.833437 (0.38) | 7.833437 (0.55) | identical cl, **slower** |
| netscicoauthor2010 | — | 4.064763 (.003) | 4.074342 (.002) | +0.23% cl |
| politicalblogs -d | — | 6.758422 (.008) | 6.758422 (.012) | identical cl, slower |
| air30k (states) | 5.394367 (1.6) | 5.480216 (0.58) | **6.306279** (0.54) | **+15.1% cl**, no time win |

**INSIGHT (redirects the whole effort):** the fast/flex path is *only* a win on
web-NotreDame. Everywhere else it is neutral-to-catastrophic — on the air30k state
network it is **+15%** codelength for zero time saving, and on shallow nets (sci2001,
polblogs) it is actually *slower* (flex still does ~2 full bottom re-clusters + per-
grandparent sub-networks, while converge's screened+incremental refine is cheap when the
hierarchy is shallow). So the interior-layer refinement that flex skips is **essential**,
not optional. => The rethink should **preserve converge's quality and cut its cost**
(directions B: bit-exact efficiency, C1: cheaper same-quality refine), NOT generalize flex.
`-F` remains a valid webND-specific fast dial, but is not the template for the new default.

### F2 — A-phase profile + ablation (webND -d, lab build, LAB_* env knobs)

| variant | codelength | lvl | refine s | K0 / Khi | gate calls/acc |
|---|--:|--:|--:|--:|--:|
| default (screen2, allK, gated) | 5.569548 | 6 | 1.504 | 1.222 / 0.273 | 16 / 10 |
| screen1 (1 up-build) | 5.591320 | 5 | 1.653 | — | 13 / 8 |
| maxK=0 (bottom refine only) | 5.634143 | 6 | 0.415 | 0.411 / 0 | 3 / 1 |
| maxK=-1 (NO interior refine) | 5.726246 | 6 | 0.004 | 0 / 0 | 2 / 0 |
| ungated (accept all) | 5.569609 | 6 | **5.719** | 1.193 / 0.260 | **2012 / 1012** |
| screen1 + maxK=0 | 5.626580 | 5 | 0.451 | — | 3 / 1 |

**Quality decomposition (webND -d):** build-only floor **5.7262** → +bottom(k=0) **5.6341** →
+higher-layers(k>0) **5.5695**. So:
- **The higher-layer refinement (k>0) is the single best deal: 5.6341→5.5695 (−1.1%) for only ~0.27s.**
  Flex throws exactly this away → that's why flex is bad on air30k/general.
- Bottom refine (k=0): 5.726→5.634 (−1.6%) but costs the most (K0=1.22s) because the dirty-set
  **re-runs k=0 ~3×** across sweeps (each k=0 pass ~0.41s; k=1 acceptance re-dirties k=0).
- Screening the 2nd up-build strategy: 5.5913→5.5695 (−0.39%), cheap (~0.15s). Worth keeping.

**Gating (webND -d):** gated vs ungated give ~identical codelength (5.569548 vs 5.569609) but
gated is **3.8× faster** (16 vs 2012 refine calls). The gate's value is *convergence/early-stop*,
NOT quality. copy+recompute are negligible when gated (0.05s over 16 calls) → **B1 (incremental
gate) is NOT worth it for the default** (only mattered in the degenerate ungated case). Scratch B1.

**flex == screen1 + maxK=0, bit-exact (5.626580, 5 lvl).** Confirms flex is just a preset of the
general refine → validates unification (E).

**=> Revised priorities.** The dominant cost is the **k=0 bottom re-partition re-run ~3× from
singletons** (1.22s). Attack it with **C1: seed each grandparent re-cluster from the CURRENT
partition + fine-tune** instead of from singletons (the partition barely changes between sweeps once
good). Keep k>0 (cheap, high-value) and screening. Also test **sweep cap `-T`** (does k=0 really need
3 passes?).

### F3 — sweep-cap dial (webND -d) + air30k interior-refine anomaly

**Sweep cap `-T` (webND -d):** T1 `5.593203` (refine 0.49s) → T2 `5.572794` (0.95s) → T0/converge
`5.569548` (1.45s). Extra sweeps pay with clear diminishing returns; T2 is within 0.06% at ~34%
less refine time, T1 within 0.42% at ~66% less. A legitimate speed/quality dial already exposed via
`-T`. (Each sweep ≈ one more k=0 re-run — confirms the re-run cost.)

**air30k (memory/state) — interior refinement HURTS.** internal==materialized both ways (no metric
bug). build+coarsening only (`LAB_MAXK=-1`, interior skipped) = **5.477628** (0.50s) is BETTER and
faster than default (interior+coarsening) = **5.480216** (0.58s). Mechanism: `maxK` caps only the
interior loop; the **coarsening loop still runs** (active for memory, no-op for base). The gated
interior refine makes locally-improving moves that push the partition into a basin where the
subsequent coarsening lands *worse*. Greedy-local, worse-global-after-next-stage. Also explains why
flex's *ungated* `refineBottomWithinParents` is catastrophic on air30k (6.306): ungated accepts
base-proxy gains that wreck the memory objective. **Lead (verify on more state nets/seeds): for
memory, skip interior refine or run coarsening last/again — potential free quality+speed win.** Not
the base-search focus; recorded for follow-up.

### F4 — C1 (seed re-runs from current) + D1 already done + the principled fix

**D1 (queue-based local moving) is ALREADY implemented:** `moveLoop` uses a per-unit dirty-set
(`if (!dirty[u]) continue;`) and only re-examines dirty units. The move primitive is already
incremental → D1 deprioritized.

**C1 (seed k=0 re-runs from current partition + fine-tune) — modest trade, NOT free (webND -d):**
refine 1.438→0.948s (−34%) but codelength 5.569548→**5.584307 (+0.27%)**. Seeded fine-tune only does
local moves, so on a grandparent whose membership changed (from the k=1 refine) it can't *repartition*
the way from-singletons can → weaker per-step gains → worse fixpoint (still gated, each step improves,
just less). Only affects DEEP nets (shallow nets run 1 sweep, no re-runs, C1 no-op). Verdict: a
webND-specific speed/quality dial, inferior to ↓.

**THE PRINCIPLED FIX (next): grandparent-level incremental refinement (bit-exact).** Why does k=0
re-run ~3× cost 1.2s? Because each re-run re-clusters *every* grandparent from singletons, but a k+1
refine only changes the membership of a *few* grandparents. Track which grandparents changed and
re-cluster ONLY those (keep from-singletons quality). Same result as full re-runs (bit-exact), skips
the unchanged majority. Expected: most of C1's speed with NONE of its quality loss. This is the real
target.

### F5 — knee/sweep dial (the practical ship-now win) + seed stability

converge does exactly **3** interior sweeps on webND -d (T3==converge). The 3rd sweep costs ~18-20%
of the trial for only +0.06% codelength. Raising the diminishing-returns knee to **5e-3** (via
`--tune-iteration-relative-threshold 5e-3`, or as the columnar default) drops it:

| net | converge | knee 5e-3 | effect |
|---|--:|--:|--|
| webND -d (seed 123) | 5.569548 / 2.54s | 5.572794 / 2.08s | +0.06% cl, **−18% trial** |
| webND -d (seed 1) | 5.568038 / 2.57s | 5.573451 / 2.06s | +0.10% / −20% |
| webND -d (seed 42) | 5.573738 / 2.52s | 5.576462 / 2.07s | +0.05% / −18% |
| science2001 -d | 7.833437 | 7.833437 | **identical** |
| netscicoauthor2010 | 4.064763 | 4.064763 | **identical** |
| politicalblogs -d | 6.758422 | 6.758422 | **identical** |
| air30k (states) | 5.480216 | 5.480216 | **identical** |

Seed-stable ~+0.06% / −18% on webND; **bit-identical on every other net** (they do 1 sweep, knee
never triggers). With `-N` the per-trial saving buys more trials → likely net-positive quality.

---

### F6 — rec #2 IMPLEMENTED + VERIFIED BIT-EXACT (grandparent-incremental k=0 cache)

Implemented in the lab worktree (`LAB_GPCACHE=1`): content-keyed cache for the k=0 refine, keyed on
(grandparent leaf-set S, exit). On a cache hit skip the sub-network build + two-level and reuse the
stored sub-partition. Bit-exact because level-0 leaves are immutable and S is emitted in stable
(ascending-id) order.

**Bit-exact verified** — base == gpcache, every seed and net:
- webND -d: seed 1 `5.568037711`, seed 42 `5.573738129`, seed 7 `5.575470062`, seed 123 `5.569547986` — all EXACT.
- sci2001 -d `7.833436601`, netsci `4.064762711`, air30k `5.480216499` — all EXACT.

**Speed (webND -d, seed 123):** refine 1.41→1.10s (K0 1.15→0.83s, **−28%**), trial 2.49→2.22s (**−11%**),
zero quality change. Cache hit-rate ~54% on re-runs (refine(1) churns ~46% of grandparents each sweep,
which caps the win). Inert on shallow nets (single sweep → all misses, still bit-exact).

**gpcache + knee5e-3 (webND -d):** trial 2.49→**1.94s (−22%)**, cl 5.572794 (+0.06%). Complementary:
knee drops the low-value 3rd sweep, gpcache accelerates the surviving re-runs.

Caveat found: `lab.sh` hardcoded `--seed 123` (overrode passed seeds) — fixed by running the binary
directly for the seed sweep. No code bug.

---

### F7 — OO-style fine+coarse seeded re-tune (Daniel's idea) + the stale-seed insight

Implemented the OO fine/coarse tune from the current partition (seed pass-1 of optimizeTwoLevel =
fine-tune, then aggregation = coarse-tune; `LAB_SEED=2`). Also confirmed the move loop ALREADY lets a
node split into a new module, so C1's weakness was the missing coarse (whole-submodule) moves.

webND -d re-run strategy (all knee 1e-3, ~3 sweeps): SEED0 from-singletons **5.569548** / refine 1.45s;
SEED1 fine-only (C1) **5.584307** (+0.27%) / 0.93s; **SEED2 fine+coarse 5.576778 (+0.13%) / 0.97s.**
Coarse-tune halves C1's loss — Daniel's intuition confirmed — but still doesn't match from-singletons.

**Why (key insight):** the grandparents that a k+1 refine dirties are the ones whose leaf-set CHANGED
(~46% each sweep). For those, the "current sub-partition" seed is STALE (it's the partition of a
different composition) → seeding converges to a nearby-but-worse optimum. Seeding is only accurate
where the grandparent is UNCHANGED — and those are exactly the ones gpcache already reuses bit-exactly.
So on the current build-then-refine schedule, **gpcache (reuse unchanged bit-exact + rediscover changed
from-singletons) is strictly better than seeding the changed ones.** Seeding pays off only if
grandparents are tuned while their composition is fresh — which is precisely what Daniel's
"grow-organically" idea (build one level, tune, bubble) would arrange. => the two ideas are linked:
**organic build makes seeded re-tune viable; on the current schedule, use gpcache.**

### F8 — Option A (organic up-build, tune each layer once while fresh)

`optimizeOrganic` (`LAB_ORGANIC=1`): interleave a seeded fine+coarse gated tune of layer (top-2) each
time a new top level forms. Grandparent invariant makes mid-build tuning safe.

| net | converge | ORGANIC A | 
|---|--:|--:|
| webND -d | 5.569548 / 2.9s | **5.652527 (+1.5%) / 1.5s** (5 lvl) |
| science2001 -d | 7.833437 | 7.833731 (+0.004%) |
| air30k | 5.480216 | **6.271833 (+14%)** |

**Fast (~half the time) but quality-poor** — flex-like on air30k. Tuning each layer ONCE going up
never re-tunes the bottom (k=0) as the upper structure settles; converge's quality comes from exactly
those re-sweeps. Confirms Daniel's instinct that the **DOWN half of "bubble up and down" is essential**
— option B. (Also: organic skips screening, worth ~0.4%.)

### F9 — Option B (bubble) + THE DECISIVE CONCLUSION on seeded tuning

`LAB_ORGANIC=2`: at each build height, converge seeded fine+coarse gated refines over all interior
layers (top-down sweeps, fresh seeds since height is fixed) before growing. Fresh back-to-back (webND -d):
converge 5.569548 / 2.91s; A (tune-once) 5.652527 / 1.48s; **B (bubble) 5.651398 (+1.5%) / 4.16s** —
slower than converge AND no better than A. air30k B = 6.271 (+14%). sci2001 B = 7.833731 (≈converge).

**CONCLUSION (resolves options A/B/C/flex/C1/SEED2 all at once):** every *seeded* re-tune — flex, C1
(fine), SEED2 (fine+coarse), organic A (fresh once), organic B (fresh converged) — lands ~1.5% worse on
webND and ~+14% on air30k, regardless of freshness or how much we bubble. The reason is fundamental:
**seeding starts each re-tune inside the current partition's basin and local moves can't escape it;
the columnar's quality comes from the k=0 from-SINGLETONS re-clustering, which re-discovers leaf groups
globally.** This mirrors OO: there the fine/coarse tune is a *polish on top of* from-scratch recursive
clustering, not a replacement — in the columnar, the from-singletons k=0 refine IS that from-scratch
step, so replacing it with seeding loses the discovery.

=> **Do NOT replace from-singletons with seeded tuning** (seeding is quality-limited). **[SUPERSEDED
by F10:** this does NOT rule out the organic/bubble SCHEDULE — with a from-singletons tune the bubble
works and matches converge on the same basin.**]** Keep from-singletons and make it cheap: **gpcache
(bit-exact)**; the knee (now 5e-3) is the quality dial. Deeper speedup: cut the ~46% grandparent CHURN
so gpcache hits more.

### F10 — CORRECTION of F9: bubble works; the fault was my seeded tune (Daniel was right)

F9 concluded too fast. The "no improvement from bubbling" was a **fixpoint artifact of using a SEEDED
re-tune**: a seeded fine+coarse refine at a fixed height reproduces the current partition → the gate
reverts → the bubble is a no-op by construction. That is an implementation flaw, not a property of
bubbling. Diagnosing (adding `LAB_ORGSEED`, `LAB_ORGANIC=2`) showed (webND -d, fresh):

| variant | codelength | lvl | trial | tune accepts |
|---|--:|--:|--:|--:|
| converge screen2 | 5.569548 | 6 | 2.96 | — |
| converge screen1 (superAgg0) | 5.591320 | 5 | 3.11 | 8 |
| B bubble **seeded** (orgSeed=2) | 5.651398 | 5 | 4.51 | 18 (no real climb) |
| B bubble **from-singletons** (orgSeed=0) | **5.589027** | 5 | 6.72 | 15 |
| A once from-singletons | 5.606236 | 5 | 1.76 | 3 |
| **B from-singletons + gpcache** | **5.589027** | 5 | **3.19** (organic 2.66) | 15 |

Findings: (1) **The bubble DOES climb** with a from-singletons tune (5.606→5.589, 3→15 accepts); it was
inert only because seeded re-tunes are fixpoints. (2) **The organic SCHEDULE is valid** —
organic-bubble-from-singletons (5.589) matches/beats converge's from-singletons refine on the SAME
basin (screen1 5.591). (3) The residual gap to converge-screen2 (5.5695) is just **screening** (the
superAgg-1 basin), which organic doesn't do yet. (4) **gpcache is bit-exact here too** and halves the
organic time (5.64→2.66s). (5) Seeded tuning IS quality-limited (the one true part of F9) — but that
rules out *seeding*, NOT the organic/bubble schedule.

Status: organic-bubble-from-singletons+gpcache (5.589 / 3.19s) is competitive but not yet beating
converge-screen2+gpcache (5.5695 / ~2.6s): it lacks screening and its bubble re-converges the whole
stack at every height (wasteful) vs converge's single dirty-set refine. To potentially WIN it needs
(a) screening and (b) a dirty-set bubble (only re-tune what changed) — both plausible. NOT a dead end.

### F11 — Organic built out (screening + dirty-set bubble) + FAIR -N10 verdict

Built option A/B properly: (1) screening (grow superAgg {0,1}, keep best) and (2) a dirty-set bubble
(only re-tune layers a change touched, not the whole stack each height), with from-singletons tuning +
gpcache. Single-trial webND -d: organic-full = **5.569703 / 6 lvl** (matches converge 5.569548,
internal==materialized) — so screening closed the quality gap and the schedule is fully valid.

Fixed a real bug for the comparison: the k=0 gpcache is keyed on (leaf-set, exit) but the sub two-level
also depends on m_seed → cleared it per trial at the top of optimizeColumnar (was bypassed on the
organic path), so -N is correct.

**Fair -N10 (best-of-10 codelength + total wall time, Daniel's methodology):**

| network | converge+gpcache | organic-full |
|---|--:|--:|
| web-NotreDame -d | **5.567412 / 27.6s** | 5.569674 / 69.6s |
| air30k (memory) | **5.472519 / 7.2s** | 6.229949 (+14%) / 9.6s |
| science2001 -d | **7.833437 / 4.4s** | 7.833437 / 7.7s |

**VERDICT (rigorous, not premature): converge+gpcache beats organic-full on every net** — equal-or-
better best-of-10 AND 1.7–2.5× faster; organic is catastrophic on memory (+14%). Why organic loses: it
does FAR more from-singletons re-clustering (screening × dirty-bubble re-tuning k=0 at *every* height ≈
heights×2 passes) vs converge's single post-build refine (~3 k=0 sweeps); gpcache absorbs the unchanged
share but not the excess. And best-of-N helps converge MORE (its trials are seed-diverse: best-of-10
5.5674 vs single 5.5695; organic's dirty-bubble converges to near the same optimum each seed, best-of-10
5.5697 ≈ single). So interleaving tune-into-build is a valid algorithm but strictly more work here with
no quality upside — build-then-refine is the more efficient schedule.

**Net of the whole A/B/C arc:** the organic/bubble idea was NOT a dead end (F9 was wrong; the bubble
works with a from-singletons tune, F10) — but measured fairly it doesn't beat converge (F11).
[**Note:** this section said "ship gpcache (C)" — SUPERSEDED by F13: gpcache is net-negative at the
shipped knee. The shipped win is the knee alone.]

### F13 — gpcache is a SUBSTITUTE for the knee, and NET-NEGATIVE at the shipped default (dropped)

Productionized gpcache (per-instance cache, always-on k=0 base, cleared per trial) and re-verified it
**bit-exact** on all nets. But measuring at the *shipped* knee (5e-3) instead of the stale 1e-3 exposed
that gpcache no longer helps and mostly hurts (webND -d, -N10, fresh):

| knee | gpcache OFF | gpcache ON |
|---|--:|--:|
| 5e-3 (default), run 1 | 26.0s | 33.2s |
| 5e-3 (default), run 2 | 29.1s | 28.8s |
| 1e-3, -N10 | 37.0s | 30.9s |
| air30k 5e-3, -N10 | **6.7s** | **8.0s** (+20%) |

**gpcache and the knee are substitutes, not complements.** gpcache accelerates the k=0 *re-runs*; the
knee (5e-3) cuts sweeps from 3→2, so there is ~1 re-run left — not enough to repay the cache's build/
hash/store overhead. Net: neutral-to-slightly-negative on webND, consistently **slower on shallow +
memory nets** (1 sweep → cache built but never reused = pure overhead). gpcache only wins at a tight knee
(1e-3, 3 sweeps: −17%) or full convergence. Since the default is 5e-3, **gpcache is not worth shipping**
— reverted. The earlier "gpcache −15% bit-exact win" (F6/F11) was real ONLY at 1e-3, which is no longer
the default. The KNEE captured the win more simply and made gpcache redundant.

Lesson (Daniel's methodology paying off again): measure the candidate against the CURRENT default, not a
stale one. gpcache looked great until benchmarked at the knee we actually shipped.

## RECOMMENDATIONS (priority order)

1. **SHIPPED (committed `36efd697`): raise columnar interior-refine knee default 1e-3 → 5e-3.**
   `ColumnarTwoLevel::m_minRelTuneImprovement`. webND -d: −18-20% trial, +0.05-0.10% cl (seed-stable);
   every other benchmarked net bit-identical. One-line change, contract tests pass. **This is the win.**

2. **gpcache — DROPPED (see F13).** Bit-exact and −15% at knee 1e-3, but a substitute for the knee;
   net-negative at the shipped 5e-3 (neutral on webND, −20% slower on shallow/memory nets). Not shipped.
   Only revisit if a tight-knee / full-convergence fast path is wanted (then it helps). Lab prototype
   under `LAB_GPCACHE` in `scratchpad/wt-exp`.

3. **flex / `-F`: keep as a webND-specific fast dial, not a general default.** Catastrophic on memory
   (air30k 6.306 vs 5.480). ALSO: its `refineBottomWithinParents` is UNGATED (accepts base-proxy gains
   that wreck the memory objective) — worth gating on the true hierarchical codelength so `-F` is at
   least safe (not necessarily good) on state nets.

4. **Memory/meta follow-up (separate from base speed):** gated interior refine + coarsening interact
   badly on air30k — build+coarsen alone (5.4776) beats interior+coarsen (5.4802). Lead: for
   correction-active objectives, run coarsening after interior refine (or gate on post-coarsen
   codelength, or skip interior). Verify on more state nets/seeds.

5. **Architecture (E):** flex == screen1 + maxK=0 (proven bit-exact). Unify optimizeFlexible /
   optimizeColumnar / refineBottomWithinParents / refineLayerWithinGrandparent into ONE configurable
   refine(layers, screenCount, seedPolicy, acceptPolicy, stopPolicy); removes duplication, exposes the
   speed/quality dial continuously (flex, knee-5e-3, converge become presets).

### Rejected / already-done (with evidence)
- **B1 incremental gate** (recompute+copy only touched levels): scratched — gated mode makes only ~16
  refine calls, copy+recompute total ~0.05s. Only mattered in degenerate ungated (2012 calls).
- **C1 seed re-runs from current**: works but +0.27% cl for −34% refine on webND — inferior to the knee
  (which is +0.06% for similar speedup and free). Local fine-tune can't repartition a changed grandparent.
- **D1 queue-based local moving**: `moveLoop` already uses a per-unit dirty-set. Done.
- **D2 interleave build+refine / adaptive scheduler**: reasoned no fundamental gain — every layer refine
  that runs is productive (gate rejects non-improving ones), so re-runs aren't "wasted"; the only waste
  is grandparent-granularity → that's rec #2, not scheduling.

---

### F14 — Review pass over the branch tip (36efd697): every claim below tested

Contract baseline green (`ctest -L columnar`, 4/4 suites). Review worktree with env-gated
prototypes: `scratchpad/wt` (session 44f91a64…): `COL_FLEXCOARSEN`, `COL_FLEXGATE`,
`COL_SEEDINPUT`, `COL_MAXK`.

**1. SHIP-WORTHY: `-F` + the coarsening loop fixes the memory catastrophe outright.**
`optimizeFlexible` never runs `mergeLeafModulesWithinParents`/`refineTopLayer` — but those are
module-level (cheap), and the merge is exactly what closes the mem gap in converge. Appending
converge's coarsening loop to `-F` (`COL_FLEXCOARSEN=1`):

| net | -F shipped | -F + coarsen | reference -C |
|---|--:|--:|--:|
| air30k -N10 best-of | 6.229949 / 12.2s | **5.472519 / 10.9s** | 5.472519 / 17.2s |
| air30k seeds 1/42/123 | 6.20 / 6.31 / 6.31 | 5.4707 / 5.4810 / 5.4802 | 5.4802 (s123) |
| meta-lazega s123 | 6.079528 | **6.060559** (== -C) | 6.060559 |
| webND/sci2001/netsci/polblogs/powergrid | — | bit-identical or better (sci2001 s1 −0.002) | time flat |

`-F` becomes safe on memory/meta nets at zero cost — and on air30k it now EQUALS converge's
best-of-10 at 37% less time. Supersedes rec #3's gating idea, because:

**2. The "ungated refine" fear never fires (rec #3 corrected).** Instrumented `-F`: 0 worsening
refines retained in 171 runs (7 nets × up to 20 seeds); internal == materialized everywhere. The
air30k gap was 100% the missing coarsening stage, not acceptance policy. A revert-gate
(`COL_FLEXGATE=1`) is bit-exact and time-neutral (webND ±3%) — fine as defensive hardening, but
it fixes nothing observable.

**3. rec #4 REFUTED — the F3 "interior refine hurts on memory" was a seed-123 artifact.**
F3's exact numbers reproduce on seed 123 (5.480216 vs 5.477628 ✓), but: air30k 5 seeds → default
(with interior refine) wins 4/5 (e.g. s1 5.4707 vs 5.4942, s999 5.4655 vs 5.4884); 3 air30k
sample nets × 3 seeds → default wins 6/9, and its losses are ≤0.003. Interior refine HELPS
memory nets on average. Drop rec #4.

**4. Soft `--cluster-data` is silently DISCARDED by `-C` — real regression vs OO.** air30k with
the OO tree (5.394367) as seed: OO improves it to 5.3899; `-C` returns 5.4802 — worse than its
own input, no warning. Prototype (`COL_SEEDINPUT=1`): rectangularize the input tree to the MODE
depth (truncate deeper paths = clean coarse-graining; repeat-finest padding only for shallower),
seed via `seedHierarchyFromLeafPaths`, `refineHierarchy`, keep best-of vs the search →
**5.395136 in 1.2s** (vs 5.4802 unseeded). Caveat: deep ragged seeds embed poorly (webND OO tree,
depths 8–12: rectangularized eval 6.38 → refined 5.622, correctly loses to search 5.5728, ~2s
wasted) → production wants a distortion guard (skip when rectangularized eval ≫ the input's OO
codelength) or native ragged-stack support. Related gap: because `seedHierarchyFromLeafPaths`
requires a rectangular tree, `evaluateColumnarPartition` falls back to OO for MOST real trees —
the columnar stack evaluation is under-exercised in the wild.

**5. `-C` silently overrides `--two-level`.** ninetriangles `-C -2`: 3 levels / 3.3858 vs OO
`-2`: 2 levels / 3.5178. Warn, or honor it — the core has `optimizeTwoLevel`, wiring is easy.

**6. Knee/-T wiring edges (webND -d, s123).** Explicit `--tune-iteration-relative-threshold
1e-5` is silently IGNORED (wiring compares against the OO default instead of checking the flag
was passed — use `Config::parsedOptions`): 1e-5 → 5.572794/2.13s (== default), 1e-3 →
5.569548/2.71s, 0 → 5.569548/2.69s. And `-F` ignores `-T` entirely (`-F -T1` == `-F`).

**7. `--entropy-corrected` reporting mismatch — PRE-EXISTING OO bug, inherited.** jazz `-C`:
internal 6.918423 vs materialized 6.935382; delta == (n−k−1)/(2D) exactly (n=198, k=11, D=5484).
BOTH engines' `--no-infomap` eval of the same tree = 6.918423 (columnar agrees with canonical).
Master reproduces the same search-vs-eval gap OO-only (ninetriangles 3.826931 vs 3.794880). File
upstream; not branch-introduced.

**8. Composed objectives report the wrong number.** meta+bias `-C`: the search optimizes
6.084055 (base+meta+bias) but the reported Best codelength is 6.060559 — the OO tree eval is
meta-only (single-inheritance can't compose). Report `columnarL` when >1 correction is active.

**9. Otherwise clean.** Parity (internal==materialized) at `-C` AND `-C -F`: states ×3 fixtures,
multilayer, meta (rate 1/3), recorded teleportation (fixture + sci2001 -d) ✓. Degenerate inputs
(single node, pair, self-link only, disconnected, star) exit 0 with sane codelengths ✓. `-N`
trial selection uses the materialized codelength → safe. Minor: `--columnar` help text is stale
("Base map equation only for now"); the contract suite never exercises `-C -F` (manual matrix
above covers it for now).

**Priority:** ship #1 (flex coarsening); fix #6 (parsedOptions gate) + #5 (warning); #4 as a
follow-up feature; #8 reporting; update recs per #2/#3; #7 as an upstream issue.

---

### F15 — BUG (shipped branch 36efd697): `-C` best-of-N returns a NEGATIVE codelength on politicalblogs

Surfaced by running the fuller benchmark set at `-N10` (best-of-N, as Infomap is normally
run). `-C -d -N10` on `networks/db/politicalblogs.net` reports **Best codelength −0.578** —
an impossible value (map-equation codelength is ≥ 0). Undirected is worse: **398/500 trials**
materialize negative. Reproduced on the plain shipped `./Infomap` (not a worktree artifact).

**Localized (not fully root-caused):**
- The **columnar core is correct** — the internal codelength is always sane (~6.74–7.05 across
  all 500 trials, never negative). Only the **materialized** value (`m_hierarchicalCodelength`,
  what trial-selection AND output use) is corrupt.
- **Cross-trial contamination, proven:** engine seed 914636141 → internal 7.054 / materialized
  **7.054 in isolation** (`COL_FORCE_SEED`, `-N1`), but internal 7.054 / materialized **−0.544**
  when it runs as trial 2 of `-N10`. Identical seed, identical partition, different
  materialization ⇒ residual state from a prior trial.
- **Trigger = 2-level results.** Every negative is a 2-level materialization (233/233 in a 300-run
  sample); every multi-level materialization is fine. `initTree` routes `maxDepth==2` paths to a
  DIFFERENT path — `initPartition(modules, hard=false)` → `setActiveNetworkFromLeafs` →
  `consolidateModules(false)` → `m_hierarchicalCodelength = getCodelength()` — which returns a
  corrupt value once a prior (3-level) trial has left the OO optimizer/tree in a dirty state.
  The 3-level branch (the multi-level tree builder + `aggregateFlowValuesFromLeafToRoot`) is fine.
- **Not** self-loops (stripping politicalblogs' 13 self-loops: still 413/500), **not** isolated
  nodes (none), **not** the OO engine (OO `-N100` clean, 6.767). Clean on webND/sci2001/netsci/
  powergrid/air30k across 100–500 trials — politicalblogs is the reproducer (its base-search
  optimum is often exactly 2-level, so it hits the buggy branch far more than the others).

**Impact:** any `-C -N>1` run whose per-trial optimum is 2-level, after at least one 3-level
trial, can select a garbage negative partition and write it out. A notes generality-check network
(politicalblogs) is affected — the `-N1` numbers in F1/F5 are fine; the best-of-N numbers are not.

**Fix directions (pick one; both cheap):**
1. **Trust the core.** `columnarPartition` already has the correct value in `columnarL`; set
   `m_hierarchicalCodelength = columnarL` instead of re-deriving it from the materialized OO tree.
   The tree materialization should decide *structure* for output, not *codelength*. One line; also
   removes the composed-objective mis-report (F14 #8) as a side effect. Downside: masks the
   underlying tree reentrancy (worth a defensive assert `materialized ≈ columnarL`).
2. **Fix the 2-level reentrancy** in `initTree`/`initPartition(modules,false)` (reset the optimizer
   state `getCodelength()` reads). Correct at the source but a deeper change.

Recommend #1 + an assert, and file #2 as a follow-up. Prototype knob for isolation:
`COL_FORCE_SEED` in `scratchpad/wt`.

---

### F16 — Two fixes IMPLEMENTED on the branch + the fuller benchmark verdict (2026-07-18)

Implemented both fixes directly on the branch working tree (built, contracts pass, `-C` bit-exact):

**(A) Report the core's codelength, not the reconstructed tree's (fixes F15).** `columnarPartition`
now sets `m_hierarchicalCodelength = columnarL` after `initTree` (keeping the tree only for output
structure), instead of trusting the OO `getCodelength()` recompute. politicalblogs `-C -d -N10`:
**−0.578 → 6.740693** (sane; the two 2-level trials now report their true 7.03/7.05 and are
correctly not selected). Every other net unchanged. The `-vv` log still prints both (`codelength X,
materialized Y`) so the underlying 2-level `initPartition` reentrancy stays visible for a later
proper fix. Also fixes F14 #8 (composed meta+bias now reports the augmented objective).

**(B) `-F` includes the coarsening loop by default.** Extracted a shared `coarsenModules(L, maxSweeps)`
(merge + gated top-regroup, the converge search's second phase) and call it from both `refineHierarchy`
(byte-behavior-identical; `-C` verified bit-exact on science2001/powergrid/air30k/malaria/netsci) and
`optimizeFlexible`. `-F` now matches `-C` on memory/meta/multilayer (air30k 6.230→5.4725, malaria
7.489→7.4445, lazega-meta 6.053→6.0346) and is a no-op on base nets.

**FULLER BENCHMARK (all 11 nets, best-of-10, codelength+time+top+levels) — the `-F` verdict flips.**
With the knee in `-C` and coarsening now in `-F`, **`-F` is dominated by `-C`**:
- Quality: `-F` ties `-C` on memory/meta/multilayer/shallow-directed, but is WORSE on the base
  hierarchy nets (netsci +0.3%, powergrid +0.5%, webND +0.9%) — it skips interior refinement.
- Speed: `-F` is NOT faster. Equal on webND (24.5 vs 24.5s) and powergrid; SLOWER on science2001
  (6.0 vs 4.9s), malaria (8.8 vs 6.3s), air30k (7.7 vs 6.7s) — its repeated `refineBottomWithinParents`
  costs more than `-C`'s knee-limited refinement. The knee (F5) erased the ~30% webND speed edge that
  was `-F`'s original justification (F1).
- => **`-C` stays the default; `-F` is at most a "leaner/shallower map" dial, no longer a speed dial.**
  The old "`-F` is ~30% faster" claim (F1, pre-knee, single-trial) is stale.

Updated `benchmark-networks.md` (paths/type/flags for all 11 + malaria as the multilayer example) and
`columnar-pr-performance-section.md` (nested Meta/OO/columnar table + full `-F` vs `-C` table). Fixes
NOT committed (awaiting review); prototypes/knobs remain in `scratchpad/wt`.

---

### F17 — `-F`'s repeated bottom re-partition was wasted; single pass restores its speed niche

Daniel asked whether `-F` needs the costly repeated bottom re-partitions. Measured: `optimizeFlexible`'s
`for (iter<8) refineBottomWithinParents()` loop runs **exactly 2 iterations on every net**, and the 2nd
never improves — it only detects convergence at full O(n_leaves) cost. **It is provably idempotent:**
`refineBottomWithinParents` keeps every leaf inside its level-2 grandparent, so the leaf-set per
grandparent is invariant; re-partitioning the same leaf-sets within the same grandparents from singletons
with the same seed reproduces the partition (same argument refineHierarchy already uses for a single
interior level). Replaced the loop with a **single call** — bit-identical on all 11 nets (verified
cap=1 vs cap=8), ~25–35% faster on the deep/memory nets.

**This REVERSES F16's "`-F` is dominated" verdict.** With the single-pass bottom refine (fair same-session
best-of-10):

| net | -C cl / time | -F cl / time |
|---|--:|--:|
| web-NotreDame -d | 5.572794 / 27.4s | 5.624483 / **20.1s** (+0.9% cl, **−27% time**) |
| powergrid | 4.749076 / 0.34s | 4.772177 / 0.20s (+0.5%, −40%) |
| netsci | 4.051868 / 0.07s | 4.064284 / 0.06s (+0.3%) |
| science2001 -d / air30k / malaria / lazega-meta / multilayer / politicalblogs | (ref) | **ties -C on cl AND time** |

So `-F` is a genuine speed/quality dial again: on nets with real hierarchy it trades a fraction of a
percent of codelength for a faster, leaner, shallower map; elsewhere it matches `-C` outright. `-C` stays
the quality default. (F16's "no faster / dominated" was measured BEFORE this optimization — superseded.)

Net of F14–F17, three branch changes implemented + verified (contracts pass, `-C` bit-exact): (A) report
`columnarL` not the reconstructed-tree codelength (fixes the negative-codelength bug F15); (B) `-F` runs
the coarsening loop (safe on memory/meta/multilayer); (C) `-F` bottom refine is a single idempotent pass
(faster). Not committed — awaiting review.

---

### F18 — `-2` wired (PR #823) + the two-level benchmark + the memory-objective diagnosis (2026-07-18)

**Gap closure got organized:** flag-by-flag audit of the `-C` dispatch vs the OO path → tracking issue
#832 with sub-issues #824–#831 (soft cluster-data discarded; -T/-Tr wiring edges → fixed in PR #833;
inert core-loop knobs; dead search-shaping flags + missing one-level fallback; regularized-multilayer
correction deferred; `-C -F` untested in CI; upstream entropy-corrected mismatch; 2-level initPartition
reentrancy root cause).

**`-2` wired (PR #823, branch columnar-two-level):** `optimizeTwoLevelStack()` = full `optimizeTwoLevel`
materialized as a 2-level stack (so `hierarchicalCodelengthFromStack` / `coarsenModules` / `toNodePaths`
apply) + the module-merge coarsening. First cut on air30k: **+3.9% vs OO -2 AND 2.1× slower** — the only
bad net in the 11-net two-level table (base/meta/multilayer tie-or-beat OO, up to −83% time; full table
in columnar-pr-performance-section.md).

**Profile (air30k -2 -C, s123):** pass 1 (13k-leaf move loop) 0.56s → agg converges at **K=1353**
(aug 6.696) — the base-driven aggregation stops far too fine for the memory objective; fine-tune barely
helps (6.676); `coarsenModules` carries the whole memory coarsening **K 1344 → 328** (6.676 → 5.627).
Greedy pairwise merges are a weak optimizer vs OO's coarse-tune (module moves under the true mem
objective): OO -2 = 5.412.

**Shipped fix (2nd commit on PR #823): interleave a seeded gated leaf fine-tune with the merge** until
the pair stops improving. The merge reshapes modules far from where the leaf loop last saw them, so
re-tuning inside the merged structure recovers most of the gap and enables further merges:
air30k -N10 5.6049 → **5.4596** (gap +3.9% → **+1.22%**); lazega meta 6.0346 → **6.0212** (+0.06% vs OO);
malaria unchanged-to-better; base nets bit-exact (gate: loop only enters when the merge improved, which
the base objective's no-op merge never does). ~4 rounds to converge on air30k.

**Remaining lead (the principled fix, not shipped): module-level correction state.** Consolidate
per-unit (physical → flow) aggregates in `consolidateToNextLevel` so module-level move loops compute
true augmented deltas — makes the aggregation trajectory itself objective-aware. Expected to close both
the residual -2 gap (+1.22%, and the +58% time — today's leaf-heavy machinery re-tunes 13k leaves where
OO moves a few hundred modules) AND likely the hierarchical air30k gap (+1.5%). Related smaller lead:
try a seeded bottom re-tune after `coarsenModules` in `refineHierarchy` (the -2 result suggests the
"refine-after-coarsen always reverts" note only holds for the *interior* refine, not a seeded leaf
re-tune).

### F19 — Module-level corrections (PR #835): single-strategy verdict, alternation rejected (2026-07-21)

**The fix from F18's lead, shipped:** Mem/Meta maintain per-unit sparse attribute aggregates
(`setUnits` after each `consolidateToNextLevel`, `resetUnitsToLeaves` after aggregation), so
`moveDelta`/`applyMove`/`proposeMoveTargets` work on aggregated units and the module-level passes of
`optimizeTwoLevel` descend the true augmented objective. Per-pass selection reuses the move loop's
incrementally tracked correction total (`m_lastCorrection`); corrections without module-move support
(Lossy, the #827 K-bias) drop out of module passes and are recomputed per pass, exactly as before.

**Single-strategy matrix (the decisive experiment; seed 123, -N10, per-trial codelengths inspected;
"off" = base-only aggregation ≡ the branch tip, verified bit-exact):**
- air30k `-2`: off 5.4596 (+1.22% vs OO) → on **5.3953 (+0.03%)**, and on is *faster* (3.8 vs 5.4s).
  The families form disjoint quality bands ~1% apart: **every** objective-aware trial (worst 5.4055)
  beats **every** base-only trial (best 5.4596).
- air30k `-d --regularized` `-2`: off **6.0118 (+7.8% vs OO)** → on **5.5793 (+0.05%)**. The prior
  flattens the base link-flow signal, so steering aggregation by base-only deltas strays completely —
  the strongest evidence that base-only aggregation optimizes the wrong objective on memory nets.
- malaria: the mirror image — every objective-aware trial worse than the base family's best
  (`-2` 7.4450 → 7.4743, `-C` 7.4445 → 7.4789; both still beat OO by ~0.4%). The known overshoot:
  greedy module-level gains reach a coarseness the fine-tune + gated merges cannot split apart.
- lazega (meta): best-of-10 unchanged; per-trial the module-aware trials only ever improve.
- hierarchical air30k `-C`: 5.4725 → 5.4652 (−0.13pp only) — confirms the +1.3–1.6% hierarchical gap
  is the *separate* structural flat-optimum problem (F-map (b)), not a correction-participation one.
- Base networks (incl. web-NotreDame): no module-move-capable correction → bit-exact, full-set verified.

**Alternation evaluated and REJECTED.** A per-trial strategy alternation (even trials objective-aware,
odd base-only, best-of-N picks the family) was prototyped as a workaround for the malaria overshoot.
The single-strategy data killed it: it halves the sampling of the winning family (alt best ≥ pure-on
best on every net where pure-on wins), costs time (objective-aware trials are the *fast* mode — the
coarsening happens in cheap module passes instead of the merge-scan + re-tune tail), carries a second
search path, and hides the real defect. Decision (Daniel): ship pure objective-aware as the only
strategy and accept malaria +0.4% as a known limitation until the **split operator** (the subdivision
half of OO's coarse-tune) exists — that is the principled fix, and with it the overshoot argument for
base-only aggregation disappears entirely.

**Seeding dependency settled (#824):** pass acceptance scores the composed leaf partition with the
exact augmented objective, so a seeded solution can never be *replaced* by an augmented-worse one even
without module-level corrections — but improving *from* a seed at coarse scale needs them: base-only
module passes only explore base-improving directions (the 7.8% regularized number measures how wrong
that is). Soft cluster-data seeding on memory networks therefore depends on this PR.

### F20 — Coarse-tune done right: trajectory repair + winner deep repair (PR #890, 2026-07-21)

**The naive split operator failed the marginal-trade rule.** First cut (per-trial: pass-1-block
pieces + from-singletons pieces, interleaved with merge+retune): malaria `-2` 7.4743 → **7.3898**
(−1.55% vs OO) but 2.5 → 8.9s — rejected (Daniel: marginal score wins may only cost marginal time;
a 3× slowdown means find a better algorithm). Trace (env-gated timers, cumulative over -N10):
malaria split 4.05s of which **from-singletons re-derivation 3.17s** — 53 fresh attempts, 43
accepted (the keep-while-improving gate never closes), 1 869 memo misses because every accepted
leaf re-tune perturbs most modules' leaf sets; reg the mirror image (attempt cost: ~2 500-piece
levels rebuilt 104×). Root cause: the post-hoc operator re-computes, repeatedly and after fine-tune
has smeared the boundaries, granularity the aggregation itself already computed and threw away.

**Fix 1 — in-trajectory descending repair (every trial, ~free):** retain each aggregation pass's
unit Level + leaf composition (gated on module-move corrections; base nets pay nothing) and repair
right after aggregation converges, BEFORE fine-tune: the retained levels still nest exactly inside
the final modules, so re-sorting each granularity (coarse → fine, skipping the final pass's own
fixpoint) is a seeded move loop per level with zero derivation, zero intersection, zero
aggregateLevel (the Levels are kept, not rebuilt). Scoring == runPass (the node-flow term is pinned
to the leaf constant, so codelengths compare across granularities). Ladder-only `-2`: air30k
5.395254 → **5.393493/3.53s** and reg 5.579318 → **5.575735/3.42s** — both beat OO *and* run faster
than the pre-split tip (strict win-win); malaria neutral (its winning cuts cross trajectory-unit
boundaries — the trajectory cannot propose them).

**Fix 2 — deep repair of the winner (once per run):** the from-singletons discovery is real,
irreplaceable work on malaria — so spend it once, on the best-of-N partition, not inside every
trial. RunSession hook after the trial loop: seed a two-level stack from the best tree
(`seedHierarchyFromLeafPaths`), run the split interleave (`deepRepairTwoLevelStack`), keep if
better. Deterministic in serial AND parallel-trial modes (no cross-trial gating — that would make
results depend on completion order); repair seed derived from the config seed; cost amortizes with
-N. Measured (`-2`, s123 N10): malaria **7.422255/3.6s** (−1.12% vs OO at −58% time), air30k
**5.392623/3.9s** (beats OO), reg **5.575409/3.8s** (beats OO), lazega **6.017860269 == OO
exactly**. Basin-count ladder for malaria: 1 repaired basin → 7.422, ~3 basins (threshold sim) →
7.404, all 10 → 7.390 — a k-best-trials repair variant is the known lever if the last 0.4% ever
matters.

**Hierarchical finding:** the trajectory repair runs inside `optimizeTwoLevel`, so all
within-parent sub-optimizers inherited it — and `-C` results barely moved (malaria `-C`
bit-identical, air30k/reg ±0.01%). The hierarchical overshoot therefore forms in the **enter-flow
up-build**, not the sub-optimizer aggregations: the flat candidate (+ pass-1 sharing) stays the
hierarchical half of #889 and would import every `-2` number above into `-C` where flat wins
(malaria 7.479 → ~7.42, air30k 5.466 → ~5.393).

**Verified:** full 13-config × {`-2`, `-C`, `-C -F`} differential vs the branch tip — the only
changes are the four `-2` improvements and the ±0.01% hierarchical corr-net shifts; every base
network (web-NotreDame included), pref-25 and multilayer bit-exact; `-F` untouched by the
`subClusterLeaves` factoring; C++ suites pass; repaired runs deterministic (re-run bit-identical).
