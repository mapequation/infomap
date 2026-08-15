## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials, minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

> **This PR** ([#1010](https://github.com/mapequation/infomap/pull/1010), fixes [#1009](https://github.com/mapequation/infomap/issues/1009)) **is a correctness fix** to the memory objective under L\*: `MemCorrection` substituted the
> physical-node `sum plogp(flow)` for the state-node one at coefficient 1, which is the base map
> equation's coefficient, while L\*'s leaf-module term consumes that quantity at a per-module rate
> `1 + qEnter*qExit/(flow*(flow+qExit)) >= 1`. `-C --non-redundant` on state / memory / multilayer input
> therefore reported L\* **too high**, and the error grew with the number of physical nodes holding
> several states in one module. **`--non-redundant` on higher-order input changes value and search
> trajectory. Everything else is bit-identical**, which is the claim the numbers below have to carry.
>
> **The base objective did not move — measured, not argued.** 12 `-C` configs run on the pre-fix binary
> and this one (jazz `-N20` two-level and hierarchical, ninetriangles two-level and hierarchical, the
> `states.net` / `states_flow.net` / `multilayer.net` fixtures two-level and hierarchical, a
> `--directed --recorded-teleportation` config, and a 54-state exactly-lumpable duplication of
> ninetriangles) reproduce their codelength to the **last bit** (`delta == 0.0`, not "within 1e-12").
> The correction's base branch is untouched down to its summation order, precisely so this holds.
>
> **STALE — must be re-measured before this PR is reviewed:** the four higher-order rows of the L\*
> table below (`multilayer (ex.)`, `malaria`, `air30k`, `air30k (reg.)`). Those are pre-fix L\* numbers
> and this PR changes them by construction. They could not be redone in the session that made the fix:
> `networks/` (malaria, air30k, and every other real-world benchmark) is not present in that checkout.
> Every other row of every table is a base-objective or first-order-L\* config that the bit-identity
> sweep above covers, and is carried over unchanged.
>
> Direction of the change, from the configs that *were* available: a 54-state lumpable duplication of
> ninetriangles under `-C --non-redundant` goes 3.226345721211 → 3.153268798134 (hierarchical, top 3 /
> 3 levels both ways) and 3.465711933541 → 3.392635010464 (`--two-level`, 9 modules both ways) — the
> second is now exactly the physical network's L\* for the same partition, which is the invariance the
> fix restores. The small state fixtures do not move at all: their optima put no physical node's two
> states in one module, so the correction is zero under either coefficient.
>
> **Not re-measured:** the `-F` and `-2` tables, and all timings. Nothing on the base-objective path
> changed, and the L\* scoring path gained one O(K) pass over level-1 modules on the cold
> (per-structural-operator) path, only when a `MemCorrection` is attached — so re-running them from the
> same networks is the check to make, not a different set of numbers.

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
> the worker leaf-input fix #997, the master sync bringing in #998, the non-redundant map equation #1001)
lives in each
> of those PRs, whose own copy of this file is the measurement that justified it; the running narrative
> is in [`columnar_wip/columnar-rethink-notes.md`](columnar_wip/columnar-rethink-notes.md). The last
> section below — "What this PR changes: the file split, and nothing else" — is the evidence for **this**
> PR.

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
L for the same partition on 7 of 13 configs). What is comparable is cost and shape, and — through
cross-scoring — whether the L\*-aware search earns its place.

**All 13 benchmark configs run under L\***: no input and no objective is excluded. Both arms
interleaved in one session, minimum of 3 repetitions, same binary, `--seed 123 -N10`. Times are
comparable within a row, not across the three batches (`pr1001-lstar`, `pr1001-lstar-ho`,
`pr1001-lstar-meta` in the run log). The four higher-order L\* codelengths are **stale** — this PR
changes them and the networks were not available to re-measure; see the note at the top.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th rowspan="2">objective</th>
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
<tr><td>ninetriangles</td><td>base</td><td align="right">3.38583082</td><td align="right">3</td><td align="right">3</td><td align="right">3.078067323</td><td align="right">3</td><td align="right">3</td><td align="right">0.011s vs 0.012s</td></tr>
<tr><td>jazz</td><td>base</td><td align="right">6.862755928</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367</td><td align="right">7</td><td align="right">2</td><td align="right">0.018s vs 0.018s</td></tr>
<tr><td>netscicoauthor2010</td><td>base</td><td align="right">4.054540245</td><td align="right">2</td><td align="right">4</td><td align="right">3.892209764</td><td align="right">2</td><td align="right"><b>5</b></td><td align="right">−3.6%</td></tr>
<tr><td>powergrid</td><td>base</td><td align="right">4.741072056</td><td align="right">5</td><td align="right">5</td><td align="right">4.509265423</td><td align="right"><b>3</b></td><td align="right"><b>7</b></td><td align="right">−1.6%</td></tr>
<tr><td>politicalblogs (<code>-d</code>)</td><td>base</td><td align="right">6.740943136</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502</td><td align="right"><b>2</b></td><td align="right">3</td><td align="right">−0.3%</td></tr>
<tr><td>science2001 (<code>-d</code>)</td><td>base</td><td align="right">7.833436601</td><td align="right">15</td><td align="right">3</td><td align="right">8.009172258</td><td align="right">22</td><td align="right">3</td><td align="right">−9.1%</td></tr>
<tr><td>web-NotreDame (<code>-d</code>)</td><td>base</td><td align="right">5.568529293</td><td align="right">5</td><td align="right">6</td><td align="right">5.517073626</td><td align="right">5</td><td align="right">6</td><td align="right">+0.6%</td></tr>
<tr><td>lazega</td><td>metadata</td><td align="right">6.017860269</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653</td><td align="right">7</td><td align="right">2</td><td align="right">0.015s vs 0.015s</td></tr>
<tr><td>multilayer (ex.)</td><td>multilayer</td><td align="right">2.011405238</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (unchanged)</td><td align="right">2</td><td align="right">2</td><td align="right">0.011s vs 0.012s</td></tr>
<tr><td>malaria</td><td>multilayer</td><td align="right">7.397501710</td><td align="right">2</td><td align="right">3</td><td align="right"><i>7.432494779 — STALE</i></td><td align="right">2</td><td align="right">3</td><td align="right">−0.4%</td></tr>
<tr><td>air30k</td><td>state/memory</td><td align="right">5.392425413</td><td align="right">22</td><td align="right">3</td><td align="right"><i>5.486124697 — STALE</i></td><td align="right">22</td><td align="right">3</td><td align="right">−2.0%</td></tr>
<tr><td>air30k (reg.)</td><td>state/memory</td><td align="right">5.576242406</td><td align="right">11</td><td align="right">3</td><td align="right"><i>5.691341197 — STALE</i></td><td align="right">11</td><td align="right">3</td><td align="right">−3.3%</td></tr>
<tr><td>science2001 (pref-mods)</td><td>base + bias</td><td align="right">8.235585529</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451</td><td align="right">25</td><td align="right">2</td><td align="right">−1.1%</td></tr>
</tbody>
</table>

**L\* is free.** No config is more than 1% slower. The −9.1% on science2001 reproduces across all three
repetitions (2.90/2.92/2.92s vs 3.19/3.24/3.28s), but it is a *trajectory* difference, not a code
speed-up: L\*-gating accepts different operators, so the search does a different amount of work. The
clean same-partition check is web-NotreDame `-N1`, where both arms compute the same partition: 1.806s
(L) vs 1.763s (L\*) — equal. Nothing in the L\* scoring path costs measurable time.

Everything above is **current numbers**. The evidence that justified L\* — the cross-scored 2×2 showing
the L\*-aware search wins on its own objective, the proof that a correction contributes the same term
under both bases, and the `_states.tree` round-trip trap — lives in
[#1001](https://github.com/mapequation/infomap/pull/1001), whose own copy of this file is the
measurement that justified it.

### What this PR changes: L\* on higher-order input, and nothing else ([#1010](https://github.com/mapequation/infomap/pull/1010))

The memory objective's physical-node codebook is a **substitution**, not an additive term: both base
objectives read a level-1 module's leaf flows only through `F_m = sum_{leaf in m} plogp(flow)`, and
`MemCorrection` is the same objective with the state-node `F_m` replaced by the physical-node one. A
substitution inherits the coefficient of the term it sits inside — and the two objectives disagree
about it. The base map equation's `T`-normalized module term collapses to
`plogp(T) - plogp(qExit) - F_m`, so the coefficient is exactly 1. L\* splits that codebook into an
enter codebook normalized by `moduleFlow` and a within codebook normalized by `T = moduleFlow + qExit`,
charging `F_m` against both, so the coefficient is
`nrLeafCodebookRate = 1 + qEnter*qExit/(flow*(flow+qExit)) >= 1`.

The correction was applied at coefficient 1 under both. Since `plogp` is superadditive under splitting,
`F^state - F^phys <= 0`, so multiplying it by 1 instead of by `rate >= 1` **under-subtracts**: L\* came
out too high, always, by `(rate - 1) * (F^phys - F^state)`.

The observable symptom is a broken invariance. Split every physical node into states the walker cannot
distinguish and lift the partition so no physical node is split across modules: the process, the
modules and every describable event are unchanged, so the codelength must be too. `L` always was. `L*`
was not:

| case | L (before = after) | L\* before | L\* after | error removed |
|---|--:|--:|--:|--:|
| two triangles, all 6 nodes ×2 | 2.320730356834 | 2.145875734724 | 2.128018591867 | +0.017857142857 |
| two triangles, node 3 only ×2 | 2.320730356834 | 2.131845122479 | 2.128018591867 | +0.003826530612 |
| ninetriangles, all 27 nodes ×2 | 3.572285805615 | 3.465711933541 | 3.392635010464 | +0.073076923077 |
| two triangles, directed (rawdir), all ×2 | 1.899533374666 | 1.844312916393 | 1.841584741790 | +0.002728174603 |
| jazz provenance lift, 30 split nodes | 6.861229774904 | 6.841080565212 | 6.836565239276 | +0.004515325936 |
| jazz provenance lift, 78 split nodes | 6.861229774904 | 6.855445280017 | 6.836565239276 | +0.018880040741 |

Every "after" equals the *physical* network's own L\* for the same partition to `<= 9e-14` (two
triangles 2.128018591867112, ninetriangles 3.392635010464057, jazz 6.836565239275705), which is the
invariance stated as an equation rather than as a delta.

**The base objective is bit-identical.** 12 `-C` configs — jazz (`-N20`, two-level and hierarchical),
ninetriangles (both), the `states.net` / `states_flow.net` / `multilayer.net` fixtures (both), a
`--directed --recorded-teleportation` config, and a 54-state lumpable duplication of ninetriangles
(both) — reproduce `delta == 0.0` exactly against the pre-fix binary. `MemCorrection`'s base branch
keeps its two global sums rather than regrouping per module, so even the floating-point summation
order is preserved.

**One source of truth for the teleport augmentation.** The correction has to charge its substitution
against the same enter/exit rates the scorer used, *including* the recorded-teleportation additions —
re-deriving them from the link-only rates would be silently wrong on exactly the flow models
(`--regularized`, `--recorded-teleportation`) where it is hardest to notice. So the teleport preamble is
extracted into `ColumnarTwoLevel::buildStackTerms()`, which both `hierarchicalCodelengthFromStack()` and
the new `leafCodebookRates()` consume, rather than written twice.

**Cost.** One O(K) pass over level-1 modules per stack scoring — plus, under recorded teleportation
only, a second run of the teleport preamble's pass over the leaves — on the cold
(per-candidate-structural-operator) path, and only when a `MemCorrection` is attached under L\*. The
per-candidate move arithmetic is untouched — the leaf move loop is deliberately base-flavoured under
L\* (see `--non-redundant-exact`), so `initMoveLoop`/`moveDelta`/`applyMove`/`mergeDelta` keep the
coefficient-1 form by design.

