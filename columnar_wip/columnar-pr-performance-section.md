## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials, minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

> **This PR fixes [#989](https://github.com/mapequation/infomap/issues/989)**: under `--parallel-trials`, the columnar engine's entropy-bias correction was built with a divisor of 1 instead of the network's total degree, because a trial worker owns an empty `Network` and the divisor was read back off it. Every number below is measured on the **fixed** binary in one session: all 65 configs (13 networks × {OO, `-C`, `-C -F`, OO `-2`, `-2 -C`}), interleaved, minimum of 3 repetitions, with the pre-fix binary run arm-for-arm in the same session. **All 65 codelengths — and every top-module and level count — are bit-identical between the two binaries**, which is the expected result: no benchmarked configuration uses `--entropy-corrected` or `--parallel-trials`, so the search this table measures is untouched. The evidence *for* the fix is the parallel-vs-serial section at the bottom, not this table; this table is here to show the fix costs nothing.
>
> Same-session time deltas (fix vs pre-fix) are within **±1.5% on every configuration that runs longer than 1 s**; the larger figures all sit on sub-0.16 s configs where the 0.01 s timer resolution dominates. Against the previous snapshot the whole session runs ~5–9% faster on *both* engines — the columnar/OO time ratio is preserved to within ~2% on every network (web-NotreDame 0.132 then and now) — so read that shift as a less busy machine, not as a change from this PR.

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
<tr><td>ninetriangles</td><td align="right">27</td><td>base</td><td>—</td><td align="right">3.38583082</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td><td align="right"><b>3.38583082</b> (=)</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">198</td><td>base</td><td>—</td><td align="right">6.86304747</td><td align="right">0.03s</td><td align="right">5</td><td align="right">2</td><td align="right"><b>6.86275593</b> (−0.00425%)</td><td align="right">0.01s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04354934</td><td align="right">0.12s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05454025 (+0.272%)</td><td align="right">0.03s (−75%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75872920</td><td align="right">1.83s</td><td align="right">5</td><td align="right">7</td><td align="right"><b>4.74107206</b> (−0.371%)</td><td align="right">0.24s (−87%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73892798</td><td align="right">0.14s</td><td align="right">80</td><td align="right">3</td><td align="right">6.74094314 (+0.0299%)</td><td align="right">0.07s (−50%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83638921</td><td align="right">7.40s</td><td align="right">11</td><td align="right">4</td><td align="right"><b>7.83343660</b> (−0.0377%)</td><td align="right">3.00s (−59%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56592477</td><td align="right">138.7s</td><td align="right">17</td><td align="right">13</td><td align="right">5.56852929 (+0.0468%)</td><td align="right">18.3s (−87%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.02s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50242050</td><td align="right">8.53s</td><td align="right">142</td><td align="right">3</td><td align="right"><b>7.39750171</b> (−1.4%)</td><td align="right">3.07s (−64%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39287115</td><td align="right">11.0s</td><td align="right">16</td><td align="right">4</td><td align="right"><b>5.39242541</b> (−0.00827%)</td><td align="right">3.65s (−67%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57843563</td><td align="right">7.26s</td><td align="right">301</td><td align="right">3</td><td align="right"><b>5.57624241</b> (−0.0393%)</td><td align="right">3.95s (−46%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.94035360</td><td align="right">7.69s</td><td align="right">25</td><td align="right">4</td><td align="right">8.23558553 (+3.72%)</td><td align="right">3.24s (−58%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats or exactly ties OO. Parentheses on the columnar `-C` columns = change vs OO (`(=)` = bit-identical; negative time = faster).

**Reading the table**
- **Codelength** — columnar `-C` **ties or beats OO on 9 of 13 configs**: it beats OO on malaria (−1.40%), powergrid (−0.37%), science2001 (−0.04%), regularized air30k (−0.04%), air30k (−0.008%) and jazz (−0.004%), and ties lazega and the toys. Remaining gaps: pref-mods +3.72% (the `|K − K_pref|` bias is leaf-loop-only, #827), netsci +0.27%, web-NotreDame +0.05%, politicalblogs +0.03%. Regularized air30k, air30k and jazz are inside the per-seed spread either way, so read those three as ties that fell on the good side of this seed rather than as wins.
- **The web-NotreDame gap is a user choice.** At the default knee it is +0.05%. Adding `--tune-iteration-relative-threshold 1e-3` takes it to 5.56067487 — **−0.094%, ahead of OO** — for +18% wall; `0` (full convergence) reaches 5.55881205, −0.128% ahead, for +85%. F26 explains why the deeper refinement is a dial rather than the default.
- **Speed** — columnar is faster on every non-trivial network: ~1.8× on regularized air30k, ~2.4–2.5× on pref-mods and science2001, ~2.8–3.0× on malaria and air30k, ~7.6× on web-NotreDame and powergrid (18.3s vs 138.7s).
- **Shape** — columnar produces leaner, shallower maps (web-NotreDame: 5 top modules / 6 levels vs OO's 17 / 13)

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.01s</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.01s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86275593 (=)</td><td align="right">0.01s</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.03s</td><td align="right">2</td><td align="right">4</td><td align="right">4.06300588 (+0.209%)</td><td align="right">0.02s</td><td align="right">4</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.24s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77402224 (+0.695%)</td><td align="right">0.15s (−38%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74094314</td><td align="right">0.07s</td><td align="right">81</td><td align="right">2</td><td align="right">6.74094314 (=)</td><td align="right">0.06s</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.83343660</td><td align="right">3.00s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">2.92s (−3%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">5.56852929</td><td align="right">18.3s</td><td align="right">5</td><td align="right">6</td><td align="right">5.62506198 (+1.02%)</td><td align="right">13.7s (−25%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.07s</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.02s (−2%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39242541</td><td align="right">3.65s</td><td align="right">22</td><td align="right">3</td><td align="right">5.39242541 (=)</td><td align="right">3.36s (−8%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">3.95s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57624241 (=)</td><td align="right">3.48s (−12%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.23558553</td><td align="right">3.24s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.18s (−2%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

Parentheses on the columnar `-F` columns = change vs `-C` (`(=)` = bit-identical; negative time = faster); both columns are from the current same-session re-measurement. Sub-0.1s toy times are shown without a percentage (measurement floor).

**`-F` still ties `-C` on 10 of 13 configs** — including pref-mods, where it previously lost another +2.3% — because the flat-first trials carry the same winning partitions into both searches. The dial only bites on the base networks with real deep hierarchy, where skipping the interior refinement trades codelength for speed — and the F23 knee widens that trade, because it deepens exactly the refinement `-F` skips: web-NotreDame **+1.02% for −25% time**, powergrid +0.69% for −38%, netsci +0.21%. Partial seeding widens the dial further, because it improves `-C` on exactly the three networks `-F` cannot follow it on (the re-refine gate makes `-F` bit-identical by construction). On the flat-winning networks `-F` is 0–12% faster than `-C`, because `-C`'s expensive pass — the interior refinement — is exactly the one the flat bottom no longer needs (below).

The columnar interior refinement stops at a diminishing-returns knee (default `--tune-iteration-relative-threshold` = 5e-3). On the two deep base networks, lowering it to 1e-3 buys 0.05–0.14% of codelength for ~18–23% more time; shallow networks are structurally unaffected. See the knee section below.

### The refine knee stays at 5e-3, and 1e-3 is a dial (F23 → F26)

`ColumnarTwoLevel::m_minRelTuneImprovement` stops the interior-layer refinement once a whole up/down
sweep gains less than this fraction of the post-build codelength. The shipped default is **5e-3**, and
the deeper setting is offered as a dial rather than defaulted — it is real quality that more trials
cannot buy (at matched CPU on web-NotreDame, 5e-3 saturates at 5.5727 by `-N12` while 1e-3 reaches
5.5674), but not worth a fifth more CPU by default. Re-measured at this tip:

| `--tune-iteration-relative-threshold` | web-NotreDame `-C` | vs default | wall |
|---|--:|--:|--:|
| 5e-3 (default) | 5.56852929 | — | — |
| 1e-3 | **5.56067487** | −0.14% | +18% |
| 0 (full convergence) | **5.55881205** | −0.17% | +85% |

Only the two deep base networks react at all: `refineSweeps` is 1 for a stack with at most one
interior layer, so science2001/air30k/malaria cannot, and `-F` never enters `refineHierarchy`. (The
default was briefly changed to 1e-3 and reverted on a measurement error — F23/F26 in the findings
log.)

> **This section is a snapshot, not a changelog.** It carries the current numbers plus the evidence for
> the change under review. Per-feature attribution for features that already landed (partial seeding
> #985, flat-first #891, coarse-tune #890, the #875 hoist, the split operator #987/#988) lives in each
> of those PRs, whose own copy of this file is the measurement that justified it; the running narrative
> is in [`columnar_wip/columnar-rethink-notes.md`](columnar_wip/columnar-rethink-notes.md). The one
> section below — "What this PR fixes: #989" — is the evidence for **this** PR.

Every measured run behind this section is logged row-per-run in [`columnar_wip/columnar-search-runs.tsv`](columnar_wip/columnar-search-runs.tsv) for plotting the codelength/time frontier. Read the `batch` column before comparing times: session noise floors ranged ±3% to 13%, so the time axis is only comparable within a batch (`load1m`, `reps`, `agg` are recorded for that reason), and `derived=1` marks rows reconstructed from reported percentages rather than measured absolutes.

### Two-level clustering (`-2`)

`--two-level` is wired to the columnar engine on the `columnar-two-level` branch (PR #823): the full two-level optimize materialized as a two-level stack, followed by the correction-aware module-merge coarsening interleaved with a seeded leaf fine-tune until the pair stops improving, plus the #889 coarse-tune (in-trajectory repair every trial + deep repair of the winning trial). **No change to the `-2` code path since PR #890**, and none from this PR — every codelength below is bit-identical to the base branch, and every row is re-measured in the same session as the tables above.

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
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.01s</td><td align="right">9</td><td align="right"><b>3.51775481</b> (=)</td><td align="right">0.01s</td><td align="right">9</td></tr>
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.02s</td><td align="right">5</td><td align="right"><b>6.86122977</b> (−0.0265%)</td><td align="right">0.01s</td><td align="right">6</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28501267</td><td align="right">0.04s</td><td align="right">56</td><td align="right"><b>4.28307258</b> (−0.0453%)</td><td align="right">0.02s</td><td align="right">59</td></tr>
<tr><td>powergrid</td><td align="right">5.60044386</td><td align="right">0.55s</td><td align="right">419</td><td align="right">5.63729688 (+0.658%)</td><td align="right">0.10s (−82%)</td><td align="right">419</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.73972141</td><td align="right">0.09s</td><td align="right">80</td><td align="right"><b>6.73957529</b> (−0.00217%)</td><td align="right">0.05s</td><td align="right">81</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.95003960</td><td align="right">3.69s</td><td align="right">496</td><td align="right"><b>7.94997883</b> (−0.000764%)</td><td align="right">2.31s (−37%)</td><td align="right">506</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">6.74298853</td><td align="right">41.2s</td><td align="right">11809</td><td align="right">6.75421666 (+0.167%)</td><td align="right">17.2s (−58%)</td><td align="right">11991</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.01s</td><td align="right">7</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.01s</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50595639</td><td align="right">6.18s</td><td align="right">142</td><td align="right"><b>7.40044538</b> (−1.41%)</td><td align="right">2.71s (−56%)</td><td align="right">168</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39331278</td><td align="right">4.26s</td><td align="right">332</td><td align="right"><b>5.39305505</b> (−0.00478%)</td><td align="right">3.35s (−21%)</td><td align="right">334</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57921689</td><td align="right">5.40s</td><td align="right">301</td><td align="right"><b>5.57557704</b> (−0.0652%)</td><td align="right">3.53s (−35%)</td><td align="right">304</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.13096953</td><td align="right">5.65s</td><td align="right">25</td><td align="right">8.23558553 (+1.29%)</td><td align="right">2.98s (−47%)</td><td align="right">25</td></tr>
</tbody>
</table>

Parentheses on the columnar columns = change vs OO `-2` (**bold** = columnar beats or exactly ties OO). Columnar `-2` ties or beats OO on 10 of 13 configs, including *every* correction network; the three exceptions are base-objective configs (powergrid +0.66%, web-NotreDame +0.17%, pref-mods +1.29%), and it is faster on every non-trivial network (−21% to −82%).

### What this PR fixes: #989, measured here

The table above is the *cost* side and it is empty — the fix is inert for every benchmarked
configuration. This is the *effect* side, and it is the reason the PR exists.

`--parallel-trials` builds each trial worker as a fresh `InfomapBase` and hands it the run's network
as an **argument** — `worker.initNetwork(m_network)`. `InfomapBase::m_network` is a value member, so
the worker keeps its own default-constructed, empty one and never stores the run's. Anything the
search reads back off `m_network` is therefore zero inside a worker.
`addColumnarCorrections` did exactly that for the entropy-bias divisor, and
`BiasedEntropyCorrection`'s `totalDegree > 0.0 ? totalDegree : 1.0` guard turned the zero into a
divisor of **1** instead of ninetriangles' 108 — a correction two orders of magnitude too strong,
under which the cheapest tree is a single module. That guard is what made a division by zero look
like a plausible answer rather than an `inf`.

`examples/networks/ninetriangles.net --seed 7 --entropy-corrected`, per trial:

| engine | `-N4 --parallel-trials`, before | `-N4 --parallel-trials`, after | `-N4` serial |
|---|--:|--:|--:|
| OO | 3.742114 ×4 | 3.742114 ×4 | 3.742114 ×4 |
| **columnar** | **4.918622 ×4** (= one-level, 1 module) | **3.635831 ×4** | **3.635831 ×4** |

The fix derives the divisor in `initNetwork(Network&)` from the network passed in, beside the
`m_optimizer->setNetworkProperties(network)` call that already hands the object-oriented objective the
same figure — which is precisely why OO never had the bug — and hands it down to sub/super instances
alongside the objective's own copy. The rule for computing it now has one definition instead of two.

**Not a regression, and the reason it was invisible.** Master's
[#947](https://github.com/mapequation/infomap/pull/947) added the OpenMP compile flags to the C++ test
targets. Before it `_OPENMP` was undefined in the test translation unit, so every `#ifdef _OPENMP`
body — which is where the parallel-trials contracts live — compiled to nothing while the cases still
appeared in the ctest list and reported as passing.

**Two test-suite problems this exposed, both worth more than the bug.**

*Worker-count invariance cannot catch a worker bug.* `Parallel trials with entropy correction are
invariant to worker count` is tagged `[columnar-contract]` and stayed green throughout, because 1
worker and 4 workers both run through `runTrialsInParallel`. A fact missing from every worker is
missing from both arms. Only a parallel-vs-serial comparison crosses the boundary where the fact goes
missing.

*The parallel-vs-serial cases were asserting something false by construction.* They compared parallel
trial *i* against a serial `-N1` run with seed 7+*i*. A trial's result is a function of its seed **and
its global trial index** — `m_columnarFlatFirstTrial = (trialOffset + trialIndex) % 2 == 1` alternates
the hierarchy-build strategy — and a `-N1` run is always trial 0. On ninetriangles with
`--markov-time 1.5` the parallel vector is [4.068, 3.771, 4.068, 3.771] while every `-N1` run returns
4.068. Those cases passed only because their own fixtures score the same either way. They now compare
against a serial `--num-trials 4` run element-wise, which is the invariant that actually holds:
**`--parallel-trials` is a scheduling choice and nothing else.** Both re-tagged cases were verified to
fail on the pre-fix binary with exactly the numbers above.

**Swept while here** — parallel `-N4` vs serial `-N4`, element-wise, `--columnar`: default, `-2`,
`-F`, `--entropy-corrected`, `--markov-time 1.5`, `--variable-markov-time`, `-d`,
`-d --recorded-teleportation`, `-d --regularized`, `--preferred-number-of-modules`, states,
multilayer, multilayer `-d`, bipartite, weighted directed, `--meta-data` (first-order and states),
`--cluster-data`, `--no-infomap --cluster-data`. All agree, so the entropy divisor was the only
run-scope fact the columnar search was reading off `m_network`.

**Deferred to [#994](https://github.com/mapequation/infomap/issues/994).** The other half of the same
asymmetry: workers never get the native columnar leaf input either, because `buildColumnarLeafInput`
runs in `RunSession` and the worker bypasses it, so every trial rebuilds from the InfoNode leaf tree.
Measured at one worker (`OMP_NUM_THREADS=1`, `-N4`, min of 3, so parallelism is not a factor), the
worker path costs **+13.3%** (science2001 `-d`), **+20.2%** (web-NotreDame `-d`) and **+7.5%**
(air30k) CPU for the same four trials. Deliberately not attributed to one cause — workers also call
the full `initNetwork` per trial where the serial loop only calls `removeModules()`, and splitting the
two needs an isolating build rather than an argument.
