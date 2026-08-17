## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials, minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. See [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details.

> **This is the snapshot for a three-PR stack** landing together into `columnar-hierarchical-core`:
> **(1)** the reporting paths that were left on the object-oriented objective (fixes
> [#1013](https://github.com/mapequation/infomap/issues/1013) and the columnar half of
> [#1002](https://github.com/mapequation/infomap/issues/1002)); **(2)** composing the metadata and
> physical-node codebooks, which the `else` in `addColumnarCorrections` had made mutually exclusive
> (fixes [#1012](https://github.com/mapequation/infomap/issues/1012)); **(3)** rejecting
> `--non-redundant --lossy` until the L\* form of the lossy objective ships (fixes
> [#1011](https://github.com/mapequation/infomap/issues/1011)).
>
> **Exactly one benchmark configuration changes value, and it is the new one.** `air30k (meta)` is a
> row added by this stack, because no benchmark configuration was metadata + higher-order — which is
> precisely why the set could not see the defect #1012 reports. Every other row of every table below is
> measured, not carried over, and **80 of 80 base-objective cells reproduce the pre-stack binary**: 73
> bit-identical and 7 differing in the last ULP (at most 1.8e-15 absolute, 3.3e-16 relative), from the
> restore path now returning the columnar core's own value rather than the object-oriented
> recomputation of the same partition.
>
> Binaries: pre-stack `d88f1c77` md5 `b601cf2f6e263c2be5ea1f1d496a77ec`, stack tip md5
> `9bb4f80ffc2d43d1ebcbca9c98c2ef75`. All 166 cells of the main batch were deterministic across the
> three repetitions.

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
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.02s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (-2.21e-14%)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50242050</td><td align="right">8.58s</td><td align="right">142</td><td align="right">3</td><td align="right"><b>7.39750171</b> (-1.4%)</td><td align="right">3.11s (-64%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39287115</td><td align="right">11.01s</td><td align="right">16</td><td align="right">4</td><td align="right"><b>5.39242541</b> (-0.00827%)</td><td align="right">3.64s (-67%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57843563</td><td align="right">7.28s</td><td align="right">301</td><td align="right">3</td><td align="right"><b>5.57624241</b> (-0.0393%)</td><td align="right">3.97s (-45%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.94035360</td><td align="right">7.72s</td><td align="right">25</td><td align="right">4</td><td align="right">8.23558553 (+3.72%)</td><td align="right">3.26s (-58%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">13 213</td><td>state/mem + meta</td><td><code>--meta-data …_usstate.meta</code></td><td align="right" colspan="4"><em>does not finish -N10 in 30 min; 8.43783233 / 5.4s at <code>-N1</code></em></td><td align="right">7.42215327</td><td align="right">8.90s</td><td align="right">23</td><td align="right">3</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats or exactly ties OO. Parentheses on the columnar columns = change vs OO (`(=)` = bit-identical). Time percentages are shown only where the OO arm is above the 0.1 s measurement floor and the change exceeds 3%.

**Reading the table**
- **Codelength** — unchanged from the previous snapshot on all thirteen pre-existing configurations, to the digit. Columnar ties or beats OO on 9 of 13: malaria (−1.4%), powergrid (−0.37%), science2001 (−0.038%), regularized air30k (−0.039%), air30k (−0.008%), jazz (−0.004%), and ties lazega and the toys. The remaining gaps are pref-mods (+3.72%, the `|K − K_pref|` bias being leaf-loop-only, #827), netsci (+0.27%), web-NotreDame (+0.047%) and politicalblogs (+0.030%).
- **`air30k (meta)` is the row this stack adds**, and the only one whose value moves: `-C` goes
  **8.199458513 → 7.422153270** (−9.5%). That is not an improvement on the same objective — before the
  fix the physical-node codebook was absent, so the two numbers price different things. The
  object-oriented arm cannot be quoted at `-N10`: it does not finish inside 30 minutes, where columnar
  takes 8.9 s. At `-N1` it is 8.437832327 in 5.4 s against columnar's 7.580768308 in 0.79 s.
- **Speed** — columnar is faster on every non-trivial network: ~1.8× on regularized air30k, ~2.4× on
  pref-mods and science2001, ~2.8× on malaria and air30k, ~7.3× on web-NotreDame and powergrid
  (18.9 s vs 137.6 s).

### The fast dial `-F`

`-F` (`--fast-hierarchical-solution`) skips the interior-layer refinement in favour of a single bottom re-partition within grandparents plus the module-level coarsening loop. Measured as `-C -F`: **`-F` alone does not select the columnar engine**, and a run without `-C` silently returns the object-oriented result.

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

`-F` is bit-identical to `-C` on **11 of 14** configurations and is 1–38% faster on every configuration above the measurement floor. The dial bites only on the three base networks with real deep hierarchy, where skipping the interior refinement trades codelength for speed — web-NotreDame +1.02% for −24%, powergrid +0.70% for −38%, netsci +0.21% for −31%.

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
<tr><td>air30k (meta)</td><td colspan="8"><em>—</em></td></tr>
</tbody>
</table>

Columnar `-2` ties or beats OO on 10 of 13 pre-existing configurations, including every correction network; the three exceptions are base-objective configurations (powergrid +0.66%, web-NotreDame +0.17%, pref-mods +1.29%), and it is faster on every non-trivial network.

### The non-redundant map equation L\* (`--non-redundant`)

L\* is a **different objective**, not a better score for the same one, so its codelength cannot be read against base L in either direction — it comes out *higher* than L on 5 of 14 configurations here. What is comparable is cost and shape.

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

L\* costs nothing: every one of the 14 configurations is **faster** than its base-objective arm, by 0.3% to 13.3%. That is the search reaching a different — often shallower — partition, not the arithmetic being cheaper.

### The refine knee stays at 5e-3

Re-measured at this tip on web-NotreDame `-C`, and unchanged by the stack (both binaries agree to the digit):

| `--tune-iteration-relative-threshold` | codelength | vs default | wall |
|---|--:|--:|--:|
| 5e-3 (default) | 5.56852929 | — | 18.9s |
| 1e-3 | **5.56067487** | −0.14% | 22.7s (+20%) |
| 0 (full convergence) | **5.55881205** | −0.17% | 36.4s (+93%) |

### What this stack changes, per sub-PR

All four binaries interleaved in **one** batch, `-N10`, minimum of 3 repetitions, over the `-C`,
`-C --non-redundant` and `-C -F` variants of all 14 networks. Percentages are against the pre-stack
binary; cells below the 0.1 s floor are excluded from the summary statistics.

| stack point | median | mean | range | n |
|---|--:|--:|--:|--:|
| + reporting paths (#1002/#1013) | −0.36% | −0.41% | −4.7% … +3.1% | 21 |
| + composed codebooks (#1012) | −0.16% | +0.28% | −2.7% … +3.3% | 21 |
| + lossy rejection (#1011), i.e. the tip | **+0.04%** | +0.06% | −2.3% … +2.4% | 21 |

*(excluding `air30k (meta)`, the one configuration whose objective changes — see below)*

**The stack is timing-neutral.** An earlier cross-batch comparison suggested a ~1–3% cost for the
reporting-path work, including +3.0% on web-NotreDame `-C`. It does not survive interleaving: measured
in one batch, that sub-PR's median is *negative*, and web-NotreDame `-C` moves +2.3% at that stack point
and +0.7% at the tip — inside the spread of the machine. The per-module breakdown and the re-stamping
run once per structural operator and once per restore, not in the move loop.

**#1011 changes no measured number by construction:** everything lossy is behind
`INFOMAP_FEATURE_LOSSY_MAP_EQUATION` and absent from a default build. A 20-configuration A/B between
pre- and post-fix *feature* builds (`32d7f08ccd5dbfeab91439f3bb568f2c` vs
`9e4d7209cfe6d23250c689d75a389bbb`) differs in **0** cells under exact float equality.

**#1012 is the whole of the change, and the whole of the cost.** The attribution isolates it exactly:
the reporting-path binary reproduces the pre-stack value on `air30k (meta)` bit-for-bit
(8.199458513), and the composed binary produces 7.422153270.

| `air30k (meta)` | pre-stack | stack tip | codelength | time |
|---|--:|--:|--:|--:|
| `-C` | 8.199458513 (67 top) | 7.422153270 (23 top) | −9.5% | 7.97s → 8.90s (+11.6%) |
| `-C --non-redundant` | 7.985932645 (68 top) | 7.215299774 (33 top) | −9.6% | 7.04s → 7.71s (+9.5%) |
| `-C -F` | 8.199458513 | 7.422153270 | −9.5% | 7.85s → 9.00s (+14.7%) |
| `-2 -C` | 9.227070874 (2624 top) | 7.424143707 (2237 top) | −19.5% | 6.06s → 9.52s (+57.1%) |

The `-2` row is the largest cost in the stack and it buys the largest quality move; the composed
objective genuinely does more work, because the physical-node codebook now participates in every module
merge rather than being absent. **This trade has been reported to and accepted by the maintainer**, on
the grounds that the pre-stack number prices a different objective, so the two are not a like-for-like
speed comparison.

**A caveat this stack does not fix, filed separately:** on malaria with metadata the composed *search*
can land above the pre-composition partition scored under the same composed objective (+0.20% to +9.8%
across three metadata assignments), because that partition is layer-pure and the composed search leaves
its basin chasing 2.324167 bits of codebook saving. The objective is right; the greedy search on it is
not always.

Every measured run behind this section is logged row-per-run in [`columnar_wip/columnar-search-runs.tsv`](columnar_wip/columnar-search-runs.tsv). Read the `batch` column before comparing times across sessions — the batches here are `pr1002-1013-snapshot` (the tables above) and `pr1002-1013-attrib2` (the per-sub-PR attribution).
