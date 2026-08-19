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

**F27 addendum — the bundled snapshot fix, isolated (2026-08-13).** Daniel's point at the master
sync: F27 bundled the split operator with dropping `m_hierLevels[0]` from the gated lambdas'
snapshot, so its reported Δ CPU is a *net* of one change that adds cost and one that removes it —
exactly what the project protocol says not to report. Isolated by reverting only the snapshot
avoidance on the current tip (web-NotreDame `-C -N10`, min-of-3 interleaved, codelength bit-identical
throughout): the snapshot fix is worth **−2.8% peak RSS and +0.3% CPU**, i.e. no measurable CPU
effect at all.

So the F27 numbers stand as the split operator's own cost — the bundling did not flatter it. What the
original wording overstated is the *magnitude*: "~50 MB of leaf CSR per gated step, twice per sweep"
is true per step, but those snapshots are allocated and freed inside the step, so they barely touch
the run's **peak**. Peak is what `maximum resident set size` reports, and a transient allocation only
raises it if it coincides with the high-water mark. Corollary: the fix does **not** meaningfully
pre-claim the leaf-CSR single-owner work (#960), which is worth a further −30.5% RSS on top of it —
the two are near-independent, and an earlier claim that F27 had eaten part of #960's win was wrong.

### F28 — Merging master in: separating "the seeds moved" from "the search changed" (2026-08-13)

122 commits of master landed under the branch. The sync itself was mechanical — the same handful of
conflicts either way, all of them places where master had generalized something the branch had done
locally — but it invalidated every number in the performance section, and the interesting part is how
to show that none of the movement is the engine.

**Why a merge rather than a rebase, decided after trying the rebase.** Thirteen sub-PRs have been
rebase-merged into this branch, so each one's recorded merge commit *is* a branch commit. The rebase
detached **13 of 13** — the PRs would still render on GitHub (`refs/pull/N/head` is retained) but they
would be marked merged into a history that no longer contains them. Daniel's second point was the
better one: the perf section is a *latest snapshot* attached to whichever sub-PR is being reviewed,
not a cumulative log, so a linear history that silently re-parents every old measurement makes those
numbers read as current. A merge commit is an explicit baseline boundary instead. Cost: one
non-linear commit on a branch that will be squashed or rebase-merged into master anyway. Worth
knowing: the merged tree is **byte-identical** to the rebased tree (verified by diff, and the built
binaries share an md5), so nothing measured had to be redone.

**The one that matters is [#949](https://github.com/mapequation/infomap/pull/949), "seed every trial
the same way, in every mode."** The branch's serial path reseeded only in sharding mode
(`trialOffset > 0` or a `--trial-results` path given); master now reseeds unconditionally from
`m_baseSeed + trialOffset + trialIndex`. That is *the same formula the columnar branch already used
in its parallel path* — master generalized it rather than contradicting it, which is why the conflict
resolution was a deletion (drop the `isShardingMode()` guard) plus one surviving columnar line
(`m_columnarFlatFirstTrial`). Consequence: trials 1..9 of an `-N10` run draw a different sequence
than before, so every best-of-10 codelength is a new draw. On both engines.

The temptation is to eyeball the table and argue. The two controls settle it without argument:

1. **`-C -N1` is bit-identical pre-sync vs post-sync on 8/8 networks.** Trial 0 has
   `trialIndex == 0` and `trialOffset == 0`, so `trialSeed` reduces to the base seed either way —
   #949 cannot touch it. If the search had changed, this is where it would show, and it doesn't.
2. **Plain master's OO `-N10` equals the synced branch's OO `-N10` on 7/7 networks.** The branch is
   exactly neutral on the OO path, so every OO shift in the table came with master and not with us.
   This is worth keeping as a standing regression check, not just a one-off: it is cheap and it
   catches any accidental leak from the columnar work into the default engine.

With those two, the remaining per-cell differences are re-draws. The seed sweep confirms the
distribution didn't move: old→new `-C` over seeds 123/234/345/456 averages +0.039% (netsci),
−0.039% (malaria), +0.059% (powergrid) — all inside ±0.06%, with per-seed spread larger than the
mean shift, and signs going both ways within every network. Seed 123's two >0.1% cells (netsci
`-C` +0.122%, malaria `-C` −0.265%) are opposite tails of the same spread.

**On speed, `-N1` is the honest instrument** — it is the only place the two binaries compute the
same partition, so a time difference is code and nothing else. Min-of-5 to min-of-7, interleaved:
malaria −2.0%, air30k −2.6%, regularized ±0, powergrid one timer tick, science2001 +2.0%,
web-NotreDame **+4.7%** (+6.7% at `-N10`). Only web-NotreDame needed explaining, and my first
explanation was wrong: I attributed it to master's new flow post-conditions (#958/#961/#963) on the
strength of a `--no-infomap` delta of 0.99s → 1.03s. That reasoning was sloppy — 0.04s cannot account
for 0.13s, and I should have noticed the arithmetic didn't close.

**What it actually is: #948's cluster-data tree-shape validation, run once per trial by the columnar
engine.** Found by elimination and then confirmed, in this order:

1. Timing registry: `flow_calculation_s` 0.315 → 0.313 and `init_network_s` 0.153 → 0.142 are flat;
   the entire delta lives in `trial_optimize_s` (1.943 → 2.073). So it is not ingest or flow.
2. Post-conditions gated off in a measurement build: **0.003s, +0.1%** of the trial. Not them.
3. #954's iterator change reverted: regression unchanged. Not that either.
4. `sample` on both binaries: `InfomapBase::initTree` is **192 samples in the new binary and absent
   from the old one's top 25**. That is the answer, and it is where I should have started.

`initTree` builds two `std::map<Path, bool>` keyed by `std::vector<unsigned int>` — one entry per
path prefix per leaf — to reject cluster data giving a module both a leaf and a sub-module as
children (#898, shipped as #948). Correct for user `--cluster-data`. But `columnarPartition` calls
`initTree` every trial to materialize `opt.toNodePaths(...)`, which is Infomap's *own* output and
cannot mix depths by construction, so on 325 729 leaves it is ~1.6M map insertions per trial for a
check that can never fire. Gating it off: web-NotreDame `-C` +7.5% → **+2.0%**, `-C -F` +11.2% →
+4.2%; science2001 and air30k are inside noise, so the cost scales with leaf count × depth exactly as
the shape predicts.

**Say precisely what this is, because "a cluster-data fix slowed the columnar engine" sounds wrong.**
#948 changed five files and **none of them is `Columnar*`** — it did not touch the columnar core. It
made the *shared* `InfomapBase::initTree` more expensive, and `columnarPartition` is the one caller
that invokes `initTree` **per trial** (to materialize `toNodePaths` into the output tree). That call
is pre-existing on this branch — `InfomapBase.cpp:2023` on the pre-sync tip — so the sync did not add
a call, it made an existing one cost more. On ordinary input OO reaches `initTree` only once per run
(cluster data, best-result restore, deep repair; `initTrialPartition` is per trial but gated on
embedded JSON initial-partition paths), which is why OO gets *faster* across the sync while `-C` gets
slower. So the earlier framing — "a cost the sync imports and the OO path pays too" — was wrong on
both the mechanism and on who pays.

**And it is not the only thing #948 added.** Asked directly whether `initTree` was the whole story, I
went back and gated all three of its reachable additions independently rather than answering from the
one I had already measured. It is not:

| addition | `-C` | `-C -F` | `-2 -C` |
|---|--:|--:|--:|
| `initTree` mixed-depth shape check | +6.9% | +8.2% | +0.6% |
| `removeSubModules` loop condition (`numLevels()` → full child scan) | −0.7% | +0.4% | +1.2% |
| `aggregatePerLevelCodelength` per-child classification | +0.3% | +2.1% | +1.8% |
| all three | +8.0% | +8.8% | +2.5% |

The shape check owns the hierarchical searches. **`-2 -C` never reaches it** — `initTree` routes the
two-level case out to `initPartition` before the check — so its +2.5% is entirely the other two, and
a fix aimed only at the shape check would leave `-2 -C` exactly where it is. Worth remembering as a
shape: *a validation PR spreads across several call paths, and the one you measure first is not
necessarily the one a given search hits.* Earlier phrasings ("it is the whole measurable cost", then
"about three quarters") were both guesses at an aggregate I had not decomposed.

The design point behind it: a validation written for *user-supplied* input is being applied to
*engine-generated* input on the hot path. The fix is to let `initTree` know which it was handed. Left
as a follow-up rather than folded into a master sync, and **nothing is disabled on the branch** — the
gate existed only in a throwaway measurement build.

**Method note.** Three hypotheses died before the right one, and each was killed by a build rather
than an argument — gate the suspect off, rebuild, measure. That is cheap (≈40s a round) and it is
strictly better than reasoning about which upstream commit "looks like" it should cost something. The
profiler should have been step 1, not step 4: a 6% regression with bit-identical output is a
*localised* cost, and `sample` localises it in one run.

The other upstream change with teeth is `7da08cfb`, which hoists per-node-constant plogp out of the
**OO** move-loop delta. OO got 5–13% faster, so every columnar-vs-OO multiplier in the tables is
quoted against a better baseline than the previous refresh. The multipliers dropped; the columnar
times did not rise to meet them.

**Method note for the next refresh.** Absolute times are not comparable across sessions, and after
#949 neither are absolute codelengths across a seeding change. What survives both is: same-session
ratios, `-N1` for code speed, and the master-OO-equality check for neutrality. Recording the old
binary alongside the new one (build the pre-rebase tip into a worktree, keep both, interleave) is
what made all of the above cheap — worth doing by default before any master sync that crosses a
seeding or RNG change.

**F28 addendum — three wrong explanations, and what they have in common (2026-08-13).**

Daniel pushed back on three claims in the first draft of this entry. All three were wrong, and they
failed the same way, so the pattern is worth more than the individual corrections.

1. *"The web-NotreDame slowdown is master's new flow post-conditions."* It is not. Gated off in a
   measurement build they cost **0.003s, +0.1%**. The real cause is #948's cluster-data shape
   validation running per trial inside `initTree` (see above).
2. *"#960's CPU win shrank from −9.9% to −1.7% because the memcpys are a smaller share of a run that
   does more work per trial."* Nothing shrank. Rebuilt #960's **original** base (`9aa7fea9`) and
   original tip and measured them in the same session: **−3.6%**, and −3.6% against the current core
   too. The −9.9% was simply over-stated at the time. There was no change to explain.
3. *"The RSS difference is F27 pre-claiming part of #960's target."* Isolated by reverting only the
   snapshot avoidance: it is worth **−2.8% peak RSS and +0.3% CPU**. It cannot account for a ~5pp
   difference, and web-NotreDame's #960 RSS delta reads −24.7% / −27.1% / −30.5% across three
   sessions, so the "difference" was inside session spread all along.

**The common failure: I explained a difference before establishing that the difference was real.**
Each time, two numbers from two sessions disagreed, and each time I reached for a mechanism instead
of first re-measuring both arms in one session. The mechanisms were all plausible — that is what made
them dangerous, because a plausible story makes the number look explained and stops the enquiry.

The discipline that would have caught all three, in order:

- **Re-measure the original pair before theorising about why an effect changed.** Building an old
  base and an old tip costs ~40s each. It distinguishes "the effect moved" from "the number was
  wrong", and here it was the number every time.
- **Check the arithmetic closes.** 0.04s of `--no-infomap` delta cannot explain 0.13s. I had that
  number in front of me and did not subtract.
- **Profile before attributing.** A regression with bit-identical output is a *localised* cost;
  `sample` found `initTree` in one run, after three hypotheses had already died.
- **Kill hypotheses with builds, not arguments.** Gate the suspect off, rebuild, measure — cheap, and
  it produces a number instead of a story.

**F28 addendum 2 — the master sync turned on tests that had never run (2026-08-13).**

CI went red on `infomap_cpp_lifecycle_tests_columnar` after the merge, while a local
`make test-native OPENMP=0` was green. The cause is master's
[#947](https://github.com/mapequation/infomap/pull/947), *"compile the C++ test targets with the
OpenMP flags they branch on"*: before it, `_OPENMP` was undefined in the test translation unit, so
**every `#ifdef _OPENMP` test body compiled away to nothing.** The parallel-trials contracts live
inside those guards, and they are tagged `[columnar-contract]`, so they had been silently no-ops for
the columnar engine since the tag was added.

The bug they immediately found is **real, pre-existing, and not caused by the sync** — verified by
running the pre-merge and post-merge binaries side by side, which give identical numbers:

| `--columnar --entropy-corrected`, ninetriangles, seed 7 | pre-merge | post-merge |
|---|--:|--:|
| `-N4 --parallel-trials`, per-trial | 4.918622 ×4 | 4.918622 ×4 |
| `-N1` serial, seeds 7/8/9/10 | 3.635831 | 3.635831 |

OO is self-consistent on the same fixture (3.742114 for both paths), so this is the columnar engine's
parallel-trials path, not the harness. Note the serial columnar result *beats* OO — the search is
fine; it is the parallel worker path that loses something. A second, smaller failure in the same
suite is a sanity check tripping on round-off: `getIndexCodelength() >= 0.0` at `-5.9952e-15`.

Two lessons, and the first is the expensive one:

1. **A green test run proves nothing about code paths the build compiled out.** `OPENMP=0` is the
   *benchmark* configuration; using it as the *test* configuration silently skipped every parallel
   contract. Run both, and treat "this suite has `#ifdef` guards" as a reason to check what the build
   actually enabled.
2. **A test that has never failed is not evidence it has ever run.** These tests were tagged for the
   columnar engine, appeared in the ctest list, and reported as passing — three signals that all
   looked like coverage and were not.

### F29 — The leaf CSR existed four times over; now once (2026-07-29)

Prompted by a memory question, not a profile: *how much does a 1.5B-link network cost in the
columnar core vs OO?* Counting bytes off `sizeof` rather than guessing turned up an embarrassment.
Per stored link the columnar representation is exactly **24 B** (out target+flow, in target+flow,
`assign()`-sized, no slack) against OO's **48 B** (`InfoEdge` 32 + an out-vector slot + an in-vector
slot) — a clean 2×. Except a trial did not hold *one* leaf level. It held four:

1. `InfomapBase`'s native columnar leaf input (the staging arrays, kept across trials),
2. a local `Level leaf` that `setupColumnarOptimizer` copied them into,
3. `m_leaf0`, which `buildFromLevel` copied *that* into,
4. `m_hierLevels[0]`, a third copy pushed by every stack build — plus `m_lvl = m_leaf0` whenever the
   active level was the leaves (so `retuneLeavesWithinModules` made it four live at once), plus a
   further copy inside each `savedLevels` / `fineLevels` / `flatLevels` save-restore of the stack,
   and one more in `trajLevels[0]` on the trajectory-repair path.

So the representation was 2× leaner per link than OO and the implementation spent the win 4× over.
Nothing ever wrote into a level after it was built — verified: no `m_lvl.<field> =` or
`m_leaf0.<field> =` anywhere, aggregation always produces a *new* level — so every one of those
copies was of an immutable object.

**Fix: one owner, everyone else points at it.**
- `ColumnarLevel` moved to namespace scope (`ColumnarLevel.h`, aliased as `ColumnarTwoLevel::Level`,
  so no call site changed) so `InfomapBase` can hold one by value without including the core header.
- `buildFromBorrowedLevel` lets a trial read the caller-owned leaf level in place; the staging arrays
  became that single owner. Copies 1–3 collapse to one.
- `leaf0()` / `lvl()` are pointer accessors: the active level *aliases* the leaf network while the
  units are leaves and only owns storage for aggregated levels.
- `hierLevel(k)` routes level 0 of the stack to `leaf0()`, and slot 0 became an empty placeholder —
  which also makes every stack save/restore copy free of the leaf CSR. Same for `trajLevels`.
- `buildFromLevel` now takes its `Level` by value, so the four internal callers with dead locals
  (`superNet`, three `sub`) hand over storage by `std::move` instead of copying.

**Result — codelength bit-identical, memory down, and faster.** All **65** configs (13 networks ×
{`-C`, `-C -F`, `-2 -C`, OO, OO `-2`}) reproduce their codelength *and* their top-module/level counts
exactly. Peak RSS, same-session alternating runs, excluding the 8.5 MB process floor:

| variant | networks | median Δ peak RSS | range |
|---|--:|--:|--:|
| `-C` | 8 | **−33.7%** | −54.4% .. −18.4% |
| `-C -F` | 8 | **−28.6%** | −44.8% .. −22.0% |
| `-2 -C` | 8 | **−23.9%** | −43.0% .. −9.6% |
| OO | 7 | +2.3% | −1.9% .. +7.2% |
| OO `-2` | 7 | +2.2% | −1.4% .. +5.1% |

The OO rows are the control: that path is untouched, so its ±2% is the noise floor of the
instrument. Speed, interleaved min-of-4 CPU seconds (`-C -N10`): web-NotreDame **−9.9%**, malaria
−5.6%, powergrid −3.7%, science2001 −2.1%, air30k −0.5% — three leaf-CSR `memcpy`s per trial were
real work. Beware single wall-clock runs here: the same web-NotreDame `-C` config that measures
−12.9% wall interleaved showed **+49%** in one unpaired run.

> **Correction (2026-08-13, at the master sync).** The −9.9% above does not reproduce, and the fault is in
> that measurement rather than in anything the master sync changed. Re-run in one session, this PR against
> **its own original base `9aa7fea9`** gives **−3.6%** on web-NotreDame `-C -N10`; against the synced
> core it gives **−3.6%** as well, and a later multi-network batch put it at −2.1%. So the CPU effect
> is ~1–3.6% depending on session, identical on both bases. My first explanation for the discrepancy —
> that the memcpys are "a smaller share of a run that does more work per trial" — was wrong twice over:
> the run does not do more work (codelength and time are essentially unchanged), and the win did not
> shrink, so there was nothing to explain. **When a headline number fails to reproduce, re-measure the
> original pair before theorising about why the effect changed** — the cheap experiment (build the old
> base and the old tip, interleave) distinguishes "the effect moved" from "the number was wrong", and
> here it was the number.
>
> A second explanation was drafted for the peak-RSS side — that F27 had pre-claimed part of the win by
> dropping the `m_hierLevels[0]` snapshot from the gated lambdas — and it is wrong too. Isolated by
> reverting only that change: **−2.8% RSS, +0.3% CPU**; those snapshots are transient, so they barely
> touch peak. web-NotreDame's RSS delta for this PR reads −24.7% / −27.1% / −30.5% across three
> sessions, so the apparent movement was session spread. **Two mechanisms invented for one
> non-difference** — see the F28 addendum.

**F29 addendum — the merge onto the current core (2026-08-13).** Bringing this PR onto the synced
core surfaced a bug that no conflict marker pointed at, and it is the useful part of the story.
`subClusterUnits` and `refineLayerWithinGrandparent` both build a local `sub` level and hand it to
`buildFromLevel`. This PR changed that signature to take `Level` **by value** so the four callers
with dead locals could hand over storage by move. Independently, F25 added a
`buildPartialSeed(sub, ...)` call *after* that handover. Git merged the two cleanly — the lines do
not overlap — and produced a use-after-move that segfaulted reproducibly on exactly the two networks
where partial seeding is live (netsci and powergrid `-C`), and on no others.

Worth remembering as a shape, not just an incident: **a by-value signature change and a new read of
the same object are individually innocent and jointly fatal, and they are exactly the kind of pair a
three-way merge cannot see.** The fix is ordering — compute the partial seed before the handover, in
both call sites — and it is bit-identical, so the only cost of getting it wrong was the crash.

Two other things the newer core needed that this PR could not have known about: `splitLevelModules`
(F27) reads its level through `m_hierLevels[k]` at `k == 0`, which is now the empty placeholder slot,
so those reads had to move to `hierLevel()`/`leaf0()`/`lvl()` — 13 sites, and silent corruption
rather than a compile error if missed, since slot 0 is a valid empty `Level`. The lesson for anyone
syncing this branch again: after any conflict resolution, grep for direct `m_hierLevels[` **reads**
with a non-constant index and for `m_leaf0`/`m_lvl`, because the accessor indirection is exactly what
a merge will not reintroduce into code written after it.

And a process note on how it nearly went missing: this text was dropped once while re-splicing the
document, by a replace that ran from an anchor "to end of file" and swallowed the section after it.
Anchor edits to *both* ends of the range when the tail is not what you are replacing.

### F30 — Second master sync, done as a PR this time (2026-08-13)

One upstream commit, #991, which moves #948's cluster-data shape validation out of `initTree` to the
input boundary. Measured on the merged binary against the pre-sync branch, same session, min-of-3,
`-N10`, **codelength bit-identical on all 39 columnar configs**: web-NotreDame `-C` **−5.6%** and
`-C -F` **−7.2%** whole-run CPU, everything else at or inside noise. That matches the shape predicted
when the cost was first isolated — it scales with leaf count × depth — and it is smaller than the
`trial_optimize_s` figure (−6.3%/−6.5%) because the whole run includes ingest and flow, which the
change does not touch.

**Process change, and the reason for it.** The first sync (F28) was merged straight onto the branch
and pushed, and it pushed the branch **red**: three master gates this branch had never satisfied
(#947 OpenMP test flags, #927 config fingerprint, #941 R man pages) only surfaced afterwards. This one
went through a `sync-master-into-columnar` branch and a PR, so CI ran on the merged result *before* it
landed, and all three configurations were verified locally first. It also gives the sync a discoverable
snapshot of its own, which is what the perf-section convention is for — the previous sync's evidence
lives only in a commit message.

Worth keeping as the rule: **a master sync is a change to this branch like any other, so it gets a
branch, a PR, and a perf snapshot.** The cost is one extra PR; the thing it buys is not landing a red
branch.

### F31 — A parallel-trial worker is not a copy of the main instance (#989, 2026-08-13)

`--columnar --parallel-trials --entropy-corrected` returned the **one-level codelength on every
trial** — 4.918622 ×4 on ninetriangles seed 7, against 3.635831 from the same run done serially, with
all 27 nodes in a single module. The second symptom was a `checkRunSanity` trip on
`getIndexCodelength() >= 0` at −5.9952e-15: one module means an index codelength of zero up to
round-off.

**Cause.** `runTrialsInParallel` builds each worker as a fresh `InfomapBase` and hands it the run's
network as an *argument*: `worker.initNetwork(m_network)`. `InfomapBase::m_network` is a value member,
so the worker keeps its own default-constructed, **empty** one — the run's network is never stored
there. Anything the search reads back off `m_network` is therefore zero inside a worker.
`addColumnarCorrections` did exactly that for the entropy-bias divisor:

```cpp
double totalDegree = m_network.sumWeightedDegree();      // 0 in a worker
if (totalDegree < m_network.sumDegree()) …               // 0
opt.addCorrection(std::make_unique<BiasedEntropyCorrection>(mult, totalDegree));
```

and `BiasedEntropyCorrection`'s constructor guards `totalDegree > 0.0 ? totalDegree : 1.0`. So the
divisor was **1** instead of ninetriangles' 108 — the correction came out two orders of magnitude too
strong, every extra node in the tree was priced at ~0.5 bits, and the cheapest tree won: one module.
The guard is what turned a division by zero into a plausible-looking wrong answer; without it the run
would have produced `inf` and been noticed years earlier.

The object-oriented engine never had this because `initNetwork(Network& network)` pushes the same
figure into the objective from the **argument** — `m_optimizer->setNetworkProperties(network)`. The
columnar correction is built later and from the wrong source. Fix: derive the divisor in
`initNetwork(Network&)` alongside that call, from the network passed in, and have
`addColumnarCorrections` read the stored value. The rule for computing it now has one definition
(`BiasedMapEquation::entropyBiasTotalDegree`) instead of two copies that could drift.

**Why it had never been seen.** Not a regression: pre- and post-merge binaries agree. Master's #947
added the OpenMP compile flags to the C++ test targets; before it, `_OPENMP` was undefined in the test
translation unit and every `#ifdef _OPENMP` body — which is where the parallel-trials contracts live —
compiled to nothing. The cases were in the ctest list and reported as passing.

**Two things this says about the test suite, both worth more than the bug.**

*Worker-count invariance cannot catch a worker bug.* `Parallel trials with entropy correction are
invariant to worker count` is tagged `[columnar-contract]` and passed throughout, because both arms —
1 worker and 4 — run through `runTrialsInParallel`. A fact missing from every worker is missing from
both. Only a **parallel vs serial** comparison crosses the boundary where the fact goes missing.

*The parallel-vs-serial cases were asserting something false by construction.* They compared parallel
trial *i* against a serial `-N1` run with seed 7+*i*. A trial's result is a function of its seed **and
its global trial index** — `m_columnarFlatFirstTrial = (trialOffset + trialIndex) % 2 == 1` alternates
the hierarchy-build strategy — and a `-N1` run is always trial 0. Where the strategy changes the
answer the equality simply does not hold: on ninetriangles `--markov-time 1.5` the parallel vector is
[4.068, 3.771, 4.068, 3.771] while every `-N1` run returns 4.068. The existing cases passed on `-N1`
only because their own fixtures score the same either way — a green that was luck, and a red waiting
for the next change to the flat-first strategy. They now compare against a serial `--num-trials 4` run
element-wise, which is the invariant that is actually true: *`--parallel-trials` is a scheduling
choice and nothing else*.

**Coverage swept while here** (parallel `-N4` vs serial `-N4`, element-wise, `--columnar`): default,
`-2`, `-F`, `--entropy-corrected`, `--markov-time 1.5`, `--variable-markov-time`, `-d`,
`-d --recorded-teleportation`, `--preferred-number-of-modules`, states, multilayer, multilayer `-d`,
bipartite, weighted directed, `--meta-data` (first-order and states), `--cluster-data`,
`--no-infomap --cluster-data`. All agree. So the entropy divisor was the only run-scope fact the
columnar search was reading off `m_network`.

**Still open, deferred (#994).** The other half of the same asymmetry: workers never get the native
columnar leaf input either, because `buildColumnarLeafInput` runs in `RunSession` and the worker
bypasses it, so `m_columnarNativeInput` is false and every trial rebuilds from the InfoNode leaf tree
via `buildFromLeaves`. Measured at one worker (`OMP_NUM_THREADS=1`, min of 3, so parallelism is not a
factor), the worker path costs **+13.3%** (science2001 `-d`), **+20.2%** (web-NotreDame `-d`) and
**+7.5%** (air30k) CPU for the same four trials. Not attributed to one cause: workers also call the
full `initNetwork` per trial, regenerating the whole InfoNode leaf tree, where the serial path only
calls `removeModules()`. Isolating the two needs a build, which is why it is a separate issue and not
an extra paragraph here.

**The general lesson, which outlives this bug.** *A parallel-trial worker is not a copy of the main
instance.* It gets the config, the cancel flag, and one `initNetwork` call. Any run-scope fact the
search needs must arrive through one of those three, and the compiler will not tell you when one
doesn't — `m_network` is a perfectly valid empty object. Before adding a read of `m_network` (or of
anything else built in `RunSession`) to code the trial loop reaches, check what it returns in a
worker. Two more such reads exist today at `InfomapBase.cpp`'s `regularizedPriorOnly` lines
(`network().numLinks() == 0`, which is true in *any* worker, so `regularizedPriorOnly` degenerates to
`regularized` there). They only widen the gate on the one-level collapse rather than change what is
computed, and `-d --regularized` on the states fixture gives the same four trials in parallel as in
serial on both engines — but they are the same shape, and they sit on the shared OO path, not the
columnar one.

### F32 — Workers borrow the leaf SoA; the rest of the worker gap is not columnar's (#994, 2026-08-13)

The other half of F31's asymmetry. `buildColumnarLeafInput` runs in `RunSession`, on the main
instance only; `runTrialsInParallel` builds each worker and calls `worker.initNetwork(m_network)`
directly, bypassing that wrapper. So `m_columnarNativeInput` stayed false in every worker and
`setupColumnarOptimizer` took the `buildFromLeaves(m_leafNodes, …)` branch — rebuilding the columnar
structure from the InfoNode leaf tree on **every trial**, while the serial path borrows a leaf SoA
built once.

A worker cannot build its own: the CLI releases the links before the trials run
(`releaseInputLinksIfCli`), which is exactly why the main instance builds it early. So it borrows the
main instance's — sound for the same reason each trial's optimizer already borrows it (immutable for
the whole run, owner outlives every borrower), and it puts workers on the *same input construction*
as the serial path instead of a parallel one.

**The two constructions were numerically identical**, checked before and after on science2001,
powergrid, air30k and malaria: all four trials bit-identical, same top modules and levels. So this is
a pure cost removal plus the removal of a divergence class that was only ever a measured fact, never
a structural one.

**Effect.** One worker (`OMP_NUM_THREADS=1`, `-N4`, min-of-3, so parallelism is not a factor), every
(build, engine, mode) cell measured once per rep and cycled so drift hits all cells alike. Columnar
parallel-arm CPU, before the borrow → after:

| network | before | after | change | gap vs serial, before → after |
|---|--:|--:|--:|--:|
| web-NotreDame `-d` (325 729 leaves) | 10.23s | 9.68s | **−5.4%** | +17.6% → +15.2% |
| science2001 `-d` (7 170) | 1.61s | 1.55s | **−3.7%** | +12.9% → +6.8% |
| air30k (13 213 states) | 1.78s | 1.79s | +0.6% (not resolvable) | +3.6% → +7.4% |

The instrument's noise floor is set by the OO arms, whose code path this change does not touch at all:
byte-identical OO code measured 0.1–2.1% apart between the two builds, and its *gap* percentages —
differences of two noisy numbers — moved by up to 2 points (air30k OO +4.5% → +6.4%). So the two large
networks are real and **air30k is not resolvable**, which is the expected shape: the removed work is a
per-trial rebuild of the columnar structure from the leaf tree, so it scales with the network, and
air30k has 13k leaves against web-NotreDame's 326k. Do not read air30k's "+0.6%" or its gap moving the
wrong way as a regression; read it as below the floor.

**The residual gap is not columnar's, and the percentages say otherwise only because of arithmetic.**
Taken at face value the residual looks damning — on web-NotreDame the columnar worker path is +15.2%
against serial while OO's is +6.4%. But the worker overhead is an **absolute** per-trial cost, not a
proportional one, and in absolute terms **OO pays more**:

| network | OO extra, per trial | columnar extra, per trial |
|---|--:|--:|
| web-NotreDame `-d` | 0.58–0.91 s | 0.32–0.38 s |
| science2001 `-d` | 0.032–0.045 s | 0.024–0.046 s |
| air30k | 0.054–0.076 s | 0.016–0.031 s |

A columnar trial on web-NotreDame takes ~2.2s against OO's ~14s, so the same fixed overhead is ~6× the
fraction. That is the whole of the percentage difference, and the arithmetic closes. **This was very
nearly recorded backwards**: measured first with all three reps of one cell before moving to the next,
the OO arms came out 5–7% apart on byte-identical code, and on that data the residual read as
columnar-specific on web-NotreDame (+15.6% vs OO's +1.8%) and as engine-independent on the other two.
Ranked percentages of a fast operation are the wrong instrument; seconds per trial is the right one.

**What the residual actually is — not attributed to a number.** A profile diff of the parallel and
serial arms shows no single dominant cost: the parallel arm spends more in `sortChildrenOnFlow`,
`aggregatePerLevelCodelength`, the full `NodePaths` walk, malloc, and `generateSubNetwork`/`InfoNode`
construction, because the worker loop does per **every** trial what the serial path either does once
(`initNetwork`, against `removeModules()` between trials) or defers to the winning trial (sort,
per-level statistics, full tree collection). Each is under 1.5% of the run and the percentages come
from two different sample denominators, so no attribution is claimed. It is shared-path work — it
belongs on master by the rule that a fix outside `Columnar*` does — and it is filed as #996.

**A guard became load-bearing that was not before.** `setupColumnarOptimizer` gates the native path on
`m_columnarNativeInput && !haveHardPartition()` — hard cluster-data needs the InfoNode leaf tree for
`restoreHardPartition`. That second condition never mattered on the worker path, because a worker's
`m_columnarNativeInput` was unconditionally false; borrowing makes it true for the first time. Removing
the guard now **segfaults** rather than returning a wrong number, and the new `Parallel trials with
hard cluster-data match serial trials` case was verified to catch it. Worth generalising: *making a
disabled path reachable makes every guard on that path newly load-bearing* — enumerate them and test
each, rather than only testing the path you meant to enable.

**Not directly test-pinned, and why.** There is no assertion that a worker actually took the borrowed
path: `runTrialsInParallel` wraps each trial in `Log::ScopedMute`, so the detail line naming the input
path never reaches a `LogCapture`, and the only alternative would be an accessor that exists for the
test alone. The observable contract (parallel == serial) is tested and the cost is measured; a silent
regression here would cost speed, not correctness.

### F33 — #831 was never `restoreBestResult`'s fault, and its cost was never real (#998/#1000, 2026-08-14)

Two separate lessons, one from each half of the fix.

**The root cause was one level up from where the issue said it was.** #831 recorded the negative
`# codelength` as `restoreBestResult` re-`initTree`-ing without the `columnarL` override the other two
materialization sites apply, and the remedy as "fix the reentrancy, not by adding a third override".
That located the *carrier* and not the fault. The objective's network-level terms —
`MapEquation::nodeFlow_log_nodeFlow`, and each derived objective's equivalent — describe **whichever
network was active when they were last initialised**, which is root's children and only sometimes the
leaves. `initTree`'s multi-level branch calls `initNetwork()` *after* building the module tree
(`InfomapBase.cpp:1879`), so it leaves them describing the **modules**; `initSuperNetwork` does the
same by design. Nothing restored them, so `initPartition(vector<unsigned int>&)` scored the leaf
partition against a module network's terms.

The arithmetic is what identified it, and it closes exactly: the error is **one whole one-level
codelength**. politicalblogs' leaf term is −7.598 and two top modules give
`2·plogp(0.5) = −1.0`, so 6.739 − 7.598 = −0.859, which is the number in the file. On the twotriangles
fixture the same subtraction is 2.320730 − 1.556657 = 0.764074 against a one-level codelength of
2.556657. **A difference that equals a known quantity of the model is a much stronger signal than a
plausible mechanism** — it named the cause before any build was made.

Three consequences worth keeping:

1. **The bug was not columnar's.** The repro needs only the public API — `initPartition(<3-level
   tree>)` then `initPartition(<2-level clu>)` on one instance — and fails on plain master. The
   columnar engine merely *reached* it, by materializing through `initTree` every trial. So it landed
   on master (#998) and came back by sync (#1000), the rule now written at the top of CLAUDE.md.
2. **The seed-123 repro had gone stale, and that made it look fixed.** #987/#988's winner repair sets
   `bestTreeMaterialized`, which makes `restoreBestResult` early-return, so the corrupt path now runs
   only when the repair does *not* improve. At the default seed politicalblogs was clean; it was
   corrupt on 4 of 7 other seeds. A checklist repro that stops firing is not evidence of a fix.
3. **It was corrupting the benchmark set in plain sight.** science2001
   `--preferred-number-of-modules 25` wrote `-1.39324` (`-C`) and `-1.40803` (`-C -F`) at seed 123
   while the console read 8.23558553. Nobody had diffed the console against the file.

**The false positive: disjoint distributions that meant nothing.** The naive fix — re-init
unconditionally — measured a real +2.5% on web-NotreDame `-2 -C`, so it was guarded on a flag
recording which network the terms describe. In the sync sweep that same config then read **+3.4%**,
and a 5-rep confirmation gave ranges that did **not overlap** (18.67–19.07 pre-merge vs 19.51–20.00
merged). Every instinct said the guard was leaking, in exactly the config and roughly the size the
mechanism predicted.

It was noise. An instrumented build counted the guarded call site: **`reinit=0 skip=11` across
`-N10`** — the re-init never executes on that path at all, because a `-2 -C` trial always leaves the
terms describing the leaf network. A third arm with the member kept and the call deleted came in at
18.41s against the merged binary's 18.43s, and a re-run in a quieter window put pre-merge at 18.57s
against merged 18.43s, overlapping.

**Why it fooled the usual defences:** min-of-N and interleaved arms both assume the disturbance is
*symmetric* across the interleave. A neighbouring agent's build running for a few minutes is not — it
lands on whichever arm occupies that window, and min-of-5 preserves the damage instead of averaging it
out. **On a shared machine, non-overlap across arms measured minutes apart is still session structure.**
The instrument that settled it was not more timing but a counter: when a suspected cost has a specific
code path, count its executions first. Zero executions ends the question in one run; timing never does.

**Also learned, and now in CLAUDE.md:** `make build-native` does not track header dependencies (#999).
Since this fix adds a member to `InfomapBase.h`, the incremental build recompiled only
`InfomapBase.cpp` and left every other TU on the old object layout — a binary that printed correct
codelengths while writing `# codelength 2.62515e-313` to every tree file. It was caught only because
the binaries were md5'd against a clean rebuild. `make test-native` (CMake) tracks headers correctly,
so the C++ suites stayed green while the native binary was incoherent — the two disagreeing is the
worst version of this.

### F34 — L\* is not a cheaper L, and its search wins on its own objective (#1001, 2026-08-14)

Benchmarked `--non-redundant` (columnar L\*) against `-C` (base L) on the 8 L\*-eligible configs, both
arms interleaved in one session, min-of-3, and cross-scored each arm's partition under **both**
objectives with `-C --no-infomap -c <tree>`. Numbers in the PR's perf snapshot; three findings here.

**1. L\* ≤ L pointwise is false.** The intuition that removing redundancy can only lower the codelength
does not survive the enter codebook: for the *same* partition L\* is lower on ninetriangles (−9.1%),
netsci (−2.3%), powergrid (−3.8%) and web-NotreDame (−0.9%), and **higher** on jazz (+0.08%),
politicalblogs (+0.76%), science2001 (+2.3%) and pref-mods (+2.6%). The separate enter codebook is extra
module-codebook structure, and on partitions with many small modules it costs more than leave-one-out
saves. Consequence for the record: **no table may compare an L arm's codelength to an L\* arm's.** The
first draft of this comparison read "L\* is worse on science2001 (8.009 vs 7.833)", which is meaningless
— they are different objectives, and cross-scoring is the only way to ask a well-posed question.

**2. The L\*-aware structural search earns its place.** L\*(P<sub>L\*</sub>) < L\*(P<sub>L</sub>) on all 5
configs where the two arms find different partitions (netsci −1.73%, powergrid −1.11%, politicalblogs
−0.041%, science2001 −0.035%, jazz −0.0006%), and ties on the 3 where they coincide. It never loses. So
Phase 1's design — base-L leaf move loop, L\*-aware gating for every structural operator — beats
"search with L, rescore with L\*" without ever costing more than 1% wall. This is the measurement the
"why the leaf move loop is not L\*-aware" argument needed to stand on: the L\*-aware *structure* search
is the part that pays, and it is the part that is cheap.

**3. On the two largest-K configs L\*-gating changes nothing.** web-NotreDame `-d` and science2001
pref-mods produce the *identical* partition in both arms (same codelengths to all digits, same 5/6 and
25/2 shapes). Not a bug — the accept/revert decisions land the same way — but it bounds the claim: L\*
reshapes the map on mid-size networks (powergrid 5 top/5 levels → 3/7, politicalblogs 81 top → 2) and
is currently inert on the largest one in the set.

**Also found, both worth fixing in #1001:**

- **The memory/multilayer rejection does not fire on auto-detected input.** `Config.cpp:212` tests
  `config.stateInput || config.multilayerInput`, which only reflect explicit input-format flags —
  validation runs *before* the network is read, so a file whose content declares `*States` /
  `*Multilayer` sails through: `air30k.net --non-redundant` runs as `Type: higher-order state` and
  reports 5.547319829, malaria as `higher-order multilayer` and reports 7.522734393. Both are L\* plus
  the physical-codebook correction — the combination the PR says is rejected and never validated. The
  guard has to run after the network type is known (or read the sniffed type).
- **`--non-redundant-exact` is inert**, and verified so (byte-identical tree bodies). It is user-visible
  CLI surface, and it is in the config fingerprint, so today it produces two fingerprints for runs that
  are bit-identical.

**Not a #1001 finding, but surfaced by it:** the console `Levels` table prints **base-L** per-level bits
under `-C` (ninetriangles: table total 3.385831 against a reported L\* of 3.078067) — that much is the
PR's stated known follow-up. But on jazz the same table is **all zeros under plain `-C` too**, with no
L\* involved, so that one is a pre-existing columnar reporting gap and needs its own issue.

#### F34 addendum — the higher-order rejection was the wrong fix, and the tree round trip lied

Two corrections to F34, both worth keeping as they were made.

**1. "Fix the guard" had the sign backwards.** F34 filed the state/multilayer escape as a leak to be
plugged. That is wrong: **L\* only constrains impossible walks** — no immediate re-entry into the module
just left, no immediate exit from the one just entered — which is orthogonal to *which codebook* a step
is coded in. It therefore does not limit support for higher-order dynamics, and the physical/state
codebook correction should compose with it. The defect is that the rejection exists at all; and the
inconsistency F34 did spot (explicitly-flagged input refused, sniffed input accepted, same network) is
evidence *for* removing it rather than for tightening it. Measured on the four higher-order configs
(perf snapshot): L\* runs, costs nothing (−0.4% to −7%), and changes nothing — identical partitions on
multilayer-ex and air30k, ties within 2e-5 on malaria and regularized air30k with the sign going both
ways. So the relaxation is about correctness of scope, not quality. Metadata is still rejected and stays
a separate decision: the same walk argument applies, but L\* × `MetaCorrection` is unvalidated.

**Generalisable:** "the guard doesn't fire" is a report about mechanism. Whether the guard *should* fire
is a modelling question, and the objective's definition answers it — not the code. F34 jumped from the
first to the second.

**2. The physical `.tree` round trip is lossy for state networks, and cross-scoring through it produced a
number that looked like a finding.** Scoring air30k's partition with `-C --no-infomap -c <tree>` returned
L = 9.765607443 against a search-reported 5.392425413. The tempting reading — "the memory correction is
not applied on the `--no-infomap` path" — was wrong. Infomap had already said what happened: *"182
physical nodes have their states split across modules in this tree. A physical tree cannot express which
state belongs to which module … the partition read back is likely not the one that was written."* It
scored a **different partition**, faithfully. Through `_states.tree` every re-scored value reproduces the
search value to all printed digits.

This is the same trap as F33's, one layer out: a plausible mechanism (correction missing on the eval
path) was available for a number whose real cause was that the *input* was not what it claimed. The check
that settles it costs one run — re-score the partition the search itself reported and require the value to
come back **identical** before trusting any other cell of the table. Any cross-scoring 2×2 should have
that identity as its first assertion, on every network, not just the ones where it is convenient.

### F35 — L\* composes with every correction, and the guard that "protected" it was dead code (#1001, 2026-08-14)

Daniel's ruling (F34 addendum) generalises: L\* constrains which walk **steps** are possible, which is
orthogonal to *which codebook* a step is coded in, so **no objective and no input is out of scope**. The
four rejections `--non-redundant` carried — memory/multilayer input, meta data, `--entropy-corrected`,
`--lossy` — are removed rather than tightened, and all 13 benchmark configs now run under L\*.

**The memory/multilayer rejection could never fire from the CLI at all**, which the F34 addendum
under-stated (it said validation "only sees an explicit `--input-format`" — there is no such option).
`config.stateInput` / `config.multilayerInput` are set by `configureNetworkMode()` when the network is
*read*, which happens after config validation, and **no option sets them**. So air30k and malaria were
never actually blocked; the four higher-order rows in the perf snapshot are not new capability, they are
capability the guard only appeared to withhold. Meta data, `--entropy-corrected` and `--lossy` were
genuinely blocked. **Generalisable:** a guard on a field that is populated later in the pipeline is not
a weak guard, it is no guard — check *when* a field is set before trusting a validation that reads it.

**Why composition is exact, and how to test it.** A correction contributes an additive term through
`ColumnarTwoLevel::objectiveCorrection()`, which the L\* branch sums exactly as the base branch does. On
a **fixed partition** the term must therefore be identical under both bases — nothing in it may depend on
the codebook structure the base objective chose. That is a testable identity, not a hope:
`LstarMeta − Lstar == Lmeta − L` to 1e-9, both arms on the columnar engine, now a unit test. The lossy
objective shows the same thing across a parameter sweep: `--lambda` 1.5 → 5 moves the lossy term by 0.16
bits on `lossy_benchmark.net` while `L − L*` stays 0.057844703 to the printed digit.

**Two honest edges.**

- At the lossy default (`--lambda 1`) everything collapses into one noise module, where L\* equals L
  exactly (the single-module golden). Both jazz and the fixture give bit-identical values in all three
  arms — correct, but not evidence of anything. The λ sweep is the evidence; the collapsed case would
  have been a false positive for "lossy composes".
- `--entropy-corrected` composes *mechanically*, but its term is counted over module codebooks and L\*
  restructures those (a separate enter codebook per module, no index codebook). Spot check on jazz,
  identical partitions: L 6.881355491 vs L\* 6.886870402. Whether the bias *formula* transfers unchanged
  to L\*'s codebook structure is a modelling question left open, and worth flagging to whoever uses that
  combination first.

**No result moved.** All 38 recorded configs reproduce codelength, top-module count and level count
exactly after the removal, which is the expected outcome for deleting validation but is the kind of
"obviously safe" change that deserves the check anyway.

#### F35 addendum — the `--lossy` half is withdrawn, and the evidence offered for it was the defect (#1011, 2026-08-15)

**Withdrawn: that `--lossy` composes with L\*, and with it the heading's "every correction".** The
generalisation from F34 — L\* constrains which walk *steps* are possible, which is orthogonal to which
codebook a step is coded in — still holds for a correction that **adds** a term. `LossyCorrection` does
not add one: its noise credit hands the module's naming cost back at coefficient exactly **+1**, which is
what `scoreStackBase` charges and *not* what `scoreStackNonRedundant` charges (`nrLeafCodebookRate >= 1`).
So `--non-redundant --lossy` reported a J that was too high on every credited module and gated on the
wrong comparison. The derivation, the magnitudes and the reason the fix is not a one-liner are in **F40**;
the rejection F35 removed is reinstated in `applyAndValidateLossyInteraction` by the PR that files #1011,
this time for the reason that was missing the first time. F35's other conclusions stand: the
memory/multilayer guard really was dead code, the four higher-order rows really were capability the guard
only appeared to withhold, and `LstarMeta − Lstar == Lmeta − L` really is the right identity for
`MetaCorrection`. Only the lossy claim is retracted; `--entropy-corrected` remains the open edge F35
already flagged.

**The λ sweep was the tell, not the evidence.** F35 offered "`--lambda` 1.5 → 5 moves the lossy term by
0.16 bits while `L − L*` stays 0.057844703 to the printed digit" as proof of composition. Re-measured on
the pre-fix binary (`md5 32d7f08ccd5dbfeab91439f3bb568f2c`), fixed partition on `lossy_benchmark.net` with
`lossy_benchmark.clu` and `--no-infomap`:

| λ | `-C --lossy` | `--non-redundant --lossy` | `L − L*` |
| --- | --- | --- | --- |
| 1 | 2.4099867595799425 | 2.3521420558461954 | 0.05784470373374706 |
| 1.5 | 2.653992887305974 | 2.596148183572227 | 0.05784470373374706 |
| 2 | 2.7309159642290513 | 2.673071260495304 | 0.05784470373374706 |
| 2.5 | 2.807839041152128 | 2.749994337418381 | 0.05784470373374706 |
| 3, 4, 5 (gate shut) | 2.818018368324836 | 2.760173664591089 | 0.05784470373374706 |
| no `--lossy` at all | 2.818018368324836 | 2.760173664591089 | 0.05784470373374706 |

The constancy is not "to the printed digit" — it is **bit-identical at every λ, and identical to the
difference with no lossy term at all**, while the three modules' rates are 1.0021645 / 1.0075758 /
1.0111111. A credit that was actually derived against L\* would have to move when the codebook rates it is
charged against are not 1. Sameness was read as "the correction is objective-independent, therefore it
composes"; it is the signature of a hardcoded coefficient 1, i.e. of a correction that was never derived
against the second objective at all.

**Generalisable, and it is F37's distinction one step earlier.** The additive-composition test — fix the
partition, require the correction term to be identical under both bases — is the correct test for a
correction that *adds*. For one that *substitutes* part of the base objective's own accounting, the same
observation flips sign: an identical correction term under both bases is not the pass condition, it is the
failure signature — a substituting term is supposed to track the rate the base objective charged the
quantity it replaces, and under L\* that rate is not 1. F35 ran one test against two kinds of correction
without noticing there were two
kinds; F37 named the distinction, and F40 supplies the missing derivation for the lossy case. When a check
returns *exactly* the same number under a change that should have perturbed it, ask what would have had to
be true for it to move before recording it as agreement.

### F36 — Splitting a 4284-line file is a measurement problem, not a text problem (#1003, 2026-08-14)

`ColumnarMapEquation.cpp` held four unrelated concerns; the split is mechanical. What made it a *change*
rather than a move is that this build has **no LTO** (`-O3` only), so the translation-unit boundaries are
optimizer boundaries. Two consequences that decided the layout:

- The per-candidate arithmetic (the move deltas, and the hoisted forms that exist specifically to shave
  6 of 13 `plogp` calls off the inner loop) went into a **header**, `ColumnarObjective.h`, so it still
  inlines into `moveLoop`. `removeModuleTerms`/`addModuleTerms` stayed in `moveLoop`'s own TU for the
  same reason. Had they gone into a sibling `.cpp` the code would look better organised and run slower,
  with nothing in the diff to say so.
- `MapEquation.h` already declares an `OldSideTerms` at **namespace scope** (deliberately, so the
  privately-inheriting OO objectives can pass it by `auto`). The columnar kernels have the same names, and
  they only coexisted because they sat in an anonymous namespace inside one `.cpp`. Promoting them to a
  header at `namespace infomap` scope would have broken any TU that sees both. They live in
  `namespace infomap::columnar`, opened with one `using namespace columnar;` per consumer, so no call
  site changed.

**The seam is where the objective becomes nameable.** `hierarchicalCodelengthFromStack` had grown to 53
lines of base-L plus 63 of L\* behind an `if`, sharing only a teleport preamble and two accessor lambdas.
It is now `StackTerms` (what a stack scoring reads, resolved once) plus `scoreStackBase` and
`scoreStackNonRedundant`, bodies verbatim. The struct is the content: it states what a base objective's
scoring depends on, so a scorer is a function of the partition rather than of the whole optimizer. This is
also the shape a third objective would plug into — but deliberately **not** an abstraction (no template
policy, no virtual objective, no subclass). L\* differs from base L only in the cold path, so any of those
would be machinery for a hot-path variant that does not exist, and the one attempt at that variant was
measured and reverted in #1001. Daniel's instruction was explicit that an experimental modularity
objective must not drive this design.

**Two things the `if` was hiding.** It shadowed the function-scope `double total` with its own, which
`-Wshadow` (on in this build) had been reporting at `ColumnarMapEquation.cpp:1767` all along. And both
branches allocate three `std::vector`s per level per call — pre-existing, shared, and now visible as
shared rather than looking like something L\* introduced. Left alone on purpose: it is the kind of fix
that does not belong inside a move.

**Verification that a move deserves.** 38 of 38 recorded configs reproduce codelength, top-module count
and level count exactly; min-of-3 interleaved A/B on the two binaries (identical partitions, so time is
code) gives −0.64% to +0.85%; 31/31 tests in all three build configurations. An earlier A/B put powergrid
at +1.92% at min-of-3, which at min-of-9 was +0.52% against a baseline spread of 0.257–0.348s — the same
lesson as F28: **a threshold crossing on a 0.25s config is a statement about the session, not the code.**

### F37 — A correction that *substitutes* inherits the objective's coefficient; only an *additive* one composes freely (#1009, 2026-08-15)

`-C --non-redundant` on state / memory / multilayer input reported L\* too high. The size of the error
was a clean function of the partition: two triangles with all six nodes duplicated into two
indistinguishable states, partition unchanged, gave exactly **+1/56 bits**; duplicating only node 3 gave
exactly **+3/784**; ninetriangles all-×2 gave **+0.073076923077**; `examples/notebooks/data/jazz.net`
with a subset of its nodes lifted to two states each gave **+0.0045** (30 split nodes) and **+0.0189**
(78). Always positive, always growing with the number of
physical nodes holding several states in one module. `L` was invariant to `1e-15` throughout, so the
duplication was correct and the defect was in the objective.

**The mechanism.** Both stack scorers read a level-1 module's leaf flows only through
`F_m = sum_{leaf in m} plogp(flow)`, linearly. `MemCorrection` is not a term added beside them — it is
the same objective with `F_m^state` replaced by `F_m^phys`, i.e. a **substitution**, and a substitution
is charged at whatever rate the surrounding term consumes `F_m`. The two objectives do not agree:

- `scoreStackBase`'s level-1 block collapses algebraically to `plogp(T) - plogp(qExit) - F_m` with
  `T = moduleFlow + qExit`. Coefficient of `F_m`: exactly **1**.
- `nrEnterWithin` charges `-qEnter*F/moduleFlow` from its enter half and `-(usage/T)*F` from its within
  half. Coefficient: `qEnter/flow + (flow + qExit - qEnter)/(flow + qExit)`, which is
  `1 + qEnter*qExit/(flow*(flow+qExit))` — **>= 1**, equal to 1 only when `qEnter == 0` or `qExit == 0`.

`F^state - F^phys <= 0` (plogp is superadditive under splitting), so charging it at 1 instead of at
`rate >= 1` under-subtracts, and L\* came out high by `(rate - 1) * (F^phys - F^state)`. The sign of the
observed error was the confirmation before a line was written.

**The rule this generalizes to** — and the reason it shipped. `src/io/Config.cpp` justified composing
every correction with L\* on the grounds that "they are additive terms that `objectiveCorrection()` sums
on top of whichever base objective is selected". True, and true of the *value* for Meta / Bias /
Preferred, whose terms carry their own objective-independent rate: metadata is charged per unit of
node-visit flow, and L\* changes which codebook names a node, not how often a node is visited. False for
Mem, which re-encodes a codebook the base objective already priced. **Additive terms compose; substitutions
inherit.** `MemCorrection` is the only one of the five that substitutes — `LossyCorrection` is the near
miss, see below.

**The fix, and why the teleport preamble had to move.** `ColumnarTwoLevel::leafCodebookRates()` returns
the active objective's per-module rate (empty under the base objective, so its arithmetic is untouched
down to the summation order), and `MemCorrection` returns `sum_m rate_m * (F_m^state - F_m^phys)`. The
rates must be the *same* enter/exit the scorer used, teleport augmentation included — re-deriving them
from the link-only crossing flows would be wrong on exactly the flow models (`--regularized`,
`--recorded-teleportation`) where nothing else would flag it. So the preamble is now
`buildStackTerms()`, consumed by both `hierarchicalCodelengthFromStack()` and `leafCodebookRates()`,
rather than duplicated. `StackTerms` moved out of the anonymous namespace into `infomap::columnar` for
that (the header only forward-declares it).

**Verification.** Every duplicated case now equals its physical L\* to `<= 9e-14`; 12 `-C` configs are
bit-identical (`delta == 0.0`) against the pre-fix binary; jazz `-C --two-level -N20` 6.861229774903977
and `-C --non-redundant --two-level -N20` 6.817184613565288 both unmoved; 31/31 tests in all three build
configurations. The regression test builds the physical and duplicated networks from the same edge list
in-process, undirected / `--directed` / `--flow-model rawdir`, two-module and multi-module — it fails on
all six L\* assertions without the fix.

**Deliberately not fixed, recorded here.** (1) The leaf **move loop** stays base-flavoured under L\*
(`--non-redundant-exact` documents this: L\* drives the structural search, the base objective drives the
move arithmetic), so `MemCorrection::initMoveLoop`/`moveDelta`/`applyMove`/`mergeDelta` keep coefficient
1 by design. (2) `LossyCorrection` has the same defect **in kind** — it re-adds `sum plogp(f_i)` at +1 to
cancel what the base charged at −1, and L\* charges it at `-rate` — but it is not a mechanical
re-weighting (`l_m` is normalized by `F_m`, not by `T`), it is feature-gated behind
`INFOMAP_FEATURE_LOSSY_MAP_EQUATION`, and combining a rate-distortion penalty with L\* is a modelling
question that needs its own derivation — filed as #1011. (3) `MetaCorrection` was checked and is **correct** at
coefficient 1 — see the rule above; the test that asserts it stays green and its comment now says which
class of correction it speaks for.

### F38 — The engine that computes the total must also compute the decomposition (#1002/#1013, 2026-08-15)

Four separate user-visible defects, one cause. The columnar core computes its codelength **on the
stack**; every consumer that reads the materialized `InfoNode` tree was therefore reading a different
objective, or nothing at all.

- **Nothing at all.** `InfoNode::codelength` is written by exactly one function,
  `calcCodelengthOnTree`. `initTree`'s `maxDepth == 2 || twoLevel` shortcut never calls it, and a flat
  `.clu` reaches `initPartition` without going through `initTree` at all. So jazz `-C` printed a
  per-level table of `0.000000` next to `Best codelength 6.862755928`, and politicalblogs `-C -d -N2`
  printed `0.000156` — verbatim the *previous* trial's root value for a 2-module top level, on a
  79-module one. A leaked stale number is the proof that the field was never written rather than
  computed as zero.
- **A different objective.** netsci `-C --non-redundant -N10`: table Total `4.103756` against
  `Best codelength 3.892209764`. The table was summing L on a run whose headline is L\*.
- **A different objective, in the codelength itself.** `evaluateColumnarPartition` falls back to
  `calcCodelengthOnTree` for a ragged tree, and there is **no object-oriented L\*** —
  `grep -rn "nrEnterWithin\|nrExitTerm" src/ | grep -v Columnar` is empty. So
  `-C --no-infomap -c <ragged.tree> --non-redundant` returned *exactly* the value without the flag:
  ninetriangles 3.458078031 both ways, against a true L\* of 3.237864808 (+6.8%).
- **A missing objective.** The same fallback silently dropped `--preferred-number-of-modules`, the one
  correction with no object-oriented counterpart. Not a rounding error: the whole 4-bit penalty, gone.

**What made the ragged case fixable at all.** L\* is *exactly* invariant under inserting a pass-through
(single-child) level and the base map equation is not. The parent's enter codebook is
`e*(plogp(e) - plogp(e))/e == 0` and the child's leave-one-out exit term has numerator
`plogp(x) - 0 - plogp(x) == 0`; under the base objective the same node costs
`plogp(x+e) - plogp(e) - plogp(x) > 0`. Measured on ninetriangles with one such level above every leaf
module: base 3.38583082 → 3.97958082, L\* 3.078067323 → bit-identical. So a ragged tree can be made
rectangular *for free* under L\*, and only under L\* — which is what gates the padding on
`nonRedundant`, and why the guard inside `seedHierarchyFromLeafPaths` stays strict: it is what keeps
the base scorer honest.

**The one exception, and the shape of its fix.** `--entropy-corrected` counts NODES
(`m_multiplier * sum_k hierLevelSize(k) / (2*totalDegree)`), so the phantom levels inflate it. The
implementation does **not** subtract an analytic `padNodes*multiplier/(2*totalDegree)` term: it marks
the phantom stack nodes while walking the leaf chains and takes *their breakdown entries* off the total.
Same number (ninetriangles ragged: 3.468634039 = 3.237864808 + 36/156, one pad node short of the padded
tree's 3.475044295 = +37/156), but derived rather than asserted — and it stays right if a future
correction also charges per node. This is only possible **because** the breakdown exists: an analytic
discount would have to name `BiasedEntropyCorrection` in `InfomapBase`.

**A fifth defect the same instrument exposed.** The one-level fallback priced its collapse with
`getOneLevelCodelength()` — `calcCodelength` on a tree with **zero** modules — while the collapse
installs **one**. Under `--entropy-corrected` those differ by exactly `multiplier/(2*totalDegree)`
(ninetriangles 4.918622452 vs 4.925032709; er(80, 0.2) 6.315339939 vs 6.315739939), and the fallback
demonstrably fires there. Identical for the base map equation with no corrections, which is why it had
survived. The object-oriented path shares the convention at the analogous site — **not** changed here;
that is a master question, filed as a follow-up.

**The lesson, stated as a rule.** *A reported decomposition is a claim about the same objective as the
reported total; if the two come from different code, they will disagree, and only one of them is being
tested.* The per-level table had no test asserting it sums to `codelength()` — that one assertion,
added in `test_map_equation_invariants.cpp`, fails on **eleven** of the twelve engine × objective ×
depth combinations before this change and is what turned four scattered symptoms into one fix. It also
immediately caught a **sixth**, out of scope here: with `--num-trials > 1` and a best trial that is not
the last, `restoreBestResult` re-materializes the winner through `initTree` and the flat shortcut leaves
it unscored again — for *both* engines (object-oriented `--two-level -N3` on ninetriangles sums 0.936
against a codelength of 3.518; `-C -N10 -o json` on jazz writes `sum(modules[].codelength) = 0.531`
against 6.863). The console table escapes because it is captured as a string from the live tree of the
winning trial; only the rewritten **file** is wrong.

**Two adversarial corrections worth keeping.** (1) "`--non-redundant` is the only configuration where
the object-oriented fallback is wrong" was false — `--preferred-number-of-modules` is worse, and its
error (4 bits) is 18× the L\* one (0.22 bits). The right statement is: *the fallback reproduces exactly
those corrections that have an object-oriented counterpart in `calcCodelength`.* Verified pairwise on a
rectangular tree for base, `-d`, `-d --recorded-teleportation`, `--markov-time`,
`--variable-markov-time`, `--entropy-corrected`, `--meta-data` and `--regularized` — all agree;
`--preferred-number-of-modules` is the only one that differs (7.38583082 vs 3.38583082). (2) The
one-level fallback was diagnosed as "exact, no numeric change required" on the strength of the
`L*(one module) == L(one module)` identity, which is real but is **not** the identity that site needs:
the comparison is zero-module against one-module, not L against L\*.

**Left alone on purpose.** `getIndexCodelength()` is made objective-correct **only** under
`--non-redundant` (where `m_optimizer` holds a base index term and `getModuleCodelength()` was
returning `L* - L_index`, a hybrid). Under `--entropy-corrected` the columnar root charge and
`m_optimizer`'s index term differ by `multiplier/(2*totalDegree)` — but so do the **object-oriented
engine's own two answers**, since its per-level table charges the root `calcCodelength(m_root)` while
`getIndexCodelength()` returns the objective's bookkeeping (twotriangles `--entropy-corrected`: 0.214286
vs 0.178571). Picking one here would only make the engines disagree, so it is a master decision, and
the differential test that caught it stays as it was.

#### F38 addendum — what the review round measured (2026-08-15)

Four corrections to F38, one of them to a number in F38 itself. Each was reproduced before it was
fixed and re-measured after; the binaries are `07e63a8afc5d7d1191d1e8d911e300d9` (pre-change tip,
rebuilt from `0ef580e3` and reproducing that digest exactly) and `73d33a045d0cb930dca7c1174d98e583`.

**1. The assertion count above is wrong: 7 of 12, not eleven of twelve.** F38 says the new per-level
assertion "fails on **eleven** of the twelve engine × objective × depth combinations". Re-measured by
copying this PR's `test/cpp/test_map_equation_invariants.cpp` onto a clean `d88f1c77` worktree and
running that case alone: `assertions: 24 | 11 passed | 13 failed`, and the 13 failures fall on **7**
of the 12 combinations —

| combination | failing assertions | before |
|---|--:|---|
| ninetriangles `-C --two-level` | 2 | table 0, reported 3.51775 |
| ninetriangles `--non-redundant` | 1 | table 3.38583 (base L), reported 3.07807 |
| ninetriangles `--non-redundant --two-level` | 2 | table 0, reported 3.34451 |
| states.net `-C` | 2 | table 0, reported 2.01141 |
| states.net `-C --two-level` | 2 | table 0, reported 2.01141 |
| states.net `--non-redundant` | 2 | table 0, reported 1.92886 |
| states.net `--non-redundant --two-level` | 2 | table 0, reported 1.92886 |

Eleven is the count of *passing* assertions, transposed. The reviewer who caught it wrote "6 of 12"
and then listed seven, so both figures in circulation were wrong; the measurement above is the one to
keep. `--non-redundant` on ninetriangles is the only combination that fails just one of its two
checks — its table is non-zero and on the wrong objective, so it fails the equality and passes
`sum > 0`. That distinction is the whole point of having both checks.

**2. The empty-path bail was live code, and its comment was false.** F38's fix rectangularizes a
ragged tree by repeating each short path's finest module id, and bails when a path is *empty* — with
a comment claiming `initTree` "normalizes a bare top-level leaf into a module of its own". It does —
but only inside its `maxDepth == 2 || twoLevel` shortcut, and that is not the branch a ragged file
takes. `leafModulePathsFromTree` walks parents up to but **not** including the root, so once the file
carries a path deeper than 2 the shortcut is skipped, the bare leaf is attached straight to the root,
and its path is empty. Infomap's own `.tree` format both writes and reads a module of one node as a
bare top-level leaf (`2 0.15 "A" 1`), so a file mixing one of those with a deeper branch is the most
common ragged shape there is — and it went
straight down the object-oriented fallback: `twotriangles_flow.net` with `1:1:1 … 1:2:3` plus that
bare leaf reported **2.714170945 with and without `--non-redundant`** — bit-identical, the flag doing
nothing — against a true L\* of **2.187131226**. That is exactly the defect F38 claims to have
eliminated, on the shape it is most likely to meet.

The fix is the normalization the comment assumed: a top-level leaf gets a synthetic module id (past
every real id and every other synthetic one) and then the ordinary padding. What the first attempt got
wrong is worth recording, because it produced a *plausible* number: treating the synthetic bottom
module as phantom too — discounting it with the pad levels — gave 2.037131226, exactly 0.15 below the
true value, and every self-consistency check still passed (the per-level table totalled it). A
top-level leaf's own module is **not** phantom: it is the module the leaf constitutes, and the three
spellings of one partition must agree.

| tree | base L | L\* | L\* `--entropy-corrected` |
|---|--:|--:|--:|
| `2 "A"` (bare) | 2.714170945 | **2.187131226** | 2.544274083 |
| `2:1 "A"` (explicit module) | 3.014170945 | **2.187131226** | 2.544274083 |
| `2:1:1 "A"` (rectangular) | 3.314170945 | **2.187131226** | 2.579988369 |

L\* cannot tell them apart and the base map equation charges every level, which is the theorem the
padding rests on, now pinned from the empty-path side too. `--entropy-corrected` counts nodes, so the
bare and explicit spellings (four internal nodes) agree and the hand-written pass-through costs one
more, `2.579988369 − 2.544274083 = 1/28 = multiplier/(2·totalDegree)`. The synthetic module has no
`InfoNode`, so its charge goes on the **root** — which is the node `aggregatePerLevelCodelength`
already charges for the root's direct leaf children, so the table still totals the codelength
(twotriangles bare, L\*: level-1 leaf bits 0.150000 + level-2 module bits 0.319806 + level-3 leaf bits
1.717325 = 2.187131).

**What an empty path actually needs — a claim this addendum first got wrong.** The first draft of this
entry said the degenerate case "falls out for free: an all-top-level tree (every path empty) now scores
3.220279696 under L\*, where before it took the fallback and reported base L". Measured on the
pre-change binary `07e63a8afc5d7d1191d1e8d911e300d9`, rebuilt for the purpose: it scores **3.220279696
there too**, with no `ragged tree` line and no fallback, and the same 3.220279696 for the `.clu`
spelling; base L is 4.470950594 for both. Nothing changed for that shape. `initTree` takes its
`maxDepth == 2 || twoLevel` shortcut whenever no path is deeper than 2, and that shortcut routes
through `initPartition`, which gives **every top-level id a real module** — so a depth-1 file arrives
at the stack rectangular and no path is empty. An empty path therefore requires the file to mix a
depth-1 path with one deeper than 2, with no `--two-level`, which is exactly
`twotriangles_top_level_leaf.tree` and exactly the case measured above. "Every path empty" cannot even
arise through `initTree`: `maxDepth` is derived from those same paths, so all-depth-1 always takes the
shortcut. The all-top-level fixture stays, relabelled in the test as a **regression guard** — it passes
unchanged on the pre-change binary, which is what makes it a guard and not evidence.

The lesson is the previous round's, repeated one level down: the false comment that round fixed said
`initTree` normalizes a bare top-level leaf *always*, and the fix's own text said it normalizes
*nothing*. Both are absolutes; the truth is a condition — **only in the flat/two-level shortcut** — and
neither absolute was checked against a binary before it was written.

**3. The restore path made the failure mode worse, not better.** F38 records this as "a sixth, out of
scope". It is worth stating plainly why it could not stay out of scope: before #1002, jazz
`-C -N10 -o json` wrote `sum(modules[].codelength) = 0.0` — obviously broken. After, it wrote
**0.530794** against `"codelength": 6.862755928`: the *last* trial's stale charges, a number a reader
can believe. A hierarchical L\* winner got base-L values instead (powergrid `-C --non-redundant -N5`,
seed 1: 5.129307 against 4.518248787). The console table was right in both cases, being captured from
the live tree of the winning trial — so the file and the console disagreed, which is harder to notice
than both being zero.

`restampColumnarCodelengths` re-scores the restored tree on the columnar stack, on both restore paths
(`restoreBestResult`, and `maybeDeepRepairBest` whose per-level table is *rebuilt* from the tree
rather than captured). After: jazz 6.862760 against 6.862755928 (the 6-significant-figure
`jsonOutputNumber` rounding, followUp (e)), powergrid 4.518248 / 4.508245 / 4.508310 against
4.518248787 / 4.508245903 / 4.508310266 for seeds 1 / 7 / 42. No reported codelength moves. It also
closes the `--parallel-trials` gap in one stroke — those trials run on worker instances, so the main
instance's index term stayed −1 and `getIndexCodelength()`/`getModuleCodelength()` stayed the hybrid;
`runTrialsInParallel` always sets `bestTreeNeedsRestore`, so the re-stamp always runs (powergrid
`-C --non-redundant --parallel-trials -N5`: 5.129307 → 4.518248).

**4. Two smaller mixings.** (a) The columnar index term was cleared *after* `initTree`, but the stale
value is printed by `initPartition`'s `generated {} levels, codelength {:g} + {:g} = {}` line — one
call *earlier* than the reset, not later as the review had it. With
`-vv --non-redundant -c <file> --num-trials 3` on twotriangles, trial 1 printed `0.1 + 2.21417` and
trials 2 and 3 printed `0 + 2.31417`, pairing the previous trial's columnar root charge with this
trial's total. Clearing it inside `initTree` ties its lifetime to the tree it describes and covers
every re-materialization, which is what the reset was for. (b) When `stampColumnarCodelengths` fails
it re-scores the tree with `calcCodelengthOnTree`, but `evaluateColumnarPartition` returned the
columnar L — two objectives in the one branch that exists to keep them together. It now returns the
object-oriented total, sharing the ragged fallback's code (`objectOrientedTreeCodelength`, penalty
included). Unreachable today, both of them.

**What this cost — measured on the phase, because a whole-run A/B could not see it.** The re-stamp is
once per run and only when a restore happens, but it re-scores the whole tree, so it is a real cost on
the large case. The first version of this paragraph quoted **+0.9 % to +1.0 %** over 11 interleaved
reps of web-NotreDame `-d -C -N10`, and the code comment next to the borrow said **+0.4 %** against
**+1.7 %** without it. Neither survives re-measurement, and the two were never consistent with each
other anyway (a claimed halving of +1.7 % is +0.85 %, not +0.4 %) — a check the arithmetic would have
caught before the numbers were written.

The whole-run A/B is the wrong instrument on this machine. Three arms (pre-change tip, tip+re-stamp
with the borrow, tip+re-stamp with a bare `buildFromLeaves`) interleaved in one session, 7 reps each,
`user+sys`, load1m 10–40 throughout: minima **19.69 / 19.93 / 19.78 s**, medians **20.73 / 20.68 /
21.10 s** — the arms' order is different in the two statistics, and the arm that must be slowest has
the second-lowest minimum. A 1 % effect is simply below the floor here.

`--timing-json` reports the restore phase on its own (`best_restore_s`), and that *is* resolvable: 5
interleaved reps per arm, same session and same command (`-d -C -N10 --seed 123`), min/median —

| arm | `best_restore_s` min / median | vs tip | share of an 18.25 s run |
|---|--:|--:|--:|
| pre-change tip (restore, no re-score) | 0.2988 / 0.3041 | — | — |
| + re-score, borrowing the leaf CSR | 0.4984 / 0.5108 | +0.199 / +0.207 s | **+1.09 % / +1.13 %** |
| + re-score, bare `buildFromLeaves` | 0.5825 / 0.5921 | +0.284 / +0.288 s | **+1.55 % / +1.58 %** |

So: the re-score costs about **0.20 s** on this network, **+1.0 % to +1.3 %** of the run once an
independent whole-run A/B on a quieter machine (+1.16 % min, +1.27 % median, 5 reps) is folded in as
the top of the band. The borrow takes **0.084 s** off it — about **30 %** of the re-score's cost, not
a halving. That band is **over the project's 1 % line**, so it is a trade to accept explicitly, not to
wave through: it buys an objective-correct `modules[].codelength`, per-level table and index term in
every file a multi-trial or parallel-trial columnar run writes, and it fires once per run, only when a
restore fires.

**Single-trial runs are almost, but not quite, free of it.** `restoreBestResult` is guarded by
`m_trialsRun > 1`, so `-N1` never reaches it — but `maybeDeepRepairBest` has no such guard and
re-stamps too, so a `-N1` run pays the re-score whenever the deep repair *improves* the winner.
"`-N1` carries none of the new work by construction" is therefore too strong; the guarantee is "unless
deep repair fires". That is read off the two guards, not exhibited: a short scan (jazz, powergrid,
netsci at `-C -N1`, with and without `-2`, six seeds) produced no `-N1` run where the repair improved,
so the case is reachable but not common. Measured anyway: malaria `-C -N1` is inside the noise band
above.

Everything else is inside the noise of this machine: malaria `-C -N10`, air30k `-C -N10`, science2001
`-d -C -N10` and malaria `-C -N1` land between −2.0 % and +2.3 % over 9 reps with **min and median
disagreeing in sign**, which is the signature of load, not of code. Quote those as bands; the point
estimates in the previous round's report (malaria +0.68 %/+1.35 %) sat at the top of a spread an
independent measurement put at +0.45 %/+0.69 % and +2.01 %/+0.35 %.

**The lesson, which is F28's one level down.** Both wrong figures were whole-run percentages of a ~1 %
effect on a machine whose whole-run noise is ±5 %. The fix was not more reps — it was to find an
instrument whose signal-to-noise matches the effect. `best_restore_s` measures the changed phase and
nothing else, and it separates three arms cleanly in 5 reps on a machine where 7 reps of the whole run
separate nothing.

**Still not covered.** `getIndexCodelength()` is objective-correct under `--non-redundant` wherever a
tree is stamped: single-trial, serial multi-trial (the restore re-stamps, so the *best* trial's root
term is paired with the best trial's total — it was the *last* trial's before), and
`--parallel-trials` (through the same restore). It is **not** covered when a caller reads it after
mutating the tree by some other route, since only stamping sets it and `initTree` clears it — the
hard-partition path (`haveHardPartition()`, API-only), where `restoreHardPartition` re-expands the
tree after the stamping, is the concrete case, and it is unverified here. Under `--entropy-corrected`
the term stays the object-oriented one on purpose, for the reason F38 gives.


### F39 — Two codebooks over the same leaves: the `else` that made `--meta-data` delete the memory objective (#1012, 2026-08-15)

`InfomapBase::addColumnarCorrections` attached `MetaCorrection` under `if (haveMetaData())` and
`MemCorrection` under `else if (haveMemory())`. The two predicates are orthogonal —
`haveMetaData()` is `!metaDataFile.empty() || numMetaDataDimensions != 0`, `haveMemory()` is
`stateInput` — so `-C --meta-data` on state or multilayer input scored the **plain state-level map
equation plus the meta term**, with the physical-node codebook absent entirely. Not approximated:
absent. `--meta-data-rate 0` on the reproducer returns the state-level value exactly, and so does a
metadata file whose categories coincide with the found partition (meta term zero); both land on the
value of the *unaggregated* network, never on the aggregated one.

**The measurement that names the defect** is a difference of differences, not a codelength. Build two
networks over identical `*Links`: AGG maps states 1 and 2 to one physical node (and the optimum puts
them in the same module), IND gives every state its own. The physical-node codebook is worth
`L(AGG) - L(IND) = -0.330578512396694`. With `--meta-data` that difference was **exactly 0.0** —
which is what "the codebook is not there" looks like when you cannot see the objective.

**Composition is exact, not approximately exact.** After removing the `else`:

    AGG -C --meta-data  =  2.247219446970401
    AGG -C              =  1.331703102498692   (memory only)
    meta term           =  0.915516344471709   (IND with meta − IND without)
    1.331703102498692 + 0.915516344471709 − 2.247219446970401  =  0.0   (exactly, in double)

and the aggregation saving comes back at full value: `L(AGG) − L(IND)` is `-0.330578512396694` with
and without metadata, residual exactly 0. This is F37's rule read forwards rather than backwards:
`MetaCorrection` **adds** a term carrying its own rate, `MemCorrection` **substitutes** one sum for
another inside the module codebook term. Neither reads the other's quantity, so there is nothing to
double-count. Additive terms compose; substitutions inherit — and two corrections that touch
different terms compose with each other regardless.

**Where the `else` came from, and why it is not policy here.** `initOptimizer` really is exclusive:
`MetaMapEquation` and `MemMapEquation` are sibling `final` classes with different
`DeltaFlowDataType`s (`DeltaFlow` vs `MemDeltaFlow`), so `InfomapOptimizer<Objective>` can hold
exactly one. That is a **type-level constraint**, not a decision anyone made. The columnar core sums
corrections instead of inheriting an objective, so it never had the constraint — the `else` was
transcribed along with the shape of the dispatch. Composing them on the OO side means writing a new
objective class (both sources are `final`, so sharing rather than copying means extracting the meta
term first); that is a separate feature, and until it exists **`-C` and the default engine
deliberately disagree on meta + higher-order input**. That divergence is now pinned by an explicit
test in the `[columnar-differential]` family rather than left as a tolerance someone might loosen.

**Why every existing meta+state test stayed green — the part worth remembering.** Co-location is a
*necessary* condition: the physical-node codebook saves nothing unless two states of one physical
node share a module. `test/fixtures/networks/states.net` shares physical node 1 between states 1 and
4, and the optimum puts them in **different** modules. So the two tests that already ran meta+state
(`test_map_equation_invariants.cpp`, `test_flow.cpp`) were blind by construction, not absent —
2.929701072500 and 2.011405238446 are unmoved by this fix. The gap was never "no coverage"; it was
"coverage on the one shape that cannot see it". Hence the new fixtures
`states_shared_physical.net` / `states_distinct_physical.net`, whose entire purpose is the
co-location, and which are only useful as a *pair*.

**A second bug fell out, invisible at `-N1`.** `restoreBestResult` re-materializes the winning
trial's tree with `initTree`, which recomputes the codelength through the **OO** objective, and only
`if (m_infomap.nonRedundant)` was the columnar value put back. That guard was sound while L\* was the
only objective the two engines could disagree on. With the composition it no longer was: a 2-trial
run printed `2.247219447` to the console and wrote `2.577797959367095` to the `.tree`/`.json` files —
the same run reporting two numbers. It is invisible at `-N1` because the whole block is guarded by
`m_trialsRun > 1`, so anyone validating the fix with a single trial sees the right number and ships a
binary that writes the wrong one at the default `-N10`. Measured on an edit-1-only build: N=1 file
2.247219446970, N=2 and N=10 file 2.577797959367, console 2.247219447 throughout.

The guard is now `if (m_infomap.columnarSearch)` — the engine, not one objective — which is what the
columnar materialization site already documents ("the columnar core is the source of truth for the
search codelength"). 78 benchmark configurations (all 13 benchmark networks × {OO, `-C`,
`-C --non-redundant`} × {`-N1`, `-N10`}, comparing the console `Best codelength` *and* the codelength
written to the output file) are identical to all 12 printed digits between the pre-fix binary
(`md5 b601cf2f6e263c2be5ea1f1d496a77ec`) and this one (`md5 f02736dae6a0e3998c1e35a1565c50b0`). No
benchmark config is meta + higher-order — `lazega`, the only metadata benchmark, is first-order.

**It is not, however, bit-identical, and calling it a "no-op" was wrong.** Read the `-o json`
`codelength` at full double precision and **nine of the 13** `-C -N10` rows *do* move, in the last
ULPs. Full sweep of the benchmark set, `-C --seed 123 -N10 --silent -o json`, pre-fix
`b601cf2f6e263c2be5ea1f1d496a77ec` against the current tip
(`md5 b8af88017f344d105236e09bec0958b3` — `f02736da…` above is the same code with the older warning
string, and a clean rebuild after the comment corrections reproduces `b8af8801…` byte for byte),
sorted by |Δ|:

| config | pre-fix | post-fix | Δ |
|---|---|---|--:|
| web-NotreDame `-d -C` | 5.5685292930834125 | 5.568529293083488 | **+7.55e-14** |
| science2001 `-d -C --preferred-number-of-modules 25` | 8.235585529219229 | 8.235585529219179 | −4.97e-14 |
| powergrid `-C` | 4.7410720563526025 | 4.741072056352614 | +1.16e-14 |
| lazega `-C` | 6.0178602693857055 | 6.017860269385701 | −4.44e-15 |
| science2001 `-d -C` | 7.83343660140164 | 7.833436601401644 | +3.55e-15 |
| jazz `-C` | 6.862755928271481 | 6.862755928271479 | −1.78e-15 |
| netsci `-C` | 4.0545402451477415 | 4.054540245147743 | +1.78e-15 |
| multilayer (example) `-C` | 2.0114052384459713 | 2.0114052384459717 | +4.44e-16 |
| ninetriangles `-C` | 3.385830820341408 | 3.3858308203414076 | −4.44e-16 |

The four rows that are bit-identical: politicalblogs (6.740943136123672), malaria
(7.397501710124526), air30k (5.3924254128857285) and air30k `-d --regularized`
(5.576242406397039). All nine moving rows round to the same 12 significant digits, so nothing
printed anywhere changes — the movement is only visible in the JSON's full precision.

**An earlier version of this entry listed only six rows and called powergrid the largest.** That was
a partial sweep read as a complete one: the three rows it missed (web-NotreDame, science2001, and
science2001 with the preferred-modules bias) include the two *largest* moves, so the "largest" claim
was wrong by 6.5×. The correction is the table above — a full 13-row sweep, whose three added rows
reproduce digit-for-digit across two independent sessions. Lesson worth keeping: an enumeration is a
claim about the rows it *omits* as much as about the ones it lists, and it is only as good as the
sweep behind it.

Both control arms are byte-exact on **all 13** rows, swept the same way: OO (jazz 6.863047469426564,
powergrid 4.758729201129937, web-NotreDame 5.565924767871836) and `-C --non-redundant` (jazz
6.868228367042876, powergrid 4.509265422814488, web-NotreDame 5.517073626480589) — the
`--non-redundant` arms already restored the columnar value before this change. So the movement is
exactly the set the guard newly covers, and it is **the point of the change, not an accident**: the
restored value is now the columnar core's own, which differs from the OO recomputation of the *same* partition in the last
bits because the two engines sum the same terms in different orders. What the run reports is now what
the winning trial optimized and what the console printed. The verifiable claim is "identical to all
12 printed digits"; "bit-identical" is a claim that does not reproduce, and it was the headline
verification claim of this change.

**The warning had to be reworded, not gated.** `--meta-data takes precedence over higher-order
input: the run optimizes the meta-data objective ... without the physical-node codebook` became false
for `-C`. Gating it on `columnarSearch` was rejected for a reason that has nothing to do with this
bug: that string is **master-resident** and `columnarSearch` is not, so the line would be
unmergeable upstream and would conflict at every `sync-master-into-columnar`. It is now worded from
the *objective* rather than from the run — the rule the sibling warnings in the same function already
state — which is true on both engines and on master. It still earns its place under `-C`: the
composed total is the columnar one, but `getIndexCodelength()` and `getMetaCodelength()` are still
materialized through the meta-data objective. The first version of the reworded message also sent the
reader to "the per-level codelength breakdown reported for this run"; that clause is **removed**,
because the columnar per-level breakdown is unreliable in this build — a pre-existing reporting gap,
and not the "always zero" one it was first written up as. Two distinct failures, both measured on the
post-fix binary:

- **Two-level results print all zeros.** `jazz -C` and `states.net -C` (with *and* without metadata)
  print `0.000000` in every cell, `Total 0.000000`.
- **Hierarchical results print a populated but wrong total.** `air30k -C --meta-data <usstate> -N1`
  prints `0.776939 / 3.657179 / 6.192867`, `Total 10.626985`, against `Best codelength 7.580768` —
  and 10.626985 is exactly the **meta-data objective's** score of that same partition
  (10.626984626737512, measured by cross-scoring it with the pre-fix binary). So the table shows a
  real number computed on the wrong objective, not an empty table. Same shape on malaria + pmod8
  (`Total 11.649203` against 9.321316, and 11.649202759236191 is the meta-only score).
  A hierarchical *base* `-C` run is fine — `ninetriangles -C` totals 3.385831, its codelength, which
  is what F34 recorded, and is why "all zeros regardless" contradicted F34 rather than following it.

The sibling branch **`columnar-report-paths`** fixes exactly this class ("the reporting paths the
first pass left on the wrong objective"): on its binary `jazz -C` totals 6.899368 and the air30k
meta run totals 8.207547, each equal to the codelength that binary reports. It will be stacked
underneath this change before either lands, so the clause is removed here rather than repaired here.

**Recorded, not fixed.** (1) Composing the two objectives on the OO engine — see above. (2)
`--regularized` multilayer under `-C --meta-data` still loses the teleport prior: the columnar core
has no `RegularizedMultilayerMapEquation` equivalent, which the correction site has always said is
deferred. (3) Found in passing, **pre-existing and on the default engine**: `air30k -d --regularized
-N10` prints `Best codelength 5.578435633` and writes `5.578461103783` (Δ 2.5e-5). Identical on both
binaries, absent at `-N1`/`-N2`, so it is the same restore-path shape as the bug above but on the OO
side, where the new guard does not apply.

#### F39 addendum — the fix *does* redirect the search on real input, and the trade is not one-signed

The claim above that "in every case the PARTITION is unchanged; only the number moves" is **false**,
and it was false because every fixture it was checked on was a toy. On real higher-order input with a
metadata file the composed objective moves most of the partition. Measured on the same two binaries
(`b601cf2f6e263c2be5ea1f1d496a77ec` pre-fix, `f02736dae6a0e3998c1e35a1565c50b0` post-fix), `--seed
123`, `-C --meta-data`:

| config | states | modules | states whose module changed (after greedy label alignment) |
|---|--:|--:|--:|
| air30k + usstate `-N1` | 13 213 | 57 → 43 | 7 335 (55.5%) |
| air30k + usstate `-N10` | 13 213 | 67 → 23 | 11 478 (86.9%) |
| malaria + pmod8 `-N1` | 2 647 | 85 → 20 | 2 169 (81.9%) |
| malaria + pmod8 `-N10` | 2 647 | 83 → 17 | 2 203 (83.2%) |

**How the metadata files are generated** (no metadata file for these networks exists in the repo or
in `networks/`). `columnar_wip/make-state-meta.py <net-with-*States> <mode> <out.meta>` writes clu
format `"<stateId> <category>"` for every state in the `*States` section. Modes used here:
`usstate` — the two-letter US state code parsed out of the airport's `*Vertices` name
(`"City,ST:Airport"`), 52 categories, real geographic metadata; `phys` — the state's physical node
id; `mod<K>`/`pmod<K>` — `stateId % K` / `physicalId % K`, structure-independent controls. For
multilayer input (`malaria`, `examples/networks/multilayer.net`) the state ids are the ones the
expansion assigns, so the file is generated from `Infomap <net> <dir> -o states` output and then fed
back to the run on the original network.

**Timing: interleaved base/fix, 5 repetitions, min-of-5, wall and CPU.** The machine carried other
builds throughout (load average 20–38), so the control matters: `air30k -C` with **no** metadata —
the configuration the fix cannot reach — comes out at +0.7% wall / +0.3% CPU at `-N1` and +0.1% at
`-N10` with identical codelengths, which is the harness's noise floor.

| config | `-N1` wall (base → fix) | `-N10` wall (base → fix) |
|---|---|---|
| air30k + usstate | 1.089 → 1.080 s (−0.9%) ; 2nd pass 1.064 → 1.083 s (+1.8%) | 10.122 → 11.549 s (+14.1%) ; 2nd pass 9.973 → 11.169 s (+12.0%) |
| air30k + phys | 1.218 → 0.989 s (−18.8%) | 10.308 → 9.727 s (−5.6%) |
| air30k + mod4 | 1.511 → 1.056 s (−30.1%) | 12.287 → 8.553 s (−30.4%) |
| air30k + mod2 | 1.229 → 0.808 s (−34.3%) | 11.126 → 6.878 s (−38.2%) |
| air30k + const (one category) | 1.279 → 0.886 s (−30.7%) | 9.576 → 5.910 s (−38.3%) |
| malaria + pmod8 | 1.067 → 0.981 s (−8.0%) | 6.458 → 5.498 s (−14.9%) |
| air30k, **no metadata** (control) | 0.691 → 0.696 s (+0.7%) | 4.820 → 4.825 s (+0.1%) |

CPU time tracks wall within 0.5 pp everywhere except the two contended reps that min-of-5 discards.
So **the cost is metadata-dependent and not one-signed**: −38% to +14%. The only slowdown that
reproduces is `air30k + usstate` at `-N10`, **+12% to +14%** across two passes — over the 1% bar, so
it needs the maintainer's approval, which has not been given. The review's +48% at `-N1` did not
reproduce with any of the five metadata assignments above (its metadata file was not specified, so it
could not be used); the closest `-N1` numbers here are −34% to +2%.

**Quality, scored like for like.** The two binaries optimize *different objectives*, so comparing
their reported codelengths is not a quality comparison. Scoring both partitions under the **composed**
objective (`fix --no-infomap -c <partition>_states.tree`, an exact round trip — each partition
re-scores to its own search value):

| config | composed L(pre-fix partition) | composed L(post-fix partition) | Δ |
|---|--:|--:|--:|
| air30k + usstate `-N1` | 8.163175 | 7.580768 | **−0.582 (−7.1%)** |
| air30k + usstate `-N10` | 8.150495 | 7.422153 | **−0.728 (−8.9%)** |
| malaria + pmod8 `-N1` | 9.241098 | 9.321316 | **+0.080 (+0.87%)** |
| malaria + pmod8 `-N10` | 9.237589 | 9.291151 | **+0.054 (+0.58%)** |
| malaria + mod4 `-N10` | 8.949729 | 9.828636 | **+0.879 (+9.8%)** |
| malaria + pmod2 `-N10` | 8.427893 | 8.444542 | **+0.017 (+0.20%)** |

On air30k the composed objective is both a better objective and better optimized. On **malaria it is
not optimized as well**: the composed search lands above the value the *pre-fix* partition already
achieves under the same composed objective, on all three metadata assignments tried. The arithmetic
says why, and it closes exactly: the pre-fix malaria partition has **zero** co-located states (no
physical node has two of its states in one leaf module), so the physical-node codebook saves nothing
on it and its composed score equals its meta-only score to 1e-15. The composed search leaves that
layer-pure basin for cross-layer merges worth 2.324167 bits of codebook saving and pays more than
that back elsewhere.

**Each plogp figure belongs to one partition, not to the network.** `Σ plogp(aggregated) − Σ plogp(state)`
is a property of the tree it is computed on, so every figure below is tagged with the partition it
was computed from; it is also exactly the gap between the two scorers on that same partition
(meta-only minus composed), which is how each row is checked:

| partition | `-N1` | `-N10` |
|---|--:|--:|
| air30k + usstate, **pre-fix** | 0.044372 (8.207547 − 8.163175) | 0.048964 (8.199459 − 8.150495) |
| air30k + usstate, **post-fix** | 3.046216 (10.626985 − 7.580768) | 3.643979 (11.066132 − 7.422153) |
| malaria + pmod8, **post-fix** | 2.327887 (11.649203 − 9.321316) | 2.324167 (11.615318 − 9.291151) |

The three figures quoted in the paragraph above (0.048964 / 3.643979 / 2.324167) are the **`-N10`**
partitions'; the `-N1` partitions give 0.044372 / 3.046216 / 2.327887. The arithmetic closes on all
six, which is the point — but quoting one of them without naming its partition reads as a property of
the network, and it is not one. So this is a **search** problem on the richer objective, not an
objective error — but it means the change is not "quality up, time up"; on malaria it is quality
*down* under its own objective.

`examples/networks/multilayer.net` is structurally blind to the whole thing: with `phys`, `pmod2` and
`mod2` metadata its optimum still puts state 0 and state 3 (both physical node 1) in different
modules, so the physical-node codebook saves nothing and both binaries return 3.596368 / 2.929701.
That is the same reason `states.net` is blind (co-location is necessary), and it is worth stating
because it is the committed multilayer fixture.

**The benchmark set cannot see any of this**, and that is the coverage gap to close: no row in
`columnar_wip/benchmark-networks.md` is meta + higher-order (`lazega`, the only metadata row, is
first-order; `air30k`, `malaria` and `multilayer` are higher-order without metadata). Proposed row —
not added here, because the row obliges the performance snapshot to carry its numbers and that file is
being rewritten from a central benchmark run:

| network | path | run flags | directedness | type | size |
|---|---|---|---|---|--:|
| air30k (meta + states) | `networks/states/air2011/air30k.net` (+ `networks/states/air2011/air30k_usstate.meta`) | `--meta-data networks/states/air2011/air30k_usstate.meta` | undirected | **state / memory + metadata** (both codebooks) | 183 physical · 13 213 state nodes |

The metadata file itself is **not committed**: at 13 213 lines it is larger than any fixture in the
repo, and the network it keys is not in the repo either (`networks/` is a symlink to a data directory
outside it), so the row's fixture belongs next to the network, generated by the committed
`columnar_wip/make-state-meta.py`:

```
python3 columnar_wip/make-state-meta.py networks/states/air2011/air30k.net usstate \
        networks/states/air2011/air30k_usstate.meta
```

**Still not measured:** peak memory on any of these configurations, and `-N10` timings on the
`--regularized` multilayer arm (which is scored without the teleport prior under `-C` anyway).

### F40 — The lossy noise credit is derived against L, not L\*; the multiplier belongs inside the gate (#1011, 2026-08-15)

F37's item (2) filed `LossyCorrection` as "the same defect in kind, needs its own derivation". It does,
and this is it — together with the reason the derivation is recorded here rather than shipped.

**The defect.** `LossyCorrection::moduleCost` is `max(0, (plogp(F_m) - S_m) - lambda*H_m)` with
`S_m = sum_{leaf in m} plogp(f_i)`, summed and negated by `hierarchicalCorrection`. When the gate is
open the correction hands `S_m` back at coefficient exactly **+1**. That is precisely what
`scoreStackBase` charged (its level-1 block collapses to `plogp(T) - plogp(qExit) - S_m`), so under the
base objective the two cancel and the credit is right. `scoreStackNonRedundant` charges `S_m` at
`nrLeafCodebookRate(p, qEnter, qExit) = 1 + qEnter*qExit/(p*(p+qExit)) >= 1`, so under L\* only one of
the `rate_m` units is returned and a residue `-(rate_m - 1)*S_m` is left standing.

The tell is that the credit is **objective-independent**. On
`test/fixtures/networks/lossy_benchmark.net` with `test/fixtures/clusters/lossy_benchmark.clu`,
`--no-infomap`, lambda 1:

| flags | codelength |
| --- | --- |
| `-C -2` | 2.818018368324836 |
| `--non-redundant -2` | 2.760173664591089 |
| `-C --lossy` | 2.4099867595799425 |
| `--non-redundant --lossy` | 2.3521420558461954 (reported today) |
| `--non-redundant --lossy`, derived | 2.3383174785435856 |

`L - J = 0.4080316087448934` **bit for bit under both objectives**, while the three modules' rates are
1.0021645021645023 / 1.0075757575757576 / 1.011111111111111. A correctly derived credit cannot be the
same number twice; only a hardcoded coefficient 1 produces that. The residue closes:
`sum_m (rate_m - 1)*l_m = 0.0138245773`, and `0.4218561860475034 - 0.4080316087448934 = 0.0138245773`.

**The derived form.** For a level-1 module with flow `p`, `qEnter e`, `qExit x`, `T = p + x`,
`l = plogp(p) - S`, the exact code costs `nrEnterWithin(p, e, x, S)` and the noise code — the same L\*
module with its node set collapsed to one shared codeword — costs
`nrEnterWithin(p, e, x, plogp(p)) + lambda*H`. `nrEnterWithin` is affine in its last argument, so the
difference factors cleanly:

```
correction_m = -max(0, rate_m * l_m - lambda * H_m),   rate_m = nrLeafCodebookRate(p, e, x)
```

Two things about that expression matter more than the arithmetic:

- **The multiplier sits INSIDE the `max(0, .)`, and multiplies `l_m` only, not `lambda*H_m`.** It moves
  the **gate**, not just the accounting: under L\* a module is noise iff `rate_m*l_m > lambda*H_m`. This
  is the difference from #1010/`MemCorrection`, where the rate was a mechanical outer re-weighting of a
  quantity whose sign was already decided. Getting this wrong would give an achievable-but-not-minimal
  J rather than a plainly wrong one, which is harder to notice.
- **F37 (2) addendum — the worry that blocked it was unfounded.** F37 argued this was "not a mechanical
  re-weighting" because `l_m` is normalized by `p_m` and not by `T`. The algebra dissolves that: the
  substitution `S -> plogp(p)` is the same substitution in *both* halves of `nrEnterWithin`, so the
  T-normalization is already carried inside `rate`, and no re-expression of `l_m` is needed.
  `lambda*H_m` is untouched — distortion is a property of the walk, the same argument that cleared
  `MetaCorrection`.

**How wrong it is, and when.** The exact discriminator is: the reported J is wrong iff some module has
`rate_m > 1` **and** `rate_m*l_m > lambda*H_m`. `rate_m > 1` alone is necessary but not sufficient, and
neither condition is universal — two verified silent cases:

- **Zero boundary flow.** Two disjoint 4-cliques partitioned into their components: `rate == 1` exactly,
  and `-C --lossy` and `--non-redundant --lossy` both give 1.584962500721156.
- **Closed gate.** lambda 5 on lossy_benchmark: reported, derived and plain L\* are all
  2.760173664591089 despite rates 1.0022 / 1.0076 / 1.0111.

The error is `sum_m [max(0, r*l - lambda*H) - max(0, l - lambda*H)] >= 0`, so the reported J is **never
below** the correct one. Measured magnitudes. The "reported" column is the shipped feature build at
`d88f1c77` (`md5 32d7f08ccd5dbfeab91439f3bb568f2c`); the "derived" column is a throwaway build of that
same commit with fix step 1 only — `hierarchicalCorrection` summing
`max(0, rates[m]*(plogp(F_m) - flf_m) - lambda*H_m)` when `leafCodebookRates()` is non-empty —
(`md5 c0ddbff71d83d6bc73fa63f4f58ee69f`), cross-checked against a from-scratch model of the objective.
Both binaries were built and run in the session that revised this note; where the model and the build
disagree it is in the last ulp (`2.338317478543585` vs `2.3383174785435856`) and the build's value is the
one quoted:

| case | reported | derived | error (as a fraction of the reported value) |
| --- | --- | --- | --- |
| lossy_benchmark, fixed partition, lambda 1 | 2.3521420558461954 | 2.3383174785435856 | 0.59% |
| lossy_benchmark, `-N5 --seed 123`, lambda 2.58 | 2.760173664591089, `# noise modules 0 of 3: (none)` | 2.7579154218173167, module 3 is noise under the derived gate | 0.08%, and a **decision inversion** |
| ring of 10 two-node modules (`rate == 7/6` exactly), lambda 0.9 | 3.569925001442311 | 3.403258334775644 | 4.7% |
| same ring, lambda 1.05 | 3.6699250014423113, 0 noise modules | 3.553258334775644, all 10 noise | 3.2%, full inversion |
| jazz, 6-module partition rescored at lambda 1 | 6.189292031495019 | 5.983830576189069 | 3.3% |

The two rows that need a recipe: the **ring** is a 20-cycle with unit weights (`i -- i+1 mod 20`) clustered
into the ten adjacent pairs `{1,2}, {3,4}, …`, scored with `--non-redundant --lossy --lambda <λ> -c <clu>
--no-infomap`. The **jazz** row is `networks/arenas-jazz.txt`, partitioned by `-C --lossy --lambda 1.5 -N5
--seed 123` (6 top modules, `--clu`) and that `.clu` then rescored with `--non-redundant --lossy --lambda
1 --no-infomap`; plain L\* on the same partition is 6.836565239275705. An earlier draft of this row quoted
6.31061782556533 / 6.082232142189095 against a plain L\* of 6.858483286533167, from a jazz partition no
recorded command reproduces — the numbers here are the ones the two named commands print. Note what the
jazz row does *not* show: both builds still print `# noise modules 3 of 6: 1 2 3`, because the printed set
comes from the base-flavoured `noiseTopModules()`, which fix step 1 does not touch. That is the reporting
gap, and it is the next paragraph's point rather than a property of the derivation.

**The search trajectory does not change.** `m_nonRedundant` is read in exactly two places in all of
`src/` (`ColumnarObjectiveScore.cpp:253` and `:270`), so L\* never enters the leaf move loop — it scores
structural operators only, which is what `--non-redundant-exact` documents. Every partition tried
pre/post the patched build was bit-identical. So the honest claim is **"the reported J moves by
0.6-5% and the reported noise set can be wrong"**, not "the search optimizes a different objective".

**An invariant is already violated today.** `test/cpp/test_lossy.cpp` asserts
`J == rate + lambda*distortion`. At lambda 1.5 on lossy_benchmark the tree headers read

```
-C --lossy               # lossy lambda 1.5 J 2.65399 rate 2.42322 distortion 0.153846   -> 2.653989 == J
--non-redundant --lossy  # lossy lambda 1.5 J 2.59615 rate 2.42322 distortion 0.153846   -> 2.653989 != J
```

off by exactly `L - L*`. The test passes only because it has never run an L\* arm — and it cannot be made
to. `INFOMAP_TEST_ENGINE` (`test/cpp/TestUtils.h:68`, wired up by `add_infomap_columnar_contract_test` in
`CMakeLists.txt:187`) takes exactly two values, `oo` and `columnar`; there is no L\* analogue, so a
`[columnar-contract]` re-run adds `--columnar` and never `--non-redundant`. **The rejection this PR adds
makes that permanent until #1011 lands**: with `--lossy --non-redundant` throwing, no test can score a
lossy assertion under L\* at all. That is the right trade — a wrong number withdrawn beats a wrong number
asserted — but it means the ready-made failing test (a `--non-redundant --lossy --lambda 1.5` arm on the
`J == rate + lambda*distortion` case, which fails today at 2.59615 against 2.653989) can only be added by
the PR that lifts the rejection. It belongs in #1011's definition of done, not before it.

**Why this PR rejects the combination instead of shipping the derivation.** The objective is derived and
verified; the **reporting layer is not built**. `InfomapBase::noiseTopModules()` gates on
`loss > lossyLambda * lossyEntropy`, i.e. the base comparison, and `getLossyRate()`/
`getLossyDistortion()` come from the object-oriented `LossyMapEquation`, i.e. the base decomposition.
Both read per-module aggregates off the `InfoNode` tree and have no access to L\* boundary rates. With
only `hierarchicalCorrection` re-weighted, lossy_benchmark at lambda 2.58 prints
`J 2.75792 rate 2.81802 distortion 0` next to `# noise modules 0 of 3: (none)` — a J strictly **below**
plain L\*, with an empty noise set and zero distortion. Today's output is wrong but internally
consistent; that half-fix is wrong *and* self-contradictory, which is worse. So `--lossy
--non-redundant` now throws in `applyAndValidateLossyInteraction`, and the derivation waits here for the
PR that also makes `noiseTopModules()` and the rate/distortion accessors L\*-aware.

**Checked and benign, so nobody re-checks it.** `getLossyOneLevelLossless()` is `H(p_alpha)` — objective-
and lambda-independent — and needs no L\* variant. `--lossy` does **not** force undirected flow: it
*rejects* directed and non-undirected flow models and forces `twoLevel`. The practical consequence is
the same (`qEnter == qExit`), but the mechanism is a rejection, not a coercion. Sliced sub-optimizers are
self-consistent: `setNonRedundant` is never called on the sub-optimizer, so a sliced `LossyCorrection`
sees `leafCodebookRates() == {}` and stays at coefficient 1, matching the sub-core's `scoreStackBase`.

**The parse-time check is live, unlike its neighbour.** `Config::lossy` and `Config::nonRedundant` are
both plain parsed flag targets, populated before `adaptDefaults()` runs, so a validation-time check
fires. The `stateInput || multilayerInput` rejection two lines above it is the dead-check trap of F35:
those fields are set by `configureNetworkMode()` when the network is *read*, and no option sets them.

### F41 — A warm start is not a refinement, and every operator the engine skips "because it never pays" pays on a seed it did not build (#824, 2026-08-19)

`-C -c` was **bit-identical** to `-C` on every input tried — 51 of 51 seed/network/mode combinations in
the batch below. `columnarPartition()` built its optimizer from the leaf network and never looked at the
InfoNode tree, so a soft `--cluster-data` partition was read, echoed in the options banner, and then
discarded without a warning. On the Jelena state network
(`network_N256_om6_nc64_E100000_mu10_sample1.net`, `-2d`, `--seed 123 -N1`) the planted partition
evaluates at **6.930934993** under `-C --no-infomap -c`, and `-C -c` returned **7.810478411** — 12.7%
worse than its own input, and to the digit what `-C` returns with no seed at all.

**The seeding primitive already existed; what was missing was the search after it.**
`seedHierarchyFromLeafPaths` has been in the engine since the evaluation path needed it (F38), and
`deepRepairColumnarBest` already used it to re-enter the search from a materialized tree. Missing were
the route from the InfoNode tree into it, and a decision about what to *run* once seeded. Four things
had to be got right, and only the first is obvious.

**1. Rectangularization must not throw the seed away.** The stack holds one module level per leaf, so a
ragged tree has to be squared up. Truncating to the **mode** depth wins or ties all five ragged trees in
the benchmark set and is the cheapest (web-NotreDame 4.4s against 11.2s for padding to the maximum and
5.8s for keeping only the finest level, both of which hand the up-build a far finer bottom). The table is
in `columnarSeedPathsFromTree`.

**2. The split operator is the point, and its gate had to be lifted.** `splitTopModules` returns 0
unless a module-move-capable correction (Mem/Meta) is attached, on the argument that "merge overshoot
only exists where a correction drives the aggregation".

**3. So did the merge operator's.** `mergeLeafModulesWithinParents` returns false with no corrections
attached because "any merge only raises the codelength". Same premise, same flaw, and it cost more than
the split gate did: a 32-triangle ring seeded with its own planted partition (one module per triangle —
over-fine, so adjacent triangles want merging) came back **unchanged** at 3.652410119 under `-2`, 2.3%
above the 3.569591891 the same run finds from scratch. With the gate lifted it reaches
**3.566165627** — below the from-scratch search and below `OO -c` (3.60767667). That ring is now
`test/fixtures/networks/clique_ring.net`.

  The generalization is the useful part: **both gates encode the premise that the partition came out of
  this engine's own aggregation**, which has already settled every module-level move, so the only
  overshoot left is what a correction introduces. An externally supplied partition has no such
  provenance — it can be over-coarse (wants splitting) or over-fine (wants merging) under the plain map
  equation. Both operators compute a full base-objective delta and are gated on the true stack
  codelength, so the gates were never protecting correctness, only cost. One flag (`m_externalSeed`)
  lifts both for the seeded path and leaves engine-produced partitions bit-exact.

**4. Polishing cannot move a level boundary.** Neither operator that acts on a seeded stack — the
two-level interleave, or the interior-layer refinement — adds or removes a level, so a seed whose
*shape* is wrong for the objective traps the search at its own depth. Powergrid seeded with its own tree
truncated to 3 levels settled at **4.965670587** against 4.75504777 unseeded. Three pipelines were built
and measured against each other over the flat-seed hierarchical rows:

  (These three arms were measured while choosing between them, i.e. before item 3 lifted the merge
  gate. They compare pipelines against each other on one binary; they are not numbers to reproduce
  against the shipped one, which is better than all three on the rows where merging is what helps.)

  | seeded hierarchical pipeline | powergrid, coarse flat seed | air30k, perturbed flat seed | science2001, perturbed flat seed |
  |---|--:|--:|--:|
  | polish, then grow levels **above** the seed | 5.018444273 | **5.392743628** | 7.871545739 |
  | polish, then re-derive **below** the seed | 4.978133607 | 5.393608612 | 7.950085457 |
  | **below, then rebuild above it** | **4.749296398** | 5.393608612 | **7.843424549** |

  Growing upward keeps the seed as the finest level, so a coarse seed can never be refined: it costs up
  to **+6.2%** on powergrid (`pg-rand2 hier`: 5.043027402 against 4.749355). Re-deriving below without
  rebuilding above leaves the seed's own top level in place, which is what caps science2001 at 7.95.
  Doing both — sub-cluster each seed module's leaf set from singletons, so no building block crosses a
  seed module, then run the ordinary enter-flow up-build over those blocks — wins or ties everywhere
  except air30k (giving up 0.0065–0.038%) and is usually also *faster*, because the discarded arm was
  the expensive one. Applied to the deep branch too it turned `pg-cut3` from 4.965670587 into
  4.777177752 and `sci-cut3` into a win at 7.831943521, below both engines' from-scratch runs.

  Stated as one rule: **the seed fixes where the search starts and at what granularity; the structure
  above it is rebuilt.** That is also what the object-oriented warm start does, once you read it —
  `coarseTune` re-derives sub-modules inside each module with a sub-Infomap and the recursion rebuilds
  what sits above them, so OO does not preserve the seed's levels either.

**And a guard, because none of the above can promise what the issue asks for.** The seeded search is
gated on the true stack codelength, so it never comes out above the **seeded stack** — but a squared-up
ragged tree is not the input partition, so that promise does not reach the input. web-NotreDame with its
own object-oriented tree (depths 3–12) is the case: the tree scores 5.566025331, better than either
engine's from-scratch run, and every squaring left the seeded search between 5.5828 and 5.6300. So the
run now also scores the input tree itself — once per run, and only when squaring changed something,
since otherwise the seeded stack already prices it — and hands the input back untouched when nothing beat
it. `-C -c` on web-NotreDame now returns the input's 19 modules at 12 levels and 5.566025331. That is
also the only path by which a ragged tree can be returned exactly as given, the stack being unable to
hold one.

**The object-oriented arm is the weaker warm start, not the stronger one.** Parity was the goal, so:
`OO -c` **collapses to a single module** on a seed far from any optimum — powergrid with a random 2-,
10- or 200-module seed returns **12.00440383 with 1 module** (science2001 9.754829232, air30k
6.21487022), where the seeded columnar path returns 4.74–4.75. Over the 51 combinations `-C -c` beats
`OO -c` on **48** and loses on 3: `pg-cut3` (+0.22%), `sci-split` (+0.033%) and `sci-pref3` (+0.027%).
It is faster than `OO -c` in almost every row (jelena `-d`: 2.0s against 14.9s).

**What the semantics cost, stated plainly.** `-C -c` continues from the seed and does *not* also run a
from-scratch search, so where the seed is a worse starting point than singletons the result is worse than
`-C` without `-c`: 10 of the 51 combinations, by 0.0014% to **0.47%** (`pg-cut3`; the rest are ≤0.06%,
nine of the ten on science2001). That is inherent to warm-starting and OO has the same exposure on the
same rows. A best-of-two variant (seeded arm plus from-scratch arm, keep the lower) was built and
measured — it removes all 10 — and **rejected**: it is a second search per trial, it is not what
`--cluster-data` means, and it is not what the object-oriented engine does.

**A regression test on a symmetric network cannot pin the search's own answer.** The clique-ring fixture
this finding added (32 identical triangles in a ring) is maximally tied, and the first version of the test
pinned all three arms. CI disagreed on exactly one of them: the FROM-SCRATCH `-2` result came out
3.5695918914487512 on macOS clang and on every `OPENMP=0` build, and 3.5675… on gcc and MSVC with
`-fopenmp` — three jobs red, one assertion each. **The columnar core contains no OpenMP** (grep for
`_OPENMP` under `Columnar*` finds nothing) and `innerParallelization` defaults off, so this is not a
parallel-nondeterminism story: `-fopenmp` changes floating-point codegen, and on a network where the
merge candidates are exactly tied, that decides which pair of triangles the aggregation merges first. The
SEEDED arm has no such freedom — it starts from the planted partition — and its 3.5661656266226012 held
on all five CI toolchains, as did the input partition's own score. So the test pins those two and asserts
the from-scratch arm only relationally. Worth remembering for any future fixture chosen because the
greedy search fails on it: the properties that make it a good demonstration also make its own answer
unstable.

**Also fixed in passing.** `padLeafPathsToUniformDepth` called `resize(maxDepth, paths[i].back())`,
whose fill value aliases an element of the vector being resized — dangling if the growth reallocates.
Both it and the new helper copy the id out first.

### F42 — The memory objective has a GROUP hysteresis no pairwise operator crosses; the group proposal lives in the block graph's own flow structure (2026-08-19)

Follow-up to the #1028 Jelena rows: `-c` with the planted partition reaches 6.87, but the FREE `-C -2d`
search lands at **7.810 with 3585 modules** (om6) and **7.989 with 1 module** (om5) — the same seed-123
binary, over a bit worse than the planted partition's own `--no-infomap` score (6.931 / 6.902). Both free
end states are *fixpoints of every operator in the engine*: the leaf move loop, the module-level
aggregation passes, the pairwise merge (`mergeLeafModulesWithinParents`), the split, the descending
in-trajectory repair. Two failure modes bracketing the same missing optimum is the signature worth
remembering.

**Why the search cannot get there.** These networks have ~200 state nodes per physical node and **zero
links between co-physical pairs** (checked: 0 of 4.9M/5.7M pairs). The mem correction's reward for
folding co-physical flow together is superadditive in module size, so the augmented optimum wants ~20
modules of ~2500 states — reachable from the fragmented state only by merging ~150 fragments *at once*.
Every pairwise fragment merge is uphill (om6 stalls), while on om5 the same superadditive pull, being
community-blind (only 20% of co-physical pairs share a planted module), snowballs the module passes
straight through every community boundary into one module. Three dead hypotheses, so nobody re-tries
them: (1) **co-physical move candidates** (`COL_COMERGE=all/seeded`, the F-era knob) change nothing that
matters — om6 7.810 → 7.808, om5 unmoved, at up to 15× the trial cost; the barrier is group-level, not
candidate-level. (2) **Probing the module graph with its own (inherited) flows** is the Louvain
equivalence — module-level moves on the original network ARE unit moves on the aggregated graph — so a
probe of the converged partition's module graph returns all singletons by construction. (3) The
**hierarchical up-build's top level** is not the community structure either (purity 0.77 vs planted;
flattening it scores 7.83–7.84).

**What works: cluster the block graph as its own network under the enter-flow transform, then gate.**
The planted communities are plainly visible in the *inter-block* flow structure (mu = 0.10): a plain
first-order two-level of om6's 3585-fragment graph finds 24 groups of ~2100–2500 states at state-weighted
purity 0.936. The regroup probe does this in-engine after the aggregation converges: probe the finest
retained trajectory level with `flow := enter` (the up-build's super-network semantics; with true flows
see dead hypothesis 2), walk a **multi-scale ladder** (re-aggregate by the found grouping, probe again —
one probe stops at the base objective's own resolution on a sparse block graph: om5's 11049 blocks probe
to 4412 groups, nowhere near 20), and offer every rung as a candidate under the true objective,
keep-best. Two details carried the last percents: the candidate polish is a seeded move loop at **block**
granularity (whole-rung moves cannot fix intra-rung impurity: om6's best rung scored 7.99 polished at
rung granularity, 7.17 at block granularity), and it runs **purify-only** (`m_noEmptyTargets`: no
empty-module targets, otherwise the polish re-fragments a coarse impure seed back into the basin the
ladder was built to escape — om5's rung-2 candidate went K 45 → 645 with empty targets on).

**Result** (seed 123, `-C -2d`): om5 **7.989 → 6.868** (280 modules), om6 **7.810 → 6.886** (443);
`-N10` lands 6.868 / 6.887 on both `-2d` and `-d` — within 0.2% of the planted-seeded 6.858 / 6.873,
with no planted knowledge. Gated on module-move corrections, so every base network is bit-identical
(verified: 32 of 32 configurations vs the #1028 binary). `COL_REGROUP=off` is the A/B baseline.
Residual: `-C -d -N1` (hierarchical, single trial) stays in the bad basin (7.88 / 7.48) — the fine-blocks
bottom skips the probe (`maxAggPasses != 0`) and a single trial has no flat-first arm; any `-N2+` or `-2`
run is covered.

### F43 — The once-per-run winner repair never ran at -N1, and would have "repaired" --no-infomap (2026-08-19)

Found while wiring F42's probe into the winner repair: `maybeDeepRepairBest` reads `result.bestTree`,
which the serial path fills **only when `-N > 1`** (`updateBestResult` guards the copy — at `-N1` the
in-memory tree IS the winner and nothing needed it before #889). At `-N1` the pre-sized placeholder
entries `(state 0, empty path)` tripped `deepRepairColumnarBest`'s tree-mismatch guard, so the #889
deep repair — measured and shipped as a winner-repair that "amortizes with -N" — has silently never run
for any single-trial mem/meta run. Fixed by materializing the winner from the in-memory tree. The fix
un-masked a second hole: with a real tree at `-N1`, the repair also ran on **`--no-infomap`**
evaluations and returned a better partition than the one the user asked to score (the fixed-partition
differential test caught it: metadata case scored 3.34 against the OO arm's 4.29). `--no-infomap` now
gates the repair explicitly. Single-trial mem/meta runs pay the repair they were always supposed to pay
— om5/om6 `-2d -N1` go from ~1.1 s to ~2.1–2.6 s *and* from 7.1–7.9 to 6.87–6.89 bits; timings for the
benchmark set are in the PR snapshot.

**F42 addendum — the ladder behind a detector (same day).** The first shipped shape ran the full
pass-1-blocks ladder in every trial, which cost the healthy state networks +2..11% at `-N10` (air30k
`-2` +8.8%) for zero codelength change there — Daniel rejected the balance. Three variants were
measured before the final shape: (a) an early stop after two consecutive rejected rungs (the winner is
empirically rung 0–1; quality bit-identical on om5/om6) does not remove the dominant cost, which is
the rung-0/1 probe + block-granularity polish itself; (b) basing the ladder on the CONVERGED partition
(a few hundred units) instead of the pass-1 blocks is nearly free but gives back 0.3–0.5% on om5/om6 —
the converged units are too coarse to purify; (c) escalating from (b) to the blocks ladder when a
cheap-ladder rung is accepted misfires, because healthy networks accept marginal (~0.01%) rungs too.
THE FINAL SHAPE: the converged-base ladder runs as a pure DETECTOR — its accepts are always rolled
back — and only a SUBSTANTIAL win (> 0.1% of the codelength; the pathology's wins are 2–9%, a healthy
network's ~0.01%) escalates to the full pass-1-blocks ladder from the untouched state. Because the
ladder consumes no shared RNG state and everything downstream re-derives from bestTop, a run whose
detector stays quiet is BIT-IDENTICAL to the pre-probe engine: verified on air30k (`-C`/`-2`),
malaria, air30kmeta `-2`, lazega, multilayer at `-N10`. Interleaved min-of-3, loaded machine (load ~9,
ratios still paired): air30k `-N10` +1.7%/−1.0%, reg +0.9%/+0.2%, meta +0.4%/+1.8%, malaria
−2.8%/−1.7% — noise-level, against +2..11% before. om5/om6 quality kept or improved: `-2d -N1`
6.867282833/6.886461009, `-N10` 6.867282589/6.887275396. The residual ±0.01% on air30kreg
(`-C`/`-F`/`-2` +0.009..0.012%) and air30kmeta (−0.0002%) comes from the winner repair's fresh-split
sub-optimizers now running the ladder inside over-merged modules — once per run, not per trial.
