## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials; `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

> **Re-measured at this PR** (`columnar-hierarchical-core` + the flat-first trials + partial seeding + the hierarchical split repair). Measured on an idle machine. **59 of the 65 codelengths below are bit-identical to the previous refresh**: the split repair moves malaria, air30k and regularized air30k under both `-C` and `-C -F`, all improvements; every base network, every OO and every `-2 -C` number is unchanged.

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
<tr><td>ninetriangles</td><td align="right">27</td><td>base</td><td>—</td><td align="right">3.38583082</td><td align="right">0.43s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.04s (−91%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">198</td><td>base</td><td>—</td><td align="right">6.86122977</td><td align="right">0.06s</td><td align="right">6</td><td align="right">2</td><td align="right"><b>6.86122977</b> (=)</td><td align="right">0.05s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04321510</td><td align="right">0.15s</td><td align="right">2</td><td align="right">5</td><td align="right">4.04960341 (+0.16%)</td><td align="right">0.06s (−60%)</td><td align="right">3</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75648389</td><td align="right">1.87s</td><td align="right">5</td><td align="right">6</td><td align="right"><b>4.73960028</b> (−0.35%)</td><td align="right">0.29s (−84%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73952481</td><td align="right">0.19s</td><td align="right">74</td><td align="right">3</td><td align="right">6.74058207 (+0.02%)</td><td align="right">0.11s (−42%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83428058</td><td align="right">8.57s</td><td align="right">12</td><td align="right">4</td><td align="right"><b>7.83343660</b> (−0.01%)</td><td align="right">3.15s (−63%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56604138</td><td align="right">158.0s</td><td align="right">19</td><td align="right">13</td><td align="right">5.56799417 (+0.04%)</td><td align="right">20.0s (−87%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.05s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.04s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50337896</td><td align="right">10.2s</td><td align="right">145</td><td align="right">3</td><td align="right"><b>7.41714932</b> (−1.15%)</td><td align="right">3.65s (−64%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39395534</td><td align="right">11.9s</td><td align="right">18</td><td align="right">4</td><td align="right"><b>5.39320406</b> (−0.01%)</td><td align="right">3.79s (−68%)</td><td align="right">20</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57565280</td><td align="right">8.36s</td><td align="right">301</td><td align="right">3</td><td align="right">5.57591489 (+0.005%)</td><td align="right">4.07s (−51%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.92800030</td><td align="right">8.11s</td><td align="right">25</td><td align="right">4</td><td align="right">8.23835056 (+3.91%)</td><td align="right">3.33s (−59%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats or exactly ties OO. Parentheses on the columnar `-C` columns = change vs OO (`(=)` = bit-identical; negative time = faster).

**Reading the table**
- **Codelength** — columnar `-C` **ties or beats OO on 8 of 13 configs**: it beats OO on powergrid (−0.36%), malaria (−1.08%), science2001 (−0.011%) and air30k (−0.005%), and exactly ties jazz, lazega and the toys. Remaining gaps: pref-mods +3.91% (the `|K − K_pref|` bias is leaf-loop-only, #827), netsci +0.16%, web-NotreDame +0.035%, politicalblogs +0.016%, regularized air30k +0.007% (a tie within seed noise).
- **The web-NotreDame gap is a user choice.** At the default knee it is +0.035%, down from +0.121% before partial seeding. Adding `--tune-iteration-relative-threshold 1e-3` takes it to **−0.096%, ahead of OO**, for about +20% CPU; `0` (full convergence) reaches 5.56061 for about +47%. F26 explains why the deeper refinement is a dial rather than the default.
- **Speed** — columnar is faster on every non-trivial network: ~2.6–3.2× on science2001 / malaria / air30k / regularized, ~5.9× on powergrid, ~8× on web-NotreDame (21.4s vs 171.6s).
- **Shape** — columnar produces leaner, shallower maps (web-NotreDame: 4 top modules / 6 levels vs OO's 19 / 13)

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.04s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.03s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86122977</td><td align="right">0.05s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86122977 (=)</td><td align="right">0.05s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.04960341</td><td align="right">0.06s</td><td align="right">3</td><td align="right">4</td><td align="right">4.06428378 (+0.36%)</td><td align="right">0.05s</td><td align="right">3</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.73960028</td><td align="right">0.29s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77217700 (+0.69%)</td><td align="right">0.20s (−31%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74058207</td><td align="right">0.11s</td><td align="right">78</td><td align="right">2</td><td align="right">6.74058207 (=)</td><td align="right">0.10s (−9%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.83343660</td><td align="right">3.15s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">3.02s (−4%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">5.56799417</td><td align="right">20.0s</td><td align="right">5</td><td align="right">6</td><td align="right">5.62448318 (+1.01%)</td><td align="right">14.7s (−26%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.04s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.04s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.41714932</td><td align="right">3.65s</td><td align="right">2</td><td align="right">3</td><td align="right">7.41714932 (=)</td><td align="right">3.56s (−2%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39320406</td><td align="right">3.79s</td><td align="right">20</td><td align="right">3</td><td align="right">5.39320406 (=)</td><td align="right">3.59s (−5%)</td><td align="right">20</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57591489</td><td align="right">4.07s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57591489 (=)</td><td align="right">3.53s (−13%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.23835056</td><td align="right">3.33s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23835056 (=)</td><td align="right">3.23s (−3%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

Parentheses on the columnar `-F` columns = change vs `-C` (`(=)` = bit-identical; negative time = faster); both columns are from the current same-session re-measurement. Sub-0.1s toy times are shown without a percentage (measurement floor).

**`-F` now ties `-C` on 10 of 13 configs** — including pref-mods, where it previously lost another +2.3% — because the flat-first trials carry the same winning partitions into both searches. The dial only bites on the base networks with real deep hierarchy, where skipping the interior refinement trades codelength for speed — and the F23 knee widens that trade, because it deepens exactly the refinement `-F` skips: web-NotreDame **+1.15% for −40% time**, powergrid +0.69% for −51%, netsci +0.36%. Partial seeding widens the dial further, because it improves `-C` on exactly the three networks `-F` cannot follow it on (the re-refine gate makes `-F` bit-identical by construction). On the flat-winning networks `-F` is now only 2–9% faster than `-C`, because `-C`'s expensive pass — the interior refinement — is exactly the one the flat bottom no longer needs (below).

The columnar interior refinement stops at a diminishing-returns knee (default `--tune-iteration-relative-threshold` = 5e-3), which on deep networks trades the last ~0.06% of codelength for ~18% less search time; shallow networks are unaffected.

### The refine knee: 5e-3 → 1e-3 (F23, this PR)

`ColumnarTwoLevel::m_minRelTuneImprovement` stops the interior-layer refinement once a whole up/down sweep gains less than this fraction of the post-build codelength. It was raised 1e-3 → 5e-3 on a measurement that web-NotreDame converged in **3** sweeps with the 3rd worth +0.06%; it now takes **5.5 sweeps (max 7)** and the truncated tail is worth **0.111%**, so the constant went stale as the rest of the engine changed around it.

The decisive evidence is an equal-CPU-budget comparison, which had never been run — earlier tuning fixed the trial count, but a looser knee makes each trial cheaper, so at fixed CPU you can buy more trials. On web-NotreDame the shipped knee is off the Pareto frontier at every budget measured:

| CPU | frontier config | codelength | 5e-3 at the same CPU |
|---|---|--:|--:|
| 3.4s | R=1e-3, `-N1` | 5.56954799 | `-N1` → 5.57279424 |
| 20.0s | R=0, `-N6` | 5.56769615 | `-N10` (22.7s) → 5.57279424 |
| 25.2s | R=1e-3, `-N10` | 5.56741191 | `-N13` (26.9s) → 5.57272320 |
| 32.0s | R=0, `-N10` | 5.56660930 | `-N16` (32.1s) → 5.57272320 |

**R=1e-3 at `-N1` (3.4s) beats R=5e-3 at `-N20` (39.9s)** — a twelfth of the CPU for 0.06% better codelength. Going `-N10` → `-N20` at the old knee costs +75% CPU and buys 0.0013%. The mechanism: per-trial spread is ~0.13%, but refinement shifts the whole trial *distribution* down, so a deeper refine's median trial beats the shallow knee's best-of-10 — extra trials cannot get there.

Cost, interleaved same-session, min-of-3 (the 22 bit-identical configs put this batch's noise floor at ~±3.5%):

| config | 5e-3 | 1e-3 | Δ codelength | CPU | wall |
|---|--:|--:|--:|--:|--:|
| web-NotreDame `-C` | 5.57279424 | **5.56741191** | −0.0966% | 23.38s → 25.42s (+8.7%) | 25.74s → 26.10s |
| powergrid `-C` | 4.74907624 | **4.74650715** | −0.0541% | 0.26s → 0.32s (+23.1%) | 0.31s → 0.36s |
| 11 other `-C` | — | bit-identical | 0 | −3.4…+2.1% (noise) | — |
| all 13 `-C -F` | — | bit-identical | 0 | −6.3…+2.1% (noise) | — |

Nothing else moves, structurally rather than by luck: `refineSweeps` is 1 for a stack with at most one interior layer, so science2001/air30k/malaria cannot react, and `-F` never enters `refineHierarchy`. `-N1` changes only on those same two deep `-C` configs (powergrid −0.0698%, web-NotreDame −0.0583%) — unlike the flat-first trials, `-N1` invariance is not a design goal here, since the knee legitimately affects every trial.

Every measured run behind this section is logged row-per-run in [`columnar_wip/columnar-search-runs.tsv`](columnar_wip/columnar-search-runs.tsv) for plotting the codelength/time frontier. Read the `batch` column before comparing times: session noise floors ranged ±3% to 13%, so the time axis is only comparable within a batch (`load1m`, `reps`, `agg` are recorded for that reason), and `derived=1` marks rows reconstructed from reported percentages rather than measured absolutes.

### Partial seeding: release the boundary, lock the module cores (F25, this PR)

A grandparent re-refine in `refineLayerWithinGrandparent` had two settings and both waste something. **From singletons** (the default) rediscovers the entire sub-partition, including the module cores that were never in doubt, and it is the most expensive pass in the search. **Fully seeded** (`seedAssignment`: singletons, then deterministic placement back into the old module, then greedy) reproduces the current partition, so the gate reverts and the pass is a full-price no-op.

Partial seeding is the middle, and it is a differently-*targeted* search rather than a cheaper one: rank a grandparent's units by how much of their flow leaves their current module, **lock** the confident cores and **release** the loosest fraction *q* as fresh singletons, so the greedy loop must re-place exactly the units whose membership was marginal.

**The ranking is the mechanism, not the release count** — the inverse control settles it (web-NotreDame, q=0.5, seed 123): boundary-release −0.015% < baseline < inverse-release +0.069% < random +0.153%. Releasing the same *number* of units by the inverse ranking is worse than baseline.

Two policy decisions came from measurement rather than design:
- **Re-refine only.** A layer is partial-seeded only on a re-refine (`refineHierarchy` sweep > 0), never on its first from-singletons derivation, because seeding's quality damage is localised to *first contact* with a grandparent rather than to stale seeds. This is what removes every regression the always-on variant had (science2001 +0.016% on 3/3 seeds, `-F` web-NotreDame +0.080% on 3/3). Consequence: `-F` refines once, so nothing there is ever a re-refine and **`-F` is bit-identical by construction**.
- **q = 0.40, metric = crossing link flow / total link flow.** Broad plateau; web-NotreDame collapses below q≈0.33 when always-on and everything degrades above 0.6. The alternative `exit/(flow + exit)` is **degenerate on undirected first-order networks** — `FlowCalculator.cpp:1475` sets `node.exitFlow = node.flow`, so the ratio is 0.5 for every leaf and the ranking collapses to node order; proof: on powergrid that metric and its exact inverse give bit-identical output.

| config (`-C`) | before | this PR | Δ CPU | vs OO |
|---|--:|--:|--:|--:|
| powergrid | 4.74907624 | **4.73960028** (−0.199%) | **+0.0%** (0.26s) | **beats by 0.36%** |
| web-NotreDame | 5.57279424 | **5.56799417** (−0.086%) | **+1.7%** (20.53 → 20.88s) | +0.035% (was +0.121%) |
| netscicoauthor2010 | 4.05186752 | **4.04960341** (−0.056%) | +0.0% (at floor) | +0.16% (was +0.21%) |
| the other 10 configs | — | bit-identical | — | — |

Seed-robust: at the 1e-3 knee the same policy held on 4 seeds per network (web-NotreDame −0.121/−0.114/−0.099/−0.140%, powergrid −0.151/−0.169/−0.253/−0.076%). Nothing regresses on any network or seed at either knee.

**Cost: none that resolves.** Interleaved min-of-3 on an idle machine: powergrid +0.0% (0.26s both), web-NotreDame +1.7% (20.53 → 20.88s), netsci and science2001 at the timer floor. At the 1e-3 knee the same feature cost powergrid +7.4%; at the shipped 5e-3 default that bill disappears **and powergrid's win is larger** (−0.199% vs −0.151%), because the shallower refinement leaves more for a well-targeted re-refine to find. Per-trial overhead is ≤1%.

air30k is **bit-identical on every variant and seed**: its `-C` winner is a flat-first trial whose leaf layer is already skipped by `m_bottomConverged` (F22), so there is no re-refine to partial-seed. Applying partial seeding to the converged flat bottom *instead* of skipping it was measured and rejected (−0.033%/−0.011% on two seeds for +16%/+34%/+41% CPU).

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
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.03s</td><td align="right">9</td><td align="right">3.51775481 (=)</td><td align="right">0.04s</td><td align="right">9</td></tr>
<tr><td>jazz</td><td align="right">6.86122977</td><td align="right">0.05s</td><td align="right">6</td><td align="right"><b>6.86122977</b> (=)</td><td align="right">0.04s</td><td align="right">6</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28611584</td><td align="right">0.06s</td><td align="right">56</td><td align="right"><b>4.28228737</b> (−0.09%)</td><td align="right">0.05s</td><td align="right">57</td></tr>
<tr><td>powergrid</td><td align="right">5.59831236</td><td align="right">0.58s</td><td align="right">420</td><td align="right">5.63395576 (+0.64%)</td><td align="right">0.14s (−76%)</td><td align="right">426</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74031825</td><td align="right">0.13s</td><td align="right">74</td><td align="right"><b>6.73918608</b> (−0.02%)</td><td align="right">0.09s (−31%)</td><td align="right">81</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.94913415</td><td align="right">4.21s</td><td align="right">496</td><td align="right">7.94947087 (+0.004%)</td><td align="right">2.38s (−43%)</td><td align="right">508</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">6.74367900</td><td align="right">45.0s</td><td align="right">11831</td><td align="right">6.75083498 (+0.11%)</td><td align="right">18.6s (−59%)</td><td align="right">11687</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.05s</td><td align="right">7</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.04s</td><td align="right">7</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.03s</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.04s</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50653124</td><td align="right">7.45s</td><td align="right">145</td><td align="right"><b>7.42225457</b> (−1.12%)</td><td align="right">2.72s (−63%)</td><td align="right">163</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39376962</td><td align="right">4.11s</td><td align="right">332</td><td align="right"><b>5.39262338</b> (−0.02%)</td><td align="right">3.50s (−15%)</td><td align="right">336</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57643406</td><td align="right">6.25s</td><td align="right">301</td><td align="right"><b>5.57540857</b> (−0.02%)</td><td align="right">3.56s (−43%)</td><td align="right">303</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.13124130</td><td align="right">5.83s</td><td align="right">25</td><td align="right">8.23577532 (+1.29%)</td><td align="right">3.03s (−48%)</td><td align="right">25</td></tr>
</tbody>
</table>

Parentheses on the columnar columns = change vs OO `-2` (**bold** = columnar beats or exactly ties OO). Columnar `-2` ties or beats OO on 11 of 13 configs, including *every* correction network; the exceptions are base-objective configs (powergrid +0.64%, pref-mods +1.29%), and it is faster on every non-trivial network (−10% to −83%).

### Flat-first trials: the flat candidate for the hierarchical searches (#889 hierarchical half, closes #834 — this PR)

Measured at the coarse-tune PR: the `-C` overshoot forms in the **enter-flow up-build**, not in the within-parent sub-optimizers — on networks whose optimum is (near-)flat with many modules (air30k: OO wants 301–332 top modules), no amount of interior refinement can reach the flat basin from the fine-blocks build. OO's own hierarchical search is flat-first (partition, then super/sub structure), so the columnar engine now gets **both search directions** and lets best-of-N pick per network:

1. **Flat-first alternate trials.** Even-numbered trials (2nd, 4th, …) of `-C` and `-F` run the flat search first; the first trial keeps the fine-blocks build, so `-N1` is **bit-identical** (verified on all 13 configs × {`-C`, `-C -F`}, and identical in cost). To avoid paying the flat pipeline where flat is hopeless, the trial first runs a **cheap probe** — the full aggregation *without* the leaf fine-tune (module-level cost), sharing pass-1 with the fine-blocks screen (no second leaf sweep) — and completes the expensive leaf-level flat pipeline (fine-tune + merge↔retune interleave) only when the probe lands within 0.5% of the fine-blocks up-build. The completed flat stack becomes a gated candidate against the refined hierarchy, and the up-builds grown from the flat bottom join the strategy screen. The probe separates the benchmark set perfectly: every network where flat truly wins probes at est/build ≤ 1.000 (air30k 0.84, regularized 0.80, malaria 0.88, pref-mods 0.63, politicalblogs 0.96, jazz 0.996, lazega 0.995), every true flat-loser at ≥ 1.008 (science2001 1.008, netsci 1.034, powergrid 1.10, web-NotreDame 1.18).
2. **The flat bottom skips the leaf-layer re-derivation** (`m_bottomConverged`). A build grown from the completed flat pipeline arrives with its leaf partition *already* at the two-level fixpoint, but `refineHierarchy` still began its interior sweep at layer 0 — and `refineLayerWithinGrandparent(0)` re-partitions every leaf from singletons within its grandparent. Phase timers say that pass was **the single most expensive thing in a flat trial and returned nothing**: 0.59 s of a 1.05 s air30k flat trial, with all five trials printing `build=5.393664 -> refined=5.393664`, while the flat completion the probe gate exists to ration cost only 0.14 s. It is skipped while the bottom is the flat fixpoint (`-F`'s `refineBottomWithinParents` likewise); interior layers above still refine, and an accepted refine there re-marks layer 0 dirty, so the re-derivation stays reachable once the structure it nests in actually moves.
3. **Winner deep repair for hierarchical runs.** The once-per-run deep repair of the winning trial (previously `-2` only) now also fires when a hierarchical run's winning tree is two-level-shaped — i.e. a flat trial won outright — and leaves deeper winners untouched. Deterministic in serial and parallel-trial modes (post-trial-loop, config-derived seed).

Per-feature attribution on the networks the PR moves (`-C`, best of 10):
| config (`-C`) | before | flat-first trials | + winner repair | OO |
|---|--:|--:|--:|--:|
| air30k | 5.46566862 | **5.39366442** | – (3-level winner) | 5.39395534 |
| air30k reg. | 5.66241021 | **5.57602419** | – (3-level winner) | 5.57565280 |
| malaria | 7.47888701 | 7.47430185 | **7.42225457** | 7.50337896 |
| lazega | 6.03455147 | 6.02121843 | **6.01786027** | 6.01786027 |
| jazz | 6.87505673 | **6.86122978** | – (base net) | 6.86122978 |
| pref-mods | 8.46079677 | **8.23835056** | – (leaf-only bias) | 7.92800030 |
| politicalblogs | 6.74069306 | **6.74058207** | – (3-level winner) | 6.73952481 |

Malaria is the one config the flat trials barely move on their own (−0.06%): its win is the winner deep repair. Malaria's flat-bottom builds are two-level, so they never enter the interior refinement at all — which is why its flat trials are *cheaper* than its hierarchical ones.

**Cost.** The flat trials cost time only where the probe says flat is competitive — i.e. where they win codelength — and item 2 removes the part of that bill that was buying nothing. Interleaved same-session A/B, `-N10`; the session ran at load average 24–65, so each config was repeated 4–5× per binary and the table reports the **minimum** observed time (the contention-free estimate), pooled over every repetition run this session. Wall and CPU agree to within ~1 pp on every row.

| config | codelength Δ | base CPU → new CPU | time Δ (`-C`) | time Δ (`-F`) | for reference: #891 before this fix |
|---|--:|--:|--:|--:|--:|
| air30k | −1.32% | 3.68s → 3.92s | **+6.5%** | +11.7% | +39% |
| air30k reg. | −1.53% | 3.52s → 4.13s | **+17.3%** | +18…21% | +61% |
| malaria | −0.76% | 3.37s → 3.34s | **−0.9%** | +4.1% | +37% |
| science2001 pref-mods | −2.63% | 3.26s → 3.42s | **+4.9%** | +5.7% | +22% |
| jazz / lazega / politicalblogs | −0.20% / −0.28% / −0.002% | ≤0.07s (floor) | 0% | 0% | — |
| science2001 | unchanged | 3.17s → 3.22s | +1.6% | ~0% | ±noise |
| web-NotreDame | unchanged | 21.19s → 21.07s | −0.6% | ~0% | ±noise |
| powergrid / netsci | unchanged | 0.25s → 0.26s / floor | +4% (0.01s) / 0% | ~0% | ±noise |

**No network now pays a non-trivial cost for a non-existent gain.** The five configs whose codelength the flat search cannot improve (netsci, powergrid, science2001, web-NotreDame, and politicalblogs at −0.002%) come in between −0.6% and +4%, and the +4% on powergrid is a single hundredth of a second at the measurement floor — the probe is module-level work on a network whose aggregation has already collapsed. Regularized air30k is the only config left with a bill worth naming, and it is now genuine flat-search work: the probe's aggregation passes (+0.15 s/trial — the tele/regularized corrections ride the module-level passes there) plus the flat completion (+0.15 s/trial), partly offset by the refinement that no longer runs. Individual repetitions of that row ranged +14% to +25% under load; +17% is the pooled minimum. `-C` stays 52–69% faster than OO on all four gaining networks.

**One codelength moved relative to the first cut of this PR**, and it is worth stating plainly: regularized air30k at seed 123 goes 5.575137 → 5.576024 (+0.016%). That 0.24% was won by a single flat trial's leaf re-derivation, and it does not survive a seed change — seeds 234/345/456 are bit-identical with and without the pass, and on all three `-C` already sits 0.02–0.10% above OO. The earlier "regularized air30k beats OO" was a seed-123 artefact; ~3 s per `-N10` run for a 1-in-4-seed lottery ticket is not a trade worth keeping, and a winner-only variant does not recover it either (at seed 123 the winning trial is one where the pass gains nothing).

### Coarse-tune: trajectory repair + winner deep repair (#889 two-level half, previous PR)

The objective-aware aggregation (#834) can overshoot: consolidation makes each pass's units atomic, so a merge that shouldn't have happened cannot be undone by later passes, the leaf fine-tune or the gated merges (single-leaf moves can't cross the barrier). The coarse-tune PR added the subdivision half, split by cost:

1. **In-trajectory descending repair** (every trial, marginal cost): each aggregation pass's unit level and leaf composition are retained — only when module-move-capable corrections are attached, so base networks pay nothing — and after the aggregation converges, *before* fine-tune smears the boundaries, each retained granularity is re-sorted within the best partition with a seeded module-level move loop, coarse to fine. This alone puts air30k and regularized air30k `-2` **below OO, at less time than before the operator existed**.
2. **Deep repair of the winning trial** (once per run): the expensive discovery step — from-singletons sub-clustering within each module (community granularity, so extracting a whole overshot community is a single gated move), interleaved with the seeded leaf fine-tune and the merge. A per-trial version was measured and rejected (malaria 2.5 → 8.9s: 78% of the time re-derived sub-clusterings the retunes kept invalidating); the engine runs it **once on the best-of-N partition** after the trial loop — deterministic in serial and parallel-trial modes, never worse than the seed, cost amortizing with `-N`. This is what finds malaria's 7.4223 and lazega's exact OO tie — and, with this PR's flat-first trials, carries those same values into `-C` and `-F`.

### Tele-path and metadata move-loop hoist (#875)

Two follow-ups to the #868 move-loop speedups: the recorded-teleportation delta hoists its six old-module plogp terms once per unit (`hoistOldSideTele`), and the metadata correction caches its per-leaf move-loop terms with the same zero-path fast-track as the memory correction. Both are **bit-exact** — codelengths are unchanged on every network × {`-C`, `-F`, `-2 -C`}.

The measurable win is the tele path: on **air30k `-d --regularized`** the hoist alone accounts for −13% / −15% / −16% on `-C` / `-F` / `-2 -C` (measured at #875). The Meta fast paths are correctness/consistency (the set has no large metadata network). The biased **science2001 `-d --preferred-number-of-modules 25`** config uses no teleport or metadata; its columnar partition lands the finest level on exactly 25 modules (`-2 -C`, top = 25) — the `|K − K_pref|` bias wired in [#827](https://github.com/mapequation/infomap/issues/827).

### The split operator on the hierarchical path (F27, this PR)

The hierarchical search could split a *single* unit off into a new empty module (`moveLoop`'s empty-module option) but never a *group* out of an over-merged one: `mergeLeafModulesWithinParents` only coarsens, and `refineLayerWithinGrandparent` re-derives a whole grandparent all-or-nothing, so a re-derivation containing one good split plus several bad ones is rejected wholesale. `splitLevelModules` is the hierarchical analogue of the two-level operator from #890: partition a level-(k+1) module's level-k children into pieces, aggregate a piece-level network, run a seeded move loop over the pieces (a piece may land in any module, including an empty one — group-split *and* cross-parent relocation), gated on the true stack codelength.

It runs **once per run on the winning trial**, not per trial. The per-trial version was measured and dropped: on air30k it accumulates 4.05% of in-trial gain and delivers −0.033%, because ~85% of that lands on hierarchical trials sitting 1.1–1.5% behind the flat-first trials, which can never win the best-of-N. **The discriminator is trial competitiveness, which is inherently cross-trial**, so no within-trial rationing separates the networks — level gating, piece-source gating, gain ratchets, attempt caps and shape gating were each measured and each failed. The per-trial half cost air30k +24% and regularized +16%.

| config (`-C`) | before | this PR | seed 234 | seed 345 | mean | Δ CPU |
|---|--:|--:|--:|--:|--:|--:|
| malaria | 7.42225457 | **7.41714932** (−0.069%) | −0.613% | −0.431% | **−0.371%** | +5.1…11.1% |
| air30k | 5.39366442 | **5.39320406** (−0.009%) | −0.001% | −0.002% | −0.004% | +1.9…2.9% |
| air30k (reg.) | 5.57602419 | **5.57591489** (−0.002%) | −0.022% | −0.031% | −0.018% | +1.5…3.1% |
| the other 10 configs | — | bit-identical | — | — | 0 | at floor |

Malaria's is the win that justifies the operator, and note that **seed 123 is its weakest seed** — the headline single-seed figure understates the mean. It is the first hierarchical `-C` result to beat malaria's repaired flat one.

Three things make the once-per-run path work, each measured separately: **best-per-shape repair** (track the best *deep* trial and repair it when the overall winner is flat — without it the hook makes zero attempts on malaria, whose winner *is* flat); a **`winner` level policy** (leaf level only when a module-move correction is attached, keeping the from-singletons piece source — web-NotreDame operator 1.92s → 1.02s at identical codelength); and a **correction gate on the whole repair** (base networks otherwise pay +7.0% on web-NotreDame for −0.0005%, because the scaffolding dwarfs the splits).

Bundled, bit-exact: the gated lambdas in `coarsenModules` and `refineHierarchy` no longer snapshot `m_hierLevels[0]` — ~50 MB of leaf CSR per gated step on web-NotreDame, twice per sweep.

**`-N1` is completely untouched** (0 attempts, 0 CPU delta, identical codelength): `updateBestResult` only materializes `bestTree` when `numTrials > 1`, so the hook is inert at single-trial — the single-trial cost rule is satisfied with no caveat.

**This and partial seeding partition the benchmark set with no overlap**, verified in both directions: with partial seeding disabled the split's three networks are bit-identical, and the split repair records **0 attempts** on web-NotreDame and science2001 against 11 attempts / 5 accepted on malaria.
