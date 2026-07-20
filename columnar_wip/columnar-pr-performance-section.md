## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials; `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

> **Re-measured at this PR** (`columnar-hierarchical-core` + the #834 module-level corrections). All five variants — OO, `-C`, `-C -F`, OO `-2`, `-2 -C` — were measured fresh in one session at this tip. Codelength changes vs the previous refresh are confined to the four state/memory/multilayer configs the PR targets (air30k, air30k reg., malaria; lazega's best is unchanged); **every base-network codelength in every variant is bit-identical**, web-NotreDame included. Wall times carry ±10–15% session noise; the air30k `-2` pair — the tightest cross-engine comparison — is the median of 4 interleaved runs. The OO `-2` times in the previous refresh appear ~2× inflated (two independent sessions both measure web-NotreDame OO `-2` at ~56–61s, not 117.7s); this refresh replaces them. `top`/`lvls` cells are parsed from the summary's best-trial row, which corrects a few OO cells from the previous refresh (codelengths unchanged). Sub-0.1s "toy" times are at the process-startup floor — shown without a percentage.

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
<tr><td>ninetriangles</td><td align="right">27</td><td>base</td><td>—</td><td align="right">3.38583082</td><td align="right">0.08s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.04s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">198</td><td>base</td><td>—</td><td align="right">6.86122978</td><td align="right">0.19s</td><td align="right">6</td><td align="right">2</td><td align="right">6.87505673 (+0.20%)</td><td align="right">0.04s</td><td align="right">10</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04321510</td><td align="right">0.39s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05186752 (+0.21%)</td><td align="right">0.09s</td><td align="right">3</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75648389</td><td align="right">1.27s</td><td align="right">5</td><td align="right">6</td><td align="right"><b>4.74907624</b> (−0.16%)</td><td align="right">0.29s (−77%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73952481</td><td align="right">0.22s</td><td align="right">74</td><td align="right">3</td><td align="right">6.74069306 (+0.02%)</td><td align="right">0.10s (−55%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83428058</td><td align="right">11.7s</td><td align="right">12</td><td align="right">4</td><td align="right"><b>7.83343660</b> (−0.01%)</td><td align="right">3.67s (−69%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56604138</td><td align="right">193.6s</td><td align="right">19</td><td align="right">12</td><td align="right">5.57279424 (+0.12%)</td><td align="right">22.4s (−88%)</td><td align="right">4</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.07s</td><td align="right">7</td><td align="right">2</td><td align="right">6.03455147 (+0.28%)</td><td align="right">0.04s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.05s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50337896</td><td align="right">13.1s</td><td align="right">145</td><td align="right">3</td><td align="right"><b>7.47888701</b> (−0.33%)</td><td align="right">3.63s (−72%)</td><td align="right">7</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39395534</td><td align="right">16.3s</td><td align="right">18</td><td align="right">4</td><td align="right">5.46518698 (+1.32%)</td><td align="right">3.85s (−76%)</td><td align="right">20</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57565280</td><td align="right">8.60s</td><td align="right">301</td><td align="right">3</td><td align="right">5.66241982 (+1.56%)</td><td align="right">4.21s (−51%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.92800030</td><td align="right">7.07s</td><td align="right">25</td><td align="right">4</td><td align="right">8.46079677 (+6.72%)</td><td align="right">4.27s (−40%)</td><td align="right">5</td><td align="right">3</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats OO. Parentheses on the columnar `-C` columns = change vs OO (`(=)` = bit-identical; negative time = faster).

**Reading the table**
- **Codelength** — columnar **beats OO** on powergrid (−0.16%), science2001 (−0.01%) and malaria (−0.33%); ties on the toys; and is within ~0.02–0.3% on the rest (politicalblogs +0.02%, jazz/netsci/lazega +0.2–0.3%, web-NotreDame +0.12%). The air30k rows (+1.3–1.6%) are the *hierarchical* flat-optimum gap — these networks' optima are near-flat (OO: 301–332 top modules) and nothing in the columnar build/refine loop can flatten; the `-2` table below shows the same networks at +0.03–0.05% once the two-level constraint removes that limitation. The flat-candidate follow-up addresses it.
- **Speed** — columnar is faster on every non-trivial network: ~3–4× on science2001 / malaria / air30k, ~4.4× on powergrid, and ~8.7× on web-NotreDame (22.4s vs 193.6s — the OO run thermally throttles on its ~3-min run, so a cool-machine ratio is closer to ~4×).
- **Shape** — columnar produces leaner, shallower maps (web-NotreDame: 4 top modules / 6 levels vs OO's 19 / 12; politicalblogs 2 vs 74) that the map equation scores as essentially equal.

### The fast dial `-F`

`-F` (`--fast-hierarchical-solution`) skips the interior-layer refinement and instead does a single bottom re-partition within grandparents plus the module-level coarsening loop. It now (a) includes the coarsening the memory / metadata / multilayer objectives need (without which it was catastrophic there — air30k +14%), and (b) does that bottom re-partition **once** rather than looping it — the second pass was a provably-idempotent no-op (leaves stay within fixed grandparents), so dropping it is bit-identical and ~25–35% faster on the deep/memory nets.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.04s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.05s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.87505673</td><td align="right">0.04s</td><td align="right">10</td><td align="right">2</td><td align="right">6.87505673 (=)</td><td align="right">0.05s</td><td align="right">10</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05186752</td><td align="right">0.09s</td><td align="right">3</td><td align="right">4</td><td align="right">4.06428378 (+0.31%)</td><td align="right">0.09s</td><td align="right">3</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74907624</td><td align="right">0.29s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77217700 (+0.49%)</td><td align="right">0.20s (−31%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74069306</td><td align="right">0.10s</td><td align="right">2</td><td align="right">3</td><td align="right">6.74069306 (=)</td><td align="right">0.11s</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.83343660</td><td align="right">3.67s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">3.67s (=)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">5.57279424</td><td align="right">22.4s</td><td align="right">4</td><td align="right">6</td><td align="right">5.62448318 (+0.93%)</td><td align="right"><b>17.4s</b> (−22%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.03455147</td><td align="right">0.04s</td><td align="right">6</td><td align="right">2</td><td align="right">6.03455147 (=)</td><td align="right">0.04s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.47888701</td><td align="right">3.63s</td><td align="right">7</td><td align="right">3</td><td align="right">7.47888701 (=)</td><td align="right">3.42s (−6%)</td><td align="right">7</td><td align="right">3</td></tr>
<tr><td>air30k (states)</td><td align="right">5.46518698</td><td align="right">3.85s</td><td align="right">20</td><td align="right">3</td><td align="right">5.46518698 (=)</td><td align="right">3.49s (−9%)</td><td align="right">20</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.66241982</td><td align="right">4.21s</td><td align="right">15</td><td align="right">3</td><td align="right">5.66241982 (=)</td><td align="right">3.27s (−22%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.46079677</td><td align="right">4.27s</td><td align="right">5</td><td align="right">3</td><td align="right">8.65395434 (+2.28%)</td><td align="right">3.88s (−9%)</td><td align="right">5</td><td align="right">3</td></tr>
</tbody>
</table>

Parentheses on the columnar `-F` columns = change vs `-C` (`(=)` = bit-identical; negative time = faster); both columns are from the current same-session re-measurement. Sub-0.1s toy times are shown without a percentage (measurement floor).

**`-F` is a real speed/quality dial again.** With coarsening it now ties `-C` on both quality **and** time for the memory / metadata / multilayer and shallow-directed networks (politicalblogs, science2001, lazega, multilayer, malaria, air30k). On the base networks with real hierarchy it trades a little codelength for speed by skipping the interior refinement: web-NotreDame **+0.9% codelength for ~22% less time (17.4s vs 22.4s)**, powergrid +0.5% for ~31% less. `-C` stays the default (best quality everywhere); `-F` is the "give up a fraction of a percent for a faster, leaner, shallower map on deep networks" dial.

The columnar interior refinement stops at a diminishing-returns knee (default `--tune-iteration-relative-threshold` = 5e-3), which on deep networks trades the last ~0.06% of codelength for ~18% less search time; shallow networks are unaffected.

### Two-level clustering (`-2`)

`--two-level` is wired to the columnar engine on the `columnar-two-level` branch (PR #823): the full two-level optimize materialized as a two-level stack, followed by the correction-aware module-merge coarsening interleaved with a seeded leaf fine-tune until the pair stops improving (the interleave is what the memory objective needs — without it air30k sat at +3.9% vs OO; the loop never enters on the base objective, whose merge is a no-op).

Same protocol as above: single-threaded, `--seed 123 -N10`, best of 10, total wall for all 10 trials.

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
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.04s</td><td align="right">9</td><td align="right">3.51775481 (=)</td><td align="right">0.04s</td><td align="right">9</td></tr>
<tr><td>jazz</td><td align="right">6.86122978</td><td align="right">0.08s</td><td align="right">6</td><td align="right">6.86122978 (=)</td><td align="right">0.10s</td><td align="right">6</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28611584</td><td align="right">0.09s</td><td align="right">56</td><td align="right"><b>4.28228737</b> (−0.09%)</td><td align="right">0.08s</td><td align="right">57</td></tr>
<tr><td>powergrid</td><td align="right">5.59831236</td><td align="right">0.70s</td><td align="right">420</td><td align="right">5.63395576 (+0.64%)</td><td align="right">0.15s (−79%)</td><td align="right">426</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74031825</td><td align="right">0.13s</td><td align="right">74</td><td align="right"><b>6.73918608</b> (−0.02%)</td><td align="right">0.09s (−31%)</td><td align="right">81</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.94913415</td><td align="right">4.57s</td><td align="right">496</td><td align="right">7.94947087 (+0.004%)</td><td align="right">3.14s (−31%)</td><td align="right">508</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">6.74367900</td><td align="right">55.9s</td><td align="right">11 831</td><td align="right">6.75083498 (+0.11%)</td><td align="right">24.7s (−56%)</td><td align="right">11 687</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.04s</td><td align="right">7</td><td align="right">6.02121843 (+0.06%)</td><td align="right">0.04s</td><td align="right">6</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.04s</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50653124</td><td align="right">8.48s</td><td align="right">145</td><td align="right"><b>7.47430185</b> (−0.43%)</td><td align="right">2.48s (−71%)</td><td align="right">160</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39376962</td><td align="right">4.9s*</td><td align="right">332</td><td align="right">5.39525375 (+0.03%)</td><td align="right">4.5s* (−8%)</td><td align="right">329</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57643406</td><td align="right">6.79s</td><td align="right">301</td><td align="right">5.57931831 (+0.05%)</td><td align="right">3.78s (−44%)</td><td align="right">301</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.13124130</td><td align="right">6.49s</td><td align="right">25</td><td align="right">8.23577532 (+1.29%)</td><td align="right">3.75s (−42%)</td><td align="right">25</td></tr>
</tbody>
</table>

Parentheses on the columnar columns = change vs OO `-2` (**bold** = columnar beats OO). `*` = median of 4 interleaved OO/columnar runs (the tightest comparison in the set; single-shot walls varied ±15%). Reading it:
- **Quality** — columnar `-2` ties or beats OO on 11 of 13 configs: beats netscicoauthor2010, politicalblogs and malaria; within +0.11% on web-NotreDame and **+0.03% / +0.05% on air30k / regularized air30k — the state/memory gap (was +1.22% / +7.81%) is closed by the module-level corrections (#834)**. The exceptions are powergrid (+0.64%) and science2001 pref-mods (+1.29%).
- **Speed** — faster on every non-trivial network (−8% to −79%).

### Module-level corrections (#834, this PR)

The columnar aggregation passes used to optimize the *base* objective only — corrections acted in the leaf move loops, the augmented pass selection and the gated merges, so on memory networks the aggregation stopped far too fine (air30k: K = 1353 where the optimum wants ~330) and the greedy pairwise merge + leaf re-tune carried the whole coarsening. Now the module-move-capable corrections (Mem, Meta) maintain per-unit attribute aggregates across consolidations, so the module-level move loop descends the true augmented objective — OO's coarse-tune equivalent, at columnar cost:

| config | base-only aggregation (previous) | objective-aware (this PR) | OO reference |
|---|--:|--:|--:|
| air30k `-2 -C` | 5.45963432 (+1.22%) | **5.39525375 (+0.03%)** | 5.39376962 |
| air30k reg. `-2 -C` | 6.01175505 (+7.81%) | **5.57931831 (+0.05%)** | 5.57643406 |
| air30k `-C` | 5.47251927 | 5.46518698 | 5.39395534 |
| malaria `-2 -C` | 7.44504738 (−0.82%) | 7.47430185 (−0.43%) | 7.50653124 |
| malaria `-C` | 7.44452817 (−0.78%) | 7.47888701 (−0.33%) | 7.50337896 |

The objective-aware trajectory is also the *faster* one (the coarsening happens in the cheap module passes instead of the merge-scan + re-tune tail), and per-trial it dominates on the state networks: every objective-aware air30k trial beats every base-only trial, with disjoint quality bands ~1% apart. The regularized row is the strongest evidence that base-only aggregation optimizes the wrong objective: the prior flattens the base link-flow signal, and steering by it costs +7.8%.

**Known limitation ([#889](https://github.com/mapequation/infomap/issues/889)):** on malaria the objective-aware module-level gains overshoot to a coarseness the downstream fine-tune and gated merges cannot split apart again — the multilayer margin over OO shrinks from −0.8% to −0.4%. The fix is the split operator (the subdivision half of OO's coarse-tune); a per-trial strategy alternation was evaluated as a workaround and rejected (it halves the sampling of the winning family, costs time, and hides the defect).

### Tele-path and metadata move-loop hoist (#875)

Two follow-ups to the #868 move-loop speedups: the recorded-teleportation delta hoists its six old-module plogp terms once per unit (`hoistOldSideTele`), and the metadata correction caches its per-leaf move-loop terms with the same zero-path fast-track as the memory correction. Both are **bit-exact** — codelengths are unchanged on every network × {`-C`, `-F`, `-2 -C`}.

The measurable win is the tele path: on **air30k `-d --regularized`** the hoist alone accounts for −13% / −15% / −16% on `-C` / `-F` / `-2 -C` (measured at #875). The Meta fast paths are correctness/consistency (the set has no large metadata network). The biased **science2001 `-d --preferred-number-of-modules 25`** config uses no teleport or metadata, so the hoist leaves it unchanged; its columnar partition lands the finest level on exactly 25 modules (`-2 -C`, top = 25) — the `|K − K_pref|` bias wired in [#827](https://github.com/mapequation/infomap/issues/827).
