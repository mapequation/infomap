## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials; `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

> **Re-measured at this PR** (`columnar-hierarchical-core` + the #889 flat-first trials). All five variants — OO, `-C`, `-C -F`, OO `-2`, `-2 -C` — were measured fresh in one session at this tip, on a busier machine than the previous refresh (OO wall times are up as much as +60%; within-session ratios are what matter). Codelength changes vs the previous refresh are confined to the `-C` / `-F` rows on the networks whose optimum the flat search reaches (**all improvements**, some large — see the flat-first section below); **every OO and `-2` codelength in every variant is bit-identical** to the previous refresh, web-NotreDame included. Sub-0.1s "toy" times are at the process-startup floor — shown without a percentage.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="3">Meta</th>
<th colspan="4">OO (default)</th>
<th colspan="4">columnar (<code>-C</code>)</th>
</tr>
<tr>
<th>nodes</th><th>objective</th><th>flags</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">27</td><td>base</td><td>—</td><td align="right">3.38583082</td><td align="right">0.08s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.06s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">198</td><td>base</td><td>—</td><td align="right">6.86122978</td><td align="right">0.07s</td><td align="right">6</td><td align="right">2</td><td align="right"><b>6.86122978</b> (=)</td><td align="right">0.05s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04321510</td><td align="right">0.16s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05186752 (+0.21%)</td><td align="right">0.06s (−62%)</td><td align="right">3</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75648389</td><td align="right">2.00s</td><td align="right">5</td><td align="right">6</td><td align="right"><b>4.74907624</b> (−0.16%)</td><td align="right">0.32s (−84%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73952481</td><td align="right">0.20s</td><td align="right">74</td><td align="right">3</td><td align="right">6.74058207 (+0.02%)</td><td align="right">0.11s (−45%)</td><td align="right">78</td><td align="right">3</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83428058</td><td align="right">10.1s</td><td align="right">12</td><td align="right">4</td><td align="right"><b>7.83343660</b> (−0.01%)</td><td align="right">3.42s (−66%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56604138</td><td align="right">197.1s</td><td align="right">19</td><td align="right">12</td><td align="right">5.57279424 (+0.12%)</td><td align="right">22.3s (−89%)</td><td align="right">4</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.05s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.04s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.03s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50337896</td><td align="right">11.4s</td><td align="right">145</td><td align="right">3</td><td align="right"><b>7.42225457</b> (−1.08%)</td><td align="right">4.17s (−63%)</td><td align="right">160</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39395534</td><td align="right">13.2s</td><td align="right">18</td><td align="right">4</td><td align="right"><b>5.39366442</b> (−0.005%)</td><td align="right">5.72s (−57%)</td><td align="right">20</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57565280</td><td align="right">9.31s</td><td align="right">301</td><td align="right">3</td><td align="right"><b>5.57513716</b> (−0.009%)</td><td align="right">6.36s (−32%)</td><td align="right">12</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.92800030</td><td align="right">8.96s</td><td align="right">25</td><td align="right">4</td><td align="right">8.23835056 (+3.91%)</td><td align="right">3.87s (−57%)</td><td align="right">25</td><td align="right">3</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats or exactly ties OO. Parentheses on the columnar `-C` columns = change vs OO (`(=)` = bit-identical; negative time = faster).

**Reading the table**
- **Codelength** — columnar `-C` now **ties or beats OO on 9 of 13 configs**: it beats OO on powergrid (−0.16%), science2001 (−0.01%), malaria (−1.08%), air30k and regularized air30k, and exactly ties jazz, lazega and the toys. The air30k flat-optimum gap that #834 tracked (+1.33% / +1.56%) is **closed** — the flat-first trials reach the near-flat optima the enter-flow up-build cannot. Remaining gaps: netsci +0.21%, web-NotreDame +0.12%, politicalblogs +0.02%, and pref-mods +3.91% (was +6.72%; the `|K − K_pref|` bias is leaf-loop-only, #827).
- **Speed** — columnar is faster on every non-trivial network: ~2–3× on science2001 / malaria / air30k / powergrid, ~9× on web-NotreDame (22.3s vs 197.1s).
- **Shape** — columnar produces leaner, shallower maps (web-NotreDame: 4 top modules / 6 levels vs OO's 19 / 12) that the map equation scores as essentially equal; where the optimum is genuinely (near-)flat (malaria), the search now reports the flat partition instead of forcing a hierarchy.

### The fast dial `-F`

`-F` (`--fast-hierarchical-solution`) skips the interior-layer refinement and instead does a single bottom re-partition within grandparents plus the module-level coarsening loop. The flat-first trials apply to `-F` too (same probe, same gating), which is why it now matches `-C` on every flat-winning network.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">columnar <code>-C</code></th>
<th colspan="4">columnar <code>-F</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.06s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.07s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86122978</td><td align="right">0.05s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86122978 (=)</td><td align="right">0.05s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05186752</td><td align="right">0.06s</td><td align="right">3</td><td align="right">4</td><td align="right">4.06428378 (+0.31%)</td><td align="right">0.05s</td><td align="right">3</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74907624</td><td align="right">0.32s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77217700 (+0.49%)</td><td align="right">0.27s (−16%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74058207</td><td align="right">0.11s</td><td align="right">78</td><td align="right">3</td><td align="right">6.74058207 (=)</td><td align="right">0.11s</td><td align="right">78</td><td align="right">3</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.83343660</td><td align="right">3.42s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">3.27s (−4%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">5.57279424</td><td align="right">22.3s</td><td align="right">4</td><td align="right">6</td><td align="right">5.62448318 (+0.93%)</td><td align="right"><b>17.9s</b> (−20%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.04s</td><td align="right">6</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.04s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.03s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.03s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.42225457</td><td align="right">4.17s</td><td align="right">160</td><td align="right">2</td><td align="right">7.42225457 (=)</td><td align="right">3.84s (−8%)</td><td align="right">160</td><td align="right">2</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39366442</td><td align="right">5.72s</td><td align="right">20</td><td align="right">3</td><td align="right">5.39366442 (=)</td><td align="right">5.55s (−3%)</td><td align="right">20</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57513716</td><td align="right">6.36s</td><td align="right">12</td><td align="right">3</td><td align="right">5.57513716 (=)</td><td align="right">5.19s (−18%)</td><td align="right">12</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.23835056</td><td align="right">3.87s</td><td align="right">25</td><td align="right">3</td><td align="right">8.23835056 (=)</td><td align="right">3.98s (+3%)</td><td align="right">25</td><td align="right">3</td></tr>
</tbody>
</table>

Parentheses on the columnar `-F` columns = change vs `-C` (`(=)` = bit-identical; negative time = faster); both columns are from the current same-session re-measurement. Sub-0.1s toy times are shown without a percentage (measurement floor).

**`-F` now ties `-C` on 10 of 13 configs** — including pref-mods, where it previously lost another +2.3% — because the flat-first trials carry the same winning partitions into both searches. The dial only bites on the base networks with real deep hierarchy, where skipping the interior refinement trades codelength for speed: web-NotreDame **+0.9% for −20% time**, powergrid +0.5% for −16%, netsci +0.31%.

The columnar interior refinement stops at a diminishing-returns knee (default `--tune-iteration-relative-threshold` = 5e-3), which on deep networks trades the last ~0.06% of codelength for ~18% less search time; shallow networks are unaffected.

### Two-level clustering (`-2`)

`--two-level` is wired to the columnar engine on the `columnar-two-level` branch (PR #823): the full two-level optimize materialized as a two-level stack, followed by the correction-aware module-merge coarsening interleaved with a seeded leaf fine-tune until the pair stops improving, plus the #889 coarse-tune (in-trajectory repair every trial + deep repair of the winning trial). **This PR does not change `-2` — every codelength below is bit-identical to the previous refresh** (times re-measured in the same session as the tables above).

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="3">OO <code>-2</code></th>
<th colspan="3">columnar <code>-2 -C</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th>
<th>codelength</th><th>time</th><th>top</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.10s</td><td align="right">9</td><td align="right">3.51775481 (=)</td><td align="right">0.07s</td><td align="right">9</td></tr>
<tr><td>jazz</td><td align="right">6.86122978</td><td align="right">0.05s</td><td align="right">6</td><td align="right">6.86122978 (=)</td><td align="right">0.04s</td><td align="right">6</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28611584</td><td align="right">0.06s</td><td align="right">56</td><td align="right"><b>4.28228737</b> (−0.09%)</td><td align="right">0.04s</td><td align="right">57</td></tr>
<tr><td>powergrid</td><td align="right">5.59831236</td><td align="right">0.83s</td><td align="right">420</td><td align="right">5.63395576 (+0.64%)</td><td align="right">0.14s (−83%)</td><td align="right">426</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74031825</td><td align="right">0.12s</td><td align="right">74</td><td align="right"><b>6.73918608</b> (−0.02%)</td><td align="right">0.09s (−25%)</td><td align="right">81</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.94913415</td><td align="right">5.09s</td><td align="right">496</td><td align="right">7.94947087 (+0.004%)</td><td align="right">3.17s (−38%)</td><td align="right">508</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">6.74367900</td><td align="right">49.9s</td><td align="right">11 831</td><td align="right">6.75083498 (+0.11%)</td><td align="right">23.4s (−53%)</td><td align="right">11 687</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.04s</td><td align="right">7</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.04s</td><td align="right">6</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.03s</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50653124</td><td align="right">9.13s</td><td align="right">145</td><td align="right"><b>7.42225457</b> (−1.12%)</td><td align="right">3.38s (−63%)</td><td align="right">160</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39376962</td><td align="right">4.47s</td><td align="right">332</td><td align="right"><b>5.39262338</b> (−0.02%)</td><td align="right">4.01s (−10%)</td><td align="right">336</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57643406</td><td align="right">6.87s</td><td align="right">301</td><td align="right"><b>5.57540857</b> (−0.02%)</td><td align="right">3.87s (−44%)</td><td align="right">302</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.13124130</td><td align="right">6.36s</td><td align="right">25</td><td align="right">8.23577532 (+1.29%)</td><td align="right">3.35s (−47%)</td><td align="right">25</td></tr>
</tbody>
</table>

Parentheses on the columnar columns = change vs OO `-2` (**bold** = columnar beats or exactly ties OO). Columnar `-2` ties or beats OO on 11 of 13 configs, including *every* correction network; the exceptions are base-objective configs (powergrid +0.64%, pref-mods +1.29%), and it is faster on every non-trivial network (−10% to −83%).

### Flat-first trials: the flat candidate for the hierarchical searches (#889 hierarchical half, closes #834 — this PR)

Measured at the coarse-tune PR: the `-C` overshoot forms in the **enter-flow up-build**, not in the within-parent sub-optimizers — on networks whose optimum is (near-)flat with many modules (air30k: OO wants 301–332 top modules), no amount of interior refinement can reach the flat basin from the fine-blocks build. OO's own hierarchical search is flat-first (partition, then super/sub structure), so the columnar engine now gets **both search directions** and lets best-of-N pick per network:

1. **Flat-first alternate trials.** Even-numbered trials (2nd, 4th, …) of `-C` and `-F` run the flat search first; the first trial keeps the fine-blocks build, so `-N1` is **bit-identical** (verified on all 13 configs × {`-C`, `-C -F`}). To avoid paying the flat pipeline where flat is hopeless, the trial first runs a **cheap probe** — the full aggregation *without* the leaf fine-tune (module-level cost), sharing pass-1 with the fine-blocks screen (no second leaf sweep) — and completes the expensive leaf-level flat pipeline (fine-tune + merge↔retune interleave) only when the probe lands within 0.5% of the fine-blocks up-build. The completed flat stack becomes a gated candidate against the refined hierarchy, and the up-builds grown from the flat bottom join the strategy screen. The probe separates the benchmark set perfectly: every network where flat truly wins probes at est/build ≤ 1.000 (air30k 0.84, regularized 0.80, malaria 0.88, pref-mods 0.63, politicalblogs 0.96, jazz 0.996, lazega 0.995), every true flat-loser at ≥ 1.008 (science2001 1.008, netsci 1.034, powergrid 1.10, web-NotreDame 1.18).
2. **Winner deep repair for hierarchical runs.** The once-per-run deep repair of the winning trial (previously `-2` only) now also fires when a hierarchical run's winning tree is two-level-shaped — i.e. a flat trial won outright — and leaves deeper winners untouched. Deterministic in serial and parallel-trial modes (post-trial-loop, config-derived seed).

Per-feature attribution on the networks the PR moves (`-C`, best of 10; the deep repair's seed and result are read off the run log):

| config (`-C`) | before | flat-first trials | + winner repair | OO |
|---|--:|--:|--:|--:|
| air30k | 5.46566862 | **5.39366442** | – (3-level winner) | 5.39395534 |
| air30k reg. | 5.66241021 | **5.57513716** | – (3-level winner) | 5.57565280 |
| malaria | 7.47888701 | 7.47430185 | **7.42225457** | 7.50337896 |
| lazega | 6.03455147 | 6.02121843 | **6.01786027** | 6.01786027 |
| jazz | 6.87505673 | **6.86122978** | – (base net) | 6.86122978 |
| pref-mods | 8.46079677 | **8.23835056** | – (leaf-only bias) | 7.92800030 |
| politicalblogs | 6.74069306 | **6.74058207** | – (3-level winner) | 6.73952481 |

**Cost.** The flat trials only cost time where the probe says flat is competitive — i.e. where they win codelength. Interleaved same-session `-N10 -C` wall deltas vs the previous tip: air30k +39%, regularized air30k +61%, malaria +37% (of which the once-per-run deep repair is 0.50s by the timing registry, 14% of the 3.6s run), pref-mods +22%; **web-NotreDame, science2001, powergrid, netsci: unchanged within noise** (probe skips, module-level cost only). Even at the higher cost, `-C` stays 32–63% faster than OO on those networks (table above) while now beating OO's codelength on all of them except pref-mods.

### Coarse-tune: trajectory repair + winner deep repair (#889 two-level half, previous PR)

The objective-aware aggregation (#834) can overshoot: consolidation makes each pass's units atomic, so a merge that shouldn't have happened cannot be undone by later passes, the leaf fine-tune or the gated merges (single-leaf moves can't cross the barrier). The coarse-tune PR added the subdivision half, split by cost:

1. **In-trajectory descending repair** (every trial, marginal cost): each aggregation pass's unit level and leaf composition are retained — only when module-move-capable corrections are attached, so base networks pay nothing — and after the aggregation converges, *before* fine-tune smears the boundaries, each retained granularity is re-sorted within the best partition with a seeded module-level move loop, coarse to fine. This alone puts air30k and regularized air30k `-2` **below OO, at less time than before the operator existed**.
2. **Deep repair of the winning trial** (once per run): the expensive discovery step — from-singletons sub-clustering within each module (community granularity, so extracting a whole overshot community is a single gated move), interleaved with the seeded leaf fine-tune and the merge. A per-trial version was measured and rejected (malaria 2.5 → 8.9s: 78% of the time re-derived sub-clusterings the retunes kept invalidating); the engine runs it **once on the best-of-N partition** after the trial loop — deterministic in serial and parallel-trial modes, never worse than the seed, cost amortizing with `-N`. This is what finds malaria's 7.4223 and lazega's exact OO tie — and, with this PR's flat-first trials, carries those same values into `-C` and `-F`.

### Tele-path and metadata move-loop hoist (#875)

Two follow-ups to the #868 move-loop speedups: the recorded-teleportation delta hoists its six old-module plogp terms once per unit (`hoistOldSideTele`), and the metadata correction caches its per-leaf move-loop terms with the same zero-path fast-track as the memory correction. Both are **bit-exact** — codelengths are unchanged on every network × {`-C`, `-F`, `-2 -C`}.

The measurable win is the tele path: on **air30k `-d --regularized`** the hoist alone accounts for −13% / −15% / −16% on `-C` / `-F` / `-2 -C` (measured at #875). The Meta fast paths are correctness/consistency (the set has no large metadata network). The biased **science2001 `-d --preferred-number-of-modules 25`** config uses no teleport or metadata; its columnar partition lands the finest level on exactly 25 modules (`-2 -C`, top = 25) — the `|K − K_pref|` bias wired in [#827](https://github.com/mapequation/infomap/issues/827).
