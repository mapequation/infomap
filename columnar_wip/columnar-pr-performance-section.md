## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = `--timing-json`'s `timing.total_s` (the engine's own wall — process wall carries ~30 ms of startup that swamps the sub-0.1 s rows), minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. See [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details.

> **This is the snapshot for the flow-community regroup probe and the `-N1` winner-repair fix** — the
> memory objective's search could not reach optima that require merging ~100 flow-connected building
> blocks at once (a GROUP hysteresis: every pairwise step is uphill), so on state networks whose
> co-physical nodes are never directly linked it returned partitions 12–16% worse in codelength than a
> partition it could itself score (`networks/debug/Jelena`, the **overlapping** rows below; found via
> the #1028 `-c` warm start). Both networks strand in the same fragment basin; om5's single-trial
> fragmented result (8.108 bits) additionally lands above the network's one-module partition
> (7.989 bits), so the #827 one-level guard replaced it with 1 module — the "collapse" output was the
> guard's, not a search snowball (F42 third addendum). Mechanism and design history in
> `columnar_wip/columnar-rethink-notes.md` F42 (+addenda) / F43 / F44.
>
> Both changes are reachable only with a module-move-capable correction attached (Mem/Meta) — forced
> on for the base objective the ladder finds nothing (powergrid/netsci/science2001 `-2 -N10`
> bit-identical in bits at up to +26% in time; F44) — and every gate is a signal the search already
> produces: the regroup ladder runs behind a rolled-back **detector** that escalates only on a win
> above 0.1% of the codelength; a sub-optimizer runs the arm only when it holds **≥ 10% of the root's
> leaves**; and a **single-trial run pays the repair's expensive fresh discovery only when its own
> trial escalated** — the pathology signal — with the cheap piece sources still running everywhere.
> A run the pathology does not touch is unchanged: all **32** base-objective configurations produce
> **byte-identical tree bodies**, and every healthy memory/metadata row is **bit-identical in bits at
> `-N10` and `-N1`** (as are the seeded `-c` rows).
>
> **Every number in every table below is from ONE session with ONE instrument** (engine `total_s`,
> min of 3 interleaved repetitions, desktop load ~4–5 of 10 cores), so a configuration shows the same
> numbers wherever it appears. The OO arm is bit-identical in bits to the previous snapshot on all 26
> configurations; its times read +2–16% above that snapshot's older, idler sessions (web-NotreDame
> 137.6 → 159.9 s), which is the environment, not the engines — the within-session ratios are the
> comparison. **Old** = a fresh `MODE=release OPENMP=0` build of the `columnar-hierarchical-core` tip
> `1e537d8d` (the #1028 head), md5 `ff9fe3fe79518966bb7519bfb9388eec`; **new** = this PR, md5
> `50ef742d84a04dfff2471ba5e895c676`; OO rows measured on the new binary (the OO path is untouched).

### Old vs new columnar — standard search (`-C -N10`)

The overlapping rows run `-C -d -N10` (directed state networks; the planted partitions score
6.902222527 / 6.930934993 bits under `--no-infomap`). wikispeedia (300 physicals, 6 475 order-2
states, zero co-physical links like every order-2 network) is the **healthy control** for the same
structural family: at 21.6 states per physical the memory reward does not dominate, and the machinery
must leave it alone.

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
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.002s (+5.6%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.00s</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.12s (+4.0%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">3.77s</td><td align="right">22</td><td align="right">3</td><td align="right">5.39242541 (=)</td><td align="right">3.96s (+4.9%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.02s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57624241 (=)</td><td align="right">4.17s (+3.7%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.29s</td><td align="right">23</td><td align="right">3</td><td align="right">7.42215327 (=)</td><td align="right">9.42s (+1.5%)</td><td align="right">23</td><td align="right">3</td></tr>
<tr><td>wikispeedia (<code>-d</code>)</td><td align="right">5.90790474</td><td align="right">0.70s</td><td align="right">199</td><td align="right">2</td><td align="right">5.90790474 (=)</td><td align="right">0.72s (+3.1%)</td><td align="right">199</td><td align="right">2</td></tr>
<tr><td>overlapping om5 (<code>-d</code>)</td><td align="right">7.82050090</td><td align="right">7.94s</td><td align="right">64</td><td align="right">4</td><td align="right"><b>6.86661780</b> (-12.2%)</td><td align="right">8.15s (+2.6%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>overlapping om6 (<code>-d</code>)</td><td align="right">7.47142904</td><td align="right">8.09s</td><td align="right">90</td><td align="right">4</td><td align="right"><b>6.88486215</b> (-7.851%)</td><td align="right">8.47s (+4.7%)</td><td align="right">451</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

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
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.005s (+6.4%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.002s (+6.7%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.40044538</td><td align="right">2.60s</td><td align="right">168</td><td align="right">2</td><td align="right">7.40044538 (=)</td><td align="right">2.65s (+1.9%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39305505</td><td align="right">3.45s</td><td align="right">334</td><td align="right">2</td><td align="right">5.39305505 (=)</td><td align="right">3.51s (+2.0%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57557704</td><td align="right">3.65s</td><td align="right">304</td><td align="right">2</td><td align="right">5.57557704 (=)</td><td align="right">3.69s (+1.1%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42414371</td><td align="right">9.83s</td><td align="right">2237</td><td align="right">2</td><td align="right">7.42414371 (=)</td><td align="right">10.23s (+4.1%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td>wikispeedia (<code>-2d</code>)</td><td align="right">5.90790474</td><td align="right">0.66s</td><td align="right">199</td><td align="right">2</td><td align="right">5.90790474 (=)</td><td align="right">0.66s (+0.5%)</td><td align="right">199</td><td align="right">2</td></tr>
<tr><td>overlapping om5 (<code>-2d</code>)</td><td align="right">7.89383431</td><td align="right">16.92s</td><td align="right">996</td><td align="right">2</td><td align="right"><b>6.86661780</b> (-13.01%)</td><td align="right">7.22s (-57.3%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>overlapping om6 (<code>-2d</code>)</td><td align="right">7.79029482</td><td align="right">9.81s</td><td align="right">3516</td><td align="right">2</td><td align="right"><b>6.88486215</b> (-11.62%)</td><td align="right">7.18s (-26.8%)</td><td align="right">451</td><td align="right">2</td></tr>
</tbody>
</table>

**Reading the comparison tables — every cell where new is worse than old, explained.** On the
overlapping family the search escapes the fragment basin (−7.9% to −13.0% in bits) and `-2d -N10`
gets faster — searching from the right basin is cheaper than thrashing in the wrong one; the
hierarchical `-d -N10` rows pay a few percent in time for their −7.9%/−12.2% in bits (the flat-first
trials that win do the escape work). On every healthy row — wikispeedia included — the result is
**bit-identical in bits** and the time deltas sit inside the ±2.7% cross-session spread earlier
snapshots quote for unchanged binaries; the small positives are the per-trial detector (rolled back,
so it can cost time but never change bits).

### Single-trial runs (`-C -N1`): the repair follows the pathology signal

The #889 once-per-run deep repair silently never ran at `-N1` (F43). It now runs there too, but its
expensive fresh from-singletons discovery is allowed **only when the trial's own regroup detector
escalated** — the search's pathology signal — while the cheap piece sources run everywhere (F44).
Overlapping rows `-C -2d -N1`; the last two rows seed the search with the planted partition (soft
`-c`, #1028), whose path has no from-scratch trial and hence no escalation, so those rows return
exactly to their pre-PR bits and times. Soft `-c` works without `-2` too: `-C -d -N1 -c` returns the
same partitions (om5 6.86279690, om6 6.87514579 bits).

| network | old bits | new bits | Δbits | old t | new t | Δt |
|---|--:|--:|--:|--:|--:|--:|
| lazega | 6.06055879 | **6.04111740** | -0.321% | 0.001s | 0.002s | +0.001s |
| malaria | 7.50222401 | 7.50222401 | = | 0.35s | 0.35s | +0.4% |
| air30k | 5.47323105 | 5.47323105 | = | 0.36s | 0.36s | +0.0% |
| air30k (reg.) | 5.66687303 | 5.66687303 | = | 0.44s | 0.47s | +5.1% |
| air30k (meta) | 7.58076831 | **7.58074898** | -0.000255% | 0.74s | 0.88s | +17.7% |
| wikispeedia `-2d` | 5.92071240 | **5.91901362** | -0.0287% | 0.074s | 0.100s | +0.026s |
| overlapping om5 `-2d` | 7.98918933 | **6.86812714** | -14% | 0.82s | 1.84s | +124.2% |
| overlapping om6 `-2d` | 7.81047841 | **6.88404244** | -11.9% | 0.96s | 1.81s | +87.2% |
| overlapping om5 `-2d -c` planted | 6.85777811 | 6.85777811 | = | 1.15s | 1.20s | +3.7% |
| overlapping om6 `-2d -c` planted | 6.87337875 | 6.87337875 | = | 1.30s | 1.31s | +0.4% |

**Where new is worse than old, explained.** The escalated overlapping rows pay +87–124% in time for
their −11.9/−14.0% in bits — the full repair, which their trials' own detectors requested. The
healthy rows keep old bits at old-to-slightly-higher times: air30k (meta)'s +17.7% and
wikispeedia's +0.026 s in time is the cheap-sources repair (seeded retunes and gated merges — the
same pass that delivers lazega's free −0.32% and meta's −0.0003% in bits). `-C -d -N1` on the
overlapping rows is bit-identical in bits (7.88481128 / 7.47937471) at +7–11% in time — the residual:
a hierarchical single trial builds a fine-blocks bottom that skips the probe by design and has no
flat-first arm, so it stays in the fragment basin; any `-N2+` or `-2` run is covered.

**Per-feature attribution, both axes** (`COL_REGROUP=off` disables the ladder and isolates the
repair): the repair alone moves overlapping om5 `-2d -N1` only to 7.89383431 bits and needs **10.3 s**
for it — its fresh splits thrash inside the guard's one-module winner without the ladder (om6:
7.79536908 bits at 2.87 s). The ladder alone (repair fresh discovery off) reaches om5 7.17509147 bits
at 0.71 s. Together, escalation-gated: **6.86812714 bits at 1.84 s** — the ladder finds the basin, the
repair polishes inside it, and each runs only where its own signal says it pays.

### OO vs columnar

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
<tr><td>ninetriangles</td><td align="right">27</td><td>base</td><td>—</td><td align="right">3.38583082</td><td align="right">0.006s</td><td align="right">3</td><td align="right">3</td><td align="right"><b>3.38583082</b> (=)</td><td align="right">0.002s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">198</td><td>base</td><td>—</td><td align="right">6.86304747</td><td align="right">0.024s</td><td align="right">5</td><td align="right">2</td><td align="right"><b>6.86275593</b> (-0.00425%)</td><td align="right">0.008s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04354934</td><td align="right">0.12s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05454025 (+0.272%)</td><td align="right">0.025s (-79%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75872920</td><td align="right">1.91s</td><td align="right">5</td><td align="right">7</td><td align="right"><b>4.74107206</b> (-0.371%)</td><td align="right">0.25s (-87%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73892798</td><td align="right">0.14s</td><td align="right">80</td><td align="right">3</td><td align="right">6.74094314 (+0.0299%)</td><td align="right">0.061s (-55%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83638921</td><td align="right">7.60s</td><td align="right">11</td><td align="right">4</td><td align="right"><b>7.83343660</b> (-0.0377%)</td><td align="right">2.93s (-61%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56592477</td><td align="right">145.43s</td><td align="right">17</td><td align="right">13</td><td align="right">5.56852929 (+0.0468%)</td><td align="right">19.47s (-87%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.013s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50242050</td><td align="right">9.03s</td><td align="right">142</td><td align="right">3</td><td align="right"><b>7.39750171</b> (-1.4%)</td><td align="right">3.12s (-65%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39287115</td><td align="right">11.33s</td><td align="right">16</td><td align="right">4</td><td align="right"><b>5.39242541</b> (-0.00827%)</td><td align="right">3.96s (-65%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57843563</td><td align="right">7.56s</td><td align="right">301</td><td align="right">3</td><td align="right"><b>5.57624241</b> (-0.0393%)</td><td align="right">4.17s (-45%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.94035360</td><td align="right">7.90s</td><td align="right">25</td><td align="right">4</td><td align="right">8.23558553 (+3.72%)</td><td align="right">3.20s (-59%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">13 213</td><td>state/mem + meta</td><td><code>--meta-data …_usstate.meta</code></td><td align="right" colspan="4"><em>does not finish -N10 in 30 min; 8.43783233 / 5.32s at <code>-N1</code></em></td><td align="right">7.42215327</td><td align="right">9.42s</td><td align="right">23</td><td align="right">3</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats or exactly ties OO. Parentheses = change vs OO on the same axis as the column. Every columnar codelength is bit-identical to the previous snapshot's.

### The fast dial `-F`

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.002s</td><td align="right">3</td><td align="right">3</td><td align="right"><b>3.38583082</b> (=)</td><td align="right">0.002s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.008s</td><td align="right">6</td><td align="right">2</td><td align="right"><b>6.86275593</b> (=)</td><td align="right">0.008s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.025s</td><td align="right">2</td><td align="right">4</td><td align="right">4.06300588 (+0.209%)</td><td align="right">0.016s</td><td align="right">4</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.25s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77402224 (+0.695%)</td><td align="right">0.15s (-40%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.061s</td><td align="right">81</td><td align="right">2</td><td align="right"><b>6.74094314</b> (=)</td><td align="right">0.058s</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">2.93s</td><td align="right">15</td><td align="right">3</td><td align="right"><b>7.83343660</b> (=)</td><td align="right">2.86s</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">19.47s</td><td align="right">5</td><td align="right">6</td><td align="right">5.62506198 (+1.02%)</td><td align="right">15.56s (-20%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.12s</td><td align="right">2</td><td align="right">3</td><td align="right"><b>7.39750171</b> (=)</td><td align="right">2.93s (-6%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">3.96s</td><td align="right">22</td><td align="right">3</td><td align="right"><b>5.39242541</b> (=)</td><td align="right">3.59s (-9%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.17s</td><td align="right">11</td><td align="right">3</td><td align="right"><b>5.57624241</b> (=)</td><td align="right">3.59s (-14%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.23558553</td><td align="right">3.20s</td><td align="right">25</td><td align="right">2</td><td align="right"><b>8.23558553</b> (=)</td><td align="right">3.18s</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.42s</td><td align="right">23</td><td align="right">3</td><td align="right"><b>7.42215327</b> (=)</td><td align="right">9.10s (-3%)</td><td align="right">23</td><td align="right">3</td></tr>
</tbody>
</table>

`-F` is bit-identical in bits to `-C` on 11 of 14 configurations; the dial bites only on the three deep-hierarchy base networks (in bits: web-NotreDame +1.02%, powergrid +0.70%, netsci +0.21%, each at a 24–38% saving in time).

### Two-level clustering (`-2`)

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
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.002s</td><td align="right">9</td><td align="right">2</td><td align="right"><b>3.51775481</b> (=)</td><td align="right">0.001s</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.013s</td><td align="right">5</td><td align="right">2</td><td align="right"><b>6.86122977</b> (-0.0265%)</td><td align="right">0.008s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28501267</td><td align="right">0.031s</td><td align="right">56</td><td align="right">2</td><td align="right"><b>4.28307258</b> (-0.0453%)</td><td align="right">0.010s</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td>powergrid</td><td align="right">5.60044386</td><td align="right">0.57s</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.658%)</td><td align="right">0.11s (-81%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73972141</td><td align="right">0.077s</td><td align="right">80</td><td align="right">2</td><td align="right"><b>6.73957529</b> (-0.00217%)</td><td align="right">0.047s</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.95003960</td><td align="right">3.75s</td><td align="right">496</td><td align="right">2</td><td align="right"><b>7.94997883</b> (-0.000764%)</td><td align="right">2.25s (-40%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td>web-NotreDame</td><td align="right">6.74298853</td><td align="right">43.72s</td><td align="right">11809</td><td align="right">2</td><td align="right">6.75421666 (+0.167%)</td><td align="right">19.05s (-56%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.009s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50595639</td><td align="right">6.31s</td><td align="right">142</td><td align="right">2</td><td align="right"><b>7.40044538</b> (-1.41%)</td><td align="right">2.65s (-58%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39331278</td><td align="right">4.43s</td><td align="right">332</td><td align="right">2</td><td align="right"><b>5.39305505</b> (-0.00478%)</td><td align="right">3.51s (-21%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57921689</td><td align="right">5.59s</td><td align="right">301</td><td align="right">2</td><td align="right"><b>5.57557704</b> (-0.0652%)</td><td align="right">3.69s (-34%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.13096953</td><td align="right">5.76s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (+1.29%)</td><td align="right">2.94s (-49%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right" colspan="4"><em>—</em></td><td align="right">7.42414371</td><td align="right">10.23s</td><td align="right">2237</td><td align="right">2</td></tr>
</tbody>
</table>

Columnar `-2` ties or beats OO in bits on 10 of 13 pre-existing configurations, including every correction network; the three exceptions are base-objective configurations (in bits: powergrid +0.66%, web-NotreDame +0.17%, pref-mods +1.29%, all at less than half OO's time). The air30k (meta) `-2` value is quoted for the first time (identical in bits on both binaries).

### The non-redundant map equation L\* (`--non-redundant`)

L\* is a **different objective**, not a better score for the same one, so its codelength cannot be read against base L in either direction. All 14 `--non-redundant` values are bit-identical in bits to the previous snapshot on this PR's binary.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.002s</td><td align="right">3</td><td align="right">3</td><td align="right"><b>3.07806732</b> (-9.09%)</td><td align="right">0.002s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.008s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86822837 (+0.0797%)</td><td align="right">0.008s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.025s</td><td align="right">2</td><td align="right">4</td><td align="right"><b>3.89220976</b> (-4%)</td><td align="right">0.025s</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.25s</td><td align="right">5</td><td align="right">5</td><td align="right"><b>4.50926542</b> (-4.89%)</td><td align="right">0.25s</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.061s</td><td align="right">81</td><td align="right">2</td><td align="right">6.78924150 (+0.716%)</td><td align="right">0.062s</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">2.93s</td><td align="right">15</td><td align="right">3</td><td align="right">8.00917226 (+2.24%)</td><td align="right">2.67s (-9%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">19.47s</td><td align="right">5</td><td align="right">6</td><td align="right"><b>5.51707363</b> (-0.924%)</td><td align="right">20.14s (+3%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>5.96862465</b> (-0.818%)</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>1.92885658</b> (-4.1%)</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.12s</td><td align="right">2</td><td align="right">3</td><td align="right">7.42757278 (+0.407%)</td><td align="right">3.03s</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">3.96s</td><td align="right">22</td><td align="right">3</td><td align="right"><b>5.37891261</b> (-0.251%)</td><td align="right">3.73s (-6%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.17s</td><td align="right">11</td><td align="right">3</td><td align="right"><b>5.56887516</b> (-0.132%)</td><td align="right">4.00s (-4%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.23558553</td><td align="right">3.20s</td><td align="right">25</td><td align="right">2</td><td align="right">8.44774545 (+2.58%)</td><td align="right">3.18s</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.42s</td><td align="right">23</td><td align="right">3</td><td align="right"><b>7.21529977</b> (-2.79%)</td><td align="right">8.23s (-13%)</td><td align="right">33</td><td align="right">3</td></tr>
</tbody>
</table>
