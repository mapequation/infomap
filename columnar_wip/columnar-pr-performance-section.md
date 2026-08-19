## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials, minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. See [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details.

> **This is the snapshot for the soft `--cluster-data` warm start** — fixing
> [#824](https://github.com/mapequation/infomap/issues/824), where `-C -c` read an initial partition,
> echoed it in the options banner and then discarded it, returning a result bit-identical to the same
> run without `-c` and sometimes far worse than the partition the user supplied.
>
> **No benchmark configuration below changes value, and that is the claim to check.** The change is
> reachable only when `--cluster-data` supplies a *soft* initial partition, which no row of any table
> below does; every seeded measurement is in its own section at the end. Verified rather than asserted:
> all **28** unseeded configurations (`-C` and `-C -2` over the 14 networks) are **bit-identical**
> between the pre-change binary and this one at `-N1`, and all 14 `-C` rows are bit-identical at `-N10`.
> The four tables here are the previous snapshot's numbers, carried over on that evidence.
>
> Binaries: pre-change `adef0c9a` md5 `9bb4f80ffc2d43d1ebcbca9c98c2ef75`, this change md5
> `ff9fe3fe79518966bb7519bfb9388eec`.
>
> **Times on those unseeded rows move by up to ±2.7%, and the sign is not stable between sessions.**
> Min of 3 interleaved repetitions at `-N10`, in three separate sessions: web-NotreDame came out +0.2%,
> −2.7%, 0.0%; regularized air30k +1.7%, −1.4%, −1.4%; air30k +1.1%, +1.0%, −2.3%; pref-mods −1.2%,
> +1.2%, −2.0%. All three sessions were 14/14 bit-identical, so the search executed the same work — the
> only code an unseeded trial adds is one `haveModules()` test — and the spread is the environment. It is
> quoted rather than resolved.

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
- **Codelength** — unchanged from the previous snapshot on all fourteen configurations, to the digit. Columnar ties or beats OO on 9 of 13: malaria (−1.4%), powergrid (−0.37%), science2001 (−0.038%), regularized air30k (−0.039%), air30k (−0.008%), jazz (−0.004%), and ties lazega and the toys. The remaining gaps are pref-mods (+3.72%, the `|K − K_pref|` bias being leaf-loop-only, #827), netsci (+0.27%), web-NotreDame (+0.047%) and politicalblogs (+0.030%).
- **`air30k (meta)`** cannot be quoted at `-N10` on the object-oriented arm: it does not finish inside
  30 minutes, where columnar takes 8.9 s. At `-N1` it is 8.437832327 in 5.4 s against columnar's
  7.580768308 in 0.79 s.
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

Measured on web-NotreDame `-C`, an unseeded configuration this change cannot reach (see the note at the top), so the previous snapshot's numbers stand:

| `--tune-iteration-relative-threshold` | codelength | vs default | wall |
|---|--:|--:|--:|
| 5e-3 (default) | 5.56852929 | — | 18.9s |
| 1e-3 | **5.56067487** | −0.14% | 22.7s (+20%) |
| 0 (full convergence) | **5.55881205** | −0.17% | 36.4s (+93%) |

### Soft `--cluster-data` warm start (#824) — what this PR changes

`-C` now **continues from** a soft `--cluster-data` partition instead of ignoring it. Same semantics as
the object-oriented engine: it warm-starts from the initial partition and does not also run a
from-scratch search. Nothing outside `-c` is touched.

All rows `--seed 123 -N1`, single-threaded; `-N1` because it is the only point where the arms compute
comparable work, so a time difference is code. Seeds are that network's own object-oriented output
(`tree`), that output perturbed (`merge` = 40% of modules merged pairwise, `split` = 40% split in two,
`shuffle` = 10% of nodes moved, `mix` = all three), truncated (`cut2`/`cut3` = the tree cut to 2/3
levels), random with a given module count (`rand<K>`), or a planted ground truth (`jelena`,
`ring32x3`). `input` is that seed scored by `-C --no-infomap -c`.

<table>
<thead>
<tr><th>case</th><th>seed</th><th>input</th><th><code>-C</code>, no <code>-c</code></th><th><code>-C -c</code> before</th><th><code>-C -c</code> after</th><th><code>OO -c</code></th></tr>
</thead>
<tbody>
<tr><td>jelena (state, planted)</td><td><code>-2d</code></td><td align="right">6.930934993</td><td align="right">7.810478411</td><td align="right">7.810478411 <em>(1.11s)</em></td><td align="right"><b>6.873378755</b> <em>(1.38s)</em></td><td align="right">6.881089886 <em>(2.70s)</em></td></tr>
<tr><td>jelena (state, planted)</td><td><code>-d</code></td><td align="right">6.930934993</td><td align="right">7.479374707</td><td align="right">7.479374707 <em>(0.92s)</em></td><td align="right"><b>6.873378755</b> <em>(2.01s)</em></td><td align="right">7.252694312 <em>(14.91s)</em></td></tr>
<tr><td>clique ring (planted)</td><td><code>-2</code></td><td align="right">3.652410119</td><td align="right">3.569591891</td><td align="right">3.569591891</td><td align="right"><b>3.566165627</b></td><td align="right">3.607676670</td></tr>
<tr><td>powergrid</td><td><code>tree</code></td><td align="right">4.752217852</td><td align="right">4.755047770</td><td align="right">4.755047770</td><td align="right"><b>4.749740760</b></td><td align="right">4.766837501</td></tr>
<tr><td>powergrid</td><td><code>cut3</code></td><td align="right">7.702201025</td><td align="right">4.755047770</td><td align="right">4.755047770</td><td align="right">4.777177752</td><td align="right"><b>4.766837501</b></td></tr>
<tr><td>powergrid</td><td><code>merge -2</code></td><td align="right">5.889421221</td><td align="right">5.641538012</td><td align="right">5.641538012</td><td align="right"><b>5.587611395</b></td><td align="right">5.592503477</td></tr>
<tr><td>powergrid</td><td><code>mix</code></td><td align="right">8.736944733</td><td align="right">4.755047770</td><td align="right">4.755047770</td><td align="right"><b>4.740553489</b></td><td align="right">4.768730841</td></tr>
<tr><td>powergrid</td><td><code>rand2</code></td><td align="right">12.87522226</td><td align="right">4.755047770</td><td align="right">4.755047770</td><td align="right"><b>4.738600187</b></td><td align="right">12.00440383 <em>(1 module)</em></td></tr>
<tr><td>science2001</td><td><code>tree</code></td><td align="right">7.835336866</td><td align="right">7.833436601</td><td align="right">7.833436601</td><td align="right"><b>7.831943521</b></td><td align="right">7.836583604</td></tr>
<tr><td>science2001</td><td><code>cut2</code></td><td align="right">8.880210195</td><td align="right">7.833436601</td><td align="right">7.833436601</td><td align="right"><b>7.837142499</b></td><td align="right">7.839902008</td></tr>
<tr><td>science2001</td><td><code>split</code></td><td align="right">8.499376591</td><td align="right">7.833436601</td><td align="right">7.833436601</td><td align="right">7.836709381</td><td align="right"><b>7.834161329</b></td></tr>
<tr><td>science2001</td><td><code>shuffle -2</code></td><td align="right">8.321818828</td><td align="right">7.952038107</td><td align="right">7.952038107</td><td align="right"><b>7.948655631</b></td><td align="right">7.964765178</td></tr>
<tr><td>air30k (state)</td><td><code>tree</code></td><td align="right">5.393962436</td><td align="right">5.473231053</td><td align="right">5.473231053</td><td align="right"><b>5.393962436</b> <em>(input kept)</em></td><td align="right">5.403880967</td></tr>
<tr><td>air30k (state)</td><td><code>mix</code></td><td align="right">7.396729364</td><td align="right">5.473231053</td><td align="right">5.473231053</td><td align="right"><b>5.392923717</b></td><td align="right">5.393397998</td></tr>
<tr><td>air30k (state)</td><td><code>merge -2</code></td><td align="right">5.413761065</td><td align="right">5.393492268</td><td align="right">5.393492268 <em>(0.47s)</em></td><td align="right">5.393608612 <em>(0.31s)</em></td><td align="right">5.397345113 <em>(0.27s)</em></td></tr>
<tr><td>netscicoauthor2010</td><td><code>tree</code></td><td align="right">4.043438853</td><td align="right">4.064688363</td><td align="right">4.064688363</td><td align="right"><b>4.043438853</b> <em>(input kept)</em></td><td align="right">4.052936819</td></tr>
<tr><td>web-NotreDame</td><td><code>tree</code></td><td align="right">5.566025331</td><td align="right">5.568529293</td><td align="right">5.568529293 <em>(4.02s)</em></td><td align="right"><b>5.566025331</b> <em>(4.42s, input kept)</em></td><td align="right">5.568908175 <em>(11.77s)</em></td></tr>
</tbody>
</table>

**Bold** = best of the two `-c` arms and `OO -c`. `-C -c` before is bit-identical to `-C` in every row,
which is the bug. These 17 rows are a representative slice of a 51-row matrix (7 networks × up to 12
seed shapes × `-2`/hierarchical); the counts below are over all 51.

**Reading it**
- **The seed is consumed.** `-C -c` differs from `-C` in **49 of 51** combinations (the two that agree
  are cases where the seeded search converges on the same partition), against 0 of 51 before.
- **Never worse than the input.** In every one of the 51 combinations the result is at or below the
  input partition's own codelength. Where the search cannot beat a supplied tree the input is handed
  back untouched — web-NotreDame returns its 19 modules at 12 levels, which is also the only way `-C`
  can emit a ragged tree at all, its stack being unable to hold one.
- **Against `OO -c`: better on 48 of 51**, losing only `pg-cut3` (+0.22%), `sci-split` (+0.033%) and
  `sci-pref3` (+0.027%). `OO -c` **collapses to one module** on a seed far from any optimum: powergrid
  with a random 2-, 10- or 200-module seed gives 12.00440383 with 1 module (science2001 9.754829232,
  air30k 6.21487022) where `-C -c` gives 4.74–4.75.
- **The cost of warm-start semantics.** Continuing from the seed instead of searching from scratch means
  that where the seed is the worse starting point, the result is worse than `-C` without `-c`: **10 of
  51**, by 0.0014% to 0.47% (`pg-cut3`; the other nine are ≤0.06%, and nine of the ten are on
  science2001). `OO -c` has the same exposure on the same rows. A best-of-two variant removes all 10 and
  was rejected — it is a second search per trial, and it is not what `--cluster-data` means.
- **Time.** Only `-c` runs are affected. At `-N1` the seeded search costs between −42%
  (air30k `merge -2`, 0.47s → 0.31s: continuing from a good partition is cheaper than building one) and
  +118% (jelena `-d`, 0.92s → 2.01s), and is faster than `OO -c` in almost every row — jelena `-d` 2.0s
  against 14.9s, web-NotreDame 4.4s against 11.8s. The input-partition score the guard needs is paid
  once per run, not per trial, and only when squaring changed the tree.

