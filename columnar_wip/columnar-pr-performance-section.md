## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`), taken from the *same execution* as the wall time and reported because it is load- and clock-independent. `time` is `--timing-json`'s `timing.total_s`: **one run per `-N10` row** (the codelength is deterministic and `instr` carries the comparison) and **interleaved minimum of 3 for `-N1` rows**.

> ⚠️ **Read the time column relatively, not absolutely, in this snapshot.** The whole sweep ran with an unrelated job holding ~5 of 10 cores (46% idle), so absolute times sit ~10–15% above the previous snapshot's on identical code — web-NotreDame `-C -N10` reads 21.1s here against 19.4s there, at an instr delta of −0.08%. Both arms are interleaved so the *deltas* survive, but several of them are visibly noise: web-NotreDame `-C -2 -N10` reads −11.9% in time against −0.24% in instructions. Where the two disagree, the instruction count is the measurement.

> **This PR ships two changes, measured together and apart.**
>
> **1. The optimizer's index rate ([#1038](https://github.com/mapequation/infomap/issues/1038)).** A unit's use rate of the index codebook is the rate at which a walker enters it — across a link *or* by teleporting into it from outside: `q = e + (T − t)·w`. `ColumnarLevel::enter` carries only `e`; every *scoring* site adds the teleport term itself, which is why scoring was always right. Six *optimizer* sites read `Level::enter` straight back as a node flow and never added it — the regroup probe, `buildHierarchyFromBottom`'s `superNet` **and the `curIndexCodelength` it is gated against**, `refineLayerWithinGrandparent` and `subClusterLevel`, `refineTopLayer`, and `splitLevelModules`' move base. So the objective used `q` while the search proposed and decided against `e`, a quantity **43.7% wrong on om5 and 72.0% on om8**. All six now go through one `setIndexRateAsFlow` / `unitIndexRate` pair, and the change is identically a no-op without recorded teleportation, where `q == e`.
>
> The derivation is confirmed by the OO core, independently of any measurement: `InfomapBase::transformNodeFlowToEnterFlow` sets `module.data.flow = module.data.enterFlow`, and `aggregateFlowValuesFromLeafToRoot` has already folded `(T − t)·w` into every **non-leaf** `enterFlow`. OO's super-network node flow *is* `q`, and its super-level gate compares against an `oldIndexLength` built from the same quantity. This is an OO-parity fix, not a new heuristic, and nothing on master needs changing.
>
> **2. The escalated ladder's leaf-granularity test is no longer restricted by its escalation source.** #1037 gated that test to trials whose detector rung was itself accepted on it (`tuneWonInDetector`). Trying to reproduce #1038's own claimed numbers failed, and that is what found the cause: those numbers were measured with the test *unrestricted*, and #1037 shipped the restriction afterwards. On om2 / om4 `-2d --regularized` the detector accepts its rung on the block score, so the flag stays false and the escalated ladder never runs the test that finds the partition ~5% better in bits underneath. The old behaviour stays reachable through `COL_REGROUP_LEAFTUNE_SCOPE=detector`.
>
> **Each change is necessary and only jointly sufficient** (one build, `--seed 123 -N1`):
>
> | | om2 | om4 | om5 | om6 | om8 |
> |---|--:|--:|--:|--:|--:|
> | tip 60378779 | 7.939021094 | 7.973772049 | 7.967095701 | 7.981574549 | 7.978912396 |
> | index rate `q` only | = | = | 7.966672583 | = | = |
> | leaf-tune scope only | 7.930556214 | = | 7.968281689 | = | 7.978296239 |
> | **both (this PR)** | **7.548816177** | **7.556894677** | 7.966995214 | = | = |
> | soft-seeded reference | 7.532737480 | 7.547750556 | 7.735067968 | 7.961829587 | 7.994735672 |
> | planted, `--no-infomap -c` | 7.583820576 | 7.584396786 | 7.791810113 | 8.025567656 | 8.294767960 |
>
> om2 and om4 end **below their planted partitions**. om6 and om8 are not failures on this row — under `--regularized` their planted partition scores *worse* than one-level — which leaves om5 as the family's last real regularized failure ([#1042](https://github.com/mapequation/infomap/issues/1042)).

> ⚠️ **Where it costs.** Over **121 paired configurations, 110 are bit-identical** at a median **+0.040% in instructions** (+0.92% in time) — the change is inert where it does nothing. Ten rows improve in bits, one is worse, and **twelve bit-identical rows pay 34–148% more instructions**. Every one of the twelve is an om `--regularized` row:
>
> | row | Δinstr | Δtime | Δbits |
> |---|--:|--:|--:|
> | om8 / om2 `-d --regularized -N1` | **+148.5% / +129.6%** | +80.4% / +109.0% | = |
> | om6 `-d --regularized` `-N10` / `-N1` | +115.6% / +114.6% | +38.2% / +72.9% | = |
> | om5 `-d --regularized` `-N10` / `-N1` | +98.2% / +89.4% | +58.1% / +42.7% | = |
> | om4 `-d --regularized -N1` | +89.2% | +40.9% | = |
> | om6 / om8 / om5 `-2d --regularized -N10` | +38.6% / +38.5% / +36.5% | +55.0% / +32.3% / +24.1% | = |
> | om8 / om6 `-2d --regularized -N1` | +37.9% / +34.9% | +29.1% / +23.6% | = |
> | om2 `-2d` plain (all four appearances) | +4.4…+4.7% | +1.4…+6.3% | = |
> | om6 `-2d` / `-d` plain | +1.1…+2.9% | −6.8…+4.9% | = |
>
> **The `-d --regularized` block is the ugly one** — om8 `-N1` goes 4.67G → 11.61G instructions for a bit-identical answer. It is *correct* work: with the fair gate the up-build keeps a super-level the broken gate discarded (3 levels instead of 2, reaching 9.008 where the tip reached 9.018). It is then thrown away in full, because on this family the regularized objective's optimum is the one-module partition and the one-level guard collapses the stack. A **sound** early exit exists — every deeper stack is bounded below by `stackL − indexTerm(k)`, by induction on `total(k+1) = total(k) − indexTerm(k) + superCodelength` — but it is far too loose to fire: om8 gives 9.018 − 4.873 = 4.145 against a 7.995 bound. Anything tighter is a heuristic and belongs to its own change, tracked in [#1041](https://github.com/mapequation/infomap/issues/1041) together with the wider finding that `-C -d` is 4–14% worse in bits than `-C -2d` across this whole family.
>
> At `-N10` the same block does pay: om2 and om4 `-d --regularized` gain **−4.84% and −5.22% in bits**, om4 at −21.7% in time.
>
> **The one row worse in bits is air30k (reg.) `-C -N1`: +0.0268% for +6.28% instructions** — single-trial path dependence inside a much larger pre-existing gap (OO reaches 5.591403536 on that row against the columnar 5.668390329). The same network at `-N10` moves the other way, −0.0268% (`-C`) and −0.0724% (`-C -2`), where the columnar arms already beat OO.
>
> **What the #1037 restriction actually bought** was ~4% of instructions on the six om2 / om6 plain rows above — and om4 `-2d --regularized` is 26% *cheaper* without it, because the run stops thrashing a basin it cannot leave. Every plain row is bit-identical with it removed, om8's 6.893377041 included.

> **Old** = the `columnar-hierarchical-core` tip `60378779`, md5 `82624b207d96b1c391d3c6237af8ca1a`; **new** = this PR, md5 `4b63cea29119bc7623e7a5385ccf6388`. Both arms interleaved in one session. `COL_TELE_INDEX_RATE=off COL_REGROUP_LEAFTUNE_SCOPE=detector` together reproduce the tip bit-for-bit.


### What the change moves

| network | table | old bits | new bits | Δbits | old t | new t | Δt | Δinstr | top old → new |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|
| om4 `-2d --regularized -N1` | `overlapping` | 7.97377205 | **7.55689468** | **-5.2281%** | 2.11s | 1.35s | (-36.1%) | (-26.35%) | 41 → 79 |
| om4 `-d --regularized -N10` | `overlapping` | 7.97307668 | **7.55665371** | **-5.2229%** | 13.39s | 10.49s | (-21.7%) | (+24.43%) | 45 → 78 |
| om4 `-2d --regularized -N10` | `overlapping` | 7.97307668 | **7.55689468** | **-5.2198%** | 7.48s | 6.74s | (-9.9%) | (-7.05%) | 45 → 79 |
| om2 `-2d --regularized -N1` | `overlapping` | 7.93902109 | **7.54881618** | **-4.9150%** | 0.827s | 1.01s | (+22.7%) | (+36.99%) | 120 → 120 |
| om2 `-2d --regularized -N10` | `overlapping` | 7.93699970 | **7.54872155** | **-4.8920%** | 2.95s | 3.12s | (+5.8%) | (+26.67%) | 124 → 126 |
| om2 `-d --regularized -N10` | `overlapping` | 7.93264485 | **7.54872155** | **-4.8398%** | 3.51s | 4.62s | (+31.6%) | (+67.76%) | 131 → 126 |
| air30k (reg.) | `twolevel` | 5.57557704 | **5.57153933** | **-0.0724%** | 4.27s | 4.27s | (-0.0%) | (-0.48%) | 304 → 304 |
| air30k (reg.) | `standard` | 5.57624241 | **5.57474682** | **-0.0268%** | 4.88s | 4.69s | (-3.8%) | (+1.26%) | 11 → 21 |
| om8 `-d --regularized -N10` | `overlapping` | 7.97781079 | **7.97668114** | **-0.0142%** | 6.00s | 9.88s | (+64.6%) | (+99.65%) | 232 → 256 |
| om5 `-2d --regularized -N1` | `overlapping` | 7.96709570 | **7.96699521** | **-0.0013%** | 4.34s | 3.57s | (-17.9%) | (-9.35%) | 103 → 104 |
| air30k (reg.) | `single` | 5.66687303 | **5.66839033** | **+0.0268%** | 0.521s | 0.553s | (+6.1%) | (+6.28%) | 15 → 20 |

### Old vs new columnar — standard search (`-C -N10`)

Overlapping and wikispeedia rows run `-C -d -N10`. One run per row: the codelength is deterministic and `instr` (instructions retired, from the same execution) is the load-independent comparison — read a sub-2% time delta against a ~0% instr delta as noise.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.002s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">None</td><td align="right">3.38583082 (=)</td><td align="right">0.002s (+15.2%)</td><td align="right">0.1G (-0.66%)</td><td align="right">3</td><td align="right">None</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.008s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">None</td><td align="right">6.86275593 (=)</td><td align="right">0.009s (+4.9%)</td><td align="right">0.1G (-0.16%)</td><td align="right">6</td><td align="right">None</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.027s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">None</td><td align="right">4.05454025 (=)</td><td align="right">0.027s (+1.0%)</td><td align="right">0.3G (-0.12%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.272s</td><td align="right">3.0G</td><td align="right">5</td><td align="right">None</td><td align="right">4.74107206 (=)</td><td align="right">0.274s (+1.0%)</td><td align="right">3.1G (+0.14%)</td><td align="right">5</td><td align="right">None</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.067s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">None</td><td align="right">6.74094314 (=)</td><td align="right">0.068s (+2.6%)</td><td align="right">0.7G (+0.01%)</td><td align="right">81</td><td align="right">None</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.37s</td><td align="right">34.2G</td><td align="right">15</td><td align="right">None</td><td align="right">7.83343660 (=)</td><td align="right">3.45s (+2.4%)</td><td align="right">34.2G (+0.04%)</td><td align="right">15</td><td align="right">None</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">22.99s</td><td align="right">189.0G</td><td align="right">5</td><td align="right">None</td><td align="right">5.56852929 (=)</td><td align="right">21.14s (-8.0%)</td><td align="right">188.9G (-0.08%)</td><td align="right">5</td><td align="right">None</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">None</td><td align="right">6.01786027 (=)</td><td align="right">0.006s (-3.5%)</td><td align="right">0.1G (-5.02%)</td><td align="right">7</td><td align="right">None</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">None</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (-6.7%)</td><td align="right">0.1G (+0.05%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.40s</td><td align="right">37.6G</td><td align="right">2</td><td align="right">None</td><td align="right">7.39750171 (=)</td><td align="right">3.40s (-0.2%)</td><td align="right">37.7G (+0.35%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">4.53s</td><td align="right">45.1G</td><td align="right">22</td><td align="right">None</td><td align="right">5.39242541 (=)</td><td align="right">4.14s (-8.6%)</td><td align="right">45.2G (+0.13%)</td><td align="right">22</td><td align="right">None</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.88s</td><td align="right">46.4G</td><td align="right">11</td><td align="right">None</td><td align="right">5.57474682 (-0.0268%)</td><td align="right">4.69s (-3.8%)</td><td align="right">47.0G (+1.26%)</td><td align="right">21</td><td align="right">None</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">10.73s</td><td align="right">98.9G</td><td align="right">23</td><td align="right">None</td><td align="right">7.42215327 (=)</td><td align="right">10.61s (-1.1%)</td><td align="right">98.9G (-0.01%)</td><td align="right">23</td><td align="right">None</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.64s</td><td align="right">35.8G</td><td align="right">25</td><td align="right">None</td><td align="right">8.23558553 (=)</td><td align="right">3.43s (-5.9%)</td><td align="right">35.8G (-0.05%)</td><td align="right">25</td><td align="right">None</td></tr>
<tr><td>overlapping om2 `-d`</td><td align="right">6.73180866</td><td align="right">4.71s</td><td align="right">47.6G</td><td align="right">690</td><td align="right">None</td><td align="right">6.73180866 (=)</td><td align="right">4.83s (+2.4%)</td><td align="right">48.7G (+2.49%)</td><td align="right">690</td><td align="right">None</td></tr>
<tr><td>overlapping om4 `-d`</td><td align="right">6.86690179</td><td align="right">9.22s</td><td align="right">85.1G</td><td align="right">173</td><td align="right">None</td><td align="right">6.86690179 (=)</td><td align="right">9.35s (+1.3%)</td><td align="right">85.1G (+0.04%)</td><td align="right">173</td><td align="right">None</td></tr>
<tr><td>overlapping om5 `-d`</td><td align="right">6.86661780</td><td align="right">9.63s</td><td align="right">84.2G</td><td align="right">308</td><td align="right">None</td><td align="right">6.86661780 (=)</td><td align="right">10.15s (+5.4%)</td><td align="right">84.3G (+0.11%)</td><td align="right">308</td><td align="right">None</td></tr>
<tr><td>overlapping om6 `-d`</td><td align="right">6.88486214</td><td align="right">9.97s</td><td align="right">85.2G</td><td align="right">451</td><td align="right">None</td><td align="right">6.88486214 (=)</td><td align="right">11.51s (+15.4%)</td><td align="right">86.3G (+1.30%)</td><td align="right">451</td><td align="right">None</td></tr>
<tr><td>overlapping om8 `-d`</td><td align="right">6.98747316</td><td align="right">10.62s</td><td align="right">89.7G</td><td align="right">203</td><td align="right">None</td><td align="right">6.98747316 (=)</td><td align="right">10.71s (+0.9%)</td><td align="right">89.8G (+0.03%)</td><td align="right">203</td><td align="right">None</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">5.90790474</td><td align="right">0.814s</td><td align="right">7.6G</td><td align="right">199</td><td align="right">None</td><td align="right">5.90790474 (=)</td><td align="right">0.793s (-2.6%)</td><td align="right">7.6G (+0.47%)</td><td align="right">199</td><td align="right">None</td></tr>
</tbody></table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Overlapping and wikispeedia rows as `-C -2d -N10`.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar <code>-C -2</code> (old)</th>
<th colspan="5">columnar <code>-C -2</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.51775481 (=)</td><td align="right">0.001s (-20.0%)</td><td align="right">0.1G (+0.03%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td>jazz</td><td align="right">6.86122977</td><td align="right">0.008s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.86122977 (=)</td><td align="right">0.009s (+1.4%)</td><td align="right">0.1G (-0.42%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28307258</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.28307258 (=)</td><td align="right">0.012s (+2.1%)</td><td align="right">0.2G (-0.02%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td>powergrid</td><td align="right">5.63729688</td><td align="right">0.126s</td><td align="right">1.3G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.124s (-1.3%)</td><td align="right">1.3G (+0.24%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73957529</td><td align="right">0.051s</td><td align="right">0.6G</td><td align="right">81</td><td align="right">2</td><td align="right">6.73957529 (=)</td><td align="right">0.052s (+0.6%)</td><td align="right">0.6G (+0.01%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.94997883</td><td align="right">2.59s</td><td align="right">24.0G</td><td align="right">506</td><td align="right">2</td><td align="right">7.94997883 (=)</td><td align="right">2.49s (-4.0%)</td><td align="right">24.0G (-0.12%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td>web-NotreDame</td><td align="right">6.75421666</td><td align="right">23.46s</td><td align="right">122.3G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.75421666 (=)</td><td align="right">20.66s (-11.9%)</td><td align="right">122.0G (-0.24%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.006s (-2.0%)</td><td align="right">0.1G (-5.33%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (-3.7%)</td><td align="right">0.1G (-0.25%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.40044538</td><td align="right">2.92s</td><td align="right">32.4G</td><td align="right">168</td><td align="right">2</td><td align="right">7.40044538 (=)</td><td align="right">2.99s (+2.4%)</td><td align="right">32.4G (+0.24%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39305505</td><td align="right">4.16s</td><td align="right">42.4G</td><td align="right">334</td><td align="right">2</td><td align="right">5.39305505 (=)</td><td align="right">4.20s (+1.0%)</td><td align="right">42.4G (=)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57557704</td><td align="right">4.27s</td><td align="right">41.9G</td><td align="right">304</td><td align="right">2</td><td align="right">5.57153933 (-0.0724%)</td><td align="right">4.27s (-0.0%)</td><td align="right">41.7G (-0.48%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42414371</td><td align="right">11.58s</td><td align="right">105.8G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.42414371 (=)</td><td align="right">11.61s (+0.2%)</td><td align="right">105.8G (=)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.13s</td><td align="right">31.7G</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.11s (-0.6%)</td><td align="right">31.7G (-0.02%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>overlapping om2 `-2d`</td><td align="right">6.73927197</td><td align="right">4.02s</td><td align="right">39.1G</td><td align="right">638</td><td align="right">2</td><td align="right">6.73927197 (=)</td><td align="right">4.08s (+1.4%)</td><td align="right">40.8G (+4.36%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td>overlapping om4 `-2d`</td><td align="right">6.86690179</td><td align="right">8.22s</td><td align="right">73.2G</td><td align="right">173</td><td align="right">2</td><td align="right">6.86690179 (=)</td><td align="right">7.64s (-7.1%)</td><td align="right">73.2G (=)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-2d`</td><td align="right">6.86661780</td><td align="right">8.34s</td><td align="right">73.1G</td><td align="right">308</td><td align="right">2</td><td align="right">6.86661780 (=)</td><td align="right">8.42s (+0.9%)</td><td align="right">73.1G (+0.06%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d`</td><td align="right">6.88486214</td><td align="right">8.72s</td><td align="right">71.0G</td><td align="right">451</td><td align="right">2</td><td align="right">6.88486214 (=)</td><td align="right">8.13s (-6.8%)</td><td align="right">73.0G (+2.84%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-2d`</td><td align="right">6.88723447</td><td align="right">9.68s</td><td align="right">77.6G</td><td align="right">921</td><td align="right">2</td><td align="right">6.88723447 (=)</td><td align="right">9.73s (+0.5%)</td><td align="right">77.6G (=)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-2d`</td><td align="right">5.90790474</td><td align="right">0.765s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.90790474 (=)</td><td align="right">0.753s (-1.5%)</td><td align="right">7.2G (-0.27%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody></table>

### Single-trial runs (`-C -N1`)

Interleaved minimum of 3 per arm.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">None</td><td align="right">3.38583082 (=)</td><td align="right">0.000s (-6.4%)</td><td align="right">0.1G (+0.08%)</td><td align="right">3</td><td align="right">None</td></tr>
<tr><td>jazz</td><td align="right">6.89936796</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">None</td><td align="right">6.89936796 (=)</td><td align="right">0.001s (+8.9%)</td><td align="right">0.1G (+0.98%)</td><td align="right">11</td><td align="right">None</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.06468836</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">4</td><td align="right">None</td><td align="right">4.06468836 (=)</td><td align="right">0.004s (+10.5%)</td><td align="right">0.1G (-0.29%)</td><td align="right">4</td><td align="right">None</td></tr>
<tr><td>powergrid</td><td align="right">4.75504777</td><td align="right">0.031s</td><td align="right">0.4G</td><td align="right">4</td><td align="right">None</td><td align="right">4.75504777 (=)</td><td align="right">0.032s (+4.5%)</td><td align="right">0.4G (-0.03%)</td><td align="right">4</td><td align="right">None</td></tr>
<tr><td>politicalblogs</td><td align="right">6.75842160</td><td align="right">0.011s</td><td align="right">0.2G</td><td align="right">2</td><td align="right">None</td><td align="right">6.75842160 (=)</td><td align="right">0.011s (+0.0%)</td><td align="right">0.2G (+0.06%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">0.419s</td><td align="right">5.7G</td><td align="right">15</td><td align="right">None</td><td align="right">7.83343660 (=)</td><td align="right">0.439s (+4.8%)</td><td align="right">5.7G (+0.03%)</td><td align="right">15</td><td align="right">None</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">2.67s</td><td align="right">26.9G</td><td align="right">5</td><td align="right">None</td><td align="right">5.56852929 (=)</td><td align="right">2.64s (-1.1%)</td><td align="right">26.9G (-0.13%)</td><td align="right">5</td><td align="right">None</td></tr>
<tr><td>lazega</td><td align="right">6.04111740</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">None</td><td align="right">6.04111740 (=)</td><td align="right">0.001s (-11.5%)</td><td align="right">0.1G (+0.47%)</td><td align="right">6</td><td align="right">None</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">None</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (+2.4%)</td><td align="right">0.1G (-0.25%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>malaria</td><td align="right">7.50222401</td><td align="right">0.393s</td><td align="right">5.3G</td><td align="right">8</td><td align="right">None</td><td align="right">7.50222401 (=)</td><td align="right">0.391s (-0.6%)</td><td align="right">5.3G (+0.03%)</td><td align="right">8</td><td align="right">None</td></tr>
<tr><td>air30k</td><td align="right">5.47323105</td><td align="right">0.415s</td><td align="right">4.7G</td><td align="right">22</td><td align="right">None</td><td align="right">5.47323105 (=)</td><td align="right">0.415s (+0.0%)</td><td align="right">4.7G (+0.32%)</td><td align="right">22</td><td align="right">None</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.66687303</td><td align="right">0.521s</td><td align="right">5.9G</td><td align="right">15</td><td align="right">None</td><td align="right">5.66839033 (+0.0268%)</td><td align="right">0.553s (+6.1%)</td><td align="right">6.2G (+6.28%)</td><td align="right">20</td><td align="right">None</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.58074898</td><td align="right">1.02s</td><td align="right">9.7G</td><td align="right">43</td><td align="right">None</td><td align="right">7.58074898 (=)</td><td align="right">1.01s (-1.1%)</td><td align="right">9.7G (+0.03%)</td><td align="right">43</td><td align="right">None</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.46079677</td><td align="right">0.435s</td><td align="right">5.9G</td><td align="right">5</td><td align="right">None</td><td align="right">8.46079677 (=)</td><td align="right">0.429s (-1.4%)</td><td align="right">5.9G (+0.01%)</td><td align="right">5</td><td align="right">None</td></tr>
<tr><td>overlapping om2 `-2d`</td><td align="right">6.73935821</td><td align="right">1.58s</td><td align="right">17.0G</td><td align="right">674</td><td align="right">2</td><td align="right">6.73935821 (=)</td><td align="right">1.67s (+5.6%)</td><td align="right">17.8G (+4.62%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td>overlapping om4 `-2d`</td><td align="right">6.86172465</td><td align="right">1.85s</td><td align="right">18.4G</td><td align="right">141</td><td align="right">2</td><td align="right">6.86172465 (=)</td><td align="right">1.87s (+0.8%)</td><td align="right">18.4G (+0.03%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-2d`</td><td align="right">6.86812714</td><td align="right">2.09s</td><td align="right">20.0G</td><td align="right">293</td><td align="right">2</td><td align="right">6.86812714 (=)</td><td align="right">2.03s (-2.9%)</td><td align="right">20.1G (+0.26%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d`</td><td align="right">6.88404244</td><td align="right">2.06s</td><td align="right">18.8G</td><td align="right">455</td><td align="right">2</td><td align="right">6.88404244 (=)</td><td align="right">2.08s (+1.2%)</td><td align="right">19.0G (+1.11%)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-2d`</td><td align="right">6.89337704</td><td align="right">2.60s</td><td align="right">23.1G</td><td align="right">911</td><td align="right">2</td><td align="right">6.89337704 (=)</td><td align="right">2.66s (+2.1%)</td><td align="right">23.1G (+0.03%)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td>overlapping om2 `-d`</td><td align="right">7.31721319</td><td align="right">0.444s</td><td align="right">4.2G</td><td align="right">75</td><td align="right">None</td><td align="right">7.31721319 (=)</td><td align="right">0.446s (+0.5%)</td><td align="right">4.2G (-0.04%)</td><td align="right">75</td><td align="right">None</td></tr>
<tr><td>overlapping om4 `-d`</td><td align="right">7.98293180</td><td align="right">0.914s</td><td align="right">8.4G</td><td align="right">1</td><td align="right">None</td><td align="right">7.98293180 (=)</td><td align="right">0.899s (-1.7%)</td><td align="right">8.4G (+0.09%)</td><td align="right">1</td><td align="right">None</td></tr>
<tr><td>overlapping om5 `-d`</td><td align="right">7.88481128</td><td align="right">0.909s</td><td align="right">8.1G</td><td align="right">66</td><td align="right">None</td><td align="right">7.88481128 (=)</td><td align="right">0.906s (-0.4%)</td><td align="right">8.1G (+0.01%)</td><td align="right">66</td><td align="right">None</td></tr>
<tr><td>overlapping om6 `-d`</td><td align="right">7.47937471</td><td align="right">0.954s</td><td align="right">8.4G</td><td align="right">92</td><td align="right">None</td><td align="right">7.47937471 (=)</td><td align="right">0.968s (+1.4%)</td><td align="right">8.4G (-0.06%)</td><td align="right">92</td><td align="right">None</td></tr>
<tr><td>overlapping om8 `-d`</td><td align="right">6.99609633</td><td align="right">0.981s</td><td align="right">8.5G</td><td align="right">206</td><td align="right">None</td><td align="right">6.99609633 (=)</td><td align="right">0.971s (-1.0%)</td><td align="right">8.5G (-0.02%)</td><td align="right">206</td><td align="right">None</td></tr>
<tr><td>overlapping om2 `-2d -c` planted</td><td align="right">6.74472199</td><td align="right">0.583s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.74472199 (=)</td><td align="right">0.620s (+6.3%)</td><td align="right">6.5G (+4.71%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td>overlapping om4 `-2d -c` planted</td><td align="right">6.85623947</td><td align="right">1.07s</td><td align="right">10.8G</td><td align="right">132</td><td align="right">2</td><td align="right">6.85623947 (=)</td><td align="right">1.05s (-1.5%)</td><td align="right">10.8G (+0.03%)</td><td align="right">132</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-2d -c` planted</td><td align="right">6.85777811</td><td align="right">1.32s</td><td align="right">13.1G</td><td align="right">296</td><td align="right">2</td><td align="right">6.85777811 (=)</td><td align="right">1.32s (-0.1%)</td><td align="right">13.1G (+0.03%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d -c` planted</td><td align="right">6.87337876</td><td align="right">1.48s</td><td align="right">13.8G</td><td align="right">446</td><td align="right">2</td><td align="right">6.87337876 (=)</td><td align="right">1.44s (-2.6%)</td><td align="right">13.8G (+0.02%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-2d -c` planted</td><td align="right">6.87523739</td><td align="right">1.62s</td><td align="right">14.8G</td><td align="right">894</td><td align="right">2</td><td align="right">6.87523739 (=)</td><td align="right">1.63s (+0.7%)</td><td align="right">14.8G (+0.04%)</td><td align="right">894</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.110s</td><td align="right">1.1G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.110s (+0.3%)</td><td align="right">1.1G (=)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">6.11312587</td><td align="right">0.093s</td><td align="right">1.0G</td><td align="right">46</td><td align="right">None</td><td align="right">6.11312587 (=)</td><td align="right">0.093s (+0.3%)</td><td align="right">1.0G (+0.88%)</td><td align="right">46</td><td align="right">None</td></tr>
</tbody></table>

### The overlapping family in full

Every configuration of the five planted overlapping state networks, both arms.

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
<tr><td>om2 `-2d -N1`</td><td align="right">6.73935821</td><td align="right">1.69s</td><td align="right">17.0G</td><td align="right">674</td><td align="right">2</td><td align="right">6.73935821 (=)</td><td align="right">1.76s (+3.7%)</td><td align="right">17.8G (+4.61%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td>om4 `-2d -N1`</td><td align="right">6.86172465</td><td align="right">2.13s</td><td align="right">18.4G</td><td align="right">141</td><td align="right">2</td><td align="right">6.86172465 (=)</td><td align="right">2.21s (+3.6%)</td><td align="right">18.4G (+0.06%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td>om5 `-2d -N1`</td><td align="right">6.86812714</td><td align="right">2.19s</td><td align="right">20.0G</td><td align="right">293</td><td align="right">2</td><td align="right">6.86812714 (=)</td><td align="right">2.28s (+4.3%)</td><td align="right">20.1G (+0.27%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td>om6 `-2d -N1`</td><td align="right">6.88404244</td><td align="right">2.23s</td><td align="right">18.8G</td><td align="right">455</td><td align="right">2</td><td align="right">6.88404244 (=)</td><td align="right">2.27s (+2.1%)</td><td align="right">19.0G (+1.11%)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td>om8 `-2d -N1`</td><td align="right">6.89337704</td><td align="right">2.80s</td><td align="right">23.1G</td><td align="right">911</td><td align="right">2</td><td align="right">6.89337704 (=)</td><td align="right">2.83s (+0.8%)</td><td align="right">23.1G (+0.04%)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td>om2 `-2d -N10`</td><td align="right">6.73927197</td><td align="right">4.37s</td><td align="right">39.1G</td><td align="right">638</td><td align="right">2</td><td align="right">6.73927197 (=)</td><td align="right">4.53s (+3.6%)</td><td align="right">40.8G (+4.40%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td>om4 `-2d -N10`</td><td align="right">6.86690179</td><td align="right">9.28s</td><td align="right">73.2G</td><td align="right">173</td><td align="right">2</td><td align="right">6.86690179 (=)</td><td align="right">9.36s (+0.8%)</td><td align="right">73.2G (=)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td>om5 `-2d -N10`</td><td align="right">6.86661780</td><td align="right">10.05s</td><td align="right">73.1G</td><td align="right">308</td><td align="right">2</td><td align="right">6.86661780 (=)</td><td align="right">8.96s (-10.8%)</td><td align="right">73.1G (+0.06%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>om6 `-2d -N10`</td><td align="right">6.88486214</td><td align="right">9.74s</td><td align="right">71.0G</td><td align="right">451</td><td align="right">2</td><td align="right">6.88486214 (=)</td><td align="right">10.22s (+4.9%)</td><td align="right">73.1G (+2.86%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td>om8 `-2d -N10`</td><td align="right">6.88723447</td><td align="right">11.89s</td><td align="right">77.7G</td><td align="right">921</td><td align="right">2</td><td align="right">6.88723447 (=)</td><td align="right">12.32s (+3.6%)</td><td align="right">77.6G (-0.06%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td>om2 `-d -N1`</td><td align="right">7.31721319</td><td align="right">0.514s</td><td align="right">4.2G</td><td align="right">75</td><td align="right">None</td><td align="right">7.31721319 (=)</td><td align="right">0.518s (+0.7%)</td><td align="right">4.2G (+0.04%)</td><td align="right">75</td><td align="right">None</td></tr>
<tr><td>om4 `-d -N1`</td><td align="right">7.98293180</td><td align="right">1.14s</td><td align="right">8.4G</td><td align="right">1</td><td align="right">None</td><td align="right">7.98293180 (=)</td><td align="right">1.09s (-4.6%)</td><td align="right">8.4G (+0.04%)</td><td align="right">1</td><td align="right">None</td></tr>
<tr><td>om5 `-d -N1`</td><td align="right">7.88481128</td><td align="right">0.915s</td><td align="right">8.1G</td><td align="right">66</td><td align="right">None</td><td align="right">7.88481128 (=)</td><td align="right">0.908s (-0.8%)</td><td align="right">8.1G (+0.12%)</td><td align="right">66</td><td align="right">None</td></tr>
<tr><td>om6 `-d -N1`</td><td align="right">7.47937471</td><td align="right">0.962s</td><td align="right">8.4G</td><td align="right">92</td><td align="right">None</td><td align="right">7.47937471 (=)</td><td align="right">0.972s (+1.0%)</td><td align="right">8.4G (-0.11%)</td><td align="right">92</td><td align="right">None</td></tr>
<tr><td>om8 `-d -N1`</td><td align="right">6.99609633</td><td align="right">0.998s</td><td align="right">8.5G</td><td align="right">206</td><td align="right">None</td><td align="right">6.99609633 (=)</td><td align="right">0.980s (-1.8%)</td><td align="right">8.5G (-0.64%)</td><td align="right">206</td><td align="right">None</td></tr>
<tr><td>om2 `-d -N10`</td><td align="right">6.73180866</td><td align="right">5.76s</td><td align="right">47.6G</td><td align="right">690</td><td align="right">None</td><td align="right">6.73180866 (=)</td><td align="right">5.60s (-2.8%)</td><td align="right">48.8G (+2.43%)</td><td align="right">690</td><td align="right">None</td></tr>
<tr><td>om4 `-d -N10`</td><td align="right">6.86690179</td><td align="right">9.55s</td><td align="right">85.2G</td><td align="right">173</td><td align="right">None</td><td align="right">6.86690179 (=)</td><td align="right">9.46s (-0.9%)</td><td align="right">85.1G (-0.12%)</td><td align="right">173</td><td align="right">None</td></tr>
<tr><td>om5 `-d -N10`</td><td align="right">6.86661780</td><td align="right">9.76s</td><td align="right">84.2G</td><td align="right">308</td><td align="right">None</td><td align="right">6.86661780 (=)</td><td align="right">9.67s (-0.9%)</td><td align="right">84.2G (+0.08%)</td><td align="right">308</td><td align="right">None</td></tr>
<tr><td>om6 `-d -N10`</td><td align="right">6.88486214</td><td align="right">9.18s</td><td align="right">85.1G</td><td align="right">451</td><td align="right">None</td><td align="right">6.88486214 (=)</td><td align="right">9.45s (+3.0%)</td><td align="right">86.2G (+1.27%)</td><td align="right">451</td><td align="right">None</td></tr>
<tr><td>om8 `-d -N10`</td><td align="right">6.98747316</td><td align="right">10.59s</td><td align="right">89.7G</td><td align="right">203</td><td align="right">None</td><td align="right">6.98747316 (=)</td><td align="right">10.98s (+3.7%)</td><td align="right">89.8G (+0.06%)</td><td align="right">203</td><td align="right">None</td></tr>
<tr><td>om2 `-2d --regularized -N1`</td><td align="right">7.93902109</td><td align="right">0.827s</td><td align="right">6.4G</td><td align="right">120</td><td align="right">2</td><td align="right">7.54881618 (-4.9150%)</td><td align="right">1.01s (+22.7%)</td><td align="right">8.7G (+36.99%)</td><td align="right">120</td><td align="right">2</td></tr>
<tr><td>om4 `-2d --regularized -N1`</td><td align="right">7.97377205</td><td align="right">2.11s</td><td align="right">13.8G</td><td align="right">41</td><td align="right">2</td><td align="right">7.55689468 (-5.2281%)</td><td align="right">1.35s (-36.1%)</td><td align="right">10.1G (-26.35%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td>om5 `-2d --regularized -N1`</td><td align="right">7.96709570</td><td align="right">4.34s</td><td align="right">30.4G</td><td align="right">103</td><td align="right">2</td><td align="right">7.96699521 (-0.0013%)</td><td align="right">3.57s (-17.9%)</td><td align="right">27.6G (-9.35%)</td><td align="right">104</td><td align="right">2</td></tr>
<tr><td>om6 `-2d --regularized -N1`</td><td align="right">7.98157455</td><td align="right">1.83s</td><td align="right">12.5G</td><td align="right">113</td><td align="right">2</td><td align="right">7.98157455 (=)</td><td align="right">2.27s (+23.6%)</td><td align="right">16.8G (+34.91%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td>om8 `-2d --regularized -N1`</td><td align="right">7.97891240</td><td align="right">2.58s</td><td align="right">17.2G</td><td align="right">234</td><td align="right">2</td><td align="right">7.97891240 (=)</td><td align="right">3.33s (+29.1%)</td><td align="right">23.7G (+37.88%)</td><td align="right">234</td><td align="right">2</td></tr>
<tr><td>om2 `-2d --regularized -N10`</td><td align="right">7.93699970</td><td align="right">2.95s</td><td align="right">20.4G</td><td align="right">124</td><td align="right">2</td><td align="right">7.54872155 (-4.8920%)</td><td align="right">3.12s (+5.8%)</td><td align="right">25.8G (+26.67%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td>om4 `-2d --regularized -N10`</td><td align="right">7.97307668</td><td align="right">7.48s</td><td align="right">53.8G</td><td align="right">45</td><td align="right">2</td><td align="right">7.55689468 (-5.2198%)</td><td align="right">6.74s (-9.9%)</td><td align="right">50.0G (-7.05%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td>om5 `-2d --regularized -N10`</td><td align="right">7.96753108</td><td align="right">5.90s</td><td align="right">41.7G</td><td align="right">98</td><td align="right">2</td><td align="right">7.96753108 (=)</td><td align="right">7.32s (+24.1%)</td><td align="right">56.9G (+36.51%)</td><td align="right">98</td><td align="right">2</td></tr>
<tr><td>om6 `-2d --regularized -N10`</td><td align="right">7.98157455</td><td align="right">5.27s</td><td align="right">37.6G</td><td align="right">113</td><td align="right">2</td><td align="right">7.98157455 (=)</td><td align="right">8.16s (+55.0%)</td><td align="right">52.0G (+38.56%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td>om8 `-2d --regularized -N10`</td><td align="right">7.97614021</td><td align="right">8.09s</td><td align="right">52.4G</td><td align="right">240</td><td align="right">2</td><td align="right">7.97614021 (=)</td><td align="right">10.70s (+32.3%)</td><td align="right">72.6G (+38.53%)</td><td align="right">240</td><td align="right">2</td></tr>
<tr><td>om2 `-d --regularized -N1`</td><td align="right">7.97050808</td><td align="right">0.315s</td><td align="right">2.4G</td><td align="right">1</td><td align="right">None</td><td align="right">7.97050808 (=)</td><td align="right">0.658s (+109.0%)</td><td align="right">5.4G (+129.63%)</td><td align="right">1</td><td align="right">None</td></tr>
<tr><td>om4 `-d --regularized -N1`</td><td align="right">7.98284920</td><td align="right">0.758s</td><td align="right">4.9G</td><td align="right">1</td><td align="right">None</td><td align="right">7.98284920 (=)</td><td align="right">1.07s (+40.9%)</td><td align="right">9.3G (+89.15%)</td><td align="right">1</td><td align="right">None</td></tr>
<tr><td>om5 `-d --regularized -N1`</td><td align="right">7.98961307</td><td align="right">0.721s</td><td align="right">4.7G</td><td align="right">1</td><td align="right">None</td><td align="right">7.98961307 (=)</td><td align="right">1.03s (+42.7%)</td><td align="right">8.9G (+89.40%)</td><td align="right">1</td><td align="right">None</td></tr>
<tr><td>om6 `-d --regularized -N1`</td><td align="right">7.99349037</td><td align="right">0.731s</td><td align="right">4.6G</td><td align="right">1</td><td align="right">None</td><td align="right">7.99349037 (=)</td><td align="right">1.26s (+72.9%)</td><td align="right">10.0G (+114.56%)</td><td align="right">1</td><td align="right">None</td></tr>
<tr><td>om8 `-d --regularized -N1`</td><td align="right">7.99473567</td><td align="right">0.739s</td><td align="right">4.7G</td><td align="right">1</td><td align="right">None</td><td align="right">7.99473567 (=)</td><td align="right">1.33s (+80.4%)</td><td align="right">11.6G (+148.47%)</td><td align="right">1</td><td align="right">None</td></tr>
<tr><td>om2 `-d --regularized -N10`</td><td align="right">7.93264485</td><td align="right">3.51s</td><td align="right">26.3G</td><td align="right">131</td><td align="right">None</td><td align="right">7.54872155 (-4.8398%)</td><td align="right">4.62s (+31.6%)</td><td align="right">44.1G (+67.76%)</td><td align="right">126</td><td align="right">None</td></tr>
<tr><td>om4 `-d --regularized -N10`</td><td align="right">7.97307668</td><td align="right">13.39s</td><td align="right">76.0G</td><td align="right">45</td><td align="right">None</td><td align="right">7.55665371 (-5.2229%)</td><td align="right">10.49s (-21.7%)</td><td align="right">94.6G (+24.43%)</td><td align="right">78</td><td align="right">None</td></tr>
<tr><td>om5 `-d --regularized -N10`</td><td align="right">7.96500972</td><td align="right">8.45s</td><td align="right">55.3G</td><td align="right">106</td><td align="right">None</td><td align="right">7.96500972 (=)</td><td align="right">13.35s (+58.1%)</td><td align="right">109.7G (+98.22%)</td><td align="right">106</td><td align="right">None</td></tr>
<tr><td>om6 `-d --regularized -N10`</td><td align="right">7.98106358</td><td align="right">8.87s</td><td align="right">51.8G</td><td align="right">119</td><td align="right">None</td><td align="right">7.98106358 (=)</td><td align="right">12.26s (+38.2%)</td><td align="right">111.6G (+115.59%)</td><td align="right">119</td><td align="right">None</td></tr>
<tr><td>om8 `-d --regularized -N10`</td><td align="right">7.97781079</td><td align="right">6.00s</td><td align="right">43.5G</td><td align="right">232</td><td align="right">None</td><td align="right">7.97668114 (-0.0142%)</td><td align="right">9.88s (+64.6%)</td><td align="right">86.8G (+99.65%)</td><td align="right">256</td><td align="right">None</td></tr>
<tr><td>om2 `-2d -c` planted seed</td><td align="right">6.74472199</td><td align="right">0.647s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.74472199 (=)</td><td align="right">0.685s (+5.9%)</td><td align="right">6.5G (+4.67%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td>om4 `-2d -c` planted seed</td><td align="right">6.85623947</td><td align="right">1.17s</td><td align="right">10.8G</td><td align="right">132</td><td align="right">2</td><td align="right">6.85623947 (=)</td><td align="right">1.18s (+0.5%)</td><td align="right">10.8G (+0.04%)</td><td align="right">132</td><td align="right">2</td></tr>
<tr><td>om5 `-2d -c` planted seed</td><td align="right">6.85777811</td><td align="right">1.42s</td><td align="right">13.1G</td><td align="right">296</td><td align="right">2</td><td align="right">6.85777811 (=)</td><td align="right">1.44s (+1.6%)</td><td align="right">13.1G (+0.03%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td>om6 `-2d -c` planted seed</td><td align="right">6.87337876</td><td align="right">1.56s</td><td align="right">13.8G</td><td align="right">446</td><td align="right">2</td><td align="right">6.87337876 (=)</td><td align="right">1.60s (+2.4%)</td><td align="right">13.8G (+0.06%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td>om8 `-2d -c` planted seed</td><td align="right">6.87523739</td><td align="right">1.67s</td><td align="right">14.8G</td><td align="right">894</td><td align="right">2</td><td align="right">6.87523739 (=)</td><td align="right">1.67s (-0.1%)</td><td align="right">14.8G (+0.01%)</td><td align="right">894</td><td align="right">2</td></tr>
<tr><td>om2 planted, `--no-infomap -c`</td><td align="right">6.78904000</td><td align="right">0.084s</td><td align="right">0.8G</td><td align="right">8</td><td align="right">2</td><td align="right">6.78904000 (=)</td><td align="right">0.101s (+20.3%)</td><td align="right">0.9G (+0.19%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td>om4 planted, `--no-infomap -c`</td><td align="right">6.88065015</td><td align="right">0.207s</td><td align="right">1.4G</td><td align="right">16</td><td align="right">2</td><td align="right">6.88065015 (=)</td><td align="right">0.260s (+25.3%)</td><td align="right">1.4G (+0.60%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td>om5 planted, `--no-infomap -c`</td><td align="right">6.90222253</td><td align="right">0.239s</td><td align="right">1.6G</td><td align="right">20</td><td align="right">2</td><td align="right">6.90222253 (=)</td><td align="right">0.258s (+8.0%)</td><td align="right">1.6G (=)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td>om6 planted, `--no-infomap -c`</td><td align="right">6.93093499</td><td align="right">0.245s</td><td align="right">1.6G</td><td align="right">24</td><td align="right">2</td><td align="right">6.93093499 (=)</td><td align="right">0.236s (-3.8%)</td><td align="right">1.6G (=)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td>om8 planted, `--no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.259s</td><td align="right">1.7G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.306s (+18.1%)</td><td align="right">1.7G (+0.05%)</td><td align="right">32</td><td align="right">2</td></tr>
</tbody></table>

### The fast dial `-F`

`-F` skips the interior-layer refinement. Measured as `-C -F`: **`-F` alone does not select the columnar engine**. Both columns are the new binary.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.002s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">None</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (-16.9%)</td><td align="right">0.1G (-4.62%)</td><td align="right">3</td><td align="right">None</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.009s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">None</td><td align="right">6.86275593 (=)</td><td align="right">0.009s (+1.5%)</td><td align="right">0.1G (-2.75%)</td><td align="right">6</td><td align="right">None</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.028s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">None</td><td align="right">4.06300588 (+0.2088%)</td><td align="right">0.017s (-38.2%)</td><td align="right">0.2G (-30.71%)</td><td align="right">4</td><td align="right">None</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.285s</td><td align="right">3.1G</td><td align="right">5</td><td align="right">None</td><td align="right">4.77402225 (+0.6950%)</td><td align="right">0.182s (-36.0%)</td><td align="right">1.9G (-37.78%)</td><td align="right">4</td><td align="right">None</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.082s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">None</td><td align="right">6.74094314 (=)</td><td align="right">0.069s (-15.9%)</td><td align="right">0.7G (-4.25%)</td><td align="right">81</td><td align="right">None</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.62s</td><td align="right">34.2G</td><td align="right">15</td><td align="right">None</td><td align="right">7.83343660 (=)</td><td align="right">3.49s (-3.5%)</td><td align="right">33.2G (-2.98%)</td><td align="right">15</td><td align="right">None</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">25.69s</td><td align="right">189.5G</td><td align="right">5</td><td align="right">None</td><td align="right">5.62506198 (+1.0152%)</td><td align="right">18.54s (-27.8%)</td><td align="right">138.8G (-26.76%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">None</td><td align="right">6.01786027 (=)</td><td align="right">0.005s (-7.0%)</td><td align="right">0.1G (-3.69%)</td><td align="right">7</td><td align="right">None</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">None</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (-9.4%)</td><td align="right">0.1G (-1.86%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.80s</td><td align="right">37.8G</td><td align="right">2</td><td align="right">None</td><td align="right">7.39750171 (=)</td><td align="right">3.52s (-7.3%)</td><td align="right">36.9G (-2.22%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">5.00s</td><td align="right">45.2G</td><td align="right">22</td><td align="right">None</td><td align="right">5.39242541 (=)</td><td align="right">4.77s (-4.7%)</td><td align="right">42.5G (-5.95%)</td><td align="right">22</td><td align="right">None</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57474682</td><td align="right">5.02s</td><td align="right">47.0G</td><td align="right">21</td><td align="right">None</td><td align="right">5.57474682 (=)</td><td align="right">4.49s (-10.5%)</td><td align="right">41.9G (-10.84%)</td><td align="right">21</td><td align="right">None</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">12.03s</td><td align="right">99.0G</td><td align="right">23</td><td align="right">None</td><td align="right">7.42215327 (=)</td><td align="right">11.64s (-3.2%)</td><td align="right">96.5G (-2.56%)</td><td align="right">23</td><td align="right">None</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.57s</td><td align="right">35.8G</td><td align="right">25</td><td align="right">None</td><td align="right">8.23558553 (=)</td><td align="right">3.64s (+2.0%)</td><td align="right">35.0G (-2.15%)</td><td align="right">25</td><td align="right">None</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">5.90790474</td><td align="right">0.828s</td><td align="right">7.6G</td><td align="right">199</td><td align="right">None</td><td align="right">5.90790474 (=)</td><td align="right">0.782s (-5.6%)</td><td align="right">7.3G (-3.81%)</td><td align="right">199</td><td align="right">None</td></tr>
</tbody></table>

### The non-redundant map equation L\* (`--non-redundant`)

L\* is a different objective, so a lower number is not a better partition of the same objective. Both columns are the new binary.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.002s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">None</td><td align="right">3.07806732 (-9.0897%)</td><td align="right">0.002s (-0.3%)</td><td align="right">0.1G (-0.42%)</td><td align="right">3</td><td align="right">None</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.009s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">None</td><td align="right">6.86822837 (+0.0797%)</td><td align="right">0.009s (+7.1%)</td><td align="right">0.2G (+2.61%)</td><td align="right">7</td><td align="right">None</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.028s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">None</td><td align="right">3.89220976 (-4.0037%)</td><td align="right">0.027s (-3.7%)</td><td align="right">0.3G (-1.81%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.330s</td><td align="right">3.1G</td><td align="right">5</td><td align="right">None</td><td align="right">4.50926542 (-4.8893%)</td><td align="right">0.282s (-14.5%)</td><td align="right">3.0G (-0.49%)</td><td align="right">3</td><td align="right">None</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.074s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">None</td><td align="right">6.78924150 (+0.7165%)</td><td align="right">0.128s (+71.8%)</td><td align="right">0.8G (+2.04%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.45s</td><td align="right">34.2G</td><td align="right">15</td><td align="right">None</td><td align="right">8.00917226 (+2.2434%)</td><td align="right">2.83s (-17.8%)</td><td align="right">30.9G (-9.50%)</td><td align="right">22</td><td align="right">None</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">22.18s</td><td align="right">189.0G</td><td align="right">5</td><td align="right">None</td><td align="right">5.51707363 (-0.9240%)</td><td align="right">22.06s (-0.6%)</td><td align="right">190.2G (+0.63%)</td><td align="right">5</td><td align="right">None</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">None</td><td align="right">5.96862465 (-0.8182%)</td><td align="right">0.006s (+6.3%)</td><td align="right">0.1G (-3.57%)</td><td align="right">7</td><td align="right">None</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">None</td><td align="right">1.92885658 (-4.1040%)</td><td align="right">0.001s (-15.8%)</td><td align="right">0.1G (-1.42%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.54s</td><td align="right">37.8G</td><td align="right">2</td><td align="right">None</td><td align="right">7.42757278 (+0.4065%)</td><td align="right">3.57s (+1.0%)</td><td align="right">37.6G (-0.38%)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">4.67s</td><td align="right">45.2G</td><td align="right">22</td><td align="right">None</td><td align="right">5.37891261 (-0.2506%)</td><td align="right">4.39s (-6.0%)</td><td align="right">44.5G (-1.45%)</td><td align="right">22</td><td align="right">None</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57474682</td><td align="right">4.88s</td><td align="right">47.0G</td><td align="right">21</td><td align="right">None</td><td align="right">5.56708744 (-0.1374%)</td><td align="right">4.57s (-6.4%)</td><td align="right">46.4G (-1.25%)</td><td align="right">21</td><td align="right">None</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">10.82s</td><td align="right">98.9G</td><td align="right">23</td><td align="right">None</td><td align="right">7.21529977 (-2.7870%)</td><td align="right">9.55s (-11.8%)</td><td align="right">86.9G (-12.17%)</td><td align="right">33</td><td align="right">None</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.47s</td><td align="right">35.8G</td><td align="right">25</td><td align="right">None</td><td align="right">8.44774545 (+2.5761%)</td><td align="right">3.38s (-2.6%)</td><td align="right">35.7G (-0.28%)</td><td align="right">25</td><td align="right">None</td></tr>
</tbody></table>

### OO vs columnar

**The OO numbers are carried from the `sync1033-fullrefresh` log, not re-measured.** This PR cannot move the OO path — both changes are columnar-only — so re-running OO would be measurement spent on a comparison no row of this change can affect, and on air30k (meta) the OO arm does not finish `-N10` at all. air30k (meta) is therefore quoted at `-N1` on the OO side. The overlapping networks are deliberately absent: OO has no regroup ladder and fixing its search is not the aim (F47).

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">object-oriented (logged)</th>
<th colspan="5">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.005s</td><td align="right">—</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.002s (-58.1%)</td><td align="right">0.1G (—)</td><td align="right">3</td><td align="right">None</td></tr>
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.023s</td><td align="right">—</td><td align="right">5</td><td align="right">2</td><td align="right">6.86275593 (-0.0042%)</td><td align="right">0.009s (-61.7%)</td><td align="right">0.1G (—)</td><td align="right">6</td><td align="right">None</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.04354934</td><td align="right">0.121s</td><td align="right">—</td><td align="right">2</td><td align="right">5</td><td align="right">4.05454025 (+0.2718%)</td><td align="right">0.027s (-77.3%)</td><td align="right">0.3G (—)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>powergrid</td><td align="right">4.75872920</td><td align="right">1.86s</td><td align="right">—</td><td align="right">5</td><td align="right">4</td><td align="right">4.74107206 (-0.3710%)</td><td align="right">0.274s (-85.3%)</td><td align="right">3.1G (—)</td><td align="right">5</td><td align="right">None</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73892798</td><td align="right">0.135s</td><td align="right">—</td><td align="right">80</td><td align="right">2</td><td align="right">6.74094314 (+0.0299%)</td><td align="right">0.068s (-49.3%)</td><td align="right">0.7G (—)</td><td align="right">81</td><td align="right">None</td></tr>
<tr><td>science2001</td><td align="right">7.83638921</td><td align="right">7.07s</td><td align="right">—</td><td align="right">11</td><td align="right">3</td><td align="right">7.83343660 (-0.0377%)</td><td align="right">3.45s (-51.3%)</td><td align="right">34.2G (—)</td><td align="right">15</td><td align="right">None</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56592477</td><td align="right">137.3s</td><td align="right">—</td><td align="right">17</td><td align="right">5</td><td align="right">5.56852929 (+0.0468%)</td><td align="right">21.14s (-84.6%)</td><td align="right">188.9G (—)</td><td align="right">5</td><td align="right">None</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.013s</td><td align="right">—</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (-0.0000%)</td><td align="right">0.006s (-57.1%)</td><td align="right">0.1G (—)</td><td align="right">7</td><td align="right">None</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">—</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (-0.0000%)</td><td align="right">0.001s (-29.6%)</td><td align="right">0.1G (—)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>malaria</td><td align="right">7.50242050</td><td align="right">8.25s</td><td align="right">—</td><td align="right">142</td><td align="right">2</td><td align="right">7.39750171 (-1.3985%)</td><td align="right">3.40s (-58.8%)</td><td align="right">37.7G (—)</td><td align="right">2</td><td align="right">None</td></tr>
<tr><td>air30k</td><td align="right">5.39287115</td><td align="right">10.93s</td><td align="right">—</td><td align="right">16</td><td align="right">3</td><td align="right">5.39242541 (-0.0083%)</td><td align="right">4.14s (-62.1%)</td><td align="right">45.2G (—)</td><td align="right">22</td><td align="right">None</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57843563</td><td align="right">7.16s</td><td align="right">—</td><td align="right">301</td><td align="right">2</td><td align="right">5.57474682 (-0.0661%)</td><td align="right">4.69s (-34.4%)</td><td align="right">47.0G (—)</td><td align="right">21</td><td align="right">None</td></tr>
<tr><td>air30k (meta)</td><td align="right">8.43783233</td><td align="right">5.18s</td><td align="right">—</td><td align="right">35</td><td align="right">3</td><td align="right">7.42215327 (-12.0372%)</td><td align="right">10.61s (+104.8%)</td><td align="right">98.9G (—)</td><td align="right">23</td><td align="right">None</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">7.94035360</td><td align="right">7.54s</td><td align="right">—</td><td align="right">25</td><td align="right">3</td><td align="right">8.23558553 (+3.7181%)</td><td align="right">3.43s (-54.6%)</td><td align="right">35.8G (—)</td><td align="right">25</td><td align="right">None</td></tr>
</tbody></table>

### OO vs columnar — two-level (`-2`)

OO column carried from the log, as above.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">object-oriented (logged)</th>
<th colspan="5">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.002s</td><td align="right">—</td><td align="right">9</td><td align="right">2</td><td align="right">3.51775481 (-0.0000%)</td><td align="right">0.001s (-50.9%)</td><td align="right">0.1G (—)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.013s</td><td align="right">—</td><td align="right">5</td><td align="right">2</td><td align="right">6.86122977 (-0.0265%)</td><td align="right">0.009s (-34.0%)</td><td align="right">0.1G (—)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28501267</td><td align="right">0.031s</td><td align="right">—</td><td align="right">56</td><td align="right">2</td><td align="right">4.28307258 (-0.0453%)</td><td align="right">0.012s (-61.9%)</td><td align="right">0.2G (—)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td>powergrid</td><td align="right">5.60044386</td><td align="right">0.559s</td><td align="right">—</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.124s (-77.7%)</td><td align="right">1.3G (—)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73972141</td><td align="right">0.076s</td><td align="right">—</td><td align="right">80</td><td align="right">2</td><td align="right">6.73957529 (-0.0022%)</td><td align="right">0.052s (-32.2%)</td><td align="right">0.6G (—)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.95003960</td><td align="right">3.66s</td><td align="right">—</td><td align="right">496</td><td align="right">2</td><td align="right">7.94997883 (-0.0008%)</td><td align="right">2.49s (-32.1%)</td><td align="right">24.0G (—)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td>web-NotreDame</td><td align="right">6.74298853</td><td align="right">41.21s</td><td align="right">—</td><td align="right">11809</td><td align="right">2</td><td align="right">6.75421666 (+0.1665%)</td><td align="right">20.66s (-49.9%)</td><td align="right">122.0G (—)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.008s</td><td align="right">—</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (-0.0000%)</td><td align="right">0.006s (-31.0%)</td><td align="right">0.1G (—)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">—</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (-0.0000%)</td><td align="right">0.001s (+35.7%)</td><td align="right">0.1G (—)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50595639</td><td align="right">6.01s</td><td align="right">—</td><td align="right">142</td><td align="right">2</td><td align="right">7.40044538 (-1.4057%)</td><td align="right">2.99s (-50.2%)</td><td align="right">32.4G (—)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39331278</td><td align="right">4.34s</td><td align="right">—</td><td align="right">332</td><td align="right">2</td><td align="right">5.39305505 (-0.0048%)</td><td align="right">4.20s (-3.1%)</td><td align="right">42.4G (—)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57921689</td><td align="right">5.36s</td><td align="right">—</td><td align="right">301</td><td align="right">2</td><td align="right">5.57153933 (-0.1376%)</td><td align="right">4.27s (-20.4%)</td><td align="right">41.7G (—)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.13096953</td><td align="right">5.55s</td><td align="right">—</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (+1.2866%)</td><td align="right">3.11s (-43.9%)</td><td align="right">31.7G (—)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody></table>
