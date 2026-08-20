## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials, minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. See [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details.

> **This is the snapshot for the flow-community regroup probe and the `-N1` winner-repair fix** — the
> memory objective's search could not reach optima that require merging ~100 flow-connected building
> blocks at once (a GROUP hysteresis: every pairwise step is uphill), so on state networks whose
> co-physical nodes are never directly linked it returned partitions 12–16% worse in codelength than a
> partition it could itself score (`networks/debug/Jelena`; found via the #1028 `-c` warm start). Mechanism and
> design history in `columnar_wip/columnar-rethink-notes.md` F42 (+addendum) / F43.
>
> Both changes are reachable only with a module-move-capable correction attached (Mem/Meta), the
> regroup ladder runs behind a rolled-back **detector** that escalates only on a win above 0.1% of the
> codelength, and the whole arm has a **1024-leaf size floor** (the hysteresis needs scale; without
> the floor the winner repair's ~140 sub-cluster detectors cost malaria `-C -N10` +5.2% in time for
> exactly 0 change in bits). A run the pathology does not touch is therefore unchanged: all **32**
> base-objective configurations (8 networks × `-C`/`-C -2` × `-N1`/`-N10`) produce **byte-identical
> tree bodies**, and every healthy memory/metadata row is **bit-identical in bits at `-N10`**
> (regularized air30k and air30k (meta) included). The four engine tables below therefore carry the
> previous snapshot's numbers unchanged; the PR's own effect is in the **old-vs-new columnar
> comparison tables**, both arms measured in this session, interleaved.
>
> **Old** = a fresh `MODE=release OPENMP=0` build of the `columnar-hierarchical-core` tip `1e537d8d`
> (the #1028 head), md5 `ff9fe3fe79518966bb7519bfb9388eec`; **new** = this PR, md5
> `0daa0aea46814211801af19dc824b6de`. Time = `--timing-json`'s `timing.total_s` (the engine's own
> wall, excluding ~30 ms process startup that would swamp the sub-0.1 s rows), min of 3 interleaved
> repetitions, desktop load ~5 of 10 cores (arms are paired, so the deltas hold).

### Old vs new columnar — standard search (`-C -N10`)

The jelena rows run `-C -d -N10` (directed state networks; the planted partitions score
6.902222527 / 6.930934993 bits under `--no-infomap`).

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">columnar <code>-C</code> (old)</th>
<th colspan="4">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.005s (=)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.002s (=)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.06s</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.11s (+1.9%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">3.89s</td><td align="right">22</td><td align="right">3</td><td align="right">5.39242541 (=)</td><td align="right">3.84s (-1.3%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.08s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57624241 (=)</td><td align="right">4.18s (+2.4%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.51s</td><td align="right">23</td><td align="right">3</td><td align="right">7.42215327 (=)</td><td align="right">9.79s (+2.9%)</td><td align="right">23</td><td align="right">3</td></tr>
<tr><td>jelena om5 (<code>-d</code>)</td><td align="right">7.82050090</td><td align="right">7.91s</td><td align="right">64</td><td align="right">4</td><td align="right"><b>6.86728259</b> (-12.19%)</td><td align="right">9.00s (+13.8%)</td><td align="right">300</td><td align="right">2</td></tr>
<tr><td>jelena om6 (<code>-d</code>)</td><td align="right">7.47142904</td><td align="right">8.32s</td><td align="right">90</td><td align="right">4</td><td align="right"><b>6.88727540</b> (-7.82%)</td><td align="right">9.09s (+9.2%)</td><td align="right">433</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Jelena rows run `-C -2d -N10`.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">columnar <code>-C -2</code> (old)</th>
<th colspan="4">columnar <code>-C -2</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.005s (=)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.002s (=)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.40044538</td><td align="right">2.67s</td><td align="right">168</td><td align="right">2</td><td align="right">7.40044538 (=)</td><td align="right">2.62s (-1.7%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39305505</td><td align="right">3.55s</td><td align="right">334</td><td align="right">2</td><td align="right">5.39305505 (=)</td><td align="right">3.63s (+2.2%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57557704</td><td align="right">3.68s</td><td align="right">304</td><td align="right">2</td><td align="right">5.57557704 (=)</td><td align="right">3.72s (+1.0%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42414371</td><td align="right">10.42s</td><td align="right">2237</td><td align="right">2</td><td align="right">7.42414371 (=)</td><td align="right">10.59s (+1.6%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td>jelena om5 (<code>-2d</code>)</td><td align="right">7.89383431</td><td align="right">17.37s</td><td align="right">996</td><td align="right">2</td><td align="right"><b>6.86728259</b> (-13.00%)</td><td align="right">8.10s (-53.4%)</td><td align="right">300</td><td align="right">2</td></tr>
<tr><td>jelena om6 (<code>-2d</code>)</td><td align="right">7.79029482</td><td align="right">10.08s</td><td align="right">3516</td><td align="right">2</td><td align="right"><b>6.88727540</b> (-11.59%)</td><td align="right">7.91s (-21.5%)</td><td align="right">433</td><td align="right">2</td></tr>
</tbody>
</table>

**Reading the comparison tables — every cell where new is worse than old, explained.** On the
pathological family the search escapes the wrong basin (−7.8% to −13.0% in bits) and `-2d -N10` gets
21.5–53.4% *faster* — searching from the right basin is cheaper than thrashing in the wrong one; the
hierarchical `-d -N10` rows pay +9.2%/+13.8% in time for the −7.8%/−12.2% in bits (the flat-first
trials that win now do the full escape work). On every healthy row the result is **bit-identical in
bits**; the time cells move −1.7% to +2.9%, inside the ±2.7% cross-session spread earlier snapshots
quote for unchanged binaries. The consistent small positives (malaria `-C` +1.9%, air30k `-2` +2.2%,
regularized `-C` +2.4%, meta `-C` +2.9% in time) are the per-trial detector — and on winners with
fewer than 64 top modules (malaria, meta) the direct escalation to the full ladder, whose candidates
all lose. If those ~2–3% in time matter, the next dial is raising the escalation floor's granularity
test; it is left simple because the deltas sit inside the session noise band.

### Single-trial runs (`-C -N1`): the winner repair now actually runs

The #889 once-per-run deep repair silently never ran at `-N1` (F43) — these rows pay it for the first
time, and it is the same repair every `-N10` run already paid. Jelena rows `-C -2d -N1`; the last two
rows seed the search with the planted partition (soft `-c`, #1028). Soft `-c` works without `-2` too:
`-C -d -N1 -c` returns the same partitions (om5 6.86279690, om6 6.87514579 bits).

| network | old bits | new bits | Δbits | old t | new t | Δt |
|---|--:|--:|--:|--:|--:|--:|
| lazega | 6.06055879 | **6.03455147** | −0.43% | 0.001s | 0.002s | +0.001s |
| malaria | 7.50222401 | **7.41249453** | −1.20% | 0.36s | 0.96s | +168% |
| air30k | 5.47323105 | 5.47323105 | = | 0.36s | 0.42s | +17.5% |
| air30k (reg.) | 5.66687303 | 5.66687303 | = | 0.46s | 0.54s | +16.9% |
| air30k (meta) | 7.58076831 | **7.58074898** | −0.0003% | 0.75s | 1.11s | +47.9% |
| jelena om5 `-2d` | 7.98918933 | **6.86728283** | −14.04% | 0.87s | 2.50s | +186% |
| jelena om6 `-2d` | 7.81047841 | **6.88646101** | −11.83% | 0.95s | 2.20s | +133% |
| jelena om5 `-2d -c` planted | 6.85777811 | 6.86279690 | +0.073% | 1.18s | 1.92s | +62.8% |
| jelena om6 `-2d -c` planted | 6.87337876 | 6.87514579 | +0.026% | 1.27s | 2.37s | +87.2% |

**Where new is worse than old:** the air30k family pays +17–48% in time for unchanged-to-−0.0003%
bits — that is the repair itself running once (its cost at `-N1` is the whole point of F43; the same
repair is amortized invisibly at `-N10`), and it is what buys malaria's −1.20% and lazega's −0.43% in
bits. The seeded `-c` rows pay +63–87% in time AND drift +0.026–0.073% in bits: the repair now runs
after `optimizeFromSeed`'s own polish and its differently-seeded move loops land nearby but not
identically; both results stay far below the planted input (6.902/6.931 bits). If that bothers,
the repair could be gated off soft-seeded runs — `optimizeFromSeed` already runs the same
split/retune/merge trio — at the price of diverging from pre-PR `-N10` seeded behaviour, where the
repair did run; flagged as a possible follow-up rather than changed here.

**Per-feature attribution, both axes** (`COL_REGROUP=off` disables the ladder and isolates the `-N1`
repair fix): the repair fix alone reproduces malaria's −1.20% and lazega's −0.43% in bits at the same
time cost as the full change (malaria 0.36 → 0.94s; the ladder adds ~0.02s there), leaves air30k at
5.47323105 bits for 0.36 → 0.43s, and moves jelena only to 7.89383431 / 7.79536908 bits — for om5 at
**10.3s**, because the repair's fresh splits thrash on the collapsed 1-module winner without the
ladder. The ladder provides the basin escape on both axes: om5 6.86728283 bits at 2.50s total, om6
6.88646101 bits at 2.20s. **Residual:** `-C -d -N1` (hierarchical, single trial) stays in the bad
basin — om5 7.88296837 bits (old 7.88481128, −0.02%) at 0.80 → 1.11s (+39%), om6 7.48049902 bits
(old 7.47937471, +0.015%) at 0.85 → 1.17s (+38%), the time being the repair trying and failing; the
fine-blocks bottom skips the probe by design and one trial has no flat-first arm. Any `-N2+` or `-2`
run is covered.

<details>
<summary><b>OO vs columnar (carried over unchanged; the PR leaves every cell bit-identical in bits)</b></summary>

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
<tr><td>ninetriangles</td><td align="right">27</td><td>base</td><td>—</td><td align="right">3.38583082</td><td align="right">0.02s</td><td align="right">3</td><td align="right">3</td><td align="right"><b>3.38583082</b> (-2.62e-14%)</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">198</td><td>base</td><td>—</td><td align="right">6.86304747</td><td align="right">0.03s</td><td align="right">5</td><td align="right">2</td><td align="right"><b>6.86275593</b> (-0.00425%)</td><td align="right">0.02s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04354934</td><td align="right">0.13s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05454025 (+0.272%)</td><td align="right">0.03s (-75%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75872920</td><td align="right">1.86s</td><td align="right">5</td><td align="right">7</td><td align="right"><b>4.74107206</b> (-0.371%)</td><td align="right">0.24s (-87%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73892798</td><td align="right">0.15s</td><td align="right">80</td><td align="right">3</td><td align="right">6.74094314 (+0.0299%)</td><td align="right">0.07s (-52%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83638921</td><td align="right">7.40s</td><td align="right">11</td><td align="right">4</td><td align="right"><b>7.83343660</b> (-0.0377%)</td><td align="right">3.03s (-59%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56592477</td><td align="right">137.6s</td><td align="right">17</td><td align="right">13</td><td align="right">5.56852929 (+0.0468%)</td><td align="right">18.89s (-86%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.02s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.02s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (-2.21e-14%)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50242050</td><td align="right">8.58s</td><td align="right">142</td><td align="right">3</td><td align="right"><b>7.39750171</b> (-1.4%)</td><td align="right">3.11s (-64%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39287115</td><td align="right">11.01s</td><td align="right">16</td><td align="right">4</td><td align="right"><b>5.39242541</b> (-0.00827%)</td><td align="right">3.64s (-67%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57843563</td><td align="right">7.28s</td><td align="right">301</td><td align="right">3</td><td align="right"><b>5.57624241</b> (-0.0393%)</td><td align="right">3.97s (-45%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.94035360</td><td align="right">7.72s</td><td align="right">25</td><td align="right">4</td><td align="right">8.23558553 (+3.72%)</td><td align="right">3.26s (-58%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">13 213</td><td>state/mem + meta</td><td><code>--meta-data …_usstate.meta</code></td><td align="right" colspan="4"><em>does not finish -N10 in 30 min; 8.43783233 / 5.4s at <code>-N1</code></em></td><td align="right">7.42215327</td><td align="right">8.90s</td><td align="right">23</td><td align="right">3</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats or exactly ties OO. Parentheses = change vs OO. Every codelength is unchanged from the previous snapshot — the PR leaves all fourteen configurations bit-identical in bits — and times carry over on that identity plus the noise-level time deltas in the comparison tables above.

</details>

<details>
<summary><b>The fast dial -F (carried over unchanged)</b></summary>

`-F` (`--fast-hierarchical-solution`) skips the interior-layer refinement in favour of a single bottom re-partition within grandparents plus the module-level coarsening loop. Measured as `-C -F`: **`-F` alone does not select the columnar engine**.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">columnar <code>-C</code></th>
<th colspan="4">columnar <code>-C -F</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td><td align="right"><b>3.38583082</b> (=)</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.02s</td><td align="right">6</td><td align="right">2</td><td align="right"><b>6.86275593</b> (=)</td><td align="right">0.01s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.03s</td><td align="right">2</td><td align="right">4</td><td align="right">4.06300588 (+0.209%)</td><td align="right">0.02s</td><td align="right">4</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.25s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77402224 (+0.695%)</td><td align="right">0.15s (-38%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.07s</td><td align="right">81</td><td align="right">2</td><td align="right"><b>6.74094314</b> (=)</td><td align="right">0.07s</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.03s</td><td align="right">15</td><td align="right">3</td><td align="right"><b>7.83343660</b> (=)</td><td align="right">2.96s</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">19.12s</td><td align="right">5</td><td align="right">6</td><td align="right">5.62506198 (+1.02%)</td><td align="right">14.37s (-25%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.02s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.16s</td><td align="right">2</td><td align="right">3</td><td align="right"><b>7.39750171</b> (=)</td><td align="right">3.10s</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">3.76s</td><td align="right">22</td><td align="right">3</td><td align="right"><b>5.39242541</b> (=)</td><td align="right">3.47s (-8%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.07s</td><td align="right">11</td><td align="right">3</td><td align="right"><b>5.57624241</b> (=)</td><td align="right">3.56s (-12%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.23558553</td><td align="right">3.35s</td><td align="right">25</td><td align="right">2</td><td align="right"><b>8.23558553</b> (=)</td><td align="right">3.27s</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.13s</td><td align="right">23</td><td align="right">3</td><td align="right"><b>7.42215327</b> (=)</td><td align="right">9.00s</td><td align="right">23</td><td align="right">3</td></tr>
</tbody>
</table>

`-F` is bit-identical to `-C` on 11 of 14 configurations; the dial bites only on the three deep-hierarchy base networks (in bits: web-NotreDame +1.02%, powergrid +0.70%, netsci +0.21%, for −25%/−38%/−31% in time). This PR leaves every `-F` cell bit-identical in bits.

</details>

<details>
<summary><b>Two-level -2 (carried over unchanged; one row added)</b></summary>

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">OO <code>-2</code></th>
<th colspan="4">columnar <code>-2 -C</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.01s</td><td align="right">9</td><td align="right">2</td><td align="right"><b>3.51775481</b> (-5.05e-14%)</td><td align="right">0.01s</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.02s</td><td align="right">5</td><td align="right">2</td><td align="right"><b>6.86122977</b> (-0.0265%)</td><td align="right">0.01s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28501267</td><td align="right">0.04s</td><td align="right">56</td><td align="right">2</td><td align="right"><b>4.28307258</b> (-0.0453%)</td><td align="right">0.02s</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td>powergrid</td><td align="right">5.60044386</td><td align="right">0.56s</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.658%)</td><td align="right">0.11s (-81%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73972141</td><td align="right">0.09s</td><td align="right">80</td><td align="right">2</td><td align="right"><b>6.73957529</b> (-0.00217%)</td><td align="right">0.05s</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.95003960</td><td align="right">3.68s</td><td align="right">496</td><td align="right">2</td><td align="right"><b>7.94997883</b> (-0.000764%)</td><td align="right">2.32s (-37%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td>web-NotreDame</td><td align="right">6.74298853</td><td align="right">40.39s</td><td align="right">11809</td><td align="right">2</td><td align="right">6.75421666 (+0.167%)</td><td align="right">17.07s (-58%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.02s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50595639</td><td align="right">6.21s</td><td align="right">142</td><td align="right">2</td><td align="right"><b>7.40044538</b> (-1.41%)</td><td align="right">2.69s (-57%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39331278</td><td align="right">4.29s</td><td align="right">332</td><td align="right">2</td><td align="right"><b>5.39305505</b> (-0.00478%)</td><td align="right">3.35s (-22%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57921689</td><td align="right">5.38s</td><td align="right">301</td><td align="right">2</td><td align="right"><b>5.57557704</b> (-0.0652%)</td><td align="right">3.52s (-34%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.13096953</td><td align="right">5.58s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (+1.29%)</td><td align="right">3.02s (-46%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right" colspan="4"><em>—</em></td><td align="right">7.42414371</td><td align="right">10.06s</td><td align="right">2237</td><td align="right">2</td></tr>
</tbody>
</table>

Columnar `-2` ties or beats OO on 10 of 13 pre-existing configurations, including every correction network; the three exceptions are base-objective configurations (in bits: powergrid +0.66%, web-NotreDame +0.17%, pref-mods +1.29%, all at less than half OO's time). Every cell is unchanged from the previous snapshot; the air30k (meta) `-2` value is quoted for the first time (identical on both binaries: 7.42414371 bits).

</details>

<details>
<summary><b>Non-redundant L* (carried over unchanged; verified bit-identical per row)</b></summary>

L\* is a **different objective**, not a better score for the same one, so its codelength cannot be read against base L in either direction. All 14 `--non-redundant` values (and their `-C` references) are bit-identical to the previous snapshot on this PR's binary, verified per row.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">columnar <code>-C</code></th>
<th colspan="4">columnar <code>-C --non-redundant</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td><td align="right"><b>3.07806732</b> (-9.09%)</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.02s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86822837 (+0.0797%)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.03s</td><td align="right">2</td><td align="right">4</td><td align="right"><b>3.89220976</b> (-4%)</td><td align="right">0.03s</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.24s</td><td align="right">5</td><td align="right">5</td><td align="right"><b>4.50926542</b> (-4.89%)</td><td align="right">0.24s</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.07s</td><td align="right">81</td><td align="right">2</td><td align="right">6.78924150 (+0.716%)</td><td align="right">0.07s</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.03s</td><td align="right">15</td><td align="right">3</td><td align="right">8.00917226 (+2.24%)</td><td align="right">2.76s (-9%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">18.89s</td><td align="right">5</td><td align="right">6</td><td align="right"><b>5.51707363</b> (-0.924%)</td><td align="right">18.76s</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>5.96862465</b> (-0.818%)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>1.92885658</b> (-4.1%)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.11s</td><td align="right">2</td><td align="right">3</td><td align="right">7.42757278 (+0.407%)</td><td align="right">3.08s</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">3.64s</td><td align="right">22</td><td align="right">3</td><td align="right"><b>5.37891261</b> (-0.251%)</td><td align="right">3.58s</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">3.97s</td><td align="right">11</td><td align="right">3</td><td align="right"><b>5.56887516</b> (-0.132%)</td><td align="right">3.87s</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.23558553</td><td align="right">3.26s</td><td align="right">25</td><td align="right">2</td><td align="right">8.44774545 (+2.58%)</td><td align="right">3.25s</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">8.90s</td><td align="right">23</td><td align="right">3</td><td align="right"><b>7.21529977</b> (-2.79%)</td><td align="right">7.71s (-13%)</td><td align="right">33</td><td align="right">3</td></tr>
</tbody>
</table>

</details>
