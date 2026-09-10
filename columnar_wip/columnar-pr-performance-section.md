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
> fallback's own predicate, so the preferred-modules bias keeps refining. **(3)** A run in which *every*
> trial ended at the fallback runs the two-level search once, after the trial loop, on the first
> trial's engine seed, and keeps it when it beats the collapse; deep repair and dissolve then treat it
> as any flat winner. `-N1` on such a row returns exactly what `-2 -N1` returns; a run in which any
> trial escaped is untouched in bits by construction and only loses the abandoned trials' refinement
> time. Traces, rejected variants, the `-N1` phase breakdown and the #1042 diagnosis are F56 in
> `columnar-rethink-notes.md`.

> **Old** = a fresh `MODE=release OPENMP=0` build of `columnar-hierarchical-core` tip `a02dc105`, md5
> `b0b878cca7afa6976779d02cb3851fb1` (byte-identical to the #1078 snapshot's binary; the tip since then is
> docs-only); **new** = this PR at `ea513645`, md5 `fe6fb58d5cc3ff9231247d8c8f18a6c4`. One session, arms
> interleaved per row, `-N1` rows as the minimum of 3 reps spread across the batch, `-N10` rows once.
> **The object-oriented arms are not re-run** — this PR does not touch that engine (CLAUDE.md) — their
> cells in the two OO tables are carried from the #1079-day session on the same machine, at the
> precision printed there; the columnar cells next to them are this session's.

### What the change moves

Every configuration where old and new differ in bits, both arms. Every move is on the columnar arm.

| network | table | old bits | new bits | Δbits | old instr | new instr | Δinstr | old time | new time | Δtime |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| air30k | `-C -N1` | 5.470440768 | **5.393492268** | **-1.4066%** | 4.5G | 7.5G | +64.53% | 0.369s | 0.646s | +75.2% |
| air30k (reg.) | `-C -N1` | 5.657913279 | **5.591354826** | **-1.1764%** | 5.9G | 8.1G | +38.69% | 0.491s | 0.720s | +46.6% |
| malaria | `-C -N1` | 7.491980364 | **7.525940462** | **+0.4533%** | 5.3G | 5.7G | +7.56% | 0.373s | 0.412s | +10.4% |
| overlapping om2 E100000 `-d` | `-C -N1` | 7.144517469 | **6.774049681** | **-5.1853%** | 4.5G | 8.1G | +77.73% | 0.419s | 0.776s | +85.0% |
| overlapping om3 `-d` | `-C -N1` | 7.97489927 | **6.823513368** | **-14.4376%** | 8.3G | 16.9G | +104.64% | 0.826s | 1.48s | +79.6% |
| overlapping om4 `-d` | `-C -N1` | 7.9829318 | **6.861724654** | **-14.0451%** | 7.1G | 19.8G | +179.23% | 0.713s | 1.91s | +168.5% |
| overlapping om5 `-d` | `-C -N1` | 7.812252898 | **6.868127142** | **-12.0852%** | 7.2G | 21.4G | +196.40% | 0.763s | 2.10s | +175.8% |
| overlapping om8 `-d` | `-C` | 6.959413622 | **6.887234466** | **-1.0371%** | 85.8G | 87.3G | +1.80% | 9.55s | 9.80s | +2.6% |
| overlapping om3 `-d` | `-C -F -N1` | 7.97489927 | **6.823513368** | **-14.4376%** | 7.8G | 16.4G | +110.74% | 0.793s | 1.47s | +85.7% |
| overlapping om4 `-d` | `-C -F -N1` | 7.9829318 | **6.861724654** | **-14.0451%** | 4.8G | 19.4G | +302.76% | 0.512s | 1.85s | +260.8% |
| overlapping om5 `-d` | `-C -F -N1` | 7.798283664 | **6.868127142** | **-11.9277%** | 5.7G | 20.9G | +263.85% | 0.658s | 2.07s | +215.2% |
| overlapping om8 `-d` | `-C -F` | 6.981469446 | **6.887234466** | **-1.3498%** | 65.9G | 75.6G | +14.76% | 7.61s | 8.67s | +13.9% |
| om2 `-d --regularized -N1` | family | 7.970508085 | **7.548816177** | **-5.2907%** | 4.3G | 9.3G | +116.84% | 0.411s | 0.928s | +125.7% |
| om3 `-d --regularized -N1` | family | 7.97414175 | **7.261268486** | **-8.9398%** | 5.5G | 11.7G | +113.61% | 0.561s | 1.17s | +108.2% |
| om4 `-d --regularized -N1` | family | 7.9828492 | **7.556894677** | **-5.3359%** | 7.3G | 11.4G | +55.76% | 0.779s | 1.25s | +60.6% |
| om5 `-d --regularized -N1` | family | 7.989613065 | **7.966995214** | **-0.2831%** | 6.7G | 28.7G | +328.88% | 0.728s | 3.24s | +345.5% |
| om6 `-d --regularized -N1` | family | 7.993490371 | **7.981574549** | **-0.1491%** | 7.8G | 17.9G | +131.02% | 0.840s | 2.14s | +155.0% |
| om7 `-d --regularized -N1` | family | 7.992395554 | **7.945571863** | **-0.5859%** | 7.3G | 31.9G | +337.62% | 0.801s | 3.75s | +368.6% |
| om8 `-d --regularized -N1` | family | 7.994735672 | **7.978912396** | **-0.1979%** | 9.4G | 24.6G | +161.71% | 1.00s | 3.00s | +199.2% |

**Every cell where new is worse than old, and why.**

- **Bits, one cell: malaria `-C -N1` +0.4533% (7.491980364 → 7.525940462), +7.6% instr.** Its single
  trial's build starts 3.3% above one-level; on this network alone the refinement of that doomed build
  (7.4920) beats the two-level search's answer (7.5259, exactly `-C -2 -N1`). Nothing in the
  build/one-level ratio separates it from om5 (ratio 1.008, where the flat answer is 12% better), so the
  rule ships without a margin and this is its price (F56 addendum). `-N10` malaria is bit-identical and
  16% faster.
- **Time on the rescued `-N1` rows (+47% to +369% wall, +39% to +338% instr, every one with a bits gain
  of 0.15% to 14.4%).** The old run returned a single module (om3 / om4 plain and `-F`, all seven
  `-d --regularized` rows) or a refined hierarchy in the wrong basin (om2 E100000 7.14, om5 7.81, air30k
  5.47, air30k reg 5.66); the new run pays the abandoned attempt (pass-1 sweep and two up-builds, no
  refinement) plus one `-2d -N1` solve and its once-per-run deep repair, and returns exactly the
  `-2d -N1` answer. Against `-2d -N1` on the same network the surcharge is the abandoned attempt alone:
  +14% (om5), +21% (om2 reg), +32% (air30k reg) — F56 addendum, phase breakdown. Half of om5's 2.10 s is
  the deep repair every `-2d -N1` pays, worth 5% in bits (om4: 7.2309 → 6.8617).
- **om8 `-d -N10` +2.6% wall / +1.8% instr for −1.037% bits; om8 `-F -N10` +13.9% / +14.8% for
  −1.350%.** The escalated probe now completes the flat pipeline in the five flat-first trials; under
  `-F` that completion is a larger share of a cheaper trial. A bits gain on both.
- **om6 / om7 / om8 E50000 `-d --regularized -N10`: +2.3% / +2.7% / +3.3% instr, same bits, wall −0.9%
  to −2.1%.** The same completions, on the density where the regularized planted partition is worse
  than one-level (F55): the completed flat candidates tie the hierarchical answer instead of beating it,
  so the work does not pay here; the removed refinement of the abandoned builds roughly cancels it in
  wall time.
- **om2 E50000 `-F -N10` read +94% wall on +1.07% instr in the sweep.** Re-measured interleaved right
  after the session, two reps each: old 4.29 / 4.22 s, new 4.14 / 4.14 s, instructions 40.2G / 40.3G,
  bits identical. The sweep's wall was the load spike (1-minute load 38 while the test builds drained);
  the cell's real cost is +0.25% instr, the escalated probe completing in `-F`'s flat-first trials.
- **Every other cell with new wall above old** (ninetriangles +9%, multilayer +7%, jazz +4%, om4 `-2d`
  +6%, om2 `-d -N1` +4%, om2 / om4 `-2d --regularized -N10` +2–4%, om2 `-F -N1` +7%) **sits on an
  instruction delta below 1%**; the session ran at 1-minute load 7–38 and the sub-millisecond rows carry
  startup variance. By this file's convention those are noise, not regressions. The `-C -2` table is
  bit-identical throughout: the two-level pipeline is untouched.

**Where the time goes the other way, same bits:** every `-N10` row whose trials had a build above
one-level lost that build's refinement — air30k −13%, air30k reg −13%, malaria −16%, om2 E100000 `-d`
−45%, om3 / om4 / om5 `-d` −31% / −24% / −24%, the family's `-d --regularized -N10` rows −21% to −46%
in instructions, `-F` on om3 / om4 / om5 −15% to −27%.

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (+9.1%)</td><td align="right">0.1G (-0.53%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.007s (+4.1%)</td><td align="right">0.1G (+0.20%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.048857953 (=)</td><td align="right">0.022s (+0.1%)</td><td align="right">0.3G (+0.11%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.238s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.717760238 (=)</td><td align="right">0.238s (+0.3%)</td><td align="right">2.8G (+0.21%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.058s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.058s (+0.1%)</td><td align="right">0.7G (+0.18%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.01s</td><td align="right">34.0G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">3.02s (+0.2%)</td><td align="right">34.1G (+0.25%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">19.8s</td><td align="right">181.3G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">19.8s (-0.1%)</td><td align="right">181.8G (+0.24%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (+2.6%)</td><td align="right">0.1G (-6.02%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+7.4%)</td><td align="right">0.1G (-0.43%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">3.15s</td><td align="right">37.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.64s (-16.4%)</td><td align="right">31.5G (-16.06%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">4.24s</td><td align="right">44.5G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.57s (-15.8%)</td><td align="right">38.6G (-13.30%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.32s</td><td align="right">46.5G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.79s (-12.4%)</td><td align="right">40.4G (-13.13%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.86s</td><td align="right">97.8G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">9.85s (-0.1%)</td><td align="right">97.9G (+0.07%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.29s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.28s (-0.4%)</td><td align="right">35.5G (-0.03%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.66s</td><td align="right">47.4G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">4.62s (-0.7%)</td><td align="right">47.5G (+0.10%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822826504</td><td align="right">8.34s</td><td align="right">88.4G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822826504 (=)</td><td align="right">5.70s (-31.6%)</td><td align="right">61.4G (-30.60%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">8.65s</td><td align="right">80.8G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">6.28s (-27.3%)</td><td align="right">61.5G (-23.86%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">8.36s</td><td align="right">81.4G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">6.52s (-22.0%)</td><td align="right">61.8G (-24.05%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">8.87s</td><td align="right">83.1G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">9.02s (+1.7%)</td><td align="right">83.2G (+0.13%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.888473273</td><td align="right">9.34s</td><td align="right">86.4G</td><td align="right">680</td><td align="right">2</td><td align="right">6.888473273 (=)</td><td align="right">9.44s (+1.0%)</td><td align="right">86.5G (+0.12%)</td><td align="right">680</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.959413622</td><td align="right">9.55s</td><td align="right">85.8G</td><td align="right">203</td><td align="right">4</td><td align="right">6.887234466 (-1.0371%)</td><td align="right">9.80s (+2.6%)</td><td align="right">87.3G (+1.80%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">6.773456578</td><td align="right">5.42s</td><td align="right">61.3G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.29s (-39.2%)</td><td align="right">33.5G (-45.27%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.851498646</td><td align="right">4.62s</td><td align="right">44.5G</td><td align="right">617</td><td align="right">4</td><td align="right">5.851498646 (=)</td><td align="right">4.59s (-0.8%)</td><td align="right">44.6G (+0.17%)</td><td align="right">617</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.876717868</td><td align="right">5.34s</td><td align="right">47.4G</td><td align="right">1031</td><td align="right">4</td><td align="right">4.876717868 (=)</td><td align="right">5.38s (+0.8%)</td><td align="right">47.4G (+0.13%)</td><td align="right">1031</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.150346795</td><td align="right">5.86s</td><td align="right">51.1G</td><td align="right">2171</td><td align="right">4</td><td align="right">4.150346795 (=)</td><td align="right">5.92s (+0.9%)</td><td align="right">51.1G (+0.15%)</td><td align="right">2171</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.617750514</td><td align="right">6.04s</td><td align="right">51.6G</td><td align="right">3050</td><td align="right">4</td><td align="right">3.617750514 (=)</td><td align="right">6.07s (+0.6%)</td><td align="right">51.7G (+0.18%)</td><td align="right">3050</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.201872271</td><td align="right">7.75s</td><td align="right">65.1G</td><td align="right">3424</td><td align="right">6</td><td align="right">3.201872271 (=)</td><td align="right">7.62s (-1.7%)</td><td align="right">65.1G (+0.07%)</td><td align="right">3424</td><td align="right">6</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.883308449</td><td align="right">7.90s</td><td align="right">69.6G</td><td align="right">4367</td><td align="right">5</td><td align="right">2.883308449 (=)</td><td align="right">8.16s (+3.3%)</td><td align="right">69.7G (+0.13%)</td><td align="right">4367</td><td align="right">5</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.730s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.728s (-0.2%)</td><td align="right">7.2G (+0.19%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-6.2%)</td><td align="right">0.1G (-0.83%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.861229775</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (=)</td><td align="right">0.007s (+0.0%)</td><td align="right">0.1G (-0.15%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.283072584</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.283072584 (=)</td><td align="right">0.009s (-0.8%)</td><td align="right">0.2G (-0.05%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.63729688</td><td align="right">0.099s</td><td align="right">1.1G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.097s (-1.8%)</td><td align="right">1.1G (-0.05%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739575295</td><td align="right">0.042s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">6.739575295 (=)</td><td align="right">0.042s (+0.6%)</td><td align="right">0.5G (-0.10%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.949978834</td><td align="right">2.31s</td><td align="right">23.8G</td><td align="right">506</td><td align="right">2</td><td align="right">7.949978834 (=)</td><td align="right">2.33s (+1.0%)</td><td align="right">23.8G (-0.02%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.754216663</td><td align="right">19.3s</td><td align="right">117.5G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.754216663 (=)</td><td align="right">19.1s (-0.8%)</td><td align="right">117.5G (-0.00%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-9.4%)</td><td align="right">0.1G (-6.57%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-18.5%)</td><td align="right">0.1G (-9.11%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.400445378</td><td align="right">2.95s</td><td align="right">32.3G</td><td align="right">168</td><td align="right">2</td><td align="right">7.400445378 (=)</td><td align="right">2.86s (-3.2%)</td><td align="right">32.3G (+0.00%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393055049</td><td align="right">3.76s</td><td align="right">41.9G</td><td align="right">334</td><td align="right">2</td><td align="right">5.393055049 (=)</td><td align="right">3.79s (+1.0%)</td><td align="right">41.9G (-0.00%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571539329</td><td align="right">3.93s</td><td align="right">41.2G</td><td align="right">304</td><td align="right">2</td><td align="right">5.571539329 (=)</td><td align="right">3.90s (-0.9%)</td><td align="right">41.2G (+0.01%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.424143707</td><td align="right">10.8s</td><td align="right">104.7G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.424143707 (=)</td><td align="right">10.6s (-1.1%)</td><td align="right">104.7G (+0.00%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.04s</td><td align="right">31.6G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.04s (-0.1%)</td><td align="right">31.6G (-0.05%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739271968</td><td align="right">3.80s</td><td align="right">39.8G</td><td align="right">638</td><td align="right">2</td><td align="right">6.739271968 (=)</td><td align="right">3.88s (+2.0%)</td><td align="right">39.8G (-0.02%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.822826504</td><td align="right">6.96s</td><td align="right">78.8G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822826504 (=)</td><td align="right">6.90s (-0.9%)</td><td align="right">78.8G (-0.01%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.866901786</td><td align="right">7.30s</td><td align="right">71.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">7.75s (+6.1%)</td><td align="right">71.6G (-0.00%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.866617805</td><td align="right">7.45s</td><td align="right">71.3G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">7.54s (+1.2%)</td><td align="right">71.3G (+0.01%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884862145</td><td align="right">8.19s</td><td align="right">71.1G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">7.84s (-4.2%)</td><td align="right">71.0G (-0.03%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.889244164</td><td align="right">8.09s</td><td align="right">73.4G</td><td align="right">675</td><td align="right">2</td><td align="right">6.889244164 (=)</td><td align="right">8.21s (+1.4%)</td><td align="right">73.4G (-0.01%)</td><td align="right">675</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.887234466</td><td align="right">8.72s</td><td align="right">75.4G</td><td align="right">921</td><td align="right">2</td><td align="right">6.887234466 (=)</td><td align="right">8.75s (+0.4%)</td><td align="right">75.4G (-0.00%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-2d`</td><td align="right">6.773456578</td><td align="right">3.67s</td><td align="right">38.0G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.69s (+0.7%)</td><td align="right">38.0G (-0.01%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-2d`</td><td align="right">6.258611497</td><td align="right">4.53s</td><td align="right">36.8G</td><td align="right">3236</td><td align="right">2</td><td align="right">6.258611497 (=)</td><td align="right">4.58s (+1.1%)</td><td align="right">36.8G (+0.00%)</td><td align="right">3236</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 E50000 `-2d`</td><td align="right">5.454385527</td><td align="right">4.31s</td><td align="right">33.7G</td><td align="right">4381</td><td align="right">2</td><td align="right">5.454385527 (=)</td><td align="right">4.29s (-0.5%)</td><td align="right">33.7G (-0.02%)</td><td align="right">4381</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 E50000 `-2d`</td><td align="right">4.903270512</td><td align="right">4.26s</td><td align="right">31.9G</td><td align="right">5443</td><td align="right">2</td><td align="right">4.903270512 (=)</td><td align="right">4.29s (+0.7%)</td><td align="right">31.9G (+0.03%)</td><td align="right">5443</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 E50000 `-2d`</td><td align="right">4.468778176</td><td align="right">4.44s</td><td align="right">32.4G</td><td align="right">6379</td><td align="right">2</td><td align="right">4.468778176 (=)</td><td align="right">4.52s (+2.0%)</td><td align="right">32.4G (+0.00%)</td><td align="right">6379</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 E50000 `-2d`</td><td align="right">4.143277395</td><td align="right">4.36s</td><td align="right">31.3G</td><td align="right">7097</td><td align="right">2</td><td align="right">4.143277395 (=)</td><td align="right">4.38s (+0.5%)</td><td align="right">31.3G (+0.00%)</td><td align="right">7097</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 E50000 `-2d`</td><td align="right">3.859069372</td><td align="right">4.45s</td><td align="right">31.6G</td><td align="right">7985</td><td align="right">2</td><td align="right">3.859069372 (=)</td><td align="right">4.40s (-1.2%)</td><td align="right">31.6G (+0.01%)</td><td align="right">7985</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.907904741</td><td align="right">0.697s</td><td align="right">6.9G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.679s (-2.6%)</td><td align="right">6.9G (-0.03%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

Interleaved minimum of 3 per arm. This is where the run-level rescue fires: a `-d -N1` row whose only
trial was abandoned or collapsed now returns the `-2d -N1` answer, and pays the abandoned hierarchical
attempt (pass-1 sweep and two up-builds, no refinement) plus that two-level solve and its deep repair.
Against `-2d -N1` on the same network the surcharge is 14–32% (F56 addendum).

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.000s (+6.9%)</td><td align="right">0.1G (-1.54%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.899367957</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">2</td><td align="right">6.899367957 (=)</td><td align="right">0.001s (-5.9%)</td><td align="right">0.1G (-0.18%)</td><td align="right">11</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.047459862</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">4</td><td align="right">4.047459862 (=)</td><td align="right">0.003s (-1.9%)</td><td align="right">0.1G (-0.28%)</td><td align="right">7</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.730850312</td><td align="right">0.025s</td><td align="right">0.4G</td><td align="right">12</td><td align="right">5</td><td align="right">4.730850312 (=)</td><td align="right">0.025s (+0.0%)</td><td align="right">0.4G (+0.11%)</td><td align="right">12</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.758265349</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">3</td><td align="right">3</td><td align="right">6.758265349 (=)</td><td align="right">0.009s (-1.8%)</td><td align="right">0.2G (-0.13%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">0.395s</td><td align="right">5.7G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">0.391s (-1.0%)</td><td align="right">5.7G (+0.11%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">2.43s</td><td align="right">24.3G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">2.38s (-1.9%)</td><td align="right">24.4G (+0.07%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.041117399</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.041117399 (=)</td><td align="right">0.001s (-4.0%)</td><td align="right">0.1G (-1.47%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-2.3%)</td><td align="right">0.1G (-0.83%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.491980364</td><td align="right">0.373s</td><td align="right">5.3G</td><td align="right">148</td><td align="right">3</td><td align="right">7.525940462 (+0.4533%)</td><td align="right">0.412s (+10.4%)</td><td align="right">5.7G (+7.56%)</td><td align="right">150</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.470440768</td><td align="right">0.369s</td><td align="right">4.5G</td><td align="right">242</td><td align="right">3</td><td align="right">5.393492268 (-1.4066%)</td><td align="right">0.646s (+75.2%)</td><td align="right">7.5G (+64.53%)</td><td align="right">336</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.657913279</td><td align="right">0.491s</td><td align="right">5.9G</td><td align="right">197</td><td align="right">3</td><td align="right">5.591354826 (-1.1764%)</td><td align="right">0.720s (+46.6%)</td><td align="right">8.1G (+38.69%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.546335898</td><td align="right">0.909s</td><td align="right">9.2G</td><td align="right">1614</td><td align="right">3</td><td align="right">7.546335898 (=)</td><td align="right">0.904s (-0.5%)</td><td align="right">9.2G (+0.07%)</td><td align="right">1614</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.460796773</td><td align="right">0.409s</td><td align="right">5.8G</td><td align="right">5</td><td align="right">3</td><td align="right">8.460796773 (=)</td><td align="right">0.411s (+0.4%)</td><td align="right">5.8G (-0.03%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739358212</td><td align="right">1.56s</td><td align="right">17.3G</td><td align="right">674</td><td align="right">2</td><td align="right">6.739358212 (=)</td><td align="right">1.54s (-1.6%)</td><td align="right">17.3G (-0.03%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.823513368</td><td align="right">1.30s</td><td align="right">15.0G</td><td align="right">72</td><td align="right">2</td><td align="right">6.823513368 (=)</td><td align="right">1.30s (+0.2%)</td><td align="right">15.0G (+0.01%)</td><td align="right">72</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.861724654</td><td align="right">1.65s</td><td align="right">17.5G</td><td align="right">141</td><td align="right">2</td><td align="right">6.861724654 (=)</td><td align="right">1.65s (+0.1%)</td><td align="right">17.5G (+0.00%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.868127142</td><td align="right">1.88s</td><td align="right">19.1G</td><td align="right">293</td><td align="right">2</td><td align="right">6.868127142 (=)</td><td align="right">1.86s (-1.1%)</td><td align="right">19.1G (-0.03%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884042436</td><td align="right">1.83s</td><td align="right">18.0G</td><td align="right">455</td><td align="right">2</td><td align="right">6.884042436 (=)</td><td align="right">1.84s (+1.0%)</td><td align="right">18.0G (-0.01%)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.889332374</td><td align="right">2.10s</td><td align="right">20.1G</td><td align="right">651</td><td align="right">2</td><td align="right">6.889332374 (=)</td><td align="right">2.07s (-1.3%)</td><td align="right">20.1G (-0.02%)</td><td align="right">651</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.893377041</td><td align="right">2.36s</td><td align="right">22.0G</td><td align="right">911</td><td align="right">2</td><td align="right">6.893377041 (=)</td><td align="right">2.32s (-1.8%)</td><td align="right">22.0G (-0.03%)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.29196634</td><td align="right">0.363s</td><td align="right">3.7G</td><td align="right">321</td><td align="right">4</td><td align="right">7.29196634 (=)</td><td align="right">0.376s (+3.6%)</td><td align="right">3.7G (+0.08%)</td><td align="right">321</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">7.97489927</td><td align="right">0.826s</td><td align="right">8.3G</td><td align="right">1</td><td align="right">2</td><td align="right">6.823513368 (-14.4376%)</td><td align="right">1.48s (+79.6%)</td><td align="right">16.9G (+104.64%)</td><td align="right">72</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">7.9829318</td><td align="right">0.713s</td><td align="right">7.1G</td><td align="right">1</td><td align="right">2</td><td align="right">6.861724654 (-14.0451%)</td><td align="right">1.91s (+168.5%)</td><td align="right">19.8G (+179.23%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.812252898</td><td align="right">0.763s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">6.868127142 (-12.0852%)</td><td align="right">2.10s (+175.8%)</td><td align="right">21.4G (+196.40%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.440161581</td><td align="right">0.798s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.440161581 (=)</td><td align="right">0.798s (-0.1%)</td><td align="right">7.5G (-0.00%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.192756466</td><td align="right">0.809s</td><td align="right">7.5G</td><td align="right">149</td><td align="right">4</td><td align="right">7.192756466 (=)</td><td align="right">0.815s (+0.7%)</td><td align="right">7.5G (+0.07%)</td><td align="right">149</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.967535764</td><td align="right">0.817s</td><td align="right">7.4G</td><td align="right">206</td><td align="right">4</td><td align="right">6.967535764 (=)</td><td align="right">0.817s (-0.0%)</td><td align="right">7.5G (+0.05%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">7.144517469</td><td align="right">0.419s</td><td align="right">4.5G</td><td align="right">3</td><td align="right">3</td><td align="right">6.774049681 (-5.1853%)</td><td align="right">0.776s (+85.0%)</td><td align="right">8.1G (+77.73%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.854829799</td><td align="right">0.442s</td><td align="right">4.3G</td><td align="right">537</td><td align="right">4</td><td align="right">5.854829799 (=)</td><td align="right">0.442s (+0.0%)</td><td align="right">4.4G (+0.34%)</td><td align="right">537</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.898784516</td><td align="right">0.442s</td><td align="right">4.2G</td><td align="right">1867</td><td align="right">3</td><td align="right">4.898784516 (=)</td><td align="right">0.446s (+0.9%)</td><td align="right">4.2G (+0.12%)</td><td align="right">1867</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.1554786</td><td align="right">0.557s</td><td align="right">5.0G</td><td align="right">2156</td><td align="right">4</td><td align="right">4.1554786 (=)</td><td align="right">0.547s (-1.9%)</td><td align="right">5.0G (+0.02%)</td><td align="right">2156</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.620157111</td><td align="right">0.687s</td><td align="right">5.9G</td><td align="right">3056</td><td align="right">4</td><td align="right">3.620157111 (=)</td><td align="right">0.687s (-0.1%)</td><td align="right">6.0G (+0.12%)</td><td align="right">3056</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.21639819</td><td align="right">0.637s</td><td align="right">5.6G</td><td align="right">3417</td><td align="right">5</td><td align="right">3.21639819 (=)</td><td align="right">0.638s (+0.2%)</td><td align="right">5.6G (+0.08%)</td><td align="right">3417</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.885785338</td><td align="right">0.921s</td><td align="right">8.1G</td><td align="right">4371</td><td align="right">5</td><td align="right">2.885785338 (=)</td><td align="right">0.924s (+0.3%)</td><td align="right">8.1G (+0.03%)</td><td align="right">4371</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om2 `-2d -c` planted</td><td align="right">6.744721993</td><td align="right">0.563s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.744721993 (=)</td><td align="right">0.559s (-0.9%)</td><td align="right">6.2G (-0.04%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d -c` planted</td><td align="right">6.820421208</td><td align="right">0.788s</td><td align="right">9.1G</td><td align="right">62</td><td align="right">2</td><td align="right">6.820421208 (=)</td><td align="right">0.796s (+1.0%)</td><td align="right">9.1G (+0.01%)</td><td align="right">62</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d -c` planted</td><td align="right">6.856239474</td><td align="right">0.957s</td><td align="right">10.4G</td><td align="right">132</td><td align="right">2</td><td align="right">6.856239474 (=)</td><td align="right">0.948s (-1.0%)</td><td align="right">10.3G (-0.02%)</td><td align="right">132</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d -c` planted</td><td align="right">6.857778113</td><td align="right">1.21s</td><td align="right">12.5G</td><td align="right">296</td><td align="right">2</td><td align="right">6.857778113 (=)</td><td align="right">1.18s (-2.7%)</td><td align="right">12.5G (-0.02%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d -c` planted</td><td align="right">6.873378755</td><td align="right">1.30s</td><td align="right">13.2G</td><td align="right">446</td><td align="right">2</td><td align="right">6.873378755 (=)</td><td align="right">1.30s (+0.1%)</td><td align="right">13.2G (-0.02%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d -c` planted</td><td align="right">6.878354613</td><td align="right">1.96s</td><td align="right">19.8G</td><td align="right">650</td><td align="right">2</td><td align="right">6.878354613 (=)</td><td align="right">1.96s (+0.4%)</td><td align="right">19.8G (-0.01%)</td><td align="right">650</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d -c` planted</td><td align="right">6.875237392</td><td align="right">1.46s</td><td align="right">14.2G</td><td align="right">894</td><td align="right">2</td><td align="right">6.875237392 (=)</td><td align="right">1.44s (-1.4%)</td><td align="right">14.2G (+0.01%)</td><td align="right">894</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">6.066305904</td><td align="right">0.070s</td><td align="right">0.8G</td><td align="right">187</td><td align="right">3</td><td align="right">6.066305904 (=)</td><td align="right">0.071s (+1.2%)</td><td align="right">0.8G (+0.26%)</td><td align="right">187</td><td align="right">3</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.092s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.091s (-1.8%)</td><td align="right">1.0G (-0.05%)</td><td align="right">184</td><td align="right">2</td></tr>
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
<tr><td align="right">om2 E100000 `-2d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">3.71s</td><td align="right">37.8G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.65s (-1.6%)</td><td align="right">37.8G (-0.00%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 E100000 `-d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">6.11s</td><td align="right">64.1G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.68s (-39.8%)</td><td align="right">34.5G (-46.14%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.60s</td><td align="right">25.1G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.69s (+3.5%)</td><td align="right">25.1G (+0.02%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N1`</td><td align="right">7.548816177</td><td align="right">0.810s</td><td align="right">8.2G</td><td align="right">120</td><td align="right">2</td><td align="right">7.548816177 (=)</td><td align="right">0.815s (+0.7%)</td><td align="right">8.2G (+0.01%)</td><td align="right">120</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">4.31s</td><td align="right">36.4G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.59s (-40.0%)</td><td align="right">24.1G (-33.77%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N1`</td><td align="right">7.970508085</td><td align="right">0.411s</td><td align="right">4.3G</td><td align="right">1</td><td align="right">2</td><td align="right">7.548816177 (-5.2907%)</td><td align="right">0.928s (+125.7%)</td><td align="right">9.3G (+116.84%)</td><td align="right">120</td><td align="right">2</td></tr>
<tr><td align="right">om2 planted, `-2d --no-infomap -c`</td><td align="right">6.789039995</td><td align="right">0.052s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.789039995 (=)</td><td align="right">0.053s (+0.5%)</td><td align="right">0.6G (-0.04%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N10`</td><td align="right">7.931196935</td><td align="right">3.24s</td><td align="right">27.9G</td><td align="right">49</td><td align="right">2</td><td align="right">7.931196935 (=)</td><td align="right">3.16s (-2.6%)</td><td align="right">27.9G (-0.02%)</td><td align="right">49</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N10`</td><td align="right">7.931196935</td><td align="right">3.84s</td><td align="right">36.4G</td><td align="right">49</td><td align="right">2</td><td align="right">7.931196935 (=)</td><td align="right">3.08s (-19.7%)</td><td align="right">28.1G (-22.90%)</td><td align="right">49</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">5.28s</td><td align="right">48.0G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">4.95s (-6.4%)</td><td align="right">48.0G (-0.07%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">0.952s</td><td align="right">9.7G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">0.957s (+0.5%)</td><td align="right">9.7G (-0.02%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">6.35s</td><td align="right">63.3G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">4.63s (-27.0%)</td><td align="right">45.0G (-28.97%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N1`</td><td align="right">7.97414175</td><td align="right">0.561s</td><td align="right">5.5G</td><td align="right">1</td><td align="right">2</td><td align="right">7.261268486 (-8.9398%)</td><td align="right">1.17s (+108.2%)</td><td align="right">11.7G (+113.61%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 planted, `-2d --no-infomap -c`</td><td align="right">6.837980937</td><td align="right">0.097s</td><td align="right">0.9G</td><td align="right">12</td><td align="right">2</td><td align="right">6.837980937 (=)</td><td align="right">0.099s (+1.8%)</td><td align="right">0.9G (-0.13%)</td><td align="right">12</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N10`</td><td align="right">7.940858042</td><td align="right">3.16s</td><td align="right">27.4G</td><td align="right">41</td><td align="right">2</td><td align="right">7.940858042 (=)</td><td align="right">3.17s (+0.4%)</td><td align="right">27.4G (+0.02%)</td><td align="right">41</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N10`</td><td align="right">7.940858042</td><td align="right">4.98s</td><td align="right">45.3G</td><td align="right">41</td><td align="right">2</td><td align="right">7.940858042 (=)</td><td align="right">3.23s (-35.2%)</td><td align="right">27.2G (-39.83%)</td><td align="right">41</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N10`</td><td align="right">7.556894677</td><td align="right">5.40s</td><td align="right">49.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">5.51s (+2.1%)</td><td align="right">49.1G (+0.02%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">0.989s</td><td align="right">9.3G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">0.983s (-0.6%)</td><td align="right">9.3G (-0.03%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N10`</td><td align="right">7.556653713</td><td align="right">7.54s</td><td align="right">67.6G</td><td align="right">78</td><td align="right">2</td><td align="right">7.556653713 (=)</td><td align="right">5.24s (-30.6%)</td><td align="right">48.7G (-27.95%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N1`</td><td align="right">7.9828492</td><td align="right">0.779s</td><td align="right">7.3G</td><td align="right">1</td><td align="right">2</td><td align="right">7.556894677 (-5.3359%)</td><td align="right">1.25s (+60.6%)</td><td align="right">11.4G (+55.76%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 planted, `-2d --no-infomap -c`</td><td align="right">6.880650147</td><td align="right">0.116s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.880650147 (=)</td><td align="right">0.110s (-5.4%)</td><td align="right">1.0G (-0.09%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N10`</td><td align="right">7.969030601</td><td align="right">3.46s</td><td align="right">29.0G</td><td align="right">26</td><td align="right">2</td><td align="right">7.969030601 (=)</td><td align="right">3.47s (+0.2%)</td><td align="right">29.0G (+0.01%)</td><td align="right">26</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N10`</td><td align="right">7.969030601</td><td align="right">5.23s</td><td align="right">45.2G</td><td align="right">26</td><td align="right">2</td><td align="right">7.969030601 (=)</td><td align="right">3.68s (-29.7%)</td><td align="right">28.3G (-37.51%)</td><td align="right">26</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N10`</td><td align="right">7.967531078</td><td align="right">6.36s</td><td align="right">55.1G</td><td align="right">98</td><td align="right">2</td><td align="right">7.967531078 (=)</td><td align="right">6.47s (+1.7%)</td><td align="right">55.1G (+0.01%)</td><td align="right">98</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N1`</td><td align="right">7.966995214</td><td align="right">3.00s</td><td align="right">26.6G</td><td align="right">104</td><td align="right">2</td><td align="right">7.966995214 (=)</td><td align="right">2.97s (-1.1%)</td><td align="right">26.6G (-0.00%)</td><td align="right">104</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N10`</td><td align="right">7.965009721</td><td align="right">9.92s</td><td align="right">87.7G</td><td align="right">106</td><td align="right">2</td><td align="right">7.965009721 (=)</td><td align="right">7.76s (-21.7%)</td><td align="right">69.4G (-20.92%)</td><td align="right">106</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N1`</td><td align="right">7.989613065</td><td align="right">0.728s</td><td align="right">6.7G</td><td align="right">1</td><td align="right">2</td><td align="right">7.966995214 (-0.2831%)</td><td align="right">3.24s (+345.5%)</td><td align="right">28.7G (+328.88%)</td><td align="right">104</td><td align="right">2</td></tr>
<tr><td align="right">om5 planted, `-2d --no-infomap -c`</td><td align="right">6.902222527</td><td align="right">0.128s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.902222527 (=)</td><td align="right">0.126s (-1.7%)</td><td align="right">1.1G (-0.08%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N10`</td><td align="right">7.959010571</td><td align="right">3.42s</td><td align="right">27.7G</td><td align="right">26</td><td align="right">2</td><td align="right">7.959010571 (=)</td><td align="right">3.42s (-0.0%)</td><td align="right">27.7G (-0.00%)</td><td align="right">26</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N10`</td><td align="right">7.959010571</td><td align="right">3.53s</td><td align="right">26.8G</td><td align="right">26</td><td align="right">2</td><td align="right">7.959010571 (=)</td><td align="right">3.50s (-0.9%)</td><td align="right">27.4G (+2.33%)</td><td align="right">26</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N10`</td><td align="right">7.981574549</td><td align="right">6.10s</td><td align="right">51.0G</td><td align="right">113</td><td align="right">2</td><td align="right">7.981574549 (=)</td><td align="right">6.16s (+0.9%)</td><td align="right">51.0G (+0.02%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N1`</td><td align="right">7.981574549</td><td align="right">1.87s</td><td align="right">15.8G</td><td align="right">113</td><td align="right">2</td><td align="right">7.981574549 (=)</td><td align="right">1.87s (+0.3%)</td><td align="right">15.8G (+0.04%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N10`</td><td align="right">7.981063575</td><td align="right">9.80s</td><td align="right">86.4G</td><td align="right">119</td><td align="right">2</td><td align="right">7.981063575 (=)</td><td align="right">7.89s (-19.5%)</td><td align="right">67.3G (-22.13%)</td><td align="right">119</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N1`</td><td align="right">7.993490371</td><td align="right">0.840s</td><td align="right">7.8G</td><td align="right">1</td><td align="right">2</td><td align="right">7.981574549 (-0.1491%)</td><td align="right">2.14s (+155.0%)</td><td align="right">17.9G (+131.02%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om6 planted, `-2d --no-infomap -c`</td><td align="right">6.930934993</td><td align="right">0.138s</td><td align="right">1.1G</td><td align="right">24</td><td align="right">2</td><td align="right">6.930934993 (=)</td><td align="right">0.132s (-4.1%)</td><td align="right">1.1G (-0.22%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N10`</td><td align="right">7.974818773</td><td align="right">3.67s</td><td align="right">29.9G</td><td align="right">23</td><td align="right">2</td><td align="right">7.974818773 (=)</td><td align="right">3.68s (+0.4%)</td><td align="right">29.9G (+0.01%)</td><td align="right">23</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N10`</td><td align="right">7.974818773</td><td align="right">3.73s</td><td align="right">28.1G</td><td align="right">23</td><td align="right">2</td><td align="right">7.974818773 (=)</td><td align="right">3.65s (-2.1%)</td><td align="right">28.9G (+2.67%)</td><td align="right">23</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N10`</td><td align="right">7.945712536</td><td align="right">7.55s</td><td align="right">60.5G</td><td align="right">184</td><td align="right">2</td><td align="right">7.945712536 (=)</td><td align="right">7.45s (-1.4%)</td><td align="right">60.5G (-0.01%)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N1`</td><td align="right">7.945571863</td><td align="right">3.50s</td><td align="right">29.8G</td><td align="right">187</td><td align="right">2</td><td align="right">7.945571863 (=)</td><td align="right">3.53s (+1.0%)</td><td align="right">29.8G (-0.01%)</td><td align="right">187</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N10`</td><td align="right">7.945712536</td><td align="right">9.54s</td><td align="right">84.4G</td><td align="right">184</td><td align="right">2</td><td align="right">7.945712536 (=)</td><td align="right">7.29s (-23.6%)</td><td align="right">60.4G (-28.43%)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N1`</td><td align="right">7.992395554</td><td align="right">0.801s</td><td align="right">7.3G</td><td align="right">1</td><td align="right">2</td><td align="right">7.945571863 (-0.5859%)</td><td align="right">3.75s (+368.6%)</td><td align="right">31.9G (+337.62%)</td><td align="right">187</td><td align="right">2</td></tr>
<tr><td align="right">om7 planted, `-2d --no-infomap -c`</td><td align="right">6.957516072</td><td align="right">0.155s</td><td align="right">1.3G</td><td align="right">28</td><td align="right">2</td><td align="right">6.957516072 (=)</td><td align="right">0.154s (-0.6%)</td><td align="right">1.3G (-0.17%)</td><td align="right">28</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N10`</td><td align="right">7.990101633</td><td align="right">3.43s</td><td align="right">27.0G</td><td align="right">8</td><td align="right">2</td><td align="right">7.990101633 (=)</td><td align="right">3.39s (-1.0%)</td><td align="right">27.0G (+0.01%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N10`</td><td align="right">7.990101633</td><td align="right">3.48s</td><td align="right">25.3G</td><td align="right">8</td><td align="right">2</td><td align="right">7.990101633 (=)</td><td align="right">3.42s (-1.8%)</td><td align="right">26.1G (+3.25%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N10`</td><td align="right">7.976140205</td><td align="right">8.60s</td><td align="right">69.9G</td><td align="right">240</td><td align="right">2</td><td align="right">7.976140205 (=)</td><td align="right">8.66s (+0.7%)</td><td align="right">69.9G (-0.00%)</td><td align="right">240</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N1`</td><td align="right">7.978912396</td><td align="right">2.75s</td><td align="right">22.5G</td><td align="right">234</td><td align="right">2</td><td align="right">7.978912396 (=)</td><td align="right">2.71s (-1.7%)</td><td align="right">22.5G (-0.01%)</td><td align="right">234</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N10`</td><td align="right">7.976681139</td><td align="right">9.36s</td><td align="right">80.9G</td><td align="right">256</td><td align="right">2</td><td align="right">7.976681139 (=)</td><td align="right">6.93s (-26.0%)</td><td align="right">55.3G (-31.66%)</td><td align="right">256</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N1`</td><td align="right">7.994735672</td><td align="right">1.00s</td><td align="right">9.4G</td><td align="right">1</td><td align="right">2</td><td align="right">7.978912396 (-0.1979%)</td><td align="right">3.00s (+199.2%)</td><td align="right">24.6G (+161.71%)</td><td align="right">234</td><td align="right">2</td></tr>
<tr><td align="right">om8 planted, `-2d --no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.145s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.144s (-1.2%)</td><td align="right">1.2G (-0.01%)</td><td align="right">32</td><td align="right">2</td></tr>
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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">3.85s</td><td align="right">40.2G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">7.48s (+94.1%)</td><td align="right">40.6G (+1.07%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822826504</td><td align="right">7.35s</td><td align="right">78.3G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822826504 (=)</td><td align="right">5.25s (-28.5%)</td><td align="right">57.0G (-27.18%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.88s</td><td align="right">66.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.97s (-13.3%)</td><td align="right">56.9G (-14.64%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">7.53s</td><td align="right">70.9G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">6.07s (-19.4%)</td><td align="right">57.1G (-19.48%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">7.87s</td><td align="right">68.6G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">7.69s (-2.3%)</td><td align="right">68.6G (+0.14%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.888473273</td><td align="right">8.36s</td><td align="right">73.4G</td><td align="right">680</td><td align="right">2</td><td align="right">6.888473273 (=)</td><td align="right">8.26s (-1.1%)</td><td align="right">73.5G (+0.13%)</td><td align="right">680</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.981469446</td><td align="right">7.61s</td><td align="right">65.9G</td><td align="right">227</td><td align="right">4</td><td align="right">6.887234466 (-1.3498%)</td><td align="right">8.67s (+13.9%)</td><td align="right">75.6G (+14.76%)</td><td align="right">921</td><td align="right">2</td></tr>
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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.321678354</td><td align="right">0.270s</td><td align="right">2.6G</td><td align="right">436</td><td align="right">4</td><td align="right">7.321678354 (=)</td><td align="right">0.288s (+6.7%)</td><td align="right">2.6G (+0.17%)</td><td align="right">436</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">7.97489927</td><td align="right">0.793s</td><td align="right">7.8G</td><td align="right">1</td><td align="right">2</td><td align="right">6.823513368 (-14.4376%)</td><td align="right">1.47s (+85.7%)</td><td align="right">16.4G (+110.74%)</td><td align="right">72</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">7.9829318</td><td align="right">0.512s</td><td align="right">4.8G</td><td align="right">1</td><td align="right">2</td><td align="right">6.861724654 (-14.0451%)</td><td align="right">1.85s (+260.8%)</td><td align="right">19.4G (+302.76%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.798283664</td><td align="right">0.658s</td><td align="right">5.7G</td><td align="right">96</td><td align="right">4</td><td align="right">6.868127142 (-11.9277%)</td><td align="right">2.07s (+215.2%)</td><td align="right">20.9G (+263.85%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.507066407</td><td align="right">0.690s</td><td align="right">5.8G</td><td align="right">87</td><td align="right">4</td><td align="right">7.507066407 (=)</td><td align="right">0.681s (-1.3%)</td><td align="right">5.8G (+0.04%)</td><td align="right">87</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.238675817</td><td align="right">0.606s</td><td align="right">5.2G</td><td align="right">145</td><td align="right">4</td><td align="right">7.238675817 (=)</td><td align="right">0.613s (+1.2%)</td><td align="right">5.2G (+0.21%)</td><td align="right">145</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">7.022972054</td><td align="right">0.613s</td><td align="right">5.2G</td><td align="right">201</td><td align="right">4</td><td align="right">7.022972054 (=)</td><td align="right">0.603s (-1.5%)</td><td align="right">5.2G (+0.17%)</td><td align="right">201</td><td align="right">4</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-17.7%)</td><td align="right">0.1G (-2.04%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (-3.3%)</td><td align="right">0.1G (-1.99%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.03324474 (-0.3856%)</td><td align="right">0.014s (-37.9%)</td><td align="right">0.2G (-29.19%)</td><td align="right">9</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.238s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.754013143 (+0.7684%)</td><td align="right">0.146s (-38.9%)</td><td align="right">1.7G (-37.68%)</td><td align="right">10</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.058s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.056s (-3.8%)</td><td align="right">0.7G (-3.98%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.02s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">2.91s (-3.6%)</td><td align="right">33.1G (-2.97%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">19.8s</td><td align="right">181.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.620539396 (+1.1539%)</td><td align="right">14.8s (-25.0%)</td><td align="right">126.7G (-30.28%)</td><td align="right">135</td><td align="right">5</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-3.1%)</td><td align="right">0.1G (-1.45%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+9.1%)</td><td align="right">0.1G (-0.44%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.64s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.62s (-0.5%)</td><td align="right">30.6G (-2.64%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.57s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.24s (-9.4%)</td><td align="right">35.9G (-6.98%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.79s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.38s (-10.8%)</td><td align="right">35.2G (-12.69%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.85s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">9.54s (-3.2%)</td><td align="right">95.4G (-2.59%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.28s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.18s (-2.9%)</td><td align="right">34.7G (-2.21%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.62s</td><td align="right">47.5G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">7.48s (+61.8%)</td><td align="right">40.6G (-14.46%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822826504</td><td align="right">5.70s</td><td align="right">61.4G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822826504 (=)</td><td align="right">5.25s (-7.9%)</td><td align="right">57.0G (-7.12%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.28s</td><td align="right">61.5G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.97s (-5.1%)</td><td align="right">56.9G (-7.51%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">6.52s</td><td align="right">61.8G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">6.07s (-7.0%)</td><td align="right">57.1G (-7.63%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">9.02s</td><td align="right">83.2G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">7.69s (-14.7%)</td><td align="right">68.6G (-17.53%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.888473273</td><td align="right">9.44s</td><td align="right">86.5G</td><td align="right">680</td><td align="right">2</td><td align="right">6.888473273 (=)</td><td align="right">8.26s (-12.4%)</td><td align="right">73.5G (-15.01%)</td><td align="right">680</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.887234466</td><td align="right">9.80s</td><td align="right">87.3G</td><td align="right">921</td><td align="right">2</td><td align="right">6.887234466 (=)</td><td align="right">8.67s (-11.5%)</td><td align="right">75.6G (-13.37%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.728s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.692s (-5.1%)</td><td align="right">6.9G (-4.02%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.078067323 (-8.7135%)</td><td align="right">0.001s (+1.4%)</td><td align="right">0.1G (+0.83%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367 (+0.0797%)</td><td align="right">0.007s (+0.2%)</td><td align="right">0.1G (+0.21%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">3.892209764 (-3.8689%)</td><td align="right">0.023s (+0.6%)</td><td align="right">0.3G (+0.95%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.238s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.509265423 (-4.4194%)</td><td align="right">0.246s (+3.2%)</td><td align="right">2.9G (+3.27%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.058s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502 (+0.7165%)</td><td align="right">0.060s (+2.4%)</td><td align="right">0.7G (+2.15%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.02s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">8.009172258 (+2.5773%)</td><td align="right">2.77s (-8.2%)</td><td align="right">31.2G (-8.69%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">19.8s</td><td align="right">181.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.512433077 (-0.7917%)</td><td align="right">20.3s (+2.8%)</td><td align="right">182.9G (+0.64%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653 (-0.8182%)</td><td align="right">0.004s (-7.4%)</td><td align="right">0.1G (-0.60%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (-4.1040%)</td><td align="right">0.001s (-7.0%)</td><td align="right">0.1G (+0.04%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.64s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.438064454 (+0.6171%)</td><td align="right">2.83s (+7.2%)</td><td align="right">33.9G (+7.74%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.57s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.378824196 (-0.2496%)</td><td align="right">3.58s (+0.2%)</td><td align="right">38.4G (-0.41%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.79s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.56659036 (-0.1426%)</td><td align="right">3.79s (-0.0%)</td><td align="right">40.1G (-0.59%)</td><td align="right">220</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.85s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.192724425 (-3.0848%)</td><td align="right">8.65s (-12.2%)</td><td align="right">85.9G (-12.23%)</td><td align="right">1910</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.28s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451 (+2.5761%)</td><td align="right">3.23s (-1.4%)</td><td align="right">35.3G (-0.46%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.728s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.903208727 (-0.0795%)</td><td align="right">0.677s (-7.0%)</td><td align="right">6.9G (-4.40%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-77.9%)</td><td align="right">0.1G (-30.23%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">5</td><td align="right">2</td><td align="right">6.862755928 (-0.0042%)</td><td align="right">0.007s (-69.7%)</td><td align="right">0.1G (-54.99%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.026557116</td><td align="right">0.119s</td><td align="right">1.3G</td><td align="right">12</td><td align="right">5</td><td align="right">4.048857953 (+0.5538%)</td><td align="right">0.022s (-81.1%)</td><td align="right">0.3G (-76.48%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.736412597</td><td align="right">1.89s</td><td align="right">20.2G</td><td align="right">11</td><td align="right">6</td><td align="right">4.717760238 (-0.3938%)</td><td align="right">0.238s (-87.4%)</td><td align="right">2.8G (-86.11%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.738927979</td><td align="right">0.136s</td><td align="right">1.4G</td><td align="right">80</td><td align="right">3</td><td align="right">6.740943136 (+0.0299%)</td><td align="right">0.058s (-57.2%)</td><td align="right">0.7G (-49.86%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.805465772</td><td align="right">7.94s</td><td align="right">63.0G</td><td align="right">220</td><td align="right">4</td><td align="right">7.807937174 (+0.0317%)</td><td align="right">3.02s (-62.0%)</td><td align="right">34.1G (-45.82%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.55442136</td><td align="right">155.7s</td><td align="right">1115.4G</td><td align="right">766</td><td align="right">9</td><td align="right">5.556421705 (+0.0360%)</td><td align="right">19.8s (-87.3%)</td><td align="right">181.8G (-83.70%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-67.4%)</td><td align="right">0.1G (-49.78%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-44.3%)</td><td align="right">0.1G (-37.30%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502028585</td><td align="right">9.20s</td><td align="right">78.1G</td><td align="right">144</td><td align="right">3</td><td align="right">7.392442593 (-1.4608%)</td><td align="right">2.64s (-71.3%)</td><td align="right">31.5G (-59.72%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392441014</td><td align="right">11.9s</td><td align="right">120.6G</td><td align="right">251</td><td align="right">3</td><td align="right">5.392285003 (-0.0029%)</td><td align="right">3.57s (-70.0%)</td><td align="right">38.6G (-68.02%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578435633</td><td align="right">7.76s</td><td align="right">81.7G</td><td align="right">301</td><td align="right">3</td><td align="right">5.574537176 (-0.0699%)</td><td align="right">3.79s (-51.2%)</td><td align="right">40.4G (-50.60%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">8.432467909</td><td align="right">5.55s</td><td align="right">52.1G</td><td align="right">114</td><td align="right">4</td><td align="right">7.421664324 (-11.9870%)</td><td align="right">9.85s (+77.5%)</td><td align="right">97.9G (+87.93%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">7.938575228</td><td align="right">6.93s</td><td align="right">57.0G</td><td align="right">25</td><td align="right">4</td><td align="right">8.235585529 (+3.7414%)</td><td align="right">3.28s (-52.7%)</td><td align="right">35.5G (-37.71%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.88554258</td><td align="right">2.23s</td><td align="right">22.4G</td><td align="right">184</td><td align="right">3</td><td align="right">5.907904741 (+0.3800%)</td><td align="right">0.728s (-67.3%)</td><td align="right">7.2G (-67.80%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-52.0%)</td><td align="right">0.1G (-37.29%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">5</td><td align="right">2</td><td align="right">6.861229775 (-0.0265%)</td><td align="right">0.007s (-44.3%)</td><td align="right">0.1G (-33.14%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.285012668</td><td align="right">0.031s</td><td align="right">0.4G</td><td align="right">56</td><td align="right">2</td><td align="right">4.283072584 (-0.0453%)</td><td align="right">0.009s (-71.8%)</td><td align="right">0.2G (-60.67%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.600443859</td><td align="right">0.698s</td><td align="right">5.9G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.097s (-86.1%)</td><td align="right">1.1G (-80.55%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739721413</td><td align="right">0.168s</td><td align="right">0.8G</td><td align="right">80</td><td align="right">2</td><td align="right">6.739575295 (-0.0022%)</td><td align="right">0.042s (-74.7%)</td><td align="right">0.5G (-35.34%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.9500396</td><td align="right">4.41s</td><td align="right">29.3G</td><td align="right">496</td><td align="right">2</td><td align="right">7.949978834 (-0.0008%)</td><td align="right">2.33s (-47.1%)</td><td align="right">23.8G (-18.63%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.742988533</td><td align="right">45.2s</td><td align="right">251.8G</td><td align="right">11809</td><td align="right">2</td><td align="right">6.754216663 (+0.1665%)</td><td align="right">19.1s (-57.7%)</td><td align="right">117.5G (-53.34%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-48.6%)</td><td align="right">0.1G (-3.18%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-43.5%)</td><td align="right">0.1G (-38.85%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.50595639</td><td align="right">6.58s</td><td align="right">55.2G</td><td align="right">142</td><td align="right">2</td><td align="right">7.400445378 (-1.4057%)</td><td align="right">2.86s (-56.6%)</td><td align="right">32.3G (-41.51%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393312779</td><td align="right">4.70s</td><td align="right">43.9G</td><td align="right">332</td><td align="right">2</td><td align="right">5.393055049 (-0.0048%)</td><td align="right">3.79s (-19.3%)</td><td align="right">41.9G (-4.59%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.579216889</td><td align="right">5.84s</td><td align="right">58.6G</td><td align="right">301</td><td align="right">2</td><td align="right">5.571539329 (-0.1376%)</td><td align="right">3.90s (-33.2%)</td><td align="right">41.2G (-29.64%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.131110023</td><td align="right">5.04s</td><td align="right">36.7G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (+1.2849%)</td><td align="right">3.04s (-39.7%)</td><td align="right">31.6G (-14.02%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.892212121</td><td align="right">1.70s</td><td align="right">17.2G</td><td align="right">184</td><td align="right">2</td><td align="right">5.907904741 (+0.2663%)</td><td align="right">0.679s (-60.0%)</td><td align="right">6.9G (-59.79%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>
