## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`); `time` is `--timing-json`'s `timing.total_s`. One run per `-N10` row (deterministic; `instr` carries the comparison); interleaved minimum of 3 for `-N1` rows. Driver and every row: [`columnar_wip/bench-1127.py`](columnar_wip/bench-1127.py), [`columnar_wip/1127-ab-results.tsv`](columnar_wip/1127-ab-results.tsv).

> **This PR lets the terminal dissolve reach the runner-up trial's tree (#1127).** The dissolve (#1074)
> runs once per run on the settled winner, but the winner is picked on the rectangular trees and the
> dissolve can reorder them. On netsci `-C -N10` the winner's tree dissolves to 4.0489 while the
> second-best trial's dissolves to 4.0236. Each trial of a multi-trial hierarchical run now prices its own
> dissolve on its stack while that stack exists: the pass's gain alone, with no score and no ragged result.
> The non-winning trial that would end lowest keeps its tree. At the end of the run that tree is dissolved
> instead of the winner's when nothing above changed the winner (one dissolve either way), or after the
> winner's when the deep repair moved it. Three rows move, all better in bits: netsci −0.62%, powergrid
> −0.11%, om4 E50000 `-d` −0.060%, at +0.66% / +0.37% / +1.35% in instructions. The 43 other rows the
> change runs on pay −0.24% to +0.54% in instructions (median +0.13%). `-N1`, `-2` and L\* rows never run
> it.

> **Old** = a fresh `MODE=release OPENMP=0` build of `columnar-hierarchical-core` tip `b478b244`, md5
> `4586def7ac957ef2a135412345b21df0`. That is the #1125 snapshot's new binary bit for bit, and its column
> here reproduces that snapshot's new column on all 177 rows (median +0.05% in instructions, +2.8% in
> seconds). **New** = this PR at `c37d7436`, md5 `54c8b6b21ab1b54dd3931a9c7f79833e`. One session, arms
> interleaved per row, `-N1` rows as the minimum of 3 reps spread across the batch, `-N10` rows once.
> Load was 13–18 during the session and 25 during the re-measure, so read `instr` before `time`. **The
> object-oriented arms are not re-run**: this PR does not touch that engine (project instructions). Their
> cells in the two OO tables are carried from the #1079-day session on the same machine, at the precision
> printed there; the columnar cells next to them are this session's.

### What the change moves

Every configuration where old and new differ in bits, both arms: two base-objective rows, where the deep
repair is a no-op and the runner-up is dissolved instead of the winner, and one om row, where the repair
moved the winner and the runner-up is dissolved after it.

| network | table | old bits | new bits | Δbits | old instr | new instr | Δinstr | old time | new time | Δtime |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| powergrid | `-C` | 4.717760238 | **4.712391773** | **-0.1138%** | 2.8G | 2.8G | +0.37% | 0.245s | 0.250s | +2.1% |
| netscicoauthor2010 | `-C` | 4.048857953 | **4.023567105** | **-0.6246%** | 0.3G | 0.3G | +0.66% | 0.024s | 0.023s | -3.6% |
| overlapping om4 E50000 `-d` | `-C` | 4.876717868 | **4.873775655** | **-0.0603%** | 47.4G | 48.1G | +1.35% | 5.35s | 5.52s | +3.3% |

**Every cell where new is worse than old, and why.**

- **Bits:** no cell is worse.
- **Instructions:** om4 E50000 `-d -N10` is the one row over 1% (+1.35%, +3.3% in seconds), for its
  −0.060% in bits. The repair moved its winner, so the runner-up gets its own stack build and dissolve
  after the winner's. Without that second block it stays at the tip's bits for +0.30% (attribution
  below).
- **Bit-identical rows the change runs on** (hierarchical `-N10`, 43 rows of 10 ms or more): −0.24% to
  +0.54% in instructions, median +0.13%. That is the per-trial pricing plus the runner-up tree copy. The
  largest are web-NotreDame `-d` +0.54% and om5 / om7 / om8 E50000 `-d` +0.44%. On web-NotreDame the
  pricing is ~29 ms over ten trials; the rest is two runner-up tree copies.
- **Rows it cannot reach** (116 rows: `-N1`, `-2`, L\*): −0.15% to +0.23% in instructions, median 0.00%.
- **Seconds:** 56 bit-identical cells read more than 1% slower. The five largest were re-measured after
  the batch, three interleaved reps per arm, appended as reps 2–4 (the tables show the minimum):

  | row | first reading | minimum of 4 | Δinstr |
  |---|--:|--:|--:|
  | netsci `-2 -N10` (unreachable) | +67.4% | +1.1% | −0.06% |
  | powergrid `-2 -N10` (unreachable) | +29.1% | +0.0% | +0.11% |
  | om7 E50000 `-2d --regularized -N10` (unreachable) | +18.7% | +1.6% | +0.03% |
  | om3 `-d --regularized -N10` | +14.9% | −1.0% | +0.13% |
  | om7 E50000 `-d --regularized -N10` | +14.2% | +2.0% (old 3.11–3.18 s, new 3.17–3.56 s) | +0.44% |

  The rest are load: the same spread appears on rows the change cannot reach.

### Reach of the change

The pricing and the runner-up slot run only in a columnar hierarchical run with more than one trial,
without `-2`, L\*, `-c` or `--no-infomap` (`tracksRunnerUp` in `src/core/InfomapBase.cpp`). A runner-up
tree is copied only when its priced reach could still beat the current best. At the end of the run it is
dissolved only when its reach is below the winner's: in place of the winner's dissolve when the repair
left the winner alone, after it otherwise. The pricing is exact (the pass's own accounting), so a
dissolved runner-up is never rejected. Serial and `--parallel-trials` pick the same runner-up (lowest
reach, earliest index on a tie) and give the same codelength on netsci (4.023567105), powergrid
(4.712391773) and om4 E50000 `-d` (4.873775655), checked on an OpenMP build (md5 `04f6eae3`).

### Per-feature attribution

Two parts, measured on separate binaries against the tip in a separate session (min of 2, instructions;
Δ on the three moved rows, then max / median on the 25 base and om `-d` `-N10` rows that do not gain):

| variant | netsci / powergrid / om4 E50000 `-d` | other rows |
|---|---|---|
| ungated: dissolve the runner-up by pre-repair L after the winner | −0.62% / −0.11% / −0.060% bits at +1.93% / +2.01% / +1.25% | +1.01% / +0.25% |
| a cheap upper bound on the dissolve's saving as the gate | same, +1.87% / +1.98% / +1.23% (the bound is 6–29× the true saving and skips nothing) | +1.03% / +0.22% |
| exact pricing, runner-up = lowest priced reach | same, +2.20% / +2.20% / +1.30% | +0.63% / +0.04% |
| **shipped**: exact pricing + one dissolve when the repair did nothing | same, **+0.73% / +0.41% / +1.29%** | **+0.55% / +0.10%** |
| shipped without the post-repair block | netsci and powergrid only; om4 E50000: no gain, +0.30% | |

The post-repair block is the whole of om4 E50000's −0.060% in bits and roughly +1.0 points of its
instructions. It is kept because it is the only path for a runner-up when the repair moves the winner.

### Old vs new columnar — standard search (`-C -N10`)

Overlapping and wikispeedia rows run `-C -d -N10`. The change runs on every row here. netsci and
powergrid move (above); every other row is bit-identical, at −0.24% to +0.54% in instructions for the
pricing and the runner-up copy.

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (+6.1%)</td><td align="right">0.1G (+0.02%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.007s (-0.6%)</td><td align="right">0.1G (+0.30%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.024s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.023567105 (-0.6246%)</td><td align="right">0.023s (-3.6%)</td><td align="right">0.3G (+0.66%)</td><td align="right">11</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.245s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.712391773 (-0.1138%)</td><td align="right">0.250s (+2.1%)</td><td align="right">2.8G (+0.37%)</td><td align="right">10</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.061s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.060s (-0.4%)</td><td align="right">0.7G (-0.24%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.11s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">3.30s (+6.1%)</td><td align="right">34.2G (+0.11%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">20.5s</td><td align="right">181.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">21.0s (+2.5%)</td><td align="right">182.8G (+0.54%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.005s (+15.2%)</td><td align="right">0.1G (-6.04%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-18.9%)</td><td align="right">0.1G (-0.36%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.89s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.77s (-4.2%)</td><td align="right">31.5G (+0.02%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.93s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">4.03s (+2.7%)</td><td align="right">38.6G (+0.10%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.54s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">4.03s (-11.3%)</td><td align="right">40.4G (+0.01%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.3s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">10.1s (-1.6%)</td><td align="right">97.9G (+0.00%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.69s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.65s (-1.0%)</td><td align="right">35.5G (+0.01%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">5.41s</td><td align="right">47.6G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">5.10s (-5.7%)</td><td align="right">47.6G (+0.01%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">6.23s</td><td align="right">59.9G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.99s (-3.8%)</td><td align="right">59.9G (+0.08%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.98s</td><td align="right">60.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">6.94s (-0.7%)</td><td align="right">60.7G (+0.11%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">7.44s</td><td align="right">59.0G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">6.55s (-12.0%)</td><td align="right">59.0G (+0.02%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">9.36s</td><td align="right">81.3G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">9.49s (+1.5%)</td><td align="right">81.3G (+0.09%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">9.81s</td><td align="right">82.7G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">9.89s (+0.8%)</td><td align="right">82.8G (+0.12%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">11.3s</td><td align="right">86.7G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">10.2s (-10.1%)</td><td align="right">86.6G (-0.12%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">6.773456578</td><td align="right">3.89s</td><td align="right">33.3G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.69s (-5.0%)</td><td align="right">33.3G (+0.08%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.851498646</td><td align="right">4.71s</td><td align="right">44.6G</td><td align="right">617</td><td align="right">4</td><td align="right">5.851498646 (=)</td><td align="right">4.73s (+0.4%)</td><td align="right">44.7G (+0.23%)</td><td align="right">617</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.876717868</td><td align="right">5.35s</td><td align="right">47.4G</td><td align="right">1031</td><td align="right">4</td><td align="right">4.873775655 (-0.0603%)</td><td align="right">5.52s (+3.3%)</td><td align="right">48.1G (+1.35%)</td><td align="right">1200</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.150346795</td><td align="right">6.55s</td><td align="right">51.1G</td><td align="right">2171</td><td align="right">4</td><td align="right">4.150346795 (=)</td><td align="right">6.27s (-4.3%)</td><td align="right">51.4G (+0.44%)</td><td align="right">2171</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.617750514</td><td align="right">6.28s</td><td align="right">51.7G</td><td align="right">3050</td><td align="right">4</td><td align="right">3.617750514 (=)</td><td align="right">6.40s (+1.9%)</td><td align="right">51.9G (+0.37%)</td><td align="right">3050</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.201872271</td><td align="right">8.35s</td><td align="right">65.2G</td><td align="right">3424</td><td align="right">6</td><td align="right">3.201872271 (=)</td><td align="right">7.86s (-5.9%)</td><td align="right">65.4G (+0.31%)</td><td align="right">3424</td><td align="right">6</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.883308449</td><td align="right">8.59s</td><td align="right">69.7G</td><td align="right">4367</td><td align="right">5</td><td align="right">2.883308449 (=)</td><td align="right">8.53s (-0.7%)</td><td align="right">70.0G (+0.37%)</td><td align="right">4367</td><td align="right">5</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.785s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.715s (-8.8%)</td><td align="right">7.2G (-0.02%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Overlapping and wikispeedia rows as `-C -2d -N10`. A two-level run has no interior level to dissolve and
never runs the change: every row is bit-identical, and the time column is the session's noise floor.

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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-15.3%)</td><td align="right">0.1G (-1.49%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.861229775</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (=)</td><td align="right">0.007s (+3.2%)</td><td align="right">0.1G (-0.17%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.283072584</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.283072584 (=)</td><td align="right">0.009s (+1.1%)</td><td align="right">0.2G (-0.06%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.63729688</td><td align="right">0.100s</td><td align="right">1.1G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.100s (+0.0%)</td><td align="right">1.1G (+0.11%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739575295</td><td align="right">0.043s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">6.739575295 (=)</td><td align="right">0.043s (+1.0%)</td><td align="right">0.5G (-0.02%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.949978834</td><td align="right">2.34s</td><td align="right">23.9G</td><td align="right">506</td><td align="right">2</td><td align="right">7.949978834 (=)</td><td align="right">2.35s (+0.5%)</td><td align="right">23.9G (-0.00%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.754216663</td><td align="right">21.9s</td><td align="right">117.7G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.754216663 (=)</td><td align="right">22.0s (+0.3%)</td><td align="right">117.7G (-0.00%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.009s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-62.0%)</td><td align="right">0.1G (-6.40%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-40.4%)</td><td align="right">0.1G (-9.79%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.400445378</td><td align="right">2.92s</td><td align="right">32.3G</td><td align="right">168</td><td align="right">2</td><td align="right">7.400445378 (=)</td><td align="right">2.90s (-0.5%)</td><td align="right">32.3G (+0.01%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393055049</td><td align="right">4.42s</td><td align="right">41.8G</td><td align="right">334</td><td align="right">2</td><td align="right">5.393055049 (=)</td><td align="right">4.07s (-7.9%)</td><td align="right">41.7G (-0.04%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571539329</td><td align="right">4.16s</td><td align="right">41.2G</td><td align="right">304</td><td align="right">2</td><td align="right">5.571539329 (=)</td><td align="right">4.23s (+1.6%)</td><td align="right">41.2G (-0.02%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.424143707</td><td align="right">12.1s</td><td align="right">104.6G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.424143707 (=)</td><td align="right">12.7s (+5.0%)</td><td align="right">104.6G (+0.03%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.17s</td><td align="right">31.6G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.14s (-1.2%)</td><td align="right">31.6G (-0.04%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.740761645</td><td align="right">3.82s</td><td align="right">35.6G</td><td align="right">625</td><td align="right">2</td><td align="right">6.740761645 (=)</td><td align="right">3.59s (-5.9%)</td><td align="right">35.6G (-0.11%)</td><td align="right">625</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.822832994</td><td align="right">8.06s</td><td align="right">77.3G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">7.40s (-8.2%)</td><td align="right">77.4G (+0.05%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.866901786</td><td align="right">7.38s</td><td align="right">70.7G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">8.03s (+8.9%)</td><td align="right">70.7G (+0.04%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.867301407</td><td align="right">8.48s</td><td align="right">68.5G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">7.68s (-9.4%)</td><td align="right">68.4G (-0.10%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.88496897</td><td align="right">8.32s</td><td align="right">69.0G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">7.90s (-5.1%)</td><td align="right">69.0G (-0.06%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.88992089</td><td align="right">8.39s</td><td align="right">69.2G</td><td align="right">663</td><td align="right">2</td><td align="right">6.88992089 (=)</td><td align="right">8.03s (-4.3%)</td><td align="right">69.2G (+0.02%)</td><td align="right">663</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.88742315</td><td align="right">9.41s</td><td align="right">74.5G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">9.15s (-2.7%)</td><td align="right">74.5G (-0.02%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-2d`</td><td align="right">6.773456578</td><td align="right">4.11s</td><td align="right">37.8G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">4.16s (+1.2%)</td><td align="right">37.8G (+0.06%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-2d`</td><td align="right">6.258611497</td><td align="right">5.96s</td><td align="right">36.9G</td><td align="right">3236</td><td align="right">2</td><td align="right">6.258611497 (=)</td><td align="right">4.61s (-22.7%)</td><td align="right">36.8G (-0.11%)</td><td align="right">3236</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 E50000 `-2d`</td><td align="right">5.454385527</td><td align="right">5.11s</td><td align="right">33.8G</td><td align="right">4381</td><td align="right">2</td><td align="right">5.454385527 (=)</td><td align="right">4.65s (-9.1%)</td><td align="right">33.8G (-0.04%)</td><td align="right">4381</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 E50000 `-2d`</td><td align="right">4.903270512</td><td align="right">4.47s</td><td align="right">31.9G</td><td align="right">5443</td><td align="right">2</td><td align="right">4.903270512 (=)</td><td align="right">4.50s (+0.8%)</td><td align="right">31.9G (+0.02%)</td><td align="right">5443</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 E50000 `-2d`</td><td align="right">4.468778176</td><td align="right">4.51s</td><td align="right">32.4G</td><td align="right">6379</td><td align="right">2</td><td align="right">4.468778176 (=)</td><td align="right">5.11s (+13.2%)</td><td align="right">32.4G (-0.00%)</td><td align="right">6379</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 E50000 `-2d`</td><td align="right">4.143277395</td><td align="right">5.56s</td><td align="right">31.2G</td><td align="right">7097</td><td align="right">2</td><td align="right">4.143277395 (=)</td><td align="right">5.12s (-7.9%)</td><td align="right">31.2G (-0.07%)</td><td align="right">7097</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 E50000 `-2d`</td><td align="right">3.859069372</td><td align="right">4.82s</td><td align="right">31.6G</td><td align="right">7985</td><td align="right">2</td><td align="right">3.859069372 (=)</td><td align="right">5.33s (+10.6%)</td><td align="right">31.6G (+0.03%)</td><td align="right">7985</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.907904741</td><td align="right">0.727s</td><td align="right">6.9G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.735s (+1.2%)</td><td align="right">6.9G (+0.02%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

Interleaved minimum of 3 per arm. A single trial has no runner-up, so every row is bit-identical and
the change never runs. The `-d -N1` rows of om2 / om5–om8 still return a refined hierarchical build 1–14%
above `-2d -N1`, the `-N1` property #1121 documents (F59).

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.000s (-2.6%)</td><td align="right">0.1G (-1.33%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.899367957</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">2</td><td align="right">6.899367957 (=)</td><td align="right">0.001s (-5.5%)</td><td align="right">0.1G (-0.39%)</td><td align="right">11</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.047459862</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">4</td><td align="right">4.047459862 (=)</td><td align="right">0.003s (-3.0%)</td><td align="right">0.1G (-0.38%)</td><td align="right">7</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.730850312</td><td align="right">0.025s</td><td align="right">0.4G</td><td align="right">12</td><td align="right">5</td><td align="right">4.730850312 (=)</td><td align="right">0.026s (+1.1%)</td><td align="right">0.4G (-0.15%)</td><td align="right">12</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.758265349</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">3</td><td align="right">3</td><td align="right">6.758265349 (=)</td><td align="right">0.009s (-1.3%)</td><td align="right">0.2G (-0.22%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">0.410s</td><td align="right">5.7G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">0.406s (-0.9%)</td><td align="right">5.7G (-0.00%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">2.49s</td><td align="right">24.4G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">2.47s (-0.8%)</td><td align="right">24.4G (-0.01%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.041117399</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.041117399 (=)</td><td align="right">0.001s (-6.5%)</td><td align="right">0.1G (-1.99%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-25.8%)</td><td align="right">0.1G (-0.21%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.491980364</td><td align="right">0.379s</td><td align="right">5.3G</td><td align="right">148</td><td align="right">3</td><td align="right">7.491980364 (=)</td><td align="right">0.375s (-0.9%)</td><td align="right">5.3G (+0.05%)</td><td align="right">148</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.470440768</td><td align="right">0.378s</td><td align="right">4.5G</td><td align="right">242</td><td align="right">3</td><td align="right">5.470440768 (=)</td><td align="right">0.385s (+1.9%)</td><td align="right">4.5G (-0.03%)</td><td align="right">242</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.657913279</td><td align="right">0.500s</td><td align="right">5.9G</td><td align="right">197</td><td align="right">3</td><td align="right">5.657913279 (=)</td><td align="right">0.503s (+0.7%)</td><td align="right">5.9G (+0.03%)</td><td align="right">197</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.546335898</td><td align="right">0.923s</td><td align="right">9.2G</td><td align="right">1614</td><td align="right">3</td><td align="right">7.546335898 (=)</td><td align="right">0.902s (-2.3%)</td><td align="right">9.2G (-0.03%)</td><td align="right">1614</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.460796773</td><td align="right">0.416s</td><td align="right">5.8G</td><td align="right">5</td><td align="right">3</td><td align="right">8.460796773 (=)</td><td align="right">0.419s (+0.7%)</td><td align="right">5.8G (-0.03%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739607071</td><td align="right">1.32s</td><td align="right">14.0G</td><td align="right">669</td><td align="right">2</td><td align="right">6.739607071 (=)</td><td align="right">1.34s (+1.8%)</td><td align="right">14.1G (+0.06%)</td><td align="right">669</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.823562921</td><td align="right">1.15s</td><td align="right">12.9G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.15s (+0.3%)</td><td align="right">12.9G (-0.02%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.861732911</td><td align="right">1.51s</td><td align="right">15.7G</td><td align="right">139</td><td align="right">2</td><td align="right">6.861732911 (=)</td><td align="right">1.54s (+1.5%)</td><td align="right">15.7G (-0.02%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.868180827</td><td align="right">1.72s</td><td align="right">17.2G</td><td align="right">291</td><td align="right">2</td><td align="right">6.868180827 (=)</td><td align="right">1.73s (+0.4%)</td><td align="right">17.2G (+0.00%)</td><td align="right">291</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884236095</td><td align="right">1.69s</td><td align="right">16.3G</td><td align="right">453</td><td align="right">2</td><td align="right">6.884236095 (=)</td><td align="right">1.69s (+0.0%)</td><td align="right">16.3G (+0.01%)</td><td align="right">453</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.889674586</td><td align="right">1.85s</td><td align="right">17.2G</td><td align="right">649</td><td align="right">2</td><td align="right">6.889674586 (=)</td><td align="right">1.87s (+0.8%)</td><td align="right">17.2G (+0.03%)</td><td align="right">649</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.894258582</td><td align="right">2.01s</td><td align="right">17.9G</td><td align="right">907</td><td align="right">2</td><td align="right">6.894258582 (=)</td><td align="right">2.02s (+0.3%)</td><td align="right">17.9G (-0.00%)</td><td align="right">907</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.29196634</td><td align="right">0.379s</td><td align="right">3.7G</td><td align="right">321</td><td align="right">4</td><td align="right">7.29196634 (=)</td><td align="right">0.383s (+1.0%)</td><td align="right">3.7G (+0.08%)</td><td align="right">321</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.823562921</td><td align="right">1.52s</td><td align="right">17.0G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.49s (-2.0%)</td><td align="right">17.0G (+0.02%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.861732911</td><td align="right">1.96s</td><td align="right">20.7G</td><td align="right">139</td><td align="right">2</td><td align="right">6.861732911 (=)</td><td align="right">1.99s (+1.5%)</td><td align="right">20.7G (+0.00%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.812252898</td><td align="right">0.772s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.812252898 (=)</td><td align="right">0.771s (-0.1%)</td><td align="right">7.2G (+0.04%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.440161581</td><td align="right">0.816s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.440161581 (=)</td><td align="right">0.823s (+0.8%)</td><td align="right">7.5G (-0.05%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.192756466</td><td align="right">0.816s</td><td align="right">7.5G</td><td align="right">149</td><td align="right">4</td><td align="right">7.192756466 (=)</td><td align="right">0.837s (+2.6%)</td><td align="right">7.5G (+0.07%)</td><td align="right">149</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.967535764</td><td align="right">0.893s</td><td align="right">7.5G</td><td align="right">206</td><td align="right">4</td><td align="right">6.967535764 (=)</td><td align="right">0.859s (-3.9%)</td><td align="right">7.5G (-0.05%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">7.144517469</td><td align="right">0.443s</td><td align="right">4.6G</td><td align="right">3</td><td align="right">3</td><td align="right">7.144517469 (=)</td><td align="right">0.440s (-0.7%)</td><td align="right">4.6G (-0.06%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.854829799</td><td align="right">0.448s</td><td align="right">4.3G</td><td align="right">537</td><td align="right">4</td><td align="right">5.854829799 (=)</td><td align="right">0.459s (+2.5%)</td><td align="right">4.4G (+0.13%)</td><td align="right">537</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.898784516</td><td align="right">0.468s</td><td align="right">4.2G</td><td align="right">1867</td><td align="right">3</td><td align="right">4.898784516 (=)</td><td align="right">0.514s (+9.9%)</td><td align="right">4.2G (-0.07%)</td><td align="right">1867</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.1554786</td><td align="right">0.571s</td><td align="right">5.0G</td><td align="right">2156</td><td align="right">4</td><td align="right">4.1554786 (=)</td><td align="right">0.577s (+1.1%)</td><td align="right">5.0G (+0.07%)</td><td align="right">2156</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.620157111</td><td align="right">0.711s</td><td align="right">6.0G</td><td align="right">3056</td><td align="right">4</td><td align="right">3.620157111 (=)</td><td align="right">0.705s (-0.8%)</td><td align="right">6.0G (+0.09%)</td><td align="right">3056</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.21639819</td><td align="right">0.658s</td><td align="right">5.6G</td><td align="right">3417</td><td align="right">5</td><td align="right">3.21639819 (=)</td><td align="right">0.655s (-0.3%)</td><td align="right">5.6G (+0.07%)</td><td align="right">3417</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.885785338</td><td align="right">0.965s</td><td align="right">8.1G</td><td align="right">4371</td><td align="right">5</td><td align="right">2.885785338 (=)</td><td align="right">0.977s (+1.2%)</td><td align="right">8.1G (-0.09%)</td><td align="right">4371</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om2 `-2d -c` planted</td><td align="right">6.744721993</td><td align="right">0.584s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.744721993 (=)</td><td align="right">0.588s (+0.6%)</td><td align="right">6.2G (-0.06%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d -c` planted</td><td align="right">6.820929855</td><td align="right">0.412s</td><td align="right">4.3G</td><td align="right">50</td><td align="right">2</td><td align="right">6.820929855 (=)</td><td align="right">0.406s (-1.5%)</td><td align="right">4.3G (-0.02%)</td><td align="right">50</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d -c` planted</td><td align="right">6.856285862</td><td align="right">0.706s</td><td align="right">7.4G</td><td align="right">129</td><td align="right">2</td><td align="right">6.856285862 (=)</td><td align="right">0.700s (-0.9%)</td><td align="right">7.4G (-0.03%)</td><td align="right">129</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d -c` planted</td><td align="right">6.857876353</td><td align="right">0.923s</td><td align="right">9.5G</td><td align="right">287</td><td align="right">2</td><td align="right">6.857876353 (=)</td><td align="right">0.950s (+3.0%)</td><td align="right">9.5G (-0.00%)</td><td align="right">287</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d -c` planted</td><td align="right">6.873464257</td><td align="right">1.13s</td><td align="right">11.3G</td><td align="right">444</td><td align="right">2</td><td align="right">6.873464257 (=)</td><td align="right">1.11s (-2.6%)</td><td align="right">11.3G (-0.04%)</td><td align="right">444</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d -c` planted</td><td align="right">6.881554086</td><td align="right">1.02s</td><td align="right">9.6G</td><td align="right">624</td><td align="right">2</td><td align="right">6.881554086 (=)</td><td align="right">0.992s (-2.5%)</td><td align="right">9.6G (-0.05%)</td><td align="right">624</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d -c` planted</td><td align="right">6.875540042</td><td align="right">1.26s</td><td align="right">11.8G</td><td align="right">885</td><td align="right">2</td><td align="right">6.875540042 (=)</td><td align="right">1.28s (+2.2%)</td><td align="right">11.8G (+0.01%)</td><td align="right">885</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">6.066305904</td><td align="right">0.073s</td><td align="right">0.8G</td><td align="right">187</td><td align="right">3</td><td align="right">6.066305904 (=)</td><td align="right">0.074s (+0.5%)</td><td align="right">0.8G (+0.04%)</td><td align="right">187</td><td align="right">3</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.095s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.094s (-0.9%)</td><td align="right">1.0G (-0.06%)</td><td align="right">184</td><td align="right">2</td></tr>
</tbody>
</table>

### The overlapping family in full

Every configuration of the planted overlapping state networks, both arms, at both trigram densities
(F55). The change runs on every hierarchical `-N10` row. om4 E50000 `-d` moves (−0.060% in bits, +1.35%
in instructions; above), and every other row is bit-identical, the `-d -N10` rows at −0.2% to +0.6% in
instructions.

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
<tr><td align="right">om2 E100000 `-2d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">4.38s</td><td align="right">36.8G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">4.31s (-1.5%)</td><td align="right">36.8G (+0.02%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 E100000 `-d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">3.64s</td><td align="right">33.5G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.56s (-2.2%)</td><td align="right">33.5G (+0.13%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.92s</td><td align="right">24.8G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">3.27s (+12.1%)</td><td align="right">24.9G (+0.23%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N1`</td><td align="right">7.548863183</td><td align="right">0.732s</td><td align="right">7.2G</td><td align="right">121</td><td align="right">2</td><td align="right">7.548863183 (=)</td><td align="right">0.737s (+0.6%)</td><td align="right">7.2G (+0.02%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.73s</td><td align="right">23.9G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.87s (+5.2%)</td><td align="right">23.9G (+0.23%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N1`</td><td align="right">7.548863183</td><td align="right">0.855s</td><td align="right">8.5G</td><td align="right">121</td><td align="right">2</td><td align="right">7.548863183 (=)</td><td align="right">0.863s (+0.9%)</td><td align="right">8.5G (-0.02%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om2 planted, `-2d --no-infomap -c`</td><td align="right">6.789039995</td><td align="right">0.056s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.789039995 (=)</td><td align="right">0.055s (-2.9%)</td><td align="right">0.6G (-0.07%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N10`</td><td align="right">7.925216272</td><td align="right">2.56s</td><td align="right">22.5G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">2.71s (+5.6%)</td><td align="right">22.5G (+0.01%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N10`</td><td align="right">7.925216272</td><td align="right">2.88s</td><td align="right">22.7G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">2.79s (-3.1%)</td><td align="right">22.8G (+0.20%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">5.76s</td><td align="right">48.0G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">5.36s (-6.9%)</td><td align="right">48.0G (-0.02%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">0.932s</td><td align="right">9.2G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">0.931s (-0.1%)</td><td align="right">9.2G (+0.00%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">4.99s</td><td align="right">45.0G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">4.94s (-1.0%)</td><td align="right">45.1G (+0.13%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">1.18s</td><td align="right">12.0G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">1.18s (-0.1%)</td><td align="right">12.0G (+0.00%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 planted, `-2d --no-infomap -c`</td><td align="right">6.837980937</td><td align="right">0.099s</td><td align="right">0.9G</td><td align="right">12</td><td align="right">2</td><td align="right">6.837980937 (=)</td><td align="right">0.098s (-1.0%)</td><td align="right">0.9G (-0.03%)</td><td align="right">12</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N10`</td><td align="right">7.92621405</td><td align="right">3.18s</td><td align="right">23.1G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">2.92s (-8.3%)</td><td align="right">23.1G (-0.08%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N10`</td><td align="right">7.92621405</td><td align="right">3.14s</td><td align="right">22.9G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">3.06s (-2.7%)</td><td align="right">23.0G (+0.32%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N10`</td><td align="right">7.556894677</td><td align="right">5.67s</td><td align="right">49.0G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">5.80s (+2.4%)</td><td align="right">49.0G (+0.01%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">0.984s</td><td align="right">9.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">0.993s (+0.9%)</td><td align="right">9.1G (+0.01%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N10`</td><td align="right">7.556653713</td><td align="right">5.96s</td><td align="right">48.6G</td><td align="right">78</td><td align="right">2</td><td align="right">7.556653713 (=)</td><td align="right">5.82s (-2.4%)</td><td align="right">48.7G (+0.16%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">1.22s</td><td align="right">11.7G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">1.25s (+1.9%)</td><td align="right">11.7G (+0.04%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 planted, `-2d --no-infomap -c`</td><td align="right">6.880650147</td><td align="right">0.121s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.880650147 (=)</td><td align="right">0.115s (-4.7%)</td><td align="right">1.0G (+0.08%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N10`</td><td align="right">7.956672505</td><td align="right">3.19s</td><td align="right">23.3G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">3.05s (-4.3%)</td><td align="right">23.3G (-0.05%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N10`</td><td align="right">7.956672505</td><td align="right">2.85s</td><td align="right">22.5G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">2.94s (+3.1%)</td><td align="right">22.6G (+0.41%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N10`</td><td align="right">7.968061942</td><td align="right">5.27s</td><td align="right">43.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.968061942 (=)</td><td align="right">5.19s (-1.6%)</td><td align="right">43.1G (-0.01%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N1`</td><td align="right">7.968629202</td><td align="right">0.893s</td><td align="right">7.8G</td><td align="right">79</td><td align="right">2</td><td align="right">7.968629202 (=)</td><td align="right">0.888s (-0.5%)</td><td align="right">7.8G (+0.00%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N10`</td><td align="right">7.968320007</td><td align="right">5.10s</td><td align="right">42.5G</td><td align="right">81</td><td align="right">2</td><td align="right">7.968320007 (=)</td><td align="right">5.05s (-1.1%)</td><td align="right">42.6G (+0.18%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N1`</td><td align="right">7.968629202</td><td align="right">1.17s</td><td align="right">10.5G</td><td align="right">79</td><td align="right">2</td><td align="right">7.968629202 (=)</td><td align="right">1.19s (+1.7%)</td><td align="right">10.5G (-0.00%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 planted, `-2d --no-infomap -c`</td><td align="right">6.902222527</td><td align="right">0.128s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.902222527 (=)</td><td align="right">0.128s (+0.1%)</td><td align="right">1.1G (-0.08%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N10`</td><td align="right">7.944047825</td><td align="right">2.87s</td><td align="right">21.9G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">2.83s (-1.4%)</td><td align="right">21.9G (+0.04%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N10`</td><td align="right">7.944047825</td><td align="right">3.14s</td><td align="right">21.6G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">2.94s (-6.3%)</td><td align="right">21.6G (+0.41%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N10`</td><td align="right">7.982650944</td><td align="right">5.24s</td><td align="right">41.2G</td><td align="right">105</td><td align="right">2</td><td align="right">7.982650944 (=)</td><td align="right">5.24s (+0.0%)</td><td align="right">41.2G (-0.03%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N1`</td><td align="right">7.982650944</td><td align="right">0.737s</td><td align="right">6.0G</td><td align="right">105</td><td align="right">2</td><td align="right">7.982650944 (=)</td><td align="right">0.751s (+1.8%)</td><td align="right">6.1G (+0.09%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N10`</td><td align="right">7.984417755</td><td align="right">5.31s</td><td align="right">41.3G</td><td align="right">102</td><td align="right">2</td><td align="right">7.984417755 (=)</td><td align="right">5.25s (-1.1%)</td><td align="right">41.3G (+0.22%)</td><td align="right">102</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N1`</td><td align="right">7.982650944</td><td align="right">1.00s</td><td align="right">8.7G</td><td align="right">105</td><td align="right">2</td><td align="right">7.982650944 (=)</td><td align="right">0.982s (-2.1%)</td><td align="right">8.7G (+0.03%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 planted, `-2d --no-infomap -c`</td><td align="right">6.930934993</td><td align="right">0.140s</td><td align="right">1.1G</td><td align="right">24</td><td align="right">2</td><td align="right">6.930934993 (=)</td><td align="right">0.140s (+0.1%)</td><td align="right">1.1G (-0.14%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N10`</td><td align="right">7.957532546</td><td align="right">3.09s</td><td align="right">23.3G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">3.14s (+1.6%)</td><td align="right">23.3G (+0.03%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N10`</td><td align="right">7.957532546</td><td align="right">3.11s</td><td align="right">22.3G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">3.17s (+2.0%)</td><td align="right">22.4G (+0.44%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N10`</td><td align="right">7.948827768</td><td align="right">5.69s</td><td align="right">41.2G</td><td align="right">166</td><td align="right">2</td><td align="right">7.948827768 (=)</td><td align="right">5.77s (+1.4%)</td><td align="right">41.2G (+0.03%)</td><td align="right">166</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N1`</td><td align="right">7.950842444</td><td align="right">0.797s</td><td align="right">6.4G</td><td align="right">168</td><td align="right">2</td><td align="right">7.950842444 (=)</td><td align="right">0.809s (+1.5%)</td><td align="right">6.4G (+0.01%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N10`</td><td align="right">7.948827768</td><td align="right">5.54s</td><td align="right">41.1G</td><td align="right">166</td><td align="right">2</td><td align="right">7.948827768 (=)</td><td align="right">5.47s (-1.1%)</td><td align="right">41.2G (+0.21%)</td><td align="right">166</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N1`</td><td align="right">7.950842444</td><td align="right">1.06s</td><td align="right">9.1G</td><td align="right">168</td><td align="right">2</td><td align="right">7.950842444 (=)</td><td align="right">1.09s (+3.1%)</td><td align="right">9.1G (-0.00%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">om7 planted, `-2d --no-infomap -c`</td><td align="right">6.957516072</td><td align="right">0.169s</td><td align="right">1.3G</td><td align="right">28</td><td align="right">2</td><td align="right">6.957516072 (=)</td><td align="right">0.163s (-3.3%)</td><td align="right">1.3G (-0.03%)</td><td align="right">28</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N10`</td><td align="right">7.978376075</td><td align="right">3.42s</td><td align="right">22.6G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">3.46s (+1.1%)</td><td align="right">22.6G (-0.02%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N10`</td><td align="right">7.978376075</td><td align="right">3.29s</td><td align="right">21.8G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">3.27s (-0.6%)</td><td align="right">21.8G (+0.44%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N10`</td><td align="right">7.979831947</td><td align="right">5.84s</td><td align="right">40.8G</td><td align="right">221</td><td align="right">2</td><td align="right">7.979831947 (=)</td><td align="right">5.42s (-7.3%)</td><td align="right">40.7G (-0.09%)</td><td align="right">221</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N1`</td><td align="right">7.983599366</td><td align="right">0.794s</td><td align="right">6.2G</td><td align="right">216</td><td align="right">2</td><td align="right">7.983599366 (=)</td><td align="right">0.787s (-0.9%)</td><td align="right">6.2G (-0.04%)</td><td align="right">216</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N10`</td><td align="right">7.978630877</td><td align="right">5.50s</td><td align="right">41.3G</td><td align="right">236</td><td align="right">2</td><td align="right">7.978630877 (=)</td><td align="right">5.47s (-0.6%)</td><td align="right">41.4G (+0.23%)</td><td align="right">236</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N1`</td><td align="right">7.983599366</td><td align="right">1.12s</td><td align="right">8.9G</td><td align="right">216</td><td align="right">2</td><td align="right">7.983599366 (=)</td><td align="right">1.06s (-5.3%)</td><td align="right">8.9G (+0.01%)</td><td align="right">216</td><td align="right">2</td></tr>
<tr><td align="right">om8 planted, `-2d --no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.153s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.156s (+1.8%)</td><td align="right">1.2G (+0.03%)</td><td align="right">32</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N1`</td><td align="right">7.925216272</td><td align="right">0.618s</td><td align="right">5.3G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">0.624s (+0.9%)</td><td align="right">5.3G (-0.05%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N1`</td><td align="right">7.925216272</td><td align="right">0.731s</td><td align="right">6.6G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">0.753s (+3.0%)</td><td align="right">6.6G (-0.06%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N1`</td><td align="right">7.92621405</td><td align="right">0.786s</td><td align="right">6.2G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">0.768s (-2.3%)</td><td align="right">6.2G (-0.02%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N1`</td><td align="right">7.92621405</td><td align="right">0.887s</td><td align="right">7.6G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">0.872s (-1.7%)</td><td align="right">7.6G (-0.11%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N1`</td><td align="right">7.956672505</td><td align="right">0.755s</td><td align="right">6.3G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">0.772s (+2.3%)</td><td align="right">6.3G (-0.02%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N1`</td><td align="right">7.956672505</td><td align="right">0.903s</td><td align="right">7.7G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">0.980s (+8.5%)</td><td align="right">7.7G (+0.09%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N1`</td><td align="right">7.944047825</td><td align="right">0.674s</td><td align="right">5.2G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">0.633s (-6.0%)</td><td align="right">5.2G (-0.10%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N1`</td><td align="right">7.944047825</td><td align="right">0.739s</td><td align="right">5.6G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">0.710s (-3.8%)</td><td align="right">5.6G (-0.13%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N1`</td><td align="right">7.957532546</td><td align="right">0.741s</td><td align="right">5.8G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">0.728s (-1.7%)</td><td align="right">5.8G (-0.01%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N1`</td><td align="right">7.957532546</td><td align="right">0.762s</td><td align="right">6.2G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">0.797s (+4.7%)</td><td align="right">6.2G (+0.10%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N1`</td><td align="right">7.978376075</td><td align="right">0.644s</td><td align="right">5.2G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">0.662s (+2.8%)</td><td align="right">5.2G (+0.01%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N1`</td><td align="right">7.978376075</td><td align="right">0.729s</td><td align="right">5.6G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">0.709s (-2.8%)</td><td align="right">5.5G (-0.15%)</td><td align="right">77</td><td align="right">2</td></tr>
</tbody>
</table>

### `-F` on the family, old vs new

`-F` is a hierarchical search, so the change runs at `-N10`; every row is bit-identical. `-N10`:

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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.09s</td><td align="right">40.3G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">4.59s (+12.2%)</td><td align="right">40.4G (+0.31%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.31s</td><td align="right">55.5G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.53s (+4.3%)</td><td align="right">55.5G (+0.12%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">5.91s</td><td align="right">56.0G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.84s (-1.2%)</td><td align="right">56.0G (+0.11%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">6.17s</td><td align="right">54.2G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">6.11s (-1.0%)</td><td align="right">54.3G (+0.14%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">8.40s</td><td align="right">66.6G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">8.98s (+6.9%)</td><td align="right">66.8G (+0.21%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">8.79s</td><td align="right">69.7G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">8.56s (-2.6%)</td><td align="right">69.8G (+0.03%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">9.43s</td><td align="right">74.8G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">9.47s (+0.4%)</td><td align="right">74.8G (+0.05%)</td><td align="right">919</td><td align="right">2</td></tr>
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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.321678354</td><td align="right">0.270s</td><td align="right">2.6G</td><td align="right">436</td><td align="right">4</td><td align="right">7.321678354 (=)</td><td align="right">0.279s (+3.1%)</td><td align="right">2.6G (+0.03%)</td><td align="right">436</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.823562921</td><td align="right">1.48s</td><td align="right">16.6G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.46s (-1.5%)</td><td align="right">16.6G (-0.02%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.861732911</td><td align="right">1.77s</td><td align="right">18.4G</td><td align="right">139</td><td align="right">2</td><td align="right">6.861732911 (=)</td><td align="right">1.75s (-1.3%)</td><td align="right">18.4G (-0.03%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.798283664</td><td align="right">0.704s</td><td align="right">5.8G</td><td align="right">96</td><td align="right">4</td><td align="right">7.798283664 (=)</td><td align="right">0.688s (-2.3%)</td><td align="right">5.8G (+0.03%)</td><td align="right">96</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.507066407</td><td align="right">0.704s</td><td align="right">5.8G</td><td align="right">87</td><td align="right">4</td><td align="right">7.507066407 (=)</td><td align="right">0.704s (+0.0%)</td><td align="right">5.8G (+0.12%)</td><td align="right">87</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.238675817</td><td align="right">0.630s</td><td align="right">5.2G</td><td align="right">145</td><td align="right">4</td><td align="right">7.238675817 (=)</td><td align="right">0.677s (+7.5%)</td><td align="right">5.2G (-0.02%)</td><td align="right">145</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">7.022972054</td><td align="right">0.634s</td><td align="right">5.2G</td><td align="right">201</td><td align="right">4</td><td align="right">7.022972054 (=)</td><td align="right">0.621s (-2.2%)</td><td align="right">5.2G (-0.04%)</td><td align="right">201</td><td align="right">4</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-19.0%)</td><td align="right">0.1G (-2.39%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.007s (-1.5%)</td><td align="right">0.1G (-2.32%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.023567105</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">11</td><td align="right">4</td><td align="right">4.03324474 (+0.2405%)</td><td align="right">0.016s (-31.9%)</td><td align="right">0.2G (-28.70%)</td><td align="right">9</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.712391773</td><td align="right">0.250s</td><td align="right">2.8G</td><td align="right">10</td><td align="right">5</td><td align="right">4.754013143 (+0.8832%)</td><td align="right">0.165s (-33.9%)</td><td align="right">1.8G (-37.08%)</td><td align="right">10</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.060s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.056s (-6.4%)</td><td align="right">0.7G (-3.94%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.30s</td><td align="right">34.2G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">3.04s (-7.9%)</td><td align="right">33.2G (-3.01%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">21.0s</td><td align="right">182.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.620539396 (+1.1539%)</td><td align="right">16.3s (-22.7%)</td><td align="right">127.2G (-30.44%)</td><td align="right">135</td><td align="right">5</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-12.0%)</td><td align="right">0.1G (-1.11%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+0.6%)</td><td align="right">0.1G (-0.91%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.77s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.71s (-2.2%)</td><td align="right">30.6G (-2.68%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">4.03s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.41s (-15.5%)</td><td align="right">35.9G (-6.99%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.03s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.89s (-3.3%)</td><td align="right">35.3G (-12.66%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.1s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">10.6s (+4.5%)</td><td align="right">95.4G (-2.54%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.65s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.27s (-10.3%)</td><td align="right">34.7G (-2.33%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">5.10s</td><td align="right">47.6G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">4.59s (-10.0%)</td><td align="right">40.4G (-15.18%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.99s</td><td align="right">59.9G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.53s (-7.6%)</td><td align="right">55.5G (-7.29%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.94s</td><td align="right">60.7G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.84s (-15.8%)</td><td align="right">56.0G (-7.69%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">6.55s</td><td align="right">59.0G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">6.11s (-6.7%)</td><td align="right">54.3G (-7.92%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">9.49s</td><td align="right">81.3G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">8.98s (-5.4%)</td><td align="right">66.8G (-17.90%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">9.89s</td><td align="right">82.8G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">8.56s (-13.4%)</td><td align="right">69.8G (-15.74%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">10.2s</td><td align="right">86.6G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">9.47s (-7.0%)</td><td align="right">74.8G (-13.55%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.715s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.713s (-0.3%)</td><td align="right">6.9G (-4.03%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.078067323 (-8.7135%)</td><td align="right">0.001s (+0.1%)</td><td align="right">0.1G (+1.89%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367 (+0.0797%)</td><td align="right">0.007s (+8.1%)</td><td align="right">0.1G (+0.27%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.023567105</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">11</td><td align="right">4</td><td align="right">3.892209764 (-3.2647%)</td><td align="right">0.023s (+0.1%)</td><td align="right">0.3G (+0.16%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.712391773</td><td align="right">0.250s</td><td align="right">2.8G</td><td align="right">10</td><td align="right">5</td><td align="right">4.509265423 (-4.3105%)</td><td align="right">0.252s (+1.0%)</td><td align="right">2.9G (+2.85%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.060s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502 (+0.7165%)</td><td align="right">0.065s (+7.5%)</td><td align="right">0.7G (+2.03%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.30s</td><td align="right">34.2G</td><td align="right">189</td><td align="right">3</td><td align="right">8.009172258 (+2.5773%)</td><td align="right">2.80s (-15.1%)</td><td align="right">31.2G (-8.80%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">21.0s</td><td align="right">182.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.512433077 (-0.7917%)</td><td align="right">24.7s (+17.5%)</td><td align="right">183.2G (+0.21%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653 (-0.8182%)</td><td align="right">0.004s (-15.0%)</td><td align="right">0.1G (-0.50%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (-4.1040%)</td><td align="right">0.001s (+8.9%)</td><td align="right">0.1G (+0.05%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.77s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.438064454 (+0.6171%)</td><td align="right">2.97s (+6.9%)</td><td align="right">33.9G (+7.69%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">4.03s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.378824196 (-0.2496%)</td><td align="right">3.93s (-2.6%)</td><td align="right">38.4G (-0.51%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">4.03s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.56659036 (-0.1426%)</td><td align="right">4.00s (-0.7%)</td><td align="right">40.1G (-0.63%)</td><td align="right">220</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.1s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.192724425 (-3.0848%)</td><td align="right">8.91s (-12.1%)</td><td align="right">85.9G (-12.24%)</td><td align="right">1910</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.65s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451 (+2.5761%)</td><td align="right">3.40s (-6.8%)</td><td align="right">35.4G (-0.49%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.715s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.903208727 (-0.0795%)</td><td align="right">0.683s (-4.5%)</td><td align="right">6.9G (-4.45%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-76.4%)</td><td align="right">0.1G (-29.50%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">5</td><td align="right">2</td><td align="right">6.862755928 (-0.0042%)</td><td align="right">0.007s (-69.3%)</td><td align="right">0.1G (-54.72%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.026557116</td><td align="right">0.119s</td><td align="right">1.3G</td><td align="right">12</td><td align="right">5</td><td align="right">4.023567105 (-0.0743%)</td><td align="right">0.023s (-80.6%)</td><td align="right">0.3G (-76.26%)</td><td align="right">11</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.736412597</td><td align="right">1.89s</td><td align="right">20.2G</td><td align="right">11</td><td align="right">6</td><td align="right">4.712391773 (-0.5072%)</td><td align="right">0.250s (-86.8%)</td><td align="right">2.8G (-86.05%)</td><td align="right">10</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.738927979</td><td align="right">0.136s</td><td align="right">1.4G</td><td align="right">80</td><td align="right">3</td><td align="right">6.740943136 (+0.0299%)</td><td align="right">0.060s (-55.7%)</td><td align="right">0.7G (-49.81%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.805465772</td><td align="right">7.94s</td><td align="right">63.0G</td><td align="right">220</td><td align="right">4</td><td align="right">7.807937174 (+0.0317%)</td><td align="right">3.30s (-58.5%)</td><td align="right">34.2G (-45.75%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.55442136</td><td align="right">155.7s</td><td align="right">1115.4G</td><td align="right">766</td><td align="right">9</td><td align="right">5.556421705 (+0.0360%)</td><td align="right">21.0s (-86.5%)</td><td align="right">182.8G (-83.61%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.005s (-61.8%)</td><td align="right">0.1G (-49.77%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-49.8%)</td><td align="right">0.1G (-37.18%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502028585</td><td align="right">9.20s</td><td align="right">78.1G</td><td align="right">144</td><td align="right">3</td><td align="right">7.392442593 (-1.4608%)</td><td align="right">2.77s (-69.9%)</td><td align="right">31.5G (-59.68%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392441014</td><td align="right">11.9s</td><td align="right">120.6G</td><td align="right">251</td><td align="right">3</td><td align="right">5.392285003 (-0.0029%)</td><td align="right">4.03s (-66.1%)</td><td align="right">38.6G (-67.98%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578435633</td><td align="right">7.76s</td><td align="right">81.7G</td><td align="right">301</td><td align="right">3</td><td align="right">5.574537176 (-0.0699%)</td><td align="right">4.03s (-48.1%)</td><td align="right">40.4G (-50.56%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">8.432467909</td><td align="right">5.55s</td><td align="right">52.1G</td><td align="right">114</td><td align="right">4</td><td align="right">7.421664324 (-11.9870%)</td><td align="right">10.1s (+82.6%)</td><td align="right">97.9G (+87.95%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">7.938575228</td><td align="right">6.93s</td><td align="right">57.0G</td><td align="right">25</td><td align="right">4</td><td align="right">8.235585529 (+3.7414%)</td><td align="right">3.65s (-47.3%)</td><td align="right">35.5G (-37.63%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.88554258</td><td align="right">2.23s</td><td align="right">22.4G</td><td align="right">184</td><td align="right">3</td><td align="right">5.907904741 (+0.3800%)</td><td align="right">0.715s (-67.9%)</td><td align="right">7.2G (-67.94%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (-50.5%)</td><td align="right">0.1G (-37.18%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">5</td><td align="right">2</td><td align="right">6.861229775 (-0.0265%)</td><td align="right">0.007s (-43.5%)</td><td align="right">0.1G (-33.03%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.285012668</td><td align="right">0.031s</td><td align="right">0.4G</td><td align="right">56</td><td align="right">2</td><td align="right">4.283072584 (-0.0453%)</td><td align="right">0.009s (-71.9%)</td><td align="right">0.2G (-60.58%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.600443859</td><td align="right">0.698s</td><td align="right">5.9G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.100s (-85.7%)</td><td align="right">1.1G (-80.52%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739721413</td><td align="right">0.168s</td><td align="right">0.8G</td><td align="right">80</td><td align="right">2</td><td align="right">6.739575295 (-0.0022%)</td><td align="right">0.043s (-74.2%)</td><td align="right">0.5G (-35.26%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.9500396</td><td align="right">4.41s</td><td align="right">29.3G</td><td align="right">496</td><td align="right">2</td><td align="right">7.949978834 (-0.0008%)</td><td align="right">2.35s (-46.7%)</td><td align="right">23.9G (-18.60%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.742988533</td><td align="right">45.2s</td><td align="right">251.8G</td><td align="right">11809</td><td align="right">2</td><td align="right">6.754216663 (+0.1665%)</td><td align="right">22.0s (-51.4%)</td><td align="right">117.7G (-53.25%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-49.2%)</td><td align="right">0.1G (-3.02%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-57.6%)</td><td align="right">0.1G (-38.66%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.50595639</td><td align="right">6.58s</td><td align="right">55.2G</td><td align="right">142</td><td align="right">2</td><td align="right">7.400445378 (-1.4057%)</td><td align="right">2.90s (-55.9%)</td><td align="right">32.3G (-41.51%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393312779</td><td align="right">4.70s</td><td align="right">43.9G</td><td align="right">332</td><td align="right">2</td><td align="right">5.393055049 (-0.0048%)</td><td align="right">4.07s (-13.3%)</td><td align="right">41.7G (-4.91%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.579216889</td><td align="right">5.84s</td><td align="right">58.6G</td><td align="right">301</td><td align="right">2</td><td align="right">5.571539329 (-0.1376%)</td><td align="right">4.23s (-27.6%)</td><td align="right">41.2G (-29.64%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.131110023</td><td align="right">5.04s</td><td align="right">36.7G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (+1.2849%)</td><td align="right">3.14s (-37.7%)</td><td align="right">31.6G (-14.01%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.892212121</td><td align="right">1.70s</td><td align="right">17.2G</td><td align="right">184</td><td align="right">2</td><td align="right">5.907904741 (+0.2663%)</td><td align="right">0.735s (-56.8%)</td><td align="right">6.9G (-59.97%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>
