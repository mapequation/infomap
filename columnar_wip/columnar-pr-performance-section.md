## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`); `time` is `--timing-json`'s `timing.total_s`. One run per `-N10` row (deterministic; `instr` carries the comparison); interleaved minimum of 3 for `-N1` rows. Driver and every row: [`columnar_wip/bench-dissolve.py`](columnar_wip/bench-dissolve.py), [`columnar_wip/dissolve-ab-results.tsv`](columnar_wip/dissolve-ab-results.tsv).

> **This PR closes the ways `-C -d` lost to `-C -2d` on the overlapping family (#1041).** Three changes,
> all in the hierarchical searches (`optimizeColumnar`, `optimizeFlexible`) and their fallback, all
> silent on every healthy row at `-N10`. **(1)** A flat-first trial completes its flat pipeline when the
> probe's own regroup ladder *escalated*, regardless of the 0.5% margin (F21): escalation is the
> detector's verdict that the trial sits in the group-hysteresis basin (F42), where the probe undersells
> the completed pipeline (om8 E100000: estimate 7.257 against 7.000 completed) and the fine-blocks
> up-build is the same greedy machinery. **(2)** A hierarchical build whose *unrefined* codelength is
> already worse than one module is abandoned instead of refined — refinement only squeezes such a build
> under the one-level bound without leaving the basin (om2 `-d --regularized -N1`: 9.85 → 7.49 refined,
> 6.95 from the two-level search) — so the trial falls to the one-level fallback. Gated by the
> fallback's own predicate, so the preferred-modules bias keeps refining, and on `numTrials > 1`, so the
> lone trial of a `-N1` run is refined as before (F56 second addendum: the abandonment is free only when
> a flat-first sibling supplies the flat answer). **(3)** A run in which *every*
> trial ended at the fallback runs the two-level search once, after the trial loop, on the first
> trial's engine seed, and keeps it when it beats the collapse; deep repair and dissolve then treat it
> as any flat winner. `-N1` on such a row returns exactly what `-2 -N1` returns; a run in which any
> trial escaped is untouched in bits by construction and only loses the abandoned trials' refinement
> time. Traces, rejected variants, the `-N1` phase breakdown and the #1042 diagnosis are F56 in
> `columnar-rethink-notes.md`.

> **Old** = a fresh `MODE=release OPENMP=0` build of `columnar-hierarchical-core` tip `a02dc105`, md5
> `b0b878cca7afa6976779d02cb3851fb1` (byte-identical to the #1078 snapshot's binary; the tip since then is
> docs-only); **new** = this PR at `40d2521d`, md5 `1fcfc3ea904344bd032793b07a1b418c`. One session, arms
> interleaved per row, `-N1` rows as the minimum of 3 reps spread across the batch, `-N10` rows once.
> **The object-oriented arms are not re-run** — this PR does not touch that engine (CLAUDE.md) — their
> cells in the two OO tables are carried from the #1079-day session on the same machine, at the
> precision printed there; the columnar cells next to them are this session's.

### What the change moves

Every configuration where old and new differ in bits, both arms. Every move is on the columnar arm.

| network | table | old bits | new bits | Δbits | old instr | new instr | Δinstr | old time | new time | Δtime |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| overlapping om3 `-d` | `-C -N1` | 7.97489927 | **6.823513368** | **-14.4376%** | 8.3G | 22.2G | +169.12% | 0.800s | 1.96s | +145.7% |
| overlapping om4 `-d` | `-C -N1` | 7.9829318 | **6.861724654** | **-14.0451%** | 7.1G | 23.5G | +230.83% | 0.692s | 2.16s | +212.5% |
| overlapping om8 `-d` | `-C` | 6.959413622 | **6.887234466** | **-1.0371%** | 85.8G | 87.3G | +1.82% | 9.36s | 9.76s | +4.3% |
| overlapping om3 `-d` | `-C -F -N1` | 7.97489927 | **6.823513368** | **-14.4376%** | 7.8G | 21.8G | +178.96% | 0.776s | 1.94s | +149.6% |
| overlapping om4 `-d` | `-C -F -N1` | 7.9829318 | **6.861724654** | **-14.0451%** | 4.8G | 21.2G | +341.05% | 0.505s | 1.99s | +293.9% |
| overlapping om8 `-d` | `-C -F` | 6.981469446 | **6.887234466** | **-1.3498%** | 65.9G | 75.6G | +14.78% | 7.48s | 8.49s | +13.6% |
| om2 `-d --regularized -N1` | family | 7.970508085 | **7.548816177** | **-5.2907%** | 4.3G | 11.5G | +168.92% | 0.398s | 1.09s | +173.2% |
| om3 `-d --regularized -N1` | family | 7.97414175 | **7.261268486** | **-8.9398%** | 5.5G | 13.7G | +149.19% | 0.544s | 1.32s | +142.6% |
| om4 `-d --regularized -N1` | family | 7.9828492 | **7.556894677** | **-5.3359%** | 7.3G | 15.0G | +103.49% | 0.749s | 1.50s | +100.8% |
| om5 `-d --regularized -N1` | family | 7.989613065 | **7.966995214** | **-0.2831%** | 6.7G | 31.6G | +370.99% | 0.705s | 3.41s | +384.5% |
| om6 `-d --regularized -N1` | family | 7.993490371 | **7.981574549** | **-0.1491%** | 7.8G | 21.7G | +180.38% | 0.815s | 2.38s | +191.6% |
| om7 `-d --regularized -N1` | family | 7.992395554 | **7.945571863** | **-0.5859%** | 7.3G | 35.2G | +382.70% | 0.776s | 3.98s | +412.4% |
| om8 `-d --regularized -N1` | family | 7.994735672 | **7.978912396** | **-0.1979%** | 9.4G | 30.0G | +219.29% | 1.00s | 3.42s | +240.7% |

**Every cell where new is worse than old, and why.** No cell is worse in bits.

- **Time on the rescued `-N1` rows (+101% to +412% wall, +103% to +383% instr, every one with a bits gain
  of 0.15% to 14.4%).** The old run returned a single module on each of them: om3 / om4 plain and `-F`,
  and all seven `-d --regularized` rows. The new run pays the collapsed hierarchical attempt (as before,
  refinement included: the abandonment is off at `-N1`) plus one `-2d -N1` solve and its once-per-run
  deep repair, and returns exactly the `-2d -N1` answer. Against `-2d -N1` on the same network the
  surcharge is the collapsed attempt alone (F56 addendum, phase breakdown). Half of om5's 3.4 s is the
  deep repair every `-2d -N1` pays, worth 5% in bits on om4 (7.2309 → 6.8617).
- **om8 `-d -N10` +4.3% wall / +1.8% instr for −1.037% bits; om8 `-F -N10` +13.6% / +14.8% for
  −1.350%.** The escalated probe now completes the flat pipeline in the five flat-first trials; under
  `-F` that completion is a larger share of a cheaper trial. A bits gain on both.
- **om6 / om7 / om8 E50000 `-d --regularized -N10`: +2.3% / +2.7% / +3.3% instr, same bits, wall −4%
  to +1%.** The same completions, on the density where the regularized planted partition is worse than
  one-level (F55): the completed flat candidates tie the hierarchical answer instead of beating it, so
  the work does not pay here; the removed refinement of the abandoned builds roughly cancels it in
  wall time.
- **Two wall-time outliers on flat instruction counts, re-measured interleaved after the session (two
  reps each):** air30k (reg.) `-C -2 -N10` read +40.0% wall on +0.27% instr — re-measured old 4.12 /
  4.16 s, new 4.24 / 4.06 s, 41.23G both; powergrid `-C -N10` read +10.3% on +0.23% — re-measured
  0.245 / 0.244 s against 0.251 / 0.244 s, 2.80G against 2.81G. Both are the session's load (1-minute
  load 5–23), not the change.
- **Every other cell with new wall above old** (politicalblogs +3%, multilayer +3%, netsci `-2` +5%,
  web-NotreDame `-2` +3%, om3 E50000 `-2d` +2%, ninetriangles / netsci `-N1` +3–6%, om5 E50000 / om7
  `-2d --regularized -N10` +2%) **sits on an instruction delta below 1%**, the noise floor of this
  session; the sub-millisecond rows carry startup variance. The `-C -2` table is bit-identical
  throughout: the two-level pipeline is untouched.

**Where the time goes the other way, same bits:** every `-N10` row whose trials had a build above
one-level lost that build's refinement — air30k −13%, air30k reg −13%, malaria −16%, om2 E100000 `-d`
−45%, om3 / om4 / om5 `-d` −31% / −24% / −24%, the family's `-d --regularized -N10` rows −21% to −46%
in instructions, `-F` on om3 / om4 / om5 −15% to −27%. At `-N1` the abandonment is off (no flat-first
sibling), so malaria, air30k, air30k (reg.), om5 and every other non-collapsing `-N1` row is
bit-identical to old at the old cost.

### Per-feature attribution (experiment binary at the tip, env-gated arms)

The two features and the three rejected shapes, measured on one `COLUMNAR_DEBUG` build of the tip with
env switches, arms interleaved per row. Bits are exact; the time column is a single run each on a
loaded machine and carries the sign only — the shipped build's instruction counts are in the tables
below. `esc` alone is what fixes om8 `-d -N10`; the rescue is what fixes the `-N1` collapses; the shipped
run-level rescue has the "every trial" column's `-N1` bits with none of its `-N10` cost (F56).

| row | tip bits | esc | esc + rescue (every trial) | probe every trial + esc | unconditional completion |
|---|--:|--:|--:|--:|--:|
| om8 -d -N10 | 6.959413622 | -1.0371% / +5% s | -1.0371% / +2% s | -1.0371% / +18% s | -1.0371% / +3% s |
| om4 -d -N1 | 7.9829318 | = / -0% s | -14.0451% / +209% s | -14.0451% / +154% s | -14.0451% / +139% s |
| om3 -d -N1 | 7.97489927 | — | -14.4376% / +155% s | — | — |
| om8 -d -N1 | 6.967535764 | = / -22% s | = / -22% s | -1.0643% / +130% s | -1.0643% / +132% s |
| om3 -d -N10 | 6.822826504 | = / +5% s | = / +32% s | = / -2% s | = / +7% s |
| om4 -d -N10 | 6.866901786 | = / +14% s | = / +35% s | = / +16% s | = / +17% s |
| om7 -d -N10 | 6.888473273 | = / -5% s | = / +2% s | +0.0112% / -2% s | +0.0112% / +4% s |
| malaria -N10 | 7.392442593 | = / +1% s | = / +1% s | +0.1083% / +22% s | +0.1083% / +3% s |
| air30k -N10 | 5.392285003 | = / +10% s | = / +2% s | = / +24% s | = / +22% s |
| air30k -d --regularized -N10 | 5.574537176 | = / -0% s | = / -2% s | = / +15% s | = / +17% s |
| wikispeedia -d -N10 | 5.907904741 | = / -5% s | = / +7% s | -0.0057% / +10% s | -0.0057% / +13% s |
| science2001 -d -N10 | 7.807937174 | = / +1% s | = / -2% s | = / +1% s | +0.3427% / -11% s |
| powergrid -N10 | 4.717760238 | = / -0% s | = / +44% s | = / +3% s | +6.3717% / +2% s |
| netsci -N10 | 4.048857953 | = / -1% s | = / -2% s | = / -1% s | +0.9537% / -41% s |
| web-NotreDame -d -N10 | 5.556421705 | = / +4% s | = / +10% s | = / +0% s | = / +57% s |

The two ways of making the rescue cheaper, on the experiment binary carrying esc + the run-level rescue
(min of 3 for `-N1`, single runs at `-N10`): option 1 — abandon a build whose unrefined codelength is
above one-level — ships; option 2 — seed the rescue's two-level search from the collapsed trial's
pass-1 blocks — saves nothing and moves om4 by +1.35%, rejected. The science2001 pref `-N1` cell is the
ungated experiment (the bias's unrefined builds all read far above one-level); the shipped rule obeys
the fallback's predicate and leaves that row untouched. F56 addendum has the phase breakdown.

| row | before (esc + run-level rescue) | option 1: abandon doomed builds, Δbits / Δs | option 2: seeded rescue, Δbits / Δs | max unrefined build / one-level |
|---|--:|--:|--:|--:|
| om3 -d -N1 | 6.823513368 · 2.07s | = / -26% | **+0.0134%** / +1% | 1.1286 |
| om4 -d -N1 | 6.861724654 · 2.24s | = / -14% | **+1.3529%** / +25% | 1.0738 |
| om4 -F -N1 | 6.867511578 · 1.84s | = / -25% | **-0.0222%** / -1% | NA |
| om8 -d -N1 | 6.967535764 · 0.85s | = / +4% | = / +9% | 0.8951 |
| om2 -d --regularized -N1 | 7.488616285 · 0.49s | **-7.1910%** / +114% | = / -1% | 1.2358 |
| om3 -d --regularized -N1 | 7.261268486 · 1.37s | = / -11% | **+0.0148%** / -2% | 1.1878 |
| om4 -d --regularized -N1 | 7.556894677 · 1.59s | = / -18% | **+0.0070%** / +4% | 1.1788 |
| om5 -d --regularized -N1 | 7.966995214 · 3.65s | = / -5% | **-0.0077%** / -12% | 1.1563 |
| om6 -d --regularized -N1 | 7.981574549 · 2.65s | = / -12% | **-0.0225%** / +24% | 1.1624 |
| om7 -d --regularized -N1 | 7.945571863 · 4.35s | = / -2% | **-0.0090%** / -17% | 1.1480 |
| om8 -d --regularized -N1 | 7.978912396 · 3.78s | = / -18% | **-0.0185%** / -16% | 1.1560 |
| om2 E50000 -d -N1 | 7.29196634 · 0.38s | = / -1% | — | 0.9320 |
| om5 -d -N1 | 7.812252898 · 0.78s | **-12.0852%** / +183% | — | 1.0079 |
| om6 -d -N1 | 7.440161581 · 0.83s | = / -2% | — | 0.9557 |
| om7 -d -N1 | 7.192756466 · 0.85s | = / -0% | — | 0.9215 |
| malaria -N1 | 7.491980364 · 0.38s | **+0.4533%** / +12% | — | 1.0328 |
| air30k -N1 | 5.470440768 · 0.39s | **-1.4066%** / +72% | — | 1.0542 |
| air30k -d --regularized -N1 | 5.657913279 · 0.50s | **-1.1764%** / +51% | — | 1.1176 |
| air30k meta -N1 | 7.546335898 · 0.95s | = / -2% | — | 0.7263 |
| science2001 pref -d -N1 | 8.460796773 · 0.42s | **+5146.3447%** / -35% | — | 13.1502 |
| science2001 -d -N1 | 7.807937174 · 0.41s | = / -1% | — | 0.8073 |
| netsci -N1 | 4.047459862 · 0.00s | = / -6% | — | 0.4949 |
| powergrid -N1 | 4.730850312 · 0.03s | = / -3% | — | 0.4283 |
| wikispeedia -d -N1 | 6.066305904 · 0.07s | = / +1% | — | 0.8611 |
| web-NotreDame -d -N1 | 5.556421705 · 2.43s | = / +1% | — | 0.3802 |
| om8 -d -N10 | 6.887234466 · 10.34s | = / -1% | — | 0.8957 |
| om4 -d -N10 | 6.866901786 · 8.53s | = / -24% | — | 1.0764 |
| ninetriangles -N10 | 3.371875026 · 0.00s | = / -10% | — | 0.7135 |
| jazz -N10 | 6.862755928 · 0.01s | = / +3% | — | 0.9495 |
| netsci -N10 | 4.048857953 · 0.02s | = / -5% | — | 0.4979 |
| powergrid -N10 | 4.717760238 · 0.24s | = / +54% | — | 0.4297 |
| politicalblogs -d -N10 | 6.740943136 · 0.06s | = / -0% | — | 0.9130 |
| science2001 -d -N10 | 7.807937174 · 3.14s | = / -3% | — | 0.8100 |
| lazega meta -N10 | 6.017860269 · 0.00s | = / -3% | — | 0.9136 |
| multilayer -N10 | 2.011405238 · 0.00s | = / +6% | — | 0.8933 |
| malaria -N10 | 7.392442593 · 3.37s | = / -16% | — | 1.0328 |
| air30k -N10 | 5.392285003 · 4.27s | = / -11% | — | 1.0585 |
| air30k -d --regularized -N10 | 5.574537176 · 4.44s | = / -9% | — | 1.1184 |
| air30k meta -N10 | 7.421664324 · 10.50s | = / +0% | — | 0.7320 |
| science2001 pref -d -N10 | 8.235585529 · 3.59s | = / -28% | — | 13.4761 |
| wikispeedia -d -N10 | 5.907904741 · 0.77s | = / -5% | — | 0.8631 |
| web-NotreDame -d -N10 | 5.556421705 · 21.19s | = / +2% | — | 0.3802 |

### Old vs new columnar — standard search (`-C -N10`)

Overlapping and wikispeedia rows run `-C -d -N10`. The only bits move is om8 E100000, where the escalated
probe now completes the flat pipeline in the five flat-first trials; every other row is bit-identical.
Time moves down wherever a trial's build started above one-level and is no longer refined (the memory
objectives: malaria, air30k, the om rows).

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar <code>-C</code> (old)</th>
<th colspan="5">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-1.9%)</td><td align="right">0.1G (+0.55%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (-1.7%)</td><td align="right">0.1G (+0.02%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.048857953 (=)</td><td align="right">0.022s (+0.5%)</td><td align="right">0.3G (+0.11%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.248s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.717760238 (=)</td><td align="right">0.273s (+10.3%)</td><td align="right">2.8G (+0.23%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.058s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.059s (+3.2%)</td><td align="right">0.7G (+0.31%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.10s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">3.07s (-0.8%)</td><td align="right">34.1G (+0.24%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">20.2s</td><td align="right">181.3G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">20.2s (+0.3%)</td><td align="right">181.8G (+0.24%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-2.4%)</td><td align="right">0.1G (-5.63%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+3.3%)</td><td align="right">0.1G (-0.25%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">3.18s</td><td align="right">37.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.65s (-16.7%)</td><td align="right">31.5G (-16.06%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.87s</td><td align="right">44.5G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.34s (-13.7%)</td><td align="right">38.6G (-13.30%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.47s</td><td align="right">46.5G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">4.23s (-5.5%)</td><td align="right">40.4G (-13.08%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.69s</td><td align="right">97.8G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">9.75s (+0.6%)</td><td align="right">97.9G (+0.07%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.19s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.21s (+0.5%)</td><td align="right">35.5G (-0.03%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.67s</td><td align="right">47.4G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">4.76s (+1.8%)</td><td align="right">47.5G (+0.12%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822826504</td><td align="right">7.89s</td><td align="right">88.4G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822826504 (=)</td><td align="right">5.45s (-31.0%)</td><td align="right">61.4G (-30.59%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">7.73s</td><td align="right">80.7G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">6.22s (-19.6%)</td><td align="right">61.5G (-23.79%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">8.37s</td><td align="right">81.4G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">6.44s (-23.1%)</td><td align="right">61.8G (-24.05%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">8.58s</td><td align="right">83.1G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">8.64s (+0.7%)</td><td align="right">83.2G (+0.09%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.888473273</td><td align="right">9.17s</td><td align="right">86.4G</td><td align="right">680</td><td align="right">2</td><td align="right">6.888473273 (=)</td><td align="right">9.11s (-0.6%)</td><td align="right">86.5G (+0.12%)</td><td align="right">680</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.959413622</td><td align="right">9.36s</td><td align="right">85.8G</td><td align="right">203</td><td align="right">4</td><td align="right">6.887234466 (-1.0371%)</td><td align="right">9.76s (+4.3%)</td><td align="right">87.3G (+1.82%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">6.773456578</td><td align="right">5.61s</td><td align="right">61.3G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.41s (-39.2%)</td><td align="right">33.5G (-45.29%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.851498646</td><td align="right">4.45s</td><td align="right">44.5G</td><td align="right">617</td><td align="right">4</td><td align="right">5.851498646 (=)</td><td align="right">4.48s (+0.7%)</td><td align="right">44.5G (+0.12%)</td><td align="right">617</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.876717868</td><td align="right">5.26s</td><td align="right">47.4G</td><td align="right">1031</td><td align="right">4</td><td align="right">4.876717868 (=)</td><td align="right">5.23s (-0.6%)</td><td align="right">47.4G (+0.11%)</td><td align="right">1031</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.150346795</td><td align="right">6.02s</td><td align="right">51.1G</td><td align="right">2171</td><td align="right">4</td><td align="right">4.150346795 (=)</td><td align="right">5.76s (-4.3%)</td><td align="right">51.1G (+0.07%)</td><td align="right">2171</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.617750514</td><td align="right">5.87s</td><td align="right">51.6G</td><td align="right">3050</td><td align="right">4</td><td align="right">3.617750514 (=)</td><td align="right">5.88s (+0.1%)</td><td align="right">51.6G (+0.14%)</td><td align="right">3050</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.201872271</td><td align="right">7.78s</td><td align="right">65.0G</td><td align="right">3424</td><td align="right">6</td><td align="right">3.201872271 (=)</td><td align="right">7.54s (-3.0%)</td><td align="right">65.1G (+0.07%)</td><td align="right">3424</td><td align="right">6</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.883308449</td><td align="right">7.94s</td><td align="right">69.6G</td><td align="right">4367</td><td align="right">5</td><td align="right">2.883308449 (=)</td><td align="right">7.94s (-0.1%)</td><td align="right">69.7G (+0.11%)</td><td align="right">4367</td><td align="right">5</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.710s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.721s (+1.5%)</td><td align="right">7.2G (+0.20%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Overlapping and wikispeedia rows as `-C -2d -N10`. Neither change touches the two-level pipeline, so every
row is bit-identical; the time column is the session's noise floor.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar <code>-C</code> (old)</th>
<th colspan="5">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-9.4%)</td><td align="right">0.1G (-0.29%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.861229775</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (=)</td><td align="right">0.007s (+1.6%)</td><td align="right">0.1G (-0.15%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.283072584</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.283072584 (=)</td><td align="right">0.009s (+4.7%)</td><td align="right">0.2G (-0.47%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.63729688</td><td align="right">0.096s</td><td align="right">1.1G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.097s (+1.1%)</td><td align="right">1.1G (+0.01%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739575295</td><td align="right">0.042s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">6.739575295 (=)</td><td align="right">0.042s (+0.4%)</td><td align="right">0.5G (-0.02%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.949978834</td><td align="right">2.32s</td><td align="right">23.8G</td><td align="right">506</td><td align="right">2</td><td align="right">7.949978834 (=)</td><td align="right">2.33s (+0.8%)</td><td align="right">23.8G (-0.02%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.754216663</td><td align="right">18.7s</td><td align="right">117.5G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.754216663 (=)</td><td align="right">19.3s (+3.3%)</td><td align="right">117.5G (+0.02%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-12.2%)</td><td align="right">0.1G (-6.43%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-9.0%)</td><td align="right">0.1G (-9.13%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.400445378</td><td align="right">2.69s</td><td align="right">32.3G</td><td align="right">168</td><td align="right">2</td><td align="right">7.400445378 (=)</td><td align="right">2.69s (-0.1%)</td><td align="right">32.3G (-0.00%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393055049</td><td align="right">4.08s</td><td align="right">41.9G</td><td align="right">334</td><td align="right">2</td><td align="right">5.393055049 (=)</td><td align="right">3.90s (-4.5%)</td><td align="right">41.9G (-0.03%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571539329</td><td align="right">3.97s</td><td align="right">41.2G</td><td align="right">304</td><td align="right">2</td><td align="right">5.571539329 (=)</td><td align="right">5.56s (+40.0%)</td><td align="right">41.3G (+0.27%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.424143707</td><td align="right">10.4s</td><td align="right">104.7G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.424143707 (=)</td><td align="right">10.3s (-0.7%)</td><td align="right">104.7G (+0.00%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.08s</td><td align="right">31.6G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.08s (-0.0%)</td><td align="right">31.6G (-0.02%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739271968</td><td align="right">3.96s</td><td align="right">39.8G</td><td align="right">638</td><td align="right">2</td><td align="right">6.739271968 (=)</td><td align="right">3.88s (-2.2%)</td><td align="right">39.8G (-0.02%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.822826504</td><td align="right">6.92s</td><td align="right">78.8G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822826504 (=)</td><td align="right">6.89s (-0.5%)</td><td align="right">78.8G (-0.01%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.866901786</td><td align="right">7.01s</td><td align="right">71.5G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">7.13s (+1.8%)</td><td align="right">71.5G (-0.00%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.866617805</td><td align="right">7.16s</td><td align="right">71.3G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">7.22s (+0.9%)</td><td align="right">71.3G (+0.01%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884862145</td><td align="right">7.92s</td><td align="right">71.0G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">7.96s (+0.5%)</td><td align="right">71.0G (-0.00%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.889244164</td><td align="right">7.95s</td><td align="right">73.4G</td><td align="right">675</td><td align="right">2</td><td align="right">6.889244164 (=)</td><td align="right">8.01s (+0.7%)</td><td align="right">73.4G (+0.00%)</td><td align="right">675</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.887234466</td><td align="right">8.95s</td><td align="right">75.4G</td><td align="right">921</td><td align="right">2</td><td align="right">6.887234466 (=)</td><td align="right">8.45s (-5.6%)</td><td align="right">75.4G (-0.04%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-2d`</td><td align="right">6.773456578</td><td align="right">3.74s</td><td align="right">38.0G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.78s (+1.0%)</td><td align="right">38.0G (+0.01%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-2d`</td><td align="right">6.258611497</td><td align="right">4.40s</td><td align="right">36.8G</td><td align="right">3236</td><td align="right">2</td><td align="right">6.258611497 (=)</td><td align="right">4.49s (+2.1%)</td><td align="right">36.8G (+0.02%)</td><td align="right">3236</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 E50000 `-2d`</td><td align="right">5.454385527</td><td align="right">4.19s</td><td align="right">33.7G</td><td align="right">4381</td><td align="right">2</td><td align="right">5.454385527 (=)</td><td align="right">4.15s (-0.8%)</td><td align="right">33.7G (+0.01%)</td><td align="right">4381</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 E50000 `-2d`</td><td align="right">4.903270512</td><td align="right">4.11s</td><td align="right">31.9G</td><td align="right">5443</td><td align="right">2</td><td align="right">4.903270512 (=)</td><td align="right">4.06s (-1.3%)</td><td align="right">31.9G (+0.01%)</td><td align="right">5443</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 E50000 `-2d`</td><td align="right">4.468778176</td><td align="right">4.43s</td><td align="right">32.4G</td><td align="right">6379</td><td align="right">2</td><td align="right">4.468778176 (=)</td><td align="right">4.34s (-2.1%)</td><td align="right">32.4G (-0.01%)</td><td align="right">6379</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 E50000 `-2d`</td><td align="right">4.143277395</td><td align="right">4.26s</td><td align="right">31.3G</td><td align="right">7097</td><td align="right">2</td><td align="right">4.143277395 (=)</td><td align="right">4.14s (-2.9%)</td><td align="right">31.3G (-0.01%)</td><td align="right">7097</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 E50000 `-2d`</td><td align="right">3.859069372</td><td align="right">4.25s</td><td align="right">31.6G</td><td align="right">7985</td><td align="right">2</td><td align="right">3.859069372 (=)</td><td align="right">4.18s (-1.6%)</td><td align="right">31.5G (-0.02%)</td><td align="right">7985</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.907904741</td><td align="right">0.656s</td><td align="right">6.9G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.658s (+0.4%)</td><td align="right">6.9G (-0.01%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

Interleaved minimum of 3 per arm. This is where the run-level rescue fires: a `-d -N1` row whose only
trial collapsed to one module now returns the `-2d -N1` answer, and pays the collapsed hierarchical
attempt plus that two-level solve and its deep repair. Every other `-N1` row is the old refined build,
bit-identical (F56 second addendum).

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar <code>-C</code> (old)</th>
<th colspan="5">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.000s (+5.5%)</td><td align="right">0.1G (-0.59%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.899367957</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">2</td><td align="right">6.899367957 (=)</td><td align="right">0.001s (-2.5%)</td><td align="right">0.1G (-0.75%)</td><td align="right">11</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.047459862</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">4</td><td align="right">4.047459862 (=)</td><td align="right">0.003s (+2.5%)</td><td align="right">0.1G (-0.61%)</td><td align="right">7</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.730850312</td><td align="right">0.025s</td><td align="right">0.4G</td><td align="right">12</td><td align="right">5</td><td align="right">4.730850312 (=)</td><td align="right">0.025s (-2.1%)</td><td align="right">0.4G (-0.10%)</td><td align="right">12</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.758265349</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">3</td><td align="right">3</td><td align="right">6.758265349 (=)</td><td align="right">0.009s (+0.7%)</td><td align="right">0.2G (-0.21%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">0.389s</td><td align="right">5.7G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">0.385s (-1.0%)</td><td align="right">5.7G (-0.03%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">2.37s</td><td align="right">24.3G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">2.34s (-1.0%)</td><td align="right">24.3G (-0.12%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.041117399</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.041117399 (=)</td><td align="right">0.001s (+0.5%)</td><td align="right">0.1G (-1.55%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-3.9%)</td><td align="right">0.1G (-0.33%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.491980364</td><td align="right">0.354s</td><td align="right">5.3G</td><td align="right">148</td><td align="right">3</td><td align="right">7.491980364 (=)</td><td align="right">0.358s (+1.0%)</td><td align="right">5.3G (-0.03%)</td><td align="right">148</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.470440768</td><td align="right">0.361s</td><td align="right">4.5G</td><td align="right">242</td><td align="right">3</td><td align="right">5.470440768 (=)</td><td align="right">0.365s (+1.0%)</td><td align="right">4.5G (+0.01%)</td><td align="right">242</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.657913279</td><td align="right">0.473s</td><td align="right">5.9G</td><td align="right">197</td><td align="right">3</td><td align="right">5.657913279 (=)</td><td align="right">0.470s (-0.5%)</td><td align="right">5.9G (-0.00%)</td><td align="right">197</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.546335898</td><td align="right">0.865s</td><td align="right">9.2G</td><td align="right">1614</td><td align="right">3</td><td align="right">7.546335898 (=)</td><td align="right">0.857s (-1.0%)</td><td align="right">9.2G (-0.02%)</td><td align="right">1614</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.460796773</td><td align="right">0.403s</td><td align="right">5.8G</td><td align="right">5</td><td align="right">3</td><td align="right">8.460796773 (=)</td><td align="right">0.399s (-1.0%)</td><td align="right">5.8G (-0.04%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739358212</td><td align="right">1.54s</td><td align="right">17.3G</td><td align="right">674</td><td align="right">2</td><td align="right">6.739358212 (=)</td><td align="right">1.56s (+0.9%)</td><td align="right">17.3G (-0.02%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.823513368</td><td align="right">1.25s</td><td align="right">15.0G</td><td align="right">72</td><td align="right">2</td><td align="right">6.823513368 (=)</td><td align="right">1.24s (-0.3%)</td><td align="right">15.0G (-0.01%)</td><td align="right">72</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.861724654</td><td align="right">1.61s</td><td align="right">17.5G</td><td align="right">141</td><td align="right">2</td><td align="right">6.861724654 (=)</td><td align="right">1.58s (-1.8%)</td><td align="right">17.5G (-0.05%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.868127142</td><td align="right">1.81s</td><td align="right">19.1G</td><td align="right">293</td><td align="right">2</td><td align="right">6.868127142 (=)</td><td align="right">1.81s (-0.1%)</td><td align="right">19.1G (+0.03%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884042436</td><td align="right">1.78s</td><td align="right">18.0G</td><td align="right">455</td><td align="right">2</td><td align="right">6.884042436 (=)</td><td align="right">1.79s (+0.8%)</td><td align="right">18.0G (+0.04%)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.889332374</td><td align="right">2.05s</td><td align="right">20.1G</td><td align="right">651</td><td align="right">2</td><td align="right">6.889332374 (=)</td><td align="right">2.05s (+0.1%)</td><td align="right">20.1G (+0.01%)</td><td align="right">651</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.893377041</td><td align="right">2.26s</td><td align="right">22.0G</td><td align="right">911</td><td align="right">2</td><td align="right">6.893377041 (=)</td><td align="right">2.30s (+1.9%)</td><td align="right">22.0G (-0.02%)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.29196634</td><td align="right">0.364s</td><td align="right">3.7G</td><td align="right">321</td><td align="right">4</td><td align="right">7.29196634 (=)</td><td align="right">0.363s (-0.2%)</td><td align="right">3.7G (+0.01%)</td><td align="right">321</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">7.97489927</td><td align="right">0.800s</td><td align="right">8.3G</td><td align="right">1</td><td align="right">2</td><td align="right">6.823513368 (-14.4376%)</td><td align="right">1.96s (+145.7%)</td><td align="right">22.2G (+169.12%)</td><td align="right">72</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">7.9829318</td><td align="right">0.692s</td><td align="right">7.1G</td><td align="right">1</td><td align="right">2</td><td align="right">6.861724654 (-14.0451%)</td><td align="right">2.16s (+212.5%)</td><td align="right">23.5G (+230.83%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.812252898</td><td align="right">0.746s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.812252898 (=)</td><td align="right">0.734s (-1.5%)</td><td align="right">7.2G (-0.09%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.440161581</td><td align="right">0.792s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.440161581 (=)</td><td align="right">0.779s (-1.6%)</td><td align="right">7.5G (+0.01%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.192756466</td><td align="right">0.812s</td><td align="right">7.5G</td><td align="right">149</td><td align="right">4</td><td align="right">7.192756466 (=)</td><td align="right">0.805s (-0.9%)</td><td align="right">7.5G (-0.05%)</td><td align="right">149</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.967535764</td><td align="right">0.808s</td><td align="right">7.4G</td><td align="right">206</td><td align="right">4</td><td align="right">6.967535764 (=)</td><td align="right">0.802s (-0.8%)</td><td align="right">7.4G (+0.03%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">7.144517469</td><td align="right">0.416s</td><td align="right">4.5G</td><td align="right">3</td><td align="right">3</td><td align="right">7.144517469 (=)</td><td align="right">0.417s (+0.1%)</td><td align="right">4.5G (-0.05%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.854829799</td><td align="right">0.426s</td><td align="right">4.3G</td><td align="right">537</td><td align="right">4</td><td align="right">5.854829799 (=)</td><td align="right">0.430s (+1.0%)</td><td align="right">4.3G (+0.11%)</td><td align="right">537</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.898784516</td><td align="right">0.432s</td><td align="right">4.2G</td><td align="right">1867</td><td align="right">3</td><td align="right">4.898784516 (=)</td><td align="right">0.435s (+0.7%)</td><td align="right">4.2G (+0.01%)</td><td align="right">1867</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.1554786</td><td align="right">0.544s</td><td align="right">5.0G</td><td align="right">2156</td><td align="right">4</td><td align="right">4.1554786 (=)</td><td align="right">0.549s (+0.9%)</td><td align="right">5.0G (-0.00%)</td><td align="right">2156</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.620157111</td><td align="right">0.675s</td><td align="right">5.9G</td><td align="right">3056</td><td align="right">4</td><td align="right">3.620157111 (=)</td><td align="right">0.682s (+1.0%)</td><td align="right">5.9G (-0.14%)</td><td align="right">3056</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.21639819</td><td align="right">0.625s</td><td align="right">5.6G</td><td align="right">3417</td><td align="right">5</td><td align="right">3.21639819 (=)</td><td align="right">0.621s (-0.7%)</td><td align="right">5.6G (-0.01%)</td><td align="right">3417</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.885785338</td><td align="right">0.914s</td><td align="right">8.1G</td><td align="right">4371</td><td align="right">5</td><td align="right">2.885785338 (=)</td><td align="right">0.908s (-0.6%)</td><td align="right">8.1G (-0.16%)</td><td align="right">4371</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om2 `-2d -c` planted</td><td align="right">6.744721993</td><td align="right">0.552s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.744721993 (=)</td><td align="right">0.548s (-0.7%)</td><td align="right">6.2G (-0.04%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d -c` planted</td><td align="right">6.820421208</td><td align="right">0.773s</td><td align="right">9.1G</td><td align="right">62</td><td align="right">2</td><td align="right">6.820421208 (=)</td><td align="right">0.772s (-0.1%)</td><td align="right">9.1G (-0.03%)</td><td align="right">62</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d -c` planted</td><td align="right">6.856239474</td><td align="right">0.928s</td><td align="right">10.3G</td><td align="right">132</td><td align="right">2</td><td align="right">6.856239474 (=)</td><td align="right">0.926s (-0.2%)</td><td align="right">10.3G (-0.04%)</td><td align="right">132</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d -c` planted</td><td align="right">6.857778113</td><td align="right">1.16s</td><td align="right">12.5G</td><td align="right">296</td><td align="right">2</td><td align="right">6.857778113 (=)</td><td align="right">1.15s (-0.8%)</td><td align="right">12.5G (-0.02%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d -c` planted</td><td align="right">6.873378755</td><td align="right">1.28s</td><td align="right">13.2G</td><td align="right">446</td><td align="right">2</td><td align="right">6.873378755 (=)</td><td align="right">1.25s (-1.8%)</td><td align="right">13.2G (-0.06%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d -c` planted</td><td align="right">6.878354613</td><td align="right">1.91s</td><td align="right">19.8G</td><td align="right">650</td><td align="right">2</td><td align="right">6.878354613 (=)</td><td align="right">1.90s (-0.8%)</td><td align="right">19.8G (-0.02%)</td><td align="right">650</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d -c` planted</td><td align="right">6.875237392</td><td align="right">1.41s</td><td align="right">14.2G</td><td align="right">894</td><td align="right">2</td><td align="right">6.875237392 (=)</td><td align="right">1.40s (-0.1%)</td><td align="right">14.2G (-0.02%)</td><td align="right">894</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">6.066305904</td><td align="right">0.072s</td><td align="right">0.8G</td><td align="right">187</td><td align="right">3</td><td align="right">6.066305904 (=)</td><td align="right">0.070s (-2.5%)</td><td align="right">0.8G (+0.06%)</td><td align="right">187</td><td align="right">3</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.092s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.091s (-0.9%)</td><td align="right">1.0G (-0.09%)</td><td align="right">184</td><td align="right">2</td></tr>
</tbody>
</table>

### The overlapping family in full

Every configuration of the planted overlapping state networks, both arms, now at both trigram densities
(F55). The `-d --regularized -N1` rows all collapsed to one module on the old binary and are rescued on
the new one; the `-N10` rows have a flat-first trial that escapes and are bit-identical.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar <code>-C</code> (old)</th>
<th colspan="5">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">om2 E100000 `-2d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">3.85s</td><td align="right">37.8G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.92s (+1.8%)</td><td align="right">37.8G (+0.01%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 E100000 `-d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">6.18s</td><td align="right">64.1G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.64s (-41.1%)</td><td align="right">34.5G (-46.16%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.67s</td><td align="right">25.0G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.68s (+0.2%)</td><td align="right">25.1G (+0.02%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N1`</td><td align="right">7.548816177</td><td align="right">0.800s</td><td align="right">8.2G</td><td align="right">120</td><td align="right">2</td><td align="right">7.548816177 (=)</td><td align="right">0.796s (-0.5%)</td><td align="right">8.2G (+0.06%)</td><td align="right">120</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">3.65s</td><td align="right">36.4G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.78s (-23.8%)</td><td align="right">24.1G (-33.75%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N1`</td><td align="right">7.970508085</td><td align="right">0.398s</td><td align="right">4.3G</td><td align="right">1</td><td align="right">2</td><td align="right">7.548816177 (-5.2907%)</td><td align="right">1.09s (+173.2%)</td><td align="right">11.5G (+168.92%)</td><td align="right">120</td><td align="right">2</td></tr>
<tr><td align="right">om2 planted, `-2d --no-infomap -c`</td><td align="right">6.789039995</td><td align="right">0.052s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.789039995 (=)</td><td align="right">0.053s (+0.4%)</td><td align="right">0.6G (+0.05%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N10`</td><td align="right">7.931196935</td><td align="right">3.14s</td><td align="right">27.9G</td><td align="right">49</td><td align="right">2</td><td align="right">7.931196935 (=)</td><td align="right">3.12s (-0.6%)</td><td align="right">27.9G (+0.01%)</td><td align="right">49</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N10`</td><td align="right">7.931196935</td><td align="right">3.70s</td><td align="right">36.4G</td><td align="right">49</td><td align="right">2</td><td align="right">7.931196935 (=)</td><td align="right">2.99s (-19.3%)</td><td align="right">28.1G (-22.89%)</td><td align="right">49</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">4.89s</td><td align="right">48.0G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">4.72s (-3.4%)</td><td align="right">48.0G (-0.03%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">0.917s</td><td align="right">9.7G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">0.923s (+0.7%)</td><td align="right">9.7G (-0.00%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">6.06s</td><td align="right">63.3G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">4.44s (-26.6%)</td><td align="right">45.0G (-28.96%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N1`</td><td align="right">7.97414175</td><td align="right">0.544s</td><td align="right">5.5G</td><td align="right">1</td><td align="right">2</td><td align="right">7.261268486 (-8.9398%)</td><td align="right">1.32s (+142.6%)</td><td align="right">13.7G (+149.19%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 planted, `-2d --no-infomap -c`</td><td align="right">6.837980937</td><td align="right">0.096s</td><td align="right">0.9G</td><td align="right">12</td><td align="right">2</td><td align="right">6.837980937 (=)</td><td align="right">0.093s (-2.2%)</td><td align="right">0.9G (-0.16%)</td><td align="right">12</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N10`</td><td align="right">7.940858042</td><td align="right">3.07s</td><td align="right">27.4G</td><td align="right">41</td><td align="right">2</td><td align="right">7.940858042 (=)</td><td align="right">3.06s (-0.2%)</td><td align="right">27.4G (-0.00%)</td><td align="right">41</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N10`</td><td align="right">7.940858042</td><td align="right">4.78s</td><td align="right">45.3G</td><td align="right">41</td><td align="right">2</td><td align="right">7.940858042 (=)</td><td align="right">3.06s (-35.9%)</td><td align="right">27.2G (-39.87%)</td><td align="right">41</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N10`</td><td align="right">7.556894677</td><td align="right">5.56s</td><td align="right">49.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">5.44s (-2.1%)</td><td align="right">49.1G (-0.01%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">0.963s</td><td align="right">9.3G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">0.952s (-1.1%)</td><td align="right">9.3G (-0.04%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N10`</td><td align="right">7.556653713</td><td align="right">6.82s</td><td align="right">67.5G</td><td align="right">78</td><td align="right">2</td><td align="right">7.556653713 (=)</td><td align="right">5.09s (-25.4%)</td><td align="right">48.7G (-27.93%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N1`</td><td align="right">7.9828492</td><td align="right">0.749s</td><td align="right">7.3G</td><td align="right">1</td><td align="right">2</td><td align="right">7.556894677 (-5.3359%)</td><td align="right">1.50s (+100.8%)</td><td align="right">15.0G (+103.49%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 planted, `-2d --no-infomap -c`</td><td align="right">6.880650147</td><td align="right">0.113s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.880650147 (=)</td><td align="right">0.110s (-2.8%)</td><td align="right">1.0G (-0.14%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N10`</td><td align="right">7.969030601</td><td align="right">3.26s</td><td align="right">29.0G</td><td align="right">26</td><td align="right">2</td><td align="right">7.969030601 (=)</td><td align="right">3.35s (+2.5%)</td><td align="right">29.0G (+0.06%)</td><td align="right">26</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N10`</td><td align="right">7.969030601</td><td align="right">5.22s</td><td align="right">45.2G</td><td align="right">26</td><td align="right">2</td><td align="right">7.969030601 (=)</td><td align="right">3.55s (-32.0%)</td><td align="right">28.3G (-37.53%)</td><td align="right">26</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N10`</td><td align="right">7.967531078</td><td align="right">6.12s</td><td align="right">55.1G</td><td align="right">98</td><td align="right">2</td><td align="right">7.967531078 (=)</td><td align="right">6.03s (-1.5%)</td><td align="right">55.1G (-0.02%)</td><td align="right">98</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N1`</td><td align="right">7.966995214</td><td align="right">2.89s</td><td align="right">26.6G</td><td align="right">104</td><td align="right">2</td><td align="right">7.966995214 (=)</td><td align="right">2.87s (-0.6%)</td><td align="right">26.6G (-0.01%)</td><td align="right">104</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N10`</td><td align="right">7.965009721</td><td align="right">9.70s</td><td align="right">87.7G</td><td align="right">106</td><td align="right">2</td><td align="right">7.965009721 (=)</td><td align="right">7.92s (-18.4%)</td><td align="right">69.4G (-20.92%)</td><td align="right">106</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N1`</td><td align="right">7.989613065</td><td align="right">0.705s</td><td align="right">6.7G</td><td align="right">1</td><td align="right">2</td><td align="right">7.966995214 (-0.2831%)</td><td align="right">3.41s (+384.5%)</td><td align="right">31.6G (+370.99%)</td><td align="right">104</td><td align="right">2</td></tr>
<tr><td align="right">om5 planted, `-2d --no-infomap -c`</td><td align="right">6.902222527</td><td align="right">0.125s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.902222527 (=)</td><td align="right">0.124s (-0.9%)</td><td align="right">1.1G (-0.18%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N10`</td><td align="right">7.959010571</td><td align="right">3.42s</td><td align="right">27.7G</td><td align="right">26</td><td align="right">2</td><td align="right">7.959010571 (=)</td><td align="right">3.38s (-1.3%)</td><td align="right">27.7G (+0.00%)</td><td align="right">26</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N10`</td><td align="right">7.959010571</td><td align="right">3.49s</td><td align="right">26.8G</td><td align="right">26</td><td align="right">2</td><td align="right">7.959010571 (=)</td><td align="right">3.35s (-4.1%)</td><td align="right">27.4G (+2.30%)</td><td align="right">26</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N10`</td><td align="right">7.981574549</td><td align="right">5.90s</td><td align="right">51.0G</td><td align="right">113</td><td align="right">2</td><td align="right">7.981574549 (=)</td><td align="right">5.91s (+0.1%)</td><td align="right">51.0G (-0.01%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N1`</td><td align="right">7.981574549</td><td align="right">1.82s</td><td align="right">15.8G</td><td align="right">113</td><td align="right">2</td><td align="right">7.981574549 (=)</td><td align="right">1.80s (-1.1%)</td><td align="right">15.8G (-0.03%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N10`</td><td align="right">7.981063575</td><td align="right">9.34s</td><td align="right">86.4G</td><td align="right">119</td><td align="right">2</td><td align="right">7.981063575 (=)</td><td align="right">8.15s (-12.7%)</td><td align="right">67.3G (-22.11%)</td><td align="right">119</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N1`</td><td align="right">7.993490371</td><td align="right">0.815s</td><td align="right">7.8G</td><td align="right">1</td><td align="right">2</td><td align="right">7.981574549 (-0.1491%)</td><td align="right">2.38s (+191.6%)</td><td align="right">21.7G (+180.38%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om6 planted, `-2d --no-infomap -c`</td><td align="right">6.930934993</td><td align="right">0.137s</td><td align="right">1.1G</td><td align="right">24</td><td align="right">2</td><td align="right">6.930934993 (=)</td><td align="right">0.133s (-3.2%)</td><td align="right">1.1G (+0.00%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N10`</td><td align="right">7.974818773</td><td align="right">3.57s</td><td align="right">29.9G</td><td align="right">23</td><td align="right">2</td><td align="right">7.974818773 (=)</td><td align="right">3.58s (+0.3%)</td><td align="right">29.9G (-0.01%)</td><td align="right">23</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N10`</td><td align="right">7.974818773</td><td align="right">3.58s</td><td align="right">28.2G</td><td align="right">23</td><td align="right">2</td><td align="right">7.974818773 (=)</td><td align="right">3.60s (+0.8%)</td><td align="right">28.9G (+2.68%)</td><td align="right">23</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N10`</td><td align="right">7.945712536</td><td align="right">7.58s</td><td align="right">60.5G</td><td align="right">184</td><td align="right">2</td><td align="right">7.945712536 (=)</td><td align="right">7.75s (+2.4%)</td><td align="right">60.5G (+0.01%)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N1`</td><td align="right">7.945571863</td><td align="right">3.41s</td><td align="right">29.8G</td><td align="right">187</td><td align="right">2</td><td align="right">7.945571863 (=)</td><td align="right">3.46s (+1.5%)</td><td align="right">29.8G (+0.03%)</td><td align="right">187</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N10`</td><td align="right">7.945712536</td><td align="right">9.22s</td><td align="right">84.4G</td><td align="right">184</td><td align="right">2</td><td align="right">7.945712536 (=)</td><td align="right">6.97s (-24.4%)</td><td align="right">60.4G (-28.44%)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N1`</td><td align="right">7.992395554</td><td align="right">0.776s</td><td align="right">7.3G</td><td align="right">1</td><td align="right">2</td><td align="right">7.945571863 (-0.5859%)</td><td align="right">3.98s (+412.4%)</td><td align="right">35.2G (+382.70%)</td><td align="right">187</td><td align="right">2</td></tr>
<tr><td align="right">om7 planted, `-2d --no-infomap -c`</td><td align="right">6.957516072</td><td align="right">0.157s</td><td align="right">1.3G</td><td align="right">28</td><td align="right">2</td><td align="right">6.957516072 (=)</td><td align="right">0.156s (-0.7%)</td><td align="right">1.3G (-0.11%)</td><td align="right">28</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N10`</td><td align="right">7.990101633</td><td align="right">3.23s</td><td align="right">27.0G</td><td align="right">8</td><td align="right">2</td><td align="right">7.990101633 (=)</td><td align="right">3.30s (+2.0%)</td><td align="right">27.0G (+0.03%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N10`</td><td align="right">7.990101633</td><td align="right">3.43s</td><td align="right">25.3G</td><td align="right">8</td><td align="right">2</td><td align="right">7.990101633 (=)</td><td align="right">3.46s (+0.9%)</td><td align="right">26.1G (+3.27%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N10`</td><td align="right">7.976140205</td><td align="right">8.33s</td><td align="right">69.9G</td><td align="right">240</td><td align="right">2</td><td align="right">7.976140205 (=)</td><td align="right">8.30s (-0.3%)</td><td align="right">69.9G (-0.01%)</td><td align="right">240</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N1`</td><td align="right">7.978912396</td><td align="right">2.66s</td><td align="right">22.5G</td><td align="right">234</td><td align="right">2</td><td align="right">7.978912396 (=)</td><td align="right">2.65s (-0.5%)</td><td align="right">22.5G (+0.01%)</td><td align="right">234</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N10`</td><td align="right">7.976681139</td><td align="right">9.17s</td><td align="right">80.9G</td><td align="right">256</td><td align="right">2</td><td align="right">7.976681139 (=)</td><td align="right">6.83s (-25.6%)</td><td align="right">55.3G (-31.65%)</td><td align="right">256</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N1`</td><td align="right">7.994735672</td><td align="right">1.00s</td><td align="right">9.4G</td><td align="right">1</td><td align="right">2</td><td align="right">7.978912396 (-0.1979%)</td><td align="right">3.42s (+240.7%)</td><td align="right">30.0G (+219.29%)</td><td align="right">234</td><td align="right">2</td></tr>
<tr><td align="right">om8 planted, `-2d --no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.144s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.143s (-0.5%)</td><td align="right">1.2G (-0.18%)</td><td align="right">32</td><td align="right">2</td></tr>
</tbody>
</table>

### `-F` on the family, old vs new

`optimizeFlexible` shares both changes. `-N10`:

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar <code>-C</code> (old)</th>
<th colspan="5">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">3.94s</td><td align="right">40.2G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">3.92s (-0.4%)</td><td align="right">40.3G (+0.14%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822826504</td><td align="right">7.33s</td><td align="right">78.3G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822826504 (=)</td><td align="right">5.26s (-28.2%)</td><td align="right">57.0G (-27.18%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.62s</td><td align="right">66.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.65s (-14.6%)</td><td align="right">56.9G (-14.65%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">7.26s</td><td align="right">70.9G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">5.87s (-19.1%)</td><td align="right">57.1G (-19.48%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">7.93s</td><td align="right">68.6G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">7.75s (-2.4%)</td><td align="right">68.6G (+0.11%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.888473273</td><td align="right">8.03s</td><td align="right">73.4G</td><td align="right">680</td><td align="right">2</td><td align="right">6.888473273 (=)</td><td align="right">7.98s (-0.6%)</td><td align="right">73.5G (+0.14%)</td><td align="right">680</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.981469446</td><td align="right">7.48s</td><td align="right">65.9G</td><td align="right">227</td><td align="right">4</td><td align="right">6.887234466 (-1.3498%)</td><td align="right">8.49s (+13.6%)</td><td align="right">75.6G (+14.78%)</td><td align="right">921</td><td align="right">2</td></tr>
</tbody>
</table>

`-N1`, interleaved minimum of 3:

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar <code>-C</code> (old)</th>
<th colspan="5">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.321678354</td><td align="right">0.259s</td><td align="right">2.6G</td><td align="right">436</td><td align="right">4</td><td align="right">7.321678354 (=)</td><td align="right">0.261s (+0.9%)</td><td align="right">2.6G (-0.01%)</td><td align="right">436</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">7.97489927</td><td align="right">0.776s</td><td align="right">7.8G</td><td align="right">1</td><td align="right">2</td><td align="right">6.823513368 (-14.4376%)</td><td align="right">1.94s (+149.6%)</td><td align="right">21.8G (+178.96%)</td><td align="right">72</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">7.9829318</td><td align="right">0.505s</td><td align="right">4.8G</td><td align="right">1</td><td align="right">2</td><td align="right">6.861724654 (-14.0451%)</td><td align="right">1.99s (+293.9%)</td><td align="right">21.2G (+341.05%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.798283664</td><td align="right">0.641s</td><td align="right">5.7G</td><td align="right">96</td><td align="right">4</td><td align="right">7.798283664 (=)</td><td align="right">0.639s (-0.3%)</td><td align="right">5.7G (+0.02%)</td><td align="right">96</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.507066407</td><td align="right">0.662s</td><td align="right">5.8G</td><td align="right">87</td><td align="right">4</td><td align="right">7.507066407 (=)</td><td align="right">0.649s (-2.1%)</td><td align="right">5.8G (+0.01%)</td><td align="right">87</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.238675817</td><td align="right">0.583s</td><td align="right">5.2G</td><td align="right">145</td><td align="right">4</td><td align="right">7.238675817 (=)</td><td align="right">0.592s (+1.5%)</td><td align="right">5.2G (+0.04%)</td><td align="right">145</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">7.022972054</td><td align="right">0.615s</td><td align="right">5.2G</td><td align="right">201</td><td align="right">4</td><td align="right">7.022972054 (=)</td><td align="right">0.590s (-4.1%)</td><td align="right">5.2G (-0.02%)</td><td align="right">201</td><td align="right">4</td></tr>
</tbody>
</table>

### The fast dial `-F`

`-F` skips the interior-layer refinement. Both columns are the new binary.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar <code>-C</code></th>
<th colspan="5">columnar <code>-C -F</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-6.0%)</td><td align="right">0.1G (-2.72%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (+0.9%)</td><td align="right">0.1G (-2.09%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.03324474 (-0.3856%)</td><td align="right">0.014s (-36.8%)</td><td align="right">0.2G (-29.19%)</td><td align="right">9</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.273s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.754013143 (+0.7684%)</td><td align="right">0.144s (-47.1%)</td><td align="right">1.7G (-37.73%)</td><td align="right">10</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.059s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.056s (-5.5%)</td><td align="right">0.7G (-3.97%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.07s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">2.85s (-7.2%)</td><td align="right">33.1G (-3.02%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">20.2s</td><td align="right">181.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.620539396 (+1.1539%)</td><td align="right">15.7s (-22.3%)</td><td align="right">126.8G (-30.25%)</td><td align="right">135</td><td align="right">5</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (+0.3%)</td><td align="right">0.1G (-1.75%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+20.7%)</td><td align="right">0.1G (+1.19%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.65s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.85s (+7.5%)</td><td align="right">30.6G (-2.59%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.34s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.30s (-1.0%)</td><td align="right">35.9G (-6.94%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.23s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.15s (-25.5%)</td><td align="right">35.2G (-12.75%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.75s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">9.93s (+1.8%)</td><td align="right">95.4G (-2.57%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.21s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.19s (-0.6%)</td><td align="right">34.7G (-2.20%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.76s</td><td align="right">47.5G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">3.92s (-17.5%)</td><td align="right">40.3G (-15.25%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822826504</td><td align="right">5.45s</td><td align="right">61.4G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822826504 (=)</td><td align="right">5.26s (-3.4%)</td><td align="right">57.0G (-7.09%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.22s</td><td align="right">61.5G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.65s (-9.1%)</td><td align="right">56.9G (-7.55%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">6.44s</td><td align="right">61.8G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">5.87s (-8.8%)</td><td align="right">57.1G (-7.62%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">8.64s</td><td align="right">83.2G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">7.75s (-10.3%)</td><td align="right">68.6G (-17.47%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.888473273</td><td align="right">9.11s</td><td align="right">86.5G</td><td align="right">680</td><td align="right">2</td><td align="right">6.888473273 (=)</td><td align="right">7.98s (-12.4%)</td><td align="right">73.5G (-15.01%)</td><td align="right">680</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.887234466</td><td align="right">9.76s</td><td align="right">87.3G</td><td align="right">921</td><td align="right">2</td><td align="right">6.887234466 (=)</td><td align="right">8.49s (-13.0%)</td><td align="right">75.6G (-13.38%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.721s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.660s (-8.4%)</td><td align="right">6.9G (-4.05%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### The non-redundant map equation L\* (`--non-redundant`)

L\* is a different objective, so a lower number is not a better partition of the same objective. Both
columns are the new binary.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar <code>-C</code></th>
<th colspan="5">columnar <code>-C --non-redundant</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.078067323 (-8.7135%)</td><td align="right">0.001s (+13.1%)</td><td align="right">0.1G (+0.68%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367 (+0.0797%)</td><td align="right">0.007s (+2.4%)</td><td align="right">0.1G (+0.14%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">3.892209764 (-3.8689%)</td><td align="right">0.023s (+3.6%)</td><td align="right">0.3G (+1.09%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.273s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.509265423 (-4.4194%)</td><td align="right">0.271s (-0.7%)</td><td align="right">2.9G (+3.30%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.059s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502 (+0.7165%)</td><td align="right">0.060s (+1.9%)</td><td align="right">0.7G (+2.14%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.07s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">8.009172258 (+2.5773%)</td><td align="right">2.77s (-9.8%)</td><td align="right">31.2G (-8.71%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">20.2s</td><td align="right">181.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.512433077 (-0.7917%)</td><td align="right">20.3s (+0.1%)</td><td align="right">182.9G (+0.62%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653 (-0.8182%)</td><td align="right">0.004s (-6.7%)</td><td align="right">0.1G (-0.99%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (-4.1040%)</td><td align="right">0.000s (-6.9%)</td><td align="right">0.1G (+0.04%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.65s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.438064454 (+0.6171%)</td><td align="right">2.87s (+8.2%)</td><td align="right">33.9G (+7.78%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.34s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.378824196 (-0.2496%)</td><td align="right">3.35s (+0.3%)</td><td align="right">38.4G (-0.40%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.23s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.56659036 (-0.1426%)</td><td align="right">3.95s (-6.6%)</td><td align="right">40.1G (-0.63%)</td><td align="right">220</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.75s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.192724425 (-3.0848%)</td><td align="right">8.99s (-7.8%)</td><td align="right">85.9G (-12.23%)</td><td align="right">1910</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.21s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451 (+2.5761%)</td><td align="right">3.19s (-0.7%)</td><td align="right">35.3G (-0.42%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.721s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.903208727 (-0.0795%)</td><td align="right">0.687s (-4.7%)</td><td align="right">6.9G (-4.39%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar

OO cells carried from the #1079-day session (not re-run, see above); columnar cells from this session.
air30k (meta) OO is `-N1` (it does not finish `-N10` in budget).

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">object-oriented (carried from the #1079-day session)</th>
<th colspan="5">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-79.8%)</td><td align="right">0.1G (-29.84%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">5</td><td align="right">2</td><td align="right">6.862755928 (-0.0042%)</td><td align="right">0.006s (-70.9%)</td><td align="right">0.1G (-55.05%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.026557116</td><td align="right">0.119s</td><td align="right">1.3G</td><td align="right">12</td><td align="right">5</td><td align="right">4.048857953 (+0.5538%)</td><td align="right">0.022s (-81.2%)</td><td align="right">0.3G (-76.49%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.736412597</td><td align="right">1.89s</td><td align="right">20.2G</td><td align="right">11</td><td align="right">6</td><td align="right">4.717760238 (-0.3938%)</td><td align="right">0.273s (-85.6%)</td><td align="right">2.8G (-86.11%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.738927979</td><td align="right">0.136s</td><td align="right">1.4G</td><td align="right">80</td><td align="right">3</td><td align="right">6.740943136 (+0.0299%)</td><td align="right">0.059s (-56.3%)</td><td align="right">0.7G (-49.85%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.805465772</td><td align="right">7.94s</td><td align="right">63.0G</td><td align="right">220</td><td align="right">4</td><td align="right">7.807937174 (+0.0317%)</td><td align="right">3.07s (-61.3%)</td><td align="right">34.1G (-45.81%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.55442136</td><td align="right">155.7s</td><td align="right">1115.4G</td><td align="right">766</td><td align="right">9</td><td align="right">5.556421705 (+0.0360%)</td><td align="right">20.2s (-87.0%)</td><td align="right">181.8G (-83.70%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-68.2%)</td><td align="right">0.1G (-49.64%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-46.5%)</td><td align="right">0.1G (-37.52%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502028585</td><td align="right">9.20s</td><td align="right">78.1G</td><td align="right">144</td><td align="right">3</td><td align="right">7.392442593 (-1.4608%)</td><td align="right">2.65s (-71.2%)</td><td align="right">31.5G (-59.73%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392441014</td><td align="right">11.9s</td><td align="right">120.6G</td><td align="right">251</td><td align="right">3</td><td align="right">5.392285003 (-0.0029%)</td><td align="right">3.34s (-71.9%)</td><td align="right">38.6G (-68.03%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578435633</td><td align="right">7.76s</td><td align="right">81.7G</td><td align="right">301</td><td align="right">3</td><td align="right">5.574537176 (-0.0699%)</td><td align="right">4.23s (-45.5%)</td><td align="right">40.4G (-50.57%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">8.432467909</td><td align="right">5.55s</td><td align="right">52.1G</td><td align="right">114</td><td align="right">4</td><td align="right">7.421664324 (-11.9870%)</td><td align="right">9.75s (+75.6%)</td><td align="right">97.9G (+87.91%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">7.938575228</td><td align="right">6.93s</td><td align="right">57.0G</td><td align="right">25</td><td align="right">4</td><td align="right">8.235585529 (+3.7414%)</td><td align="right">3.21s (-53.7%)</td><td align="right">35.5G (-37.72%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.88554258</td><td align="right">2.23s</td><td align="right">22.4G</td><td align="right">184</td><td align="right">3</td><td align="right">5.907904741 (+0.3800%)</td><td align="right">0.721s (-67.7%)</td><td align="right">7.2G (-67.79%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar — two-level (`-2`)

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">object-oriented (carried from the #1079-day session)</th>
<th colspan="5">columnar <code>-C -2</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-52.4%)</td><td align="right">0.1G (-37.43%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">5</td><td align="right">2</td><td align="right">6.861229775 (-0.0265%)</td><td align="right">0.007s (-44.8%)</td><td align="right">0.1G (-33.13%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.285012668</td><td align="right">0.031s</td><td align="right">0.4G</td><td align="right">56</td><td align="right">2</td><td align="right">4.283072584 (-0.0453%)</td><td align="right">0.009s (-71.1%)</td><td align="right">0.2G (-60.75%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.600443859</td><td align="right">0.698s</td><td align="right">5.9G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.097s (-86.1%)</td><td align="right">1.1G (-80.54%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739721413</td><td align="right">0.168s</td><td align="right">0.8G</td><td align="right">80</td><td align="right">2</td><td align="right">6.739575295 (-0.0022%)</td><td align="right">0.042s (-74.9%)</td><td align="right">0.5G (-35.28%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.9500396</td><td align="right">4.41s</td><td align="right">29.3G</td><td align="right">496</td><td align="right">2</td><td align="right">7.949978834 (-0.0008%)</td><td align="right">2.33s (-47.1%)</td><td align="right">23.8G (-18.63%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.742988533</td><td align="right">45.2s</td><td align="right">251.8G</td><td align="right">11809</td><td align="right">2</td><td align="right">6.754216663 (+0.1665%)</td><td align="right">19.3s (-57.2%)</td><td align="right">117.5G (-53.35%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-51.6%)</td><td align="right">0.1G (-3.39%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-37.3%)</td><td align="right">0.1G (-38.92%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.50595639</td><td align="right">6.58s</td><td align="right">55.2G</td><td align="right">142</td><td align="right">2</td><td align="right">7.400445378 (-1.4057%)</td><td align="right">2.69s (-59.1%)</td><td align="right">32.3G (-41.53%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393312779</td><td align="right">4.70s</td><td align="right">43.9G</td><td align="right">332</td><td align="right">2</td><td align="right">5.393055049 (-0.0048%)</td><td align="right">3.90s (-17.0%)</td><td align="right">41.9G (-4.58%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.579216889</td><td align="right">5.84s</td><td align="right">58.6G</td><td align="right">301</td><td align="right">2</td><td align="right">5.571539329 (-0.1376%)</td><td align="right">5.56s (-4.7%)</td><td align="right">41.3G (-29.46%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.131110023</td><td align="right">5.04s</td><td align="right">36.7G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (+1.2849%)</td><td align="right">3.08s (-39.0%)</td><td align="right">31.6G (-14.01%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.892212121</td><td align="right">1.70s</td><td align="right">17.2G</td><td align="right">184</td><td align="right">2</td><td align="right">5.907904741 (+0.2663%)</td><td align="right">0.658s (-61.3%)</td><td align="right">6.9G (-59.79%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>
