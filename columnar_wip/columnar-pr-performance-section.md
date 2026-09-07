## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`); `time` is `--timing-json`'s `timing.total_s`. One run per `-N10` row (deterministic; `instr` carries the comparison); interleaved minimum of 3 for `-N1` rows.

> **This PR is the master sync**, bringing #1044 through #1076 onto the branch. Three of them change the hierarchical **object-oriented** search and are the reason this sync re-measures the OO arm rather than carrying it from the log: **#1075** dissolves intermediate modules that no longer pay for their index codebook, **#1040** makes `numLevels()` the tree's depth, and **#1076** parses `--cluster-data` once per run. It also carries #1052/#1054 (the tree-level `|K − K_pref|` cost) and #1044 (linlog smoothness at q=1).

> **The columnar search is untouched: every columnar row is bit-identical old vs new** (`-C`, `-C -2`, `-C -N1`, `-F`, `--non-redundant`, and the whole overlapping family). #1075 lives in `InfomapBase::hierarchicalPartition`, which the columnar engine does not run, so the sync moves only the OO arm. That is the point of re-measuring OO here — the previous sync's snapshot carried OO from a log because its PRs were columnar-only; this one cannot.

> **Old** = a fresh `MODE=release OPENMP=0` build of the `columnar-hierarchical-core` tip `ea0c3d78`, md5 `02a498e804c1e91c379aaa1901211190`; **new** = this sync (merge `54e1158f`), md5 `533d4455993129f42ffc6ca2ff79265e`. Both arms interleaved in one session.

### What the change moves

Every configuration where old and new differ in bits. All are on the OO arm; the columnar arm is bit-identical everywhere. #1075 lowers the codelength on every hierarchical network (it removes levels that do not pay for themselves); the one `+` cell is the #1052/#1054 two-level `science2001 (pref.)` search-path change carried from the prior sync, 50× under the 0.1% bar.

| network | table | old bits | new bits | Δbits | old instr | new instr | Δinstr |
|---|---|--:|--:|--:|--:|--:|--:|
| ninetriangles | OO | 3.38583082 | **3.371875026** | **-0.4122%** | 0.1G | 0.1G | -0.07% |
| netscicoauthor2010 | OO | 4.043549344 | **4.026557116** | **-0.4202%** | 1.3G | 1.3G | +0.24% |
| powergrid | OO | 4.758729201 | **4.736412597** | **-0.4690%** | 20.1G | 20.2G | +0.14% |
| science2001 | OO | 7.836389208 | **7.805465772** | **-0.3946%** | 63.0G | 63.0G | +0.05% |
| web-NotreDame | OO | 5.565924768 | **5.55442136** | **-0.2067%** | 1109.9G | 1113.7G | +0.34% |
| malaria | OO | 7.502420503 | **7.502028585** | **-0.0052%** | 78.0G | 78.1G | +0.01% |
| air30k | OO | 5.392871148 | **5.392441014** | **-0.0080%** | 120.5G | 120.5G | +0.05% |
| air30k (meta) | OO | 8.437832327 | **8.432467909** | **-0.0636%** | 52.0G | 52.0G | +0.05% |
| science2001 (pref.) | OO | 7.940353597 | **7.938575228** | **-0.0224%** | 65.1G | 57.0G | -12.49% |
| science2001 (pref.) | OO -2 | 8.130969526 | **8.131110023** | **+0.0017%** | 44.0G | 36.7G | -16.52% |

> **The `--cluster-data` scoring cost the previous sync attempt inherited is gone.** #1051 (in this sync) adds the duplicate-id report; on its own it re-parses the clu file, which the previous sync's snapshot measured at **+7.0–7.5% instructions** on the planted `--no-infomap -c` rows. #1076 (also in this sync) makes the report read the trials' own single parse instead, so those rows now sit at **+0.31–0.43%** — the cost of computing the report, not of a second parse. Old here is the columnar tip, which has neither PR, so the delta is the net of both: report added, second parse removed.

### Old vs new columnar — standard search (`-C -N10`)

Overlapping and wikispeedia rows run `-C -d -N10`. Every codelength is unchanged; read a sub-2% time delta against a ~0% instr delta as noise.

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
<tr><td align="right">ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (-0.2%)</td><td align="right">0.1G (-0.16%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (+0.1%)</td><td align="right">0.1G (-0.06%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.054540245</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">4</td><td align="right">4.054540245 (=)</td><td align="right">0.022s (-1.1%)</td><td align="right">0.3G (+0.11%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.741072056</td><td align="right">0.237s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.741072056 (=)</td><td align="right">0.238s (+0.2%)</td><td align="right">2.8G (+0.19%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.057s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.057s (-0.1%)</td><td align="right">0.7G (+0.16%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.833436601</td><td align="right">3.00s</td><td align="right">34.0G</td><td align="right">15</td><td align="right">3</td><td align="right">7.833436601 (=)</td><td align="right">3.34s (+11.1%)</td><td align="right">34.1G (+0.06%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.568529293</td><td align="right">19.6s</td><td align="right">182.7G</td><td align="right">5</td><td align="right">6</td><td align="right">5.568529293 (=)</td><td align="right">19.7s (+0.7%)</td><td align="right">183.0G (+0.20%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (=)</td><td align="right">0.1G (-6.79%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+42.1%)</td><td align="right">0.1G (-0.65%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.39750171</td><td align="right">3.08s</td><td align="right">37.5G</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">3.07s (-0.4%)</td><td align="right">37.5G (+0.01%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392425413</td><td align="right">3.86s</td><td align="right">44.5G</td><td align="right">22</td><td align="right">3</td><td align="right">5.392425413 (=)</td><td align="right">3.86s (=)</td><td align="right">44.5G (+0.03%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574746817</td><td align="right">4.16s</td><td align="right">46.5G</td><td align="right">21</td><td align="right">3</td><td align="right">5.574746817 (=)</td><td align="right">4.17s (+0.2%)</td><td align="right">46.5G (+0.02%)</td><td align="right">21</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.53s</td><td align="right">97.8G</td><td align="right">23</td><td align="right">3</td><td align="right">7.42215327 (=)</td><td align="right">9.53s (=)</td><td align="right">97.9G (+0.01%)</td><td align="right">23</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.24s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.19s (-1.6%)</td><td align="right">35.5G (-0.01%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.33s</td><td align="right">47.5G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">4.36s (+0.7%)</td><td align="right">47.5G (+0.06%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">8.08s</td><td align="right">83.1G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">8.04s (-0.4%)</td><td align="right">83.2G (+0.05%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.866617805</td><td align="right">8.06s</td><td align="right">81.5G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">8.20s (+1.7%)</td><td align="right">81.6G (+0.07%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.884862145</td><td align="right">8.43s</td><td align="right">83.3G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">8.44s (+0.1%)</td><td align="right">83.3G (+0.07%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.987473156</td><td align="right">9.02s</td><td align="right">85.7G</td><td align="right">203</td><td align="right">4</td><td align="right">6.987473156 (=)</td><td align="right">9.16s (+1.5%)</td><td align="right">85.8G (+0.08%)</td><td align="right">203</td><td align="right">4</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.706s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.712s (+0.9%)</td><td align="right">7.2G (+0.08%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Overlapping and wikispeedia rows as `-C -2d -N10`.

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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.001s (+6.5%)</td><td align="right">0.1G (-0.87%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.861229775</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (=)</td><td align="right">0.007s (-2.4%)</td><td align="right">0.1G (-0.26%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.283072584</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.283072584 (=)</td><td align="right">0.009s (-0.6%)</td><td align="right">0.2G (+0.22%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.63729688</td><td align="right">0.097s</td><td align="right">1.2G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.098s (+1.4%)</td><td align="right">1.2G (+0.27%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739575295</td><td align="right">0.041s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">6.739575295 (=)</td><td align="right">0.042s (+1.9%)</td><td align="right">0.5G (-0.03%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.949978834</td><td align="right">2.26s</td><td align="right">23.8G</td><td align="right">506</td><td align="right">2</td><td align="right">7.949978834 (=)</td><td align="right">2.30s (+1.8%)</td><td align="right">23.8G (+0.01%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.754216663</td><td align="right">19.4s</td><td align="right">117.7G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.754216663 (=)</td><td align="right">19.6s (+1.1%)</td><td align="right">117.9G (+0.19%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (+9.1%)</td><td align="right">0.1G (-5.66%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+49.9%)</td><td align="right">0.1G (-0.12%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.400445378</td><td align="right">2.72s</td><td align="right">32.3G</td><td align="right">168</td><td align="right">2</td><td align="right">7.400445378 (=)</td><td align="right">2.72s (=)</td><td align="right">32.3G (=)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393055049</td><td align="right">3.64s</td><td align="right">41.9G</td><td align="right">334</td><td align="right">2</td><td align="right">5.393055049 (=)</td><td align="right">3.72s (+2.3%)</td><td align="right">41.9G (+0.03%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571539329</td><td align="right">3.81s</td><td align="right">41.2G</td><td align="right">304</td><td align="right">2</td><td align="right">5.571539329 (=)</td><td align="right">3.75s (-1.4%)</td><td align="right">41.2G (=)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.424143707</td><td align="right">11.1s</td><td align="right">104.8G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.424143707 (=)</td><td align="right">10.4s (-7.0%)</td><td align="right">104.7G (-0.02%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.01s</td><td align="right">31.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">2.97s (-1.5%)</td><td align="right">31.5G (-0.01%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739271968</td><td align="right">3.71s</td><td align="right">39.8G</td><td align="right">638</td><td align="right">2</td><td align="right">6.739271968 (=)</td><td align="right">3.69s (-0.7%)</td><td align="right">39.8G (+0.03%)</td><td align="right">638</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.866901786</td><td align="right">7.06s</td><td align="right">71.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">7.01s (-0.7%)</td><td align="right">71.6G (+0.03%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.866617805</td><td align="right">7.10s</td><td align="right">71.3G</td><td align="right">308</td><td align="right">2</td><td align="right">6.866617805 (=)</td><td align="right">7.08s (-0.3%)</td><td align="right">71.3G (+0.03%)</td><td align="right">308</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884862145</td><td align="right">7.62s</td><td align="right">71.1G</td><td align="right">451</td><td align="right">2</td><td align="right">6.884862145 (=)</td><td align="right">7.53s (-1.2%)</td><td align="right">71.1G (+0.05%)</td><td align="right">451</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.887234466</td><td align="right">8.28s</td><td align="right">75.4G</td><td align="right">921</td><td align="right">2</td><td align="right">6.887234466 (=)</td><td align="right">8.33s (+0.7%)</td><td align="right">75.5G (+0.04%)</td><td align="right">921</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.907904741</td><td align="right">0.652s</td><td align="right">6.9G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.657s (+0.7%)</td><td align="right">6.9G (+0.06%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

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
<tr><td align="right">ninetriangles</td><td align="right">3.38583082</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.000s (=)</td><td align="right">0.1G (-0.48%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.899367957</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">2</td><td align="right">6.899367957 (=)</td><td align="right">0.001s (+3.0%)</td><td align="right">0.1G (-0.37%)</td><td align="right">11</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.064688363</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">4</td><td align="right">4</td><td align="right">4.064688363 (=)</td><td align="right">0.003s (-0.1%)</td><td align="right">0.1G (-0.55%)</td><td align="right">4</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.75504777</td><td align="right">0.025s</td><td align="right">0.4G</td><td align="right">4</td><td align="right">5</td><td align="right">4.75504777 (=)</td><td align="right">0.024s (-0.2%)</td><td align="right">0.4G (-0.10%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.758421601</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">2</td><td align="right">3</td><td align="right">6.758421601 (=)</td><td align="right">0.009s (+0.7%)</td><td align="right">0.2G (-0.28%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.833436601</td><td align="right">0.392s</td><td align="right">5.7G</td><td align="right">15</td><td align="right">3</td><td align="right">7.833436601 (=)</td><td align="right">0.389s (-0.6%)</td><td align="right">5.7G (-0.04%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.568529293</td><td align="right">2.31s</td><td align="right">23.9G</td><td align="right">5</td><td align="right">6</td><td align="right">5.568529293 (=)</td><td align="right">2.30s (-0.5%)</td><td align="right">23.9G (+0.03%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.041117399</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.041117399 (=)</td><td align="right">0.001s (-4.1%)</td><td align="right">0.1G (-1.11%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (+4.2%)</td><td align="right">0.1G (-0.22%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502224007</td><td align="right">0.354s</td><td align="right">5.3G</td><td align="right">8</td><td align="right">3</td><td align="right">7.502224007 (=)</td><td align="right">0.360s (+1.6%)</td><td align="right">5.3G (=)</td><td align="right">8</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.473231053</td><td align="right">0.354s</td><td align="right">4.5G</td><td align="right">22</td><td align="right">3</td><td align="right">5.473231053 (=)</td><td align="right">0.363s (+2.6%)</td><td align="right">4.5G (+0.03%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.668390329</td><td align="right">0.477s</td><td align="right">5.9G</td><td align="right">20</td><td align="right">3</td><td align="right">5.668390329 (=)</td><td align="right">0.474s (-0.7%)</td><td align="right">5.9G (+0.03%)</td><td align="right">20</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.580748978</td><td align="right">0.862s</td><td align="right">9.3G</td><td align="right">43</td><td align="right">3</td><td align="right">7.580748978 (=)</td><td align="right">0.876s (+1.7%)</td><td align="right">9.3G (+0.02%)</td><td align="right">43</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.460796773</td><td align="right">0.403s</td><td align="right">5.8G</td><td align="right">5</td><td align="right">3</td><td align="right">8.460796773 (=)</td><td align="right">0.405s (+0.7%)</td><td align="right">5.8G (+0.04%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739358212</td><td align="right">1.52s</td><td align="right">17.3G</td><td align="right">674</td><td align="right">2</td><td align="right">6.739358212 (=)</td><td align="right">1.52s (-0.3%)</td><td align="right">17.3G (+0.01%)</td><td align="right">674</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.861724654</td><td align="right">1.59s</td><td align="right">17.6G</td><td align="right">141</td><td align="right">2</td><td align="right">6.861724654 (=)</td><td align="right">1.62s (+1.6%)</td><td align="right">17.6G (-0.02%)</td><td align="right">141</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.868127142</td><td align="right">1.81s</td><td align="right">19.2G</td><td align="right">293</td><td align="right">2</td><td align="right">6.868127142 (=)</td><td align="right">1.80s (-0.4%)</td><td align="right">19.2G (-0.01%)</td><td align="right">293</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884042436</td><td align="right">1.76s</td><td align="right">18.0G</td><td align="right">455</td><td align="right">2</td><td align="right">6.884042436 (=)</td><td align="right">1.77s (+0.7%)</td><td align="right">18.0G (=)</td><td align="right">455</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.893377041</td><td align="right">2.22s</td><td align="right">22.0G</td><td align="right">911</td><td align="right">2</td><td align="right">6.893377041 (=)</td><td align="right">2.23s (+0.4%)</td><td align="right">22.0G (=)</td><td align="right">911</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.317213189</td><td align="right">0.362s</td><td align="right">3.7G</td><td align="right">75</td><td align="right">4</td><td align="right">7.317213189 (=)</td><td align="right">0.364s (+0.7%)</td><td align="right">3.7G (+0.09%)</td><td align="right">75</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">7.9829318</td><td align="right">0.750s</td><td align="right">7.6G</td><td align="right">1</td><td align="right">4</td><td align="right">7.9829318 (=)</td><td align="right">0.751s (+0.1%)</td><td align="right">7.6G (+0.06%)</td><td align="right">1</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.884811284</td><td align="right">0.723s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.884811284 (=)</td><td align="right">0.721s (-0.4%)</td><td align="right">7.2G (+0.12%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.479374707</td><td align="right">0.760s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.479374707 (=)</td><td align="right">0.769s (+1.1%)</td><td align="right">7.5G (+0.06%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.996096327</td><td align="right">0.779s</td><td align="right">7.5G</td><td align="right">206</td><td align="right">4</td><td align="right">6.996096327 (=)</td><td align="right">0.771s (-1.0%)</td><td align="right">7.5G (+0.11%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om2 `-2d -c` planted</td><td align="right">6.744721993</td><td align="right">0.540s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.744721993 (=)</td><td align="right">0.546s (+1.1%)</td><td align="right">6.2G (+0.06%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d -c` planted</td><td align="right">6.856239474</td><td align="right">0.916s</td><td align="right">10.4G</td><td align="right">132</td><td align="right">2</td><td align="right">6.856239474 (=)</td><td align="right">0.926s (+1.2%)</td><td align="right">10.4G (+0.06%)</td><td align="right">132</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d -c` planted</td><td align="right">6.857778113</td><td align="right">1.14s</td><td align="right">12.6G</td><td align="right">296</td><td align="right">2</td><td align="right">6.857778113 (=)</td><td align="right">1.15s (+0.5%)</td><td align="right">12.6G (+0.03%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d -c` planted</td><td align="right">6.873378755</td><td align="right">1.25s</td><td align="right">13.3G</td><td align="right">446</td><td align="right">2</td><td align="right">6.873378755 (=)</td><td align="right">1.25s (+0.3%)</td><td align="right">13.3G (+0.03%)</td><td align="right">446</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d -c` planted</td><td align="right">6.875237392</td><td align="right">1.42s</td><td align="right">14.2G</td><td align="right">894</td><td align="right">2</td><td align="right">6.875237392 (=)</td><td align="right">1.42s (-0.1%)</td><td align="right">14.2G (=)</td><td align="right">894</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.089s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.089s (-0.4%)</td><td align="right">1.0G (+0.10%)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">6.113125871</td><td align="right">0.070s</td><td align="right">0.8G</td><td align="right">46</td><td align="right">3</td><td align="right">6.113125871 (=)</td><td align="right">0.071s (+0.6%)</td><td align="right">0.8G (+0.08%)</td><td align="right">46</td><td align="right">3</td></tr>
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
<tr><td align="right">om2 `-2d --regularized -N1`</td><td align="right">7.548816177</td><td align="right">0.781s</td><td align="right">8.2G</td><td align="right">120</td><td align="right">2</td><td align="right">7.548816177 (=)</td><td align="right">0.789s (+1.1%)</td><td align="right">8.2G (+0.08%)</td><td align="right">120</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">0.960s</td><td align="right">9.3G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">0.971s (+1.1%)</td><td align="right">9.3G (+0.02%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N1`</td><td align="right">7.966995214</td><td align="right">2.83s</td><td align="right">26.7G</td><td align="right">104</td><td align="right">2</td><td align="right">7.966995214 (=)</td><td align="right">2.83s (+0.1%)</td><td align="right">26.7G (+0.02%)</td><td align="right">104</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N1`</td><td align="right">7.981574549</td><td align="right">1.79s</td><td align="right">15.9G</td><td align="right">113</td><td align="right">2</td><td align="right">7.981574549 (=)</td><td align="right">1.75s (-2.6%)</td><td align="right">15.9G (+0.01%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N1`</td><td align="right">7.978912396</td><td align="right">2.56s</td><td align="right">22.6G</td><td align="right">234</td><td align="right">2</td><td align="right">7.978912396 (=)</td><td align="right">2.58s (+0.8%)</td><td align="right">22.6G (=)</td><td align="right">234</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.47s</td><td align="right">25.0G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.54s (+2.8%)</td><td align="right">25.1G (+0.06%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N10`</td><td align="right">7.556894677</td><td align="right">5.20s</td><td align="right">49.2G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">5.23s (+0.5%)</td><td align="right">49.2G (+0.08%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N10`</td><td align="right">7.967531078</td><td align="right">6.16s</td><td align="right">55.1G</td><td align="right">98</td><td align="right">2</td><td align="right">7.967531078 (=)</td><td align="right">6.15s (-0.1%)</td><td align="right">55.1G (+0.05%)</td><td align="right">98</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N10`</td><td align="right">7.981574549</td><td align="right">5.91s</td><td align="right">51.0G</td><td align="right">113</td><td align="right">2</td><td align="right">7.981574549 (=)</td><td align="right">5.86s (-0.9%)</td><td align="right">51.0G (+0.02%)</td><td align="right">113</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N10`</td><td align="right">7.976140205</td><td align="right">8.21s</td><td align="right">70.0G</td><td align="right">240</td><td align="right">2</td><td align="right">7.976140205 (=)</td><td align="right">8.17s (-0.4%)</td><td align="right">70.0G (+0.04%)</td><td align="right">240</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N1`</td><td align="right">7.970508085</td><td align="right">0.479s</td><td align="right">5.0G</td><td align="right">1</td><td align="right">3</td><td align="right">7.970508085 (=)</td><td align="right">0.465s (-2.9%)</td><td align="right">5.0G (+0.02%)</td><td align="right">1</td><td align="right">3</td></tr>
<tr><td align="right">om4 `-d --regularized -N1`</td><td align="right">7.9828492</td><td align="right">0.916s</td><td align="right">8.6G</td><td align="right">1</td><td align="right">3</td><td align="right">7.9828492 (=)</td><td align="right">0.877s (-4.3%)</td><td align="right">8.6G (+0.03%)</td><td align="right">1</td><td align="right">3</td></tr>
<tr><td align="right">om5 `-d --regularized -N1`</td><td align="right">7.989613065</td><td align="right">0.899s</td><td align="right">8.2G</td><td align="right">1</td><td align="right">3</td><td align="right">7.989613065 (=)</td><td align="right">0.857s (-4.7%)</td><td align="right">8.2G (+0.04%)</td><td align="right">1</td><td align="right">3</td></tr>
<tr><td align="right">om6 `-d --regularized -N1`</td><td align="right">7.993490371</td><td align="right">0.962s</td><td align="right">9.2G</td><td align="right">1</td><td align="right">3</td><td align="right">7.993490371 (=)</td><td align="right">0.948s (-1.4%)</td><td align="right">9.2G (+0.03%)</td><td align="right">1</td><td align="right">3</td></tr>
<tr><td align="right">om8 `-d --regularized -N1`</td><td align="right">7.994735672</td><td align="right">1.13s</td><td align="right">10.7G</td><td align="right">1</td><td align="right">3</td><td align="right">7.994735672 (=)</td><td align="right">1.13s (-0.6%)</td><td align="right">10.7G (+0.09%)</td><td align="right">1</td><td align="right">3</td></tr>
<tr><td align="right">om2 `-d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">3.98s</td><td align="right">43.2G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">3.88s (-2.5%)</td><td align="right">43.2G (+0.07%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N10`</td><td align="right">7.556653713</td><td align="right">9.33s</td><td align="right">92.7G</td><td align="right">78</td><td align="right">2</td><td align="right">7.556653713 (=)</td><td align="right">9.01s (-3.5%)</td><td align="right">92.7G (+0.05%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N10`</td><td align="right">7.965009721</td><td align="right">11.5s</td><td align="right">107.6G</td><td align="right">106</td><td align="right">2</td><td align="right">7.965009721 (=)</td><td align="right">11.1s (-3.1%)</td><td align="right">107.6G (+0.05%)</td><td align="right">106</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N10`</td><td align="right">7.981063575</td><td align="right">11.6s</td><td align="right">109.8G</td><td align="right">119</td><td align="right">2</td><td align="right">7.981063575 (=)</td><td align="right">11.4s (-1.4%)</td><td align="right">109.9G (+0.05%)</td><td align="right">119</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N10`</td><td align="right">7.976681139</td><td align="right">9.51s</td><td align="right">84.9G</td><td align="right">256</td><td align="right">2</td><td align="right">7.976681139 (=)</td><td align="right">9.47s (-0.4%)</td><td align="right">85.0G (+0.07%)</td><td align="right">256</td><td align="right">2</td></tr>
<tr><td align="right">om2 planted, `--no-infomap -c`</td><td align="right">6.789039995</td><td align="right">0.054s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.789039995 (=)</td><td align="right">0.055s (+1.4%)</td><td align="right">0.6G (+0.31%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om4 planted, `--no-infomap -c`</td><td align="right">6.880650147</td><td align="right">0.108s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.880650147 (=)</td><td align="right">0.114s (+5.2%)</td><td align="right">1.0G (+0.42%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td align="right">om5 planted, `--no-infomap -c`</td><td align="right">6.902222527</td><td align="right">0.111s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.902222527 (=)</td><td align="right">0.121s (+8.8%)</td><td align="right">1.1G (+0.42%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td align="right">om6 planted, `--no-infomap -c`</td><td align="right">6.930934993</td><td align="right">0.120s</td><td align="right">1.1G</td><td align="right">24</td><td align="right">2</td><td align="right">6.930934993 (=)</td><td align="right">0.126s (+5.0%)</td><td align="right">1.2G (+0.39%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td align="right">om8 planted, `--no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.126s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.135s (+7.5%)</td><td align="right">1.2G (+0.43%)</td><td align="right">32</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (-8.5%)</td><td align="right">0.1G (-1.61%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (-2.8%)</td><td align="right">0.1G (-1.91%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.054540245</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">4</td><td align="right">4.063005877 (+0.2088%)</td><td align="right">0.014s (-38.5%)</td><td align="right">0.2G (-29.83%)</td><td align="right">4</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.741072056</td><td align="right">0.238s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.774022245 (+0.6950%)</td><td align="right">0.141s (-40.6%)</td><td align="right">1.8G (-37.56%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.057s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.055s (-4.4%)</td><td align="right">0.7G (-3.97%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.833436601</td><td align="right">3.34s</td><td align="right">34.1G</td><td align="right">15</td><td align="right">3</td><td align="right">7.833436601 (=)</td><td align="right">2.84s (-14.8%)</td><td align="right">33.0G (-3.05%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.568529293</td><td align="right">19.7s</td><td align="right">183.0G</td><td align="right">5</td><td align="right">6</td><td align="right">5.625061982 (+1.0152%)</td><td align="right">14.2s (-28.0%)</td><td align="right">127.5G (-30.36%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (+9.1%)</td><td align="right">0.1G (-0.26%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-27.0%)</td><td align="right">0.1G (-0.47%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.39750171</td><td align="right">3.07s</td><td align="right">37.5G</td><td align="right">2</td><td align="right">3</td><td align="right">7.39750171 (=)</td><td align="right">2.96s (-3.4%)</td><td align="right">36.6G (-2.25%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392425413</td><td align="right">3.86s</td><td align="right">44.5G</td><td align="right">22</td><td align="right">3</td><td align="right">5.392425413 (=)</td><td align="right">3.55s (-8.1%)</td><td align="right">41.8G (-6.05%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574746817</td><td align="right">4.17s</td><td align="right">46.5G</td><td align="right">21</td><td align="right">3</td><td align="right">5.574746817 (=)</td><td align="right">3.70s (-11.3%)</td><td align="right">41.3G (-11.02%)</td><td align="right">21</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.53s</td><td align="right">97.9G</td><td align="right">23</td><td align="right">3</td><td align="right">7.42215327 (=)</td><td align="right">9.12s (-4.3%)</td><td align="right">95.3G (-2.59%)</td><td align="right">23</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.19s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.17s (-0.8%)</td><td align="right">34.7G (-2.19%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.712s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.672s (-5.6%)</td><td align="right">6.9G (-4.05%)</td><td align="right">199</td><td align="right">2</td></tr>
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
</tr></thead><tbody>
<tr><td align="right">ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">3</td><td align="right">3</td><td align="right">3.078067323 (-9.0897%)</td><td align="right">0.001s (+3.3%)</td><td align="right">0.1G (+0.09%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.006s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367 (+0.0797%)</td><td align="right">0.006s (-2.1%)</td><td align="right">0.1G (+0.08%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.054540245</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">2</td><td align="right">4</td><td align="right">3.892209764 (-4.0037%)</td><td align="right">0.021s (-3.8%)</td><td align="right">0.3G (-2.31%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.741072056</td><td align="right">0.238s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.509265423 (-4.8893%)</td><td align="right">0.229s (-3.7%)</td><td align="right">2.8G (-1.52%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.057s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502 (+0.7165%)</td><td align="right">0.058s (+0.8%)</td><td align="right">0.7G (+0.31%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.833436601</td><td align="right">3.34s</td><td align="right">34.1G</td><td align="right">15</td><td align="right">3</td><td align="right">8.009172258 (+2.2434%)</td><td align="right">2.71s (-18.8%)</td><td align="right">30.8G (-9.70%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.568529293</td><td align="right">19.7s</td><td align="right">183.0G</td><td align="right">5</td><td align="right">6</td><td align="right">5.517073626 (-0.9240%)</td><td align="right">19.6s (-0.4%)</td><td align="right">184.1G (+0.60%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653 (-0.8182%)</td><td align="right">0.003s (-11.8%)</td><td align="right">0.1G (-3.23%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (-4.1040%)</td><td align="right">0.000s (-41.6%)</td><td align="right">0.1G (-1.62%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.39750171</td><td align="right">3.07s</td><td align="right">37.5G</td><td align="right">2</td><td align="right">3</td><td align="right">7.427572783 (+0.4065%)</td><td align="right">3.07s (=)</td><td align="right">37.4G (-0.21%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392425413</td><td align="right">3.86s</td><td align="right">44.5G</td><td align="right">22</td><td align="right">3</td><td align="right">5.378912606 (-0.2506%)</td><td align="right">3.84s (-0.6%)</td><td align="right">43.9G (-1.42%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574746817</td><td align="right">4.17s</td><td align="right">46.5G</td><td align="right">21</td><td align="right">3</td><td align="right">5.567087439 (-0.1374%)</td><td align="right">4.17s (+0.1%)</td><td align="right">45.9G (-1.18%)</td><td align="right">21</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.42215327</td><td align="right">9.53s</td><td align="right">97.9G</td><td align="right">23</td><td align="right">3</td><td align="right">7.215299774 (-2.7870%)</td><td align="right">8.30s (-12.9%)</td><td align="right">85.8G (-12.32%)</td><td align="right">33</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.19s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451 (+2.5761%)</td><td align="right">3.18s (-0.2%)</td><td align="right">35.3G (-0.43%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.712s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.903208727 (-0.0795%)</td><td align="right">0.670s (-6.0%)</td><td align="right">6.9G (-4.46%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar

**Both arms re-measured on the new binary**, because #1075/#1040 move the OO path. air30k (meta) OO is `-N1` (it does not finish `-N10` in budget).

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.38583082 (+0.4139%)</td><td align="right">0.001s (-80.5%)</td><td align="right">0.1G (-38.90%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">5</td><td align="right">2</td><td align="right">6.862755928 (-0.0042%)</td><td align="right">0.006s (-71.3%)</td><td align="right">0.1G (-56.25%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.026557116</td><td align="right">0.118s</td><td align="right">1.3G</td><td align="right">12</td><td align="right">5</td><td align="right">4.054540245 (+0.6950%)</td><td align="right">0.022s (-81.1%)</td><td align="right">0.3G (-76.97%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.736412597</td><td align="right">1.85s</td><td align="right">20.2G</td><td align="right">11</td><td align="right">6</td><td align="right">4.741072056 (+0.0984%)</td><td align="right">0.238s (-87.2%)</td><td align="right">2.8G (-86.05%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.738927979</td><td align="right">0.134s</td><td align="right">1.4G</td><td align="right">80</td><td align="right">3</td><td align="right">6.740943136 (+0.0299%)</td><td align="right">0.057s (-57.3%)</td><td align="right">0.7G (-49.40%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.805465772</td><td align="right">7.67s</td><td align="right">63.0G</td><td align="right">220</td><td align="right">4</td><td align="right">7.833436601 (+0.3583%)</td><td align="right">3.34s (-56.5%)</td><td align="right">34.1G (-45.93%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.55442136</td><td align="right">144.8s</td><td align="right">1113.7G</td><td align="right">766</td><td align="right">9</td><td align="right">5.568529293 (+0.2540%)</td><td align="right">19.7s (-86.4%)</td><td align="right">183.0G (-83.56%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.018s</td><td align="right">0.2G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-79.9%)</td><td align="right">0.1G (-45.63%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-21.1%)</td><td align="right">0.1G (-7.23%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502028585</td><td align="right">8.89s</td><td align="right">78.1G</td><td align="right">144</td><td align="right">3</td><td align="right">7.39750171 (-1.3933%)</td><td align="right">3.07s (-65.5%)</td><td align="right">37.5G (-51.97%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392441014</td><td align="right">11.4s</td><td align="right">120.5G</td><td align="right">251</td><td align="right">3</td><td align="right">5.392425413 (-0.0003%)</td><td align="right">3.86s (-66.3%)</td><td align="right">44.5G (-63.09%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578435633</td><td align="right">7.63s</td><td align="right">81.6G</td><td align="right">301</td><td align="right">3</td><td align="right">5.574746817 (-0.0661%)</td><td align="right">4.17s (-45.3%)</td><td align="right">46.5G (-43.08%)</td><td align="right">21</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">8.432467909</td><td align="right">5.33s</td><td align="right">52.0G</td><td align="right">114</td><td align="right">4</td><td align="right">7.42215327 (-11.9812%)</td><td align="right">9.53s (+79.0%)</td><td align="right">97.9G (+88.11%)</td><td align="right">23</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">7.938575228</td><td align="right">6.81s</td><td align="right">57.0G</td><td align="right">25</td><td align="right">4</td><td align="right">8.235585529 (+3.7414%)</td><td align="right">3.19s (-53.1%)</td><td align="right">35.5G (-37.66%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar — two-level (`-2`)

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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.001s (-59.9%)</td><td align="right">0.1G (-13.79%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.011s</td><td align="right">0.2G</td><td align="right">5</td><td align="right">2</td><td align="right">6.861229775 (-0.0265%)</td><td align="right">0.007s (-41.9%)</td><td align="right">0.1G (-27.52%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.285012668</td><td align="right">0.028s</td><td align="right">0.4G</td><td align="right">56</td><td align="right">2</td><td align="right">4.283072584 (-0.0453%)</td><td align="right">0.009s (-69.5%)</td><td align="right">0.2G (-56.13%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.600443859</td><td align="right">0.551s</td><td align="right">5.9G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.098s (-82.2%)</td><td align="right">1.2G (-80.39%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739721413</td><td align="right">0.074s</td><td align="right">0.8G</td><td align="right">80</td><td align="right">2</td><td align="right">6.739575295 (-0.0022%)</td><td align="right">0.042s (-43.3%)</td><td align="right">0.5G (-33.63%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.9500396</td><td align="right">3.68s</td><td align="right">29.2G</td><td align="right">496</td><td align="right">2</td><td align="right">7.949978834 (-0.0008%)</td><td align="right">2.30s (-37.5%)</td><td align="right">23.8G (-18.33%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.742988533</td><td align="right">39.8s</td><td align="right">251.3G</td><td align="right">11809</td><td align="right">2</td><td align="right">6.754216663 (+0.1665%)</td><td align="right">19.6s (-50.7%)</td><td align="right">117.9G (-53.08%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-46.3%)</td><td align="right">0.1G (-26.19%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+9.3%)</td><td align="right">0.1G (-2.75%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.50595639</td><td align="right">6.17s</td><td align="right">55.2G</td><td align="right">142</td><td align="right">2</td><td align="right">7.400445378 (-1.4057%)</td><td align="right">2.72s (-55.8%)</td><td align="right">32.3G (-41.53%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393312779</td><td align="right">4.76s</td><td align="right">43.9G</td><td align="right">332</td><td align="right">2</td><td align="right">5.393055049 (-0.0048%)</td><td align="right">3.72s (-21.8%)</td><td align="right">41.9G (-4.64%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.579216889</td><td align="right">5.46s</td><td align="right">58.6G</td><td align="right">301</td><td align="right">2</td><td align="right">5.571539329 (-0.1376%)</td><td align="right">3.75s (-31.2%)</td><td align="right">41.2G (-29.65%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.131110023</td><td align="right">4.81s</td><td align="right">36.7G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (+1.2849%)</td><td align="right">2.97s (-38.3%)</td><td align="right">31.5G (-14.11%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

