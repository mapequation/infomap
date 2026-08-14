## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials, minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

> **This PR adds the non-redundant map equation L\*** ([#1001](https://github.com/mapequation/infomap/pull/1001))
> as a base-objective variant of the columnar engine, behind `--non-redundant`. The claim these numbers
> have to support is twofold: that the flag-gated change leaves the **default** columnar path untouched,
> and that the L\*-aware structural search is worth having — that it finds better L\* than scoring a
> base-L partition with L\*. Both are measured below, on the binary built from this PR's tip
> (`7ec6b954`, `md5 2f9860060abf2c6095157b37923a7254`).
>
> **The default path is unchanged, measured not assumed.** All 13 configs of the table below were
> re-run OO-vs-`-C` on this binary, interleaved, minimum of 3 repetitions: **all 26 codelengths, top-module
> counts and level counts are bit-identical to the published snapshot**, and every non-toy time lands
> within ±5% with the OO/`-C` ratios intact (powergrid 7.45× vs 7.46×, web-NotreDame 7.56× vs 7.39×,
> science2001 2.56× vs 2.51×). The deltas above ±8% are all on sub-0.05s toys, i.e. the measurement
> floor. The table itself is therefore carried over unchanged.
>
> **Not re-measured:** the `-F` and `-2` tables further down. `--non-redundant` cannot reach either code
> path, and the `-C` arm they are compared against reproduced bit-identically, so re-running them would
> only re-time unchanged code. Stated here rather than left implicit.
>
> **New evidence section:** "The non-redundant map equation L\* (`--non-redundant`)" below.

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
<tr><td>netscicoauthor2010</td><td align="right">552</td><td>base</td><td>—</td><td align="right">4.04354934</td><td align="right">0.13s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05454025 (+0.272%)</td><td align="right">0.03s (−77%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4 941</td><td>base</td><td>—</td><td align="right">4.75872920</td><td align="right">1.94s</td><td align="right">5</td><td align="right">7</td><td align="right"><b>4.74107206</b> (−0.371%)</td><td align="right">0.26s (−87%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">1 046</td><td>base</td><td><code>-d</code></td><td align="right">6.73892798</td><td align="right">0.15s</td><td align="right">80</td><td align="right">3</td><td align="right">6.74094314 (+0.0299%)</td><td align="right">0.07s (−53%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7 170</td><td>base</td><td><code>-d</code></td><td align="right">7.83638921</td><td align="right">7.87s</td><td align="right">11</td><td align="right">4</td><td align="right"><b>7.83343660</b> (−0.0377%)</td><td align="right">3.14s (−60%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">325 729</td><td>base</td><td><code>-d</code></td><td align="right">5.56592477</td><td align="right">144.8s</td><td align="right">17</td><td align="right">13</td><td align="right">5.56852929 (+0.0468%)</td><td align="right">19.6s (−86%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">69</td><td>metadata</td><td>—</td><td align="right">6.01786027</td><td align="right">0.02s</td><td align="right">7</td><td align="right">2</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">5</td><td>multilayer</td><td>—</td><td align="right">2.01140524</td><td align="right">0.01s</td><td align="right">2</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.00s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">307·9L</td><td>multilayer</td><td>—</td><td align="right">7.50242050</td><td align="right">9.10s</td><td align="right">142</td><td align="right">3</td><td align="right"><b>7.39750171</b> (−1.4%)</td><td align="right">3.24s (−64%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">13 213</td><td>state/memory</td><td>—</td><td align="right">5.39287115</td><td align="right">11.7s</td><td align="right">16</td><td align="right">4</td><td align="right"><b>5.39242541</b> (−0.00827%)</td><td align="right">3.91s (−67%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">13 213</td><td>state/memory</td><td><code>-d --regularized</code></td><td align="right">5.57843563</td><td align="right">7.63s</td><td align="right">301</td><td align="right">3</td><td align="right"><b>5.57624241</b> (−0.0393%)</td><td align="right">4.15s (−46%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">7 170</td><td>base</td><td><code>-d --preferred-number-of-modules 25</code></td><td align="right">7.94035360</td><td align="right">8.02s</td><td align="right">25</td><td align="right">4</td><td align="right">8.23558553 (+3.72%)</td><td align="right">3.38s (−58%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

`nodes`: state nodes for air30k (13 213 states over 183 physical); physical·layers for malaria. **Bold** = columnar beats or exactly ties OO. Parentheses on the columnar `-C` columns = change vs OO (`(=)` = bit-identical; negative time = faster).

**Reading the table**
- **Codelength** — columnar `-C` **ties or beats OO on 9 of 13 configs**: it beats OO on malaria (−1.40%), powergrid (−0.37%), science2001 (−0.04%), regularized air30k (−0.04%), air30k (−0.008%) and jazz (−0.004%), and ties lazega and the toys. Remaining gaps: pref-mods +3.72% (the `|K − K_pref|` bias is leaf-loop-only, #827), netsci +0.27%, web-NotreDame +0.05%, politicalblogs +0.03%. Regularized air30k, air30k and jazz are inside the per-seed spread either way, so read those three as ties that fell on the good side of this seed rather than as wins.
- **The web-NotreDame gap is a user choice.** At the default knee it is +0.05%. Adding `--tune-iteration-relative-threshold 1e-3` takes it to 5.56067487 — **−0.094%, ahead of OO** — for +18–30% wall; `0` (full convergence) reaches 5.55881205, −0.128% ahead, for +85–95%. F26 explains why the deeper refinement is a dial rather than the default.
- **Speed** — columnar is faster on every non-trivial network: ~1.8× on regularized air30k, ~2.4–2.6× on pref-mods and science2001, ~2.8–3.1× on malaria and air30k, ~7.4× on web-NotreDame and powergrid (19.6s vs 144.8s).
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
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.26s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77402225 (+0.695%)</td><td align="right">0.16s (−38%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.74094314</td><td align="right">0.07s</td><td align="right">81</td><td align="right">2</td><td align="right">6.74094314 (=)</td><td align="right">0.07s</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.83343660</td><td align="right">3.14s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">3.04s (−3%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">5.56852929</td><td align="right">19.6s</td><td align="right">5</td><td align="right">6</td><td align="right">5.62506198 (+1.02%)</td><td align="right">15.5s (−21%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.01s</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.00s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.00s</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.24s</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.19s (−2%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39242541</td><td align="right">3.91s</td><td align="right">22</td><td align="right">3</td><td align="right">5.39242541 (=)</td><td align="right">3.66s (−6%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.15s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57624241 (=)</td><td align="right">3.69s (−11%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.23558553</td><td align="right">3.38s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.32s (−2%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

Parentheses on the columnar `-F` columns = change vs `-C` (`(=)` = bit-identical; negative time = faster); both columns are from the current same-session re-measurement. Sub-0.1s toy times are shown without a percentage (measurement floor).

**`-F` still ties `-C` on 10 of 13 configs** — including pref-mods, where it previously lost another +2.3% — because the flat-first trials carry the same winning partitions into both searches. The dial only bites on the base networks with real deep hierarchy, where skipping the interior refinement trades codelength for speed — and the F23 knee widens that trade, because it deepens exactly the refinement `-F` skips: web-NotreDame **+1.02% for −21% time**, powergrid +0.69% for −38%, netsci +0.21%. Partial seeding widens the dial further, because it improves `-C` on exactly the three networks `-F` cannot follow it on (the re-refine gate makes `-F` bit-identical by construction). On the flat-winning networks `-F` is 0–11% faster than `-C`, because `-C`'s expensive pass — the interior refinement — is exactly the one the flat bottom no longer needs (below).

The columnar interior refinement stops at a diminishing-returns knee (default `--tune-iteration-relative-threshold` = 5e-3). On the two deep base networks, lowering it to 1e-3 buys 0.05–0.14% of codelength for ~20–30% more time; shallow networks are structurally unaffected. See the knee section below.

### The refine knee stays at 5e-3, and 1e-3 is a dial (F23 → F26)

`ColumnarTwoLevel::m_minRelTuneImprovement` stops the interior-layer refinement once a whole up/down
sweep gains less than this fraction of the post-build codelength. The shipped default is **5e-3**, and
the deeper setting is offered as a dial rather than defaulted — it is real quality that more trials
cannot buy (at matched CPU on web-NotreDame, 5e-3 saturates at 5.5727 by `-N12` while 1e-3 reaches
5.5674), but not worth a fifth more CPU by default. Re-measured at this tip:

| `--tune-iteration-relative-threshold` | web-NotreDame `-C` | vs default | wall |
|---|--:|--:|--:|
| 5e-3 (default) | 5.56852929 | — | — |
| 1e-3 | **5.56067487** | −0.14% | +22% (+18–30% across sessions) |
| 0 (full convergence) | **5.55881205** | −0.17% | +92% (+85–95% across sessions) |

Only the two deep base networks react at all: `refineSweeps` is 1 for a stack with at most one
interior layer, so science2001/air30k/malaria cannot, and `-F` never enters `refineHierarchy`. (The
default was briefly changed to 1e-3 and reverted on a measurement error — F23/F26 in the findings
log.)

> **This section is a snapshot, not a changelog.** It carries the current numbers plus the evidence for
> the change under review. Per-feature attribution for features that already landed (partial seeding
> #985, flat-first #891, coarse-tune #890, the #875 hoist, the split operator #987/#988,
> the worker leaf-input fix #997, the master sync bringing in #998) lives in each
> of those PRs, whose own copy of this file is the measurement that justified it; the running narrative
> is in [`columnar_wip/columnar-rethink-notes.md`](columnar_wip/columnar-rethink-notes.md). The last two
> sections below — "The non-redundant map equation L\*" and "`--non-redundant-exact` is inert" — are the
> evidence for **this** PR.

Every measured run behind this section is logged row-per-run in [`columnar_wip/columnar-search-runs.tsv`](columnar_wip/columnar-search-runs.tsv) for plotting the codelength/time frontier. Read the `batch` column before comparing times: session noise floors ranged ±3% to 13%, so the time axis is only comparable within a batch (`load1m`, `reps`, `agg` are recorded for that reason), and `derived=1` marks rows reconstructed from reported percentages rather than measured absolutes.

### Two-level clustering (`-2`)

`--two-level` is wired to the columnar engine on the `columnar-two-level` branch (PR #823): the full two-level optimize materialized as a two-level stack, followed by the correction-aware module-merge coarsening interleaved with a seeded leaf fine-tune until the pair stops improving, plus the #889 coarse-tune (in-trajectory repair every trial + deep repair of the winning trial). **No change to the `-2` code path since PR #890**, and none from this sync — every codelength below is bit-identical to the base branch, and every row is re-measured in the same session as the tables above.

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
<tr><td>powergrid</td><td align="right">5.60044386</td><td align="right">0.58s</td><td align="right">419</td><td align="right">5.63729688 (+0.658%)</td><td align="right">0.12s (−79%)</td><td align="right">419</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.73972141</td><td align="right">0.09s</td><td align="right">80</td><td align="right"><b>6.73957529</b> (−0.00217%)</td><td align="right">0.06s</td><td align="right">81</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.95003960</td><td align="right">3.98s</td><td align="right">496</td><td align="right"><b>7.94997883</b> (−0.000764%)</td><td align="right">2.45s (−38%)</td><td align="right">506</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">6.74298853</td><td align="right">44.0s</td><td align="right">11809</td><td align="right">6.75421666 (+0.167%)</td><td align="right">19.6s (−56%)</td><td align="right">11991</td></tr>
<tr><td>lazega (meta)</td><td align="right">6.01786027</td><td align="right">0.01s</td><td align="right">7</td><td align="right"><b>6.01786027</b> (=)</td><td align="right">0.01s</td><td align="right">7</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.00s</td><td align="right">2</td><td align="right"><b>2.01140524</b> (=)</td><td align="right">0.00s</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50595639</td><td align="right">6.63s</td><td align="right">142</td><td align="right"><b>7.40044538</b> (−1.41%)</td><td align="right">2.85s (−57%)</td><td align="right">168</td></tr>
<tr><td>air30k (states)</td><td align="right">5.39331278</td><td align="right">4.61s</td><td align="right">332</td><td align="right"><b>5.39305505</b> (−0.00478%)</td><td align="right">3.57s (−23%)</td><td align="right">334</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57921689</td><td align="right">5.73s</td><td align="right">301</td><td align="right"><b>5.57557704</b> (−0.0652%)</td><td align="right">3.77s (−34%)</td><td align="right">304</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.13096953</td><td align="right">5.92s</td><td align="right">25</td><td align="right">8.23558553 (+1.29%)</td><td align="right">3.14s (−47%)</td><td align="right">25</td></tr>
</tbody>
</table>

Parentheses on the columnar columns = change vs OO `-2` (**bold** = columnar beats or exactly ties OO). Columnar `-2` ties or beats OO on 10 of 13 configs, including *every* correction network; the three exceptions are base-objective configs (powergrid +0.66%, web-NotreDame +0.17%, pref-mods +1.29%), and it is faster on every non-trivial network (−21% to −82%).

### The non-redundant map equation L\* (`--non-redundant`)

L\* is a **different objective**, not a better score for the same one, so its codelength cannot be read
against base L in either direction (see the cross-scored table below, where L\* comes out *higher* than
L for the same partition on four of eight configs). What is comparable is cost and shape, and — through
cross-scoring — whether the L\*-aware search earns its place.

First the 8 first-order configs; the higher-order ones follow in their own table below. Both arms
interleaved in one session, minimum of 3 repetitions, same binary, `--seed 123 -N10`.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="3">columnar <code>-C</code> (base L)</th>
<th colspan="3">columnar <code>-C --non-redundant</code> (L*)</th>
<th rowspan="2">Δ time</th>
</tr>
<tr>
<th>L</th><th>top</th><th>lvls</th>
<th>L*</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">3</td><td align="right">3</td><td align="right">3.078067323</td><td align="right">3</td><td align="right">3</td><td align="right">0.011s vs 0.012s</td></tr>
<tr><td>jazz</td><td align="right">6.862755928</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367</td><td align="right">7</td><td align="right">2</td><td align="right">0.018s vs 0.018s</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.054540245</td><td align="right">2</td><td align="right">4</td><td align="right">3.892209764</td><td align="right">2</td><td align="right"><b>5</b></td><td align="right">−3.6%</td></tr>
<tr><td>powergrid</td><td align="right">4.741072056</td><td align="right">5</td><td align="right">5</td><td align="right">4.509265423</td><td align="right"><b>3</b></td><td align="right"><b>7</b></td><td align="right">−1.6%</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td align="right">6.740943136</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502</td><td align="right"><b>2</b></td><td align="right">3</td><td align="right">−0.3%</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td align="right">7.833436601</td><td align="right">15</td><td align="right">3</td><td align="right">8.009172258</td><td align="right">22</td><td align="right">3</td><td align="right">−9.1%</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td align="right">5.568529293</td><td align="right">5</td><td align="right">6</td><td align="right">5.517073626</td><td align="right">5</td><td align="right">6</td><td align="right">+0.6%</td></tr>
<tr><td>science2001 (pref-mods)</td><td align="right">8.235585529</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451</td><td align="right">25</td><td align="right">2</td><td align="right">−1.1%</td></tr>
</tbody>
</table>

Times are min-of-3 wall for all 10 trials; the two toys are shown as absolutes (measurement floor).
Absolute times in this batch sit ~5% above the headline table's because the machine was busier
(`load1m ≈ 4`) — the arms are interleaved within the batch, so Δ time is unaffected.

**L\* is free.** No config is more than 1% slower. The −9.1% on science2001 is reproducible across all
three repetitions (2.90/2.92/2.92s vs 3.19/3.24/3.28s), but it is a *trajectory* difference, not a code
speed-up: L\*-gating accepts different operators, so the search does a different amount of work. The
clean same-partition check is web-NotreDame `-N1`, where the two arms compute the same partition:
1.806s (L) vs 1.763s (L\*) — equal. Nothing in the L\* scoring path costs measurable time.

**Cross-scored: does the L\*-aware search earn its place?** Each arm's partition scored under both
objectives (`-C --no-infomap -c <tree>`, adding `--non-redundant` for the L\* column):

| network | L*(P<sub>L</sub>) | L*(P<sub>L\*</sub>) | L\* gained by the L\*-aware search | L(P<sub>L\*</sub>) vs L(P<sub>L</sub>) |
|---|--:|--:|--:|--:|
| ninetriangles | 3.078067323 | 3.078067323 | — same partition | — |
| jazz | 6.868270839 | **6.868228367** | −0.0006% | +0.058% |
| netscicoauthor2010 | 3.960538494 | **3.892209764** | **−1.73%** | +1.21% |
| powergrid | 4.560035073 | **4.509265423** | **−1.11%** | +7.73% |
| politicalblogs (`-d`) | 6.792005387 | **6.789241502** | −0.041% | +0.012% |
| science2001 (`-d`) | 8.011990676 | **8.009172258** | −0.035% | +0.008% |
| web-NotreDame (`-d`) | 5.517073626 | 5.517073626 | — same partition | — |
| science2001 (pref-mods) | 8.447745451 | 8.447745451 | — same partition | — |

Three readings, and the middle one is the answer to the design question:

- **L\* is not pointwise below L.** For the *same* partition it is lower on ninetriangles (−9.1%),
  netsci (−2.3%), powergrid (−3.8%) and web-NotreDame (−0.9%), but **higher** on jazz (+0.08%),
  politicalblogs (+0.76%), science2001 (+2.3%) and pref-mods (+2.6%). The separate enter codebook can
  cost more than leave-one-out saves. So the two arms' headline codelengths are not a quality ranking
  in either direction — which is why the columns above are labelled by objective.
- **The L\*-aware structural search never loses on its own objective** — it wins on all 5 configs where
  the partitions differ, by up to 1.73%, and ties on the 3 where they coincide. That is the measured
  case for hosting L\* as a search-driving objective rather than a post-hoc rescore.
- **L\* prefers different structure.** powergrid goes 5 top / 5 levels → 3 / 7 and pays +7.7% in base L;
  politicalblogs collapses 81 top modules to 2. On the two largest-K configs (web-NotreDame, pref-mods)
  L\*-gating changes nothing at all — the search lands on the identical partition.

### L\* on higher-order networks: the physical codebook composes, and the guard should go

L\* constrains **impossible walks** — no immediate re-entry into the module just left, no immediate exit
from the one just entered. That is orthogonal to *which codebook* a walk step is coded in, so it does not
limit support for higher-order dynamics: the physical/state codebook correction should apply on top, and
the two together are well defined. The `Config.cpp` rejection of state/multilayer input is therefore the
wrong constraint, and it is also **inconsistent**: it tests `config.stateInput || config.multilayerInput`,
which only reflect explicit input-format flags, so validation (which runs before the network is read) lets
the identical network through whenever the type is sniffed from the file. `air30k.net` and the malaria
multilayer run today; the same networks named with an explicit input format are refused.

Measured, same convention as above, both arms interleaved, min-of-3:

| network | L | L\* | Δ time | top | lvls |
|---|--:|--:|--:|--:|--:|
| multilayer (ex.) | 2.011405238 | 1.928856578 | 0.011s vs 0.012s | 2 = 2 | 2 = 2 |
| malaria | 7.397501710 | 7.432494779 | −0.4% | 2 = 2 | 3 = 3 |
| air30k (states) | 5.392425413 | 5.486124697 | −2.0% | 22 = 22 | 3 = 3 |
| air30k (reg.) | 5.576242406 | 5.691341197 | −3.3% | 11 = 11 | 3 = 3 |

Cross-scored the same way (**through `_states.tree`**, see below):

| network | L*(P<sub>L</sub>) | L*(P<sub>L\*</sub>) | reading |
|---|--:|--:|---|
| multilayer (ex.) | 1.928856578 | 1.928856578 | identical partition |
| air30k (states) | 5.486124697 | 5.486124697 | identical partition |
| air30k (reg.) | 5.691439090 | 5.691341197 | tie (−0.0017%) |
| malaria | 7.432487054 | 7.432494779 | tie (+0.0001%, sign against L\*) |

**L\* runs correctly on higher-order input and costs nothing there, but changes nothing either** — the
partitions are identical on two of four and tie to within 2×10⁻⁵ on the other two, with the sign going
both ways (on malaria and regularized air30k the L\* arm's partition is also marginally *better* under
base L than the base arm's own best, which is best read as best-of-10 stochasticity between two
near-identical partitions, not as either search beating the other). So relaxing the guard is about
correctness of scope, not about a quality gain. Note that the same "L\* only forbids impossible walks"
argument applies to **metadata**, which is still rejected; L\* × `MetaCorrection` is unvalidated, so that
one is a separate decision rather than an oversight.

> **Cross-scoring a higher-order partition must go through `_states.tree`.** Scoring these through the
> physical `.tree` first gave air30k L = 9.766 against a search-reported 5.392 — not an objective
> difference but a mangled partition, and Infomap says so: *"182 physical nodes have their states split
> across modules in this tree … the partition read back is likely not the one that was written."* The
> physical tree cannot express which state sits in which module. With `_states.tree` every re-scored
> L(P<sub>L</sub>) and L\*(P<sub>L\*</sub>) reproduces the search value to all printed digits.

### `--non-redundant-exact` is inert in Phase 1 — nothing to measure yet

`m_nrExact` is set by `setNonRedundantExact` and stored, but never read in `ColumnarMapEquation.cpp`:
the exact O(m) leave-one-out exit sweep it selects belongs to the leaf move loop, which Phase 1 does not
make L\*-aware (see the PR's "Why the leaf move loop is not L\*-aware"). Verified rather than assumed —
science2001 `-d -N1` with and without the flag produces a **byte-identical tree body** and the same
`# codelength 8.01141`; only the recorded command line and timestamps differ.

So the O(m)-exact vs O(K)-power-series comparison cannot be run on this PR: neither path exists here.
It belongs to whichever PR revives the L\*-aware leaf loop, where the networks would need subsetting to
keep an O(m)-per-candidate sweep inside a sane wall-clock. Recorded here so a later session does not
re-run an empty A/B and conclude the two are equivalent in cost.
