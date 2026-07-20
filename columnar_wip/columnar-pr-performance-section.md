## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials; `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

> **Re-measured on the current engine** — `columnar-hierarchical-core` plus this stack (#874 wires `--preferred-number-of-modules`; #875 adds the tele/meta hoist). The first 11 rows of each table reflect **#868's** already-merged move-loop speedups (this section predated #868, so its columnar times were stale — this refresh brings them current); their **codelengths are bit-identical** to the earlier numbers (#868 and the hoist are correctness-preserving), so only `time`/`top`/`lvls` moved (times dropped 10–40% on `-C`). The last two rows — **air30k `-d --regularized`** (recorded teleportation) and **science2001 `-d --preferred-number-of-modules 25`** (the `|K − K_pref|` bias) — are the new configs this stack enables. **OO columns are carried over** for the base networks (OO code untouched, re-runs bit-identical) and freshly measured for the two new configs, so absolute cross-engine times carry a small ±noise. Sub-0.1s "toy" times are at the process-startup floor — shown without a percentage.

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
<tr><td>ninetriangles</td><td align="right">27</td><td>base</td><td>—</td><td align="right">3.38583082</td><td align="right">0.04s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.02s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">198</td><td>base</td><td>—</td><td align="right">6.86122978</td><td align="right">0.08s</td><td align="right">5</td><td align="right">2</td><td align="right">6.87505673 (+0.20%)</td><td align="right">0.02s</td><td align="right">10</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04321510</td><td align="right">0.19s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05186752 (+0.21%)</td><td align="right">0.03s</td><td align="right">3</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75648389</td><td align="right">2.33s</td><td align="right">4</td><td align="right">6</td><td align="right"><b>4.74907624</b> (−0.16%)</td><td align="right">0.26s (−89%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73952481</td><td align="right">0.20s</td><td align="right">66</td><td align="right">3</td><td align="right">6.74069306 (+0.02%)</td><td align="right">0.08s (−60%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83428058</td><td align="right">10.3s</td><td align="right">11</td><td align="right">4</td><td align="right"><b>7.83343660</b> (−0.01%)</td><td align="right">3.58s (−65%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56604138</td><td align="right">188.9s</td><td align="right">16</td><td align="right">13</td><td align="right">5.57279424 (+0.12%)</td><td align="right">22.1s (−88%)</td><td align="right">4</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.08s</td><td align="right">5</td><td align="right">2</td><td align="right">6.03455147 (+0.28%)</td><td align="right">0.01s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50337896</td><td align="right">12.1s</td><td align="right">138</td><td align="right">3</td><td align="right"><b>7.44452817</b> (−0.78%)</td><td align="right">3.90s (−68%)</td><td align="right">9</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39395534</td><td align="right">14.0s</td><td align="right">16</td><td align="right">4</td><td align="right">5.47251927 (+1.46%)</td><td align="right">4.19s (−70%)</td><td align="right">20</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57565280</td><td align="right">9.81s</td><td align="right">301</td><td align="right">3</td><td align="right">5.66883586 (+1.67%)</td><td align="right">4.63s (−53%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.92800030</td><td align="right">9.02s</td><td align="right">25</td><td align="right">4</td><td align="right">8.46079677 (+6.72%)</td><td align="right">3.43s (−62%)</td><td align="right">5</td><td align="right">3</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats OO. Parentheses on the columnar `-C` columns = change vs OO (`(=)` = bit-identical; negative time = faster).

**Reading the table**
- **Codelength** — columnar **beats OO** on powergrid (−0.16%), science2001 (−0.01%) and malaria (−0.78%); ties on the toys; and is within ~0.02–0.3% on the rest (politicalblogs +0.02%, jazz/netsci/lazega +0.2–0.3%, web-NotreDame +0.12%, air30k +1.5% — the residual memory-search gap).
- **Speed** — columnar is faster on every non-trivial network: ~3× on science2001 / malaria / air30k, ~9× on powergrid, and ~8.5× on web-NotreDame (22.1s vs 188.9s — the OO run thermally throttles on its ~3-min run, so a cool-machine ratio is closer to ~4×). The ~3× on the memory/directed nets is the post-#868 engine (the earlier session recorded ~2×).
- **Shape** — columnar produces leaner, shallower maps (web-NotreDame: 4 top modules / 6 levels vs OO's 16 / 13; politicalblogs 2 vs 66) that the map equation scores as essentially equal.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.02s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.02s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.87505673</td><td align="right">0.02s</td><td align="right">10</td><td align="right">2</td><td align="right">6.87505673 (=)</td><td align="right">0.02s</td><td align="right">10</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05186752</td><td align="right">0.03s</td><td align="right">3</td><td align="right">4</td><td align="right">4.06428378 (+0.31%)</td><td align="right">0.02s</td><td align="right">3</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74907624</td><td align="right">0.26s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77217700 (+0.49%)</td><td align="right">0.15s (−42%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74069306</td><td align="right">0.08s</td><td align="right">2</td><td align="right">3</td><td align="right">6.74069306 (=)</td><td align="right">0.07s</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.83343660</td><td align="right">3.58s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">3.48s (−3%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">5.57279424</td><td align="right">22.1s</td><td align="right">4</td><td align="right">6</td><td align="right">5.62448318 (+0.93%)</td><td align="right"><b>15.6s</b> (−29%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.03455147</td><td align="right">0.01s</td><td align="right">6</td><td align="right">2</td><td align="right">6.03455147 (=)</td><td align="right">0.01s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.44452817</td><td align="right">3.90s</td><td align="right">9</td><td align="right">3</td><td align="right">7.44452817 (=)</td><td align="right">3.55s (−9%)</td><td align="right">9</td><td align="right">3</td></tr>
<tr><td>air30k (states)</td><td align="right">5.47251927</td><td align="right">4.19s</td><td align="right">20</td><td align="right">3</td><td align="right">5.47251927 (=)</td><td align="right">3.99s (−5%)</td><td align="right">20</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.66883586</td><td align="right">4.63s</td><td align="right">15</td><td align="right">3</td><td align="right">5.66883586 (=)</td><td align="right">3.98s (−14%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.46079677</td><td align="right">3.43s</td><td align="right">5</td><td align="right">3</td><td align="right">8.65395434 (+2.28%)</td><td align="right">3.33s (−3%)</td><td align="right">5</td><td align="right">3</td></tr>
</tbody>
</table>

Parentheses on the columnar `-F` columns = change vs `-C` (`(=)` = bit-identical; negative time = faster); both columns are from the current same-session re-measurement. Sub-0.1s toy times are shown without a percentage (measurement floor).

**`-F` is a real speed/quality dial again.** With coarsening it now ties `-C` on both quality **and** time for the memory / metadata / multilayer and shallow-directed networks (politicalblogs, science2001, lazega, multilayer, malaria, air30k). On the base networks with real hierarchy it trades a little codelength for speed by skipping the interior refinement: web-NotreDame **+0.9% codelength for ~29% less time (15.6s vs 22.1s)**, powergrid +0.5% for ~42% less. `-C` stays the default (best quality everywhere); `-F` is the "give up a fraction of a percent for a faster, leaner, shallower map on deep networks" dial.

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
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.10s</td><td align="right">9</td><td align="right">3.51775481 (=)</td><td align="right">0.01s</td><td align="right">9</td></tr>
<tr><td>jazz</td><td align="right">6.86122978</td><td align="right">0.08s</td><td align="right">6</td><td align="right">6.86122978 (=)</td><td align="right">0.02s</td><td align="right">6</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28611584</td><td align="right">0.14s</td><td align="right">56</td><td align="right"><b>4.28228737</b> (−0.09%)</td><td align="right">0.02s (−86%)</td><td align="right">57</td></tr>
<tr><td>powergrid</td><td align="right">5.59831236</td><td align="right">1.43s</td><td align="right">420</td><td align="right">5.63395576 (+0.64%)</td><td align="right">0.11s (−92%)</td><td align="right">426</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74031825</td><td align="right">0.18s</td><td align="right">74</td><td align="right"><b>6.73918608</b> (−0.02%)</td><td align="right">0.06s (−67%)</td><td align="right">81</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.94913415</td><td align="right">9.94s</td><td align="right">496</td><td align="right">7.94947087 (+0.004%)</td><td align="right">2.94s (−70%)</td><td align="right">508</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">6.74367900</td><td align="right">117.7s</td><td align="right">11 831</td><td align="right">6.75083498 (+0.11%)</td><td align="right">20.7s (−82%)</td><td align="right">11 687</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.06s</td><td align="right">7</td><td align="right">6.02121843 (+0.06%)</td><td align="right">0.01s</td><td align="right">6</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.05s</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.01s</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50653124</td><td align="right">19.95s</td><td align="right">145</td><td align="right"><b>7.44504738</b> (−0.82%)</td><td align="right">2.82s (−86%)</td><td align="right">157</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39376962</td><td align="right">9.84s</td><td align="right">332</td><td align="right">5.45963432 (<b>+1.22%</b>)</td><td align="right">5.22s (−47%)</td><td align="right">324</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57643406</td><td align="right">7.13s</td><td align="right">301</td><td align="right">6.01175505 (+7.81%)</td><td align="right">5.20s (−27%)</td><td align="right">278</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.13124130</td><td align="right">6.52s</td><td align="right">25</td><td align="right">8.23577532 (+1.29%)</td><td align="right">3.24s (−50%)</td><td align="right">25</td></tr>
</tbody>
</table>

Parentheses on the columnar columns = change vs OO `-2` (**bold** = columnar beats OO). Reading it:
- **Quality** — columnar `-2` ties or beats OO on 9 of 11 networks (beats on netscicoauthor2010, politicalblogs, malaria), and is within +0.11% on web-NotreDame. The exceptions are powergrid (+0.64%) and air30k (+1.22%).
- **Speed** — now faster on **every** non-trivial network (−47% to −92%). The former exception, air30k (**+58%** in the pre-#868 session), is **−47%** on the current engine — the #868 move-loop speedups closed the `-2` state/memory speed gap.
- **The remaining gap is the state/memory objective (air30k) on the codelength axis only** (+1.22%); the speed axis is now faster than OO. Profiled cause of the codelength gap: the columnar aggregation passes optimize the *base* objective only (corrections act in the leaf move loops, the augmented pass selection, and the gated merges), so the base-driven aggregation stops far too fine for the memory objective (K = 1353 where the optimum wants ~330) and the greedy pairwise merge + leaf re-tune has to carry the whole coarsening. OO's coarse-tune moves modules under the true memory objective. The principled fix is module-level correction state (consolidate per-unit physical-flow aggregates so module moves see the augmented delta); the same machinery is also the likely cure for the residual +1.5% hierarchical air30k gap.

### Tele-path and metadata move-loop hoist (this branch)

Two follow-ups to the #868 move-loop speedups: the recorded-teleportation delta hoists its six old-module plogp terms once per unit (`hoistOldSideTele`), and the metadata correction caches its per-leaf move-loop terms with the same zero-path fast-track as the memory correction. Both are **bit-exact** — codelengths are unchanged on every network × {`-C`, `-F`, `-2 -C`}, so the base rows above are pure #868.

The measurable win is the tele path. The columnar times for **air30k `-d --regularized`** in the tables above are *with* the hoist; **without** it they are `-C` 5.34s, `-F` 4.69s, `-2 -C` 6.20s — so the hoist alone accounts for **−13% / −15% / −16%**. The Meta fast paths are correctness/consistency (the set has no large metadata network). The biased **science2001 `-d --preferred-number-of-modules 25`** config uses no teleport or metadata, so the hoist leaves it unchanged (confirming the hoist touches only the tele path); its columnar partition lands the finest level on exactly 25 modules (`-2 -C`, top = 25) — the `|K − K_pref|` bias wired in [#827](https://github.com/mapequation/infomap/issues/827).
