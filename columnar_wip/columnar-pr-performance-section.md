## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`); `time` is `--timing-json`'s `timing.total_s`. One run per `-N10` row (deterministic; `instr` carries the comparison); interleaved minimum of 3 for `-N1` rows. Driver and every row: [`columnar_wip/bench-dissolve.py`](columnar_wip/bench-dissolve.py), [`columnar_wip/dissolve-ab-results.tsv`](columnar_wip/dissolve-ab-results.tsv).

> **This PR is the columnar dissolve of unprofitable levels** (#1074, second track; the columnar counterpart of the OO #1075 that the sync #1077 brings in). The equal-depth columnar search cannot represent a ragged optimum, so a terminal pass on the settled winner removes every intermediate module-of-modules whose index codebook no longer pays for itself, best-first, on the pass's own exact accounting for every objective but L\* (L\* re-scores). Runs once per run after the deep repair, never per trial; guarded off under `--preferred-number-of-modules`; a no-op on `-2`. **Every codelength change below is on the columnar arm and is a gain**; the OO arm is untouched (it already has #1075) and is re-measured here only so that every table comes from one session.

> **Old** = a fresh `MODE=release OPENMP=0` build of the sync branch `sync-master-into-columnar-1076` tip `57a25a92` (the base this PR stacks on), md5 `533d4455993129f42ffc6ca2ff79265e`; **new** = this PR's head `8ab3d39c`, md5 `b0b878cca7afa6976779d02cb3851fb1`. Both arms interleaved in one session. The machine carried a desktop load (Chrome, VS Code; load average 6–15) throughout, so wall times sit above the sync snapshot's on both arms; the old-vs-new comparison is unaffected and `instr` is load-independent.

### What the change moves

Every configuration where old and new differ in bits, both arms. Lower is better; every move is on the columnar arm and every move is down.

| network | table | old bits | new bits | Δbits | old instr | new instr | Δinstr |
|---|---|--:|--:|--:|--:|--:|--:|
| ninetriangles | `-C` | 3.38583082 | **3.371875026** | **-0.4122%** | 0.1G | 0.1G | +0.18% |
| netscicoauthor2010 | `-C` | 4.054540245 | **4.048857953** | **-0.1401%** | 0.3G | 0.3G | -0.24% |
| powergrid | `-C` | 4.741072056 | **4.717760238** | **-0.4917%** | 2.8G | 2.8G | -0.52% |
| science2001 | `-C` | 7.833436601 | **7.807937174** | **-0.3255%** | 34.1G | 34.1G | -0.03% |
| web-NotreDame | `-C` | 5.568529293 | **5.556421705** | **-0.2174%** | 183.1G | 181.4G | -0.90% |
| malaria | `-C` | 7.39750171 | **7.392442593** | **-0.0684%** | 37.5G | 37.5G | -0.01% |
| air30k | `-C` | 5.392425413 | **5.392285003** | **-0.0026%** | 44.5G | 44.5G | -0.07% |
| air30k (reg.) | `-C` | 5.574746817 | **5.574537176** | **-0.0038%** | 46.5G | 46.5G | -0.04% |
| air30k (meta) | `-C` | 7.42215327 | **7.421664324** | **-0.0066%** | 97.9G | 97.9G | -0.04% |
| overlapping om8 `-d` | `-C` | 6.987473156 | **6.959413622** | **-0.4016%** | 85.8G | 85.8G | -0.03% |
| ninetriangles | `-C -N1` | 3.38583082 | **3.371875026** | **-0.4122%** | 0.1G | 0.1G | -0.80% |
| netscicoauthor2010 | `-C -N1` | 4.064688363 | **4.047459862** | **-0.4239%** | 0.1G | 0.1G | +0.37% |
| powergrid | `-C -N1` | 4.75504777 | **4.730850312** | **-0.5089%** | 0.4G | 0.4G | +1.83% |
| politicalblogs | `-C -N1` | 6.758421601 | **6.758265349** | **-0.0023%** | 0.2G | 0.2G | +0.52% |
| science2001 | `-C -N1` | 7.833436601 | **7.807937174** | **-0.3255%** | 5.7G | 5.7G | +0.06% |
| web-NotreDame | `-C -N1` | 5.568529293 | **5.556421705** | **-0.2174%** | 23.9G | 24.3G | +1.88% |
| malaria | `-C -N1` | 7.502224007 | **7.491980364** | **-0.1365%** | 5.3G | 5.3G | +0.00% |
| air30k | `-C -N1` | 5.473231053 | **5.470440768** | **-0.0510%** | 4.5G | 4.5G | -0.02% |
| air30k (reg.) | `-C -N1` | 5.668390329 | **5.657913279** | **-0.1848%** | 5.9G | 5.9G | -0.41% |
| air30k (meta) | `-C -N1` | 7.580748978 | **7.546335898** | **-0.4540%** | 9.3G | 9.2G | -0.40% |
| overlapping om2 `-d` | `-C -N1` | 7.317213189 | **7.29196634** | **-0.3450%** | 3.7G | 3.7G | -0.42% |
| overlapping om5 `-d` | `-C -N1` | 7.884811284 | **7.812252898** | **-0.9202%** | 7.2G | 7.2G | -0.12% |
| overlapping om6 `-d` | `-C -N1` | 7.479374707 | **7.440161581** | **-0.5243%** | 7.5G | 7.5G | -0.14% |
| overlapping om8 `-d` | `-C -N1` | 6.996096327 | **6.967535764** | **-0.4082%** | 7.5G | 7.4G | -0.46% |
| wikispeedia `-d` | `-C -N1` | 6.113125871 | **6.066305904** | **-0.7659%** | 0.8G | 0.8G | -1.47% |

> **Time.** The pass is paid once per run, so `-N10` rows carry it at a tenth of the weight of `-N1` rows — and at `-N10` it *replaces* the best-trial restore's re-materialization, so those rows come out at or below old in instructions (web-NotreDame `-C -d -N10` −0.90% instr, 20.40 → 20.44 s, with −0.22% bits; a sub-2% wall delta against a sub-1% instr delta is noise on this loaded machine, here and in every table below). `-N1` rows pay the pass on top of one trial, and two sit over the 1% instr line, both with a bit gain: **web-NotreDame `-C -d -N1` +1.9% instr / +2.5% s (2.3 → 2.36 s) for −0.22% bits; powergrid `-N1` +1.8% instr / +3.3% s (0.025 → 0.0259 s) for −0.51% bits.** om4 `-d -N1`, the one row where the pass finds nothing, is now *below* old (-6.8% instr, 7.62G → 7.11G, 0.746 → 0.693 s): its winner is the one-level fallback, whose tree the columnar path used to leave four levels deep under a single module (a pre-existing bug fixed in this PR, F54), so the flat-winner skip spares the pass and the per-level statistics and writer see a two-level tree. Every other `-N1` row is within noise of old. The `--timing-json` key `dissolve_s` isolates the pass (web-NotreDame `-N1`: 0.045 s of 2.36 s).

### Per-feature attribution (`-N1` and `-N10` subset)

The PR is one behaviour (the dissolve, F53) plus four cost reductions on it (F54). Same rows, same session, one binary per commit; instructions retired against old. V0 is the pass as first implemented (F53's tip `f2dea4d3`); V1 makes its gain loop O(1) per candidate and its entropy-bias accounting exact; V2 decides on the pass's own accounting instead of a re-score; V3 compacts seed ids by (parent, element) pairs instead of prefix vectors; **V4 runs the pass on the winning trial's own stack and splices the tree in place, stamping from the pass's terms.** This PR is V4 plus two commits that change no row of this subset except om4: the pass is skipped on a flat winner, and the one-level fallback writes a flat tree. Bits are identical across V0–V4 on every row.

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
<tr><td align="right">ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.371875026 (-0.4122%)</td><td align="right">0.001s (+17.4%)</td><td align="right">0.1G (+0.18%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.007s (-3.9%)</td><td align="right">0.1G (-0.45%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.054540245</td><td align="right">0.061s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">4</td><td align="right">4.048857953 (-0.1401%)</td><td align="right">0.024s (-60.3%)</td><td align="right">0.3G (-0.24%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.741072056</td><td align="right">0.246s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.717760238 (-0.4917%)</td><td align="right">0.244s (-0.8%)</td><td align="right">2.8G (-0.52%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.059s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.059s (+0.5%)</td><td align="right">0.7G (-0.08%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.833436601</td><td align="right">3.10s</td><td align="right">34.1G</td><td align="right">15</td><td align="right">3</td><td align="right">7.807937174 (-0.3255%)</td><td align="right">3.08s (-0.8%)</td><td align="right">34.1G (-0.03%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.568529293</td><td align="right">20.4s</td><td align="right">183.1G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (-0.2174%)</td><td align="right">20.4s (+0.2%)</td><td align="right">181.4G (-0.90%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (+2.2%)</td><td align="right">0.1G (-6.13%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+39.5%)</td><td align="right">0.1G (-0.20%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.39750171</td><td align="right">3.23s</td><td align="right">37.5G</td><td align="right">2</td><td align="right">3</td><td align="right">7.392442593 (-0.0684%)</td><td align="right">3.21s (-0.4%)</td><td align="right">37.5G (-0.01%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392425413</td><td align="right">4.14s</td><td align="right">44.5G</td><td align="right">22</td><td align="right">3</td><td align="right">5.392285003 (-0.0026%)</td><td align="right">4.13s (-0.2%)</td><td align="right">44.5G (-0.07%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574746817</td><td align="right">4.40s</td><td align="right">46.5G</td><td align="right">21</td><td align="right">3</td><td align="right">5.574537176 (-0.0038%)</td><td align="right">4.45s (+1.2%)</td><td align="right">46.5G (-0.04%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.42215327</td><td align="right">10.1s</td><td align="right">97.9G</td><td align="right">23</td><td align="right">3</td><td align="right">7.421664324 (-0.0066%)</td><td align="right">10.2s (+0.8%)</td><td align="right">97.9G (-0.04%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.23s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.23s (+0.2%)</td><td align="right">35.5G (-0.03%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.49s</td><td align="right">47.5G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">4.54s (+1.1%)</td><td align="right">47.5G (-0.06%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">8.06s</td><td align="right">83.1G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">7.76s (-3.6%)</td><td align="right">80.7G (-2.94%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">8.68s</td><td align="right">81.6G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">8.23s (-5.2%)</td><td align="right">81.4G (-0.27%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">8.70s</td><td align="right">83.3G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">8.70s (-0.0%)</td><td align="right">83.1G (-0.26%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.987473156</td><td align="right">8.81s</td><td align="right">85.8G</td><td align="right">203</td><td align="right">4</td><td align="right">6.959413622 (-0.4016%)</td><td align="right">9.30s (+5.5%)</td><td align="right">85.8G (-0.03%)</td><td align="right">203</td><td align="right">4</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.693s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.701s (+1.1%)</td><td align="right">7.2G (-0.13%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-8.5%)</td><td align="right">0.1G (-0.67%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.861229775</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (=)</td><td align="right">0.006s (-1.9%)</td><td align="right">0.1G (-0.32%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.283072584</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.283072584 (=)</td><td align="right">0.009s (-0.4%)</td><td align="right">0.2G (-0.45%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.63729688</td><td align="right">0.098s</td><td align="right">1.2G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.096s (-1.9%)</td><td align="right">1.1G (-0.49%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739575295</td><td align="right">0.042s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">6.739575295 (=)</td><td align="right">0.042s (-0.5%)</td><td align="right">0.5G (-0.23%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.949978834</td><td align="right">2.26s</td><td align="right">23.8G</td><td align="right">506</td><td align="right">2</td><td align="right">7.949978834 (=)</td><td align="right">2.27s (+0.8%)</td><td align="right">23.8G (+0.00%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.754216663</td><td align="right">19.1s</td><td align="right">117.8G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.754216663 (=)</td><td align="right">19.3s (+1.2%)</td><td align="right">117.4G (-0.35%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-0.8%)</td><td align="right">0.1G (-6.27%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+48.2%)</td><td align="right">0.1G (-0.14%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.400445378</td><td align="right">2.73s</td><td align="right">32.3G</td><td align="right">168</td><td align="right">2</td><td align="right">7.400445378 (=)</td><td align="right">2.69s (-1.4%)</td><td align="right">32.3G (-0.01%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393055049</td><td align="right">3.63s</td><td align="right">41.9G</td><td align="right">334</td><td align="right">2</td><td align="right">5.393055049 (=)</td><td align="right">3.62s (-0.3%)</td><td align="right">41.9G (-0.03%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571539329</td><td align="right">3.76s</td><td align="right">41.2G</td><td align="right">304</td><td align="right">2</td><td align="right">5.571539329 (=)</td><td align="right">3.73s (-0.8%)</td><td align="right">41.2G (-0.02%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.424143707</td><td align="right">10.2s</td><td align="right">104.7G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.424143707 (=)</td><td align="right">10.3s (+0.4%)</td><td align="right">104.7G (-0.01%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.04s</td><td align="right">31.6G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">2.97s (-2.2%)</td><td align="right">31.5G (-0.08%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739271968</td><td align="right">3.72s</td><td align="right">39.8G</td><td align="right">638</td><td align="right">2</td><td align="right">6.739271968 (=)</td><td align="right">3.75s (+0.8%)</td><td align="right">39.8G (-0.04%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.866901786</td><td align="right">6.96s</td><td align="right">71.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">6.97s (+0.2%)</td><td align="right">71.5G (-0.09%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.866617805</td><td align="right">7.29s</td><td align="right">71.3G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">7.38s (+1.1%)</td><td align="right">71.3G (-0.09%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884862145</td><td align="right">7.50s</td><td align="right">71.1G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">7.46s (-0.7%)</td><td align="right">71.0G (-0.09%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.887234466</td><td align="right">8.32s</td><td align="right">75.5G</td><td align="right">921</td><td align="right">2</td><td align="right">6.887234466 (=)</td><td align="right">8.36s (+0.5%)</td><td align="right">75.4G (-0.11%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.907904741</td><td align="right">0.657s</td><td align="right">6.9G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.656s (-0.1%)</td><td align="right">6.9G (-0.07%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.38583082</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.371875026 (-0.4122%)</td><td align="right">0.000s (+11.1%)</td><td align="right">0.1G (-0.80%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.899367957</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">2</td><td align="right">6.899367957 (=)</td><td align="right">0.001s (+2.9%)</td><td align="right">0.1G (-0.24%)</td><td align="right">11</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.064688363</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">4</td><td align="right">4</td><td align="right">4.047459862 (-0.4239%)</td><td align="right">0.003s (+5.0%)</td><td align="right">0.1G (+0.37%)</td><td align="right">7</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.75504777</td><td align="right">0.025s</td><td align="right">0.4G</td><td align="right">4</td><td align="right">5</td><td align="right">4.730850312 (-0.5089%)</td><td align="right">0.026s (+3.3%)</td><td align="right">0.4G (+1.83%)</td><td align="right">12</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.758421601</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">2</td><td align="right">3</td><td align="right">6.758265349 (-0.0023%)</td><td align="right">0.009s (+0.4%)</td><td align="right">0.2G (+0.52%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.833436601</td><td align="right">0.391s</td><td align="right">5.7G</td><td align="right">15</td><td align="right">3</td><td align="right">7.807937174 (-0.3255%)</td><td align="right">0.385s (-1.5%)</td><td align="right">5.7G (+0.06%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.568529293</td><td align="right">2.30s</td><td align="right">23.9G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (-0.2174%)</td><td align="right">2.36s (+2.5%)</td><td align="right">24.3G (+1.88%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.041117399</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.041117399 (=)</td><td align="right">0.001s (-8.9%)</td><td align="right">0.1G (-1.58%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-4.8%)</td><td align="right">0.1G (-0.17%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502224007</td><td align="right">0.353s</td><td align="right">5.3G</td><td align="right">8</td><td align="right">3</td><td align="right">7.491980364 (-0.1365%)</td><td align="right">0.355s (+0.8%)</td><td align="right">5.3G (+0.00%)</td><td align="right">148</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.473231053</td><td align="right">0.350s</td><td align="right">4.5G</td><td align="right">22</td><td align="right">3</td><td align="right">5.470440768 (-0.0510%)</td><td align="right">0.360s (+3.0%)</td><td align="right">4.5G (-0.02%)</td><td align="right">242</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.668390329</td><td align="right">0.470s</td><td align="right">5.9G</td><td align="right">20</td><td align="right">3</td><td align="right">5.657913279 (-0.1848%)</td><td align="right">0.463s (-1.6%)</td><td align="right">5.9G (-0.41%)</td><td align="right">197</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.580748978</td><td align="right">0.867s</td><td align="right">9.3G</td><td align="right">43</td><td align="right">3</td><td align="right">7.546335898 (-0.4540%)</td><td align="right">0.855s (-1.4%)</td><td align="right">9.2G (-0.40%)</td><td align="right">1614</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.460796773</td><td align="right">0.404s</td><td align="right">5.8G</td><td align="right">5</td><td align="right">3</td><td align="right">8.460796773 (=)</td><td align="right">0.402s (-0.6%)</td><td align="right">5.8G (+0.02%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739358212</td><td align="right">1.51s</td><td align="right">17.3G</td><td align="right">674</td><td align="right">2</td><td align="right">6.739358212 (=)</td><td align="right">1.52s (+0.3%)</td><td align="right">17.3G (-0.07%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.861724654</td><td align="right">1.57s</td><td align="right">17.6G</td><td align="right">141</td><td align="right">2</td><td align="right">6.861724654 (=)</td><td align="right">1.58s (+0.7%)</td><td align="right">17.5G (-0.28%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.868127142</td><td align="right">1.80s</td><td align="right">19.2G</td><td align="right">293</td><td align="right">2</td><td align="right">6.868127142 (=)</td><td align="right">1.81s (+0.7%)</td><td align="right">19.1G (-0.31%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884042436</td><td align="right">1.77s</td><td align="right">18.0G</td><td align="right">455</td><td align="right">2</td><td align="right">6.884042436 (=)</td><td align="right">1.77s (-0.1%)</td><td align="right">18.0G (-0.37%)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.893377041</td><td align="right">2.24s</td><td align="right">22.0G</td><td align="right">911</td><td align="right">2</td><td align="right">6.893377041 (=)</td><td align="right">2.27s (+1.5%)</td><td align="right">22.0G (+0.01%)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.317213189</td><td align="right">0.358s</td><td align="right">3.7G</td><td align="right">75</td><td align="right">4</td><td align="right">7.29196634 (-0.3450%)</td><td align="right">0.364s (+1.6%)</td><td align="right">3.7G (-0.42%)</td><td align="right">321</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">7.9829318</td><td align="right">0.746s</td><td align="right">7.6G</td><td align="right">1</td><td align="right">4</td><td align="right">7.9829318 (=)</td><td align="right">0.693s (-7.1%)</td><td align="right">7.1G (-6.78%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.884811284</td><td align="right">0.729s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.812252898 (-0.9202%)</td><td align="right">0.741s (+1.6%)</td><td align="right">7.2G (-0.12%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.479374707</td><td align="right">0.778s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.440161581 (-0.5243%)</td><td align="right">0.786s (+1.0%)</td><td align="right">7.5G (-0.14%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.996096327</td><td align="right">0.788s</td><td align="right">7.5G</td><td align="right">206</td><td align="right">4</td><td align="right">6.967535764 (-0.4082%)</td><td align="right">0.807s (+2.5%)</td><td align="right">7.4G (-0.46%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om2 `-2d -c` planted</td><td align="right">6.744721993</td><td align="right">0.545s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.744721993 (=)</td><td align="right">0.554s (+1.6%)</td><td align="right">6.2G (-0.26%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d -c` planted</td><td align="right">6.856239474</td><td align="right">0.930s</td><td align="right">10.4G</td><td align="right">132</td><td align="right">2</td><td align="right">6.856239474 (=)</td><td align="right">0.931s (+0.1%)</td><td align="right">10.4G (-0.54%)</td><td align="right">132</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d -c` planted</td><td align="right">6.857778113</td><td align="right">1.15s</td><td align="right">12.6G</td><td align="right">296</td><td align="right">2</td><td align="right">6.857778113 (=)</td><td align="right">1.15s (-0.2%)</td><td align="right">12.5G (-0.48%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d -c` planted</td><td align="right">6.873378755</td><td align="right">1.27s</td><td align="right">13.3G</td><td align="right">446</td><td align="right">2</td><td align="right">6.873378755 (=)</td><td align="right">1.26s (-0.7%)</td><td align="right">13.2G (-0.51%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d -c` planted</td><td align="right">6.875237392</td><td align="right">1.41s</td><td align="right">14.2G</td><td align="right">894</td><td align="right">2</td><td align="right">6.875237392 (=)</td><td align="right">1.42s (+0.9%)</td><td align="right">14.2G (-0.57%)</td><td align="right">894</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.088s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.089s (+0.6%)</td><td align="right">1.0G (-0.24%)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">6.113125871</td><td align="right">0.070s</td><td align="right">0.8G</td><td align="right">46</td><td align="right">3</td><td align="right">6.066305904 (-0.7659%)</td><td align="right">0.070s (+0.3%)</td><td align="right">0.8G (-1.47%)</td><td align="right">187</td><td align="right">3</td></tr>
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
<tr><td align="right">om2 `-2d --regularized -N1`</td><td align="right">7.548816177</td><td align="right">0.775s</td><td align="right">8.2G</td><td align="right">120</td><td align="right">2</td><td align="right">7.548816177 (=)</td><td align="right">0.781s (+0.8%)</td><td align="right">8.2G (-0.08%)</td><td align="right">120</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">0.953s</td><td align="right">9.3G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">0.945s (-0.9%)</td><td align="right">9.3G (-0.57%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N1`</td><td align="right">7.966995214</td><td align="right">2.86s</td><td align="right">26.7G</td><td align="right">104</td><td align="right">2</td><td align="right">7.966995214 (=)</td><td align="right">2.84s (-0.5%)</td><td align="right">26.6G (-0.19%)</td><td align="right">104</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N1`</td><td align="right">7.981574549</td><td align="right">1.76s</td><td align="right">15.9G</td><td align="right">113</td><td align="right">2</td><td align="right">7.981574549 (=)</td><td align="right">1.79s (+1.4%)</td><td align="right">15.8G (-0.24%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N1`</td><td align="right">7.978912396</td><td align="right">2.62s</td><td align="right">22.6G</td><td align="right">234</td><td align="right">2</td><td align="right">7.978912396 (=)</td><td align="right">2.63s (+0.5%)</td><td align="right">22.5G (-0.27%)</td><td align="right">234</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.51s</td><td align="right">25.1G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.55s (+1.7%)</td><td align="right">25.0G (-0.07%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N10`</td><td align="right">7.556894677</td><td align="right">5.34s</td><td align="right">49.2G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">5.40s (+1.3%)</td><td align="right">49.1G (-0.11%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N10`</td><td align="right">7.967531078</td><td align="right">6.16s</td><td align="right">55.1G</td><td align="right">98</td><td align="right">2</td><td align="right">7.967531078 (=)</td><td align="right">6.12s (-0.7%)</td><td align="right">55.1G (-0.10%)</td><td align="right">98</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N10`</td><td align="right">7.981574549</td><td align="right">5.93s</td><td align="right">51.0G</td><td align="right">113</td><td align="right">2</td><td align="right">7.981574549 (=)</td><td align="right">5.78s (-2.5%)</td><td align="right">51.0G (-0.12%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N10`</td><td align="right">7.976140205</td><td align="right">8.31s</td><td align="right">70.0G</td><td align="right">240</td><td align="right">2</td><td align="right">7.976140205 (=)</td><td align="right">8.28s (-0.4%)</td><td align="right">69.9G (-0.11%)</td><td align="right">240</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N1`</td><td align="right">7.970508085</td><td align="right">0.464s</td><td align="right">5.0G</td><td align="right">1</td><td align="right">3</td><td align="right">7.970508085 (=)</td><td align="right">0.398s (-14.2%)</td><td align="right">4.3G (-14.75%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N1`</td><td align="right">7.9828492</td><td align="right">0.888s</td><td align="right">8.6G</td><td align="right">1</td><td align="right">3</td><td align="right">7.9828492 (=)</td><td align="right">0.747s (-15.9%)</td><td align="right">7.4G (-14.88%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N1`</td><td align="right">7.989613065</td><td align="right">0.858s</td><td align="right">8.2G</td><td align="right">1</td><td align="right">3</td><td align="right">7.989613065 (=)</td><td align="right">0.703s (-18.0%)</td><td align="right">6.7G (-17.75%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N1`</td><td align="right">7.993490371</td><td align="right">0.961s</td><td align="right">9.2G</td><td align="right">1</td><td align="right">3</td><td align="right">7.993490371 (=)</td><td align="right">0.806s (-16.2%)</td><td align="right">7.8G (-15.40%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N1`</td><td align="right">7.994735672</td><td align="right">1.14s</td><td align="right">10.7G</td><td align="right">1</td><td align="right">3</td><td align="right">7.994735672 (=)</td><td align="right">0.991s (-12.7%)</td><td align="right">9.4G (-12.25%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">4.05s</td><td align="right">43.2G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">3.37s (-16.8%)</td><td align="right">36.3G (-15.88%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N10`</td><td align="right">7.556653713</td><td align="right">9.06s</td><td align="right">92.7G</td><td align="right">78</td><td align="right">2</td><td align="right">7.556653713 (=)</td><td align="right">6.88s (-24.1%)</td><td align="right">67.6G (-27.14%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N10`</td><td align="right">7.965009721</td><td align="right">11.6s</td><td align="right">107.6G</td><td align="right">106</td><td align="right">2</td><td align="right">7.965009721 (=)</td><td align="right">10.3s (-11.5%)</td><td align="right">87.7G (-18.50%)</td><td align="right">106</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N10`</td><td align="right">7.981063575</td><td align="right">12.0s</td><td align="right">109.9G</td><td align="right">119</td><td align="right">2</td><td align="right">7.981063575 (=)</td><td align="right">9.43s (-21.1%)</td><td align="right">86.4G (-21.36%)</td><td align="right">119</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N10`</td><td align="right">7.976681139</td><td align="right">9.14s</td><td align="right">84.9G</td><td align="right">256</td><td align="right">2</td><td align="right">7.976681139 (=)</td><td align="right">8.79s (-3.8%)</td><td align="right">80.9G (-4.79%)</td><td align="right">256</td><td align="right">2</td></tr>
<tr><td align="right">om2 planted, `-2d --no-infomap -c`</td><td align="right">6.789039995</td><td align="right">0.052s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.789039995 (=)</td><td align="right">0.053s (+2.5%)</td><td align="right">0.6G (-2.58%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om4 planted, `-2d --no-infomap -c`</td><td align="right">6.880650147</td><td align="right">0.112s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.880650147 (=)</td><td align="right">0.109s (-3.3%)</td><td align="right">1.0G (-2.89%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td align="right">om5 planted, `-2d --no-infomap -c`</td><td align="right">6.902222527</td><td align="right">0.127s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.902222527 (=)</td><td align="right">0.124s (-2.5%)</td><td align="right">1.1G (-2.63%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td align="right">om6 planted, `-2d --no-infomap -c`</td><td align="right">6.930934993</td><td align="right">0.133s</td><td align="right">1.2G</td><td align="right">24</td><td align="right">2</td><td align="right">6.930934993 (=)</td><td align="right">0.130s (-2.5%)</td><td align="right">1.1G (-2.97%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td align="right">om8 planted, `-2d --no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.148s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.145s (-1.8%)</td><td align="right">1.2G (-3.12%)</td><td align="right">32</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-30.5%)</td><td align="right">0.1G (-2.35%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (-6.1%)</td><td align="right">0.1G (-2.40%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.024s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.03324474 (-0.3856%)</td><td align="right">0.014s (-41.2%)</td><td align="right">0.2G (-29.76%)</td><td align="right">9</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.244s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.754013143 (+0.7684%)</td><td align="right">0.141s (-42.2%)</td><td align="right">1.7G (-37.80%)</td><td align="right">10</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.059s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.055s (-8.0%)</td><td align="right">0.7G (-4.13%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.08s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">2.87s (-6.7%)</td><td align="right">33.0G (-3.02%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">20.4s</td><td align="right">181.4G</td><td align="right">5</td><td align="right">6</td><td align="right">5.620539396 (+1.1539%)</td><td align="right">14.6s (-28.4%)</td><td align="right">126.3G (-30.42%)</td><td align="right">135</td><td align="right">5</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-7.3%)</td><td align="right">0.1G (-2.39%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-43.2%)</td><td align="right">0.1G (-0.76%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">3.21s</td><td align="right">37.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">3.02s (-6.1%)</td><td align="right">36.6G (-2.27%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">4.13s</td><td align="right">44.5G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.59s (-13.0%)</td><td align="right">41.8G (-6.08%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.45s</td><td align="right">46.5G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.66s (-17.7%)</td><td align="right">41.3G (-11.09%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.2s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">9.25s (-9.4%)</td><td align="right">95.3G (-2.63%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.23s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.13s (-3.2%)</td><td align="right">34.7G (-2.22%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.701s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.668s (-4.7%)</td><td align="right">6.9G (-3.99%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.078067323 (-8.7135%)</td><td align="right">0.001s (-13.7%)</td><td align="right">0.1G (+0.78%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367 (+0.0797%)</td><td align="right">0.007s (-0.4%)</td><td align="right">0.1G (+0.02%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.024s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">3.892209764 (-3.8689%)</td><td align="right">0.023s (-5.6%)</td><td align="right">0.3G (+0.57%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.244s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.509265423 (-4.4194%)</td><td align="right">0.240s (-1.6%)</td><td align="right">2.9G (+3.29%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.059s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502 (+0.7165%)</td><td align="right">0.058s (-2.3%)</td><td align="right">0.7G (+2.01%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.08s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">8.009172258 (+2.5773%)</td><td align="right">2.78s (-9.7%)</td><td align="right">31.1G (-8.75%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">20.4s</td><td align="right">181.4G</td><td align="right">5</td><td align="right">6</td><td align="right">5.512433077 (-0.7917%)</td><td align="right">19.4s (-5.2%)</td><td align="right">182.4G (+0.52%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653 (-0.8182%)</td><td align="right">0.003s (-13.0%)</td><td align="right">0.1G (-2.72%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (-4.1040%)</td><td align="right">0.000s (-43.0%)</td><td align="right">0.1G (-1.57%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">3.21s</td><td align="right">37.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.427572783 (+0.4752%)</td><td align="right">3.05s (-5.2%)</td><td align="right">37.5G (+0.11%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">4.13s</td><td align="right">44.5G</td><td align="right">257</td><td align="right">3</td><td align="right">5.378824196 (-0.2496%)</td><td align="right">3.76s (-9.0%)</td><td align="right">43.9G (-1.39%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.45s</td><td align="right">46.5G</td><td align="right">228</td><td align="right">3</td><td align="right">5.56659036 (-0.1426%)</td><td align="right">4.11s (-7.7%)</td><td align="right">45.9G (-1.17%)</td><td align="right">220</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.2s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.192724425 (-3.0848%)</td><td align="right">8.26s (-19.1%)</td><td align="right">85.9G (-12.27%)</td><td align="right">1910</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.23s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451 (+2.5761%)</td><td align="right">3.17s (-1.9%)</td><td align="right">35.3G (-0.44%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.701s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.903208727 (-0.0795%)</td><td align="right">0.649s (-7.4%)</td><td align="right">6.9G (-4.50%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-74.1%)</td><td align="right">0.1G (-38.77%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">5</td><td align="right">2</td><td align="right">6.862755928 (-0.0042%)</td><td align="right">0.007s (-70.6%)</td><td align="right">0.1G (-56.32%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.026557116</td><td align="right">0.118s</td><td align="right">1.3G</td><td align="right">12</td><td align="right">5</td><td align="right">4.048857953 (+0.5538%)</td><td align="right">0.024s (-79.6%)</td><td align="right">0.3G (-76.93%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.736412597</td><td align="right">1.86s</td><td align="right">20.2G</td><td align="right">11</td><td align="right">6</td><td align="right">4.717760238 (-0.3938%)</td><td align="right">0.244s (-86.8%)</td><td align="right">2.8G (-86.11%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.738927979</td><td align="right">0.132s</td><td align="right">1.4G</td><td align="right">80</td><td align="right">3</td><td align="right">6.740943136 (+0.0299%)</td><td align="right">0.059s (-55.2%)</td><td align="right">0.7G (-49.37%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.805465772</td><td align="right">7.61s</td><td align="right">63.0G</td><td align="right">220</td><td align="right">4</td><td align="right">7.807937174 (+0.0317%)</td><td align="right">3.08s (-59.6%)</td><td align="right">34.1G (-45.94%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.55442136</td><td align="right">144.8s</td><td align="right">1113.7G</td><td align="right">766</td><td align="right">9</td><td align="right">5.556421705 (+0.0360%)</td><td align="right">20.4s (-85.9%)</td><td align="right">181.4G (-83.71%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-67.4%)</td><td align="right">0.1G (-46.14%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-15.8%)</td><td align="right">0.1G (-6.49%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502028585</td><td align="right">8.75s</td><td align="right">78.0G</td><td align="right">144</td><td align="right">3</td><td align="right">7.392442593 (-1.4608%)</td><td align="right">3.21s (-63.3%)</td><td align="right">37.5G (-51.96%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392441014</td><td align="right">11.4s</td><td align="right">120.5G</td><td align="right">251</td><td align="right">3</td><td align="right">5.392285003 (-0.0029%)</td><td align="right">4.13s (-63.7%)</td><td align="right">44.5G (-63.09%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578435633</td><td align="right">7.39s</td><td align="right">81.6G</td><td align="right">301</td><td align="right">3</td><td align="right">5.574537176 (-0.0699%)</td><td align="right">4.45s (-39.8%)</td><td align="right">46.5G (-43.09%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">8.432467909</td><td align="right">5.26s</td><td align="right">52.0G</td><td align="right">114</td><td align="right">4</td><td align="right">7.421664324 (-11.9870%)</td><td align="right">10.2s (+93.8%)</td><td align="right">97.9G (+88.06%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">7.938575228</td><td align="right">6.72s</td><td align="right">56.9G</td><td align="right">25</td><td align="right">4</td><td align="right">8.235585529 (+3.7414%)</td><td align="right">3.23s (-51.9%)</td><td align="right">35.5G (-37.66%)</td><td align="right">25</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-64.4%)</td><td align="right">0.1G (-14.21%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">5</td><td align="right">2</td><td align="right">6.861229775 (-0.0265%)</td><td align="right">0.006s (-46.5%)</td><td align="right">0.1G (-27.89%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.285012668</td><td align="right">0.029s</td><td align="right">0.4G</td><td align="right">56</td><td align="right">2</td><td align="right">4.283072584 (-0.0453%)</td><td align="right">0.009s (-70.3%)</td><td align="right">0.2G (-56.48%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.600443859</td><td align="right">0.572s</td><td align="right">5.9G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.096s (-83.2%)</td><td align="right">1.1G (-80.48%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739721413</td><td align="right">0.078s</td><td align="right">0.8G</td><td align="right">80</td><td align="right">2</td><td align="right">6.739575295 (-0.0022%)</td><td align="right">0.042s (-46.6%)</td><td align="right">0.5G (-33.78%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.9500396</td><td align="right">3.92s</td><td align="right">29.2G</td><td align="right">496</td><td align="right">2</td><td align="right">7.949978834 (-0.0008%)</td><td align="right">2.27s (-42.0%)</td><td align="right">23.8G (-18.40%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.742988533</td><td align="right">45.4s</td><td align="right">251.6G</td><td align="right">11809</td><td align="right">2</td><td align="right">6.754216663 (+0.1665%)</td><td align="right">19.3s (-57.4%)</td><td align="right">117.4G (-53.33%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-53.5%)</td><td align="right">0.1G (-27.03%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+11.2%)</td><td align="right">0.1G (-2.04%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.50595639</td><td align="right">6.78s</td><td align="right">55.2G</td><td align="right">142</td><td align="right">2</td><td align="right">7.400445378 (-1.4057%)</td><td align="right">2.69s (-60.4%)</td><td align="right">32.3G (-41.58%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393312779</td><td align="right">4.75s</td><td align="right">43.9G</td><td align="right">332</td><td align="right">2</td><td align="right">5.393055049 (-0.0048%)</td><td align="right">3.62s (-23.9%)</td><td align="right">41.9G (-4.72%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.579216889</td><td align="right">5.96s</td><td align="right">58.6G</td><td align="right">301</td><td align="right">2</td><td align="right">5.571539329 (-0.1376%)</td><td align="right">3.73s (-37.4%)</td><td align="right">41.2G (-29.69%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.131110023</td><td align="right">4.77s</td><td align="right">36.7G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (+1.2849%)</td><td align="right">2.97s (-37.8%)</td><td align="right">31.5G (-14.11%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>
