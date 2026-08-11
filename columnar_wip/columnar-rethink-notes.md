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

**F20 addendum — per-feature attribution + single-trial cost (per the tightened reporting protocol).**
Codelength attribution (exact): the trajectory repair carries most of air30k (−0.033% of −0.049%)
and reg (−0.064% of −0.070%) at neutral-or-negative time (its sweeps are offset by cheaper
merge/retune tails); the winner deep repair carries ALL of malaria (−0.70%) and lazega (→ exact OO
tie). Deep-repair cost from the timing registry (`--timing-json`, N10): malaria 0.71s, reg 0.22s,
air30k 0.15s, lazega ~0 — total run vs tip, interleaved same-session: malaria +17%, air30k +4%,
reg −9%. **Single-trial (`-N1`): `deep_repair_s` ≈ 0.00s on all four** — a barren basin rejects
cheaply (piece aggregation + one settled sweep per source), so the post-trial pass does not
penalize single-trial runs on this set. One honest wrinkle: malaria `-N1` lands +0.005% worse
(7.525581 → 7.525940) — the trajectory repair changes the trial's downstream trajectory and
per-trial results are not monotone (best-of-N is what improves); the repair itself is gated and
never worsens its seed. Cross-session wall clocks proved misleading during this work (±20% between
back-to-back runs; the earlier "ladder faster than tip on air30k" was session drift) — the timing
registry's internal split is the reliable instrument for feature costs.

### F21 — Flat-first trials: the flat candidate lands (#889 hierarchical half, closes #834) (2026-07-21)

The `-C` overshoot forms in the enter-flow up-build (F20): on networks whose optimum is
(near-)flat with many modules, no interior refinement reaches the flat basin from the fine-blocks
build. Daniel's framing — alternate trials, "every second trial run the two-level algorithm as
seed" — is what shipped, after two design iterations driven by the marginal-trade rule:

1. **v1 (rejected): unconditional flat pipeline every even trial.** Quality landed immediately
   (air30k `-C` 5.4657 → 5.3937, beating OO), but web-NotreDame paid +30% wall for *zero* gain —
   five full `-2` solves on a network whose flat optimum is +21% worse than its hierarchy. Pure
   loss on hierarchy-winning nets = the same failure mode as the per-trial split operator (F20).
2. **v2 (shipped): probe-gated flat trials.** The even trial first runs the full aggregation
   *without* the leaf fine-tune (`optimizeTwoLevel(0, false)`) — module-level cost — and reuses
   its pass-1 blocks (`m_leafBlocks`) as the fine-blocks bottom for the regular strategy screen,
   so no second leaf sweep. Only if the probe lands within **0.5%** of the fine-blocks post-build
   codelength does the trial complete the leaf-level flat pipeline (deferred fine-tune to
   convergence + the optimizeTwoLevelStack merge↔retune interleave, factored as
   `completeFlatFromAggregation`), add the flat-bottom up-builds to the screen, and keep the flat
   stack as a gated candidate against the refined winner.

**The probe separates perfectly** (est/build, `COLUMNAR_DEBUG` print): every true flat-winner
≤ 1.000 (pref-mods 0.63, reg 0.80, air30k 0.84, malaria 0.88, polblogs 0.96, jazz 0.996, lazega
0.995, multilayer 1.000), every true flat-loser ≥ 1.008 (science2001 1.0079, netsci 1.034,
powergrid 1.10, web-NotreDame 1.18, ninetriangles 1.04). Margin 0.5% sits in the gap; the
initial 2% margin let science2001 through as the lone false positive (+36% wall for nothing).
Key asymmetry behind the tight margin: post-build refinement gains far more than the flat
completion does (air30k build 6.54 → refined 5.47; flat est 5.47 → completed 5.39), so a
generous margin only buys false positives.

**Winner deep repair extended to hierarchical runs**: the once-per-run repair now also fires when
a `-C`/`-F` winner is two-level-shaped (all tree paths length 2 — a flat trial won outright);
deeper winners untouched. This carries malaria `-C` 7.4743 → 7.4223 (repair = 0.50s of the 3.6s
run by the timing registry) and lazega to the exact OO tie 6.01786. air30k/reg winners are
3-level flat-bottom builds, so the repair correctly skips them.

Results (`-N10 -C`, per-feature attribution in the perf section): air30k 5.4657 → **5.3937**
(beats OO 5.3940 — #834's hierarchical residual closed), reg 5.6624 → **5.5751** (beats OO),
malaria 7.4789 → **7.4223** (beats OO by 1.1%), jazz + lazega → exact OO ties, pref-mods −2.63%
(8.4608 → 8.2384; OO 7.9280 keeps the lead — leaf-only bias, #827), polblogs −0.002%. `-F` gets
identical wins (flat trials wired in both entries); `-F` now ties `-C` on 10/13 configs.
**Nothing got worse**: `-2` bit-exact everywhere, `-N1` bit-identical on all 13 × {`-C`,`-C -F`}
(first trial stays hierarchical-first per Daniel's ordering), OO untouched.

Cost (interleaved same-session, the only wall numbers to trust — this session drifted +60% on
OO NotreDame): air30k +39%, reg +61%, malaria +37%, pref-mods +22%; NotreDame / science2001 /
powergrid / netsci unchanged within noise (probe skips). The costed nets are exactly the ones
whose codelength improves 0.8–2.6%; `-C` stays 32–63% faster than OO on all of them.

Determinism: strategy = global trial index parity (identical serial/parallel); repair
post-trial-loop with config-derived seed; re-runs bit-identical. Design lesson recorded: the
probe/gate pattern (cheap same-objective estimate → gate the expensive leaf-level work) is the
reusable answer to "operator X only helps some networks"; compare probes only against references
they can't trivially dominate (an up-build *on the probe's own bottom* is a superset of the flat
stack and always wins — the fine-blocks build is the honest reference).

### F22 — Flat-first, rethought: the redundant leaf re-derivation (#891 revision, 2026-07-27)

**The complaint (Daniel):** F21's flat trials bought their codelength at +22–61% wall — too much.
The rethink started by asking *where that time actually goes*, instead of rationing the flat
search harder.

**Measurement first.** Env-gated CPU timers around each phase of `optimizeColumnar`, accumulated
separately for hierarchical-first and flat-first trials (seed 123, `-N10 -C`, seconds per trial):

| net | hier: bottom / screen / refine | flat: bottom / screen / refine / complete / flat-build |
|---|---|---|
| air30k | 0.37 / 0.10 / 0.16 | 0.46 / 0.09 / **0.59** / 0.14 / 0.01 |
| air30k reg. | 0.14 / 0.13 / 0.13 | 0.29 / 0.14 / **0.60** / 0.15 / 0.01 |
| malaria | 0.22 / 0.02 / 0.22 | 0.21 / 0.01 / 0.00 / 0.06 / 0.01 |
| pref-25 | 0.15 / 0.03 / 0.20 | 0.18 / 0.03 / 0.04 / 0.19 / 0.01 |

`completeFlatFromAggregation` — "the expensive leaf-level pipeline" the whole probe/gate exists to
ration — is **0.14 s/trial** on air30k. The *refinement of the resulting build* is **0.59 s/trial**,
four times as much, and the per-trial trace says it earns nothing: all five air30k flat trials
print `build=5.393664 -> refined=5.393664` (gain 0.0000%), and four of five on reg. F21 had
rationed the cheap half and left the expensive half unrationed.

**Root cause.** `refineHierarchy`'s interior sweep starts at layer 0, and
`refineLayerWithinGrandparent(0)` re-partitions every leaf from singletons inside its grandparent.
On the fine-blocks bottom that pass *is* the hierarchy search — air30k `build=6.5515 -> 5.4732`,
16.5% for 0.10 s. On a flat bottom the leaf partition is already the two-level fixpoint:
`completeFlatFromAggregation` has just run the deferred fine-tune to convergence plus the
merge↔retune interleave. The refine re-solves a problem whose answer it is holding, at full leaf
cost, and reverts. `-F` has the same shape one level up via `refineBottomWithinParents`.

So F21's defect was never "the flat search is expensive". It was **a flat bottom being fed to a
refinement written for an unrefined bottom.**

**Fix (30 lines, `m_bottomConverged`).** Set by `completeFlatFromAggregation`; carried by whichever
build wins the strategy screen (`bestBottomConverged`); cleared at both entry points and when `-F`
falls back to the fine-blocks stack. While set, `refineHierarchy` starts layer 0 clean in its dirty
set, and `-F` skips `refineBottomWithinParents`. Interior layers *above* the bottom still refine,
and the existing neighbour-dirty logic re-marks layer 0 whenever a refine above it is accepted — so
the leaf re-derivation stays reachable exactly when the structure it nests in actually moved.

**Cost of the feature, before and after** (vs the pre-#891 tip; interleaved same-session A/B, each
config repeated 4–5× per binary and reported as the **minimum** observed time pooled over every
repetition — the session carried load average 24–65 and single runs scattered by up to 10 pp, so
the minimum is the only stable estimator. Wall and CPU agree within ~1 pp on every row; the phase
timers above agree with these ratios too):

| config | codelength Δ | base CPU → new CPU | F21 (wall) | F22 (`-C`) | F22 (`-F`) |
|---|--:|--:|--:|--:|--:|
| air30k | −1.32% | 3.68s → 3.92s | +39% | **+6.5%** | +11.7% |
| air30k reg. | −1.53% | 3.52s → 4.13s | +61% | **+17.3%** | +18…21% |
| malaria | −0.76% | 3.37s → 3.34s | +37% | **−0.9%** | +4.1% |
| science2001 pref-25 | −2.63% | 3.26s → 3.42s | +22% | **+4.9%** | +5.7% |
| jazz / lazega / polblogs | −0.20 / −0.28 / −0.002% | ≤0.07s (floor) | — | 0% | 0% |
| science2001 | 0 | 3.17s → 3.22s | ±noise | +1.6% | ~0% |
| web-NotreDame | 0 | 21.19s → 21.07s | ±noise | −0.6% | ~0% |
| powergrid / netsci | 0 | 0.25s → 0.26s / floor | ±noise | +4% (0.01s) / 0% | ~0% |

**Re-checked explicitly for "high cost, no gain" rows and there are none left**: every config the
flat search cannot improve lands between −0.6% and +4%, and the +4% (powergrid) is one hundredth
of a second at the measurement floor. Regularized air30k is the one config with a bill worth
naming, and it is now genuine flat-search work — the probe's aggregation passes (+0.15 s/trial,
expensive there because the tele/regularized corrections ride the module-level passes) plus the
completion (+0.15 s/trial), partly offset by the refine that no longer runs. Individual
repetitions of that row spanned +14% to +25% under load; +17% is the pooled minimum.

**Per-feature attribution** (exact, `-C -N10`): flat-first trials alone carry jazz (→ exact OO
tie), polblogs, air30k (−1.32%), reg (−1.53%), pref-25 (−2.63%) and lazega down to 6.02122; the
winner deep repair carries **all** of malaria (7.47430 → 7.42225, −0.70%) and the last step of
lazega (6.02122 → 6.01786, the exact OO tie). Malaria's flat trials on their own are worth only
−0.06% — and they *reduce* its trial cost, because malaria's flat-bottom builds are two-level
(`lvls=2`), so they skip the interior refine entirely.

**The one codelength change, and what it says about F21's claim.** Regularized air30k at seed 123
goes 5.575137 → 5.576024 (+0.016%): the single flat trial whose leaf re-derivation *did* gain
(0.24%). Seed sweep, `-C -N10`:

| seed | F21 | F22 | OO |
|---|--:|--:|--:|
| 123 | 5.575137 | 5.576024 | 5.575653 |
| 234 | 5.575581 | 5.575581 | 5.573527 |
| 345 | 5.574277 | 5.574277 | 5.573256 |
| 456 | 5.579702 | 5.579702 | 5.574199 |

It fires on **one seed in four**; on the other three the variants are bit-identical and `-C`
already sits 0.02–0.10% *above* OO. F21's "regularized air30k beats OO" was therefore a seed-123
artefact of that lottery ticket, and the honest claim is a tie within seed noise. ~3 s per `-N10`
run for a 1-in-4 chance fails the marginal-trade rule; a winner-only variant would not recover it
either (at seed 123 the *winning* trial is one where the refine gains nothing).

**Verified:** `-N1` bit-identical to the pre-#891 tip on all 13 configs × {`-C`, `-C -F`} (26/26)
*and* identical in cost (the post-trial repair correctly declines: an `-N1` winner is
hierarchical-shaped); `-2 -C` bit-identical on all 13; `-N10` re-runs bit-identical; every `-C` /
`-C -F` codelength identical to F21 except the reg row above.

**Design lesson.** F21's probe/gate rationed the right operator for the wrong reason — it was sized
against the flat completion, which measurement then showed to be cheap. Generalizing:
**every refinement in this engine encodes an assumption about how converged its input is, and
handing it a *more*-converged input than it expects is a silent, full-price no-op.** Worth checking
the same way wherever a new bottom is introduced under an existing refinement.

**F22 addendum — why the hierarchical refinements are not seeded (Daniel's question, measured).**
The seeded-init primitive Daniel has raised repeatedly — OO's fine-tune initialisation: singletons,
then deterministically place every unit back into its old module, then start the greedy loop — *is*
implemented here as `seedAssignment()` (`ColumnarMapEquation.cpp:663`), and it already carries the
`m_deferTerms` optimisation (running plogp terms rebuilt once at the end, one O(K) pass instead of
~12 plogp per placed unit). It has four call sites: the in-trajectory descending repair (`:1143`),
the two-level leaf fine-tune (`:1186`), `splitTopModules` (`:1309`) and `retuneLeavesWithinModules`
(`:1456`) — **all inside the two-level search**. Neither hierarchical refinement uses it:
`refineLayerWithinGrandparent` and `subClusterLeaves` each construct a fresh `ColumnarTwoLevel` and
call `subOpt.optimizeTwoLevel()` with default arguments, i.e. a full from-singletons re-derivation
that discards the partition it was handed. So the gap was real and worth measuring.

Prototyped (env-gated so all variants share one binary): an optional `pass1Seed` on
`optimizeTwoLevel` that replaces pass 1's `initPartition()` with `seedAssignment(seed)`, wired into
both hierarchical refinements — `splitTopModules` deliberately left from-singletons, since
from-singletons discovery is its entire purpose. Full set, `-C -N10`, CPU seconds:

| net | from-singletons (skip variant) | seeded everywhere | Δ codelength | Δ CPU |
|---|--:|--:|--:|--:|
| web-NotreDame | 5.57279424 / 20.64s | 5.62354266 / 17.79s | **+0.91%** | −14% |
| powergrid | 4.74907624 / 0.27s | 4.75626628 / 0.21s | **+0.15%** | −22% |
| netsci | 4.05186752 / 0.02s | 4.05395419 / 0.02s | +0.052% | 0% |
| science2001 | 7.83343660 / 3.19s | 7.83373138 / 2.63s | +0.004% | −18% |
| politicalblogs | 6.74058207 / 0.07s | 6.73939588 / 0.07s | −0.018% | 0% |
| malaria | 7.42225457 / 3.37s | 7.42225457 / 3.00s | 0 | −11% |
| pref-25 | 8.23835056 / 3.43s | 8.23835056 / 3.23s | 0 | −6% |
| air30k | 5.39366442 / 3.85s | 5.39365252 / 3.96s | −0.0002% | +3% |
| air30k reg. | 5.57602419 / 4.11s | 5.57601959 / 4.42s | −0.0001% | +8% |

**Verdict: seeding the hierarchical refinements is a speed/quality dial, not a free speedup.** On an
*unrefined* fine-blocks bottom the from-singletons re-derivation is doing genuine discovery — it is
what turns air30k's `build=6.5515` into `5.4732` (16.5%) — and seeding confines the greedy loop to
the neighbourhood of the fine blocks it was handed. It buys 11–22% on the deep networks and pays
+0.91% on web-NotreDame for it, which is `-F` territory, not `-C` territory. (Under `-F` the same
switch is genuinely mixed: netsci −0.108%, powergrid −0.070%, polblogs −0.018% *better* and faster,
web-NotreDame +0.539% worse. Logged as a lead for the `-F` dial, not shipped.)

**The sharp version of the question — seed the converged flat bottom instead of skipping it**
(seeded only when `m_bottomConverged`; verified to change nothing on the other 11 configs):

| config | from-singletons (#891) | **skip (shipped)** | seeded instead |
|---|--:|--:|--:|
| air30k | 5.393664418 / 5.28s | **5.393664418 / 3.75s** | 5.393652521 / 3.95s |
| air30k reg. | 5.575137160 / 5.20s | **5.576024192 / 4.02s** | 5.576019591 / 4.28s |

A seeded pass is the cheapest possible way to *ask* whether that layer holds anything, and the
answer is 0.0002% / 0.0001% for +5.3% / +6.5% CPU — which is independent confirmation that the
layer really is at its fixpoint, and by the marginal-trade rule not worth buying. The skip stays.
Worth remembering that the seeded variant is the "insurance" option: it proves the claim per run
instead of assuming it, for ~5–6% on the two air30k configs and nothing anywhere else.

### F23 — The refine knee went stale: 5e-3 → 1e-3 (2026-07-28)

**Daniel: "don't assume current convergence thresholds are optimal."** He was right, and the reason is
mechanical rather than a tuning error. F5 raised `m_minRelTuneImprovement` 1e-3 → 5e-3 on the
measurement that web-NotreDame converged in **3** interior sweeps with the 3rd worth +0.06%. It now
takes **5.5 sweeps (max 7)** and the truncated tail is worth **0.111%**. The constant never changed;
what it truncates did, as #835/#890/#891 reshaped the search around it. F13 is the companion evidence
for how load-bearing this one number is: gpcache was −15% at knee 1e-3 and net-negative at 5e-3, i.e.
an idea was rejected because of where the knee happened to sit.

**Equal-CPU-budget is the decisive framing, and it had never been run.** F5 and F13 both tuned at a
fixed trial count. For a best-of-N search that is the wrong axis — a looser knee makes each trial
cheaper, so at fixed CPU you can buy more trials. Measured on web-NotreDame (seed 123), the shipped
knee is off the Pareto frontier at **every** budget:

| CPU | frontier config | codelength | 5e-3 at the same CPU |
|---|---|--:|--:|
| 3.4s | R=1e-3, N=1 | 5.569547986 | N=1 → 5.572794236 |
| 20.0s | R=0, N=6 | 5.567696152 | N=10 (22.7s) → 5.572794236 |
| 25.2s | R=1e-3, N=10 | 5.567411908 | N=13 (26.9s) → 5.572723204 |
| 32.0s | R=0, N=10 | 5.566609295 | N=16 (32.1s) → 5.572723204 |

**R=1e-3 at N=1 (3.38s) beats R=5e-3 at N=20 (39.9s)** — a twelfth of the CPU, 0.06% better. Going
N=10→20 at the shipped knee costs +75% CPU for 0.0013%. Mechanism: per-trial spread is ~0.13%, but
refinement shifts the whole trial *distribution* down, so R=0's median trial beats R=5e-3's
best-of-10. Extra trials cannot reach where a deeper refine goes. (Follow-up deferred per Daniel:
master's `--converge` adaptive-trial feature is the right lever for reclaiming that budget, and it
composes — trials plateau long before refinement does.)

**Shipped: 1e-3** (Daniel's pick, "being at the pareto frontier seems best"). R=0 was the other live
option (full −0.106% for +47% CPU) and stays documented as the higher-budget frontier point.

**Verified serially at the merge tip, interleaved, min-of-3, full 13 configs × {`-C`, `-C -F`}:**

| config | 5e-3 | 1e-3 | Δ cl | CPU | wall | lvls |
|---|--:|--:|--:|--:|--:|--:|
| web-NotreDame `-C` | 5.57279424 | **5.56741191** | −0.0966% | 23.38→25.42s (+8.7%) | 25.74→26.10s | 6→6 |
| powergrid `-C` | 4.74907624 | **4.74650715** | −0.0541% | 0.26→0.32s (+23.1%) | 0.31→0.36s | 5→5 |
| 11 other `-C` | — | bit-identical | 0 | −3.4…+2.1% | — | — |
| all 13 `-C -F` | — | bit-identical | 0 | −6.3…+2.1% | — | — |

The 22 bit-identical configs put this batch's noise floor at ~±3.5%, so webND's +8.7% is real and
powergrid's +23.1% is real but +0.06s absolute. **Blast radius is structural, not lucky:**
`refineSweeps = (numInterior <= 1) ? 1 : maxSweeps` means a stack with at most one interior layer runs
exactly one sweep, so science2001/air30k/malaria cannot react; and `-F` never calls `refineHierarchy`
at all. Against OO the change is worth more than it looks: web-NotreDame's gap closes 5× (+0.121% →
**+0.025%**) and powergrid's win widens (−0.156% → **−0.210%**).

**Caps audited (instrumented counters) — three are dead code, one binds but must not be touched:**

| cap | value | max observed | binds? |
|---|--:|--:|---|
| `kCoreLoopLimit` | 10 | 10 (truncates) | **YES** — webND 923/430844 calls, `-F` 774/79003, air30k 11-17 |
| `kMinImprovement` | 1e-10 | — | **NO** — zero positive-gain rejections on any net |
| `coarsenModules(L, 1000)` | 1000 | **3** | no (333× headroom) |
| `round < 100` (×3) | 100 | **5** | no |
| `refineHierarchy` maxSweeps | 1000 | **7** | no |

`kCoreLoopLimit` fires often, so **#826 is wrong that it never does** — but raising it to 20 makes
webND **+0.048% worse** and the seed-123 malaria −0.314% "win" evaporated on seeds 234/345. It is a
trajectory perturbation, not a harmful truncation. Keep at 10. `kMinImprovement` is provably inert:
every rejected gated step had gain ≤ 0, never in (0, 1e-10]. `kFlatProbeMargin` 0.005 still separates
perfectly (winners ≤ 1.0000, losers ≥ 1.0079, essentially unmoved since F21) — keep.

**Combining absolute + relative (Daniel's suggestion) is algebraically degenerate.** Both conjuncts
test the same scalar, so OO's AND form ≡ threshold `max(A, R·startL)` and OR ≡ `min(A, R·startL)` — a
1-D dial, not a 2-D grid. Confirmed empirically: `A=1e-4, R=5e-3` is bit-identical to the default and
`A=3e-2, R=0` reproduces it too (3e-2 ≈ 5e-3 × startL). The same degeneracy holds in the OO form at
`InfomapBase.cpp:2332`. **A is only non-redundant when R=0** — which is its real job: an absolute floor
is what makes full convergence safe rather than a second dimension.

**And the shape question has a sharper answer than "no".** Geometric-decay stopping collapses: every
ratio from 0.1 to 0.9 gives the same 2-sweep answer, because sweep 2's gain is already <10% of sweep
1's — yet **41 sweeps still buys 0.093%**. The gain tail is made of individually tiny sweeps, so *no
gain-magnitude criterion can capture it*. The shape is fine; the framing "stop when gains get small"
is what cannot work. Only the threshold's position is a real lever. Per-layer knees were dominated
(one config reached full-convergence quality ~7% cheaper — inside noise, not worth the complexity).

**Run-results log added: `columnar_wip/columnar-search-runs.tsv`** (Daniel's request, for plotting the
codelength/time Pareto front). One row per measured run, 26 flat columns:
`batch, datetime_utc, load1m, reps, agg, binary_md5, commit, pr_issue, network, nodes, objective,
flags, engine, variant, knee_R, trials_N, seed, operator, operator_cfg, codelength, wall_s, cpu_s,
top, levels, derived, notes`. **Read `batch` before comparing times**: this session's noise floors
ranged ±3% to **13%** (the threshold batch measured 24.45/22.41/21.59s for a *bit-identical*
web-NotreDame partition), so the time axis is only comparable within a batch — hence the `load1m`,
`reps` and `agg` columns. `derived=1` marks rows reconstructed from reported percentages rather than
measured absolutes; exclude them for a clean front. `top`/`lvls` are populated only for the serial
batches. Batches so far: `perf-refresh-891` (13×5 at the merge tip), `knee-ab` (this change, serial),
`knee-budget` (the equal-CPU sweep above), `hsplit`, `freemove-verify`, `partseed-curve`,
`partseed-best` (see F24).

### F24 — Hierarchical search exploration round: seven candidates, two wins (2026-07-28)

Daniel: "go through the whole hierarchical algorithm and see if you can improve it and don't limit
yourself with current features or ideas." Seven candidates were implemented in isolated worktrees, all
env-gated with default-OFF verified bit-identical, and the promising ones adversarially verified.
**Everything below was measured at knee 5e-3 and must be re-baselined at the F23 default of 1e-3
before any of it ships** — webND now runs 5.5 refine sweeps instead of 2, which changes the ground
under at least two of these results.

**A correction to the record first.** The premise for revisiting the organic/bubble idea was that the
F8-F11 arc lacked seeded refinement. It did not: F7 implemented exactly the OO fine-tune init
(`LAB_SEED=2`, "seed pass-1 of optimizeTwoLevel = fine-tune, then aggregation = coarse-tune") and F10
added `LAB_ORGSEED` to A/B seeded vs from-singletons *inside* the bubble (seeded 5.651398/4.51s and
inert, from-singletons 5.589027/6.72s and climbing). The legitimate reason to revisit was different
and remains valid: the **engine underneath changed** — F11's air30k +14% was measured when aggregation
was base-only, the same defect whose fix (#835) moved regularized air30k 6.0118 → 5.5793.

#### THE UNIFYING MECHANISM (the most reusable output of the round)

**The pipeline's stages are not independent: pre-improving a stage's input disarms it.** F22 found this
from one side (refining an already-converged bottom is a full-price no-op). Four candidates rediscovered
it independently from other sides:

- **A (trajectory bottoms)**: a coarser bottom wins the post-build screen (powergrid by 4.7%) and then
  refines **0.24-0.54% worse** in 5/5 trials — the coarseness it gained is exactly the headroom
  refinement needed. Force-refining webND's coarse bottom: 1.05-1.66% worse in 5/5 at +45% CPU.
  **Corollary worth remembering: the post-build codelength is NOT a valid cross-granularity comparator**,
  so any future bottom candidate cannot be screened against another granularity this way.
- **D (mid-build seeded polish)**: the F10 fixpoint artefact does *not* reproduce — polish of a freshly
  nested layer is accepted 69/69 (webND), 71/76 (powergrid), so F7's freshness prediction was right.
  It still loses, because a gated improvement to an *intermediate* state is not monotone in the final
  one: placed before `refineHierarchy` it lowers the reference the from-singletons refine must beat,
  steals its accepts, trips the knee sooner, and corrupts the strategy screen (powergrid 5→7 levels,
  +0.61%). Placed after, it is monotone and never regressed but only mops up residue (+10-36% CPU for
  −0.004..−0.093%).
- **E (cross-parent relocation)**: accepts 19/26 passes at webND's leaf-module layer under `-F` but
  **0/24 at the same layer under `-C`** — `refineLayerWithinGrandparent` already reaches those
  configurations indirectly.
- **hsplit**: on base networks splits are accepted only *above* the leaf layer (science2001 13/13 at
  k=1, NotreDame 10/10 at k=2..4) while k=0 accepts **nothing** and is the most expensive level
  (NotreDame k=0 = 2.11s of a 3.12s operator budget, 0/19) — because refine(0) just re-derived it.

**Corollary: anything that helps `-C` marginally helps `-F` a lot**, because `-F` has no interior refine
to disarm. Three candidates independently landed there (E, D, B-insert), which makes narrowing `-F`'s
quality gap the most promising unshipped direction out of this round.

#### REJECTED, with the evidence

- **A — trajectory bottoms as extra screen candidates.** Zero codelength change on all 5 nets (and both
  `-C -F`) for +1.1..7.1% CPU. Winning granularity per net: webND fine blocks K~31.6k (trajectory 6.7%
  worse); powergrid trajectory K~615-637 wins the screen but refines worse; air30k/malaria the completed
  flat bottom dominates by construction (it *is* the coarsest trajectory level plus fine-tune plus
  merges: air30k flat build 5.394 vs best trajectory build 5.471). The ladder is squeezed out from both
  ends. **Do not retry "which granularity starts the build" — the lever is refinement headroom.**
- **B — interior level collapse / insert.** Neither changes best-of-10 codelength or level count on any
  net; collapse accepted 1× in 10 trials (a losing one), insert 4×, all losing. The real finding is the
  **rejection ladder**: removing the top level costs +0.35% (powergrid), then +1.9%, +7.2%, +23.8% going
  down, because a level's index-code saving scales with the units it groups. **The up-build's stopping
  rule is essentially exact, and refinement makes levels MORE worth keeping, never less.** Inserting a
  level below the leaf modules (the OO recursion direction) is catastrophic: +6.8% air30k, +9.7%
  science2001 — the columnar-6 vs OO-13 depth gap is not something the bottom of the stack wants.
  Pre-refine placement makes powergrid's insert fire repeatedly on the raw build yet land 4.749076 →
  4.749796 (+0.015%) worse: the inserted level steers refinement into a worse basin.
- **C — composition-change-gated seeded refinement (F7's unbuilt middle). This closes F7.** The
  composition-change distribution is **bimodal, not spread**: webND k=0 sweep 2 (mean frac 0.4573,
  reproducing F7's "~46% churn" exactly) has 2044/6050 grandparents *exactly* unchanged (33.5% of
  leaves), only 375 (6.2%) anywhere in (0, 0.4), and 3019 (49.9%) at ≥0.4. **There is no middle to
  tune — a finer dial cannot exist**, which is why the threshold curve is flat from X=0.02 to 0.4.
  Second finding: the gate can only fire on a *re*-refine, and `refineSweeps=1` for ≤1 interior layer
  means science2001/air30k/malaria never have one (history/total refine calls per `-N10`: webND 38/77,
  powergrid 30/60, the other three 0). Third and most useful: **seeding's quality damage is localised to
  the FIRST refine of each grandparent, not to staleness** — seeding every re-refine and sparing only
  the first costs +0.06..0.10% on webND against +0.91% for unconditional seeding. Speed was −1.6% inside
  a ±3% floor, i.e. nothing. NOTE: measured at knee 5e-3 where webND reaches 2 sweeps; at 1e-3 the
  re-refine domain roughly triples, so the *speed* half deserves one re-look.
- **D — mid-build seeded polish.** See mechanism above. Under `-C`: +10..41% CPU for zero change on 3/5
  nets, +0.033% on powergrid, only webND gains (−0.093%). Under `-F` the same knob is worth −0.11%
  (webND) and −0.23..−0.30% (powergrid) because it is then the only interior tuning there is.
- **E — cross-parent relocation.** Quality confirmed and seed-robust, cost claim refuted. Under `-C`
  the only win is powergrid 4.749076238 → 4.745919949 (−0.0665%, 0.28→0.28s CPU, 2 wins/2 ties over 4
  seeds); webND `-C` is bit-identical at N10 *and* costs a real +3.0% paired mean / +6.3% min-of-13 from
  24 full seeded move loops over a 38,941-unit layer that are all rejected. Under `-F` it is a genuine
  win: powergrid −0.0756/−0.1433/−0.1399/−0.1763% on **4/4 seeds** (0.16→0.19s), webND
  −0.035/−0.002/−0.052% on **3/3** (17.16→17.92s). Structurally unreachable on 3 of 5 benchmark nets
  (needs a ≥4-level stack; science2001/air30k/malaria make zero calls). Indicated fix: gate to layers
  above the leaf-module layer — the trace says k=1 accepts 0/24 under `-C` while k=2 accepts 6/22 and
  k=3 4/12, so that should delete the cost and keep the wins.

#### WIN 1 — the #890 split operator extended to the hierarchical path (Daniel's directive)

`splitLevelModules(k, L, allowSingletons)`: partition a level-(k+1) module's level-k children into
pieces, aggregate a piece-level network, run a **seeded move loop** over pieces (so a piece can land in
any module including an empty one → group-split *and* cross-parent relocation), gate on
`hierarchicalCodelengthFromStack()`. Called from the `coarsenModules` interleave for stacks with ≥3
levels, plus a once-per-run winner variant. `subClusterLeaves` generalised to `subClusterUnits` with the
enter-flow transform for interior levels.

`-C -N10` seed 123, CPU min-of-3 (load 24-57, indicative):

| network | off | auto+winner | Δ cl | Δ CPU | acc/att | lvls |
|---|---|---|--:|--:|---|---|
| **malaria** | 7.422254572 / 3.70s | **7.400465990 / 3.76s** | **−0.294%** | **+1.6%** | 9/21 | **2→3** |
| air30k | 5.393664418 / 4.52s | 5.391903126 / 5.72s | −0.0327% | **+27%** | 42/79 | 3→3 |
| air30k reg. | 5.576024192 / 4.09s | 5.575002740 / 5.13s | −0.0183% | **+25%** | 27/57 | 3→3 |
| web-NotreDame | 5.572794236 / 23.19s | 5.572610218 / 24.16s | −0.0033% | +4.2% | 10/70 | 6→6 |
| science2001 | 7.833436601 / 3.41s | 7.833339072 / 3.74s | −0.00125% | +9.7% | 13/38 | 3→3 |
| powergrid + 6 others | unchanged | unchanged | 0 | floor | 0 acc | — |

Malaria −0.294% for +1.6% is seed-consistent (−0.294/−0.274/−0.280% on 123/234/345) and is **the first
time a hierarchical `-C` solution beats malaria's repaired flat one**. No codelength regression
anywhere; determinism verified.

**The correction gate should NOT be inherited, but it is right per LEVEL.** `splitTopModules` returns 0
unless a module-move-capable correction is attached ("the base merge is a no-op"); that reasoning does
not transfer, because hierarchical over-merging comes from `buildHierarchyFromBottom`'s greedy
enter-flow super-search, which runs on base networks too — and indeed base nets do accept splits. But
only above the leaf layer. Hence the shipped policy `auto` = interior always + leaf only when a
module-move correction is attached, which recovers all of `all`'s codelength at 3-5× less operator cost.

**A load-bearing interaction:** with the per-trial split but *without* extending the winner deep-repair
hook to deep winners, malaria **regresses** 7.4223 → 7.4453 (+0.31%) — the improved hierarchical trial
outscores the unrepaired flat trials, the winner is no longer two-level-shaped, and the existing flat
repair never fires. The hook extension is not optional.

**Open cost problem:** air30k/reg buy −0.02..−0.03% for +25-27%, which fails the marginal-trade rule.
That cost is k=0 (~1.0s) plus the extra coarsening sweeps each accepted split re-triggers. Next step is
to ration k=0 the way F21/F22 ration the flat pipeline (cheap probe, or "only after the merge accepted
something"); malaria's win is also at k=0 but costs 0.06s against air30k's 1.0s, so the discriminator
is attempt volume, not level. Also flagged: `coarsenModules`' gated lambda copies all of
`m_hierLevels` **including the leaf network** on every step — pre-existing, but the split operator makes
it fire far more often, which revives the "B1 incremental gate" idea the TL;DR dismissed as "negligible
when gated" (it was measured when gated steps were rare).

#### WIN 2 — partial seeding: release the boundary, lock the cores (Daniel's idea)

Daniel: "maybe the seeding can be partial even to not lock too much." Built without touching
`seedAssignment`: compact the *locked* units' modules to 0..K-1 and hand each *released* unit a fresh id
K, K+1, … (compacting over locked units only is essential — compacting all modules first overflows the
sub-network id space at high release fractions). Endpoints verified: P=1 reproduces from-singletons
exactly, P=0 reproduces full seeding exactly.

Best setting = boundary release of the loosest half, leaf layer only (`bnd P=0.5 k0`):

| network | seeds | mean Δ cl | per-seed | Δ CPU |
|---|--:|--:|---|--:|
| malaria | 5 | **−0.211%** | +0.215, −0.243, 0, −0.613, −0.413 | **−11%** |
| powergrid | 3 | **−0.075%** | −0.126, −0.057, −0.043 (3/3) | 0% |
| web-NotreDame | 5 | **−0.063%** | −0.078, −0.011, −0.059, −0.082, −0.084 (5/5) | −2% |
| air30k | 3 | 0.000% | bit-identical every seed | 0% |
| science2001 | 3 | +0.016% | +0.009, +0.024, +0.015 (3/3 worse) | −4% |

**It costs no time** — a quality win at zero or negative CPU, which is rare in this arc, and it passes
the marginal-trade rule outright. Not a best-of-N lottery: the per-trial *mean* moves down at every seed
(webND −0.027..−0.072%, powergrid −0.083..−0.106%, malaria −0.164..−0.321%), i.e. the whole distribution
shifts.

**The inverse control is what makes this a mechanism rather than a coincidence.** Releasing the same
number of units by the *inverse* looseness ranking (lock the boundary, free the cores) gives a perfect
ordering 6/6 across networks and seeds, on both best-of-10 and trial mean:

**bnd0.50 < baseline < inv0.50 < rand0.50**

Inverse release is *worse than baseline*; boundary release is better. So which units you release matters
more than how many — it is a better-targeted search, not a differently-sized one. (`inv` beating `rand`
is a secondary hint that releasing a coherent stratum beats scattering holes through every module.)
`small` (release modules under a size threshold) is inert — T=2/3/5/10 all land at the fully-seeded
codelength. And **partial seeding is a quality lever, not a speed lever**: full seeding's −11..−22% CPU
lives at release ≤0.10 where webND is +0.47..+0.62% *worse*; wherever the codelength wins, CPU is inside
the noise band.

Caveats: air30k is inert *because* of the F22 skip (its `-C` winner is a flat-first trial whose leaf
layer is already skipped) — so F22 and partial seeding interact. science2001's +0.016% is below the
0.1% reporting bar but perfectly consistent 3/3. The optimum release fraction is not located (q=0.5 and
0.75 are near-tied, nothing finer tested), and the boundary metric (crossing link flow / total link
flow, computed inside the grandparent) was never compared against the `exit/(flow+exit)` alternative.

#### OPEN LEADS OUT OF THIS ROUND
1. Re-baseline everything at knee 1e-3 (F23) before combining anything.
2. Ration `splitLevelModules` at k=0 (probe or merge-triggered) to fix air30k's +25-27%.
3. Make `coarsenModules`' gate incremental (bit-exact) — cheapens every gated operator here.
4. Gate `refineLayerFreely` to layers above the leaf-module layer.
5. An `-F` quality package: E relocation + D mid-build polish + B interior insert all win there.
6. Partial seeding: finer q sweep, wider seed sweep, and compare the two boundary metrics.

### F25 — Partial seeding: release the boundary, lock the cores (PR #985, 2026-08-10)

**Daniel's idea, restated:** "there is not only a single way to implement seeded bubble ... maybe the
seeding can be partial even to not lock too much." That last clause is the whole result.

A grandparent re-refine had exactly two settings and both waste something. **From singletons**
(`refineLayerWithinGrandparent`'s default) rediscovers the entire sub-partition including the module
cores that were never in doubt, and it is the most expensive pass in the search. **Fully seeded**
(`seedAssignment`) reproduces the current partition, the gate reverts, and the pass is a full-price
no-op — F10's "fixpoint artefact", re-confirmed at F22 (seeding a converged bottom gains 0.0002%).
Partial seeding is the middle: rank a grandparent's units by how much of their flow leaves their
current module, LOCK the confident cores, RELEASE the loosest fraction q as fresh singletons.
Implementation compacts the LOCKED units' modules to 0..K-1 and gives released units fresh ids
K, K+1, … — compacting over all modules first can overflow the sub-network's id space at high q.

**The ranking is the mechanism, not the release count.** Inverse control (webND, q=0.5, s123):
`bnd −0.015% < base < inv +0.069% < rand +0.153%`; malaria trial mean `bnd −0.261 < iex −0.127 <
base < rand +0.050 < inv +0.081`. Releasing the same NUMBER of units by the inverse ranking is worse
than baseline. That is what distinguishes this from "a cheaper refine".

**Round 1 (at knee 5e-3) did NOT survive the deeper knee — and the fix came from a REJECTED idea.**
At 1e-3, round 1's setting (bnd q=0.5, leaf-only, always-on) gave webND only −0.024% (was −0.063%),
malaria's headline −0.211% turned out to have been carried by seeds outside 123/234/345 (seed 123
reproduces round 1 exactly at +0.215%, which validated the port), science2001 kept +0.016% on 3/3,
and extending to `-F` cost webND +0.080% on 3/3. What rescued it was F24-C's finding — the
composition-gated experiment that was itself rejected: **seeding's quality damage is localised to the
FIRST refine of a grandparent, not to staleness.** Turned into policy (partial-seed only when
`refineHierarchy` sweep > 0), every regression disappears. Worth recording as a pattern: a rejected
experiment's *diagnosis* outlived its *implementation*.

**Shipped policy:** bnd, q=0.40, re-refine only, all interior layers.

| config (`-C`) | before | after | s234 | s345 | s456 |
|---|--:|--:|--:|--:|--:|
| web-NotreDame | 5.567411908 | **5.560674868** (−0.121%) | −0.114% | −0.099% | −0.140% |
| powergrid | 4.746507150 | **4.739334923** (−0.151%) | −0.169% | −0.253% | −0.076% |
| netscicoauthor2010 | 4.051867517 | **4.049603407** (−0.056%) | −0.000% | −0.005% | — |
| other 10 configs | — | bit-identical | ✓ | ✓ | — |

**Milestone: web-NotreDame `-C` now BEATS OO** — 5.560674868 vs 5.566041380 (−0.096%). Its arc across
three PRs: +0.121% behind → +0.025% (F23 knee) → **−0.096% ahead**. It was the last large base network
where the columnar engine trailed. powergrid's win widened −0.16% → −0.21% → **−0.36%**. `-C` now
ties-or-beats OO on **9 of 13** configs.

**q sweep** (per-trial mean, always-on, s123; and RE+ALL best-of-10):

| q | 0.20 | 0.25 | 0.33 | 0.40 | 0.50 | 0.60 | 0.667 | 0.75 | 0.85 |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| always webND | +0.170 | — | — | +0.005 | −0.028 | −0.036 | +0.013 | −0.011 | −0.012 |
| always powergrid | — | — | — | −0.164 | −0.159 | −0.070 | −0.085 | −0.070 | −0.004 |
| RE+ALL webND (best) | — | −0.094 | −0.113 | **−0.117** | −0.096 | −0.072 | — | −0.059 | — |
| RE+ALL powergrid (best) | — | −0.234 | −0.183 | **−0.191** | −0.128 | −0.142 | — | −0.073 | — |

Broad plateau, collapse below q≈0.33 when always-on, degradation above 0.6. Round 1's q=0.5 was
near-optimal but not optimal.

**Metric finding worth keeping: `exit/(flow+exit)` is DEGENERATE on undirected first-order networks.**
`FlowCalculator.cpp:1475` sets `node.exitFlow = node.flow`, so the ratio is 0.5 for every leaf and the
ranking collapses to node order. Proof: on powergrid that metric and its exact inverse (`iex`) give
**bit-identical** 4.736119954. It is a genuine metric only on directed/higher-order networks, so the
module-aware crossing-flow ratio ships. Head-to-head where `ex` is meaningful: webND bnd −0.024 / ex
−0.037 (tie under the RE policy); malaria bnd −0.009 / **ex +0.195**.

**Cost — one measurable bill.** powergrid −0.15…−0.25% for **+7.4% CPU** (2.97 → 3.19s at `-N100`,
resolving against a −0.3% floor; +9.7% on 0.31s at `-N10`). webND +1.2% against a ±2.1–2.4% floor =
not resolvable; netsci +0.0%. Pareto check: buying powergrid's quality with TRIALS instead costs the
baseline 2.97s (+860% CPU) to reach 4.737757 where this reaches 4.739335 at 0.34s — ~80× cheaper, so
the trade sits on the right side of the frontier. Per-trial overhead ≤1% (Infomap's own per-trial
timers summed over `-N10`, 6 reps: 22.635s base vs 22.820s against a 22.708s floor).

**Verified:** off switch (`COL_PARTSEED_Q=1`) → `.tree` byte-identical to baseline; determinism
byte-identical on re-run; `-2 -C` bit-identical on all 13; `-C -F` bit-identical on all 5 nets by
construction; full 13×5 refresh **62/65 codelengths bit-identical**, every OO and `-2` number
unchanged. Noise floors measured on known-bit-identical configs: ±2.1–2.4% webND `-N10`, −1.6%
science2001, −0.3% powergrid `-N100`, +16.7% politicalblogs (0.06s, timer-resolution bound, unusable).

**F22 interaction re-confirmed (item 5 of the brief):** air30k is bit-identical on every variant and
seed because its `-C` winner is a flat-first trial whose leaf layer is already skipped
(`m_bottomConverged`). Partial-seeding that converged flat bottom instead of skipping it was measured
and rejected: s234 −0.0329%, s345 −0.0113% for **+16%/+34%/+41% CPU** — worse than the fully-seeded
pass it would replace. On malaria the knob is inert for a different reason: its unchanged trials are
2-level stacks with no grandparent at all, which also explains why malaria's best-of-10 barely moves
while its trial mean drops 0.25%.

**Correction to F24.** The round-2 agent reported that F24's malaria −0.294% needed
`COL_HSPLIT_WINNER=all` rather than `auto`. That is wrong and F24 is correct as written: on the
round-1 binary `COL_HSPLIT=auto COL_HSPLIT_WINNER=1` reproduces **7.40046599** exactly, and
`COL_HSPLIT=auto` *without* the winner hook gives 7.445260297 — which is precisely the +0.31%
regression F24 already documents as the load-bearing winner-hook interaction. Re-verified directly.

### F26 — The knee revert: a measurement error, and what survives it (2026-08-11)

**F23's cost figure was wrong and the default has been reverted to 5e-3.** Daniel caught it by
arithmetic: web-NotreDame was 5.5728/21.4s at the #891 tip with OO at 171.6s; a later session had OO
at 158.3s, so the same code should land near 19.7s, against 24.8s measured — about +25%, far more
than the +8.7% F23 claimed for the knee.

Decomposed directly, interleaved, idle machine (load 3.3), web-NotreDame `-C -N10`, min-of-3:

| step | codelength | CPU | Δ cl | Δ CPU |
|---|--:|--:|--:|--:|
| knee 5e-3 (#891 tip) | 5.572794236 | 20.53s | — | — |
| knee 1e-3 (F23/#983) | 5.567411908 | 24.69s | −0.0966% | **+20.3%** |
| + partial seeding (F25) | 5.560674868 | 25.19s | −0.121% | **+2.0%** |

powergrid, min-of-4: 0.26s → 0.32s (knee, +23.1%, −0.054%) → 0.36s (partseed, +12.5%, −0.151%).

**Where F23's error came from.** Its A/B ran at load average ~20–25 with min-of-3. That inflated the
5e-3 arm's baseline (23.38s reported vs 20.53s idle) more than the 1e-3 arm (25.42s vs 24.69s),
compressing the delta from +20.3% to +8.7%. Min-of-3 under load was not enough separation, and the
two arms were not hit equally. **The lesson is not "measure more reps" — it is that an A/B under load
can be biased, not merely noisy, and the bias favoured the change.** The bit-identical-config control
used in F25 (ten configs the change cannot affect, whose walls must reproduce) would have caught it;
F23 had no such control because the knee touches everything it measures. Use a control arm that the
change provably cannot affect, or measure idle.

**What survives: the Pareto claim.** Re-tested at matched CPU on the idle machine — the old knee
genuinely cannot buy the deeper knee's quality with trials:

| config | codelength | CPU |
|---|--:|--:|
| 5e-3, `-N10` | 5.572794236 | 20.43s |
| 5e-3, `-N12` | 5.572723204 | 24.31s |
| 5e-3, `-N14` | 5.572723204 | 27.93s |
| **1e-3, `-N10`** | **5.567411908** | 24.35s |

At ~24.3s the old knee reaches 5.5727 where the new one reaches 5.5674, and `-N14` (+37% CPU) does
not move it at all — the trial distribution has saturated. So the deeper refinement is real, reachable
only by refining, and worth *offering*. It is not worth a fifth more CPU by default for a tenth of a
percent (Daniel's call), so it is a dial: `--tune-iteration-relative-threshold 1e-3` reproduces the
reverted default exactly (5.567411908) and `0` gives full convergence (5.566609295). The columnar path
honors an explicit value even when it equals the OO default, via the `parsedOptions` check from #825.

**Consequence for the OO comparison, stated plainly:** the "web-NotreDame beats OO" milestone was
mostly the knee's doing and is given back. At the shipped default webND is **+0.035%** vs OO (was
+0.121% before partial seeding, −0.096% with the deeper knee), and `-C` ties-or-beats OO on **8 of
13** rather than 9. Users who want the win ask for it with one flag.

**And partial seeding is BETTER at the restored knee** — the F25 cost question evaporates:

| network | at knee 1e-3 | at knee 5e-3 (shipped) |
|---|--:|--:|
| powergrid | −0.151% for **+7.4% CPU** | **−0.199% for +0.0%** |
| web-NotreDame | −0.121% for +1.2% (unresolvable) | −0.086% for +1.7% |
| netscicoauthor2010 | −0.056% for +0.0% | −0.056% for +0.0% |

powergrid gets a *larger* win at *no* cost, because a shallower refinement leaves more for a
well-targeted re-refine to find — the same mechanism as F22/F24 seen once more: **an operator's value
is a function of how converged its input is.** The full 13×5 refresh at the shipping configuration is
62/65 bit-identical to the pre-knee (#891) refresh, with only the three partial-seeding cells moving,
all improvements.

**Filed, not fixed:** `-hh` documents `--tune-iteration-relative-threshold` as "(Default: 1e-05)",
which is the OO default — the columnar default is 5e-3, so the help misleads `-C` users about what
they are changing from. Correcting it touches generated binding metadata (`make build-r-swig
build-python-swig build-binding-options build-js-metadata`), so it wants its own change.

### F27 — The split operator reaches the hierarchical path (PR, 2026-08-11)

Daniel: "the #890 split operator should be extended to the hierarchical path so you can use that in your
experiments." Shipped, but not in the shape the first round expected.

**The structural gap.** `moveLoop` always offers one empty module as a candidate
(`ColumnarMapEquation.cpp`, the `m_mMembers[cMod] > 1 && !m_emptyModules.empty()` branch), and
`seedAssignment` rebuilds that pool, so a *single* unit can always split off — including in seeded
loops. But a *group* never could: `mergeLeafModulesWithinParents` only coarsens, and
`refineLayerWithinGrandparent` re-derives a grandparent all-or-nothing, so one good split bundled with
several bad ones is rejected wholesale. `splitLevelModules(k, L, allowSingletons)` closes it — the
piece-level seeded move loop gives group-split AND cross-parent relocation in one operator.

**The finding that changed the design: the discriminator is TRIAL COMPETITIVENESS, not attempt volume.**
Round 1 (F24) reported the per-trial operator at malaria −0.294%/+1.6% but air30k +27% and reg +25% for
−0.02..−0.03%, and my diagnosis — "the discriminator is attempt volume, not level; malaria's win is at
k=0 too but costs 0.06s against air30k's 1.0s" — was **wrong**, refuted by per-trial codelengths:

| air30k trials | shape | off | on | Δ |
|---|---|--:|--:|--:|
| 1,3,5,7,9 | hierarchical | 5.4657–5.4732 | 5.4523–5.4563 | −0.20…−0.31% |
| 2,4,6,8,10 | flat-first | 5.3937–5.4033 | 5.3920–5.4025 | −0.006…−0.032% |

The operator accumulates **4.05%** of in-trial gain on air30k and delivers −0.033%, because ~85% lands
on hierarchical trials sitting 1.1–1.5% behind the flat-first trials, which can never win the
best-of-N. On malaria the same trials *do* win. Trial competitiveness is inherently cross-trial, so
**no within-trial rationing can separate the two networks** — level gating, piece-source gating,
per-level gain ratchets (malaria 0.114%/attempt vs air30k 0.068%, a 1.7× gap far too thin), attempt
caps (2.0 vs 3.7 per trial) and `m_bottomConverged` shape gating (separates air30k correctly and
malaria backwards) were each built, measured and dropped. Generalizable: **a repair operator that
raises mean trial quality can be worth nothing at best-of-N, and the cost/benefit split may live on an
axis no per-trial signal can see.**

**Shipped shape: drop the per-trial half, fix the once-per-run half.** Three components, each measured:
1. **Best-per-shape repair** — track the best *deep* trial separately and repair it when the overall
   winner is flat. Without it `COL_HSPLIT_WINNER` makes **0 attempts** on malaria (its winner is flat),
   which is precisely why round 1 believed the per-trial half was load-bearing.
2. **`winner` level policy** — leaf level only with a module-move correction, keeping the
   from-singletons piece source. webND operator 1.92s → 1.02s at identical codelength.
3. **Correction gate on the whole repair** — base networks otherwise buy −0.0005% (webND) / −0.0008%
   (science2001) for +7.0% / +0.3%, since the scaffolding (325k-node optimizer rebuild + 6-level seed +
   tree re-materialization) dwarfs the splits.

Plus one bit-exact fix: the gated lambdas in `coarsenModules` and `refineHierarchy` no longer snapshot
`m_hierLevels[0]` (~50 MB of leaf CSR per gated step on webND, twice per sweep). Two mechanisms were
built and removed rather than shipped: repair iteration (identical codelengths, 10–40% more attempts)
and a per-level productivity ratchet (inert — the existing dirty flag already prevents any level
reaching 3 attempts in one coarsening call).

**Results at the post-#985 baseline** (`-C -N10`, seeds 123/234/345, idle): malaria mean **−0.371%**
(−0.0688/−0.6134/−0.4305) at +5.1…11.1%; air30k −0.0085/−0.0007/−0.0021% at +1.9…2.9%; reg
−0.0020/−0.0223/−0.0305% at +1.5…3.1%; every base network bit-identical. Full 13×5: **59/65
bit-identical**, the six movers being those three networks under `-C` and `-C -F`.

**Why the win survived two intervening changes exactly** (the knee revert and partial seeding): malaria's
stack has a single interior layer, so `refineSweeps` is 1. The knee only bites with more than one
interior layer, and partial seeding only fires on a *re*-refine, which a single-sweep search never
reaches. Both commits are structurally inert on malaria, so neither the baseline nor the repaired value
could move — the numbers are digit-for-digit identical to the pre-rebase measurement, not merely close.

**Disjointness verified in both directions**, not assumed: with `COL_PARTSEED_Q=1` the split's three
networks are bit-identical (partial seeding is inert exactly where the split works), while partial
seeding stays live elsewhere (powergrid 4.749076 → 4.739600); and the split records **0 attempts** on
webND and science2001 against 11/5 on malaria.

**Merge conflict, resolved by composing rather than choosing.** Only `subClusterLeaves` genuinely
conflicted (`refineHierarchy` merged on offset): partial seeding had appended a seed pointer, the split
work had rewritten it as a wrapper over a new `subClusterUnits`. `subClusterUnits` took the seed
pointer as an extra parameter and `subClusterLeaves` became a one-line wrapper; all four call sites keep
their prior behaviour, confirmed by default-off being bit-identical on all seven networks.

**One structural note, documented rather than changed:** on a deep winner the hook constructs the repair
optimizer *before* the `hasModuleMoveCorrections()` bail, so base networks pay one
`setupColumnarOptimizer` they previously skipped (measured −0.4…0.0% on webND — net negative, because
the gated-lambda fix more than pays for it). Bailing earlier would mean duplicating the
correction-attachment predicate outside `addColumnarCorrections`, which would drift as corrections are
added; asking the constructed object is the robust form, and the cost is inside the noise floor.
