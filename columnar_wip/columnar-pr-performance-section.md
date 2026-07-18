## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials (read + 10× optimize + write); `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

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
<tr><td>ninetriangles</td><td align="right">27</td><td>base</td><td>—</td><td align="right">3.38583082</td><td align="right">0.04s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.04s (0%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">198</td><td>base</td><td>—</td><td align="right">6.86122978</td><td align="right">0.08s</td><td align="right">5</td><td align="right">2</td><td align="right">6.87505673 (+0.20%)</td><td align="right">0.05s (−38%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04321510</td><td align="right">0.19s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05186752 (+0.21%)</td><td align="right">0.08s (−58%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75648389</td><td align="right">2.33s</td><td align="right">4</td><td align="right">6</td><td align="right"><b>4.74907624</b> (−0.16%)</td><td align="right">0.33s (−86%)</td><td align="right">3</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73952481</td><td align="right">0.20s</td><td align="right">66</td><td align="right">3</td><td align="right">6.74069306 (+0.02%)</td><td align="right">0.13s (−35%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83428058</td><td align="right">10.3s</td><td align="right">11</td><td align="right">4</td><td align="right"><b>7.83343660</b> (−0.01%)</td><td align="right">4.9s (−52%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56604138</td><td align="right">188.9s</td><td align="right">16</td><td align="right">13</td><td align="right">5.57279424 (+0.12%)</td><td align="right">24.5s (−87%)</td><td align="right">2</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.08s</td><td align="right">5</td><td align="right">2</td><td align="right">6.03455147 (+0.28%)</td><td align="right">0.04s (−50%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.04s (0%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50337896</td><td align="right">12.1s</td><td align="right">138</td><td align="right">3</td><td align="right"><b>7.44452817</b> (−0.78%)</td><td align="right">6.3s (−48%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39395534</td><td align="right">14.0s</td><td align="right">16</td><td align="right">4</td><td align="right">5.47251927 (+1.46%)</td><td align="right">6.7s (−52%)</td><td align="right">19</td><td align="right">3</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats OO. Parentheses on the columnar `-C` columns = change vs OO (`(=)` = bit-identical; negative time = faster).

**Reading the table**
- **Codelength** — columnar **beats OO** on powergrid (−0.16%), science2001 (−0.01%) and malaria (−0.78%); ties on the toys; and is within ~0.02–0.3% on the rest (politicalblogs +0.02%, jazz/netsci/lazega +0.2–0.3%, web-NotreDame +0.12%, air30k +1.5% — the residual memory-search gap).
- **Speed** — columnar is faster on every non-trivial network: ~2× on science2001 / malaria / air30k, ~7× on powergrid, and ~7.7× on web-NotreDame (24.5s vs 188.9s — the OO run thermally throttles on its ~3-min run, so a cool-machine ratio is closer to ~4×).
- **Shape** — columnar produces leaner, shallower maps (web-NotreDame: 2 top modules / 6 levels vs OO's 16 / 13; politicalblogs 2 vs 66) that the map equation scores as essentially equal.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.04s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.04s (0%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.87505673</td><td align="right">0.05s</td><td align="right">8</td><td align="right">2</td><td align="right">6.87505673 (=)</td><td align="right">0.04s (−20%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05186752</td><td align="right">0.07s</td><td align="right">2</td><td align="right">4</td><td align="right">4.06428378 (+0.31%)</td><td align="right">0.06s (−14%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74907624</td><td align="right">0.34s</td><td align="right">3</td><td align="right">5</td><td align="right">4.77217700 (+0.49%)</td><td align="right">0.20s (−41%)</td><td align="right">3</td><td align="right">5</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74069306</td><td align="right">0.12s</td><td align="right">2</td><td align="right">3</td><td align="right">6.74069306 (=)</td><td align="right">0.12s (0%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.83343660</td><td align="right">4.3s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">4.1s (−5%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">5.57279424</td><td align="right">27.4s</td><td align="right">2</td><td align="right">6</td><td align="right">5.62448318 (+0.93%)</td><td align="right"><b>20.1s</b> (−27%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.03455147</td><td align="right">0.04s</td><td align="right">6</td><td align="right">2</td><td align="right">6.03455147 (=)</td><td align="right">0.04s (0%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.04s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.04s (0%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.44452817</td><td align="right">5.8s</td><td align="right">5</td><td align="right">3</td><td align="right">7.44452817 (=)</td><td align="right">6.2s (+7%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td>air30k (states)</td><td align="right">5.47251927</td><td align="right">6.5s</td><td align="right">19</td><td align="right">3</td><td align="right">5.47251927 (=)</td><td align="right">6.3s (−3%)</td><td align="right">19</td><td align="right">3</td></tr>
</tbody>
</table>

Parentheses on the columnar `-F` columns = change vs `-C` (`(=)` = bit-identical; negative time = faster). Times are from a separate warm-machine session than the OO table above, so absolute values carry ±noise — the per-table percentages are the fair comparison.

**`-F` is a real speed/quality dial again.** With coarsening it now ties `-C` on both quality **and** time for the memory / metadata / multilayer and shallow-directed networks (politicalblogs, science2001, lazega, multilayer, malaria, air30k). On the base networks with real hierarchy it trades a little codelength for speed by skipping the interior refinement: web-NotreDame **+0.9% codelength for ~27% less time (20.1s vs 27.4s)**, powergrid +0.5% for ~40% less. `-C` stays the default (best quality everywhere); `-F` is the "give up a fraction of a percent for a faster, leaner, shallower map on deep networks" dial.

The columnar interior refinement stops at a diminishing-returns knee (default `--tune-iteration-relative-threshold` = 5e-3), which on deep networks trades the last ~0.06% of codelength for ~18% less search time; shallow networks are unaffected.
