## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = total wall for all 10 trials, minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. The network set spans base (undirected + directed), metadata, multilayer and state/memory objectives; see [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details. Columnar column = the default columnar search (`-C`).

> **This PR is a reporting-correctness fix**: the columnar engine now reports **its own objective**
> everywhere, not the object-oriented one. It computes its codelength on the stack, so everything that
> read the materialized `InfoNode` tree was reading a different objective — or nothing at all:
> jazz `-C` printed a per-level table of `0.000000` next to `Best codelength 6.862755928`; netsci
> `-C --non-redundant -N10` printed a table totalling `4.103756` next to a headline of `3.892209764`;
> `-C --no-infomap -c <ragged.tree> --non-redundant` returned the **base** L (ninetriangles
> 3.458078031 for both objectives, against a true L\* of 3.237864808); the same fallback dropped
> `--preferred-number-of-modules` entirely; and the one-level collapse priced itself with a
> **zero-module** tree while installing **one** module. See F38 in
> [`columnar_wip/columnar-rethink-notes.md`](columnar_wip/columnar-rethink-notes.md).
>
> **Only one class of run changes value, and it is the last item.** Under `--entropy-corrected`, where
> the objective charges per node, the one-level collapse now costs `multiplier/(2*totalDegree)` more —
> the price of the partition it actually installs. Measured: an ER(80, 0.2) graph under
> `-C -N1 --entropy-corrected` goes 6.315339939 → 6.315739939 (= +1/2500 = exactly that term), with and
> without `--non-redundant`. Nothing else moves: **28 interleaved A/B configs**, 26 bit-identical, the
> two exceptions being that same collapse.
>
> **The tables below were re-measured on this binary against the pre-change one, interleaved, same
> session.** All 13 `-C -N10` configs of the main table are **bit-identical** in codelength, top modules
> and levels, and so are all four higher-order `--non-redundant` configs — which also let the three
> rows the previous PR left marked STALE be **replaced with measured values** (malaria 7.427572783,
> air30k 5.378912606, air30k regularized 5.568875163). The `time` columns are **carried over from the
> previous session** and are not re-measured here: re-measuring only the columnar column would make each
> row's OO/columnar ratio cross-session, which is worse than a stated carry-over. The time evidence for
> this PR is the same-session A/B below instead.
>
> **Speed: one config over the 1% line, and it is reproducible.** Minimum and median of 11–15
> interleaved repetitions, CPU time (`user+sys`), `-N10` unless stated:
>
> | config | min | median |
> |---|--:|--:|
> | malaria `-C` | **+0.68%** | **+1.35%** |
> | malaria `-C -N1` | +0.00% | +0.00% |
> | air30k `-C` | −2.17% | +0.26% |
> | air30k `-C -N1` | −5.26% | +0.00% |
> | science2001 `-d -C` | +0.34% | +0.00% |
> | web-NotreDame `-d -C -N1` | −0.38% | −1.13% |
> | web-NotreDame `-d -C -2 -N1` | +1.26% | +0.00% |
> | powergrid `-C` | +4.35% (0.23→0.24s, one tick) | +0.00% |
>
> Malaria is the only one that survives repetition: a **control** run of the pre-change binary against a
> byte-identical copy of itself, same harness, gives +0.00% / −0.34%, so the harness has no position
> bias. Gating the per-trial breakdown+stamping off halves it (+0.68% / +0.67%); gating the columnar
> one-level value off as well leaves +1.02% / +0.33%, i.e. the residual is not cleanly attributable and
> is partly code layout. `sample` on the full binary puts `codelengthBreakdownFromStack` and
> `oneLevelCodelength` at 1 sample each out of 253, consistent with the measured magnitude. The added
> work is one extra stack scoring per trial (the breakdown) plus one O(leaves × depth) walk to stamp it
> onto the tree; the one-level value is computed **once per run**, not per trial. **Nothing is added to
> the move loop or to any per-candidate delta.**

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
> the worker leaf-input fix #997, the master sync bringing in #998, the non-redundant map equation #1001,
> the file split #1003, the physical-node codebook rate under L\* #1010) lives in each
> of those PRs, whose own copy of this file is the measurement that justified it; the running narrative
> is in [`columnar_wip/columnar-rethink-notes.md`](columnar_wip/columnar-rethink-notes.md). The last
> section below — "What this PR changes: the reporting paths, and one collapse price" — is the evidence
> for **this** PR.

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
`pr1001-lstar-meta` in the run log). The four higher-order L\* codelengths were marked stale by the
previous PR (which changed them and could not reach the networks); they are **measured** here on both
the pre-change and the post-change binary and are identical on the two, so they are this PR's numbers
as much as the previous one's.

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
<tr><td>malaria</td><td>multilayer</td><td align="right">7.397501710</td><td align="right">2</td><td align="right">3</td><td align="right">7.427572783</td><td align="right">2</td><td align="right">3</td><td align="right">−0.4%</td></tr>
<tr><td>air30k</td><td>state/memory</td><td align="right">5.392425413</td><td align="right">22</td><td align="right">3</td><td align="right">5.378912606</td><td align="right">22</td><td align="right">3</td><td align="right">−2.0%</td></tr>
<tr><td>air30k (reg.)</td><td>state/memory</td><td align="right">5.576242406</td><td align="right">11</td><td align="right">3</td><td align="right">5.568875163</td><td align="right">11</td><td align="right">3</td><td align="right">−3.3%</td></tr>
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

### What this PR changes: the reporting paths, and one collapse price

The columnar core computes its codelength **on the stack**. Everything that read the materialized
`InfoNode` tree was therefore reading a different objective — or nothing at all. `InfoNode::codelength`
has exactly one writer, `calcCodelengthOnTree`, and it is the **base** map equation; `initTree`'s
`maxDepth == 2 || twoLevel` shortcut never calls it, and a flat `.clu` reaches `initPartition` without
going through `initTree` at all.

| symptom | before | after |
|---|--:|--:|
| jazz `-C -N10`, per-level table Total | `0.000000` | `6.862756` (= `Best codelength`) |
| politicalblogs `-C -d -N2`, level-1 index bits | `0.000156` (the *previous* trial's value, leaked) | a real 79-module index term |
| netsci `-C --non-redundant -N10`, table Total | `4.103756` (base L) | `3.892210` (= the L\* headline) |
| ninetriangles `-C --no-infomap -c <ragged.tree> --non-redundant` | 3.458078031 (= base L) | **3.237864808** |
| … `+ --entropy-corrected` | 3.688847262 (base L + bias) | **3.468634039** |
| … `+ --preferred-number-of-modules 5` | 3.458078031 (penalty gone) | **5.458078031** |
| states `-C --no-infomap -c <ragged state tree> --non-redundant` | 2.078071905 (= base L) | **1.928856578** |
| ER(80, 0.2) `-C -N1 --entropy-corrected` (one-level collapse) | 6.315339939 | **6.315739939** |

**Why a ragged tree can be fixed for free under L\*, and only under L\*.** L\* is exactly invariant
under inserting a pass-through (single-child) level: the parent's enter codebook is
`e*(plogp(e) - plogp(e))/e == 0` and the child's leave-one-out exit term has numerator
`plogp(x) - 0 - plogp(x) == 0`. The base map equation charges `plogp(x+e) - plogp(e) - plogp(x) > 0` for
the same node. Measured on ninetriangles with one such level above every leaf module: base
3.38583082 → 3.97958082, L\* 3.078067323 → bit-identical. So the evaluation rectangularizes a ragged
tree by repeating each short path's own finest module id — **gated on `--non-redundant`**, and the
rectangularity guard inside `seedHierarchyFromLeafPaths` stays strict, because it is what keeps the base
scorer honest.

`--entropy-corrected` is the exception, and it is handled by construction rather than by formula: the
phantom nodes are marked while the leaf chains are walked, and *their breakdown entries* come off the
total. Same number as the analytic `padNodes*multiplier/(2*totalDegree)` discount (ninetriangles ragged
3.468634039 = 3.237864808 + 36/156, one pad node less than the hand-padded tree's 3.475044295 =
+37/156), but it stays right if another correction ever charges per node.

**The fallback that remains is now complete.** For every objective *except* L\*, a ragged tree still
falls back to `calcCodelengthOnTree`, which is exact there — checked pairwise between the engines on a
rectangular tree for base, `-d`, `-d --recorded-teleportation`, `--markov-time 1.5`,
`--variable-markov-time`, `--entropy-corrected`, `--meta-data` and `--regularized`: all agree.
`--preferred-number-of-modules` is the **only** one that does not (7.38583082 vs 3.38583082), because it
has no object-oriented counterpart at all — so its `|K - K_pref|` penalty is added to the fallback value
from the tree's own leaf-module count.

**The one-level collapse now prices the partition it installs.** `getOneLevelCodelength()` is
`calcCodelength` on a tree with **zero** modules; the collapse installs **one**. Under
`--entropy-corrected` those differ by exactly `multiplier/(2*totalDegree)` (ninetriangles 4.918622452
vs 4.925032709), and the fallback demonstrably fires there. Identical for the base map equation with no
corrections, which is why it had survived. The object-oriented path keeps the zero-module convention at
the analogous site — that is a master question, not one to settle on this branch.

A visible consequence, stated so it is not read as a bug: on such a run the summary now prints
`One-level codelength 6.315339939` above `Best codelength 6.315739939`, i.e. **best > one-level**, and
`Relative savings -0.01%`. The printed reference is the ZERO-module tree, which is not a partition at
all; the collapsed one-module partition genuinely costs `multiplier/(2*totalDegree)` more under a
per-node correction. The alternative — making the printed reference the one-module value — would move
`getRelativeCodelengthSavings()` and the OO/columnar comparison line for both engines, so it belongs to
the same master decision.

**What is deliberately *not* changed.** `getIndexCodelength()` becomes objective-correct only under
`--non-redundant`, where `getModuleCodelength()` was returning `L* - L_index`, a hybrid of two
objectives. Under `--entropy-corrected` the columnar root charge and the objective's own index term
differ by `multiplier/(2*totalDegree)` — but so do the **object-oriented engine's own two answers**,
since its per-level table charges the root `calcCodelength(m_root)` while `getIndexCodelength()` returns
the objective's bookkeeping (twotriangles `--entropy-corrected`: 0.214286 vs 0.178571). Picking one here
would only make the two engines differ, so the differential test stays exactly as it was.

**Known remaining gap, out of scope here.** With `--num-trials > 1` and a best trial that is not the
last, `restoreBestResult` re-materializes the winner through `initTree` and the flat shortcut leaves it
unscored again — so the **rewritten output file** loses the stamping (jazz `-C -N10 -o json`:
`sum(modules[].codelength) = 0.531` against a codelength of 6.862755928). The **console** table is
unaffected, because it is captured as a string from the live tree of the winning trial. This is the
shared half of #1002 and hits the object-oriented engine identically (`--two-level -N3` on ninetriangles
sums 0.936 against 3.518); it belongs to the `initTree` fix on master, and the columnar re-stamp on
restore is a one-line follow-up on top of it.
