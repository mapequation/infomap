## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`); `time` is `--timing-json`'s `timing.total_s`. One run per `-N10` row (deterministic; `instr` carries the comparison); interleaved minimum of 3 for `-N1` rows. Driver and every row: [`columnar_wip/bench-1122.py`](columnar_wip/bench-1122.py), [`columnar_wip/1122-ab-results.tsv`](columnar_wip/1122-ab-results.tsv).

> **This PR makes `-d -N1` return `-2d -N1`'s partition when its lone trial collapses to one module and
> the run-level rescue only ties the collapse (#1122).** The rescue (#1041, #1081) runs the two-level
> search on the trial's own seed. When that search beats the collapse it becomes the winner and raises
> the escalation signal that lets the deep repair search for fresh splits. When it only ties, as on the
> six E50000 `--regularized` rows, it was discarded together with its signal: the `-N1` repair ran
> without fresh discovery on the one-module fallback and the run returned one module, while `-2d -N1`
> on the same seed runs that discovery (#1124's block-granularity extraction) and returns 77–112 top
> modules. The change raises the signal whether or not the rescue is kept: one line moved out of the
> `if`. On the six rows `-d -N1` now returns `-2d -N1`'s partition bit for bit, −0.20% to −0.66% in bits,
> for +66% to +102% in seconds (+68% to +98% in instructions), which puts `-d -N1` at 1.06–1.20× the
> time of `-2d -N1`. No other row of the 177 moves in bits. **The time cost is above the marginal-trade
> rule, so this PR is a decision, not a default** (see "The trade" below).

> **Old** = a fresh `MODE=release OPENMP=0` build of `columnar-hierarchical-core` tip `14379b34` (#1124
> merged), md5 `1b8e57233835ea68322601026b33dd63`, which is the #1124 snapshot's new binary bit for bit;
> its column here reproduces that snapshot's new column on all 177 rows (median +0.01% in instructions,
> +0.5% in seconds). **New** = this PR, md5 `4586def7ac957ef2a135412345b21df0`. One session, arms
> interleaved per row, `-N1` rows as the minimum of 3 reps spread across the batch, `-N10` rows once;
> load 6–13 throughout, so read `instr` before `time`. **The object-oriented arms are not re-run**: this
> PR does not touch that engine (project instructions). Their cells in the two OO tables are carried
> from the #1079-day session on the same machine, at the precision printed there; the columnar cells
> next to them are this session's.

### What the change moves

Every configuration where old and new differ in bits, both arms: the six E50000
`-d --regularized -N1` rows and nothing else, which is the reach the code predicts (next section).

| network | table | old bits | new bits | Δbits | old instr | new instr | Δinstr | old time | new time | Δtime |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| om3 E50000 `-d --regularized -N1` | family | 7.969664223 | **7.925216272** | **-0.5577%** | 3.9G | 6.6G | +70.81% | 0.402s | 0.721s | +79.5% |
| om4 E50000 `-d --regularized -N1` | family | 7.978790193 | **7.92621405** | **-0.6589%** | 4.1G | 7.6G | +82.61% | 0.470s | 0.859s | +82.8% |
| om5 E50000 `-d --regularized -N1` | family | 7.98862707 | **7.956672505** | **-0.4000%** | 4.2G | 7.7G | +80.77% | 0.481s | 0.896s | +86.4% |
| om6 E50000 `-d --regularized -N1` | family | 7.989209626 | **7.944047825** | **-0.5653%** | 3.3G | 5.6G | +68.15% | 0.416s | 0.693s | +66.5% |
| om7 E50000 `-d --regularized -N1` | family | 7.992344954 | **7.957532546** | **-0.4356%** | 3.1G | 6.2G | +98.00% | 0.384s | 0.776s | +102.2% |
| om8 E50000 `-d --regularized -N1` | family | 7.994534803 | **7.978376075** | **-0.2021%** | 3.1G | 5.5G | +77.06% | 0.403s | 0.692s | +71.5% |

**Every cell where new is worse than old, and why.** No cell is worse in bits. The six moved rows are
+66% to +102% in seconds: that is the change, and "The trade" below is about it. 51 bit-identical
cells read more than 1% slower in seconds. On every bit-identical row of 10 ms or more (156 rows) the
instruction delta is −0.20% to +0.32%, and the change cannot run on any of them (next section). The
three largest were re-measured after the batch, three interleaved reps per arm, appended as reps 2–4
(the tables show the minimum): air30k (reg.) `-N10` +10.2% → −2.2%, om2 `-d --regularized -N10`
+11.1% → −0.2%, om7 `-F -d -N10` +9.4% → +1.7% (old 8.06–8.69 s, new 8.20–8.97 s). The largest left
are science2001 (pref.) `-N1` +7.1% (already a minimum of 3; −0.001% in instructions) and om2 E100000
`-d --regularized -N10` +5.5% (+0.04%): the session's noise floor. The sub-10 ms rows (ninetriangles,
jazz, lazega, the multilayer example) move by −9% to +0.5% in instructions at 0.06–0.1 G, in both
directions: startup, as in #1081's and #1124's sessions.

### Reach of the change

The signal has one reader, the deep repair's gate `freshDiscovery = m_numTrials > 1 ||
m_columnarRegroupEscalated` (`src/core/InfomapBase.cpp`), and the rescue only runs on a hierarchical
run whose every trial collapsed. So:

- a run with more than one trial already has fresh discovery, so no `-N10` row is reachable;
- a two-level run (`-2`) never enters the rescue, so no `-2` row is reachable;
- a `-N1` run whose trial did not collapse never enters the rescue (every non-overlapping row, and
  every overlapping `-d -N1` / `-F -d -N1` row without `--regularized` except om3 / om4 E100000);
- a `-N1` run whose rescue beats the collapse raised the signal before this PR as well (om3 / om4
  `-d -N1` and `-F -d -N1`, om2 E50000 and every E100000 `-d --regularized -N1` row).

What is left is a `-N1` hierarchical run whose lone trial collapsed, whose rescue tied the collapse,
and whose rescue's regroup detector escalated: the six rows above. The session agrees: 171 of the 177
rows are bit-identical.

### Parity with `-2d -N1`, and what it costs

`--regularized`, E50000, `-N1`, this session:

| row | `-2d -N1` | `-d -N1` old | `-d -N1` new | new / `-2d` time |
|---|---|---|---|--:|
| om3 | 7.925216272, 89 top, 0.601s | 7.969664223, 1 top, 0.402s | 7.925216272 (= `-2d`), 0.721s | 1.20× |
| om4 | 7.92621405, 110 top, 0.725s | 7.978790193, 1 top, 0.470s | 7.92621405 (= `-2d`), 0.859s | 1.18× |
| om5 | 7.956672505, 101 top, 0.758s | 7.98862707, 1 top, 0.481s | 7.956672505 (= `-2d`), 0.896s | 1.18× |
| om6 | 7.944047825, 94 top, 0.642s | 7.989209626, 1 top, 0.416s | 7.944047825 (= `-2d`), 0.693s | 1.08× |
| om7 | 7.957532546, 112 top, 0.694s | 7.992344954, 1 top, 0.384s | 7.957532546 (= `-2d`), 0.776s | 1.12× |
| om8 | 7.978376075, 77 top, 0.655s | 7.994534803, 1 top, 0.403s | 7.978376075 (= `-2d`), 0.692s | 1.06× |

**The trade.** −0.20% to −0.66% in bits for +66% to +102% in seconds (+0.28 to +0.42 s on 0.4 s runs),
on `-N1` runs only; `-N10` on the same networks already returns this partition. Against the
marginal-trade rule, the time is not marginal. For the change: the bits are 2–6.6× the 0.1%
regression threshold, and the answer changes kind, from one module (no structure) to the 77–112
modules the same seed's two-level search finds. That is #1081's contract (`-d -N1` does not lose to
`-2d -N1`) on the one family where the rescue only ties. The added time is the repair `-2d -N1`
already pays, on top of the collapsed hierarchical trial. A cheaper route to the same partition would
have to skip one of the two. Skipping the repair is the gap itself. Skipping the hierarchical trial is
#1121's flat-first probe, rejected for +25% to +34% in seconds on healthy memory and metadata `-N1`
rows (F59). The repair's own cost was already cut by #1124: before it, the same line cost +106% to
+205% in seconds for −0.04% to −0.48% in bits (F58, F59).

### Old vs new columnar — standard search (`-C -N10`)

Overlapping and wikispeedia rows run `-C -d -N10`. The change cannot reach a `-N10` row (with more than
one trial the repair has fresh discovery already), so every row is bit-identical and the time column is
the session's noise floor (see "Every cell where new is worse" above for the re-measured cells).

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-9.1%)</td><td align="right">0.1G (-0.42%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.007s (+4.9%)</td><td align="right">0.1G (+0.51%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.048857953 (=)</td><td align="right">0.024s (+3.2%)</td><td align="right">0.3G (-0.05%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.248s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.717760238 (=)</td><td align="right">0.244s (-1.4%)</td><td align="right">2.8G (-0.06%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.060s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.063s (+5.0%)</td><td align="right">0.7G (+0.04%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.02s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">3.08s (+2.1%)</td><td align="right">34.1G (+0.02%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">20.2s</td><td align="right">181.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">20.7s (+2.8%)</td><td align="right">181.9G (+0.04%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (+1.0%)</td><td align="right">0.1G (-6.02%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+9.8%)</td><td align="right">0.1G (-0.48%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.65s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.75s (+3.7%)</td><td align="right">31.5G (+0.06%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.55s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.61s (+1.7%)</td><td align="right">38.6G (-0.02%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.94s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.85s (-2.2%)</td><td align="right">40.4G (-0.03%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">11.0s</td><td align="right">98.0G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">10.4s (-5.2%)</td><td align="right">97.9G (-0.02%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.36s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.30s (-1.8%)</td><td align="right">35.5G (-0.01%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.96s</td><td align="right">47.5G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">5.04s (+1.6%)</td><td align="right">47.5G (+0.00%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.61s</td><td align="right">59.8G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.60s (-0.2%)</td><td align="right">59.8G (-0.00%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.32s</td><td align="right">60.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">6.19s (-2.1%)</td><td align="right">60.6G (+0.01%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">6.78s</td><td align="right">59.0G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">6.60s (-2.8%)</td><td align="right">59.0G (-0.03%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">8.90s</td><td align="right">81.2G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">8.80s (-1.0%)</td><td align="right">81.2G (+0.02%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">10.1s</td><td align="right">82.8G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">10.1s (+0.4%)</td><td align="right">82.7G (-0.02%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">9.84s</td><td align="right">86.4G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">9.84s (+0.0%)</td><td align="right">86.4G (-0.00%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">6.773456578</td><td align="right">3.63s</td><td align="right">33.3G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.52s (-3.0%)</td><td align="right">33.3G (-0.05%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.851498646</td><td align="right">4.73s</td><td align="right">44.6G</td><td align="right">617</td><td align="right">4</td><td align="right">5.851498646 (=)</td><td align="right">4.83s (+2.1%)</td><td align="right">44.6G (+0.01%)</td><td align="right">617</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.876717868</td><td align="right">5.29s</td><td align="right">47.4G</td><td align="right">1031</td><td align="right">4</td><td align="right">4.876717868 (=)</td><td align="right">5.30s (+0.2%)</td><td align="right">47.4G (+0.02%)</td><td align="right">1031</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.150346795</td><td align="right">5.93s</td><td align="right">51.1G</td><td align="right">2171</td><td align="right">4</td><td align="right">4.150346795 (=)</td><td align="right">6.05s (+2.0%)</td><td align="right">51.2G (+0.07%)</td><td align="right">2171</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.617750514</td><td align="right">6.19s</td><td align="right">51.7G</td><td align="right">3050</td><td align="right">4</td><td align="right">3.617750514 (=)</td><td align="right">6.12s (-1.1%)</td><td align="right">51.7G (-0.05%)</td><td align="right">3050</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.201872271</td><td align="right">7.75s</td><td align="right">65.2G</td><td align="right">3424</td><td align="right">6</td><td align="right">3.201872271 (=)</td><td align="right">7.74s (-0.1%)</td><td align="right">65.1G (-0.10%)</td><td align="right">3424</td><td align="right">6</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.883308449</td><td align="right">8.21s</td><td align="right">69.8G</td><td align="right">4367</td><td align="right">5</td><td align="right">2.883308449 (=)</td><td align="right">8.27s (+0.6%)</td><td align="right">69.7G (-0.06%)</td><td align="right">4367</td><td align="right">5</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.718s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.742s (+3.2%)</td><td align="right">7.2G (+0.08%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Overlapping and wikispeedia rows as `-C -2d -N10`. A two-level run never enters the rescue: every row
is bit-identical, and the time column is the session's noise floor.

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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-4.2%)</td><td align="right">0.1G (-0.39%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.861229775</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (=)</td><td align="right">0.007s (-0.6%)</td><td align="right">0.1G (-0.35%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.283072584</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.283072584 (=)</td><td align="right">0.008s (-7.8%)</td><td align="right">0.2G (-0.29%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.63729688</td><td align="right">0.097s</td><td align="right">1.1G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.101s (+3.8%)</td><td align="right">1.1G (-0.08%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739575295</td><td align="right">0.043s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">6.739575295 (=)</td><td align="right">0.042s (-2.0%)</td><td align="right">0.5G (-0.10%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.949978834</td><td align="right">2.37s</td><td align="right">23.8G</td><td align="right">506</td><td align="right">2</td><td align="right">7.949978834 (=)</td><td align="right">2.33s (-1.7%)</td><td align="right">23.8G (-0.02%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.754216663</td><td align="right">20.0s</td><td align="right">117.6G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.754216663 (=)</td><td align="right">20.3s (+1.2%)</td><td align="right">117.5G (-0.04%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-8.0%)</td><td align="right">0.1G (-6.52%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-11.3%)</td><td align="right">0.1G (-9.05%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.400445378</td><td align="right">2.82s</td><td align="right">32.3G</td><td align="right">168</td><td align="right">2</td><td align="right">7.400445378 (=)</td><td align="right">2.89s (+2.7%)</td><td align="right">32.3G (+0.03%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393055049</td><td align="right">3.86s</td><td align="right">41.7G</td><td align="right">334</td><td align="right">2</td><td align="right">5.393055049 (=)</td><td align="right">3.85s (-0.1%)</td><td align="right">41.7G (+0.00%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571539329</td><td align="right">3.87s</td><td align="right">41.2G</td><td align="right">304</td><td align="right">2</td><td align="right">5.571539329 (=)</td><td align="right">3.90s (+0.8%)</td><td align="right">41.2G (+0.02%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.424143707</td><td align="right">10.8s</td><td align="right">104.5G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.424143707 (=)</td><td align="right">11.1s (+2.5%)</td><td align="right">104.6G (+0.01%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.21s</td><td align="right">31.6G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.36s (+4.7%)</td><td align="right">31.6G (-0.02%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.740761645</td><td align="right">4.16s</td><td align="right">35.7G</td><td align="right">625</td><td align="right">2</td><td align="right">6.740761645 (=)</td><td align="right">3.93s (-5.5%)</td><td align="right">35.7G (-0.16%)</td><td align="right">625</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.822832994</td><td align="right">7.85s</td><td align="right">77.3G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">7.09s (-9.7%)</td><td align="right">77.3G (-0.08%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.866901786</td><td align="right">7.26s</td><td align="right">70.7G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">7.20s (-0.9%)</td><td align="right">70.7G (-0.00%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.867301407</td><td align="right">7.08s</td><td align="right">68.4G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">7.25s (+2.3%)</td><td align="right">68.4G (-0.03%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.88496897</td><td align="right">8.11s</td><td align="right">69.0G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">8.18s (+0.8%)</td><td align="right">69.0G (-0.02%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.88992089</td><td align="right">8.00s</td><td align="right">69.2G</td><td align="right">663</td><td align="right">2</td><td align="right">6.88992089 (=)</td><td align="right">7.95s (-0.5%)</td><td align="right">69.2G (+0.00%)</td><td align="right">663</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.88742315</td><td align="right">8.69s</td><td align="right">74.5G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">8.87s (+2.1%)</td><td align="right">74.5G (-0.00%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-2d`</td><td align="right">6.773456578</td><td align="right">4.08s</td><td align="right">37.8G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.83s (-6.1%)</td><td align="right">37.8G (-0.06%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-2d`</td><td align="right">6.258611497</td><td align="right">4.50s</td><td align="right">36.8G</td><td align="right">3236</td><td align="right">2</td><td align="right">6.258611497 (=)</td><td align="right">4.48s (-0.4%)</td><td align="right">36.8G (-0.01%)</td><td align="right">3236</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 E50000 `-2d`</td><td align="right">5.454385527</td><td align="right">4.42s</td><td align="right">33.7G</td><td align="right">4381</td><td align="right">2</td><td align="right">5.454385527 (=)</td><td align="right">4.38s (-0.9%)</td><td align="right">33.7G (-0.01%)</td><td align="right">4381</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 E50000 `-2d`</td><td align="right">4.903270512</td><td align="right">4.39s</td><td align="right">31.9G</td><td align="right">5443</td><td align="right">2</td><td align="right">4.903270512 (=)</td><td align="right">4.33s (-1.6%)</td><td align="right">31.9G (-0.02%)</td><td align="right">5443</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 E50000 `-2d`</td><td align="right">4.468778176</td><td align="right">4.54s</td><td align="right">32.4G</td><td align="right">6379</td><td align="right">2</td><td align="right">4.468778176 (=)</td><td align="right">4.44s (-2.1%)</td><td align="right">32.4G (+0.02%)</td><td align="right">6379</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 E50000 `-2d`</td><td align="right">4.143277395</td><td align="right">4.48s</td><td align="right">31.2G</td><td align="right">7097</td><td align="right">2</td><td align="right">4.143277395 (=)</td><td align="right">4.37s (-2.5%)</td><td align="right">31.2G (-0.02%)</td><td align="right">7097</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 E50000 `-2d`</td><td align="right">3.859069372</td><td align="right">4.52s</td><td align="right">31.5G</td><td align="right">7985</td><td align="right">2</td><td align="right">3.859069372 (=)</td><td align="right">4.56s (+0.9%)</td><td align="right">31.6G (+0.01%)</td><td align="right">7985</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.907904741</td><td align="right">0.722s</td><td align="right">6.9G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.713s (-1.1%)</td><td align="right">6.9G (-0.00%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

Interleaved minimum of 3 per arm. Every row is bit-identical: no row here has a rescue that only ties
(om3 / om4 `-d -N1` are rescued and the rescue wins, so they raised the signal before this PR). The
`-d -N1` rows of om2 / om5–om8 still return a refined hierarchical build 1–14% above `-2d -N1`, the
`-N1` property #1121 documents (F59), and om4 `-N1` beating `-N10` stays #1083.

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.000s (+3.7%)</td><td align="right">0.1G (-0.93%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.899367957</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">2</td><td align="right">6.899367957 (=)</td><td align="right">0.001s (+0.8%)</td><td align="right">0.1G (-0.50%)</td><td align="right">11</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.047459862</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">4</td><td align="right">4.047459862 (=)</td><td align="right">0.003s (-6.0%)</td><td align="right">0.1G (-0.47%)</td><td align="right">7</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.730850312</td><td align="right">0.026s</td><td align="right">0.4G</td><td align="right">12</td><td align="right">5</td><td align="right">4.730850312 (=)</td><td align="right">0.025s (-0.4%)</td><td align="right">0.4G (-0.03%)</td><td align="right">12</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.758265349</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">3</td><td align="right">3</td><td align="right">6.758265349 (=)</td><td align="right">0.009s (+0.2%)</td><td align="right">0.2G (-0.07%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">0.397s</td><td align="right">5.7G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">0.402s (+1.2%)</td><td align="right">5.7G (-0.10%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">2.43s</td><td align="right">24.4G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">2.42s (-0.2%)</td><td align="right">24.4G (-0.09%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.041117399</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.041117399 (=)</td><td align="right">0.001s (-4.8%)</td><td align="right">0.1G (-1.18%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-13.5%)</td><td align="right">0.1G (-0.98%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.491980364</td><td align="right">0.370s</td><td align="right">5.3G</td><td align="right">148</td><td align="right">3</td><td align="right">7.491980364 (=)</td><td align="right">0.370s (+0.0%)</td><td align="right">5.3G (-0.01%)</td><td align="right">148</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.470440768</td><td align="right">0.370s</td><td align="right">4.5G</td><td align="right">242</td><td align="right">3</td><td align="right">5.470440768 (=)</td><td align="right">0.375s (+1.2%)</td><td align="right">4.5G (-0.02%)</td><td align="right">242</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.657913279</td><td align="right">0.497s</td><td align="right">5.9G</td><td align="right">197</td><td align="right">3</td><td align="right">5.657913279 (=)</td><td align="right">0.484s (-2.7%)</td><td align="right">5.9G (-0.08%)</td><td align="right">197</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.546335898</td><td align="right">0.894s</td><td align="right">9.2G</td><td align="right">1614</td><td align="right">3</td><td align="right">7.546335898 (=)</td><td align="right">0.934s (+4.5%)</td><td align="right">9.3G (+0.02%)</td><td align="right">1614</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.460796773</td><td align="right">0.411s</td><td align="right">5.8G</td><td align="right">5</td><td align="right">3</td><td align="right">8.460796773 (=)</td><td align="right">0.440s (+7.1%)</td><td align="right">5.8G (-0.00%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739607071</td><td align="right">1.30s</td><td align="right">14.0G</td><td align="right">669</td><td align="right">2</td><td align="right">6.739607071 (=)</td><td align="right">1.31s (+0.2%)</td><td align="right">14.0G (-0.05%)</td><td align="right">669</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.823562921</td><td align="right">1.15s</td><td align="right">12.9G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.13s (-1.6%)</td><td align="right">12.9G (-0.01%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.861732911</td><td align="right">1.51s</td><td align="right">15.7G</td><td align="right">139</td><td align="right">2</td><td align="right">6.861732911 (=)</td><td align="right">1.50s (-0.8%)</td><td align="right">15.7G (-0.05%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.868180827</td><td align="right">1.68s</td><td align="right">17.2G</td><td align="right">291</td><td align="right">2</td><td align="right">6.868180827 (=)</td><td align="right">1.67s (-1.0%)</td><td align="right">17.2G (+0.03%)</td><td align="right">291</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884236095</td><td align="right">1.68s</td><td align="right">16.3G</td><td align="right">453</td><td align="right">2</td><td align="right">6.884236095 (=)</td><td align="right">1.69s (+0.8%)</td><td align="right">16.3G (-0.02%)</td><td align="right">453</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.889674586</td><td align="right">1.81s</td><td align="right">17.2G</td><td align="right">649</td><td align="right">2</td><td align="right">6.889674586 (=)</td><td align="right">1.82s (+0.4%)</td><td align="right">17.2G (+0.00%)</td><td align="right">649</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.894258582</td><td align="right">1.93s</td><td align="right">17.9G</td><td align="right">907</td><td align="right">2</td><td align="right">6.894258582 (=)</td><td align="right">1.94s (+0.3%)</td><td align="right">17.9G (-0.00%)</td><td align="right">907</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.29196634</td><td align="right">0.371s</td><td align="right">3.7G</td><td align="right">321</td><td align="right">4</td><td align="right">7.29196634 (=)</td><td align="right">0.378s (+1.9%)</td><td align="right">3.7G (+0.04%)</td><td align="right">321</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.823562921</td><td align="right">1.48s</td><td align="right">17.0G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.49s (+0.3%)</td><td align="right">17.0G (+0.03%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.861732911</td><td align="right">1.93s</td><td align="right">20.7G</td><td align="right">139</td><td align="right">2</td><td align="right">6.861732911 (=)</td><td align="right">1.96s (+2.0%)</td><td align="right">20.7G (+0.01%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.812252898</td><td align="right">0.762s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.812252898 (=)</td><td align="right">0.765s (+0.4%)</td><td align="right">7.2G (-0.10%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.440161581</td><td align="right">0.835s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.440161581 (=)</td><td align="right">0.810s (-3.0%)</td><td align="right">7.5G (+0.14%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.192756466</td><td align="right">0.827s</td><td align="right">7.5G</td><td align="right">149</td><td align="right">4</td><td align="right">7.192756466 (=)</td><td align="right">0.852s (+3.0%)</td><td align="right">7.5G (+0.03%)</td><td align="right">149</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.967535764</td><td align="right">0.828s</td><td align="right">7.5G</td><td align="right">206</td><td align="right">4</td><td align="right">6.967535764 (=)</td><td align="right">0.822s (-0.8%)</td><td align="right">7.5G (+0.03%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">7.144517469</td><td align="right">0.429s</td><td align="right">4.5G</td><td align="right">3</td><td align="right">3</td><td align="right">7.144517469 (=)</td><td align="right">0.439s (+2.4%)</td><td align="right">4.5G (-0.00%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.854829799</td><td align="right">0.446s</td><td align="right">4.4G</td><td align="right">537</td><td align="right">4</td><td align="right">5.854829799 (=)</td><td align="right">0.442s (-0.7%)</td><td align="right">4.4G (-0.05%)</td><td align="right">537</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.898784516</td><td align="right">0.448s</td><td align="right">4.2G</td><td align="right">1867</td><td align="right">3</td><td align="right">4.898784516 (=)</td><td align="right">0.449s (+0.3%)</td><td align="right">4.2G (-0.01%)</td><td align="right">1867</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.1554786</td><td align="right">0.556s</td><td align="right">5.0G</td><td align="right">2156</td><td align="right">4</td><td align="right">4.1554786 (=)</td><td align="right">0.561s (+0.9%)</td><td align="right">5.0G (+0.02%)</td><td align="right">2156</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.620157111</td><td align="right">0.701s</td><td align="right">6.0G</td><td align="right">3056</td><td align="right">4</td><td align="right">3.620157111 (=)</td><td align="right">0.681s (-2.8%)</td><td align="right">5.9G (-0.16%)</td><td align="right">3056</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.21639819</td><td align="right">0.643s</td><td align="right">5.6G</td><td align="right">3417</td><td align="right">5</td><td align="right">3.21639819 (=)</td><td align="right">0.658s (+2.3%)</td><td align="right">5.6G (+0.32%)</td><td align="right">3417</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.885785338</td><td align="right">0.950s</td><td align="right">8.1G</td><td align="right">4371</td><td align="right">5</td><td align="right">2.885785338 (=)</td><td align="right">0.949s (-0.1%)</td><td align="right">8.1G (+0.15%)</td><td align="right">4371</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om2 `-2d -c` planted</td><td align="right">6.744721993</td><td align="right">0.568s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.744721993 (=)</td><td align="right">0.576s (+1.4%)</td><td align="right">6.2G (-0.08%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d -c` planted</td><td align="right">6.820929855</td><td align="right">0.414s</td><td align="right">4.3G</td><td align="right">50</td><td align="right">2</td><td align="right">6.820929855 (=)</td><td align="right">0.409s (-1.3%)</td><td align="right">4.3G (-0.10%)</td><td align="right">50</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d -c` planted</td><td align="right">6.856285862</td><td align="right">0.730s</td><td align="right">7.4G</td><td align="right">129</td><td align="right">2</td><td align="right">6.856285862 (=)</td><td align="right">0.706s (-3.3%)</td><td align="right">7.3G (-0.04%)</td><td align="right">129</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d -c` planted</td><td align="right">6.857876353</td><td align="right">0.919s</td><td align="right">9.5G</td><td align="right">287</td><td align="right">2</td><td align="right">6.857876353 (=)</td><td align="right">0.919s (-0.0%)</td><td align="right">9.5G (+0.02%)</td><td align="right">287</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d -c` planted</td><td align="right">6.873464257</td><td align="right">1.12s</td><td align="right">11.3G</td><td align="right">444</td><td align="right">2</td><td align="right">6.873464257 (=)</td><td align="right">1.15s (+3.1%)</td><td align="right">11.3G (+0.04%)</td><td align="right">444</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d -c` planted</td><td align="right">6.881554086</td><td align="right">0.975s</td><td align="right">9.6G</td><td align="right">624</td><td align="right">2</td><td align="right">6.881554086 (=)</td><td align="right">0.997s (+2.3%)</td><td align="right">9.6G (-0.01%)</td><td align="right">624</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d -c` planted</td><td align="right">6.875540042</td><td align="right">1.23s</td><td align="right">11.8G</td><td align="right">885</td><td align="right">2</td><td align="right">6.875540042 (=)</td><td align="right">1.22s (-0.2%)</td><td align="right">11.8G (+0.00%)</td><td align="right">885</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">6.066305904</td><td align="right">0.071s</td><td align="right">0.8G</td><td align="right">187</td><td align="right">3</td><td align="right">6.066305904 (=)</td><td align="right">0.073s (+3.0%)</td><td align="right">0.8G (+0.03%)</td><td align="right">187</td><td align="right">3</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.093s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.093s (+0.2%)</td><td align="right">1.0G (-0.11%)</td><td align="right">184</td><td align="right">2</td></tr>
</tbody>
</table>

### The overlapping family in full

Every configuration of the planted overlapping state networks, both arms, at both trigram densities
(F55). The change moves the six E50000 `-d --regularized -N1` rows, om3–om8, from one module to
`-2d -N1`'s partition (−0.20% to −0.66% in bits, +66% to +102% in seconds; "Parity with `-2d -N1`"
above). Every other row is bit-identical, including om2 E50000 and every E100000
`-d --regularized -N1` row, whose rescue beats the collapse.

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
<tr><td align="right">om2 E100000 `-2d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">4.21s</td><td align="right">36.8G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">4.01s (-4.6%)</td><td align="right">36.8G (-0.08%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 E100000 `-d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">3.38s</td><td align="right">33.5G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.56s (+5.5%)</td><td align="right">33.5G (+0.04%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.72s</td><td align="right">24.8G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.52s (-7.2%)</td><td align="right">24.8G (-0.06%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N1`</td><td align="right">7.548863183</td><td align="right">0.707s</td><td align="right">7.2G</td><td align="right">121</td><td align="right">2</td><td align="right">7.548863183 (=)</td><td align="right">0.711s (+0.6%)</td><td align="right">7.2G (+0.01%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.42s</td><td align="right">23.8G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.42s (-0.2%)</td><td align="right">23.8G (-0.03%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N1`</td><td align="right">7.548863183</td><td align="right">0.855s</td><td align="right">8.5G</td><td align="right">121</td><td align="right">2</td><td align="right">7.548863183 (=)</td><td align="right">0.843s (-1.4%)</td><td align="right">8.5G (+0.02%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om2 planted, `-2d --no-infomap -c`</td><td align="right">6.789039995</td><td align="right">0.056s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.789039995 (=)</td><td align="right">0.055s (-1.8%)</td><td align="right">0.6G (+0.04%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N10`</td><td align="right">7.925216272</td><td align="right">2.54s</td><td align="right">22.5G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">2.59s (+2.0%)</td><td align="right">22.5G (+0.01%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N10`</td><td align="right">7.925216272</td><td align="right">2.54s</td><td align="right">22.7G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">2.55s (+0.1%)</td><td align="right">22.7G (-0.04%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">5.00s</td><td align="right">48.0G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">5.01s (+0.2%)</td><td align="right">48.0G (+0.02%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">0.923s</td><td align="right">9.2G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">0.919s (-0.4%)</td><td align="right">9.2G (+0.03%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">4.55s</td><td align="right">45.0G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">4.62s (+1.6%)</td><td align="right">45.0G (-0.00%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">1.17s</td><td align="right">12.0G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">1.17s (+0.2%)</td><td align="right">12.0G (+0.01%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 planted, `-2d --no-infomap -c`</td><td align="right">6.837980937</td><td align="right">0.101s</td><td align="right">0.9G</td><td align="right">12</td><td align="right">2</td><td align="right">6.837980937 (=)</td><td align="right">0.101s (-0.3%)</td><td align="right">0.9G (+0.12%)</td><td align="right">12</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N10`</td><td align="right">7.92621405</td><td align="right">2.70s</td><td align="right">23.0G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">2.70s (+0.3%)</td><td align="right">23.0G (+0.02%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N10`</td><td align="right">7.92621405</td><td align="right">2.94s</td><td align="right">22.9G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">2.89s (-1.5%)</td><td align="right">22.9G (-0.03%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N10`</td><td align="right">7.556894677</td><td align="right">5.99s</td><td align="right">49.0G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">5.85s (-2.2%)</td><td align="right">49.0G (-0.01%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">0.984s</td><td align="right">9.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">0.981s (-0.3%)</td><td align="right">9.1G (-0.02%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N10`</td><td align="right">7.556653713</td><td align="right">5.56s</td><td align="right">48.6G</td><td align="right">78</td><td align="right">2</td><td align="right">7.556653713 (=)</td><td align="right">5.77s (+3.8%)</td><td align="right">48.7G (+0.13%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">1.21s</td><td align="right">11.7G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">1.25s (+3.7%)</td><td align="right">11.7G (-0.00%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 planted, `-2d --no-infomap -c`</td><td align="right">6.880650147</td><td align="right">0.117s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.880650147 (=)</td><td align="right">0.115s (-1.7%)</td><td align="right">1.0G (-0.13%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N10`</td><td align="right">7.956672505</td><td align="right">2.96s</td><td align="right">23.3G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">2.89s (-2.3%)</td><td align="right">23.3G (-0.04%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N10`</td><td align="right">7.956672505</td><td align="right">2.86s</td><td align="right">22.5G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">2.85s (-0.4%)</td><td align="right">22.5G (+0.06%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N10`</td><td align="right">7.968061942</td><td align="right">5.08s</td><td align="right">43.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.968061942 (=)</td><td align="right">4.96s (-2.3%)</td><td align="right">43.1G (+0.01%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N1`</td><td align="right">7.968629202</td><td align="right">0.880s</td><td align="right">7.8G</td><td align="right">79</td><td align="right">2</td><td align="right">7.968629202 (=)</td><td align="right">0.886s (+0.7%)</td><td align="right">7.8G (-0.01%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N10`</td><td align="right">7.968320007</td><td align="right">5.10s</td><td align="right">42.5G</td><td align="right">81</td><td align="right">2</td><td align="right">7.968320007 (=)</td><td align="right">5.16s (+1.1%)</td><td align="right">42.5G (-0.00%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N1`</td><td align="right">7.968629202</td><td align="right">1.13s</td><td align="right">10.5G</td><td align="right">79</td><td align="right">2</td><td align="right">7.968629202 (=)</td><td align="right">1.15s (+1.8%)</td><td align="right">10.5G (+0.05%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 planted, `-2d --no-infomap -c`</td><td align="right">6.902222527</td><td align="right">0.129s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.902222527 (=)</td><td align="right">0.132s (+2.8%)</td><td align="right">1.1G (-0.04%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N10`</td><td align="right">7.944047825</td><td align="right">2.79s</td><td align="right">21.9G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">2.72s (-2.3%)</td><td align="right">21.9G (-0.01%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N10`</td><td align="right">7.944047825</td><td align="right">2.80s</td><td align="right">21.6G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">2.82s (+0.7%)</td><td align="right">21.5G (-0.05%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N10`</td><td align="right">7.982650944</td><td align="right">5.47s</td><td align="right">41.3G</td><td align="right">105</td><td align="right">2</td><td align="right">7.982650944 (=)</td><td align="right">5.07s (-7.4%)</td><td align="right">41.2G (-0.15%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N1`</td><td align="right">7.982650944</td><td align="right">0.719s</td><td align="right">6.0G</td><td align="right">105</td><td align="right">2</td><td align="right">7.982650944 (=)</td><td align="right">0.744s (+3.4%)</td><td align="right">6.0G (+0.02%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N10`</td><td align="right">7.984417755</td><td align="right">4.94s</td><td align="right">41.2G</td><td align="right">102</td><td align="right">2</td><td align="right">7.984417755 (=)</td><td align="right">4.89s (-1.1%)</td><td align="right">41.2G (-0.01%)</td><td align="right">102</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N1`</td><td align="right">7.982650944</td><td align="right">1.03s</td><td align="right">8.7G</td><td align="right">105</td><td align="right">2</td><td align="right">7.982650944 (=)</td><td align="right">0.988s (-3.9%)</td><td align="right">8.7G (+0.04%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 planted, `-2d --no-infomap -c`</td><td align="right">6.930934993</td><td align="right">0.133s</td><td align="right">1.1G</td><td align="right">24</td><td align="right">2</td><td align="right">6.930934993 (=)</td><td align="right">0.131s (-1.4%)</td><td align="right">1.1G (-0.06%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N10`</td><td align="right">7.957532546</td><td align="right">2.97s</td><td align="right">23.3G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">2.94s (-0.8%)</td><td align="right">23.3G (+0.02%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N10`</td><td align="right">7.957532546</td><td align="right">3.06s</td><td align="right">22.3G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">2.96s (-3.3%)</td><td align="right">22.3G (-0.03%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N10`</td><td align="right">7.948827768</td><td align="right">5.21s</td><td align="right">41.2G</td><td align="right">166</td><td align="right">2</td><td align="right">7.948827768 (=)</td><td align="right">5.18s (-0.6%)</td><td align="right">41.2G (-0.01%)</td><td align="right">166</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N1`</td><td align="right">7.950842444</td><td align="right">0.773s</td><td align="right">6.4G</td><td align="right">168</td><td align="right">2</td><td align="right">7.950842444 (=)</td><td align="right">0.810s (+4.8%)</td><td align="right">6.4G (-0.00%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N10`</td><td align="right">7.948827768</td><td align="right">5.12s</td><td align="right">41.1G</td><td align="right">166</td><td align="right">2</td><td align="right">7.948827768 (=)</td><td align="right">5.06s (-1.2%)</td><td align="right">41.1G (-0.00%)</td><td align="right">166</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N1`</td><td align="right">7.950842444</td><td align="right">1.04s</td><td align="right">9.1G</td><td align="right">168</td><td align="right">2</td><td align="right">7.950842444 (=)</td><td align="right">1.04s (+0.6%)</td><td align="right">9.1G (-0.06%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">om7 planted, `-2d --no-infomap -c`</td><td align="right">6.957516072</td><td align="right">0.162s</td><td align="right">1.3G</td><td align="right">28</td><td align="right">2</td><td align="right">6.957516072 (=)</td><td align="right">0.154s (-5.0%)</td><td align="right">1.3G (-0.12%)</td><td align="right">28</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N10`</td><td align="right">7.978376075</td><td align="right">2.90s</td><td align="right">22.6G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">2.95s (+1.9%)</td><td align="right">22.6G (-0.04%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N10`</td><td align="right">7.978376075</td><td align="right">2.89s</td><td align="right">21.7G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">2.91s (+0.6%)</td><td align="right">21.7G (-0.02%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N10`</td><td align="right">7.979831947</td><td align="right">5.28s</td><td align="right">40.7G</td><td align="right">221</td><td align="right">2</td><td align="right">7.979831947 (=)</td><td align="right">5.22s (-1.1%)</td><td align="right">40.7G (+0.01%)</td><td align="right">221</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N1`</td><td align="right">7.983599366</td><td align="right">0.768s</td><td align="right">6.2G</td><td align="right">216</td><td align="right">2</td><td align="right">7.983599366 (=)</td><td align="right">0.758s (-1.2%)</td><td align="right">6.2G (+0.00%)</td><td align="right">216</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N10`</td><td align="right">7.978630877</td><td align="right">5.08s</td><td align="right">41.3G</td><td align="right">236</td><td align="right">2</td><td align="right">7.978630877 (=)</td><td align="right">5.05s (-0.5%)</td><td align="right">41.3G (+0.01%)</td><td align="right">236</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N1`</td><td align="right">7.983599366</td><td align="right">1.05s</td><td align="right">8.9G</td><td align="right">216</td><td align="right">2</td><td align="right">7.983599366 (=)</td><td align="right">1.03s (-1.2%)</td><td align="right">8.9G (+0.00%)</td><td align="right">216</td><td align="right">2</td></tr>
<tr><td align="right">om8 planted, `-2d --no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.150s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.147s (-2.3%)</td><td align="right">1.2G (-0.08%)</td><td align="right">32</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N1`</td><td align="right">7.925216272</td><td align="right">0.594s</td><td align="right">5.3G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">0.601s (+1.2%)</td><td align="right">5.3G (-0.03%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N1`</td><td align="right">7.969664223</td><td align="right">0.402s</td><td align="right">3.9G</td><td align="right">1</td><td align="right">2</td><td align="right">7.925216272 (-0.5577%)</td><td align="right">0.721s (+79.5%)</td><td align="right">6.6G (+70.81%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N1`</td><td align="right">7.92621405</td><td align="right">0.722s</td><td align="right">6.2G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">0.725s (+0.4%)</td><td align="right">6.2G (+0.00%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N1`</td><td align="right">7.978790193</td><td align="right">0.470s</td><td align="right">4.1G</td><td align="right">1</td><td align="right">2</td><td align="right">7.92621405 (-0.6589%)</td><td align="right">0.859s (+82.8%)</td><td align="right">7.6G (+82.61%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N1`</td><td align="right">7.956672505</td><td align="right">0.765s</td><td align="right">6.3G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">0.758s (-0.9%)</td><td align="right">6.3G (+0.02%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N1`</td><td align="right">7.98862707</td><td align="right">0.481s</td><td align="right">4.2G</td><td align="right">1</td><td align="right">2</td><td align="right">7.956672505 (-0.4000%)</td><td align="right">0.896s (+86.4%)</td><td align="right">7.7G (+80.77%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N1`</td><td align="right">7.944047825</td><td align="right">0.640s</td><td align="right">5.2G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">0.642s (+0.2%)</td><td align="right">5.2G (+0.09%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N1`</td><td align="right">7.989209626</td><td align="right">0.416s</td><td align="right">3.3G</td><td align="right">1</td><td align="right">2</td><td align="right">7.944047825 (-0.5653%)</td><td align="right">0.693s (+66.5%)</td><td align="right">5.6G (+68.15%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N1`</td><td align="right">7.957532546</td><td align="right">0.704s</td><td align="right">5.8G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">0.694s (-1.5%)</td><td align="right">5.8G (-0.06%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N1`</td><td align="right">7.992344954</td><td align="right">0.384s</td><td align="right">3.1G</td><td align="right">1</td><td align="right">2</td><td align="right">7.957532546 (-0.4356%)</td><td align="right">0.776s (+102.2%)</td><td align="right">6.2G (+98.00%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N1`</td><td align="right">7.978376075</td><td align="right">0.640s</td><td align="right">5.2G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">0.655s (+2.3%)</td><td align="right">5.2G (-0.05%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N1`</td><td align="right">7.994534803</td><td align="right">0.403s</td><td align="right">3.1G</td><td align="right">1</td><td align="right">2</td><td align="right">7.978376075 (-0.2021%)</td><td align="right">0.692s (+71.5%)</td><td align="right">5.5G (+77.06%)</td><td align="right">77</td><td align="right">2</td></tr>
</tbody>
</table>

### `-F` on the family, old vs new

`optimizeFlexible` hands a collapsed lone trial to the same rescue, so the change could reach
`-F -d -N1`; on these rows the rescue either wins (om3 / om4) or never runs, and every row is
bit-identical. `-N10`:

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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.44s</td><td align="right">40.3G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">3.98s (-10.3%)</td><td align="right">40.3G (-0.20%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.49s</td><td align="right">55.5G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.33s (-2.9%)</td><td align="right">55.5G (-0.01%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">5.76s</td><td align="right">56.0G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.82s (+1.0%)</td><td align="right">56.0G (+0.00%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">6.05s</td><td align="right">54.2G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">5.96s (-1.6%)</td><td align="right">54.2G (-0.00%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">7.85s</td><td align="right">66.6G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">7.56s (-3.7%)</td><td align="right">66.6G (-0.03%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">8.06s</td><td align="right">69.7G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">8.20s (+1.7%)</td><td align="right">69.7G (+0.02%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">8.72s</td><td align="right">74.7G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">8.69s (-0.3%)</td><td align="right">74.7G (+0.00%)</td><td align="right">919</td><td align="right">2</td></tr>
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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.321678354</td><td align="right">0.274s</td><td align="right">2.6G</td><td align="right">436</td><td align="right">4</td><td align="right">7.321678354 (=)</td><td align="right">0.277s (+1.1%)</td><td align="right">2.6G (+0.07%)</td><td align="right">436</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.823562921</td><td align="right">1.45s</td><td align="right">16.6G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.44s (-0.6%)</td><td align="right">16.6G (+0.00%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.861732911</td><td align="right">1.75s</td><td align="right">18.4G</td><td align="right">139</td><td align="right">2</td><td align="right">6.861732911 (=)</td><td align="right">1.74s (-0.5%)</td><td align="right">18.4G (-0.02%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.798283664</td><td align="right">0.667s</td><td align="right">5.8G</td><td align="right">96</td><td align="right">4</td><td align="right">7.798283664 (=)</td><td align="right">0.668s (+0.1%)</td><td align="right">5.8G (+0.06%)</td><td align="right">96</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.507066407</td><td align="right">0.692s</td><td align="right">5.8G</td><td align="right">87</td><td align="right">4</td><td align="right">7.507066407 (=)</td><td align="right">0.720s (+4.1%)</td><td align="right">5.8G (+0.01%)</td><td align="right">87</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.238675817</td><td align="right">0.619s</td><td align="right">5.2G</td><td align="right">145</td><td align="right">4</td><td align="right">7.238675817 (=)</td><td align="right">0.621s (+0.3%)</td><td align="right">5.2G (+0.10%)</td><td align="right">145</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">7.022972054</td><td align="right">0.623s</td><td align="right">5.2G</td><td align="right">201</td><td align="right">4</td><td align="right">7.022972054 (=)</td><td align="right">0.617s (-1.0%)</td><td align="right">5.2G (+0.07%)</td><td align="right">201</td><td align="right">4</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-11.1%)</td><td align="right">0.1G (-1.74%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.007s (-3.5%)</td><td align="right">0.1G (-2.54%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.024s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.03324474 (-0.3856%)</td><td align="right">0.015s (-37.5%)</td><td align="right">0.2G (-29.36%)</td><td align="right">9</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.244s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.754013143 (+0.7684%)</td><td align="right">0.146s (-40.2%)</td><td align="right">1.7G (-37.66%)</td><td align="right">10</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.063s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.057s (-8.6%)</td><td align="right">0.7G (-4.11%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.08s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">2.94s (-4.6%)</td><td align="right">33.1G (-2.98%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">20.7s</td><td align="right">181.9G</td><td align="right">5</td><td align="right">6</td><td align="right">5.620539396 (+1.1539%)</td><td align="right">15.4s (-25.7%)</td><td align="right">126.7G (-30.30%)</td><td align="right">135</td><td align="right">5</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-2.8%)</td><td align="right">0.1G (-1.42%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+9.5%)</td><td align="right">0.1G (-0.14%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.75s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.68s (-2.5%)</td><td align="right">30.6G (-2.67%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.61s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.25s (-9.9%)</td><td align="right">35.9G (-6.96%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.85s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.37s (-12.5%)</td><td align="right">35.2G (-12.69%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.4s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">10.2s (-2.5%)</td><td align="right">95.4G (-2.58%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.30s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.89s (+17.9%)</td><td align="right">34.8G (-2.01%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">5.04s</td><td align="right">47.5G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">3.98s (-20.9%)</td><td align="right">40.3G (-15.31%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.60s</td><td align="right">59.8G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.33s (-4.9%)</td><td align="right">55.5G (-7.28%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.19s</td><td align="right">60.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.82s (-5.9%)</td><td align="right">56.0G (-7.65%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">6.60s</td><td align="right">59.0G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">5.96s (-9.7%)</td><td align="right">54.2G (-7.99%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">8.80s</td><td align="right">81.2G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">7.56s (-14.1%)</td><td align="right">66.6G (-17.98%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">10.1s</td><td align="right">82.7G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">8.20s (-19.2%)</td><td align="right">69.7G (-15.74%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">9.84s</td><td align="right">86.4G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">8.69s (-11.6%)</td><td align="right">74.7G (-13.53%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.742s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.691s (-6.8%)</td><td align="right">6.9G (-4.10%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.078067323 (-8.7135%)</td><td align="right">0.001s (+2.6%)</td><td align="right">0.1G (+2.21%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367 (+0.0797%)</td><td align="right">0.007s (-1.7%)</td><td align="right">0.1G (-0.59%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.024s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">3.892209764 (-3.8689%)</td><td align="right">0.023s (-2.6%)</td><td align="right">0.3G (+0.93%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.244s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.509265423 (-4.4194%)</td><td align="right">0.245s (+0.4%)</td><td align="right">2.9G (+3.35%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.063s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502 (+0.7165%)</td><td align="right">0.061s (-3.2%)</td><td align="right">0.7G (+2.12%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.08s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">8.009172258 (+2.5773%)</td><td align="right">2.79s (-9.3%)</td><td align="right">31.2G (-8.71%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">20.7s</td><td align="right">181.9G</td><td align="right">5</td><td align="right">6</td><td align="right">5.512433077 (-0.7917%)</td><td align="right">20.3s (-1.9%)</td><td align="right">182.9G (+0.59%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653 (-0.8182%)</td><td align="right">0.004s (-12.1%)</td><td align="right">0.1G (-0.52%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (-4.1040%)</td><td align="right">0.001s (-9.4%)</td><td align="right">0.1G (+0.18%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.75s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.438064454 (+0.6171%)</td><td align="right">3.14s (+14.2%)</td><td align="right">33.9G (+7.82%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.61s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.378824196 (-0.2496%)</td><td align="right">3.52s (-2.3%)</td><td align="right">38.4G (-0.40%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.85s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.56659036 (-0.1426%)</td><td align="right">3.83s (-0.4%)</td><td align="right">40.1G (-0.59%)</td><td align="right">220</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.4s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.192724425 (-3.0848%)</td><td align="right">9.51s (-8.8%)</td><td align="right">86.0G (-12.23%)</td><td align="right">1910</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.30s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451 (+2.5761%)</td><td align="right">3.31s (+0.1%)</td><td align="right">35.4G (-0.43%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.742s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.903208727 (-0.0795%)</td><td align="right">0.674s (-9.2%)</td><td align="right">6.9G (-4.51%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-79.0%)</td><td align="right">0.1G (-30.09%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">5</td><td align="right">2</td><td align="right">6.862755928 (-0.0042%)</td><td align="right">0.007s (-68.9%)</td><td align="right">0.1G (-54.66%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.026557116</td><td align="right">0.119s</td><td align="right">1.3G</td><td align="right">12</td><td align="right">5</td><td align="right">4.048857953 (+0.5538%)</td><td align="right">0.024s (-80.1%)</td><td align="right">0.3G (-76.44%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.736412597</td><td align="right">1.89s</td><td align="right">20.2G</td><td align="right">11</td><td align="right">6</td><td align="right">4.717760238 (-0.3938%)</td><td align="right">0.244s (-87.1%)</td><td align="right">2.8G (-86.12%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.738927979</td><td align="right">0.136s</td><td align="right">1.4G</td><td align="right">80</td><td align="right">3</td><td align="right">6.740943136 (+0.0299%)</td><td align="right">0.063s (-53.8%)</td><td align="right">0.7G (-49.75%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.805465772</td><td align="right">7.94s</td><td align="right">63.0G</td><td align="right">220</td><td align="right">4</td><td align="right">7.807937174 (+0.0317%)</td><td align="right">3.08s (-61.2%)</td><td align="right">34.1G (-45.81%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.55442136</td><td align="right">155.7s</td><td align="right">1115.4G</td><td align="right">766</td><td align="right">9</td><td align="right">5.556421705 (+0.0360%)</td><td align="right">20.7s (-86.7%)</td><td align="right">181.9G (-83.70%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-66.1%)</td><td align="right">0.1G (-49.70%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-41.7%)</td><td align="right">0.1G (-37.21%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502028585</td><td align="right">9.20s</td><td align="right">78.1G</td><td align="right">144</td><td align="right">3</td><td align="right">7.392442593 (-1.4608%)</td><td align="right">2.75s (-70.1%)</td><td align="right">31.5G (-59.70%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392441014</td><td align="right">11.9s</td><td align="right">120.6G</td><td align="right">251</td><td align="right">3</td><td align="right">5.392285003 (-0.0029%)</td><td align="right">3.61s (-69.7%)</td><td align="right">38.6G (-68.02%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578435633</td><td align="right">7.76s</td><td align="right">81.7G</td><td align="right">301</td><td align="right">3</td><td align="right">5.574537176 (-0.0699%)</td><td align="right">3.85s (-50.4%)</td><td align="right">40.4G (-50.59%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">8.432467909</td><td align="right">5.55s</td><td align="right">52.1G</td><td align="right">114</td><td align="right">4</td><td align="right">7.421664324 (-11.9870%)</td><td align="right">10.4s (+88.0%)</td><td align="right">97.9G (+87.97%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">7.938575228</td><td align="right">6.93s</td><td align="right">57.0G</td><td align="right">25</td><td align="right">4</td><td align="right">8.235585529 (+3.7414%)</td><td align="right">3.30s (-52.4%)</td><td align="right">35.5G (-37.71%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.88554258</td><td align="right">2.23s</td><td align="right">22.4G</td><td align="right">184</td><td align="right">3</td><td align="right">5.907904741 (+0.3800%)</td><td align="right">0.742s (-66.7%)</td><td align="right">7.2G (-67.93%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-51.5%)</td><td align="right">0.1G (-37.26%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">5</td><td align="right">2</td><td align="right">6.861229775 (-0.0265%)</td><td align="right">0.007s (-44.6%)</td><td align="right">0.1G (-33.06%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.285012668</td><td align="right">0.031s</td><td align="right">0.4G</td><td align="right">56</td><td align="right">2</td><td align="right">4.283072584 (-0.0453%)</td><td align="right">0.008s (-73.1%)</td><td align="right">0.2G (-60.66%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.600443859</td><td align="right">0.698s</td><td align="right">5.9G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.101s (-85.6%)</td><td align="right">1.1G (-80.53%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739721413</td><td align="right">0.168s</td><td align="right">0.8G</td><td align="right">80</td><td align="right">2</td><td align="right">6.739575295 (-0.0022%)</td><td align="right">0.042s (-74.9%)</td><td align="right">0.5G (-35.28%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.9500396</td><td align="right">4.41s</td><td align="right">29.3G</td><td align="right">496</td><td align="right">2</td><td align="right">7.949978834 (-0.0008%)</td><td align="right">2.33s (-47.1%)</td><td align="right">23.8G (-18.62%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.742988533</td><td align="right">45.2s</td><td align="right">251.8G</td><td align="right">11809</td><td align="right">2</td><td align="right">6.754216663 (+0.1665%)</td><td align="right">20.3s (-55.2%)</td><td align="right">117.5G (-53.33%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-50.5%)</td><td align="right">0.1G (-3.21%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-34.5%)</td><td align="right">0.1G (-38.58%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.50595639</td><td align="right">6.58s</td><td align="right">55.2G</td><td align="right">142</td><td align="right">2</td><td align="right">7.400445378 (-1.4057%)</td><td align="right">2.89s (-56.0%)</td><td align="right">32.3G (-41.50%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393312779</td><td align="right">4.70s</td><td align="right">43.9G</td><td align="right">332</td><td align="right">2</td><td align="right">5.393055049 (-0.0048%)</td><td align="right">3.85s (-18.1%)</td><td align="right">41.7G (-4.93%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.579216889</td><td align="right">5.84s</td><td align="right">58.6G</td><td align="right">301</td><td align="right">2</td><td align="right">5.571539329 (-0.1376%)</td><td align="right">3.90s (-33.2%)</td><td align="right">41.2G (-29.64%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.131110023</td><td align="right">5.04s</td><td align="right">36.7G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (+1.2849%)</td><td align="right">3.36s (-33.3%)</td><td align="right">31.6G (-13.97%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.892212121</td><td align="right">1.70s</td><td align="right">17.2G</td><td align="right">184</td><td align="right">2</td><td align="right">5.907904741 (+0.2663%)</td><td align="right">0.713s (-58.0%)</td><td align="right">6.9G (-59.96%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>
