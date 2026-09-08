## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`); `time` is `--timing-json`'s `timing.total_s`. One run per `-N10` row (deterministic; `instr` carries the comparison); interleaved minimum of 3 for `-N1` rows. Driver and every row: [`columnar_wip/bench-dissolve.py`](columnar_wip/bench-dissolve.py), [`columnar_wip/dissolve-ab-results.tsv`](columnar_wip/dissolve-ab-results.tsv).

> **This PR is the columnar dissolve of unprofitable levels** (#1074, second track; the columnar counterpart of the OO #1075 that the sync #1077 brings in). The equal-depth columnar search cannot represent a ragged optimum, so a terminal pass on the settled winner removes every intermediate module-of-modules whose index codebook no longer pays for itself, best-first, on the pass's own exact accounting for every objective but L\* (L\* re-scores). Runs once per run after the deep repair, never per trial; guarded off under `--preferred-number-of-modules`; a no-op on `-2`. **Every codelength change below is on the columnar arm and is a gain**; the OO arm is untouched (it already has #1075) and is re-measured here only so that every table comes from one session.

> **Old** = a fresh `MODE=release OPENMP=0` build of the sync branch `sync-master-into-columnar-1076` tip `57a25a92` (the base this PR stacks on), md5 `533d4455993129f42ffc6ca2ff79265e`; **new** = this PR's head `ac1f4e26`, md5 `aa248e8238a33bbb9b372c0a79918137`. Both arms interleaved in one session. The machine carried a desktop load (Chrome, VS Code; load average 6–15) throughout, so wall times sit above the sync snapshot's on both arms; the old-vs-new comparison is unaffected and `instr` is load-independent.

### What the change moves

Every configuration where old and new differ in bits, both arms. Lower is better; every move is on the columnar arm and every move is down.

| network | table | old bits | new bits | Δbits | old instr | new instr | Δinstr |
|---|---|--:|--:|--:|--:|--:|--:|
| ninetriangles | `-C` | 3.38583082 | **3.371875026** | **-0.4122%** | 0.1G | 0.1G | -0.21% |
| netscicoauthor2010 | `-C` | 4.054540245 | **4.048857953** | **-0.1401%** | 0.3G | 0.3G | -0.41% |
| powergrid | `-C` | 4.741072056 | **4.717760238** | **-0.4917%** | 2.8G | 2.8G | -0.53% |
| science2001 | `-C` | 7.833436601 | **7.807937174** | **-0.3255%** | 34.1G | 34.0G | -0.04% |
| web-NotreDame | `-C` | 5.568529293 | **5.556421705** | **-0.2174%** | 183.0G | 181.3G | -0.95% |
| malaria | `-C` | 7.39750171 | **7.392442593** | **-0.0684%** | 37.5G | 37.5G | +0.01% |
| air30k | `-C` | 5.392425413 | **5.392285003** | **-0.0026%** | 44.5G | 44.5G | -0.05% |
| air30k (reg.) | `-C` | 5.574746817 | **5.574537176** | **-0.0038%** | 46.5G | 46.4G | -0.05% |
| air30k (meta) | `-C` | 7.42215327 | **7.421664324** | **-0.0066%** | 97.9G | 97.8G | -0.04% |
| overlapping om8 `-d` | `-C` | 6.987473156 | **6.959413622** | **-0.4016%** | 85.9G | 85.8G | -0.14% |
| ninetriangles | `-C -N1` | 3.38583082 | **3.371875026** | **-0.4122%** | 0.1G | 0.1G | -0.48% |
| netscicoauthor2010 | `-C -N1` | 4.064688363 | **4.047459862** | **-0.4239%** | 0.1G | 0.1G | +0.57% |
| powergrid | `-C -N1` | 4.75504777 | **4.730850312** | **-0.5089%** | 0.4G | 0.4G | +1.80% |
| politicalblogs | `-C -N1` | 6.758421601 | **6.758265349** | **-0.0023%** | 0.2G | 0.2G | +0.53% |
| science2001 | `-C -N1` | 7.833436601 | **7.807937174** | **-0.3255%** | 5.7G | 5.7G | +0.10% |
| web-NotreDame | `-C -N1` | 5.568529293 | **5.556421705** | **-0.2174%** | 23.9G | 24.3G | +1.88% |
| malaria | `-C -N1` | 7.502224007 | **7.491980364** | **-0.1365%** | 5.3G | 5.3G | +0.00% |
| air30k | `-C -N1` | 5.473231053 | **5.470440768** | **-0.0510%** | 4.5G | 4.5G | -0.06% |
| air30k (reg.) | `-C -N1` | 5.668390329 | **5.657913279** | **-0.1848%** | 5.9G | 5.9G | -0.47% |
| air30k (meta) | `-C -N1` | 7.580748978 | **7.546335898** | **-0.4540%** | 9.3G | 9.2G | -0.34% |
| overlapping om2 `-d` | `-C -N1` | 7.317213189 | **7.29196634** | **-0.3450%** | 3.7G | 3.7G | -0.49% |
| overlapping om5 `-d` | `-C -N1` | 7.884811284 | **7.812252898** | **-0.9202%** | 7.2G | 7.2G | -0.13% |
| overlapping om6 `-d` | `-C -N1` | 7.479374707 | **7.440161581** | **-0.5243%** | 7.5G | 7.5G | -0.37% |
| overlapping om8 `-d` | `-C -N1` | 6.996096327 | **6.967535764** | **-0.4082%** | 7.5G | 7.4G | -0.40% |
| wikispeedia `-d` | `-C -N1` | 6.113125871 | **6.066305904** | **-0.7659%** | 0.8G | 0.8G | -1.50% |

> **Time.** The pass is paid once per run, so `-N10` rows carry it at a tenth of the weight of `-N1` rows — and at `-N10` it *replaces* the best-trial restore's re-materialization, so those rows come out at or below old in instructions (web-NotreDame `-C -d -N10` −0.95% instr with −0.22% bits; its 19.0 → 19.3 s is a +1.3% wall delta against a −0.95% instr delta, i.e. noise on this loaded machine, as is every sub-2% time delta below). `-N1` rows pay the pass on top of one trial, and three sit over the 1% instr line: **web-NotreDame `-C -d -N1` +1.9% instr / +3.9% s (2.30 → 2.39 s) for −0.22% bits; powergrid `-N1` +1.8% instr / +1.7% s (0.0248 → 0.0252 s) for −0.51% bits; om4 `-d -N1` +1.1% instr / +1.1% s for no bit gain.** om4 is the one row without a gain: its winner is the one-level fallback, whose tree still carries the search's four levels under a single module (a pre-existing fallback inconsistency recorded in F54, fix proposed there), so the pass seeds and scores that tree and finds nothing that beats the one-level bar; with the fallback flattened the flat-winner skip removes the cost. Every other `-N1` row is within noise of old. The `--timing-json` key `dissolve_s` isolates the pass (web-NotreDame `-N1`: 0.045 s of 2.39 s).

### Per-feature attribution (`-N1` and `-N10` subset)

The PR is one behaviour (the dissolve, F53) plus four cost reductions on it (F54). Same rows, same session, one binary per commit; instructions retired against old. V0 is the pass as first implemented (F53's tip `f2dea4d3`); V1 makes its gain loop O(1) per candidate and its entropy-bias accounting exact; V2 decides on the pass's own accounting instead of a re-score; V3 compacts seed ids by (parent, element) pairs instead of prefix vectors; **V4 (this PR) runs the pass on the winning trial's own stack and splices the tree in place, stamping from the pass's terms.** Bits are identical across V0–V4 on every row.

| configuration | old bits | new bits | old instr | V0 | V1 | V2 | V3 | **V4** |
|---|--:|--:|--:|--:|--:|--:|--:|--:|
| web-NotreDame -d -N1 | 5.56852929 | 5.5564217 (-0.2174%) | 23.88G | +70.9% | +56.0% | +33.5% | +19.2% | **+1.9%** |
| om4 -d -N1 | 7.9829318 | 7.9829318 (=) | 7.63G | +81.3% | +19.5% | +3.9% | +1.0% | **+1.1%** |
| powergrid -N1 | 4.75504777 | 4.73085031 (-0.5089%) | 0.36G | +39.5% | +39.3% | +23.8% | +15.3% | **+1.8%** |
| science2001 -d -N1 | 7.8334366 | 7.80793717 (-0.3255%) | 5.69G | +6.1% | +6.0% | +3.8% | +3.3% | **-0.0%** |
| malaria -N1 | 7.50222401 | 7.49198036 (-0.1365%) | 5.29G | +2.4% | +2.3% | +1.4% | +1.3% | **-0.0%** |
| om8 -d -N1 | 6.99609633 | 6.96753576 (-0.4082%) | 7.49G | +26.8% | +24.8% | +15.1% | +8.5% | **-0.5%** |
| powergrid -N10 | 4.74107206 | 4.71776024 (-0.4917%) | 2.82G | +2.8% | +2.7% | +0.8% | -0.3% | **-0.5%** |
| web-NotreDame -d -N10 | 5.56852929 | 5.5564217 (-0.2174%) | 183.05G | +5.8% | +3.9% | +0.9% | -1.0% | **-0.9%** |
| science2001 -d -N10 | 7.8334366 | 7.80793717 (-0.3255%) | 34.05G | +0.6% | +0.6% | +0.3% | +0.2% | **-0.0%** |
| malaria -N10 | 7.39750171 | 7.39244259 (-0.0684%) | 37.49G | +0.3% | +0.3% | +0.1% | +0.1% | **+0.0%** |
| netsci -N10 | 4.05454025 | 4.04885795 (-0.1401%) | 0.31G | +2.1% | +0.3% | -1.1% | -1.8% | **-2.2%** |

Instructions retired vs old; interleaved, 2 reps for `-N1` rows (min), 1 for `-N10`. Instructions only: this subset was measured in its own pass, and the one time per configuration is the one in the tables below.

### Old vs new columnar — standard search (`-C -N10`)

Overlapping and wikispeedia rows run `-C -d -N10`. Every codelength that moves, moves down. At `-N10` the pass replaces the best-trial restore's re-materialization, so time on the rows where it fires is at or below old; a sub-2% time delta against a ~0% instr delta is noise.

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
<tr><td align="right">ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.371875026 (-0.4122%)</td><td align="right">0.001s (-6.5%)</td><td align="right">0.1G (-0.21%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.007s (+0.1%)</td><td align="right">0.1G (-0.38%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.054540245</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">4</td><td align="right">4.048857953 (-0.1401%)</td><td align="right">0.022s (-1.2%)</td><td align="right">0.3G (-0.41%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.741072056</td><td align="right">0.238s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.717760238 (-0.4917%)</td><td align="right">0.240s (+0.6%)</td><td align="right">2.8G (-0.53%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.058s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.059s (+0.5%)</td><td align="right">0.7G (+0.19%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.833436601</td><td align="right">2.99s</td><td align="right">34.1G</td><td align="right">15</td><td align="right">3</td><td align="right">7.807937174 (-0.3255%)</td><td align="right">2.93s (-1.8%)</td><td align="right">34.0G (-0.04%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.568529293</td><td align="right">19.0s</td><td align="right">183.0G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (-0.2174%)</td><td align="right">19.3s (+1.3%)</td><td align="right">181.3G (-0.95%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (+0.4%)</td><td align="right">0.1G (-6.06%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+37.7%)</td><td align="right">0.1G (-0.30%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.39750171</td><td align="right">3.01s</td><td align="right">37.5G</td><td align="right">2</td><td align="right">3</td><td align="right">7.392442593 (-0.0684%)</td><td align="right">3.02s (+0.5%)</td><td align="right">37.5G (+0.01%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392425413</td><td align="right">3.75s</td><td align="right">44.5G</td><td align="right">22</td><td align="right">3</td><td align="right">5.392285003 (-0.0026%)</td><td align="right">3.73s (-0.6%)</td><td align="right">44.5G (-0.05%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574746817</td><td align="right">4.09s</td><td align="right">46.5G</td><td align="right">21</td><td align="right">3</td><td align="right">5.574537176 (-0.0038%)</td><td align="right">4.05s (-0.9%)</td><td align="right">46.4G (-0.05%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.22s</td><td align="right">97.9G</td><td align="right">23</td><td align="right">3</td><td align="right">7.421664324 (-0.0066%)</td><td align="right">9.21s (-0.1%)</td><td align="right">97.8G (-0.04%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.31s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.34s (+0.7%)</td><td align="right">35.5G (-0.04%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">5.10s</td><td align="right">47.6G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">5.22s (+2.2%)</td><td align="right">47.5G (-0.17%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">8.46s</td><td align="right">83.2G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">8.46s (-0.0%)</td><td align="right">83.0G (-0.21%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">9.12s</td><td align="right">81.6G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">8.62s (-5.5%)</td><td align="right">81.4G (-0.24%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">9.07s</td><td align="right">83.3G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">9.24s (+1.8%)</td><td align="right">83.1G (-0.25%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.987473156</td><td align="right">10.3s</td><td align="right">85.9G</td><td align="right">203</td><td align="right">4</td><td align="right">6.959413622 (-0.4016%)</td><td align="right">9.76s (-5.0%)</td><td align="right">85.8G (-0.14%)</td><td align="right">203</td><td align="right">4</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.720s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.719s (-0.1%)</td><td align="right">7.2G (-0.16%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Overlapping and wikispeedia rows as `-C -2d -N10`. A two-level tree has no interior level to dissolve, so the pass is a no-op and every row is bit-identical; the seed compaction (V3) is the only code these rows run differently, in the `-N10` best restore.

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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-7.9%)</td><td align="right">0.1G (-0.40%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.861229775</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (=)</td><td align="right">0.007s (+8.5%)</td><td align="right">0.1G (-0.13%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.283072584</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.283072584 (=)</td><td align="right">0.009s (+7.9%)</td><td align="right">0.2G (-0.21%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.63729688</td><td align="right">0.100s</td><td align="right">1.2G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.098s (-1.7%)</td><td align="right">1.1G (-0.49%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739575295</td><td align="right">0.043s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">6.739575295 (=)</td><td align="right">0.043s (-1.3%)</td><td align="right">0.5G (-0.20%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.949978834</td><td align="right">2.32s</td><td align="right">23.9G</td><td align="right">506</td><td align="right">2</td><td align="right">7.949978834 (=)</td><td align="right">2.33s (+0.3%)</td><td align="right">23.8G (-0.04%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.754216663</td><td align="right">19.9s</td><td align="right">118.0G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.754216663 (=)</td><td align="right">19.5s (-2.0%)</td><td align="right">117.7G (-0.28%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-2.6%)</td><td align="right">0.1G (-6.27%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+23.8%)</td><td align="right">0.1G (-0.40%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.400445378</td><td align="right">2.79s</td><td align="right">32.3G</td><td align="right">168</td><td align="right">2</td><td align="right">7.400445378 (=)</td><td align="right">2.81s (+0.6%)</td><td align="right">32.3G (-0.00%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393055049</td><td align="right">3.85s</td><td align="right">41.9G</td><td align="right">334</td><td align="right">2</td><td align="right">5.393055049 (=)</td><td align="right">3.86s (+0.3%)</td><td align="right">41.9G (-0.02%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571539329</td><td align="right">3.94s</td><td align="right">41.2G</td><td align="right">304</td><td align="right">2</td><td align="right">5.571539329 (=)</td><td align="right">3.94s (+0.1%)</td><td align="right">41.2G (-0.01%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.424143707</td><td align="right">10.7s</td><td align="right">104.8G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.424143707 (=)</td><td align="right">10.9s (+1.9%)</td><td align="right">104.8G (+0.01%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.05s</td><td align="right">31.6G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.09s (+1.1%)</td><td align="right">31.6G (-0.04%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739271968</td><td align="right">3.93s</td><td align="right">39.8G</td><td align="right">638</td><td align="right">2</td><td align="right">6.739271968 (=)</td><td align="right">4.11s (+4.8%)</td><td align="right">39.8G (+0.02%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.866901786</td><td align="right">7.55s</td><td align="right">71.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">7.41s (-1.9%)</td><td align="right">71.6G (-0.11%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.866617805</td><td align="right">7.58s</td><td align="right">71.3G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">7.76s (+2.3%)</td><td align="right">71.3G (-0.05%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884862145</td><td align="right">7.92s</td><td align="right">71.1G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">7.92s (+0.0%)</td><td align="right">71.0G (-0.11%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.887234466</td><td align="right">8.88s</td><td align="right">75.5G</td><td align="right">921</td><td align="right">2</td><td align="right">6.887234466 (=)</td><td align="right">8.82s (-0.6%)</td><td align="right">75.4G (-0.12%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.907904741</td><td align="right">0.679s</td><td align="right">6.9G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.687s (+1.3%)</td><td align="right">6.9G (-0.01%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

Interleaved minimum of 3 per arm. The once-per-run pass is paid on top of a single trial here, on the winning trial's own stack (no seed) with the tree spliced in place (no rebuild).

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
<tr><td align="right">ninetriangles</td><td align="right">3.38583082</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.371875026 (-0.4122%)</td><td align="right">0.000s (+8.6%)</td><td align="right">0.1G (-0.48%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.899367957</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">2</td><td align="right">6.899367957 (=)</td><td align="right">0.001s (-6.5%)</td><td align="right">0.1G (-0.14%)</td><td align="right">11</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.064688363</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">4</td><td align="right">4</td><td align="right">4.047459862 (-0.4239%)</td><td align="right">0.003s (+3.5%)</td><td align="right">0.1G (+0.57%)</td><td align="right">7</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.75504777</td><td align="right">0.025s</td><td align="right">0.4G</td><td align="right">4</td><td align="right">5</td><td align="right">4.730850312 (-0.5089%)</td><td align="right">0.025s (+1.7%)</td><td align="right">0.4G (+1.80%)</td><td align="right">12</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.758421601</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">2</td><td align="right">3</td><td align="right">6.758265349 (-0.0023%)</td><td align="right">0.009s (-0.6%)</td><td align="right">0.2G (+0.53%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.833436601</td><td align="right">0.397s</td><td align="right">5.7G</td><td align="right">15</td><td align="right">3</td><td align="right">7.807937174 (-0.3255%)</td><td align="right">0.395s (-0.5%)</td><td align="right">5.7G (+0.10%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.568529293</td><td align="right">2.30s</td><td align="right">23.9G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (-0.2174%)</td><td align="right">2.39s (+3.9%)</td><td align="right">24.3G (+1.88%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.041117399</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.041117399 (=)</td><td align="right">0.001s (-7.2%)</td><td align="right">0.1G (-1.65%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-12.5%)</td><td align="right">0.1G (-0.03%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502224007</td><td align="right">0.362s</td><td align="right">5.3G</td><td align="right">8</td><td align="right">3</td><td align="right">7.491980364 (-0.1365%)</td><td align="right">0.357s (-1.3%)</td><td align="right">5.3G (+0.00%)</td><td align="right">148</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.473231053</td><td align="right">0.355s</td><td align="right">4.5G</td><td align="right">22</td><td align="right">3</td><td align="right">5.470440768 (-0.0510%)</td><td align="right">0.352s (-0.7%)</td><td align="right">4.5G (-0.06%)</td><td align="right">242</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.668390329</td><td align="right">0.480s</td><td align="right">5.9G</td><td align="right">20</td><td align="right">3</td><td align="right">5.657913279 (-0.1848%)</td><td align="right">0.463s (-3.6%)</td><td align="right">5.9G (-0.47%)</td><td align="right">197</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.580748978</td><td align="right">0.861s</td><td align="right">9.3G</td><td align="right">43</td><td align="right">3</td><td align="right">7.546335898 (-0.4540%)</td><td align="right">0.872s (+1.2%)</td><td align="right">9.2G (-0.34%)</td><td align="right">1614</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.460796773</td><td align="right">0.406s</td><td align="right">5.8G</td><td align="right">5</td><td align="right">3</td><td align="right">8.460796773 (=)</td><td align="right">0.402s (-0.9%)</td><td align="right">5.8G (-0.01%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739358212</td><td align="right">1.56s</td><td align="right">17.3G</td><td align="right">674</td><td align="right">2</td><td align="right">6.739358212 (=)</td><td align="right">1.54s (-1.2%)</td><td align="right">17.3G (-0.08%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.861724654</td><td align="right">1.60s</td><td align="right">17.6G</td><td align="right">141</td><td align="right">2</td><td align="right">6.861724654 (=)</td><td align="right">1.61s (+0.9%)</td><td align="right">17.5G (-0.32%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.868127142</td><td align="right">1.82s</td><td align="right">19.2G</td><td align="right">293</td><td align="right">2</td><td align="right">6.868127142 (=)</td><td align="right">1.83s (+0.6%)</td><td align="right">19.1G (-0.30%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884042436</td><td align="right">1.80s</td><td align="right">18.0G</td><td align="right">455</td><td align="right">2</td><td align="right">6.884042436 (=)</td><td align="right">1.80s (-0.0%)</td><td align="right">18.0G (-0.38%)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.893377041</td><td align="right">2.31s</td><td align="right">22.0G</td><td align="right">911</td><td align="right">2</td><td align="right">6.893377041 (=)</td><td align="right">2.31s (+0.1%)</td><td align="right">22.0G (-0.03%)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.317213189</td><td align="right">0.364s</td><td align="right">3.7G</td><td align="right">75</td><td align="right">4</td><td align="right">7.29196634 (-0.3450%)</td><td align="right">0.364s (+0.0%)</td><td align="right">3.7G (-0.49%)</td><td align="right">321</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">7.9829318</td><td align="right">0.751s</td><td align="right">7.6G</td><td align="right">1</td><td align="right">4</td><td align="right">7.9829318 (=)</td><td align="right">0.759s (+1.1%)</td><td align="right">7.7G (+1.09%)</td><td align="right">1</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.884811284</td><td align="right">0.759s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.812252898 (-0.9202%)</td><td align="right">0.784s (+3.3%)</td><td align="right">7.2G (-0.13%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.479374707</td><td align="right">0.812s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.440161581 (-0.5243%)</td><td align="right">0.802s (-1.3%)</td><td align="right">7.5G (-0.37%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.996096327</td><td align="right">0.809s</td><td align="right">7.5G</td><td align="right">206</td><td align="right">4</td><td align="right">6.967535764 (-0.4082%)</td><td align="right">0.815s (+0.6%)</td><td align="right">7.4G (-0.40%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om2 `-2d -c` planted</td><td align="right">6.744721993</td><td align="right">0.566s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.744721993 (=)</td><td align="right">0.559s (-1.3%)</td><td align="right">6.2G (-0.15%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d -c` planted</td><td align="right">6.856239474</td><td align="right">0.930s</td><td align="right">10.4G</td><td align="right">132</td><td align="right">2</td><td align="right">6.856239474 (=)</td><td align="right">0.921s (-1.0%)</td><td align="right">10.3G (-0.50%)</td><td align="right">132</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d -c` planted</td><td align="right">6.857778113</td><td align="right">1.20s</td><td align="right">12.6G</td><td align="right">296</td><td align="right">2</td><td align="right">6.857778113 (=)</td><td align="right">1.16s (-3.8%)</td><td align="right">12.5G (-0.48%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d -c` planted</td><td align="right">6.873378755</td><td align="right">1.27s</td><td align="right">13.3G</td><td align="right">446</td><td align="right">2</td><td align="right">6.873378755 (=)</td><td align="right">1.26s (-0.6%)</td><td align="right">13.2G (-0.52%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d -c` planted</td><td align="right">6.875237392</td><td align="right">1.43s</td><td align="right">14.2G</td><td align="right">894</td><td align="right">2</td><td align="right">6.875237392 (=)</td><td align="right">1.42s (-0.8%)</td><td align="right">14.2G (-0.50%)</td><td align="right">894</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.089s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.094s (+5.1%)</td><td align="right">1.0G (-0.33%)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">6.113125871</td><td align="right">0.072s</td><td align="right">0.8G</td><td align="right">46</td><td align="right">3</td><td align="right">6.066305904 (-0.7659%)</td><td align="right">0.069s (-3.9%)</td><td align="right">0.8G (-1.50%)</td><td align="right">187</td><td align="right">3</td></tr>
</tbody>
</table>

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
<tr><td align="right">om2 `-2d --regularized -N1`</td><td align="right">7.548816177</td><td align="right">0.774s</td><td align="right">8.2G</td><td align="right">120</td><td align="right">2</td><td align="right">7.548816177 (=)</td><td align="right">0.785s (+1.5%)</td><td align="right">8.2G (-0.07%)</td><td align="right">120</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">0.950s</td><td align="right">9.3G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">0.987s (+3.8%)</td><td align="right">9.3G (-0.51%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N1`</td><td align="right">7.966995214</td><td align="right">2.86s</td><td align="right">26.7G</td><td align="right">104</td><td align="right">2</td><td align="right">7.966995214 (=)</td><td align="right">2.92s (+2.1%)</td><td align="right">26.6G (-0.16%)</td><td align="right">104</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N1`</td><td align="right">7.981574549</td><td align="right">1.84s</td><td align="right">15.9G</td><td align="right">113</td><td align="right">2</td><td align="right">7.981574549 (=)</td><td align="right">1.84s (+0.1%)</td><td align="right">15.8G (-0.23%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N1`</td><td align="right">7.978912396</td><td align="right">2.78s</td><td align="right">22.6G</td><td align="right">234</td><td align="right">2</td><td align="right">7.978912396 (=)</td><td align="right">2.61s (-6.2%)</td><td align="right">22.5G (-0.26%)</td><td align="right">234</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.73s</td><td align="right">25.1G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.76s (+1.1%)</td><td align="right">25.1G (-0.06%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N10`</td><td align="right">7.556894677</td><td align="right">5.87s</td><td align="right">49.2G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">5.61s (-4.4%)</td><td align="right">49.1G (-0.13%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N10`</td><td align="right">7.967531078</td><td align="right">6.42s</td><td align="right">55.2G</td><td align="right">98</td><td align="right">2</td><td align="right">7.967531078 (=)</td><td align="right">6.38s (-0.5%)</td><td align="right">55.1G (-0.10%)</td><td align="right">98</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N10`</td><td align="right">7.981574549</td><td align="right">6.22s</td><td align="right">51.0G</td><td align="right">113</td><td align="right">2</td><td align="right">7.981574549 (=)</td><td align="right">6.19s (-0.5%)</td><td align="right">51.0G (-0.14%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N10`</td><td align="right">7.976140205</td><td align="right">8.83s</td><td align="right">70.0G</td><td align="right">240</td><td align="right">2</td><td align="right">7.976140205 (=)</td><td align="right">8.65s (-2.1%)</td><td align="right">69.9G (-0.11%)</td><td align="right">240</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N1`</td><td align="right">7.970508085</td><td align="right">0.474s</td><td align="right">5.0G</td><td align="right">1</td><td align="right">3</td><td align="right">7.970508085 (=)</td><td align="right">0.478s (+0.9%)</td><td align="right">5.0G (+0.60%)</td><td align="right">1</td><td align="right">3</td></tr>
<tr><td align="right">om4 `-d --regularized -N1`</td><td align="right">7.9828492</td><td align="right">0.888s</td><td align="right">8.6G</td><td align="right">1</td><td align="right">3</td><td align="right">7.9828492 (=)</td><td align="right">0.915s (+3.0%)</td><td align="right">8.7G (+0.71%)</td><td align="right">1</td><td align="right">3</td></tr>
<tr><td align="right">om5 `-d --regularized -N1`</td><td align="right">7.989613065</td><td align="right">0.864s</td><td align="right">8.1G</td><td align="right">1</td><td align="right">3</td><td align="right">7.989613065 (=)</td><td align="right">0.873s (+1.1%)</td><td align="right">8.2G (+0.70%)</td><td align="right">1</td><td align="right">3</td></tr>
<tr><td align="right">om6 `-d --regularized -N1`</td><td align="right">7.993490371</td><td align="right">0.991s</td><td align="right">9.2G</td><td align="right">1</td><td align="right">3</td><td align="right">7.993490371 (=)</td><td align="right">0.976s (-1.5%)</td><td align="right">9.2G (+0.69%)</td><td align="right">1</td><td align="right">3</td></tr>
<tr><td align="right">om8 `-d --regularized -N1`</td><td align="right">7.994735672</td><td align="right">1.16s</td><td align="right">10.7G</td><td align="right">1</td><td align="right">3</td><td align="right">7.994735672 (=)</td><td align="right">1.18s (+1.3%)</td><td align="right">10.8G (+0.59%)</td><td align="right">1</td><td align="right">3</td></tr>
<tr><td align="right">om2 `-d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">4.49s</td><td align="right">43.3G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">4.23s (-5.8%)</td><td align="right">43.2G (-0.19%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N10`</td><td align="right">7.556653713</td><td align="right">9.57s</td><td align="right">92.7G</td><td align="right">78</td><td align="right">2</td><td align="right">7.556653713 (=)</td><td align="right">9.42s (-1.5%)</td><td align="right">92.6G (-0.14%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N10`</td><td align="right">7.965009721</td><td align="right">11.7s</td><td align="right">107.7G</td><td align="right">106</td><td align="right">2</td><td align="right">7.965009721 (=)</td><td align="right">11.7s (-0.3%)</td><td align="right">107.5G (-0.12%)</td><td align="right">106</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N10`</td><td align="right">7.981063575</td><td align="right">12.4s</td><td align="right">109.9G</td><td align="right">119</td><td align="right">2</td><td align="right">7.981063575 (=)</td><td align="right">12.1s (-2.0%)</td><td align="right">109.8G (-0.15%)</td><td align="right">119</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N10`</td><td align="right">7.976681139</td><td align="right">10.1s</td><td align="right">85.0G</td><td align="right">256</td><td align="right">2</td><td align="right">7.976681139 (=)</td><td align="right">9.99s (-0.8%)</td><td align="right">84.8G (-0.22%)</td><td align="right">256</td><td align="right">2</td></tr>
<tr><td align="right">om2 planted, `--no-infomap -c`</td><td align="right">6.789039995</td><td align="right">0.057s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.789039995 (=)</td><td align="right">0.054s (-5.1%)</td><td align="right">0.6G (-2.56%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om4 planted, `--no-infomap -c`</td><td align="right">6.880650147</td><td align="right">0.110s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.880650147 (=)</td><td align="right">0.112s (+1.5%)</td><td align="right">1.0G (-2.69%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td align="right">om5 planted, `--no-infomap -c`</td><td align="right">6.902222527</td><td align="right">0.125s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.902222527 (=)</td><td align="right">0.125s (-0.5%)</td><td align="right">1.1G (-2.70%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td align="right">om6 planted, `--no-infomap -c`</td><td align="right">6.930934993</td><td align="right">0.135s</td><td align="right">1.2G</td><td align="right">24</td><td align="right">2</td><td align="right">6.930934993 (=)</td><td align="right">0.129s (-4.6%)</td><td align="right">1.1G (-2.98%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td align="right">om8 planted, `--no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.141s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.141s (+0.4%)</td><td align="right">1.2G (-3.18%)</td><td align="right">32</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-5.9%)</td><td align="right">0.1G (-2.25%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (-3.4%)</td><td align="right">0.1G (-2.12%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.03324474 (-0.3856%)</td><td align="right">0.014s (-36.3%)</td><td align="right">0.2G (-29.33%)</td><td align="right">9</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.240s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.754013143 (+0.7684%)</td><td align="right">0.147s (-38.7%)</td><td align="right">1.7G (-37.73%)</td><td align="right">10</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.059s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.057s (-3.1%)</td><td align="right">0.7G (-4.22%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">2.93s</td><td align="right">34.0G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">2.93s (-0.1%)</td><td align="right">33.0G (-2.98%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">19.3s</td><td align="right">181.3G</td><td align="right">5</td><td align="right">6</td><td align="right">5.620539396 (+1.1539%)</td><td align="right">15.3s (-20.9%)</td><td align="right">126.4G (-30.30%)</td><td align="right">135</td><td align="right">5</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-7.4%)</td><td align="right">0.1G (-3.06%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-28.6%)</td><td align="right">0.1G (-0.63%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">3.02s</td><td align="right">37.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">3.15s (+4.1%)</td><td align="right">36.6G (-2.23%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.73s</td><td align="right">44.5G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.89s (+4.4%)</td><td align="right">41.8G (-6.01%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.05s</td><td align="right">46.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.90s (-3.8%)</td><td align="right">41.3G (-11.02%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.21s</td><td align="right">97.8G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">9.84s (+6.8%)</td><td align="right">95.3G (-2.55%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.34s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.21s (-3.7%)</td><td align="right">34.7G (-2.24%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.719s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.693s (-3.7%)</td><td align="right">6.9G (-4.01%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### The non-redundant map equation L\* (`--non-redundant`)

L\* is a different objective, so a lower number is not a better partition of the same objective. Both columns are the new binary. Under L\* the pass's base-estimated proposal is re-scored on L\* and kept only if lower — and it is, on 4 of these 15 rows, read against the sync snapshot's L\* table (this session has no old L\* arm): **web-NotreDame 5.517073626 → 5.512433077 (−0.084% bits, 184.1G → 182.5G instr), air30k (meta) 7.215299774 → 7.192724425 (−0.313%, 85.8G → 85.9G), air30k (reg.) −0.009% and air30k −0.002% at unchanged instr**; the other 11 are the equal-depth stack's own L\*, unchanged. F53's log entry called L\* "never-worse (SAME)"; SAME was wrong (its A/B had no old L\* arm to compare against), never-worse holds.

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.078067323 (-8.7135%)</td><td align="right">0.001s (+10.6%)</td><td align="right">0.1G (+0.98%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367 (+0.0797%)</td><td align="right">0.006s (-3.4%)</td><td align="right">0.1G (+0.78%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">3.892209764 (-3.8689%)</td><td align="right">0.023s (+2.0%)</td><td align="right">0.3G (+1.40%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.240s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.509265423 (-4.4194%)</td><td align="right">0.251s (+4.5%)</td><td align="right">2.9G (+3.44%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.059s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502 (+0.7165%)</td><td align="right">0.060s (+2.8%)</td><td align="right">0.7G (+1.88%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">2.93s</td><td align="right">34.0G</td><td align="right">189</td><td align="right">3</td><td align="right">8.009172258 (+2.5773%)</td><td align="right">2.84s (-3.2%)</td><td align="right">31.1G (-8.69%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">19.3s</td><td align="right">181.3G</td><td align="right">5</td><td align="right">6</td><td align="right">5.512433077 (-0.7917%)</td><td align="right">20.5s (+6.1%)</td><td align="right">182.5G (+0.69%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653 (-0.8182%)</td><td align="right">0.004s (-4.9%)</td><td align="right">0.1G (-3.29%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (-4.1040%)</td><td align="right">0.000s (-32.8%)</td><td align="right">0.1G (-1.61%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">3.02s</td><td align="right">37.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.427572783 (+0.4752%)</td><td align="right">3.23s (+6.9%)</td><td align="right">37.6G (+0.18%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.73s</td><td align="right">44.5G</td><td align="right">257</td><td align="right">3</td><td align="right">5.378824196 (-0.2496%)</td><td align="right">4.00s (+7.3%)</td><td align="right">43.9G (-1.31%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.05s</td><td align="right">46.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.56659036 (-0.1426%)</td><td align="right">4.36s (+7.6%)</td><td align="right">45.9G (-1.09%)</td><td align="right">220</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">9.21s</td><td align="right">97.8G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.192724425 (-3.0848%)</td><td align="right">8.69s (-5.7%)</td><td align="right">85.9G (-12.20%)</td><td align="right">1910</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.34s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451 (+2.5761%)</td><td align="right">3.31s (-0.9%)</td><td align="right">35.4G (-0.44%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.719s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.903208727 (-0.0795%)</td><td align="right">0.691s (-4.0%)</td><td align="right">6.9G (-4.42%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar

**Both arms on the new binary.** The OO arm carries #1075 (the sync); the columnar arm now carries this PR's pass, so the two engines' dissolves face each other. air30k (meta) OO is `-N1` (it does not finish `-N10` in budget).

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">object-oriented (this PR)</th>
<th colspan="5">columnar <code>-C</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-81.1%)</td><td align="right">0.1G (-38.92%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">5</td><td align="right">2</td><td align="right">6.862755928 (-0.0042%)</td><td align="right">0.007s (-71.0%)</td><td align="right">0.1G (-56.35%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.026557116</td><td align="right">0.121s</td><td align="right">1.3G</td><td align="right">12</td><td align="right">5</td><td align="right">4.048857953 (+0.5538%)</td><td align="right">0.022s (-81.5%)</td><td align="right">0.3G (-77.05%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.736412597</td><td align="right">1.92s</td><td align="right">20.2G</td><td align="right">11</td><td align="right">6</td><td align="right">4.717760238 (-0.3938%)</td><td align="right">0.240s (-87.5%)</td><td align="right">2.8G (-86.13%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.738927979</td><td align="right">0.137s</td><td align="right">1.4G</td><td align="right">80</td><td align="right">3</td><td align="right">6.740943136 (+0.0299%)</td><td align="right">0.059s (-57.3%)</td><td align="right">0.7G (-49.34%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.805465772</td><td align="right">8.02s</td><td align="right">63.0G</td><td align="right">220</td><td align="right">4</td><td align="right">7.807937174 (+0.0317%)</td><td align="right">2.93s (-63.4%)</td><td align="right">34.0G (-45.99%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.55442136</td><td align="right">151.9s</td><td align="right">1114.9G</td><td align="right">766</td><td align="right">9</td><td align="right">5.556421705 (+0.0360%)</td><td align="right">19.3s (-87.3%)</td><td align="right">181.3G (-83.74%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-69.7%)</td><td align="right">0.1G (-45.99%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-29.1%)</td><td align="right">0.1G (-6.29%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502028585</td><td align="right">9.25s</td><td align="right">78.1G</td><td align="right">144</td><td align="right">3</td><td align="right">7.392442593 (-1.4608%)</td><td align="right">3.02s (-67.3%)</td><td align="right">37.5G (-51.99%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392441014</td><td align="right">12.0s</td><td align="right">120.6G</td><td align="right">251</td><td align="right">3</td><td align="right">5.392285003 (-0.0029%)</td><td align="right">3.73s (-69.0%)</td><td align="right">44.5G (-63.12%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578435633</td><td align="right">7.76s</td><td align="right">81.7G</td><td align="right">301</td><td align="right">3</td><td align="right">5.574537176 (-0.0699%)</td><td align="right">4.05s (-47.8%)</td><td align="right">46.4G (-43.13%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">8.432467909</td><td align="right">5.52s</td><td align="right">52.0G</td><td align="right">114</td><td align="right">4</td><td align="right">7.421664324 (-11.9870%)</td><td align="right">9.21s (+66.7%)</td><td align="right">97.8G (+88.00%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">7.938575228</td><td align="right">7.16s</td><td align="right">57.0G</td><td align="right">25</td><td align="right">4</td><td align="right">8.235585529 (+3.7414%)</td><td align="right">3.34s (-53.4%)</td><td align="right">35.5G (-37.68%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar — two-level (`-2`)

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="5">object-oriented (this PR)</th>
<th colspan="5">columnar <code>-C -2</code> (this PR)</th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>instr</th><th>top</th><th>lvls</th>
</tr></thead><tbody>
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-64.2%)</td><td align="right">0.1G (-13.85%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">5</td><td align="right">2</td><td align="right">6.861229775 (-0.0265%)</td><td align="right">0.007s (-40.1%)</td><td align="right">0.1G (-27.41%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.285012668</td><td align="right">0.029s</td><td align="right">0.4G</td><td align="right">56</td><td align="right">2</td><td align="right">4.283072584 (-0.0453%)</td><td align="right">0.009s (-67.5%)</td><td align="right">0.2G (-56.19%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.600443859</td><td align="right">0.554s</td><td align="right">5.9G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.098s (-82.3%)</td><td align="right">1.1G (-80.49%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739721413</td><td align="right">0.076s</td><td align="right">0.8G</td><td align="right">80</td><td align="right">2</td><td align="right">6.739575295 (-0.0022%)</td><td align="right">0.043s (-43.6%)</td><td align="right">0.5G (-33.78%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.9500396</td><td align="right">3.66s</td><td align="right">29.2G</td><td align="right">496</td><td align="right">2</td><td align="right">7.949978834 (-0.0008%)</td><td align="right">2.33s (-36.4%)</td><td align="right">23.8G (-18.36%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.742988533</td><td align="right">40.1s</td><td align="right">251.3G</td><td align="right">11809</td><td align="right">2</td><td align="right">6.754216663 (+0.1665%)</td><td align="right">19.5s (-51.3%)</td><td align="right">117.7G (-53.19%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-48.7%)</td><td align="right">0.1G (-27.02%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+17.2%)</td><td align="right">0.1G (-2.86%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.50595639</td><td align="right">6.10s</td><td align="right">55.2G</td><td align="right">142</td><td align="right">2</td><td align="right">7.400445378 (-1.4057%)</td><td align="right">2.81s (-54.0%)</td><td align="right">32.3G (-41.55%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393312779</td><td align="right">4.30s</td><td align="right">43.9G</td><td align="right">332</td><td align="right">2</td><td align="right">5.393055049 (-0.0048%)</td><td align="right">3.86s (-10.2%)</td><td align="right">41.9G (-4.64%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.579216889</td><td align="right">5.45s</td><td align="right">58.6G</td><td align="right">301</td><td align="right">2</td><td align="right">5.571539329 (-0.1376%)</td><td align="right">3.94s (-27.8%)</td><td align="right">41.2G (-29.64%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.131110023</td><td align="right">5.07s</td><td align="right">36.8G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (+1.2849%)</td><td align="right">3.09s (-39.1%)</td><td align="right">31.6G (-14.14%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>
