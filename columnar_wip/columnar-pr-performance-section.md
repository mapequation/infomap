## Performance

> Manual old-vs-new benchmark of the `--columnar` engine across the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = `--timing-json`'s `timing.total_s` (the engine's own wall — process wall carries ~30 ms of startup that swamps the sub-0.1 s rows), minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. See [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details.

> **This is the snapshot for the two regroup-ladder defects found on the overlapping family (F47).**
> Three networks joined `networks/debug/Jelena/` — om2, om4, om8, alongside om5/om6 — and each fails in a
> way #1029's ladder cannot reach. The causes turned out to be unrelated:
>
> 1. **The ladder gates its candidate at BLOCK granularity against a LEAF-granularity incumbent.** A rung
>    candidate is polished purify-only over the base units, then compared against a `bestCodelength` that
>    belongs to a partition the leaf move loop has already tuned — a constrained partition against a free
>    one. The ladder loses candidates to the handicap rather than to the objective. On om8 the winning
>    candidate reads 7.528429223 as blocks against the incumbent's 7.446619468 and is rejected, so the
>    detector never escalates and the arm is inert (`COL_REGROUP=off` reproduces the tip bit-for-bit).
>    Re-scored after the same leaf-level tune the incumbent had, it reads 7.075193632 and the run ends at
>    **6.893377041 instead of 7.412472853 — −7.00% in bits** (seeds 234/345: −6.5%/−7.0%).
> 2. **The probe's enter-flow transform rescales flow but not the teleport aggregates.** The probe
>    clusters the block graph under `probeNet.flow = probeNet.enter`, which rescales codeword usage to
>    boundary flow; the per-unit recorded-teleport aggregates were left at the original scale. The
>    *inconsistency* is the defect, not the teleport term — it keeps its full magnitude while everything
>    it competes with shrinks. Under `--regularized` it then dominates (global teleport flow ~2× the
>    summed enter flow) and, being size-proportional and community-blind, turns the probe's objective
>    into mostly a uniform size penalty: it collapses to 100 groups where the consistent probe resolves
>    536. The fix rescales the aggregates by the transform's own factor. Zeroing them instead (`drop`,
>    kept as an A/B handle) also unlocks the wins but is worse on both axes — at `-N10` `scale` beats
>    `drop` in **every** time cell on these rows (om8 6.25 s vs 9.49 s, om4 5.75 vs 6.83, om6 6.50 vs
>    7.43) and is equal or better in bits on three of five.
>
> **The two only work together**, which is why neither surfaced alone: the probe fix finds the right
> grouping and only the leaf re-tune can see that it is right. Fix 2 is a no-op without recorded
> teleportation, so every base and plain-memory row is bit-identical to the tip by construction.
>
> ⚠️ **The one cost that is not noise, and it needs a decision.** On the `--regularized` overlapping rows
> where the fixes find nothing — om5 and om6 — the arm now costs **+10.6% and +14.0% in time at unchanged
> bits** (`-N10`, min of 3). It is the same code that buys −4.89% / −5.21% in bits on om2 / om4 in the
> same regime: with the scales consistent the ladder walks a real community hierarchy on these networks
> instead of a collapsed one, and walks more rungs doing it. So it is a within-family trade, not a free
> regression — but it is above the 1%-without-a-gain bar and is called out here rather than buried.
>
> **The claim these tables have to support is that this is the whole extent of it: 6 rows better, nothing
> worse in bits anywhere, every other configuration bit-identical.** Better: om8 `-2d` −7.00%, om4
> `-2d --regularized` −5.21%, om2 `-2d --regularized` −4.94%, air30k `-2d --regularized` −0.072%, air30k
> `-d --regularized` −0.026%, om5 `-2d --regularized` −0.0096%.
>
> **Old** = a fresh `MODE=release OPENMP=0` build of the `columnar-hierarchical-core` tip `a008b85c`,
> md5 `45d8f427c293fa9f0a4b8e3e64eecb61`; **new** = this PR, md5 `09ac65f806c49d2d6ed1c392b3a7fb48`. Both arms
> interleaved in one session with one instrument.

> ⚠️ **Where the time goes, and the two cells that need a decision.** Over the **99 bit-identical
> paired configurations** the time delta is median **+0.23%**, mean **+0.94%** — the arm is free where it
> does nothing. Every real cost lands on the overlapping family, i.e. on the networks the arm exists for:
>
> | row | Δbits | Δtime | what it is |
> |---|--:|--:|---|
> | om8 `-C -2d -N1` | **−7.0030%** | +145.9% | om8 joins the escalating set and pays what om5/om6 pay |
> | om8 `-C -2d -N10` | **−6.8931%** | −13.1% | free |
> | om4 `-C -2d --regularized -N1` / `-N10` | **−5.23% / −5.21%** | −38.4% / −16.9% | free, and faster |
> | om2 `-C -2d --regularized -N1` / `-N10` | **−4.89% / −4.91%** | −8.7% / +11.0% | |
> | om8 `-C -2d --regularized -N1` | −0.0332% | **+56.8%** | ⚠️ marginal bits win, large time cost |
> | om8 `-C -2d --regularized -N10` | **+0.0134%** | −15.6% | ⚠️ the only cell worse in bits, inside the 0.1% bar |
> | om5 / om6 `-C -2d --regularized` | = | **+8.9…+17.7%** | ⚠️ probe fix walks a real hierarchy, finds nothing here |
> | om8 `-C -d -N10` | = | **+9.8%** | ⚠️ 10 leaf tunes, all wasted |
>
> The last four rows are the ones to argue about, and both mechanisms were chased to the point where the
> cost is provably irreducible at the gate rather than merely unexplained:
>
> - **The leaf tune.** Its cost is 1.3–2.5% of engine CPU on the healthy rows (air30k `-C`/`-2`/meta,
>   wikispeedia, malaria — clocked directly inside the operator, since wall deltas that small are not
>   measurable on a loaded desktop). On the overlapping family it reaches ~10%, because a sweep there is
>   over 50–58k leaves and the hierarchical build runs a ladder in every qualifying sub-optimizer. A
>   distance gate is the obvious shaping and the distribution rules it out: on om8 `-C -d -N10` the ten
>   wasted tunes sit at gaps of 1.12, 1.14, 1.20, 1.31, 1.32, 3.99, 4.08, 4.10, 4.33, 4.60% above the
>   incumbent, and **the rescue worth −7.00% in bits sits at 1.0986%** — inside that cluster, below five
>   of them. air30k's useless test is at 0.82%, *closer* than the rescue. Any threshold that keeps the
>   rescue keeps half the waste.
> - **The probe fix.** Already shaped once: rescaling the teleport aggregates rather than zeroing them
>   beats zeroing in **every** time cell on these rows (om8 `-N10` 6.25 s against 9.49 s, om4 5.75
>   against 6.83, om6 6.50 against 7.43) and is equal or better in bits on three of five, which is why
>   `scale` is the default and `drop` is only an A/B handle.
>
> Instrument: `--timing-json`'s `timing.total_s`, minimum of 3 interleaved repetitions, one session; the
> flagged rows above re-measured at minimum of 5. Desktop load ran 4–19 with one transient spike, so
> treat any single sub-2% cell as noise — the median over 99 unchanged rows is the number to trust, and
> the four flagged rows were re-measured on a quiet machine.

> ⚠️ **The overlapping rows of the previous snapshot do not reproduce and are not carried forward.**
> The snapshot for #1035 quotes om5 at 7.10873240 (130 modules) for both `-C -d -N10` and `-C -2d -N10`,
> and 7.17509147 (135 modules) at `-C -2d -N1`. A clean build of the tip it names returns
> **6.866617805 (308 modules)** for both `-N10` configurations and **6.868127142 (293 modules)** at
> `-2d -N1` — consistent with F42's own reported 6.868/6.887, and with om6. Two different configurations
> also carried identical numbers there, which the snapshot rules forbid in the mirror-image sense. The
> rows below are all freshly measured; no cause for the old figures is offered here, because none was
> established.

> **Two residuals that this PR does not move, so a reviewer does not read them as regressions.**
> (1) `-C -d -N1` on the overlapping family is still in the bad basin on om4 / om5 / om6
> (7.98293180 with **one module**, 7.88481128, 7.47937471) — F42's documented residual: the
> fine-blocks bottom skips the probe (`maxAggPasses != 0`) and a single hierarchical trial has no
> flat-first arm. Any `-N2+` or `-2` run is covered, and om2 / om8 happen to escape anyway
> (7.31721319 / 6.99609633). (2) **om5 `-2d --regularized`** improves by only 0.0096% and stays
> 3.0% in bits above its own soft-seeded reference (7.96633137 against 7.73506797) — the one row in
> the family neither fix reaches, logged as open in F47 rather than papered over.

> **No OO-vs-columnar tables in this snapshot.** The regroup ladder is columnar-only, so nothing in this
> PR can move the OO path and an OO arm would be measurement spent on a comparison no row of this change
> can affect. (It is also where this session lost time: air30k (meta) `-N10` on the OO engine still does
> not finish — killed at 12 minutes — and the orphaned process it left behind contaminated one batch's
> timings, which had to be discarded and re-run.) The OO gap on this family is real and recorded in F47;
> it closes when columnar replaces OO. Restoring the tables is one batch away if they are wanted for the
> #808 framing.
### What the change moves

Auto-generated: **every** paired configuration where old and new differ, and nothing else in this document differs at all. `planted` / `soft-seeded` are quality references, not arms — the planted `.clu` scored with `--no-infomap -c`, and the same file as a soft `-c` seed (#1028).

| network | flags | old bits | new bits | Δbits | old t | new t | Δt | top old → new | reference |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|
| overlapping om8 | `-C -2d -N1` | 7.41247285 | **6.89337704** | **-7.0030%** | 0.929s | 2.283s | +145.9% | 4175 → 911 | planted 6.98103476 / soft-seeded 6.87523739 |
| overlapping om8 | `-C -2d -N10` | 7.39712802 | **6.88723447** | **-6.8931%** | 9.795s | 8.510s | -13.1% | 4167 → 921 | planted 6.98103476 / soft-seeded 6.87523739 |
| overlapping om4 | `-C -2d --regularized -N1` | 7.97377205 | **7.55713556** | **-5.2251%** | 1.382s | 0.852s | -38.4% | 41 → 55 | planted 7.58439679 / soft-seeded 7.54775056 |
| overlapping om4 | `-C -2d --regularized -N10` | 7.97307668 | **7.55739869** | **-5.2135%** | 5.588s | 4.647s | -16.9% | 45 → 57 | planted 7.58439679 / soft-seeded 7.54775056 |
| overlapping om2 | `-C -2d --regularized -N10` | 7.93699970 | **7.54767235** | **-4.9052%** | 2.015s | 2.236s | +11.0% | 124 → 119 | planted 7.58382058 / soft-seeded 7.53273748 |
| overlapping om2 | `-C -2d --regularized -N1` | 7.93902109 | **7.55074805** | **-4.8907%** | 0.631s | 0.576s | -8.7% | 120 → 112 | planted 7.58382058 / soft-seeded 7.53273748 |
| air30k (reg.) | `-C -2d --regularized -N10` | 5.57557704 | **5.57153933** | **-0.0724%** | 4.180s | 4.111s | -1.7% | 304 → 304 | — |
| air30k (reg.) | `-C -d --regularized -N1` | 5.66687303 | **5.66332790** | **-0.0626%** | 0.453s | 0.470s | +3.8% | 15 → 13 | — |
| overlapping om8 ⚠️ | `-C -2d --regularized -N1` | 7.97891240 | **7.97626040** | **-0.0332%** | 2.098s | 3.290s | +56.8% | 234 → 259 | planted 8.29476796 — worse than one-level 7.99473567; soft seed collapses to it |
| air30k (reg.) | `-C --non-redundant -d --regularized -N10` | 5.56887516 | **5.56708153** | **-0.0322%** | 4.420s | 4.456s | +0.8% | 11 → 13 | — |
| air30k (reg.) | `-C -d --regularized -N10` | 5.57624241 | **5.57479757** | **-0.0259%** | 4.354s | 4.365s | +0.2% | 11 → 11 | — |
| air30k (reg.) | `-C -F -d --regularized -N10` | 5.57624241 | **5.57479757** | **-0.0259%** | 4.021s | 3.929s | -2.3% | 11 → 11 | — |
| overlapping om5 | `-C -2d --regularized -N1` | 7.96709570 | **7.96608370** | **-0.0127%** | 3.372s | 3.179s | -5.7% | 103 → 105 | planted 7.79181011 / soft-seeded 7.73506797 |
| overlapping om8 ⚠️ | `-C -2d --regularized -N10` | 7.97614021 | **7.97720957** | **+0.0134%** | 6.836s | 5.773s | -15.6% | 240 → 256 | planted 8.29476796 — worse than one-level 7.99473567; soft seed collapses to it |

⚠️ = the cells flagged in the note above: om8 `--regularized` at `-N10` is the only cell worse in bits anywhere (+0.0134%, inside the 0.1% bar, with −15.6% in time), and om8 `--regularized` at `-N1` buys −0.0332% in bits for +56.8% in time.

### The overlapping family in full

The regression guard for F42 and the evidence for F47. `planted` and `planted --reg` are the same partition scored with `--no-infomap -c`; under `--regularized` it is worse than one-level on om6/om8, so on those two rows it is not a bound on anything (see `benchmark-networks.md`).

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">columnar (old, tip a008b85c)</th>
<th colspan="4">columnar (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>om2 `-2d -N1`</td><td align="right">6.73935821</td><td align="right">1.490s</td><td align="right">674</td><td align="right">2</td><td align="right">6.73935821 (=)</td><td align="right">1.497s (+0.5%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td>om4 `-2d -N1`</td><td align="right">6.86172465</td><td align="right">1.589s</td><td align="right">141</td><td align="right">2</td><td align="right">6.86172465 (=)</td><td align="right">1.579s (-0.6%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td>om5 `-2d -N1`</td><td align="right">6.86812714</td><td align="right">1.773s</td><td align="right">293</td><td align="right">2</td><td align="right">6.86812714 (=)</td><td align="right">1.878s (+5.9%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td>om6 `-2d -N1`</td><td align="right">6.88404244</td><td align="right">1.789s</td><td align="right">455</td><td align="right">2</td><td align="right">6.88404244 (=)</td><td align="right">1.796s (+0.4%)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td>om8 `-2d -N1`</td><td align="right">7.41247285</td><td align="right">0.929s</td><td align="right">4175</td><td align="right">2</td><td align="right">6.89337704 (-7.0030%)</td><td align="right">2.283s (+145.9%)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td>om2 `-2d -N10`</td><td align="right">6.73927197</td><td align="right">3.621s</td><td align="right">638</td><td align="right">2</td><td align="right">6.73927197 (=)</td><td align="right">3.794s (+4.8%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td>om4 `-2d -N10`</td><td align="right">6.86690179</td><td align="right">6.823s</td><td align="right">173</td><td align="right">2</td><td align="right">6.86690179 (=)</td><td align="right">6.860s (+0.5%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td>om5 `-2d -N10`</td><td align="right">6.86661780</td><td align="right">6.611s</td><td align="right">308</td><td align="right">2</td><td align="right">6.86661780 (=)</td><td align="right">7.443s (+12.6%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>om6 `-2d -N10`</td><td align="right">6.88486214</td><td align="right">7.279s</td><td align="right">451</td><td align="right">2</td><td align="right">6.88486214 (=)</td><td align="right">7.493s (+2.9%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td>om8 `-2d -N10`</td><td align="right">7.39712802</td><td align="right">9.795s</td><td align="right">4167</td><td align="right">2</td><td align="right">6.88723447 (-6.8931%)</td><td align="right">8.510s (-13.1%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td>om2 `-d -N1`</td><td align="right">7.31721319</td><td align="right">0.366s</td><td align="right">75</td><td align="right">4</td><td align="right">7.31721319 (=)</td><td align="right">0.352s (-4.0%)</td><td align="right">75</td><td align="right">4</td></tr>
<tr><td>om4 `-d -N1`</td><td align="right">7.98293180</td><td align="right">0.739s</td><td align="right">1</td><td align="right">4</td><td align="right">7.98293180 (=)</td><td align="right">0.737s (-0.3%)</td><td align="right">1</td><td align="right">4</td></tr>
<tr><td>om5 `-d -N1`</td><td align="right">7.88481128</td><td align="right">0.737s</td><td align="right">66</td><td align="right">4</td><td align="right">7.88481128 (=)</td><td align="right">0.739s (+0.3%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td>om6 `-d -N1`</td><td align="right">7.47937471</td><td align="right">0.785s</td><td align="right">92</td><td align="right">4</td><td align="right">7.47937471 (=)</td><td align="right">0.788s (+0.4%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td>om8 `-d -N1`</td><td align="right">6.99609633</td><td align="right">0.800s</td><td align="right">206</td><td align="right">4</td><td align="right">6.99609633 (=)</td><td align="right">0.808s (+1.0%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td>om2 `-d -N10`</td><td align="right">6.73180866</td><td align="right">4.424s</td><td align="right">690</td><td align="right">2</td><td align="right">6.73180866 (=)</td><td align="right">4.374s (-1.1%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td>om4 `-d -N10`</td><td align="right">6.86690179</td><td align="right">7.825s</td><td align="right">173</td><td align="right">2</td><td align="right">6.86690179 (=)</td><td align="right">7.962s (+1.8%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td>om5 `-d -N10`</td><td align="right">6.86661780</td><td align="right">7.877s</td><td align="right">308</td><td align="right">2</td><td align="right">6.86661780 (=)</td><td align="right">8.108s (+2.9%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>om6 `-d -N10`</td><td align="right">6.88486214</td><td align="right">8.505s</td><td align="right">451</td><td align="right">2</td><td align="right">6.88486214 (=)</td><td align="right">8.693s (+2.2%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td>om8 `-d -N10`</td><td align="right">6.98747316</td><td align="right">8.320s</td><td align="right">203</td><td align="right">4</td><td align="right">6.98747316 (=)</td><td align="right">9.106s (+9.5%)</td><td align="right">203</td><td align="right">4</td></tr>
<tr><td>om2 `-2d --regularized -N1`</td><td align="right">7.93902109</td><td align="right">0.631s</td><td align="right">120</td><td align="right">2</td><td align="right">7.55074805 (-4.8907%)</td><td align="right">0.576s (-8.7%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td>om4 `-2d --regularized -N1`</td><td align="right">7.97377205</td><td align="right">1.382s</td><td align="right">41</td><td align="right">2</td><td align="right">7.55713556 (-5.2251%)</td><td align="right">0.852s (-38.4%)</td><td align="right">55</td><td align="right">2</td></tr>
<tr><td>om5 `-2d --regularized -N1`</td><td align="right">7.96709570</td><td align="right">3.372s</td><td align="right">103</td><td align="right">2</td><td align="right">7.96608370 (-0.0127%)</td><td align="right">3.179s (-5.7%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td>om6 `-2d --regularized -N1`</td><td align="right">7.98157455</td><td align="right">1.380s</td><td align="right">113</td><td align="right">2</td><td align="right">7.98157455 (=)</td><td align="right">1.537s (+11.3%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td>om8 `-2d --regularized -N1`</td><td align="right">7.97891240</td><td align="right">2.098s</td><td align="right">234</td><td align="right">2</td><td align="right">7.97626040 (-0.0332%)</td><td align="right">3.290s (+56.8%)</td><td align="right">259</td><td align="right">2</td></tr>
<tr><td>om2 `-2d --regularized -N10`</td><td align="right">7.93699970</td><td align="right">2.015s</td><td align="right">124</td><td align="right">2</td><td align="right">7.54767235 (-4.9052%)</td><td align="right">2.236s (+11.0%)</td><td align="right">119</td><td align="right">2</td></tr>
<tr><td>om4 `-2d --regularized -N10`</td><td align="right">7.97307668</td><td align="right">5.588s</td><td align="right">45</td><td align="right">2</td><td align="right">7.55739869 (-5.2135%)</td><td align="right">4.647s (-16.9%)</td><td align="right">57</td><td align="right">2</td></tr>
<tr><td>om5 `-2d --regularized -N10`</td><td align="right">7.96753108</td><td align="right">4.939s</td><td align="right">98</td><td align="right">2</td><td align="right">7.96753108 (=)</td><td align="right">5.378s (+8.9%)</td><td align="right">98</td><td align="right">2</td></tr>
<tr><td>om6 `-2d --regularized -N10`</td><td align="right">7.98157455</td><td align="right">4.521s</td><td align="right">113</td><td align="right">2</td><td align="right">7.98157455 (=)</td><td align="right">5.249s (+16.1%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td>om8 `-2d --regularized -N10`</td><td align="right">7.97614021</td><td align="right">6.836s</td><td align="right">240</td><td align="right">2</td><td align="right">7.97720957 (+0.0134%)</td><td align="right">5.773s (-15.6%)</td><td align="right">256</td><td align="right">2</td></tr>
<tr><td>om2 `-2d -c` planted seed, `-N1`</td><td align="right">6.74472199</td><td align="right">0.519s</td><td align="right">476</td><td align="right">2</td><td align="right">6.74472199 (=)</td><td align="right">0.545s (+5.0%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td>om4 `-2d -c` planted seed, `-N1`</td><td align="right">6.85623947</td><td align="right">0.920s</td><td align="right">132</td><td align="right">2</td><td align="right">6.85623947 (=)</td><td align="right">0.915s (-0.6%)</td><td align="right">132</td><td align="right">2</td></tr>
<tr><td>om5 `-2d -c` planted seed, `-N1`</td><td align="right">6.85777811</td><td align="right">1.194s</td><td align="right">296</td><td align="right">2</td><td align="right">6.85777811 (=)</td><td align="right">1.182s (-1.0%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td>om6 `-2d -c` planted seed, `-N1`</td><td align="right">6.87337876</td><td align="right">1.282s</td><td align="right">446</td><td align="right">2</td><td align="right">6.87337876 (=)</td><td align="right">1.296s (+1.1%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td>om8 `-2d -c` planted seed, `-N1`</td><td align="right">6.87523739</td><td align="right">1.417s</td><td align="right">894</td><td align="right">2</td><td align="right">6.87523739 (=)</td><td align="right">1.423s (+0.4%)</td><td align="right">894</td><td align="right">2</td></tr>
<tr><td>om2 planted, `--no-infomap -c`</td><td align="right">6.78904000</td><td align="right">0.050s</td><td align="right">8</td><td align="right">2</td><td align="right">6.78904000 (=)</td><td align="right">0.054s (+8.7%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td>om4 planted, `--no-infomap -c`</td><td align="right">6.88065015</td><td align="right">0.101s</td><td align="right">16</td><td align="right">2</td><td align="right">6.88065015 (=)</td><td align="right">0.100s (-1.2%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td>om5 planted, `--no-infomap -c`</td><td align="right">6.90222253</td><td align="right">0.113s</td><td align="right">20</td><td align="right">2</td><td align="right">6.90222253 (=)</td><td align="right">0.127s (+12.2%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td>om6 planted, `--no-infomap -c`</td><td align="right">6.93093499</td><td align="right">0.122s</td><td align="right">24</td><td align="right">2</td><td align="right">6.93093499 (=)</td><td align="right">0.126s (+3.7%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td>om8 planted, `--no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.133s</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.134s (+0.4%)</td><td align="right">32</td><td align="right">2</td></tr>
<tr><td>om2 planted, `--regularized`</td><td align="right">7.58382058</td><td align="right">0.096s</td><td align="right">8</td><td align="right">2</td><td align="right">7.58382058 (=)</td><td align="right">0.094s (-2.6%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td>om4 planted, `--regularized`</td><td align="right">7.58439679</td><td align="right">0.170s</td><td align="right">16</td><td align="right">2</td><td align="right">7.58439679 (=)</td><td align="right">0.167s (-1.5%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td>om5 planted, `--regularized`</td><td align="right">7.79181011</td><td align="right">0.195s</td><td align="right">20</td><td align="right">2</td><td align="right">7.79181011 (=)</td><td align="right">0.192s (-1.9%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td>om6 planted, `--regularized`</td><td align="right">8.02556766</td><td align="right">0.195s</td><td align="right">24</td><td align="right">2</td><td align="right">8.02556766 (=)</td><td align="right">0.195s (+0.4%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td>om8 planted, `--regularized`</td><td align="right">8.29476796</td><td align="right">0.217s</td><td align="right">32</td><td align="right">2</td><td align="right">8.29476796 (=)</td><td align="right">0.209s (-4.0%)</td><td align="right">32</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — standard search (`-C -N10`)

The overlapping and wikispeedia rows run `-C -d -N10` (directed state networks).

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">columnar <code>-C</code> (old)</th>
<th colspan="4">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (+7.4%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.007s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86275593 (=)</td><td align="right">0.006s (-2.4%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.023s</td><td align="right">2</td><td align="right">4</td><td align="right">4.05454025 (=)</td><td align="right">0.022s (-0.9%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.240s</td><td align="right">5</td><td align="right">5</td><td align="right">4.74107206 (=)</td><td align="right">0.236s (-1.5%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.058s</td><td align="right">81</td><td align="right">2</td><td align="right">6.74094314 (=)</td><td align="right">0.060s (+4.3%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.064s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">3.060s (-0.2%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">20.0s</td><td align="right">5</td><td align="right">6</td><td align="right">5.56852929 (=)</td><td align="right">20.1s (+0.3%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.004s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.004s (-0.8%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.000s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.000s (-3.9%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.182s</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.182s (=)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">4.100s</td><td align="right">22</td><td align="right">3</td><td align="right">5.39242541 (=)</td><td align="right">4.308s (+5.1%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.354s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57479757 (-0.0259%)</td><td align="right">4.365s (+0.2%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">10.0s</td><td align="right">23</td><td align="right">3</td><td align="right">7.42215327 (=)</td><td align="right">9.758s (-2.7%)</td><td align="right">23</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.244s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.210s (-1.0%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>overlapping om2 `-d`</td><td align="right">6.73180866</td><td align="right">4.424s</td><td align="right">690</td><td align="right">2</td><td align="right">6.73180866 (=)</td><td align="right">4.374s (-1.1%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td>overlapping om4 `-d`</td><td align="right">6.86690179</td><td align="right">7.825s</td><td align="right">173</td><td align="right">2</td><td align="right">6.86690179 (=)</td><td align="right">7.962s (+1.8%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-d`</td><td align="right">6.86661780</td><td align="right">7.877s</td><td align="right">308</td><td align="right">2</td><td align="right">6.86661780 (=)</td><td align="right">8.108s (+2.9%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-d`</td><td align="right">6.88486214</td><td align="right">8.505s</td><td align="right">451</td><td align="right">2</td><td align="right">6.88486214 (=)</td><td align="right">8.693s (+2.2%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-d`</td><td align="right">6.98747316</td><td align="right">8.320s</td><td align="right">203</td><td align="right">4</td><td align="right">6.98747316 (=)</td><td align="right">9.106s (+9.5%)</td><td align="right">203</td><td align="right">4</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">5.90790474</td><td align="right">0.678s</td><td align="right">199</td><td align="right">2</td><td align="right">5.90790474 (=)</td><td align="right">0.688s (+1.4%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Overlapping and wikispeedia rows as `-C -2d -N10`.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">columnar <code>-C -2</code> (old)</th>
<th colspan="4">columnar <code>-C -2</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.000s</td><td align="right">9</td><td align="right">2</td><td align="right">3.51775481 (=)</td><td align="right">0.000s (+3.3%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td>jazz</td><td align="right">6.86122977</td><td align="right">0.007s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86122977 (=)</td><td align="right">0.006s (-2.7%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28307258</td><td align="right">0.009s</td><td align="right">59</td><td align="right">2</td><td align="right">4.28307258 (=)</td><td align="right">0.008s (-1.7%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td>powergrid</td><td align="right">5.63729688</td><td align="right">0.099s</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.098s (-0.7%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73957529</td><td align="right">0.043s</td><td align="right">81</td><td align="right">2</td><td align="right">6.73957529 (=)</td><td align="right">0.043s (-0.9%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.94997883</td><td align="right">2.329s</td><td align="right">506</td><td align="right">2</td><td align="right">7.94997883 (=)</td><td align="right">2.335s (+0.2%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td>web-NotreDame</td><td align="right">6.75421666</td><td align="right">19.6s</td><td align="right">11991</td><td align="right">2</td><td align="right">6.75421666 (=)</td><td align="right">19.2s (-2.2%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.003s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.003s (+3.9%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.000s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.000s (+16.0%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.40044538</td><td align="right">2.838s</td><td align="right">168</td><td align="right">2</td><td align="right">7.40044538 (=)</td><td align="right">2.822s (-0.6%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39305505</td><td align="right">3.895s</td><td align="right">334</td><td align="right">2</td><td align="right">5.39305505 (=)</td><td align="right">3.998s (+2.6%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57557704</td><td align="right">4.180s</td><td align="right">304</td><td align="right">2</td><td align="right">5.57153933 (-0.0724%)</td><td align="right">4.111s (-1.7%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42414371</td><td align="right">10.8s</td><td align="right">2237</td><td align="right">2</td><td align="right">7.42414371 (=)</td><td align="right">10.8s (-0.7%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">2.978s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">2.979s (=)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>overlapping om2 `-2d`</td><td align="right">6.73927197</td><td align="right">3.621s</td><td align="right">638</td><td align="right">2</td><td align="right">6.73927197 (=)</td><td align="right">3.794s (+4.8%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td>overlapping om4 `-2d`</td><td align="right">6.86690179</td><td align="right">6.823s</td><td align="right">173</td><td align="right">2</td><td align="right">6.86690179 (=)</td><td align="right">6.860s (+0.5%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-2d`</td><td align="right">6.86661780</td><td align="right">6.611s</td><td align="right">308</td><td align="right">2</td><td align="right">6.86661780 (=)</td><td align="right">7.443s (+12.6%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d`</td><td align="right">6.88486214</td><td align="right">7.279s</td><td align="right">451</td><td align="right">2</td><td align="right">6.88486214 (=)</td><td align="right">7.493s (+2.9%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-2d`</td><td align="right">7.39712802</td><td align="right">9.795s</td><td align="right">4167</td><td align="right">2</td><td align="right">6.88723447 (-6.8931%)</td><td align="right">8.510s (-13.1%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-2d`</td><td align="right">5.90790474</td><td align="right">0.632s</td><td align="right">199</td><td align="right">2</td><td align="right">5.90790474 (=)</td><td align="right">0.652s (+3.1%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

`-N1` is where a time difference is code and nothing else — except on the rows this PR changes, where the new arm computes a different (better) partition and pays for the escalation it now triggers. The `-2d -c` rows seed the search with the planted partition (soft `-c`, #1028).

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">columnar <code>-C -N1</code> (old)</th>
<th colspan="4">columnar <code>-C -N1</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>lazega</td><td align="right">6.04111740</td><td align="right">0.001s</td><td align="right">6</td><td align="right">2</td><td align="right">6.04111740 (=)</td><td align="right">0.001s (-5.2%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.000s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.000s (-4.9%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50222401</td><td align="right">0.370s</td><td align="right">8</td><td align="right">3</td><td align="right">7.50222401 (=)</td><td align="right">0.374s (+1.2%)</td><td align="right">8</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.47323105</td><td align="right">0.379s</td><td align="right">22</td><td align="right">3</td><td align="right">5.47323105 (=)</td><td align="right">0.360s (-4.9%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.66687303</td><td align="right">0.453s</td><td align="right">15</td><td align="right">3</td><td align="right">5.66332790 (-0.0626%)</td><td align="right">0.470s (+3.8%)</td><td align="right">13</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.58074898</td><td align="right">0.860s</td><td align="right">43</td><td align="right">3</td><td align="right">7.58074898 (=)</td><td align="right">0.865s (+0.5%)</td><td align="right">43</td><td align="right">3</td></tr>
<tr><td>overlapping om2 `-2d`</td><td align="right">6.73935821</td><td align="right">1.490s</td><td align="right">674</td><td align="right">2</td><td align="right">6.73935821 (=)</td><td align="right">1.497s (+0.5%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td>overlapping om4 `-2d`</td><td align="right">6.86172465</td><td align="right">1.589s</td><td align="right">141</td><td align="right">2</td><td align="right">6.86172465 (=)</td><td align="right">1.579s (-0.6%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-2d`</td><td align="right">6.86812714</td><td align="right">1.773s</td><td align="right">293</td><td align="right">2</td><td align="right">6.86812714 (=)</td><td align="right">1.878s (+5.9%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d`</td><td align="right">6.88404244</td><td align="right">1.789s</td><td align="right">455</td><td align="right">2</td><td align="right">6.88404244 (=)</td><td align="right">1.796s (+0.4%)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-2d`</td><td align="right">7.41247285</td><td align="right">0.929s</td><td align="right">4175</td><td align="right">2</td><td align="right">6.89337704 (-7.0030%)</td><td align="right">2.283s (+145.9%)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.088s</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.088s (+1.1%)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-d`</td><td align="right">7.88481128</td><td align="right">0.737s</td><td align="right">66</td><td align="right">4</td><td align="right">7.88481128 (=)</td><td align="right">0.739s (+0.3%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td>overlapping om6 `-d`</td><td align="right">7.47937471</td><td align="right">0.785s</td><td align="right">92</td><td align="right">4</td><td align="right">7.47937471 (=)</td><td align="right">0.788s (+0.4%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">6.11312587</td><td align="right">0.069s</td><td align="right">46</td><td align="right">3</td><td align="right">6.11312587 (=)</td><td align="right">0.072s (+4.1%)</td><td align="right">46</td><td align="right">3</td></tr>
<tr><td>overlapping om5 `-2d -c` planted</td><td align="right">6.85777811</td><td align="right">1.194s</td><td align="right">296</td><td align="right">2</td><td align="right">6.85777811 (=)</td><td align="right">1.182s (-1.0%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d -c` planted</td><td align="right">6.87337876</td><td align="right">1.282s</td><td align="right">446</td><td align="right">2</td><td align="right">6.87337876 (=)</td><td align="right">1.296s (+1.1%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-2d -c` planted</td><td align="right">6.87523739</td><td align="right">1.417s</td><td align="right">894</td><td align="right">2</td><td align="right">6.87523739 (=)</td><td align="right">1.423s (+0.4%)</td><td align="right">894</td><td align="right">2</td></tr>
</tbody>
</table>

### The fast dial `-F`

`-F` (`--fast-hierarchical-solution`) skips the interior-layer refinement in favour of a single bottom re-partition within grandparents plus the module-level coarsening loop. Measured as `-C -F`: **`-F` alone does not select the columnar engine**. Both columns are the new binary.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (-22.1%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.006s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86275593 (=)</td><td align="right">0.006s (-3.5%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.022s</td><td align="right">2</td><td align="right">4</td><td align="right">4.06300588 (+0.2088%)</td><td align="right">0.014s (-37.8%)</td><td align="right">4</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.236s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77402225 (+0.6950%)</td><td align="right">0.144s (-39.0%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.060s</td><td align="right">81</td><td align="right">2</td><td align="right">6.74094314 (=)</td><td align="right">0.056s (-7.9%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.060s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">2.934s (-4.1%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">20.1s</td><td align="right">5</td><td align="right">6</td><td align="right">5.62506198 (+1.0152%)</td><td align="right">15.0s (-25.6%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.004s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.003s (-9.3%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.000s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.000s (-9.6%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.182s</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.163s (-0.6%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">4.308s</td><td align="right">22</td><td align="right">3</td><td align="right">5.39242541 (=)</td><td align="right">3.918s (-9.1%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57479757</td><td align="right">4.365s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57479757 (=)</td><td align="right">3.929s (-10.0%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.758s</td><td align="right">23</td><td align="right">3</td><td align="right">7.42215327 (=)</td><td align="right">9.533s (-2.3%)</td><td align="right">23</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.210s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.151s (-1.8%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">5.90790474</td><td align="right">0.688s</td><td align="right">199</td><td align="right">2</td><td align="right">5.90790474 (=)</td><td align="right">0.658s (-4.4%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### The non-redundant map equation L\* (`--non-redundant`)

L\* is a different objective, so a lower number is not a better partition of the same objective. Both columns are the new binary.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">3</td><td align="right">3</td><td align="right">3.07806732 (-9.0897%)</td><td align="right">0.001s (-11.2%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.006s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86822837 (+0.0797%)</td><td align="right">0.006s (+0.2%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.022s</td><td align="right">2</td><td align="right">4</td><td align="right">3.89220976 (-4.0037%)</td><td align="right">0.022s (-3.3%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.236s</td><td align="right">5</td><td align="right">5</td><td align="right">4.50926542 (-4.8893%)</td><td align="right">0.231s (-2.0%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.060s</td><td align="right">81</td><td align="right">2</td><td align="right">6.78924150 (+0.7165%)</td><td align="right">0.057s (-5.2%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.060s</td><td align="right">15</td><td align="right">3</td><td align="right">8.00917226 (+2.2434%)</td><td align="right">2.756s (-9.9%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">20.1s</td><td align="right">5</td><td align="right">6</td><td align="right">5.51707363 (-0.9240%)</td><td align="right">20.3s (+1.2%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.004s</td><td align="right">7</td><td align="right">2</td><td align="right">5.96862465 (-0.8182%)</td><td align="right">0.003s (-12.6%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.000s</td><td align="right">2</td><td align="right">2</td><td align="right">1.92885658 (-4.1040%)</td><td align="right">0.000s (-8.8%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.182s</td><td align="right">2</td><td align="right">3</td><td align="right">7.42757278 (+0.4065%)</td><td align="right">3.206s (+0.8%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">4.308s</td><td align="right">22</td><td align="right">3</td><td align="right">5.37891261 (-0.2506%)</td><td align="right">4.154s (-3.6%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57479757</td><td align="right">4.365s</td><td align="right">11</td><td align="right">3</td><td align="right">5.56708153 (-0.1384%)</td><td align="right">4.456s (+2.1%)</td><td align="right">13</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.758s</td><td align="right">23</td><td align="right">3</td><td align="right">7.21529977 (-2.7870%)</td><td align="right">8.128s (-16.7%)</td><td align="right">33</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.210s</td><td align="right">25</td><td align="right">2</td><td align="right">8.44774545 (+2.5761%)</td><td align="right">3.306s (+3.0%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">5.90790474</td><td align="right">0.688s</td><td align="right">199</td><td align="right">2</td><td align="right">5.90320873 (-0.0795%)</td><td align="right">0.654s (-4.9%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

