## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`); `time` is `--timing-json`'s `timing.total_s`. One run per `-N10` row (deterministic; `instr` carries the comparison); interleaved minimum of 3 for `-N1` rows. Driver and every row: [`columnar_wip/bench-dissolve.py`](columnar_wip/bench-dissolve.py), [`columnar_wip/dissolve-ab-results.tsv`](columnar_wip/dissolve-ab-results.tsv).

> **This PR closes the ways `-C -d` lost to `-C -2d` on the overlapping family (#1041), and takes out
> the work that made the rescued runs expensive.** Three changes in the hierarchical searches
> (`optimizeColumnar`, `optimizeFlexible`) and their fallback, silent on every healthy row at `-N10`,
> plus a fourth in the deep repair they hand their answer to. **(1)** A flat-first trial completes its flat pipeline when the
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
> time. **(4)** The rescued `-N1` rows paid for work the search had already done (om5 `-d --regularized
> -N1` 0.67 → 3.41 s for −0.26% in bits on the first version of this PR): the deep repair re-clustered
> a module of 99% of the states from singletons six times at ~0.4 s each, for 0.0003–0.015% per round,
> and the lone trial refined a doomed build through the module coarsen although its interior sweeps had
> left it 14% above one-level. Now the repair does not re-derive a module of ≥ 95% of the states when
> the search converged to one (the one-level fallback is still repaired in full), repeats a fresh
> derivation only while the last one bought ≥ 0.01% of the codelength per network re-clustered, and the
> lone trial abandons a doomed build its interior sweeps (`-F`: its bottom re-partition) left above
> one-level. That run is now 0.67 → 1.10 s. Traces, rejected variants, the `-N1` phase breakdown and the
> #1042 diagnosis are F56 in `columnar-rethink-notes.md`; the cost cuts, their attribution and the
> rejected structural re-draw guard are F57.

> **Old** = a fresh `MODE=release OPENMP=0` build of `columnar-hierarchical-core` tip `a02dc105`, md5
> `b0b878cca7afa6976779d02cb3851fb1` (byte-identical to the #1078 snapshot's binary; the tip since then is
> docs-only); **new** = this PR at `d336c7a9`, md5 `10d2ec79dcbc307890d4b4e550171c50`. One session, arms
> interleaved per row, `-N1` rows as the minimum of 3 reps spread across the batch, `-N10` rows once.
> **The object-oriented arms are not re-run** — this PR does not touch that engine (project instructions) — their
> cells in the two OO tables are carried from the #1079-day session on the same machine, at the
> precision printed there; the columnar cells next to them are this session's.

### What the change moves

Every configuration where old and new differ in bits, both arms. Every move is on the columnar arm.

| network | table | old bits | new bits | Δbits | old instr | new instr | Δinstr | old time | new time | Δtime |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| overlapping om2 `-2d` | `-C -N1` | 6.739358212 | **6.739607071** | **+0.0037%** | 17.3G | 14.0G | -18.69% | 1.49s | 1.23s | -16.9% |
| overlapping om3 `-2d` | `-C -N1` | 6.823513368 | **6.823562921** | **+0.0007%** | 15.0G | 12.9G | -13.62% | 1.21s | 1.05s | -13.9% |
| overlapping om3 `-d` | `-C -N1` | 7.97489927 | **6.823562921** | **-14.4370%** | 8.3G | 17.5G | +112.17% | 0.764s | 1.44s | +87.8% |
| overlapping om3 `-2d -c` planted | `-C -N1` | 6.820421208 | **6.820929855** | **+0.0075%** | 9.1G | 4.3G | -53.16% | 0.771s | 0.369s | -52.2% |
| overlapping om4 `-2d` | `-C -N1` | 6.861724654 | **6.861732911** | **+0.0001%** | 17.5G | 15.7G | -10.13% | 1.52s | 1.35s | -11.4% |
| overlapping om4 `-d` | `-C -N1` | 7.9829318 | **6.861732911** | **-14.0450%** | 7.1G | 21.2G | +199.03% | 0.636s | 1.85s | +191.2% |
| overlapping om4 `-2d -c` planted | `-C -N1` | 6.856239474 | **6.856285862** | **+0.0007%** | 10.3G | 7.3G | -29.00% | 0.886s | 0.644s | -27.3% |
| overlapping om5 `-2d` | `-C -N1` | 6.868127142 | **6.868180827** | **+0.0008%** | 19.1G | 17.2G | -9.88% | 1.73s | 1.57s | -9.2% |
| overlapping om5 `-2d -c` planted | `-C -N1` | 6.857778113 | **6.857876353** | **+0.0014%** | 12.5G | 9.5G | -24.54% | 1.15s | 0.852s | -25.7% |
| overlapping om6 `-2d` | `-C -N1` | 6.884042436 | **6.884236095** | **+0.0028%** | 18.0G | 16.3G | -9.10% | 1.68s | 1.56s | -7.4% |
| overlapping om6 `-2d -c` planted | `-C -N1` | 6.873378755 | **6.873464257** | **+0.0012%** | 13.2G | 11.2G | -14.77% | 1.21s | 1.03s | -15.4% |
| overlapping om7 `-2d` | `-C -N1` | 6.889332374 | **6.889674586** | **+0.0050%** | 20.1G | 17.2G | -14.40% | 1.93s | 1.66s | -14.2% |
| overlapping om7 `-2d -c` planted | `-C -N1` | 6.878354613 | **6.881554086** | **+0.0465%** | 19.8G | 9.6G | -51.57% | 1.84s | 0.908s | -50.6% |
| overlapping om8 `-2d` | `-C -N1` | 6.893377041 | **6.894258582** | **+0.0128%** | 22.0G | 17.9G | -18.61% | 2.17s | 1.76s | -18.8% |
| overlapping om8 `-2d -c` planted | `-C -N1` | 6.875237392 | **6.875540042** | **+0.0044%** | 14.2G | 11.8G | -16.77% | 1.38s | 1.13s | -18.0% |
| overlapping om7 `-d` | `-C` | 6.888473273 | **6.88945591** | **+0.0143%** | 86.3G | 82.6G | -4.33% | 8.59s | 8.26s | -3.8% |
| overlapping om3 `-d` | `-C` | 6.822826504 | **6.822832994** | **+0.0001%** | 88.4G | 59.8G | -32.34% | 7.57s | 5.04s | -33.5% |
| overlapping om6 `-d` | `-C` | 6.884862145 | **6.88496897** | **+0.0016%** | 83.1G | 81.1G | -2.35% | 8.10s | 7.92s | -2.2% |
| overlapping om5 `-d` | `-C` | 6.866617805 | **6.867301407** | **+0.0100%** | 81.4G | 58.9G | -27.63% | 7.73s | 5.69s | -26.4% |
| overlapping om8 `-d` | `-C` | 6.959413622 | **6.88742315** | **-1.0344%** | 85.7G | 86.4G | +0.76% | 8.52s | 8.71s | +2.2% |
| overlapping om2 `-2d` | `-C -2` | 6.739271968 | **6.740761645** | **+0.0221%** | 39.8G | 35.6G | -10.53% | 3.69s | 3.33s | -9.7% |
| overlapping om5 `-2d` | `-C -2` | 6.866617805 | **6.867301407** | **+0.0100%** | 71.2G | 68.4G | -4.05% | 6.74s | 6.48s | -3.9% |
| overlapping om8 `-2d` | `-C -2` | 6.887234466 | **6.88742315** | **+0.0027%** | 75.4G | 74.4G | -1.25% | 8.93s | 7.73s | -13.4% |
| overlapping om7 `-2d` | `-C -2` | 6.889244164 | **6.88992089** | **+0.0098%** | 73.3G | 69.1G | -5.74% | 7.46s | 7.00s | -6.2% |
| overlapping om3 `-2d` | `-C -2` | 6.822826504 | **6.822832994** | **+0.0001%** | 78.8G | 77.3G | -1.96% | 6.40s | 6.29s | -1.7% |
| overlapping om6 `-2d` | `-C -2` | 6.884862145 | **6.88496897** | **+0.0016%** | 71.0G | 69.0G | -2.89% | 7.02s | 6.83s | -2.7% |
| overlapping om3 `-d` | `-C -F -N1` | 7.97489927 | **6.823562921** | **-14.4370%** | 7.8G | 17.1G | +118.67% | 0.730s | 1.44s | +96.6% |
| overlapping om4 `-d` | `-C -F -N1` | 7.9829318 | **6.861732911** | **-14.0450%** | 4.8G | 18.9G | +293.96% | 0.448s | 1.65s | +268.6% |
| overlapping om5 `-d` | `-C -F` | 6.866617805 | **6.867301407** | **+0.0100%** | 70.9G | 54.2G | -23.54% | 6.87s | 5.22s | -24.0% |
| overlapping om8 `-d` | `-C -F` | 6.981469446 | **6.88742315** | **-1.3471%** | 65.9G | 74.7G | +13.40% | 6.93s | 7.82s | +12.8% |
| overlapping om7 `-d` | `-C -F` | 6.888473273 | **6.88945591** | **+0.0143%** | 73.4G | 69.6G | -5.08% | 7.51s | 7.12s | -5.1% |
| overlapping om3 `-d` | `-C -F` | 6.822826504 | **6.822832994** | **+0.0001%** | 78.3G | 55.4G | -29.15% | 6.78s | 4.65s | -31.5% |
| overlapping om6 `-d` | `-C -F` | 6.884862145 | **6.88496897** | **+0.0016%** | 68.5G | 66.6G | -2.88% | 6.86s | 6.67s | -2.7% |
| om2 `-2d --regularized -N1` | family | 7.548816177 | **7.548863183** | **+0.0006%** | 8.2G | 7.2G | -11.96% | 0.760s | 0.680s | -10.5% |
| om2 `-d --regularized -N1` | family | 7.970508085 | **7.548863183** | **-5.2901%** | 4.3G | 8.8G | +106.93% | 0.378s | 0.824s | +118.2% |
| om3 `-d --regularized -N1` | family | 7.97414175 | **7.261268486** | **-8.9398%** | 5.5G | 12.5G | +127.95% | 0.511s | 1.13s | +121.9% |
| om4 `-d --regularized -N1` | family | 7.9828492 | **7.556894677** | **-5.3359%** | 7.3G | 12.3G | +67.36% | 0.704s | 1.20s | +70.1% |
| om5 `-2d --regularized -N1` | family | 7.966995214 | **7.968629202** | **+0.0205%** | 26.6G | 7.8G | -70.71% | 2.67s | 0.784s | -70.6% |
| om5 `-d --regularized -N1` | family | 7.989613065 | **7.968629202** | **-0.2626%** | 6.7G | 11.0G | +64.71% | 0.666s | 1.10s | +65.0% |
| om6 `-2d --regularized -N1` | family | 7.981574549 | **7.982650944** | **+0.0135%** | 15.8G | 6.0G | -61.78% | 1.61s | 0.626s | -61.1% |
| om6 `-d --regularized -N1` | family | 7.993490371 | **7.982650944** | **-0.1356%** | 7.8G | 9.3G | +19.74% | 0.775s | 0.968s | +24.9% |
| om7 `-2d --regularized -N1` | family | 7.945571863 | **7.950842444** | **+0.0663%** | 29.7G | 6.4G | -78.65% | 3.09s | 0.662s | -78.5% |
| om7 `-d --regularized -N1` | family | 7.992395554 | **7.950842444** | **-0.5199%** | 7.3G | 9.7G | +32.98% | 0.727s | 1.01s | +39.2% |
| om8 `-2d --regularized -N1` | family | 7.978912396 | **7.983599366** | **+0.0587%** | 22.5G | 6.2G | -72.68% | 2.38s | 0.652s | -72.6% |
| om8 `-d --regularized -N1` | family | 7.994735672 | **7.983599366** | **-0.1393%** | 9.4G | 9.5G | +0.67% | 0.935s | 0.999s | +6.8% |
| om6 `-2d --regularized -N10` | family | 7.981574549 | **7.982650944** | **+0.0135%** | 50.9G | 41.2G | -19.12% | 5.32s | 4.33s | -18.6% |
| om7 `-d --regularized -N10` | family | 7.945712536 | **7.948827768** | **+0.0392%** | 84.4G | 41.1G | -51.30% | 8.80s | 4.72s | -46.4% |
| om6 E50000 `-d --regularized -N10` | family | 7.959010571 | **7.960481009** | **+0.0185%** | 26.8G | 24.4G | -8.90% | 3.21s | 2.73s | -14.9% |
| om7 E50000 `-2d --regularized -N10` | family | 7.974818773 | **7.977053735** | **+0.0280%** | 29.9G | 25.4G | -15.18% | 3.31s | 2.82s | -14.8% |
| om5 `-2d --regularized -N10` | family | 7.967531078 | **7.968061942** | **+0.0067%** | 55.1G | 43.0G | -21.84% | 5.67s | 4.53s | -20.1% |
| om6 `-d --regularized -N10` | family | 7.981063575 | **7.984417755** | **+0.0420%** | 86.4G | 41.2G | -52.29% | 8.79s | 4.32s | -50.9% |
| om8 `-2d --regularized -N10` | family | 7.976140205 | **7.979831947** | **+0.0463%** | 69.9G | 40.7G | -41.78% | 7.61s | 4.51s | -40.8% |
| om7 E50000 `-d --regularized -N10` | family | 7.974818773 | **7.977053735** | **+0.0280%** | 28.1G | 24.4G | -13.50% | 3.31s | 2.83s | -14.6% |
| om8 E50000 `-2d --regularized -N10` | family | 7.990101633 | **7.991103173** | **+0.0125%** | 27.0G | 23.9G | -11.39% | 3.00s | 2.64s | -12.0% |
| om5 `-d --regularized -N10` | family | 7.965009721 | **7.968320007** | **+0.0416%** | 87.7G | 42.5G | -51.52% | 8.66s | 4.35s | -49.8% |
| om7 `-2d --regularized -N10` | family | 7.945712536 | **7.948827768** | **+0.0392%** | 60.5G | 41.2G | -31.91% | 6.50s | 4.50s | -30.8% |
| om8 `-d --regularized -N10` | family | 7.976681139 | **7.978630877** | **+0.0244%** | 80.9G | 41.2G | -49.00% | 8.33s | 4.50s | -45.9% |
| om6 E50000 `-2d --regularized -N10` | family | 7.959010571 | **7.960481009** | **+0.0185%** | 27.7G | 24.7G | -10.90% | 3.05s | 2.68s | -12.3% |
| om8 E50000 `-d --regularized -N10` | family | 7.990101633 | **7.991103173** | **+0.0125%** | 25.3G | 23.0G | -8.94% | 3.01s | 2.75s | -8.7% |

**Every cell where new is worse than old, and why.**

- **Bits: +0.0001% to +0.066% on the overlapping family's deep-repair rows, every one with fewer
  instructions (−1% to −79%).** The `-2d -N1` rows, the `-2d -c` planted rows, om3 / om5–om8 `-d` /
  `-2d` / `-F` at `-N10`, and the regularized family rows that were not rescued. This is the repair's
  cost cut (4): rounds that bought less than they cost are no longer paid for. Largest: om7
  `-2d --regularized -N1` +0.0663% for −78.7% instr (3.09 → 0.66 s), om8 `-2d --regularized -N1` +0.0587%
  for −72.7%, om7 `-2d -c` planted +0.0465% for −51.6%, om8 `-2d --regularized -N10` +0.0463% for −41.8%,
  om6 / om5 `-d --regularized -N10` +0.042% for −52% / −52%. No cell is worse by more than 0.07% in bits;
  every row off the overlapping family is bit-identical, malaria / air30k / wikispeedia included (F57).
- **Time on the rescued `-N1` rows, every one with a bits gain.** The old run returned a single module on
  each; the new run pays the collapsed attempt (now without the module coarsen, (4)) plus one two-level
  solve and its repair, and returns the `-2d -N1` answer. om3 / om4 `-d` +88% / +191% for −14.4% /
  −14.0%; om3 / om4 `-F` +97% / +269% for the same; om2 / om3 / om4 `--regularized` +118% / +122% / +70%
  for −5.3% / −8.9% / −5.3%. **om5 / om6 / om7 / om8 `--regularized`: +65% / +25% / +39% / +6.8% in
  seconds for −0.26% / −0.14% / −0.52% / −0.14% in bits — still a poor ratio on om5–om7.** On the first
  version of this PR those four read +385% / +192% / +412% / +241%; what is left is the rescue's
  two-level solve itself (om5: about 0.4 s), which no measured signal predicts will land next to
  one-level before running it. Flagged, not settled.
- **om8 `-d -N10` +0.76% instr / +2.2% wall for −1.034% bits; om8 `-F -N10` +13.4% / +12.8% for
  −1.347%.** The escalated probe completes the flat pipeline in the five flat-first trials (1); a bits
  gain on both.
- **science2001 `-C -N10`: the batch's single run read +16.8% wall on +0.28% instr (2.885 s against
  3.369 s).** Re-measured after the batch, three interleaved reps per arm on the same binaries (load
  6–7): old 2.881 / 2.865 / 2.916 s, new 2.934 / 2.918 / 2.888 s, 34.03–34.05G against 34.12–34.13G,
  bits identical; appended as reps 2–4 and the table shows the minimum, +0.8%. A base objective at
  `-N10` runs none of (4); the +0.28% in instructions was already in this PR's previous snapshot
  (34.06G → 34.14G).
- **Every other cell with new wall above old sits on an instruction delta below 1%** (multilayer,
  ninetriangles, jazz, netsci, lazega −5.7% instr, politicalblogs, air30k (meta), science2001 `-N1`,
  om2 E100000 / om6 / om7 E50000 / om8 `-d -N1`, om6 `-F -N1`, om6 / om7 E50000 `-d -N10`, air30k (reg.) /
  malaria `-C -2`, om3 E50000 `-2d --regularized -N10`; +1.0% to +6.9% wall), the noise floor of this
  session; the sub-millisecond rows carry startup variance.

**Where the time goes the other way, same bits:** every `-N10` row whose trials had a build above
one-level lost that build's refinement (2) — air30k −13%, air30k reg −13%, malaria −16%, om2 E100000
`-d` −46%, om3 / om4 `-d` −32% / −25%, the family's `-d --regularized -N10` rows −23% to −48% where their
bits do not move, `-F` on om3 / om4 −29% / −16% in instructions. At `-N1` malaria, air30k, air30k (reg.),
om5 and every other non-collapsing row is bit-identical to old: the lone trial's interior sweeps take
their doomed builds under one-level, so (4) leaves them refined as before.

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

#### (4) The cost cuts: attribution, and the numbers they moved against this PR's previous snapshot

Measured on an env-gated build of this commit (F57; arms interleaved, `-N1` rows min of 3; Δbits / Δs
against the cuts switched off, which is the PR's previous code). (a) = no re-derivation of a module of
≥ 95% of the states the search converged to; (b) = repeat a fresh derivation only while the last one
bought ≥ 0.01% per network re-clustered; (c) = the lone trial abandons a doomed build its interior
sweeps left above one-level.

| row | (a) | (b) | (c) | all |
|---|---|---|---|---|
| om5 reg `-d -N1` | +0.0205% / −58% | +0.0105% / −44% | 0 / −6% | +0.0205% / −64% |
| om7 reg `-d -N1` | +0.0663% / −68% | +0.0280% / −39% | 0 / −9% | +0.0663% / −73% |
| om2 reg `-d -N1` | 0 / +1% | +0.0006% / −4% | 0 / −10% | +0.0006% / −17% |
| om3 `-d -N1` | 0 / 0% | +0.0007% / −8% | 0 / −16% | +0.0007% / −24% |
| om4 `-F -d -N1` | 0 / 0% | +0.0001% / −8% | 0 / −4% | +0.0001% / −12% |
| om2 `-2d -N1` | 0 / −1% | +0.0037% / −19% | 0 / −2% | +0.0037% / −19% |
| om5 reg `-2d -N1` | +0.0205% / −72% | +0.0105% / −52% | 0 / +1% | +0.0205% / −70% |
| om2 `-2d -N10` | 0 / −3% | +0.0221% / −10% | 0 / −2% | +0.0221% / −10% |
| om5 reg `-d -N10` | +0.0416% / −39% | +0.0370% / −30% | 0 / +1% | +0.0416% / −37% |
| om7 `-d -N10` | 0 / +2% | +0.0143% / −3% | 0 / 0% | +0.0143% / −5% |
| om3 E50000 reg `-d -N10` | 0 / −5% | 0 / −5% | 0 / −5% | 0 / −6% |
| om5 E50000 reg `-d -N10` | 0 / −1% | 0 / +1% | 0 / +1% | 0 / +1% |

(c) is zero by construction off `-N1`; ±2% there is noise. Rejected on the way (F57): a structural re-draw
guard (re-cluster only modules that drifted > 5%), malaria `-C -2 -N10` +0.28% and om2 `-d -N10` +0.35%
in bits — on those networks re-drawing a nearly unchanged module pays; and (a) tested per module per
round, om3–om7 E50000 `--regularized -N10` +0.22–0.49% in bits — those winners are the one-level
fallback, which no search produced, and its top-down repair is the only operator that leaves it.

Against this PR's previous snapshot (`40d2521d`, md5 `1fcfc3ea`): every row whose bits moved, the bits
compared directly and the time as each session's new/old ratio against the same old binary (`b0b878cc`,
bit-identical old arm in both sessions), so machine load cancels. Every other row is bit-identical; the
only unmoved row whose instruction ratio rose by more than 1% is the sub-millisecond multilayer example
`-C -2` (+9% on 0.06G, startup).

| network | table | prev bits | new bits | Δbits | prev new/old instr | new new/old instr | Δ | prev new/old time | new new/old time | Δ |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| om7 `-2d --regularized -N1` | family | 7.945571863 | 7.950842444 | +0.0663% | 1.000 | 0.214 | -78.7% | 1.015 | 0.215 | -78.9% |
| om7 `-d --regularized -N1` | family | 7.945571863 | 7.950842444 | +0.0663% | 4.827 | 1.330 | -72.4% | 5.124 | 1.392 | -72.8% |
| om8 `-2d --regularized -N1` | family | 7.978912396 | 7.983599366 | +0.0587% | 1.000 | 0.273 | -72.7% | 0.995 | 0.274 | -72.4% |
| om8 `-d --regularized -N1` | family | 7.978912396 | 7.983599366 | +0.0587% | 3.193 | 1.007 | -68.5% | 3.407 | 1.068 | -68.6% |
| overlapping om7 `-2d -c` planted | `-C -N1` | 6.878354613 | 6.881554086 | +0.0465% | 1.000 | 0.484 | -51.6% | 0.992 | 0.494 | -50.3% |
| om8 `-2d --regularized -N10` | family | 7.976140205 | 7.979831947 | +0.0463% | 1.000 | 0.582 | -41.8% | 0.997 | 0.592 | -40.5% |
| om6 `-d --regularized -N10` | family | 7.981063575 | 7.984417755 | +0.0420% | 0.779 | 0.477 | -38.8% | 0.873 | 0.491 | -43.8% |
| om5 `-d --regularized -N10` | family | 7.965009721 | 7.968320007 | +0.0416% | 0.791 | 0.485 | -38.7% | 0.816 | 0.502 | -38.5% |
| om7 `-d --regularized -N10` | family | 7.945712536 | 7.948827768 | +0.0392% | 0.716 | 0.487 | -31.9% | 0.756 | 0.536 | -29.1% |
| om7 `-2d --regularized -N10` | family | 7.945712536 | 7.948827768 | +0.0392% | 1.000 | 0.681 | -31.9% | 1.024 | 0.692 | -32.4% |
| om7 E50000 `-2d --regularized -N10` | family | 7.974818773 | 7.977053735 | +0.0280% | 1.000 | 0.848 | -15.2% | 1.003 | 0.852 | -15.1% |
| om7 E50000 `-d --regularized -N10` | family | 7.974818773 | 7.977053735 | +0.0280% | 1.027 | 0.865 | -15.8% | 1.008 | 0.854 | -15.3% |
| om8 `-d --regularized -N10` | family | 7.976681139 | 7.978630877 | +0.0244% | 0.683 | 0.510 | -25.4% | 0.744 | 0.541 | -27.4% |
| overlapping om2 `-2d` | `-C -2` | 6.739271968 | 6.740761645 | +0.0221% | 1.000 | 0.895 | -10.5% | 0.978 | 0.903 | -7.7% |
| om5 `-2d --regularized -N1` | family | 7.966995214 | 7.968629202 | +0.0205% | 1.000 | 0.293 | -70.7% | 0.994 | 0.294 | -70.4% |
| om5 `-d --regularized -N1` | family | 7.966995214 | 7.968629202 | +0.0205% | 4.710 | 1.647 | -65.0% | 4.845 | 1.650 | -65.9% |
| om6 E50000 `-d --regularized -N10` | family | 7.959010571 | 7.960481009 | +0.0185% | 1.023 | 0.911 | -10.9% | 0.959 | 0.851 | -11.3% |
| om6 E50000 `-2d --regularized -N10` | family | 7.959010571 | 7.960481009 | +0.0185% | 1.000 | 0.891 | -10.9% | 0.987 | 0.877 | -11.1% |
| overlapping om7 `-d` | `-C` | 6.888473273 | 6.889455910 | +0.0143% | 1.001 | 0.957 | -4.4% | 0.994 | 0.962 | -3.2% |
| overlapping om7 `-d` | `-C -F` | 6.888473273 | 6.889455910 | +0.0143% | 1.001 | 0.949 | -5.2% | 0.994 | 0.949 | -4.6% |
| om6 `-2d --regularized -N1` | family | 7.981574549 | 7.982650944 | +0.0135% | 1.000 | 0.382 | -61.8% | 0.989 | 0.389 | -60.6% |
| om6 `-d --regularized -N1` | family | 7.981574549 | 7.982650944 | +0.0135% | 2.804 | 1.197 | -57.3% | 2.916 | 1.249 | -57.2% |
| om6 `-2d --regularized -N10` | family | 7.981574549 | 7.982650944 | +0.0135% | 1.000 | 0.809 | -19.1% | 1.001 | 0.814 | -18.7% |
| overlapping om8 `-2d` | `-C -N1` | 6.893377041 | 6.894258582 | +0.0128% | 1.000 | 0.814 | -18.6% | 1.019 | 0.812 | -20.2% |
| om8 E50000 `-2d --regularized -N10` | family | 7.990101633 | 7.991103173 | +0.0125% | 1.000 | 0.886 | -11.4% | 1.020 | 0.880 | -13.8% |
| om8 E50000 `-d --regularized -N10` | family | 7.990101633 | 7.991103173 | +0.0125% | 1.033 | 0.911 | -11.8% | 1.009 | 0.913 | -9.5% |
| overlapping om5 `-2d` | `-C -2` | 6.866617805 | 6.867301407 | +0.0100% | 1.000 | 0.959 | -4.1% | 1.009 | 0.961 | -4.8% |
| overlapping om5 `-d` | `-C -F` | 6.866617805 | 6.867301407 | +0.0100% | 0.805 | 0.765 | -5.0% | 0.809 | 0.760 | -6.1% |
| overlapping om5 `-d` | `-C` | 6.866617805 | 6.867301407 | +0.0100% | 0.759 | 0.724 | -4.7% | 0.769 | 0.736 | -4.3% |
| overlapping om7 `-2d` | `-C -2` | 6.889244164 | 6.889920890 | +0.0098% | 1.000 | 0.943 | -5.7% | 1.007 | 0.938 | -6.9% |
| overlapping om3 `-2d -c` planted | `-C -N1` | 6.820421208 | 6.820929855 | +0.0075% | 1.000 | 0.468 | -53.1% | 0.999 | 0.478 | -52.1% |
| om5 `-2d --regularized -N10` | family | 7.967531078 | 7.968061942 | +0.0067% | 1.000 | 0.782 | -21.8% | 0.985 | 0.799 | -18.9% |
| overlapping om7 `-2d` | `-C -N1` | 6.889332374 | 6.889674586 | +0.0050% | 1.000 | 0.856 | -14.4% | 1.001 | 0.858 | -14.3% |
| overlapping om8 `-2d -c` planted | `-C -N1` | 6.875237392 | 6.875540042 | +0.0044% | 1.000 | 0.832 | -16.8% | 0.999 | 0.820 | -17.9% |
| overlapping om2 `-2d` | `-C -N1` | 6.739358212 | 6.739607071 | +0.0037% | 1.000 | 0.813 | -18.7% | 1.009 | 0.831 | -17.7% |
| overlapping om6 `-2d` | `-C -N1` | 6.884042436 | 6.884236095 | +0.0028% | 1.000 | 0.909 | -9.1% | 1.008 | 0.926 | -8.1% |
| overlapping om8 `-2d` | `-C -2` | 6.887234466 | 6.887423150 | +0.0027% | 1.000 | 0.987 | -1.2% | 0.944 | 0.866 | -8.3% |
| overlapping om8 `-d` | `-C -F` | 6.887234466 | 6.887423150 | +0.0027% | 1.148 | 1.134 | -1.2% | 1.136 | 1.128 | -0.7% |
| overlapping om8 `-d` | `-C` | 6.887234466 | 6.887423150 | +0.0027% | 1.018 | 1.008 | -1.0% | 1.043 | 1.022 | -2.0% |
| overlapping om6 `-d` | `-C` | 6.884862145 | 6.884968970 | +0.0016% | 1.001 | 0.976 | -2.4% | 1.007 | 0.978 | -2.9% |
| overlapping om6 `-2d` | `-C -2` | 6.884862145 | 6.884968970 | +0.0016% | 1.000 | 0.971 | -2.9% | 1.005 | 0.973 | -3.2% |
| overlapping om6 `-d` | `-C -F` | 6.884862145 | 6.884968970 | +0.0016% | 1.001 | 0.971 | -3.0% | 0.976 | 0.973 | -0.4% |
| overlapping om5 `-2d -c` planted | `-C -N1` | 6.857778113 | 6.857876353 | +0.0014% | 1.000 | 0.755 | -24.5% | 0.992 | 0.743 | -25.1% |
| overlapping om6 `-2d -c` planted | `-C -N1` | 6.873378755 | 6.873464257 | +0.0012% | 0.999 | 0.852 | -14.7% | 0.982 | 0.846 | -13.9% |
| overlapping om5 `-2d` | `-C -N1` | 6.868127142 | 6.868180827 | +0.0008% | 1.000 | 0.901 | -9.9% | 0.999 | 0.908 | -9.2% |
| overlapping om3 `-2d` | `-C -N1` | 6.823513368 | 6.823562921 | +0.0007% | 1.000 | 0.864 | -13.6% | 0.997 | 0.861 | -13.6% |
| overlapping om3 `-d` | `-C -N1` | 6.823513368 | 6.823562921 | +0.0007% | 2.691 | 2.122 | -21.2% | 2.457 | 1.878 | -23.6% |
| overlapping om3 `-d` | `-C -F -N1` | 6.823513368 | 6.823562921 | +0.0007% | 2.790 | 2.187 | -21.6% | 2.496 | 1.966 | -21.2% |
| overlapping om4 `-2d -c` planted | `-C -N1` | 6.856239474 | 6.856285862 | +0.0007% | 1.000 | 0.710 | -29.0% | 0.998 | 0.727 | -27.2% |
| om2 `-2d --regularized -N1` | family | 7.548816177 | 7.548863183 | +0.0006% | 1.001 | 0.880 | -12.0% | 0.995 | 0.895 | -10.0% |
| om2 `-d --regularized -N1` | family | 7.548816177 | 7.548863183 | +0.0006% | 2.689 | 2.069 | -23.1% | 2.732 | 2.182 | -20.1% |
| overlapping om4 `-2d` | `-C -N1` | 6.861724654 | 6.861732911 | +0.0001% | 1.000 | 0.899 | -10.1% | 0.982 | 0.886 | -9.8% |
| overlapping om4 `-d` | `-C -N1` | 6.861724654 | 6.861732911 | +0.0001% | 3.308 | 2.990 | -9.6% | 3.125 | 2.912 | -6.8% |
| overlapping om4 `-d` | `-C -F -N1` | 6.861724654 | 6.861732911 | +0.0001% | 4.411 | 3.940 | -10.7% | 3.939 | 3.686 | -6.4% |
| overlapping om3 `-d` | `-C` | 6.822826504 | 6.822832994 | +0.0001% | 0.694 | 0.677 | -2.5% | 0.690 | 0.665 | -3.5% |
| overlapping om3 `-2d` | `-C -2` | 6.822826504 | 6.822832994 | +0.0001% | 1.000 | 0.980 | -2.0% | 0.995 | 0.983 | -1.2% |
| overlapping om3 `-d` | `-C -F` | 6.822826504 | 6.822832994 | +0.0001% | 0.728 | 0.709 | -2.7% | 0.718 | 0.685 | -4.5% |

### Old vs new columnar — standard search (`-C -N10`)

Overlapping and wikispeedia rows run `-C -d -N10`. om8 E100000 gains −1.03% in bits, where the escalated
probe now completes the flat pipeline in the five flat-first trials; om3 / om5–om7 move +0.0001% to
+0.014% where the deep repair's cost cut (4) stops paying for rounds that bought less than they cost;
every other row is bit-identical. Time moves down wherever a trial's build started above one-level and
is no longer refined (the memory objectives: malaria, air30k, the om rows).

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (+6.9%)</td><td align="right">0.1G (-0.09%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (+1.1%)</td><td align="right">0.1G (+0.25%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.048857953 (=)</td><td align="right">0.023s (-2.2%)</td><td align="right">0.3G (+0.15%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.233s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.717760238 (=)</td><td align="right">0.232s (-0.5%)</td><td align="right">2.8G (+0.19%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.056s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.057s (+2.1%)</td><td align="right">0.7G (+0.32%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">2.87s</td><td align="right">34.0G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">2.89s (+0.8%)</td><td align="right">34.1G (+0.26%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">18.7s</td><td align="right">181.2G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">18.6s (-0.3%)</td><td align="right">181.7G (+0.23%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (+2.5%)</td><td align="right">0.1G (-5.73%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (+0.2%)</td><td align="right">0.1G (-0.41%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.98s</td><td align="right">37.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.48s (-16.7%)</td><td align="right">31.5G (-16.08%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.67s</td><td align="right">44.5G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.19s (-13.2%)</td><td align="right">38.6G (-13.29%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.94s</td><td align="right">46.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.50s (-11.1%)</td><td align="right">40.3G (-13.12%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.05s</td><td align="right">97.8G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">9.14s (+1.0%)</td><td align="right">97.9G (+0.06%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.15s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.16s (+0.2%)</td><td align="right">35.5G (-0.01%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.32s</td><td align="right">47.4G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">4.29s (-0.7%)</td><td align="right">47.5G (+0.11%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822826504</td><td align="right">7.57s</td><td align="right">88.4G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822832994 (+0.0001%)</td><td align="right">5.04s (-33.5%)</td><td align="right">59.8G (-32.34%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">7.41s</td><td align="right">80.7G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.55s (-25.0%)</td><td align="right">60.6G (-24.93%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">7.73s</td><td align="right">81.4G</td><td align="right">308</td><td align="right">2</td><td align="right">6.867301407 (+0.0100%)</td><td align="right">5.69s (-26.4%)</td><td align="right">58.9G (-27.63%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">8.10s</td><td align="right">83.1G</td><td align="right">451</td><td align="right">2</td><td align="right">6.88496897 (+0.0016%)</td><td align="right">7.92s (-2.2%)</td><td align="right">81.1G (-2.35%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.888473273</td><td align="right">8.59s</td><td align="right">86.3G</td><td align="right">680</td><td align="right">2</td><td align="right">6.88945591 (+0.0143%)</td><td align="right">8.26s (-3.8%)</td><td align="right">82.6G (-4.33%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.959413622</td><td align="right">8.52s</td><td align="right">85.7G</td><td align="right">203</td><td align="right">4</td><td align="right">6.88742315 (-1.0344%)</td><td align="right">8.71s (+2.2%)</td><td align="right">86.4G (+0.76%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">6.773456578</td><td align="right">5.24s</td><td align="right">61.3G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.01s (-42.6%)</td><td align="right">33.2G (-45.76%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.851498646</td><td align="right">4.32s</td><td align="right">44.5G</td><td align="right">617</td><td align="right">4</td><td align="right">5.851498646 (=)</td><td align="right">4.31s (-0.3%)</td><td align="right">44.5G (+0.09%)</td><td align="right">617</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.876717868</td><td align="right">4.83s</td><td align="right">47.3G</td><td align="right">1031</td><td align="right">4</td><td align="right">4.876717868 (=)</td><td align="right">4.81s (-0.4%)</td><td align="right">47.4G (+0.11%)</td><td align="right">1031</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.150346795</td><td align="right">5.41s</td><td align="right">51.0G</td><td align="right">2171</td><td align="right">4</td><td align="right">4.150346795 (=)</td><td align="right">5.41s (+0.0%)</td><td align="right">51.1G (+0.13%)</td><td align="right">2171</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.617750514</td><td align="right">5.49s</td><td align="right">51.6G</td><td align="right">3050</td><td align="right">4</td><td align="right">3.617750514 (=)</td><td align="right">5.59s (+1.8%)</td><td align="right">51.7G (+0.15%)</td><td align="right">3050</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.201872271</td><td align="right">6.94s</td><td align="right">64.9G</td><td align="right">3424</td><td align="right">6</td><td align="right">3.201872271 (=)</td><td align="right">7.02s (+1.0%)</td><td align="right">65.0G (+0.13%)</td><td align="right">3424</td><td align="right">6</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.883308449</td><td align="right">7.48s</td><td align="right">69.6G</td><td align="right">4367</td><td align="right">5</td><td align="right">2.883308449 (=)</td><td align="right">7.46s (-0.2%)</td><td align="right">69.7G (+0.11%)</td><td align="right">4367</td><td align="right">5</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.679s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.669s (-1.4%)</td><td align="right">7.2G (-0.32%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Overlapping and wikispeedia rows as `-C -2d -N10`. Changes (1)–(3) do not touch the two-level pipeline;
the deep repair's cost cut (4) does, and moves om2 / om3 / om5–om8 by +0.0001% to +0.022% in bits for
−1% to −11% in instructions. Every other row is bit-identical and its time column is the session's
noise floor.

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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (+1.1%)</td><td align="right">0.1G (-0.56%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.861229775</td><td align="right">0.011s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (=)</td><td align="right">0.006s (-39.9%)</td><td align="right">0.1G (-0.09%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.283072584</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.283072584 (=)</td><td align="right">0.009s (-1.8%)</td><td align="right">0.2G (-0.25%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.63729688</td><td align="right">0.093s</td><td align="right">1.1G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.094s (+0.9%)</td><td align="right">1.1G (-0.02%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739575295</td><td align="right">0.042s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">6.739575295 (=)</td><td align="right">0.041s (-0.9%)</td><td align="right">0.5G (-0.06%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.949978834</td><td align="right">2.21s</td><td align="right">23.8G</td><td align="right">506</td><td align="right">2</td><td align="right">7.949978834 (=)</td><td align="right">2.22s (+0.5%)</td><td align="right">23.8G (-0.00%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.754216663</td><td align="right">17.1s</td><td align="right">117.4G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.754216663 (=)</td><td align="right">17.0s (-0.2%)</td><td align="right">117.4G (-0.01%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-9.7%)</td><td align="right">0.1G (-6.54%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-0.1%)</td><td align="right">0.1G (-0.87%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.400445378</td><td align="right">2.62s</td><td align="right">32.3G</td><td align="right">168</td><td align="right">2</td><td align="right">7.400445378 (=)</td><td align="right">2.65s (+1.2%)</td><td align="right">32.3G (+0.01%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393055049</td><td align="right">3.43s</td><td align="right">41.9G</td><td align="right">334</td><td align="right">2</td><td align="right">5.393055049 (=)</td><td align="right">3.39s (-1.2%)</td><td align="right">41.7G (-0.34%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571539329</td><td align="right">3.58s</td><td align="right">41.2G</td><td align="right">304</td><td align="right">2</td><td align="right">5.571539329 (=)</td><td align="right">3.62s (+1.3%)</td><td align="right">41.2G (+0.00%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.424143707</td><td align="right">9.86s</td><td align="right">104.7G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.424143707 (=)</td><td align="right">9.82s (-0.4%)</td><td align="right">104.5G (-0.22%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">2.91s</td><td align="right">31.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">2.93s (+0.6%)</td><td align="right">31.5G (+0.00%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739271968</td><td align="right">3.69s</td><td align="right">39.8G</td><td align="right">638</td><td align="right">2</td><td align="right">6.740761645 (+0.0221%)</td><td align="right">3.33s (-9.7%)</td><td align="right">35.6G (-10.53%)</td><td align="right">625</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.822826504</td><td align="right">6.40s</td><td align="right">78.8G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822832994 (+0.0001%)</td><td align="right">6.29s (-1.7%)</td><td align="right">77.3G (-1.96%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.866901786</td><td align="right">6.64s</td><td align="right">71.5G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">6.52s (-1.9%)</td><td align="right">70.6G (-1.27%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.866617805</td><td align="right">6.74s</td><td align="right">71.2G</td><td align="right">308</td><td align="right">2</td><td align="right">6.867301407 (+0.0100%)</td><td align="right">6.48s (-3.9%)</td><td align="right">68.4G (-4.05%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884862145</td><td align="right">7.02s</td><td align="right">71.0G</td><td align="right">451</td><td align="right">2</td><td align="right">6.88496897 (+0.0016%)</td><td align="right">6.83s (-2.7%)</td><td align="right">69.0G (-2.89%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.889244164</td><td align="right">7.46s</td><td align="right">73.3G</td><td align="right">675</td><td align="right">2</td><td align="right">6.88992089 (+0.0098%)</td><td align="right">7.00s (-6.2%)</td><td align="right">69.1G (-5.74%)</td><td align="right">663</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.887234466</td><td align="right">8.93s</td><td align="right">75.4G</td><td align="right">921</td><td align="right">2</td><td align="right">6.88742315 (+0.0027%)</td><td align="right">7.73s (-13.4%)</td><td align="right">74.4G (-1.25%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-2d`</td><td align="right">6.773456578</td><td align="right">3.38s</td><td align="right">38.0G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.34s (-1.1%)</td><td align="right">37.7G (-0.75%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-2d`</td><td align="right">6.258611497</td><td align="right">3.97s</td><td align="right">36.8G</td><td align="right">3236</td><td align="right">2</td><td align="right">6.258611497 (=)</td><td align="right">3.98s (+0.1%)</td><td align="right">36.8G (-0.00%)</td><td align="right">3236</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 E50000 `-2d`</td><td align="right">5.454385527</td><td align="right">3.87s</td><td align="right">33.7G</td><td align="right">4381</td><td align="right">2</td><td align="right">5.454385527 (=)</td><td align="right">3.88s (+0.3%)</td><td align="right">33.7G (-0.01%)</td><td align="right">4381</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 E50000 `-2d`</td><td align="right">4.903270512</td><td align="right">3.82s</td><td align="right">31.9G</td><td align="right">5443</td><td align="right">2</td><td align="right">4.903270512 (=)</td><td align="right">3.81s (-0.2%)</td><td align="right">31.9G (+0.01%)</td><td align="right">5443</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 E50000 `-2d`</td><td align="right">4.468778176</td><td align="right">3.90s</td><td align="right">32.3G</td><td align="right">6379</td><td align="right">2</td><td align="right">4.468778176 (=)</td><td align="right">3.88s (-0.6%)</td><td align="right">32.3G (+0.03%)</td><td align="right">6379</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 E50000 `-2d`</td><td align="right">4.143277395</td><td align="right">3.84s</td><td align="right">31.3G</td><td align="right">7097</td><td align="right">2</td><td align="right">4.143277395 (=)</td><td align="right">3.82s (-0.6%)</td><td align="right">31.2G (-0.51%)</td><td align="right">7097</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 E50000 `-2d`</td><td align="right">3.859069372</td><td align="right">3.91s</td><td align="right">31.5G</td><td align="right">7985</td><td align="right">2</td><td align="right">3.859069372 (=)</td><td align="right">3.89s (-0.4%)</td><td align="right">31.5G (+0.00%)</td><td align="right">7985</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.907904741</td><td align="right">0.667s</td><td align="right">6.9G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.633s (-5.2%)</td><td align="right">6.9G (-0.51%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

Interleaved minimum of 3 per arm. This is where the run-level rescue fires: a `-d -N1` row whose only
trial collapsed to one module now returns the `-2d -N1` answer, and pays the collapsed hierarchical
attempt (without its module coarsen, (4)) plus that two-level solve and its deep repair. Every other
`-d -N1` row is the old refined build, bit-identical (F56 second addendum, F57). The deep repair is no
longer what a `-2d -N1` row pays most for: on om5 regularized it was 2.8 s for −0.04% in bits and is
0.2 s for −0.02%, on om4 it keeps its −5.1% (7.2309 → 6.8617) at 0.7 s instead of 0.9 s. The `-2d -N1`
rows therefore move +0.0001% to +0.066% in bits for −9% to −79% in instructions. The pre-repair winner
selection that lets om4 `-N1` (6.8617) beat `-N10` (6.8669) in both `-2d` and `-d` is the two-level
pipeline's own and stays filed as #1083.

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.000s (-0.5%)</td><td align="right">0.1G (-0.59%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.899367957</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">2</td><td align="right">6.899367957 (=)</td><td align="right">0.001s (+0.3%)</td><td align="right">0.1G (-0.27%)</td><td align="right">11</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.047459862</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">4</td><td align="right">4.047459862 (=)</td><td align="right">0.003s (+2.1%)</td><td align="right">0.1G (-0.24%)</td><td align="right">7</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.730850312</td><td align="right">0.025s</td><td align="right">0.4G</td><td align="right">12</td><td align="right">5</td><td align="right">4.730850312 (=)</td><td align="right">0.025s (-0.8%)</td><td align="right">0.4G (+0.09%)</td><td align="right">12</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.758265349</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">3</td><td align="right">3</td><td align="right">6.758265349 (=)</td><td align="right">0.009s (-1.5%)</td><td align="right">0.2G (-0.21%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">0.376s</td><td align="right">5.7G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">0.383s (+1.9%)</td><td align="right">5.7G (+0.10%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">2.26s</td><td align="right">24.3G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">2.27s (+0.5%)</td><td align="right">24.3G (+0.13%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.041117399</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.041117399 (=)</td><td align="right">0.001s (-1.5%)</td><td align="right">0.1G (-1.31%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (+6.4%)</td><td align="right">0.1G (-0.29%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.491980364</td><td align="right">0.338s</td><td align="right">5.3G</td><td align="right">148</td><td align="right">3</td><td align="right">7.491980364 (=)</td><td align="right">0.335s (-1.1%)</td><td align="right">5.3G (+0.12%)</td><td align="right">148</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.470440768</td><td align="right">0.339s</td><td align="right">4.5G</td><td align="right">242</td><td align="right">3</td><td align="right">5.470440768 (=)</td><td align="right">0.327s (-3.4%)</td><td align="right">4.5G (+0.08%)</td><td align="right">242</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.657913279</td><td align="right">0.447s</td><td align="right">5.9G</td><td align="right">197</td><td align="right">3</td><td align="right">5.657913279 (=)</td><td align="right">0.443s (-0.9%)</td><td align="right">5.9G (+0.08%)</td><td align="right">197</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.546335898</td><td align="right">0.815s</td><td align="right">9.2G</td><td align="right">1614</td><td align="right">3</td><td align="right">7.546335898 (=)</td><td align="right">0.813s (-0.1%)</td><td align="right">9.2G (+0.09%)</td><td align="right">1614</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.460796773</td><td align="right">0.396s</td><td align="right">5.8G</td><td align="right">5</td><td align="right">3</td><td align="right">8.460796773 (=)</td><td align="right">0.388s (-1.9%)</td><td align="right">5.8G (+0.01%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739358212</td><td align="right">1.49s</td><td align="right">17.3G</td><td align="right">674</td><td align="right">2</td><td align="right">6.739607071 (+0.0037%)</td><td align="right">1.23s (-16.9%)</td><td align="right">14.0G (-18.69%)</td><td align="right">669</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.823513368</td><td align="right">1.21s</td><td align="right">15.0G</td><td align="right">72</td><td align="right">2</td><td align="right">6.823562921 (+0.0007%)</td><td align="right">1.05s (-13.9%)</td><td align="right">12.9G (-13.62%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.861724654</td><td align="right">1.52s</td><td align="right">17.5G</td><td align="right">141</td><td align="right">2</td><td align="right">6.861732911 (+0.0001%)</td><td align="right">1.35s (-11.4%)</td><td align="right">15.7G (-10.13%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.868127142</td><td align="right">1.73s</td><td align="right">19.1G</td><td align="right">293</td><td align="right">2</td><td align="right">6.868180827 (+0.0008%)</td><td align="right">1.57s (-9.2%)</td><td align="right">17.2G (-9.88%)</td><td align="right">291</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884042436</td><td align="right">1.68s</td><td align="right">18.0G</td><td align="right">455</td><td align="right">2</td><td align="right">6.884236095 (+0.0028%)</td><td align="right">1.56s (-7.4%)</td><td align="right">16.3G (-9.10%)</td><td align="right">453</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.889332374</td><td align="right">1.93s</td><td align="right">20.1G</td><td align="right">651</td><td align="right">2</td><td align="right">6.889674586 (+0.0050%)</td><td align="right">1.66s (-14.2%)</td><td align="right">17.2G (-14.40%)</td><td align="right">649</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.893377041</td><td align="right">2.17s</td><td align="right">22.0G</td><td align="right">911</td><td align="right">2</td><td align="right">6.894258582 (+0.0128%)</td><td align="right">1.76s (-18.8%)</td><td align="right">17.9G (-18.61%)</td><td align="right">907</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.29196634</td><td align="right">0.338s</td><td align="right">3.7G</td><td align="right">321</td><td align="right">4</td><td align="right">7.29196634 (=)</td><td align="right">0.334s (-1.2%)</td><td align="right">3.7G (+0.12%)</td><td align="right">321</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">7.97489927</td><td align="right">0.764s</td><td align="right">8.3G</td><td align="right">1</td><td align="right">2</td><td align="right">6.823562921 (-14.4370%)</td><td align="right">1.44s (+87.8%)</td><td align="right">17.5G (+112.17%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">7.9829318</td><td align="right">0.636s</td><td align="right">7.1G</td><td align="right">1</td><td align="right">2</td><td align="right">6.861732911 (-14.0450%)</td><td align="right">1.85s (+191.2%)</td><td align="right">21.2G (+199.03%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.812252898</td><td align="right">0.688s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.812252898 (=)</td><td align="right">0.685s (-0.4%)</td><td align="right">7.2G (+0.17%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.440161581</td><td align="right">0.719s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.440161581 (=)</td><td align="right">0.724s (+0.7%)</td><td align="right">7.5G (+0.17%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.192756466</td><td align="right">0.737s</td><td align="right">7.5G</td><td align="right">149</td><td align="right">4</td><td align="right">7.192756466 (=)</td><td align="right">0.732s (-0.6%)</td><td align="right">7.5G (+0.10%)</td><td align="right">149</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.967535764</td><td align="right">0.732s</td><td align="right">7.4G</td><td align="right">206</td><td align="right">4</td><td align="right">6.967535764 (=)</td><td align="right">0.749s (+2.3%)</td><td align="right">7.4G (+0.13%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">7.144517469</td><td align="right">0.380s</td><td align="right">4.5G</td><td align="right">3</td><td align="right">3</td><td align="right">7.144517469 (=)</td><td align="right">0.385s (+1.3%)</td><td align="right">4.5G (+0.13%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.854829799</td><td align="right">0.408s</td><td align="right">4.3G</td><td align="right">537</td><td align="right">4</td><td align="right">5.854829799 (=)</td><td align="right">0.406s (-0.6%)</td><td align="right">4.3G (+0.01%)</td><td align="right">537</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.898784516</td><td align="right">0.406s</td><td align="right">4.2G</td><td align="right">1867</td><td align="right">3</td><td align="right">4.898784516 (=)</td><td align="right">0.407s (+0.3%)</td><td align="right">4.2G (+0.15%)</td><td align="right">1867</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.1554786</td><td align="right">0.511s</td><td align="right">5.0G</td><td align="right">2156</td><td align="right">4</td><td align="right">4.1554786 (=)</td><td align="right">0.506s (-1.0%)</td><td align="right">5.0G (-0.10%)</td><td align="right">2156</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.620157111</td><td align="right">0.618s</td><td align="right">5.9G</td><td align="right">3056</td><td align="right">4</td><td align="right">3.620157111 (=)</td><td align="right">0.628s (+1.6%)</td><td align="right">5.9G (+0.08%)</td><td align="right">3056</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.21639819</td><td align="right">0.574s</td><td align="right">5.5G</td><td align="right">3417</td><td align="right">5</td><td align="right">3.21639819 (=)</td><td align="right">0.581s (+1.2%)</td><td align="right">5.6G (+0.19%)</td><td align="right">3417</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.885785338</td><td align="right">0.848s</td><td align="right">8.1G</td><td align="right">4371</td><td align="right">5</td><td align="right">2.885785338 (=)</td><td align="right">0.852s (+0.5%)</td><td align="right">8.1G (+0.14%)</td><td align="right">4371</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om2 `-2d -c` planted</td><td align="right">6.744721993</td><td align="right">0.535s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.744721993 (=)</td><td align="right">0.538s (+0.6%)</td><td align="right">6.2G (-0.02%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d -c` planted</td><td align="right">6.820421208</td><td align="right">0.771s</td><td align="right">9.1G</td><td align="right">62</td><td align="right">2</td><td align="right">6.820929855 (+0.0075%)</td><td align="right">0.369s (-52.2%)</td><td align="right">4.3G (-53.16%)</td><td align="right">50</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d -c` planted</td><td align="right">6.856239474</td><td align="right">0.886s</td><td align="right">10.3G</td><td align="right">132</td><td align="right">2</td><td align="right">6.856285862 (+0.0007%)</td><td align="right">0.644s (-27.3%)</td><td align="right">7.3G (-29.00%)</td><td align="right">129</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d -c` planted</td><td align="right">6.857778113</td><td align="right">1.15s</td><td align="right">12.5G</td><td align="right">296</td><td align="right">2</td><td align="right">6.857876353 (+0.0014%)</td><td align="right">0.852s (-25.7%)</td><td align="right">9.5G (-24.54%)</td><td align="right">287</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d -c` planted</td><td align="right">6.873378755</td><td align="right">1.21s</td><td align="right">13.2G</td><td align="right">446</td><td align="right">2</td><td align="right">6.873464257 (+0.0012%)</td><td align="right">1.03s (-15.4%)</td><td align="right">11.2G (-14.77%)</td><td align="right">444</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d -c` planted</td><td align="right">6.878354613</td><td align="right">1.84s</td><td align="right">19.8G</td><td align="right">650</td><td align="right">2</td><td align="right">6.881554086 (+0.0465%)</td><td align="right">0.908s (-50.6%)</td><td align="right">9.6G (-51.57%)</td><td align="right">624</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d -c` planted</td><td align="right">6.875237392</td><td align="right">1.38s</td><td align="right">14.2G</td><td align="right">894</td><td align="right">2</td><td align="right">6.875540042 (+0.0044%)</td><td align="right">1.13s (-18.0%)</td><td align="right">11.8G (-16.77%)</td><td align="right">885</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">6.066305904</td><td align="right">0.067s</td><td align="right">0.8G</td><td align="right">187</td><td align="right">3</td><td align="right">6.066305904 (=)</td><td align="right">0.067s (+0.4%)</td><td align="right">0.8G (+0.17%)</td><td align="right">187</td><td align="right">3</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.087s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.087s (+0.9%)</td><td align="right">1.0G (-0.13%)</td><td align="right">184</td><td align="right">2</td></tr>
</tbody>
</table>

### The overlapping family in full

Every configuration of the planted overlapping state networks, both arms, now at both trigram densities
(F55). The `-d --regularized -N1` rows all collapsed to one module on the old binary and are rescued on
the new one. The `-N10` rows have a flat-first trial that escapes; they and the `-2d` rows move only
where the deep repair's cost cut (4) bites — the E100000 om5–om8 rows (+0.007% to +0.066% in bits for
−19% to −79% in instructions) and om6–om8 E50000 (+0.013% to +0.028% for −9% to −15%).

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
<tr><td align="right">om2 E100000 `-2d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">3.43s</td><td align="right">37.8G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.38s (-1.4%)</td><td align="right">36.7G (-2.81%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 E100000 `-d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">5.49s</td><td align="right">64.1G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.08s (-43.9%)</td><td align="right">33.5G (-47.80%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.47s</td><td align="right">25.0G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.41s (-2.3%)</td><td align="right">24.8G (-1.13%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N1`</td><td align="right">7.548816177</td><td align="right">0.760s</td><td align="right">8.2G</td><td align="right">120</td><td align="right">2</td><td align="right">7.548863183 (+0.0006%)</td><td align="right">0.680s (-10.5%)</td><td align="right">7.2G (-11.96%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">3.26s</td><td align="right">36.3G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.24s (-31.5%)</td><td align="right">23.8G (-34.48%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N1`</td><td align="right">7.970508085</td><td align="right">0.378s</td><td align="right">4.3G</td><td align="right">1</td><td align="right">2</td><td align="right">7.548863183 (-5.2901%)</td><td align="right">0.824s (+118.2%)</td><td align="right">8.8G (+106.93%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om2 planted, `-2d --no-infomap -c`</td><td align="right">6.789039995</td><td align="right">0.047s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.789039995 (=)</td><td align="right">0.048s (+0.6%)</td><td align="right">0.6G (-0.00%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N10`</td><td align="right">7.931196935</td><td align="right">2.69s</td><td align="right">27.9G</td><td align="right">49</td><td align="right">2</td><td align="right">7.931196935 (=)</td><td align="right">2.75s (+2.3%)</td><td align="right">27.9G (-0.02%)</td><td align="right">49</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N10`</td><td align="right">7.931196935</td><td align="right">3.74s</td><td align="right">36.4G</td><td align="right">49</td><td align="right">2</td><td align="right">7.931196935 (=)</td><td align="right">2.89s (-22.8%)</td><td align="right">28.1G (-22.91%)</td><td align="right">49</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">4.50s</td><td align="right">48.0G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">4.44s (-1.4%)</td><td align="right">48.0G (+0.00%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">0.878s</td><td align="right">9.7G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">0.831s (-5.3%)</td><td align="right">9.2G (-5.13%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">5.74s</td><td align="right">63.3G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">4.18s (-27.2%)</td><td align="right">45.0G (-28.97%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N1`</td><td align="right">7.97414175</td><td align="right">0.511s</td><td align="right">5.5G</td><td align="right">1</td><td align="right">2</td><td align="right">7.261268486 (-8.9398%)</td><td align="right">1.13s (+121.9%)</td><td align="right">12.5G (+127.95%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 planted, `-2d --no-infomap -c`</td><td align="right">6.837980937</td><td align="right">0.085s</td><td align="right">0.9G</td><td align="right">12</td><td align="right">2</td><td align="right">6.837980937 (=)</td><td align="right">0.083s (-2.8%)</td><td align="right">0.9G (-0.04%)</td><td align="right">12</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N10`</td><td align="right">7.940858042</td><td align="right">2.87s</td><td align="right">27.4G</td><td align="right">41</td><td align="right">2</td><td align="right">7.940858042 (=)</td><td align="right">2.86s (-0.3%)</td><td align="right">27.4G (+0.00%)</td><td align="right">41</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N10`</td><td align="right">7.940858042</td><td align="right">4.53s</td><td align="right">45.3G</td><td align="right">41</td><td align="right">2</td><td align="right">7.940858042 (=)</td><td align="right">2.85s (-37.2%)</td><td align="right">27.2G (-39.84%)</td><td align="right">41</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N10`</td><td align="right">7.556894677</td><td align="right">4.85s</td><td align="right">49.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">4.88s (+0.7%)</td><td align="right">48.9G (-0.38%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">0.897s</td><td align="right">9.2G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">0.884s (-1.5%)</td><td align="right">9.1G (-2.03%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N10`</td><td align="right">7.556653713</td><td align="right">6.54s</td><td align="right">67.5G</td><td align="right">78</td><td align="right">2</td><td align="right">7.556653713 (=)</td><td align="right">4.73s (-27.7%)</td><td align="right">48.6G (-28.10%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N1`</td><td align="right">7.9828492</td><td align="right">0.704s</td><td align="right">7.3G</td><td align="right">1</td><td align="right">2</td><td align="right">7.556894677 (-5.3359%)</td><td align="right">1.20s (+70.1%)</td><td align="right">12.3G (+67.36%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 planted, `-2d --no-infomap -c`</td><td align="right">6.880650147</td><td align="right">0.097s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.880650147 (=)</td><td align="right">0.098s (+0.7%)</td><td align="right">1.0G (-0.05%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N10`</td><td align="right">7.969030601</td><td align="right">3.11s</td><td align="right">29.0G</td><td align="right">26</td><td align="right">2</td><td align="right">7.969030601 (=)</td><td align="right">3.06s (-1.7%)</td><td align="right">29.0G (-0.08%)</td><td align="right">26</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N10`</td><td align="right">7.969030601</td><td align="right">4.71s</td><td align="right">45.2G</td><td align="right">26</td><td align="right">2</td><td align="right">7.969030601 (=)</td><td align="right">3.08s (-34.6%)</td><td align="right">28.2G (-37.57%)</td><td align="right">26</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N10`</td><td align="right">7.967531078</td><td align="right">5.67s</td><td align="right">55.1G</td><td align="right">98</td><td align="right">2</td><td align="right">7.968061942 (+0.0067%)</td><td align="right">4.53s (-20.1%)</td><td align="right">43.0G (-21.84%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N1`</td><td align="right">7.966995214</td><td align="right">2.67s</td><td align="right">26.6G</td><td align="right">104</td><td align="right">2</td><td align="right">7.968629202 (+0.0205%)</td><td align="right">0.784s (-70.6%)</td><td align="right">7.8G (-70.71%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N10`</td><td align="right">7.965009721</td><td align="right">8.66s</td><td align="right">87.7G</td><td align="right">106</td><td align="right">2</td><td align="right">7.968320007 (+0.0416%)</td><td align="right">4.35s (-49.8%)</td><td align="right">42.5G (-51.52%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N1`</td><td align="right">7.989613065</td><td align="right">0.666s</td><td align="right">6.7G</td><td align="right">1</td><td align="right">2</td><td align="right">7.968629202 (-0.2626%)</td><td align="right">1.10s (+65.0%)</td><td align="right">11.0G (+64.71%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 planted, `-2d --no-infomap -c`</td><td align="right">6.902222527</td><td align="right">0.111s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.902222527 (=)</td><td align="right">0.108s (-2.5%)</td><td align="right">1.1G (+0.01%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N10`</td><td align="right">7.959010571</td><td align="right">3.05s</td><td align="right">27.7G</td><td align="right">26</td><td align="right">2</td><td align="right">7.960481009 (+0.0185%)</td><td align="right">2.68s (-12.3%)</td><td align="right">24.7G (-10.90%)</td><td align="right">23</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N10`</td><td align="right">7.959010571</td><td align="right">3.21s</td><td align="right">26.8G</td><td align="right">26</td><td align="right">2</td><td align="right">7.960481009 (+0.0185%)</td><td align="right">2.73s (-14.9%)</td><td align="right">24.4G (-8.90%)</td><td align="right">23</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N10`</td><td align="right">7.981574549</td><td align="right">5.32s</td><td align="right">50.9G</td><td align="right">113</td><td align="right">2</td><td align="right">7.982650944 (+0.0135%)</td><td align="right">4.33s (-18.6%)</td><td align="right">41.2G (-19.12%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N1`</td><td align="right">7.981574549</td><td align="right">1.61s</td><td align="right">15.8G</td><td align="right">113</td><td align="right">2</td><td align="right">7.982650944 (+0.0135%)</td><td align="right">0.626s (-61.1%)</td><td align="right">6.0G (-61.78%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N10`</td><td align="right">7.981063575</td><td align="right">8.79s</td><td align="right">86.4G</td><td align="right">119</td><td align="right">2</td><td align="right">7.984417755 (+0.0420%)</td><td align="right">4.32s (-50.9%)</td><td align="right">41.2G (-52.29%)</td><td align="right">102</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N1`</td><td align="right">7.993490371</td><td align="right">0.775s</td><td align="right">7.8G</td><td align="right">1</td><td align="right">2</td><td align="right">7.982650944 (-0.1356%)</td><td align="right">0.968s (+24.9%)</td><td align="right">9.3G (+19.74%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 planted, `-2d --no-infomap -c`</td><td align="right">6.930934993</td><td align="right">0.116s</td><td align="right">1.1G</td><td align="right">24</td><td align="right">2</td><td align="right">6.930934993 (=)</td><td align="right">0.115s (-0.3%)</td><td align="right">1.1G (-0.14%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N10`</td><td align="right">7.974818773</td><td align="right">3.31s</td><td align="right">29.9G</td><td align="right">23</td><td align="right">2</td><td align="right">7.977053735 (+0.0280%)</td><td align="right">2.82s (-14.8%)</td><td align="right">25.4G (-15.18%)</td><td align="right">21</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N10`</td><td align="right">7.974818773</td><td align="right">3.31s</td><td align="right">28.1G</td><td align="right">23</td><td align="right">2</td><td align="right">7.977053735 (+0.0280%)</td><td align="right">2.83s (-14.6%)</td><td align="right">24.4G (-13.50%)</td><td align="right">21</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N10`</td><td align="right">7.945712536</td><td align="right">6.50s</td><td align="right">60.5G</td><td align="right">184</td><td align="right">2</td><td align="right">7.948827768 (+0.0392%)</td><td align="right">4.50s (-30.8%)</td><td align="right">41.2G (-31.91%)</td><td align="right">166</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N1`</td><td align="right">7.945571863</td><td align="right">3.09s</td><td align="right">29.7G</td><td align="right">187</td><td align="right">2</td><td align="right">7.950842444 (+0.0663%)</td><td align="right">0.662s (-78.5%)</td><td align="right">6.4G (-78.65%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N10`</td><td align="right">7.945712536</td><td align="right">8.80s</td><td align="right">84.4G</td><td align="right">184</td><td align="right">2</td><td align="right">7.948827768 (+0.0392%)</td><td align="right">4.72s (-46.4%)</td><td align="right">41.1G (-51.30%)</td><td align="right">166</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N1`</td><td align="right">7.992395554</td><td align="right">0.727s</td><td align="right">7.3G</td><td align="right">1</td><td align="right">2</td><td align="right">7.950842444 (-0.5199%)</td><td align="right">1.01s (+39.2%)</td><td align="right">9.7G (+32.98%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">om7 planted, `-2d --no-infomap -c`</td><td align="right">6.957516072</td><td align="right">0.136s</td><td align="right">1.3G</td><td align="right">28</td><td align="right">2</td><td align="right">6.957516072 (=)</td><td align="right">0.135s (-0.4%)</td><td align="right">1.3G (-0.05%)</td><td align="right">28</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N10`</td><td align="right">7.990101633</td><td align="right">3.00s</td><td align="right">27.0G</td><td align="right">8</td><td align="right">2</td><td align="right">7.991103173 (+0.0125%)</td><td align="right">2.64s (-12.0%)</td><td align="right">23.9G (-11.39%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N10`</td><td align="right">7.990101633</td><td align="right">3.01s</td><td align="right">25.3G</td><td align="right">8</td><td align="right">2</td><td align="right">7.991103173 (+0.0125%)</td><td align="right">2.75s (-8.7%)</td><td align="right">23.0G (-8.94%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N10`</td><td align="right">7.976140205</td><td align="right">7.61s</td><td align="right">69.9G</td><td align="right">240</td><td align="right">2</td><td align="right">7.979831947 (+0.0463%)</td><td align="right">4.51s (-40.8%)</td><td align="right">40.7G (-41.78%)</td><td align="right">221</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N1`</td><td align="right">7.978912396</td><td align="right">2.38s</td><td align="right">22.5G</td><td align="right">234</td><td align="right">2</td><td align="right">7.983599366 (+0.0587%)</td><td align="right">0.652s (-72.6%)</td><td align="right">6.2G (-72.68%)</td><td align="right">216</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N10`</td><td align="right">7.976681139</td><td align="right">8.33s</td><td align="right">80.9G</td><td align="right">256</td><td align="right">2</td><td align="right">7.978630877 (+0.0244%)</td><td align="right">4.50s (-45.9%)</td><td align="right">41.2G (-49.00%)</td><td align="right">236</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N1`</td><td align="right">7.994735672</td><td align="right">0.935s</td><td align="right">9.4G</td><td align="right">1</td><td align="right">2</td><td align="right">7.983599366 (-0.1393%)</td><td align="right">0.999s (+6.8%)</td><td align="right">9.5G (+0.67%)</td><td align="right">216</td><td align="right">2</td></tr>
<tr><td align="right">om8 planted, `-2d --no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.125s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.125s (-0.1%)</td><td align="right">1.2G (+0.00%)</td><td align="right">32</td><td align="right">2</td></tr>
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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">3.71s</td><td align="right">40.2G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">3.69s (-0.6%)</td><td align="right">40.2G (+0.13%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822826504</td><td align="right">6.78s</td><td align="right">78.3G</td><td align="right">70</td><td align="right">2</td><td align="right">6.822832994 (+0.0001%)</td><td align="right">4.65s (-31.5%)</td><td align="right">55.4G (-29.15%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.25s</td><td align="right">66.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.24s (-16.2%)</td><td align="right">55.9G (-16.01%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">6.87s</td><td align="right">70.9G</td><td align="right">308</td><td align="right">2</td><td align="right">6.867301407 (+0.0100%)</td><td align="right">5.22s (-24.0%)</td><td align="right">54.2G (-23.54%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">6.86s</td><td align="right">68.5G</td><td align="right">451</td><td align="right">2</td><td align="right">6.88496897 (+0.0016%)</td><td align="right">6.67s (-2.7%)</td><td align="right">66.6G (-2.88%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.888473273</td><td align="right">7.51s</td><td align="right">73.4G</td><td align="right">680</td><td align="right">2</td><td align="right">6.88945591 (+0.0143%)</td><td align="right">7.12s (-5.1%)</td><td align="right">69.6G (-5.08%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.981469446</td><td align="right">6.93s</td><td align="right">65.9G</td><td align="right">227</td><td align="right">4</td><td align="right">6.88742315 (-1.3471%)</td><td align="right">7.82s (+12.8%)</td><td align="right">74.7G (+13.40%)</td><td align="right">919</td><td align="right">2</td></tr>
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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.321678354</td><td align="right">0.245s</td><td align="right">2.6G</td><td align="right">436</td><td align="right">4</td><td align="right">7.321678354 (=)</td><td align="right">0.246s (+0.5%)</td><td align="right">2.6G (+0.15%)</td><td align="right">436</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">7.97489927</td><td align="right">0.730s</td><td align="right">7.8G</td><td align="right">1</td><td align="right">2</td><td align="right">6.823562921 (-14.4370%)</td><td align="right">1.44s (+96.6%)</td><td align="right">17.1G (+118.67%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">7.9829318</td><td align="right">0.448s</td><td align="right">4.8G</td><td align="right">1</td><td align="right">2</td><td align="right">6.861732911 (-14.0450%)</td><td align="right">1.65s (+268.6%)</td><td align="right">18.9G (+293.96%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.798283664</td><td align="right">0.578s</td><td align="right">5.7G</td><td align="right">96</td><td align="right">4</td><td align="right">7.798283664 (=)</td><td align="right">0.570s (-1.3%)</td><td align="right">5.7G (+0.13%)</td><td align="right">96</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.507066407</td><td align="right">0.598s</td><td align="right">5.8G</td><td align="right">87</td><td align="right">4</td><td align="right">7.507066407 (=)</td><td align="right">0.614s (+2.6%)</td><td align="right">5.8G (+0.18%)</td><td align="right">87</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.238675817</td><td align="right">0.537s</td><td align="right">5.2G</td><td align="right">145</td><td align="right">4</td><td align="right">7.238675817 (=)</td><td align="right">0.540s (+0.5%)</td><td align="right">5.2G (+0.23%)</td><td align="right">145</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">7.022972054</td><td align="right">0.535s</td><td align="right">5.2G</td><td align="right">201</td><td align="right">4</td><td align="right">7.022972054 (=)</td><td align="right">0.540s (+0.9%)</td><td align="right">5.2G (+0.21%)</td><td align="right">201</td><td align="right">4</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-21.1%)</td><td align="right">0.1G (-1.86%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (-3.0%)</td><td align="right">0.1G (-2.26%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.03324474 (-0.3856%)</td><td align="right">0.014s (-39.4%)</td><td align="right">0.2G (-29.27%)</td><td align="right">9</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.232s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.754013143 (+0.7684%)</td><td align="right">0.143s (-38.5%)</td><td align="right">1.7G (-37.66%)</td><td align="right">10</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.057s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.054s (-6.3%)</td><td align="right">0.7G (-4.12%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">2.89s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">2.83s (-2.0%)</td><td align="right">33.1G (-2.99%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">18.6s</td><td align="right">181.7G</td><td align="right">5</td><td align="right">6</td><td align="right">5.620539396 (+1.1539%)</td><td align="right">13.9s (-25.6%)</td><td align="right">126.7G (-30.28%)</td><td align="right">135</td><td align="right">5</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-10.0%)</td><td align="right">0.1G (-1.94%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+20.6%)</td><td align="right">0.1G (-0.62%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.48s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.43s (-2.0%)</td><td align="right">30.6G (-2.64%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.19s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">2.91s (-8.6%)</td><td align="right">35.9G (-6.97%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.50s</td><td align="right">40.3G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.01s (-14.0%)</td><td align="right">35.2G (-12.70%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.14s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">8.86s (-3.1%)</td><td align="right">95.3G (-2.60%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.16s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.07s (-2.7%)</td><td align="right">34.7G (-2.20%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.29s</td><td align="right">47.5G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">3.69s (-14.0%)</td><td align="right">40.2G (-15.25%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.04s</td><td align="right">59.8G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">4.65s (-7.7%)</td><td align="right">55.4G (-7.28%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">5.55s</td><td align="right">60.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.24s (-5.6%)</td><td align="right">55.9G (-7.65%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">5.69s</td><td align="right">58.9G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">5.22s (-8.2%)</td><td align="right">54.2G (-7.98%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">7.92s</td><td align="right">81.1G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">6.67s (-15.7%)</td><td align="right">66.6G (-17.97%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">8.26s</td><td align="right">82.6G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">7.12s (-13.8%)</td><td align="right">69.6G (-15.69%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">8.71s</td><td align="right">86.4G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">7.82s (-10.2%)</td><td align="right">74.7G (-13.50%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.669s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.675s (+0.9%)</td><td align="right">6.9G (-4.00%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.078067323 (-8.7135%)</td><td align="right">0.001s (-5.9%)</td><td align="right">0.1G (+0.89%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367 (+0.0797%)</td><td align="right">0.006s (-0.2%)</td><td align="right">0.1G (+0.16%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">3.892209764 (-3.8689%)</td><td align="right">0.023s (-0.4%)</td><td align="right">0.3G (+0.98%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.232s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.509265423 (-4.4194%)</td><td align="right">0.234s (+0.9%)</td><td align="right">2.9G (+3.38%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.057s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502 (+0.7165%)</td><td align="right">0.057s (-0.2%)</td><td align="right">0.7G (+1.99%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">2.89s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">8.009172258 (+2.5773%)</td><td align="right">2.69s (-7.0%)</td><td align="right">31.1G (-8.71%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">18.6s</td><td align="right">181.7G</td><td align="right">5</td><td align="right">6</td><td align="right">5.512433077 (-0.7917%)</td><td align="right">18.8s (+1.0%)</td><td align="right">182.7G (+0.60%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653 (-0.8182%)</td><td align="right">0.003s (-9.5%)</td><td align="right">0.1G (-0.71%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (-4.1040%)</td><td align="right">0.000s (+0.3%)</td><td align="right">0.1G (+0.15%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.48s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.438064454 (+0.6171%)</td><td align="right">2.65s (+6.9%)</td><td align="right">33.9G (+7.74%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.19s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.378824196 (-0.2496%)</td><td align="right">3.20s (+0.2%)</td><td align="right">38.4G (-0.41%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.50s</td><td align="right">40.3G</td><td align="right">228</td><td align="right">3</td><td align="right">5.56659036 (-0.1426%)</td><td align="right">3.49s (-0.2%)</td><td align="right">40.1G (-0.60%)</td><td align="right">220</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.14s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.192724425 (-3.0848%)</td><td align="right">7.93s (-13.3%)</td><td align="right">85.9G (-12.23%)</td><td align="right">1910</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.16s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451 (+2.5761%)</td><td align="right">3.13s (-0.7%)</td><td align="right">35.3G (-0.43%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.669s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.903208727 (-0.0795%)</td><td align="right">0.672s (+0.3%)</td><td align="right">6.9G (-4.41%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-78.5%)</td><td align="right">0.1G (-30.44%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">5</td><td align="right">2</td><td align="right">6.862755928 (-0.0042%)</td><td align="right">0.006s (-70.7%)</td><td align="right">0.1G (-54.97%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.026557116</td><td align="right">0.119s</td><td align="right">1.3G</td><td align="right">12</td><td align="right">5</td><td align="right">4.048857953 (+0.5538%)</td><td align="right">0.023s (-80.9%)</td><td align="right">0.3G (-76.48%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.736412597</td><td align="right">1.89s</td><td align="right">20.2G</td><td align="right">11</td><td align="right">6</td><td align="right">4.717760238 (-0.3938%)</td><td align="right">0.232s (-87.7%)</td><td align="right">2.8G (-86.12%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.738927979</td><td align="right">0.136s</td><td align="right">1.4G</td><td align="right">80</td><td align="right">3</td><td align="right">6.740943136 (+0.0299%)</td><td align="right">0.057s (-57.8%)</td><td align="right">0.7G (-49.84%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.805465772</td><td align="right">7.94s</td><td align="right">63.0G</td><td align="right">220</td><td align="right">4</td><td align="right">7.807937174 (+0.0317%)</td><td align="right">2.89s (-63.6%)</td><td align="right">34.1G (-45.84%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.55442136</td><td align="right">155.7s</td><td align="right">1115.4G</td><td align="right">766</td><td align="right">9</td><td align="right">5.556421705 (+0.0360%)</td><td align="right">18.6s (-88.0%)</td><td align="right">181.7G (-83.71%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-68.2%)</td><td align="right">0.1G (-49.60%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-53.0%)</td><td align="right">0.1G (-37.20%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502028585</td><td align="right">9.20s</td><td align="right">78.1G</td><td align="right">144</td><td align="right">3</td><td align="right">7.392442593 (-1.4608%)</td><td align="right">2.48s (-73.0%)</td><td align="right">31.5G (-59.73%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392441014</td><td align="right">11.9s</td><td align="right">120.6G</td><td align="right">251</td><td align="right">3</td><td align="right">5.392285003 (-0.0029%)</td><td align="right">3.19s (-73.2%)</td><td align="right">38.6G (-68.03%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578435633</td><td align="right">7.76s</td><td align="right">81.7G</td><td align="right">301</td><td align="right">3</td><td align="right">5.574537176 (-0.0699%)</td><td align="right">3.50s (-54.9%)</td><td align="right">40.3G (-50.62%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">8.432467909</td><td align="right">5.55s</td><td align="right">52.1G</td><td align="right">114</td><td align="right">4</td><td align="right">7.421664324 (-11.9870%)</td><td align="right">9.14s (+64.7%)</td><td align="right">97.9G (+87.85%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">7.938575228</td><td align="right">6.93s</td><td align="right">57.0G</td><td align="right">25</td><td align="right">4</td><td align="right">8.235585529 (+3.7414%)</td><td align="right">3.16s (-54.5%)</td><td align="right">35.5G (-37.74%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.88554258</td><td align="right">2.23s</td><td align="right">22.4G</td><td align="right">184</td><td align="right">3</td><td align="right">5.907904741 (+0.3800%)</td><td align="right">0.669s (-70.0%)</td><td align="right">7.2G (-67.97%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-52.1%)</td><td align="right">0.1G (-37.24%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">5</td><td align="right">2</td><td align="right">6.861229775 (-0.0265%)</td><td align="right">0.006s (-47.1%)</td><td align="right">0.1G (-32.97%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.285012668</td><td align="right">0.031s</td><td align="right">0.4G</td><td align="right">56</td><td align="right">2</td><td align="right">4.283072584 (-0.0453%)</td><td align="right">0.009s (-72.4%)</td><td align="right">0.2G (-60.81%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.600443859</td><td align="right">0.698s</td><td align="right">5.9G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.094s (-86.5%)</td><td align="right">1.1G (-80.55%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739721413</td><td align="right">0.168s</td><td align="right">0.8G</td><td align="right">80</td><td align="right">2</td><td align="right">6.739575295 (-0.0022%)</td><td align="right">0.041s (-75.4%)</td><td align="right">0.5G (-35.34%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.9500396</td><td align="right">4.41s</td><td align="right">29.3G</td><td align="right">496</td><td align="right">2</td><td align="right">7.949978834 (-0.0008%)</td><td align="right">2.22s (-49.7%)</td><td align="right">23.8G (-18.68%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.742988533</td><td align="right">45.2s</td><td align="right">251.8G</td><td align="right">11809</td><td align="right">2</td><td align="right">6.754216663 (+0.1665%)</td><td align="right">17.0s (-62.4%)</td><td align="right">117.4G (-53.37%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-53.4%)</td><td align="right">0.1G (-3.17%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-60.1%)</td><td align="right">0.1G (-38.67%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.50595639</td><td align="right">6.58s</td><td align="right">55.2G</td><td align="right">142</td><td align="right">2</td><td align="right">7.400445378 (-1.4057%)</td><td align="right">2.65s (-59.7%)</td><td align="right">32.3G (-41.53%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393312779</td><td align="right">4.70s</td><td align="right">43.9G</td><td align="right">332</td><td align="right">2</td><td align="right">5.393055049 (-0.0048%)</td><td align="right">3.39s (-27.9%)</td><td align="right">41.7G (-4.95%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.579216889</td><td align="right">5.84s</td><td align="right">58.6G</td><td align="right">301</td><td align="right">2</td><td align="right">5.571539329 (-0.1376%)</td><td align="right">3.62s (-38.0%)</td><td align="right">41.2G (-29.68%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.131110023</td><td align="right">5.04s</td><td align="right">36.7G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (+1.2849%)</td><td align="right">2.93s (-41.9%)</td><td align="right">31.5G (-14.06%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.892212121</td><td align="right">1.70s</td><td align="right">17.2G</td><td align="right">184</td><td align="right">2</td><td align="right">5.907904741 (+0.2663%)</td><td align="right">0.633s (-62.8%)</td><td align="right">6.9G (-60.00%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>
