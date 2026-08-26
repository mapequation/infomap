## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`), taken from the *same execution* as the wall time and reported because it is load- and clock-independent — on this desktop a wall delta of a couple of percent is below the measurement's resolution, and `instr` is what separates real work from noise. `time` is `--timing-json`'s `timing.total_s`: **one run per `-N10` row** (the codelength is deterministic and `instr` carries the comparison) and **interleaved minimum of 3 for `-N1` rows**.

> **This PR fixes one defect: the regroup ladder gated its candidate at BLOCK granularity against a
> LEAF-granularity incumbent.** A rung candidate is polished purify-only over the base units, then
> compared against a `bestCodelength` belonging to a partition the leaf move loop has already tuned — a
> constrained partition against a free one, so the ladder lost candidates to the handicap rather than to
> the objective. On om8 the winning candidate reads **7.528429223** as blocks against the incumbent's
> **7.446619468** and is rejected; the detector never escalates and the arm is inert (`COL_REGROUP=off`
> reproduces the tip bit-for-bit). Re-scored after the same leaf-level tune the incumbent had, it reads
> **7.075193632**, the detector escalates at 4.99%, and the run ends at **6.893377041 instead of
> 7.412472853 — −7.00% in bits** (−6.89% at `-N10`; seeds 234/345 give −6.5%/−7.0%).
>
> **Scope is what keeps it from costing anything where it cannot win.** The test answers an *escalation*
> question — is this trial in the pathological basin? — so it runs unconditionally in the detector pass,
> and is carried into the escalated ladder only when the detector's own accepted rung **came from it**. A
> trial that escalated on the block score alone is one where the ladder already works. It is also
> restricted to the ladder's first rung, and capped at a single sweep: the cap is on the *test*, not the
> answer, since an accepted candidate is re-tuned to convergence by the downstream fine-tune regardless.
> With that gate the `--regularized` overlapping rows go from +13.8…+16.1% to **−0.02…+0.07%** in
> instructions at unchanged bits. Two alternatives were measured and rejected — detector-only loses the
> om8 fix entirely while still spending +18.7% instructions, and root-optimizer-only changes no measured
> row, so it was removed rather than kept as an unjustified constant.

> ⚠️ **Where it still costs, in instructions retired.** Over the **111 bit-identical paired
> configurations** the median is **+0.021% in instructions** and −0.10% in time — the change is inert
> where it does nothing, and only two rows exceed 1%:
>
> | row | Δinstr | Δtime | Δbits |
> |---|--:|--:|--:|
> | om8 `-C -d -N10` | **+9.2%** | +9.8% | = |
> | wikispeedia `-C -2d -N10` | **+2.1%** | −0.0% | = |
> | air30k (meta) `-C -N10` | +0.80% | −0.3% | = |
> | malaria `-C -N10` | +0.25% | −1.1% | = |
> | om5 / om6 `-C -2d --regularized -N10` | −0.003% / +0.04% | +1.1% / +2.7% | = |
> | web-NotreDame `-C -N10` | +0.02% | +0.1% | = |
>
> **om8 `-C -d -N10` is the one that genuinely pays.** There the root detector's test wins, so the
> escalation gate correctly lets it through, but the hierarchical pipeline does not turn the escape into
> a better answer — both arms end at 6.987473156, itself worse than what the two-level search now reaches
> on the same network (6.887234466). That `-d` vs `-2d` gap on om8 exists in **both** arms and is not
> created here; it looks like a separate up-build limitation and is left as an open observation. No
> distance gate can remove the cost: on that row the wasted tests sit at score gaps of 1.12–4.60% above
> the incumbent while the rescue worth −7.00% sits at **1.0986%**, inside the cluster, with air30k's
> useless test closer still at 0.82%.

> **Deferred, deliberately: the recorded-teleportation index rate.** A module's use rate of the index
> codebook is `q = e + (T − t)·w` — the walker enters across a link *or* by teleporting in from outside.
> Every scoring site computes that (via `moduleTeleEnter`), but two places in the **optimizer** feed
> themselves `Level::enter`, which carries only `e`: the regroup probe, and `buildHierarchyFromBottom`
> (`superNet.flow = cur.enter` and the `curIndexCodelength` beside it). So the objective has always used
> `q` while the search proposes against `e` whenever teleportation is recorded. That is a real defect, it
> is **not fixed here**, and it gets its own PR. It is **columnar-only** — the OO core folds the teleport
> term straight into `enterFlow` in `aggregateFlowValuesFromLeafToRoot`, so there `enterFlow` *is* `q` and
> every consumer including the super-network build gets it for free; nothing to fix on master. The
> columnar representation keeps the two apart deliberately (a group's `(T−t)·w` is not additive over its
> members and has to be recomputed from the aggregates), and the price is that a consumer of
> `Level::enter` which forgets the teleport term silently gets the wrong quantity. Consequence for this
> snapshot: the om2/om4 `-2d --regularized` wins
> (−4.89%/−5.21% in bits) belong to that PR, because the corrected index rate is what makes the probe
> resolve them; on its own it buys nothing and costs +21…+32% in time, so neither half is shippable alone.
> Scoring is a separate code path and is untouched — verified on a multi-level tree round trip
> (air30k `-d --regularized`: **8.993069309** on OO, on the tip, and on this binary) and on twelve
> fixed-partition configurations across om2/om5/om6/om8 with and without `--regularized`.

> **Old** = a fresh `MODE=release OPENMP=0` build of the `columnar-hierarchical-core` tip `a008b85c`,
> md5 `45d8f427c293fa9f0a4b8e3e64eecb61`; **new** = this PR, md5 `82624b207d96b1c391d3c6237af8ca1a`. Both
> arms interleaved in one session. `COL_REGROUP_LEAFTUNE=off` reproduces the tip exactly.
### What the change moves

Auto-generated: **every** paired configuration in this document where old and new differ. Nothing else does.

| network | flags | old bits | new bits | Δbits | old t | new t | Δt | Δinstr | top old → new |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|
| om8 | `-C -2d -N1` | 7.41247285 | **6.89337704** | **-7.0030%** | 0.914s | 2.23s | +144.0% | +210.39% | 4175 → 911 |
| om8 | `-C -2d -N10` | 7.39712802 | **6.88723447** | **-6.8931%** | 9.61s | 8.74s | -9.1% | -2.41% | 4167 → 921 |

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
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.002s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (-17.8%)</td><td align="right">0.1G (-5.73%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.86275593 (=)</td><td align="right">0.007s (-0.2%)</td><td align="right">0.1G (-1.02%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">4</td><td align="right">4.05454025 (=)</td><td align="right">0.022s (+1.1%)</td><td align="right">0.3G (+0.04%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.236s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.74107206 (=)</td><td align="right">0.243s (+3.2%)</td><td align="right">2.8G (+0.02%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.059s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.74094314 (=)</td><td align="right">0.058s (-0.8%)</td><td align="right">0.7G (-0.20%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.05s</td><td align="right">34.1G</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">3.18s (+4.1%)</td><td align="right">34.1G (+0.03%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">19.41s</td><td align="right">182.4G</td><td align="right">5</td><td align="right">6</td><td align="right">5.56852929 (=)</td><td align="right">19.43s (+0.1%)</td><td align="right">182.5G (+0.02%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.003s (-9.1%)</td><td align="right">0.1G (-6.21%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (+8.0%)</td><td align="right">0.1G (+0.05%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.12s</td><td align="right">37.3G</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.08s (-1.1%)</td><td align="right">37.4G (+0.25%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">4.02s</td><td align="right">44.0G</td><td align="right">22</td><td align="right">3</td><td align="right">5.39242541 (=)</td><td align="right">4.00s (-0.7%)</td><td align="right">44.4G (+0.92%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.22s</td><td align="right">45.6G</td><td align="right">11</td><td align="right">3</td><td align="right">5.57624241 (=)</td><td align="right">4.29s (+1.7%)</td><td align="right">45.9G (+0.63%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.84s</td><td align="right">97.1G</td><td align="right">23</td><td align="right">3</td><td align="right">7.42215327 (=)</td><td align="right">9.82s (-0.3%)</td><td align="right">97.9G (+0.80%)</td><td align="right">23</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.26s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.31s (+1.6%)</td><td align="right">35.5G (=)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>overlapping om2 `-d`</td><td align="right">6.73180866</td><td align="right">4.30s</td><td align="right">46.3G</td><td align="right">690</td><td align="right">2</td><td align="right">6.73180866 (=)</td><td align="right">4.38s (+2.1%)</td><td align="right">46.3G (+0.01%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td>overlapping om4 `-d`</td><td align="right">6.86690179</td><td align="right">8.31s</td><td align="right">83.1G</td><td align="right">173</td><td align="right">2</td><td align="right">6.86690179 (=)</td><td align="right">8.39s (+1.0%)</td><td align="right">83.1G (+0.03%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-d`</td><td align="right">6.86661780</td><td align="right">8.08s</td><td align="right">81.5G</td><td align="right">308</td><td align="right">2</td><td align="right">6.86661780 (=)</td><td align="right">8.10s (+0.2%)</td><td align="right">81.5G (=)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-d`</td><td align="right">6.88486214</td><td align="right">8.34s</td><td align="right">82.2G</td><td align="right">451</td><td align="right">2</td><td align="right">6.88486214 (=)</td><td align="right">8.47s (+1.6%)</td><td align="right">82.2G (-0.02%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-d`</td><td align="right">6.98747316</td><td align="right">8.46s</td><td align="right">78.5G</td><td align="right">203</td><td align="right">4</td><td align="right">6.98747316 (=)</td><td align="right">9.28s (+9.8%)</td><td align="right">85.7G (+9.16%)</td><td align="right">203</td><td align="right">4</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">5.90790474</td><td align="right">0.705s</td><td align="right">7.1G</td><td align="right">199</td><td align="right">2</td><td align="right">5.90790474 (=)</td><td align="right">0.705s (-0.1%)</td><td align="right">7.2G (+1.01%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

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
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.51775481 (=)</td><td align="right">0.001s (-6.6%)</td><td align="right">0.1G (+0.30%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td>jazz</td><td align="right">6.86122977</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.86122977 (=)</td><td align="right">0.006s (-6.7%)</td><td align="right">0.1G (-0.25%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28307258</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.28307258 (=)</td><td align="right">0.009s (-0.7%)</td><td align="right">0.2G (+0.21%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td>powergrid</td><td align="right">5.63729688</td><td align="right">0.096s</td><td align="right">1.1G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.099s (+3.3%)</td><td align="right">1.2G (+0.22%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73957529</td><td align="right">0.043s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">6.73957529 (=)</td><td align="right">0.042s (-2.4%)</td><td align="right">0.5G (+0.02%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.94997883</td><td align="right">2.32s</td><td align="right">23.8G</td><td align="right">506</td><td align="right">2</td><td align="right">7.94997883 (=)</td><td align="right">2.37s (+2.1%)</td><td align="right">23.8G (+0.02%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td>web-NotreDame</td><td align="right">6.75421666</td><td align="right">19.01s</td><td align="right">117.6G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.75421666 (=)</td><td align="right">18.84s (-0.9%)</td><td align="right">117.6G (=)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.003s (-1.1%)</td><td align="right">0.1G (+0.06%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.000s (+6.9%)</td><td align="right">0.1G (+1.16%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.40044538</td><td align="right">2.83s</td><td align="right">32.1G</td><td align="right">168</td><td align="right">2</td><td align="right">7.40044538 (=)</td><td align="right">2.74s (-3.1%)</td><td align="right">32.2G (+0.39%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39305505</td><td align="right">3.84s</td><td align="right">41.1G</td><td align="right">334</td><td align="right">2</td><td align="right">5.39305505 (=)</td><td align="right">3.71s (-3.4%)</td><td align="right">41.9G (+1.96%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57557704</td><td align="right">3.82s</td><td align="right">40.9G</td><td align="right">304</td><td align="right">2</td><td align="right">5.57557704 (=)</td><td align="right">3.87s (+1.3%)</td><td align="right">41.4G (+1.39%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42414371</td><td align="right">10.73s</td><td align="right">103.5G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.42414371 (=)</td><td align="right">10.66s (-0.6%)</td><td align="right">104.7G (+1.24%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.04s</td><td align="right">31.6G</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.08s (+1.3%)</td><td align="right">31.6G (=)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>overlapping om2 `-2d`</td><td align="right">6.73927197</td><td align="right">3.56s</td><td align="right">38.1G</td><td align="right">638</td><td align="right">2</td><td align="right">6.73927197 (=)</td><td align="right">3.56s (=)</td><td align="right">38.1G (-0.01%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td>overlapping om4 `-2d`</td><td align="right">6.86690179</td><td align="right">7.27s</td><td align="right">71.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.86690179 (=)</td><td align="right">7.00s (-3.6%)</td><td align="right">71.6G (+0.05%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-2d`</td><td align="right">6.86661780</td><td align="right">6.71s</td><td align="right">71.3G</td><td align="right">308</td><td align="right">2</td><td align="right">6.86661780 (=)</td><td align="right">7.17s (+6.8%)</td><td align="right">71.3G (+0.02%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d`</td><td align="right">6.88486214</td><td align="right">7.17s</td><td align="right">69.0G</td><td align="right">451</td><td align="right">2</td><td align="right">6.88486214 (=)</td><td align="right">7.29s (+1.7%)</td><td align="right">69.0G (+0.02%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-2d`</td><td align="right">7.39712802</td><td align="right">9.61s</td><td align="right">77.3G</td><td align="right">4167</td><td align="right">2</td><td align="right">6.88723447 (-6.8931%)</td><td align="right">8.74s (-9.1%)</td><td align="right">75.4G (-2.41%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-2d`</td><td align="right">5.90790474</td><td align="right">0.664s</td><td align="right">6.8G</td><td align="right">199</td><td align="right">2</td><td align="right">5.90790474 (=)</td><td align="right">0.664s (=)</td><td align="right">6.9G (+2.08%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

Interleaved minimum of 3. `-N1` is where a time difference is code and nothing else, except on the rows this PR changes, where the new arm computes a different partition and pays for the escalation it now triggers.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar <code>-C -N1</code> (old)</th>
<th colspan="5">columnar <code>-C -N1</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>lazega</td><td align="right">6.04111740</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.04111740 (=)</td><td align="right">0.001s (-6.6%)</td><td align="right">0.1G (-0.81%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.000s (-1.6%)</td><td align="right">0.1G (-0.08%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50222401</td><td align="right">0.350s</td><td align="right">5.3G</td><td align="right">8</td><td align="right">3</td><td align="right">7.50222401 (=)</td><td align="right">0.362s (+3.6%)</td><td align="right">5.3G (+0.09%)</td><td align="right">8</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.47323105</td><td align="right">0.350s</td><td align="right">4.5G</td><td align="right">22</td><td align="right">3</td><td align="right">5.47323105 (=)</td><td align="right">0.351s (+0.3%)</td><td align="right">4.5G (-0.04%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.66687303</td><td align="right">0.448s</td><td align="right">5.7G</td><td align="right">15</td><td align="right">3</td><td align="right">5.66687303 (=)</td><td align="right">0.455s (+1.5%)</td><td align="right">5.7G (+0.12%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.58074898</td><td align="right">0.892s</td><td align="right">9.2G</td><td align="right">43</td><td align="right">3</td><td align="right">7.58074898 (=)</td><td align="right">0.876s (-1.7%)</td><td align="right">9.3G (+0.40%)</td><td align="right">43</td><td align="right">3</td></tr>
<tr><td>overlapping om2 `-2d`</td><td align="right">6.73935821</td><td align="right">1.42s</td><td align="right">16.5G</td><td align="right">674</td><td align="right">2</td><td align="right">6.73935821 (=)</td><td align="right">1.44s (+1.2%)</td><td align="right">16.5G (+0.02%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td>overlapping om4 `-2d`</td><td align="right">6.86172465</td><td align="right">1.58s</td><td align="right">17.6G</td><td align="right">141</td><td align="right">2</td><td align="right">6.86172465 (=)</td><td align="right">1.57s (-1.0%)</td><td align="right">17.6G (-0.01%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-2d`</td><td align="right">6.86812714</td><td align="right">1.81s</td><td align="right">19.1G</td><td align="right">293</td><td align="right">2</td><td align="right">6.86812714 (=)</td><td align="right">1.77s (-2.2%)</td><td align="right">19.1G (-0.02%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d`</td><td align="right">6.88404244</td><td align="right">1.79s</td><td align="right">17.8G</td><td align="right">455</td><td align="right">2</td><td align="right">6.88404244 (=)</td><td align="right">1.72s (-3.6%)</td><td align="right">17.8G (-0.02%)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-2d`</td><td align="right">7.41247285</td><td align="right">0.914s</td><td align="right">7.1G</td><td align="right">4175</td><td align="right">2</td><td align="right">6.89337704 (-7.0030%)</td><td align="right">2.23s (+144.0%)</td><td align="right">22.0G (+210.39%)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td>overlapping om2 `-d`</td><td align="right">7.31721319</td><td align="right">0.353s</td><td align="right">3.7G</td><td align="right">75</td><td align="right">4</td><td align="right">7.31721319 (=)</td><td align="right">0.355s (+0.4%)</td><td align="right">3.7G (-0.07%)</td><td align="right">75</td><td align="right">4</td></tr>
<tr><td>overlapping om4 `-d`</td><td align="right">7.98293180</td><td align="right">0.736s</td><td align="right">7.6G</td><td align="right">1</td><td align="right">4</td><td align="right">7.98293180 (=)</td><td align="right">0.731s (-0.7%)</td><td align="right">7.6G (+0.01%)</td><td align="right">1</td><td align="right">4</td></tr>
<tr><td>overlapping om5 `-d`</td><td align="right">7.88481128</td><td align="right">0.737s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.88481128 (=)</td><td align="right">0.722s (-2.1%)</td><td align="right">7.2G (=)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td>overlapping om6 `-d`</td><td align="right">7.47937471</td><td align="right">0.756s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.47937471 (=)</td><td align="right">0.783s (+3.5%)</td><td align="right">7.5G (+0.10%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td>overlapping om8 `-d`</td><td align="right">6.99609633</td><td align="right">0.783s</td><td align="right">7.5G</td><td align="right">206</td><td align="right">4</td><td align="right">6.99609633 (=)</td><td align="right">0.781s (-0.3%)</td><td align="right">7.5G (-0.02%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td>overlapping om2 `-2d -c` planted</td><td align="right">6.74472199</td><td align="right">0.530s</td><td align="right">5.9G</td><td align="right">476</td><td align="right">2</td><td align="right">6.74472199 (=)</td><td align="right">0.518s (-2.2%)</td><td align="right">5.9G (-0.02%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td>overlapping om4 `-2d -c` planted</td><td align="right">6.85623947</td><td align="right">0.918s</td><td align="right">10.4G</td><td align="right">132</td><td align="right">2</td><td align="right">6.85623947 (=)</td><td align="right">0.921s (+0.3%)</td><td align="right">10.4G (+0.02%)</td><td align="right">132</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-2d -c` planted</td><td align="right">6.85777811</td><td align="right">1.17s</td><td align="right">12.6G</td><td align="right">296</td><td align="right">2</td><td align="right">6.85777811 (=)</td><td align="right">1.15s (-1.4%)</td><td align="right">12.6G (-0.01%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d -c` planted</td><td align="right">6.87337876</td><td align="right">1.25s</td><td align="right">13.3G</td><td align="right">446</td><td align="right">2</td><td align="right">6.87337876 (=)</td><td align="right">1.26s (+1.4%)</td><td align="right">13.3G (-0.02%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td>overlapping om8 `-2d -c` planted</td><td align="right">6.87523739</td><td align="right">1.42s</td><td align="right">14.2G</td><td align="right">894</td><td align="right">2</td><td align="right">6.87523739 (=)</td><td align="right">1.42s (-0.3%)</td><td align="right">14.2G (+0.02%)</td><td align="right">894</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.088s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.089s (+1.7%)</td><td align="right">1.0G (+1.53%)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">6.11312587</td><td align="right">0.069s</td><td align="right">0.8G</td><td align="right">46</td><td align="right">3</td><td align="right">6.11312587 (=)</td><td align="right">0.070s (+1.7%)</td><td align="right">0.8G (+0.11%)</td><td align="right">46</td><td align="right">3</td></tr>
</tbody>
</table>

### The overlapping family in full

The F42/F47 regression guard. Under `--regularized` the planted partition of om6/om8 is worse than one-level, so it bounds nothing there.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">columnar (old, tip a008b85c)</th>
<th colspan="5">columnar (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>om2 `-2d -N1`</td><td align="right">6.73935821</td><td align="right">1.42s</td><td align="right">16.5G</td><td align="right">674</td><td align="right">2</td><td align="right">6.73935821 (=)</td><td align="right">1.44s (+1.2%)</td><td align="right">16.5G (+0.02%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td>om4 `-2d -N1`</td><td align="right">6.86172465</td><td align="right">1.58s</td><td align="right">17.6G</td><td align="right">141</td><td align="right">2</td><td align="right">6.86172465 (=)</td><td align="right">1.57s (-1.0%)</td><td align="right">17.6G (-0.01%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td>om5 `-2d -N1`</td><td align="right">6.86812714</td><td align="right">1.81s</td><td align="right">19.1G</td><td align="right">293</td><td align="right">2</td><td align="right">6.86812714 (=)</td><td align="right">1.77s (-2.2%)</td><td align="right">19.1G (-0.02%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td>om6 `-2d -N1`</td><td align="right">6.88404244</td><td align="right">1.79s</td><td align="right">17.8G</td><td align="right">455</td><td align="right">2</td><td align="right">6.88404244 (=)</td><td align="right">1.72s (-3.6%)</td><td align="right">17.8G (-0.02%)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td>om8 `-2d -N1`</td><td align="right">7.41247285</td><td align="right">0.914s</td><td align="right">7.1G</td><td align="right">4175</td><td align="right">2</td><td align="right">6.89337704 (-7.0030%)</td><td align="right">2.23s (+144.0%)</td><td align="right">22.0G (+210.39%)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td>om2 `-2d -N10`</td><td align="right">6.73927197</td><td align="right">3.56s</td><td align="right">38.1G</td><td align="right">638</td><td align="right">2</td><td align="right">6.73927197 (=)</td><td align="right">3.56s (=)</td><td align="right">38.1G (-0.01%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td>om4 `-2d -N10`</td><td align="right">6.86690179</td><td align="right">7.27s</td><td align="right">71.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.86690179 (=)</td><td align="right">7.00s (-3.6%)</td><td align="right">71.6G (+0.05%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td>om5 `-2d -N10`</td><td align="right">6.86661780</td><td align="right">6.71s</td><td align="right">71.3G</td><td align="right">308</td><td align="right">2</td><td align="right">6.86661780 (=)</td><td align="right">7.17s (+6.8%)</td><td align="right">71.3G (+0.02%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>om6 `-2d -N10`</td><td align="right">6.88486214</td><td align="right">7.17s</td><td align="right">69.0G</td><td align="right">451</td><td align="right">2</td><td align="right">6.88486214 (=)</td><td align="right">7.29s (+1.7%)</td><td align="right">69.0G (+0.02%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td>om8 `-2d -N10`</td><td align="right">7.39712802</td><td align="right">9.61s</td><td align="right">77.3G</td><td align="right">4167</td><td align="right">2</td><td align="right">6.88723447 (-6.8931%)</td><td align="right">8.74s (-9.1%)</td><td align="right">75.4G (-2.41%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td>om2 `-d -N1`</td><td align="right">7.31721319</td><td align="right">0.353s</td><td align="right">3.7G</td><td align="right">75</td><td align="right">4</td><td align="right">7.31721319 (=)</td><td align="right">0.355s (+0.4%)</td><td align="right">3.7G (-0.07%)</td><td align="right">75</td><td align="right">4</td></tr>
<tr><td>om4 `-d -N1`</td><td align="right">7.98293180</td><td align="right">0.736s</td><td align="right">7.6G</td><td align="right">1</td><td align="right">4</td><td align="right">7.98293180 (=)</td><td align="right">0.731s (-0.7%)</td><td align="right">7.6G (+0.01%)</td><td align="right">1</td><td align="right">4</td></tr>
<tr><td>om5 `-d -N1`</td><td align="right">7.88481128</td><td align="right">0.737s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.88481128 (=)</td><td align="right">0.722s (-2.1%)</td><td align="right">7.2G (=)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td>om6 `-d -N1`</td><td align="right">7.47937471</td><td align="right">0.756s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.47937471 (=)</td><td align="right">0.783s (+3.5%)</td><td align="right">7.5G (+0.10%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td>om8 `-d -N1`</td><td align="right">6.99609633</td><td align="right">0.783s</td><td align="right">7.5G</td><td align="right">206</td><td align="right">4</td><td align="right">6.99609633 (=)</td><td align="right">0.781s (-0.3%)</td><td align="right">7.5G (-0.02%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td>om2 `-d -N10`</td><td align="right">6.73180866</td><td align="right">4.30s</td><td align="right">46.3G</td><td align="right">690</td><td align="right">2</td><td align="right">6.73180866 (=)</td><td align="right">4.38s (+2.1%)</td><td align="right">46.3G (+0.01%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td>om4 `-d -N10`</td><td align="right">6.86690179</td><td align="right">8.31s</td><td align="right">83.1G</td><td align="right">173</td><td align="right">2</td><td align="right">6.86690179 (=)</td><td align="right">8.39s (+1.0%)</td><td align="right">83.1G (+0.03%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td>om5 `-d -N10`</td><td align="right">6.86661780</td><td align="right">8.08s</td><td align="right">81.5G</td><td align="right">308</td><td align="right">2</td><td align="right">6.86661780 (=)</td><td align="right">8.10s (+0.2%)</td><td align="right">81.5G (=)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td>om6 `-d -N10`</td><td align="right">6.88486214</td><td align="right">8.34s</td><td align="right">82.2G</td><td align="right">451</td><td align="right">2</td><td align="right">6.88486214 (=)</td><td align="right">8.47s (+1.6%)</td><td align="right">82.2G (-0.02%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td>om8 `-d -N10`</td><td align="right">6.98747316</td><td align="right">8.46s</td><td align="right">78.5G</td><td align="right">203</td><td align="right">4</td><td align="right">6.98747316 (=)</td><td align="right">9.28s (+9.8%)</td><td align="right">85.7G (+9.16%)</td><td align="right">203</td><td align="right">4</td></tr>
<tr><td>om2 `-2d --regularized -N1`</td><td align="right">7.93902109</td><td align="right">0.602s</td><td align="right">5.8G</td><td align="right">120</td><td align="right">2</td><td align="right">7.93902109 (=)</td><td align="right">0.607s (+0.7%)</td><td align="right">5.8G (+0.01%)</td><td align="right">120</td><td align="right">2</td></tr>
<tr><td>om4 `-2d --regularized -N1`</td><td align="right">7.97377205</td><td align="right">1.40s</td><td align="right">12.9G</td><td align="right">41</td><td align="right">2</td><td align="right">7.97377205 (=)</td><td align="right">1.42s (+1.6%)</td><td align="right">12.9G (-0.01%)</td><td align="right">41</td><td align="right">2</td></tr>
<tr><td>om5 `-2d --regularized -N1`</td><td align="right">7.96709570</td><td align="right">3.42s</td><td align="right">29.5G</td><td align="right">103</td><td align="right">2</td><td align="right">7.96709570 (=)</td><td align="right">3.39s (-1.0%)</td><td align="right">29.5G (-0.01%)</td><td align="right">103</td><td align="right">2</td></tr>
<tr><td>om6 `-2d --regularized -N1`</td><td align="right">7.98157455</td><td align="right">1.42s</td><td align="right">11.5G</td><td align="right">113</td><td align="right">2</td><td align="right">7.98157455 (=)</td><td align="right">1.38s (-3.1%)</td><td align="right">11.5G (-0.02%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td>om8 `-2d --regularized -N1`</td><td align="right">7.97891240</td><td align="right">2.02s</td><td align="right">16.1G</td><td align="right">234</td><td align="right">2</td><td align="right">7.97891240 (=)</td><td align="right">2.06s (+2.0%)</td><td align="right">16.1G (-0.01%)</td><td align="right">234</td><td align="right">2</td></tr>
<tr><td>om2 `-2d --regularized -N10`</td><td align="right">7.93699970</td><td align="right">2.17s</td><td align="right">19.6G</td><td align="right">124</td><td align="right">2</td><td align="right">7.93699970 (=)</td><td align="right">2.15s (-0.9%)</td><td align="right">19.6G (=)</td><td align="right">124</td><td align="right">2</td></tr>
<tr><td>om4 `-2d --regularized -N10`</td><td align="right">7.97307668</td><td align="right">5.83s</td><td align="right">52.2G</td><td align="right">45</td><td align="right">2</td><td align="right">7.97307668 (=)</td><td align="right">5.69s (-2.4%)</td><td align="right">52.2G (-0.02%)</td><td align="right">45</td><td align="right">2</td></tr>
<tr><td>om5 `-2d --regularized -N10`</td><td align="right">7.96753108</td><td align="right">4.69s</td><td align="right">39.9G</td><td align="right">98</td><td align="right">2</td><td align="right">7.96753108 (=)</td><td align="right">4.75s (+1.1%)</td><td align="right">39.9G (=)</td><td align="right">98</td><td align="right">2</td></tr>
<tr><td>om6 `-2d --regularized -N10`</td><td align="right">7.98157455</td><td align="right">4.42s</td><td align="right">36.6G</td><td align="right">113</td><td align="right">2</td><td align="right">7.98157455 (=)</td><td align="right">4.54s (+2.7%)</td><td align="right">36.6G (+0.04%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td>om8 `-2d --regularized -N10`</td><td align="right">7.97614021</td><td align="right">6.53s</td><td align="right">49.7G</td><td align="right">240</td><td align="right">2</td><td align="right">7.97614021 (=)</td><td align="right">6.77s (+3.7%)</td><td align="right">49.8G (+0.03%)</td><td align="right">240</td><td align="right">2</td></tr>
<tr><td>om2 `-2d -c` planted seed</td><td align="right">6.74472199</td><td align="right">0.530s</td><td align="right">5.9G</td><td align="right">476</td><td align="right">2</td><td align="right">6.74472199 (=)</td><td align="right">0.518s (-2.2%)</td><td align="right">5.9G (-0.02%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td>om4 `-2d -c` planted seed</td><td align="right">6.85623947</td><td align="right">0.918s</td><td align="right">10.4G</td><td align="right">132</td><td align="right">2</td><td align="right">6.85623947 (=)</td><td align="right">0.921s (+0.3%)</td><td align="right">10.4G (+0.02%)</td><td align="right">132</td><td align="right">2</td></tr>
<tr><td>om5 `-2d -c` planted seed</td><td align="right">6.85777811</td><td align="right">1.17s</td><td align="right">12.6G</td><td align="right">296</td><td align="right">2</td><td align="right">6.85777811 (=)</td><td align="right">1.15s (-1.4%)</td><td align="right">12.6G (-0.01%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td>om6 `-2d -c` planted seed</td><td align="right">6.87337876</td><td align="right">1.25s</td><td align="right">13.3G</td><td align="right">446</td><td align="right">2</td><td align="right">6.87337876 (=)</td><td align="right">1.26s (+1.4%)</td><td align="right">13.3G (-0.02%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td>om8 `-2d -c` planted seed</td><td align="right">6.87523739</td><td align="right">1.42s</td><td align="right">14.2G</td><td align="right">894</td><td align="right">2</td><td align="right">6.87523739 (=)</td><td align="right">1.42s (-0.3%)</td><td align="right">14.2G (+0.02%)</td><td align="right">894</td><td align="right">2</td></tr>
<tr><td>om2 planted, `--no-infomap -c`</td><td align="right">6.78904000</td><td align="right">0.050s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.78904000 (=)</td><td align="right">0.051s (+2.1%)</td><td align="right">0.6G (+0.05%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td>om4 planted, `--no-infomap -c`</td><td align="right">6.88065015</td><td align="right">0.102s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.88065015 (=)</td><td align="right">0.105s (+2.8%)</td><td align="right">1.0G (-0.08%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td>om5 planted, `--no-infomap -c`</td><td align="right">6.90222253</td><td align="right">0.111s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.90222253 (=)</td><td align="right">0.115s (+3.1%)</td><td align="right">1.1G (-0.15%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td>om6 planted, `--no-infomap -c`</td><td align="right">6.93093499</td><td align="right">0.123s</td><td align="right">1.1G</td><td align="right">24</td><td align="right">2</td><td align="right">6.93093499 (=)</td><td align="right">0.116s (-5.5%)</td><td align="right">1.1G (=)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td>om8 planted, `--no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.136s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.131s (-4.1%)</td><td align="right">1.2G (+0.05%)</td><td align="right">32</td><td align="right">2</td></tr>
<tr><td>om2 planted, `--regularized`</td><td align="right">7.58382058</td><td align="right">0.092s</td><td align="right">0.9G</td><td align="right">8</td><td align="right">2</td><td align="right">7.58382058 (=)</td><td align="right">0.091s (-0.9%)</td><td align="right">0.9G (+0.12%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td>om4 planted, `--regularized`</td><td align="right">7.58439679</td><td align="right">0.174s</td><td align="right">1.6G</td><td align="right">16</td><td align="right">2</td><td align="right">7.58439679 (=)</td><td align="right">0.167s (-3.7%)</td><td align="right">1.6G (-0.06%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td>om5 planted, `--regularized`</td><td align="right">7.79181011</td><td align="right">0.186s</td><td align="right">1.7G</td><td align="right">20</td><td align="right">2</td><td align="right">7.79181011 (=)</td><td align="right">0.185s (-0.6%)</td><td align="right">1.7G (+0.04%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td>om6 planted, `--regularized`</td><td align="right">8.02556766</td><td align="right">0.196s</td><td align="right">1.7G</td><td align="right">24</td><td align="right">2</td><td align="right">8.02556766 (=)</td><td align="right">0.195s (-1.0%)</td><td align="right">1.7G (-0.01%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td>om8 planted, `--regularized`</td><td align="right">8.29476796</td><td align="right">0.207s</td><td align="right">1.8G</td><td align="right">32</td><td align="right">2</td><td align="right">8.29476796 (=)</td><td align="right">0.209s (+1.0%)</td><td align="right">1.8G (=)</td><td align="right">32</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar

**The OO numbers are carried from the `sync1033-fullrefresh` log, not re-measured.** This PR cannot move the OO path — the regroup ladder is columnar-only — so re-running OO would be measurement spent on a comparison no row of this change can affect, and on air30k (meta) the OO arm does not finish `-N10` at all. air30k (meta) is therefore quoted at `-N1` on the OO side. The overlapping networks are deliberately absent: OO has no regroup ladder and fixing its search is not the aim (F47).

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
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.005s</td><td align="right">—</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (-75.7%)</td><td align="right">0.1G (—)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.023s</td><td align="right">—</td><td align="right">5</td><td align="right">2</td><td align="right">6.86275593 (-0.0042%)</td><td align="right">0.007s (-72.0%)</td><td align="right">0.1G (—)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.04354934</td><td align="right">0.121s</td><td align="right">—</td><td align="right">2</td><td align="right">5</td><td align="right">4.05454025 (+0.2718%)</td><td align="right">0.022s (-81.6%)</td><td align="right">0.3G (—)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.75872920</td><td align="right">1.86s</td><td align="right">—</td><td align="right">5</td><td align="right">4</td><td align="right">4.74107206 (-0.3710%)</td><td align="right">0.243s (-86.9%)</td><td align="right">2.8G (—)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73892798</td><td align="right">0.135s</td><td align="right">—</td><td align="right">80</td><td align="right">2</td><td align="right">6.74094314 (+0.0299%)</td><td align="right">0.058s (-56.6%)</td><td align="right">0.7G (—)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.83638921</td><td align="right">7.07s</td><td align="right">—</td><td align="right">11</td><td align="right">3</td><td align="right">7.83343660 (-0.0377%)</td><td align="right">3.18s (-55.1%)</td><td align="right">34.1G (—)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56592477</td><td align="right">137.3s</td><td align="right">—</td><td align="right">17</td><td align="right">5</td><td align="right">5.56852929 (+0.0468%)</td><td align="right">19.43s (-85.9%)</td><td align="right">182.5G (—)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.013s</td><td align="right">—</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.003s (-72.9%)</td><td align="right">0.1G (—)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">—</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (-66.7%)</td><td align="right">0.1G (—)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50242050</td><td align="right">8.25s</td><td align="right">—</td><td align="right">142</td><td align="right">2</td><td align="right">7.39750171 (-1.3985%)</td><td align="right">3.08s (-62.6%)</td><td align="right">37.4G (—)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39287115</td><td align="right">10.93s</td><td align="right">—</td><td align="right">16</td><td align="right">3</td><td align="right">5.39242541 (-0.0083%)</td><td align="right">4.00s (-63.4%)</td><td align="right">44.4G (—)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57843563</td><td align="right">7.16s</td><td align="right">—</td><td align="right">301</td><td align="right">2</td><td align="right">5.57624241 (-0.0393%)</td><td align="right">4.29s (-40.2%)</td><td align="right">45.9G (—)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">8.43783233</td><td align="right">5.18s</td><td align="right">—</td><td align="right">35</td><td align="right">3</td><td align="right">7.42215327 (-12.0372%)</td><td align="right">9.82s (+89.7%)</td><td align="right">97.9G (—)</td><td align="right">23</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">7.94035360</td><td align="right">7.54s</td><td align="right">—</td><td align="right">25</td><td align="right">3</td><td align="right">8.23558553 (+3.7181%)</td><td align="right">3.31s (-56.0%)</td><td align="right">35.5G (—)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar — two-level (`-2`)

OO column carried from the log, as above.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">object-oriented <code>-2</code> (logged)</th>
<th colspan="5">columnar <code>-C -2</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.002s</td><td align="right">—</td><td align="right">9</td><td align="right">2</td><td align="right">3.51775481 (=)</td><td align="right">0.001s (-70.6%)</td><td align="right">0.1G (—)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.013s</td><td align="right">—</td><td align="right">5</td><td align="right">2</td><td align="right">6.86122977 (-0.0265%)</td><td align="right">0.006s (-49.6%)</td><td align="right">0.1G (—)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28501267</td><td align="right">0.031s</td><td align="right">—</td><td align="right">56</td><td align="right">2</td><td align="right">4.28307258 (-0.0453%)</td><td align="right">0.009s (-71.9%)</td><td align="right">0.2G (—)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td>powergrid</td><td align="right">5.60044386</td><td align="right">0.559s</td><td align="right">—</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.099s (-82.2%)</td><td align="right">1.2G (—)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73972141</td><td align="right">0.076s</td><td align="right">—</td><td align="right">80</td><td align="right">2</td><td align="right">6.73957529 (-0.0022%)</td><td align="right">0.042s (-44.1%)</td><td align="right">0.5G (—)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.95003960</td><td align="right">3.66s</td><td align="right">—</td><td align="right">496</td><td align="right">2</td><td align="right">7.94997883 (-0.0008%)</td><td align="right">2.37s (-35.3%)</td><td align="right">23.8G (—)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td>web-NotreDame</td><td align="right">6.74298853</td><td align="right">41.21s</td><td align="right">—</td><td align="right">11809</td><td align="right">2</td><td align="right">6.75421666 (+0.1665%)</td><td align="right">18.84s (-54.3%)</td><td align="right">117.6G (—)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.008s</td><td align="right">—</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.003s (-57.4%)</td><td align="right">0.1G (—)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">—</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.000s (-68.0%)</td><td align="right">0.1G (—)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50595639</td><td align="right">6.01s</td><td align="right">—</td><td align="right">142</td><td align="right">2</td><td align="right">7.40044538 (-1.4057%)</td><td align="right">2.74s (-54.3%)</td><td align="right">32.2G (—)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39331278</td><td align="right">4.34s</td><td align="right">—</td><td align="right">332</td><td align="right">2</td><td align="right">5.39305505 (-0.0048%)</td><td align="right">3.71s (-14.5%)</td><td align="right">41.9G (—)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57921689</td><td align="right">5.36s</td><td align="right">—</td><td align="right">301</td><td align="right">2</td><td align="right">5.57557704 (-0.0652%)</td><td align="right">3.87s (-27.9%)</td><td align="right">41.4G (—)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.13096953</td><td align="right">5.55s</td><td align="right">—</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (+1.2866%)</td><td align="right">3.08s (-44.5%)</td><td align="right">31.6G (—)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

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
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (-28.9%)</td><td align="right">0.1G (-6.18%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.86275593 (=)</td><td align="right">0.006s (-5.0%)</td><td align="right">0.1G (-2.21%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">4</td><td align="right">4.06300588 (+0.2088%)</td><td align="right">0.013s (-39.4%)</td><td align="right">0.2G (-29.91%)</td><td align="right">4</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.243s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.77402225 (+0.6950%)</td><td align="right">0.143s (-41.1%)</td><td align="right">1.7G (-37.66%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.058s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.74094314 (=)</td><td align="right">0.055s (-6.2%)</td><td align="right">0.7G (-3.99%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.18s</td><td align="right">34.1G</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">2.95s (-7.2%)</td><td align="right">33.0G (-3.04%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">19.43s</td><td align="right">182.5G</td><td align="right">5</td><td align="right">6</td><td align="right">5.62506198 (+1.0152%)</td><td align="right">14.68s (-24.5%)</td><td align="right">127.0G (-30.37%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.003s (-3.9%)</td><td align="right">0.1G (-2.30%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.000s (-18.6%)</td><td align="right">0.1G (-1.36%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.08s</td><td align="right">37.4G</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.01s (-2.4%)</td><td align="right">36.5G (-2.26%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">4.00s</td><td align="right">44.4G</td><td align="right">22</td><td align="right">3</td><td align="right">5.39242541 (=)</td><td align="right">3.64s (-8.9%)</td><td align="right">41.7G (-6.10%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.29s</td><td align="right">45.9G</td><td align="right">11</td><td align="right">3</td><td align="right">5.57624241 (=)</td><td align="right">3.86s (-10.0%)</td><td align="right">40.8G (-11.16%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.82s</td><td align="right">97.9G</td><td align="right">23</td><td align="right">3</td><td align="right">7.42215327 (=)</td><td align="right">9.63s (-1.9%)</td><td align="right">95.3G (-2.61%)</td><td align="right">23</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.31s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.17s (-4.2%)</td><td align="right">34.7G (-2.23%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">5.90790474</td><td align="right">0.705s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.90790474 (=)</td><td align="right">0.674s (-4.4%)</td><td align="right">6.9G (-4.04%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

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
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.07806732 (-9.0897%)</td><td align="right">0.001s (-14.0%)</td><td align="right">0.1G (-5.66%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.86822837 (+0.0797%)</td><td align="right">0.007s (+2.9%)</td><td align="right">0.1G (+0.09%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">4</td><td align="right">3.89220976 (-4.0037%)</td><td align="right">0.022s (-0.8%)</td><td align="right">0.3G (-2.46%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.243s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.50926542 (-4.8893%)</td><td align="right">0.231s (-5.2%)</td><td align="right">2.8G (-1.54%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.058s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.78924150 (+0.7165%)</td><td align="right">0.061s (+4.0%)</td><td align="right">0.7G (+0.43%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">3.18s</td><td align="right">34.1G</td><td align="right">15</td><td align="right">3</td><td align="right">8.00917226 (+2.2434%)</td><td align="right">2.80s (-12.0%)</td><td align="right">30.8G (-9.70%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">19.43s</td><td align="right">182.5G</td><td align="right">5</td><td align="right">6</td><td align="right">5.51707363 (-0.9240%)</td><td align="right">19.50s (+0.3%)</td><td align="right">183.6G (+0.62%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.96862465 (-0.8182%)</td><td align="right">0.003s (-8.7%)</td><td align="right">0.1G (-2.67%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.92885658 (-4.1040%)</td><td align="right">0.000s (-23.2%)</td><td align="right">0.1G (-1.87%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.39750171</td><td align="right">3.08s</td><td align="right">37.4G</td><td align="right">2</td><td align="right">3</td><td align="right">7.42757278 (+0.4065%)</td><td align="right">3.13s (+1.5%)</td><td align="right">37.3G (-0.21%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39242541</td><td align="right">4.00s</td><td align="right">44.4G</td><td align="right">22</td><td align="right">3</td><td align="right">5.37891261 (-0.2506%)</td><td align="right">3.84s (-4.0%)</td><td align="right">43.8G (-1.47%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57624241</td><td align="right">4.29s</td><td align="right">45.9G</td><td align="right">11</td><td align="right">3</td><td align="right">5.56887516 (-0.1321%)</td><td align="right">4.29s (=)</td><td align="right">45.4G (-1.13%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.82s</td><td align="right">97.9G</td><td align="right">23</td><td align="right">3</td><td align="right">7.21529977 (-2.7870%)</td><td align="right">8.58s (-12.6%)</td><td align="right">85.8G (-12.34%)</td><td align="right">33</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.31s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.44774545 (+2.5761%)</td><td align="right">3.32s (+0.2%)</td><td align="right">35.4G (-0.41%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">5.90790474</td><td align="right">0.705s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.90320873 (-0.0795%)</td><td align="right">0.663s (-5.9%)</td><td align="right">6.9G (-4.31%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

