## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`); `time` is `--timing-json`'s `timing.total_s`. One run per `-N10` row (deterministic; `instr` carries the comparison); interleaved minimum of 3 for `-N1` rows. Driver and every row: [`columnar_wip/bench-n1-gaps.py`](columnar_wip/bench-n1-gaps.py), [`columnar_wip/n1gaps-ab-results.tsv`](columnar_wip/n1gaps-ab-results.tsv).

> **This PR takes out work two post-trial passes repeated: the `-N1` run-level rescue re-swept the
> leaves its collapsed trial had just swept (#1120), and the deep repair re-aggregated the whole
> network every round when its seed was the one-level fallback (the cost behind #1122).** Two changes,
> each on rows the other never reaches, plus a comment. **(1) Pass-1 reuse.** When a `-N1` hierarchical
> run collapses to one module, #1081's rescue runs the two-level search on the trial's own seed, whose
> pass 1 is the trial's leaf sweep bit for bit; the lone trial now keeps what that sweep left (assignment
> and module aggregates) and the rescue starts at the first aggregation pass. Bit-identical on every row;
> the rescued rows −3% to −13.5% in seconds. **(2) Block-granularity extraction from a one-module repair
> seed.** When every trial collapsed and the deep repair starts from the one-level fallback (the E50000
> `--regularized` family), each fresh derivation stops at its pass-1 building blocks instead of
> aggregating to convergence, and a round has to buy 5e-4 of the codelength per network re-clustered
> instead of 1e-4. What the repair peels off there are groups of 3–23 states, finer than the aggregated
> pieces it used to draw (~12–95 states), so each old round re-aggregated ~all of the network to expose a
> few of them. 18 rows, every one better on both axes: −0.075% to −0.245% in bits and −5.8% to −49% in
> seconds. Every repair whose seed has more than one module is untouched. **Not in this PR:** #1122's
> one-liner (let `-d -N1` pay that repair as `-2d -N1` does) is measured below as a third arm and left as a
> decision — −0.20% to −0.66% in bits for +48% to +83% in seconds; and #1121 — no cheap verdict separates
> the overlapping family from the healthy memory networks at `-N1`, so `-N1` stays one hierarchical-first
> trial, documented where the trial alternation is defined (F59).

> **Old** = a fresh `MODE=release OPENMP=0` build of `columnar-hierarchical-core` tip `bc31f036`, md5
> `10d2ec79dcbc307890d4b4e550171c50` — the #1081 snapshot's new binary; its column here reproduces that
> snapshot's new column bit for bit on all 165 shared rows, at a median +0.08% in instructions (and +11.6%
> in seconds: a busier machine, so read times within this session only). **New** = this PR at
> `740c3dbb`, md5 `1b8e57233835ea68322601026b33dd63` (`src/` is unchanged after it). **Fix** = `740c3dbb` + the #1122
> one-liner, md5 `aad38435e51ab666c62fec6b99d26b4c`, on the six rows that line touches. One session, arms
> interleaved per row, `-N1` rows as the minimum of 3 reps spread across the batch, `-N10` rows once.
> **The object-oriented arms are not re-run** — this PR does not touch that engine (project
> instructions) — their cells in the two OO tables are carried from the #1079-day session on the same
> machine, at the precision printed there; the columnar cells next to them are this session's.

### What the change moves

Every configuration where old and new differ in bits, both arms. Every move is change (2), on the
E50000 `--regularized` family, at `-N1` and `-N10`, `-2d` and `-d`.

| network | table | old bits | new bits | Δbits | old instr | new instr | Δinstr | old time | new time | Δtime |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| om3 E50000 `-2d --regularized -N1` | family | 7.931196935 | **7.925216272** | **-0.0754%** | 10.7G | 5.3G | -50.42% | 1.19s | 0.601s | -49.3% |
| om4 E50000 `-2d --regularized -N1` | family | 7.940858042 | **7.92621405** | **-0.1844%** | 10.6G | 6.2G | -41.34% | 1.22s | 0.748s | -38.6% |
| om5 E50000 `-2d --regularized -N1` | family | 7.969030601 | **7.956672505** | **-0.1551%** | 12.0G | 6.3G | -47.53% | 1.39s | 0.762s | -45.4% |
| om6 E50000 `-2d --regularized -N1` | family | 7.960481009 | **7.944047825** | **-0.2064%** | 8.0G | 5.2G | -35.69% | 0.956s | 0.626s | -34.5% |
| om7 E50000 `-2d --regularized -N1` | family | 7.977053735 | **7.957532546** | **-0.2447%** | 7.9G | 5.8G | -26.61% | 0.934s | 0.692s | -26.0% |
| om8 E50000 `-2d --regularized -N1` | family | 7.991103173 | **7.978376075** | **-0.1593%** | 6.5G | 5.2G | -20.37% | 0.783s | 0.641s | -18.2% |
| om3 E50000 `-d --regularized -N10` | family | 7.931196935 | **7.925216272** | **-0.0754%** | 28.1G | 22.7G | -19.19% | 3.22s | 2.57s | -20.2% |
| om4 E50000 `-2d --regularized -N10` | family | 7.940858042 | **7.92621405** | **-0.1844%** | 27.4G | 23.0G | -15.94% | 3.19s | 2.86s | -10.3% |
| om6 E50000 `-d --regularized -N10` | family | 7.960481009 | **7.944047825** | **-0.2064%** | 24.4G | 21.5G | -11.75% | 3.21s | 2.78s | -13.2% |
| om7 E50000 `-2d --regularized -N10` | family | 7.977053735 | **7.957532546** | **-0.2447%** | 25.4G | 23.3G | -8.24% | 3.19s | 2.93s | -8.3% |
| om4 E50000 `-d --regularized -N10` | family | 7.940858042 | **7.92621405** | **-0.1844%** | 27.2G | 22.9G | -16.10% | 3.27s | 2.79s | -14.7% |
| om5 E50000 `-2d --regularized -N10` | family | 7.969030601 | **7.956672505** | **-0.1551%** | 29.0G | 23.3G | -19.68% | 3.60s | 2.83s | -21.2% |
| om7 E50000 `-d --regularized -N10` | family | 7.977053735 | **7.957532546** | **-0.2447%** | 24.4G | 22.3G | -8.66% | 3.30s | 2.91s | -11.8% |
| om8 E50000 `-2d --regularized -N10` | family | 7.991103173 | **7.978376075** | **-0.1593%** | 23.9G | 22.6G | -5.52% | 3.10s | 2.86s | -7.7% |
| om3 E50000 `-2d --regularized -N10` | family | 7.931196935 | **7.925216272** | **-0.0754%** | 27.9G | 22.5G | -19.32% | 3.16s | 2.51s | -20.6% |
| om5 E50000 `-d --regularized -N10` | family | 7.969030601 | **7.956672505** | **-0.1551%** | 28.3G | 22.6G | -20.18% | 3.59s | 2.98s | -17.0% |
| om6 E50000 `-2d --regularized -N10` | family | 7.960481009 | **7.944047825** | **-0.2064%** | 24.7G | 21.9G | -11.62% | 3.24s | 2.93s | -9.6% |
| om8 E50000 `-d --regularized -N10` | family | 7.991103173 | **7.978376075** | **-0.1593%** | 23.1G | 21.7G | -5.74% | 3.07s | 2.89s | -5.8% |

**Every cell where new is worse than old, and why.** No cell is worse in bits. No bit-identical row is
worse by more than +0.17% in instructions. 36 cells read slower in seconds, all bit-identical and all at
−0.5% to +0.17% in instructions — neither change runs on them (no collapsed lone trial, no one-module
repair seed). The three largest were re-measured after the batch, three interleaved reps per arm, and
appended as reps 2–4 (the tables show the minimum): om6 `-F -d -N10` +14.5% → +1.2% (old 7.35–7.50 s, new
7.44–7.56 s), wikispeedia `-d -N10` +9.4% → +0.2%, om7 `-d -N10` +4.9% → +1.8% (old 8.85–9.28 s, new
9.01–9.19 s), at +0.00% to +0.03% in instructions. The rest are +1.0% to +5.1%, sub-millisecond rows
included; the session's noise floor at load 5–14.

**Where the time goes down at the same bits — change (1).** Every `-N1` row the rescue fires on:
om3 / om4 `-d -N1` −5.0% / −5.2% in seconds (−2.9% / −2.5% in instructions), om3 / om4 `-F -N1` −3.4% /
−4.2%, the E100000 `-d --regularized -N1` rows om2–om8 −6% to −12%, the E50000 ones om3–om8 −10.4% to
−13.5% (−7.6% to −10.6% in instructions). lazega and the multilayer example (−6% to −9% in
instructions at 0.06–0.1G) are sub-millisecond runs whose instruction counts moved by the same amount
between binaries in #1081's session too: startup, not search.

### Per-feature attribution

The two changes act on disjoint rows by construction — (1) only when a lone `-N1` hierarchical trial
collapsed (the kept pass-1 state exists only then), (2) only when the deep repair's seed is one module
(every trial collapsed and the run-level rescue did not beat one module; at `-N1` without the #1122
one-liner that repair has no fresh discovery, so (2) never runs on a `-d -N1` row). Every moved row in
this snapshot is therefore one feature's alone: the bits column above is (2), the rescued rows' time is
(1). Each was also measured alone against the tip, on its own binary, before they were combined (F59):
(1) bit-identical on 24 rows, rescued rows −2.5% to −18.6% in seconds, healthy `-N1` rows within ±0.13% in
instructions. Inside (2), the yield bar, on the experiment binaries (`-2d --regularized`, E50000; Δ against
the tip):

| row | stop at blocks, 1e-4 bar | stop at blocks, 5e-4 bar (shipped) |
|---|---|---|
| om3 `-2d -N1` | −0.102% bits / −34% s | −0.075% / −46% |
| om4 `-2d -N1` | −0.184% / −41% | −0.184% / −38% |
| om5 `-2d -N1` | −0.198% / −26% | −0.155% / −44% |
| om6 `-2d -N1` | −0.268% / −5% | −0.206% / −32% |
| om7 `-2d -N1` | −0.312% / −4% | −0.245% / −26% |
| om8 `-2d -N1` | −0.174% / **+7%** | −0.159% / −18% |
| om3–om8 `-2d -N10` | −0.10% to −0.31% / −14% to +1% | −0.08% to −0.25% / −3% to −20% |

The 5e-4 bar gives up 0.02–0.07% in bits against the 1e-4 bar and is faster than the tip on every row;
the 1e-4 bar is slower than the tip on om8. Rejected (F59): pass-1 blocks as an extra source beside the
aggregated pieces on every seed (om8 repair 0.44 → 0.93 s, and it reaches every guard row); blocks for the
re-derivations only (slower than the cut everywhere); one block derivation and stop (om3 +0.11%, om4
+0.16% in bits against the tip).

### #1122's one-liner, as a third arm

Raising the escalation signal from the rescue whether or not the rescue is kept lets the `-d -N1` repair
run the extraction `-2d -N1` runs, so `-d -N1` returns `-2d -N1`'s partition (#1081's contract). On top of
change (2) it costs:

| row | old | new (this PR) | new + one-liner |
|---|---|---|---|
| om3 E50000 `-d --regularized -N1` | 7.969664223, 0.467s, 1 top | 7.969664223 (=), 0.408s (-12.5%) | 7.925216272 (-0.5577%), 0.740s (+58.6%), +57.6% instr, 89 top |
| om4 E50000 `-d --regularized -N1` | 7.978790193, 0.507s, 1 top | 7.978790193 (=), 0.453s (-10.6%) | 7.926214050 (-0.6589%), 0.878s (+73.3%), +68.3% instr, 110 top |
| om5 E50000 `-d --regularized -N1` | 7.988627070, 0.535s, 1 top | 7.988627070 (=), 0.480s (-10.4%) | 7.956672505 (-0.4000%), 0.907s (+69.4%), +66.8% instr, 101 top |
| om6 E50000 `-d --regularized -N1` | 7.989209626, 0.472s, 1 top | 7.989209626 (=), 0.408s (-13.5%) | 7.944047825 (-0.5653%), 0.696s (+47.5%), +51.5% instr, 94 top |
| om7 E50000 `-d --regularized -N1` | 7.992344954, 0.431s, 1 top | 7.992344954 (=), 0.380s (-11.7%) | 7.957532546 (-0.4356%), 0.788s (+83.0%), +77.2% instr, 112 top |
| om8 E50000 `-d --regularized -N1` | 7.994534803, 0.448s, 1 top | 7.994534803 (=), 0.391s (-12.7%) | 7.978376075 (-0.2021%), 0.683s (+52.6%), +58.2% instr, 77 top |

Left out of this PR: −0.20% to −0.66% in bits does not clear +48% to +83% in seconds under the
trade-off rule on its own. Without (2) the same line cost +106% to +205% for −0.04% to −0.48% (F58).

### Old vs new columnar — standard search (`-C -N10`)

Overlapping and wikispeedia rows run `-C -d -N10`. Neither change runs at `-N10` on these rows (no
lone trial; no repair seed of one module), so every row is bit-identical and the time column is the
session's noise floor (see "Every cell where new is worse" above for the re-measured largest cells).

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-0.5%)</td><td align="right">0.1G (+0.15%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.007s (-0.4%)</td><td align="right">0.1G (-0.10%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.048857953 (=)</td><td align="right">0.023s (-1.3%)</td><td align="right">0.3G (+0.01%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.241s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.717760238 (=)</td><td align="right">0.237s (-1.3%)</td><td align="right">2.8G (-0.01%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.060s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.059s (-2.9%)</td><td align="right">0.7G (+0.17%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.04s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">3.03s (-0.4%)</td><td align="right">34.1G (+0.01%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">20.3s</td><td align="right">181.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">19.9s (-1.7%)</td><td align="right">181.8G (-0.00%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-0.3%)</td><td align="right">0.1G (-5.88%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-2.9%)</td><td align="right">0.1G (-0.64%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.73s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.67s (-2.5%)</td><td align="right">31.5G (-0.01%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.58s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.50s (-2.3%)</td><td align="right">38.6G (-0.01%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.89s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.90s (+0.5%)</td><td align="right">40.4G (-0.00%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.1s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">10.1s (+0.3%)</td><td align="right">97.9G (+0.01%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.32s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.36s (+1.2%)</td><td align="right">35.5G (+0.02%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.72s</td><td align="right">47.5G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">4.70s (-0.5%)</td><td align="right">47.5G (-0.03%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.66s</td><td align="right">59.8G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.63s (-0.5%)</td><td align="right">59.8G (-0.01%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.34s</td><td align="right">60.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">6.29s (-0.7%)</td><td align="right">60.6G (+0.01%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">6.27s</td><td align="right">58.9G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">6.32s (+0.8%)</td><td align="right">59.0G (+0.01%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">8.96s</td><td align="right">81.2G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">8.71s (-2.8%)</td><td align="right">81.2G (+0.03%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">8.85s</td><td align="right">82.7G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">9.01s (+1.8%)</td><td align="right">82.7G (+0.00%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">9.82s</td><td align="right">86.5G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">9.78s (-0.5%)</td><td align="right">86.4G (-0.02%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">6.773456578</td><td align="right">3.23s</td><td align="right">33.3G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.31s (+2.5%)</td><td align="right">33.2G (-0.02%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.851498646</td><td align="right">4.74s</td><td align="right">44.6G</td><td align="right">617</td><td align="right">4</td><td align="right">5.851498646 (=)</td><td align="right">4.73s (-0.2%)</td><td align="right">44.6G (+0.01%)</td><td align="right">617</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.876717868</td><td align="right">5.35s</td><td align="right">47.4G</td><td align="right">1031</td><td align="right">4</td><td align="right">4.876717868 (=)</td><td align="right">5.27s (-1.5%)</td><td align="right">47.4G (-0.01%)</td><td align="right">1031</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.150346795</td><td align="right">5.89s</td><td align="right">51.1G</td><td align="right">2171</td><td align="right">4</td><td align="right">4.150346795 (=)</td><td align="right">5.89s (+0.1%)</td><td align="right">51.1G (-0.02%)</td><td align="right">2171</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.617750514</td><td align="right">6.14s</td><td align="right">51.7G</td><td align="right">3050</td><td align="right">4</td><td align="right">3.617750514 (=)</td><td align="right">6.05s (-1.4%)</td><td align="right">51.6G (-0.03%)</td><td align="right">3050</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.201872271</td><td align="right">7.92s</td><td align="right">65.1G</td><td align="right">3424</td><td align="right">6</td><td align="right">3.201872271 (=)</td><td align="right">7.96s (+0.4%)</td><td align="right">65.1G (+0.09%)</td><td align="right">3424</td><td align="right">6</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.883308449</td><td align="right">8.58s</td><td align="right">69.8G</td><td align="right">4367</td><td align="right">5</td><td align="right">2.883308449 (=)</td><td align="right">8.16s (-5.0%)</td><td align="right">69.8G (+0.07%)</td><td align="right">4367</td><td align="right">5</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.710s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.711s (+0.2%)</td><td align="right">7.2G (+0.01%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Overlapping and wikispeedia rows as `-C -2d -N10`. Every row is bit-identical: change (2) needs a
one-module repair seed, which only the E50000 `--regularized` rows produce (family table below), and
(1) is a hierarchical-trial change. The time column is the session's noise floor.

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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.001s (-5.5%)</td><td align="right">0.1G (-0.45%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.861229775</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (=)</td><td align="right">0.007s (+0.9%)</td><td align="right">0.1G (-0.27%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.283072584</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">4.283072584 (=)</td><td align="right">0.009s (+4.5%)</td><td align="right">0.2G (-0.05%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.63729688</td><td align="right">0.098s</td><td align="right">1.1G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.098s (-0.3%)</td><td align="right">1.1G (-0.03%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739575295</td><td align="right">0.043s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">6.739575295 (=)</td><td align="right">0.043s (+0.9%)</td><td align="right">0.5G (-0.04%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.949978834</td><td align="right">2.34s</td><td align="right">23.9G</td><td align="right">506</td><td align="right">2</td><td align="right">7.949978834 (=)</td><td align="right">2.38s (+2.1%)</td><td align="right">23.9G (+0.01%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.754216663</td><td align="right">19.6s</td><td align="right">117.5G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.754216663 (=)</td><td align="right">19.4s (-1.0%)</td><td align="right">117.5G (+0.00%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-8.2%)</td><td align="right">0.1G (-6.48%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-9.8%)</td><td align="right">0.1G (-9.12%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.400445378</td><td align="right">2.83s</td><td align="right">32.3G</td><td align="right">168</td><td align="right">2</td><td align="right">7.400445378 (=)</td><td align="right">2.79s (-1.4%)</td><td align="right">32.3G (+0.00%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393055049</td><td align="right">4.45s</td><td align="right">41.8G</td><td align="right">334</td><td align="right">2</td><td align="right">5.393055049 (=)</td><td align="right">3.89s (-12.6%)</td><td align="right">41.7G (-0.04%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571539329</td><td align="right">3.92s</td><td align="right">41.2G</td><td align="right">304</td><td align="right">2</td><td align="right">5.571539329 (=)</td><td align="right">3.93s (+0.3%)</td><td align="right">41.2G (-0.03%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.424143707</td><td align="right">10.8s</td><td align="right">104.5G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.424143707 (=)</td><td align="right">10.8s (-0.8%)</td><td align="right">104.5G (-0.01%)</td><td align="right">2237</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.09s</td><td align="right">31.6G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.07s (-0.8%)</td><td align="right">31.6G (-0.01%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.740761645</td><td align="right">3.66s</td><td align="right">35.6G</td><td align="right">625</td><td align="right">2</td><td align="right">6.740761645 (=)</td><td align="right">3.72s (+1.5%)</td><td align="right">35.6G (+0.00%)</td><td align="right">625</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.822832994</td><td align="right">7.01s</td><td align="right">77.3G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">6.96s (-0.7%)</td><td align="right">77.3G (-0.01%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.866901786</td><td align="right">7.47s</td><td align="right">70.7G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">7.32s (-2.0%)</td><td align="right">70.7G (+0.00%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.867301407</td><td align="right">7.25s</td><td align="right">68.4G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">7.21s (-0.5%)</td><td align="right">68.4G (+0.02%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.88496897</td><td align="right">7.97s</td><td align="right">69.0G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">7.53s (-5.6%)</td><td align="right">69.0G (-0.00%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.88992089</td><td align="right">7.70s</td><td align="right">69.2G</td><td align="right">663</td><td align="right">2</td><td align="right">6.88992089 (=)</td><td align="right">7.94s (+3.1%)</td><td align="right">69.2G (-0.01%)</td><td align="right">663</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.88742315</td><td align="right">8.72s</td><td align="right">74.5G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">8.70s (-0.3%)</td><td align="right">74.5G (-0.01%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-2d`</td><td align="right">6.773456578</td><td align="right">3.72s</td><td align="right">37.8G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.82s (+2.7%)</td><td align="right">37.8G (+0.00%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-2d`</td><td align="right">6.258611497</td><td align="right">4.64s</td><td align="right">36.8G</td><td align="right">3236</td><td align="right">2</td><td align="right">6.258611497 (=)</td><td align="right">4.66s (+0.4%)</td><td align="right">36.8G (+0.01%)</td><td align="right">3236</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 E50000 `-2d`</td><td align="right">5.454385527</td><td align="right">4.36s</td><td align="right">33.8G</td><td align="right">4381</td><td align="right">2</td><td align="right">5.454385527 (=)</td><td align="right">4.40s (+0.9%)</td><td align="right">33.8G (-0.00%)</td><td align="right">4381</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 E50000 `-2d`</td><td align="right">4.903270512</td><td align="right">4.34s</td><td align="right">31.9G</td><td align="right">5443</td><td align="right">2</td><td align="right">4.903270512 (=)</td><td align="right">4.37s (+0.8%)</td><td align="right">31.9G (-0.03%)</td><td align="right">5443</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 E50000 `-2d`</td><td align="right">4.468778176</td><td align="right">4.90s</td><td align="right">32.4G</td><td align="right">6379</td><td align="right">2</td><td align="right">4.468778176 (=)</td><td align="right">4.71s (-3.9%)</td><td align="right">32.4G (-0.06%)</td><td align="right">6379</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 E50000 `-2d`</td><td align="right">4.143277395</td><td align="right">4.36s</td><td align="right">31.2G</td><td align="right">7097</td><td align="right">2</td><td align="right">4.143277395 (=)</td><td align="right">4.40s (+0.7%)</td><td align="right">31.2G (-0.05%)</td><td align="right">7097</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 E50000 `-2d`</td><td align="right">3.859069372</td><td align="right">4.50s</td><td align="right">31.6G</td><td align="right">7985</td><td align="right">2</td><td align="right">3.859069372 (=)</td><td align="right">4.47s (-0.7%)</td><td align="right">31.6G (+0.00%)</td><td align="right">7985</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.907904741</td><td align="right">0.680s</td><td align="right">6.9G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.673s (-1.0%)</td><td align="right">6.9G (-0.05%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

Interleaved minimum of 3 per arm. Change (1) fires on the rows the run-level rescue fires on — om3 /
om4 `-d -N1` (and `-F`, next section): bit-identical, −5% in seconds, the rescue no longer re-sweeping
the leaves its trial swept. Every other row is bit-identical with an instruction delta under ±0.2%. The
`-d -N1` rows of om2 / om5–om8 still return a refined hierarchical build 1–14% above `-2d -N1` — the
`-N1` property #1121 documents (F59) — and om4 `-N1` beating `-N10` stays #1083.

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.000s (+5.0%)</td><td align="right">0.1G (-0.50%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.899367957</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">2</td><td align="right">6.899367957 (=)</td><td align="right">0.001s (+0.4%)</td><td align="right">0.1G (-0.50%)</td><td align="right">11</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.047459862</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">4</td><td align="right">4.047459862 (=)</td><td align="right">0.003s (+2.1%)</td><td align="right">0.1G (-0.25%)</td><td align="right">7</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.730850312</td><td align="right">0.026s</td><td align="right">0.4G</td><td align="right">12</td><td align="right">5</td><td align="right">4.730850312 (=)</td><td align="right">0.026s (-0.8%)</td><td align="right">0.4G (-0.07%)</td><td align="right">12</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.758265349</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">3</td><td align="right">3</td><td align="right">6.758265349 (=)</td><td align="right">0.009s (-0.6%)</td><td align="right">0.2G (-0.30%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">0.399s</td><td align="right">5.7G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">0.402s (+0.9%)</td><td align="right">5.7G (-0.04%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">2.49s</td><td align="right">24.4G</td><td align="right">5</td><td align="right">6</td><td align="right">5.556421705 (=)</td><td align="right">2.41s (-3.2%)</td><td align="right">24.4G (-0.01%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.041117399</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.041117399 (=)</td><td align="right">0.001s (-2.5%)</td><td align="right">0.1G (-1.71%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (+4.6%)</td><td align="right">0.1G (-0.37%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.491980364</td><td align="right">0.370s</td><td align="right">5.3G</td><td align="right">148</td><td align="right">3</td><td align="right">7.491980364 (=)</td><td align="right">0.364s (-1.5%)</td><td align="right">5.3G (-0.02%)</td><td align="right">148</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.470440768</td><td align="right">0.377s</td><td align="right">4.5G</td><td align="right">242</td><td align="right">3</td><td align="right">5.470440768 (=)</td><td align="right">0.373s (-1.1%)</td><td align="right">4.5G (+0.04%)</td><td align="right">242</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.657913279</td><td align="right">0.488s</td><td align="right">5.9G</td><td align="right">197</td><td align="right">3</td><td align="right">5.657913279 (=)</td><td align="right">0.499s (+2.1%)</td><td align="right">5.9G (+0.00%)</td><td align="right">197</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.546335898</td><td align="right">0.941s</td><td align="right">9.2G</td><td align="right">1614</td><td align="right">3</td><td align="right">7.546335898 (=)</td><td align="right">0.894s (-5.0%)</td><td align="right">9.2G (-0.03%)</td><td align="right">1614</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.460796773</td><td align="right">0.413s</td><td align="right">5.8G</td><td align="right">5</td><td align="right">3</td><td align="right">8.460796773 (=)</td><td align="right">0.411s (-0.5%)</td><td align="right">5.8G (+0.02%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739607071</td><td align="right">1.34s</td><td align="right">14.0G</td><td align="right">669</td><td align="right">2</td><td align="right">6.739607071 (=)</td><td align="right">1.31s (-1.9%)</td><td align="right">14.0G (-0.01%)</td><td align="right">669</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.823562921</td><td align="right">1.14s</td><td align="right">12.9G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.16s (+1.6%)</td><td align="right">12.9G (+0.03%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.861732911</td><td align="right">1.51s</td><td align="right">15.7G</td><td align="right">139</td><td align="right">2</td><td align="right">6.861732911 (=)</td><td align="right">1.49s (-1.0%)</td><td align="right">15.7G (-0.02%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.868180827</td><td align="right">1.76s</td><td align="right">17.2G</td><td align="right">291</td><td align="right">2</td><td align="right">6.868180827 (=)</td><td align="right">1.70s (-3.5%)</td><td align="right">17.2G (-0.03%)</td><td align="right">291</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884236095</td><td align="right">1.71s</td><td align="right">16.3G</td><td align="right">453</td><td align="right">2</td><td align="right">6.884236095 (=)</td><td align="right">1.69s (-1.0%)</td><td align="right">16.3G (-0.04%)</td><td align="right">453</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.889674586</td><td align="right">1.81s</td><td align="right">17.2G</td><td align="right">649</td><td align="right">2</td><td align="right">6.889674586 (=)</td><td align="right">1.83s (+1.1%)</td><td align="right">17.2G (-0.04%)</td><td align="right">649</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.894258582</td><td align="right">1.92s</td><td align="right">17.9G</td><td align="right">907</td><td align="right">2</td><td align="right">6.894258582 (=)</td><td align="right">1.93s (+0.7%)</td><td align="right">17.9G (+0.06%)</td><td align="right">907</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.29196634</td><td align="right">0.372s</td><td align="right">3.7G</td><td align="right">321</td><td align="right">4</td><td align="right">7.29196634 (=)</td><td align="right">0.367s (-1.4%)</td><td align="right">3.7G (+0.04%)</td><td align="right">321</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.823562921</td><td align="right">1.58s</td><td align="right">17.5G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.50s (-5.0%)</td><td align="right">17.0G (-2.94%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.861732911</td><td align="right">2.07s</td><td align="right">21.3G</td><td align="right">139</td><td align="right">2</td><td align="right">6.861732911 (=)</td><td align="right">1.96s (-5.2%)</td><td align="right">20.7G (-2.48%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.812252898</td><td align="right">0.785s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.812252898 (=)</td><td align="right">0.778s (-0.8%)</td><td align="right">7.2G (+0.04%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.440161581</td><td align="right">0.824s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.440161581 (=)</td><td align="right">0.811s (-1.5%)</td><td align="right">7.5G (-0.01%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.192756466</td><td align="right">0.824s</td><td align="right">7.5G</td><td align="right">149</td><td align="right">4</td><td align="right">7.192756466 (=)</td><td align="right">0.818s (-0.7%)</td><td align="right">7.5G (+0.05%)</td><td align="right">149</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.967535764</td><td align="right">0.825s</td><td align="right">7.5G</td><td align="right">206</td><td align="right">4</td><td align="right">6.967535764 (=)</td><td align="right">0.840s (+1.9%)</td><td align="right">7.5G (+0.03%)</td><td align="right">206</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">7.144517469</td><td align="right">0.424s</td><td align="right">4.6G</td><td align="right">3</td><td align="right">3</td><td align="right">7.144517469 (=)</td><td align="right">0.429s (+1.1%)</td><td align="right">4.6G (+0.02%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.854829799</td><td align="right">0.434s</td><td align="right">4.3G</td><td align="right">537</td><td align="right">4</td><td align="right">5.854829799 (=)</td><td align="right">0.453s (+4.4%)</td><td align="right">4.3G (+0.14%)</td><td align="right">537</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.898784516</td><td align="right">0.443s</td><td align="right">4.2G</td><td align="right">1867</td><td align="right">3</td><td align="right">4.898784516 (=)</td><td align="right">0.449s (+1.4%)</td><td align="right">4.2G (-0.03%)</td><td align="right">1867</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.1554786</td><td align="right">0.552s</td><td align="right">5.0G</td><td align="right">2156</td><td align="right">4</td><td align="right">4.1554786 (=)</td><td align="right">0.556s (+0.7%)</td><td align="right">5.0G (-0.05%)</td><td align="right">2156</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.620157111</td><td align="right">0.681s</td><td align="right">6.0G</td><td align="right">3056</td><td align="right">4</td><td align="right">3.620157111 (=)</td><td align="right">0.684s (+0.3%)</td><td align="right">6.0G (+0.06%)</td><td align="right">3056</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.21639819</td><td align="right">0.645s</td><td align="right">5.5G</td><td align="right">3417</td><td align="right">5</td><td align="right">3.21639819 (=)</td><td align="right">0.634s (-1.7%)</td><td align="right">5.6G (+0.23%)</td><td align="right">3417</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.885785338</td><td align="right">0.927s</td><td align="right">8.1G</td><td align="right">4371</td><td align="right">5</td><td align="right">2.885785338 (=)</td><td align="right">0.952s (+2.7%)</td><td align="right">8.1G (+0.09%)</td><td align="right">4371</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om2 `-2d -c` planted</td><td align="right">6.744721993</td><td align="right">0.563s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.744721993 (=)</td><td align="right">0.571s (+1.4%)</td><td align="right">6.2G (+0.02%)</td><td align="right">476</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d -c` planted</td><td align="right">6.820929855</td><td align="right">0.404s</td><td align="right">4.3G</td><td align="right">50</td><td align="right">2</td><td align="right">6.820929855 (=)</td><td align="right">0.420s (+4.2%)</td><td align="right">4.3G (+0.07%)</td><td align="right">50</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d -c` planted</td><td align="right">6.856285862</td><td align="right">0.720s</td><td align="right">7.4G</td><td align="right">129</td><td align="right">2</td><td align="right">6.856285862 (=)</td><td align="right">0.688s (-4.5%)</td><td align="right">7.3G (-0.04%)</td><td align="right">129</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d -c` planted</td><td align="right">6.857876353</td><td align="right">0.931s</td><td align="right">9.5G</td><td align="right">287</td><td align="right">2</td><td align="right">6.857876353 (=)</td><td align="right">0.935s (+0.4%)</td><td align="right">9.5G (+0.02%)</td><td align="right">287</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d -c` planted</td><td align="right">6.873464257</td><td align="right">1.12s</td><td align="right">11.3G</td><td align="right">444</td><td align="right">2</td><td align="right">6.873464257 (=)</td><td align="right">1.13s (+1.2%)</td><td align="right">11.3G (-0.00%)</td><td align="right">444</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d -c` planted</td><td align="right">6.881554086</td><td align="right">0.984s</td><td align="right">9.6G</td><td align="right">624</td><td align="right">2</td><td align="right">6.881554086 (=)</td><td align="right">0.982s (-0.2%)</td><td align="right">9.6G (+0.00%)</td><td align="right">624</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d -c` planted</td><td align="right">6.875540042</td><td align="right">1.22s</td><td align="right">11.8G</td><td align="right">885</td><td align="right">2</td><td align="right">6.875540042 (=)</td><td align="right">1.22s (-0.2%)</td><td align="right">11.8G (-0.05%)</td><td align="right">885</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">6.066305904</td><td align="right">0.072s</td><td align="right">0.8G</td><td align="right">187</td><td align="right">3</td><td align="right">6.066305904 (=)</td><td align="right">0.072s (-0.2%)</td><td align="right">0.8G (+0.00%)</td><td align="right">187</td><td align="right">3</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.092s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.094s (+2.0%)</td><td align="right">1.0G (-0.00%)</td><td align="right">184</td><td align="right">2</td></tr>
</tbody>
</table>

### The overlapping family in full

Every configuration of the planted overlapping state networks, both arms, at both trigram densities
(F55); the E50000 `--regularized -N1` rows are new in this snapshot. Change (2) moves every E50000
`--regularized` row whose repair starts from one module: `-2d -N1`, `-2d -N10` and `-d -N10` on om3–om8,
−0.075% to −0.245% in bits for −5.8% to −49% in seconds, from 7–49 modules to 77–112. Change (1) takes
−6% to −13.5% in seconds off every rescued `-d --regularized -N1` row, bit-identical; on E50000 those
rows still return one module (the #1122 table above has what the one-liner would buy).

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
<tr><td align="right">om2 E100000 `-2d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">3.84s</td><td align="right">36.8G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.80s (-1.1%)</td><td align="right">36.8G (-0.03%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 E100000 `-d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">3.63s</td><td align="right">33.5G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.59s (-1.2%)</td><td align="right">33.5G (+0.02%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.74s</td><td align="right">24.8G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.68s (-2.2%)</td><td align="right">24.8G (+0.00%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N1`</td><td align="right">7.548863183</td><td align="right">0.707s</td><td align="right">7.2G</td><td align="right">121</td><td align="right">2</td><td align="right">7.548863183 (=)</td><td align="right">0.717s (+1.4%)</td><td align="right">7.2G (+0.03%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.54s</td><td align="right">23.8G</td><td align="right">126</td><td align="right">2</td><td align="right">7.548721547 (=)</td><td align="right">2.48s (-2.3%)</td><td align="right">23.8G (-0.03%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N1`</td><td align="right">7.548863183</td><td align="right">0.909s</td><td align="right">8.8G</td><td align="right">121</td><td align="right">2</td><td align="right">7.548863183 (=)</td><td align="right">0.840s (-7.6%)</td><td align="right">8.5G (-3.40%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om2 planted, `-2d --no-infomap -c`</td><td align="right">6.789039995</td><td align="right">0.052s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.789039995 (=)</td><td align="right">0.049s (-4.9%)</td><td align="right">0.6G (-0.00%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N10`</td><td align="right">7.931196935</td><td align="right">3.16s</td><td align="right">27.9G</td><td align="right">49</td><td align="right">2</td><td align="right">7.925216272 (-0.0754%)</td><td align="right">2.51s (-20.6%)</td><td align="right">22.5G (-19.32%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N10`</td><td align="right">7.931196935</td><td align="right">3.22s</td><td align="right">28.1G</td><td align="right">49</td><td align="right">2</td><td align="right">7.925216272 (-0.0754%)</td><td align="right">2.57s (-20.2%)</td><td align="right">22.7G (-19.19%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">5.06s</td><td align="right">48.0G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">5.23s (+3.2%)</td><td align="right">48.0G (+0.00%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">0.913s</td><td align="right">9.2G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">0.911s (-0.2%)</td><td align="right">9.2G (-0.02%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">4.85s</td><td align="right">45.0G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">4.66s (-3.9%)</td><td align="right">45.0G (-0.01%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">1.24s</td><td align="right">12.5G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">1.17s (-6.1%)</td><td align="right">12.0G (-4.38%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 planted, `-2d --no-infomap -c`</td><td align="right">6.837980937</td><td align="right">0.097s</td><td align="right">0.9G</td><td align="right">12</td><td align="right">2</td><td align="right">6.837980937 (=)</td><td align="right">0.097s (+0.3%)</td><td align="right">0.9G (-0.14%)</td><td align="right">12</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N10`</td><td align="right">7.940858042</td><td align="right">3.19s</td><td align="right">27.4G</td><td align="right">41</td><td align="right">2</td><td align="right">7.92621405 (-0.1844%)</td><td align="right">2.86s (-10.3%)</td><td align="right">23.0G (-15.94%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N10`</td><td align="right">7.940858042</td><td align="right">3.27s</td><td align="right">27.2G</td><td align="right">41</td><td align="right">2</td><td align="right">7.92621405 (-0.1844%)</td><td align="right">2.79s (-14.7%)</td><td align="right">22.9G (-16.10%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N10`</td><td align="right">7.556894677</td><td align="right">5.57s</td><td align="right">49.0G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">5.61s (+0.7%)</td><td align="right">49.0G (-0.00%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">0.977s</td><td align="right">9.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">0.971s (-0.6%)</td><td align="right">9.1G (-0.00%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N10`</td><td align="right">7.556653713</td><td align="right">5.35s</td><td align="right">48.6G</td><td align="right">78</td><td align="right">2</td><td align="right">7.556653713 (=)</td><td align="right">5.40s (+1.0%)</td><td align="right">48.6G (-0.01%)</td><td align="right">78</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">1.35s</td><td align="right">12.3G</td><td align="right">79</td><td align="right">2</td><td align="right">7.556894677 (=)</td><td align="right">1.21s (-10.0%)</td><td align="right">11.7G (-4.72%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om4 planted, `-2d --no-infomap -c`</td><td align="right">6.880650147</td><td align="right">0.110s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.880650147 (=)</td><td align="right">0.111s (+0.8%)</td><td align="right">1.0G (-0.15%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N10`</td><td align="right">7.969030601</td><td align="right">3.60s</td><td align="right">29.0G</td><td align="right">26</td><td align="right">2</td><td align="right">7.956672505 (-0.1551%)</td><td align="right">2.83s (-21.2%)</td><td align="right">23.3G (-19.68%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N10`</td><td align="right">7.969030601</td><td align="right">3.59s</td><td align="right">28.3G</td><td align="right">26</td><td align="right">2</td><td align="right">7.956672505 (-0.1551%)</td><td align="right">2.98s (-17.0%)</td><td align="right">22.6G (-20.18%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N10`</td><td align="right">7.968061942</td><td align="right">5.08s</td><td align="right">43.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.968061942 (=)</td><td align="right">5.12s (+0.8%)</td><td align="right">43.1G (+0.00%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N1`</td><td align="right">7.968629202</td><td align="right">0.880s</td><td align="right">7.8G</td><td align="right">79</td><td align="right">2</td><td align="right">7.968629202 (=)</td><td align="right">0.886s (+0.7%)</td><td align="right">7.8G (-0.02%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N10`</td><td align="right">7.968320007</td><td align="right">4.85s</td><td align="right">42.5G</td><td align="right">81</td><td align="right">2</td><td align="right">7.968320007 (=)</td><td align="right">4.85s (+0.0%)</td><td align="right">42.5G (-0.01%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N1`</td><td align="right">7.968629202</td><td align="right">1.26s</td><td align="right">11.0G</td><td align="right">79</td><td align="right">2</td><td align="right">7.968629202 (=)</td><td align="right">1.14s (-9.4%)</td><td align="right">10.5G (-5.28%)</td><td align="right">79</td><td align="right">2</td></tr>
<tr><td align="right">om5 planted, `-2d --no-infomap -c`</td><td align="right">6.902222527</td><td align="right">0.128s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.902222527 (=)</td><td align="right">0.128s (+0.3%)</td><td align="right">1.1G (-0.06%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N10`</td><td align="right">7.960481009</td><td align="right">3.24s</td><td align="right">24.7G</td><td align="right">23</td><td align="right">2</td><td align="right">7.944047825 (-0.2064%)</td><td align="right">2.93s (-9.6%)</td><td align="right">21.9G (-11.62%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N10`</td><td align="right">7.960481009</td><td align="right">3.21s</td><td align="right">24.4G</td><td align="right">23</td><td align="right">2</td><td align="right">7.944047825 (-0.2064%)</td><td align="right">2.78s (-13.2%)</td><td align="right">21.5G (-11.75%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N10`</td><td align="right">7.982650944</td><td align="right">5.03s</td><td align="right">41.2G</td><td align="right">105</td><td align="right">2</td><td align="right">7.982650944 (=)</td><td align="right">5.09s (+1.1%)</td><td align="right">41.2G (-0.04%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N1`</td><td align="right">7.982650944</td><td align="right">0.746s</td><td align="right">6.0G</td><td align="right">105</td><td align="right">2</td><td align="right">7.982650944 (=)</td><td align="right">0.717s (-3.9%)</td><td align="right">6.0G (-0.04%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N10`</td><td align="right">7.984417755</td><td align="right">4.89s</td><td align="right">41.2G</td><td align="right">102</td><td align="right">2</td><td align="right">7.984417755 (=)</td><td align="right">4.93s (+0.9%)</td><td align="right">41.2G (+0.00%)</td><td align="right">102</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N1`</td><td align="right">7.982650944</td><td align="right">1.12s</td><td align="right">9.3G</td><td align="right">105</td><td align="right">2</td><td align="right">7.982650944 (=)</td><td align="right">0.980s (-12.4%)</td><td align="right">8.7G (-6.57%)</td><td align="right">105</td><td align="right">2</td></tr>
<tr><td align="right">om6 planted, `-2d --no-infomap -c`</td><td align="right">6.930934993</td><td align="right">0.133s</td><td align="right">1.1G</td><td align="right">24</td><td align="right">2</td><td align="right">6.930934993 (=)</td><td align="right">0.135s (+1.7%)</td><td align="right">1.1G (+0.08%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N10`</td><td align="right">7.977053735</td><td align="right">3.19s</td><td align="right">25.4G</td><td align="right">21</td><td align="right">2</td><td align="right">7.957532546 (-0.2447%)</td><td align="right">2.93s (-8.3%)</td><td align="right">23.3G (-8.24%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N10`</td><td align="right">7.977053735</td><td align="right">3.30s</td><td align="right">24.4G</td><td align="right">21</td><td align="right">2</td><td align="right">7.957532546 (-0.2447%)</td><td align="right">2.91s (-11.8%)</td><td align="right">22.3G (-8.66%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N10`</td><td align="right">7.948827768</td><td align="right">5.39s</td><td align="right">41.2G</td><td align="right">166</td><td align="right">2</td><td align="right">7.948827768 (=)</td><td align="right">5.14s (-4.7%)</td><td align="right">41.2G (-0.01%)</td><td align="right">166</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N1`</td><td align="right">7.950842444</td><td align="right">0.766s</td><td align="right">6.4G</td><td align="right">168</td><td align="right">2</td><td align="right">7.950842444 (=)</td><td align="right">0.772s (+0.8%)</td><td align="right">6.4G (+0.01%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N10`</td><td align="right">7.948827768</td><td align="right">5.08s</td><td align="right">41.1G</td><td align="right">166</td><td align="right">2</td><td align="right">7.948827768 (=)</td><td align="right">5.09s (+0.1%)</td><td align="right">41.1G (+0.03%)</td><td align="right">166</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N1`</td><td align="right">7.950842444</td><td align="right">1.16s</td><td align="right">9.7G</td><td align="right">168</td><td align="right">2</td><td align="right">7.950842444 (=)</td><td align="right">1.03s (-11.3%)</td><td align="right">9.1G (-6.32%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">om7 planted, `-2d --no-infomap -c`</td><td align="right">6.957516072</td><td align="right">0.156s</td><td align="right">1.3G</td><td align="right">28</td><td align="right">2</td><td align="right">6.957516072 (=)</td><td align="right">0.159s (+1.8%)</td><td align="right">1.3G (+0.17%)</td><td align="right">28</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N10`</td><td align="right">7.991103173</td><td align="right">3.10s</td><td align="right">23.9G</td><td align="right">7</td><td align="right">2</td><td align="right">7.978376075 (-0.1593%)</td><td align="right">2.86s (-7.7%)</td><td align="right">22.6G (-5.52%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N10`</td><td align="right">7.991103173</td><td align="right">3.07s</td><td align="right">23.1G</td><td align="right">7</td><td align="right">2</td><td align="right">7.978376075 (-0.1593%)</td><td align="right">2.89s (-5.8%)</td><td align="right">21.7G (-5.74%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N10`</td><td align="right">7.979831947</td><td align="right">5.18s</td><td align="right">40.7G</td><td align="right">221</td><td align="right">2</td><td align="right">7.979831947 (=)</td><td align="right">5.25s (+1.2%)</td><td align="right">40.7G (-0.00%)</td><td align="right">221</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N1`</td><td align="right">7.983599366</td><td align="right">0.751s</td><td align="right">6.2G</td><td align="right">216</td><td align="right">2</td><td align="right">7.983599366 (=)</td><td align="right">0.756s (+0.7%)</td><td align="right">6.2G (+0.00%)</td><td align="right">216</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N10`</td><td align="right">7.978630877</td><td align="right">5.15s</td><td align="right">41.3G</td><td align="right">236</td><td align="right">2</td><td align="right">7.978630877 (=)</td><td align="right">5.08s (-1.5%)</td><td align="right">41.3G (+0.00%)</td><td align="right">236</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N1`</td><td align="right">7.983599366</td><td align="right">1.19s</td><td align="right">9.5G</td><td align="right">216</td><td align="right">2</td><td align="right">7.983599366 (=)</td><td align="right">1.04s (-12.0%)</td><td align="right">8.9G (-6.47%)</td><td align="right">216</td><td align="right">2</td></tr>
<tr><td align="right">om8 planted, `-2d --no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.149s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.98103476 (=)</td><td align="right">0.148s (-0.6%)</td><td align="right">1.2G (-0.25%)</td><td align="right">32</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N1`</td><td align="right">7.931196935</td><td align="right">1.19s</td><td align="right">10.7G</td><td align="right">49</td><td align="right">2</td><td align="right">7.925216272 (-0.0754%)</td><td align="right">0.601s (-49.3%)</td><td align="right">5.3G (-50.42%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N1`</td><td align="right">7.969664223</td><td align="right">0.467s</td><td align="right">4.2G</td><td align="right">1</td><td align="right">2</td><td align="right">7.969664223 (=)</td><td align="right">0.408s (-12.5%)</td><td align="right">3.9G (-7.85%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N1`</td><td align="right">7.940858042</td><td align="right">1.22s</td><td align="right">10.6G</td><td align="right">41</td><td align="right">2</td><td align="right">7.92621405 (-0.1844%)</td><td align="right">0.748s (-38.6%)</td><td align="right">6.2G (-41.34%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N1`</td><td align="right">7.978790193</td><td align="right">0.507s</td><td align="right">4.5G</td><td align="right">1</td><td align="right">2</td><td align="right">7.978790193 (=)</td><td align="right">0.453s (-10.6%)</td><td align="right">4.1G (-7.90%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N1`</td><td align="right">7.969030601</td><td align="right">1.39s</td><td align="right">12.0G</td><td align="right">26</td><td align="right">2</td><td align="right">7.956672505 (-0.1551%)</td><td align="right">0.762s (-45.4%)</td><td align="right">6.3G (-47.53%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N1`</td><td align="right">7.98862707</td><td align="right">0.535s</td><td align="right">4.6G</td><td align="right">1</td><td align="right">2</td><td align="right">7.98862707 (=)</td><td align="right">0.480s (-10.4%)</td><td align="right">4.2G (-7.64%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N1`</td><td align="right">7.960481009</td><td align="right">0.956s</td><td align="right">8.0G</td><td align="right">23</td><td align="right">2</td><td align="right">7.944047825 (-0.2064%)</td><td align="right">0.626s (-34.5%)</td><td align="right">5.2G (-35.69%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N1`</td><td align="right">7.989209626</td><td align="right">0.472s</td><td align="right">3.7G</td><td align="right">1</td><td align="right">2</td><td align="right">7.989209626 (=)</td><td align="right">0.408s (-13.5%)</td><td align="right">3.3G (-9.97%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N1`</td><td align="right">7.977053735</td><td align="right">0.934s</td><td align="right">7.9G</td><td align="right">21</td><td align="right">2</td><td align="right">7.957532546 (-0.2447%)</td><td align="right">0.692s (-26.0%)</td><td align="right">5.8G (-26.61%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N1`</td><td align="right">7.992344954</td><td align="right">0.431s</td><td align="right">3.5G</td><td align="right">1</td><td align="right">2</td><td align="right">7.992344954 (=)</td><td align="right">0.380s (-11.7%)</td><td align="right">3.1G (-10.49%)</td><td align="right">1</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N1`</td><td align="right">7.991103173</td><td align="right">0.783s</td><td align="right">6.5G</td><td align="right">7</td><td align="right">2</td><td align="right">7.978376075 (-0.1593%)</td><td align="right">0.641s (-18.2%)</td><td align="right">5.2G (-20.37%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N1`</td><td align="right">7.994534803</td><td align="right">0.448s</td><td align="right">3.5G</td><td align="right">1</td><td align="right">2</td><td align="right">7.994534803 (=)</td><td align="right">0.391s (-12.7%)</td><td align="right">3.1G (-10.62%)</td><td align="right">1</td><td align="right">2</td></tr>
</tbody>
</table>

### `-F` on the family, old vs new

`optimizeFlexible` hands a collapsed lone trial to the same rescue, so change (1) applies: om3 / om4
`-F -d -N1` bit-identical, −3.4% / −4.2% in seconds. `-N10`:

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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.14s</td><td align="right">40.3G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">4.19s (+1.3%)</td><td align="right">40.3G (-0.04%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.26s</td><td align="right">55.5G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.17s (-1.7%)</td><td align="right">55.5G (-0.03%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">5.90s</td><td align="right">56.0G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.86s (-0.6%)</td><td align="right">56.0G (+0.01%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">5.97s</td><td align="right">54.2G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">6.03s (+1.0%)</td><td align="right">54.2G (-0.03%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">7.35s</td><td align="right">66.6G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">7.44s (+1.2%)</td><td align="right">66.6G (+0.00%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">8.02s</td><td align="right">69.7G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">7.94s (-0.9%)</td><td align="right">69.7G (-0.00%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">8.74s</td><td align="right">74.8G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">8.82s (+0.9%)</td><td align="right">74.8G (-0.01%)</td><td align="right">919</td><td align="right">2</td></tr>
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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.321678354</td><td align="right">0.263s</td><td align="right">2.6G</td><td align="right">436</td><td align="right">4</td><td align="right">7.321678354 (=)</td><td align="right">0.276s (+5.1%)</td><td align="right">2.6G (+0.04%)</td><td align="right">436</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.823562921</td><td align="right">1.51s</td><td align="right">17.1G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.46s (-3.4%)</td><td align="right">16.6G (-3.03%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.861732911</td><td align="right">1.83s</td><td align="right">19.0G</td><td align="right">139</td><td align="right">2</td><td align="right">6.861732911 (=)</td><td align="right">1.75s (-4.2%)</td><td align="right">18.4G (-2.74%)</td><td align="right">139</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.798283664</td><td align="right">0.667s</td><td align="right">5.8G</td><td align="right">96</td><td align="right">4</td><td align="right">7.798283664 (=)</td><td align="right">0.664s (-0.4%)</td><td align="right">5.8G (+0.04%)</td><td align="right">96</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.507066407</td><td align="right">0.689s</td><td align="right">5.8G</td><td align="right">87</td><td align="right">4</td><td align="right">7.507066407 (=)</td><td align="right">0.702s (+1.8%)</td><td align="right">5.8G (+0.06%)</td><td align="right">87</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.238675817</td><td align="right">0.613s</td><td align="right">5.2G</td><td align="right">145</td><td align="right">4</td><td align="right">7.238675817 (=)</td><td align="right">0.614s (+0.1%)</td><td align="right">5.2G (+0.03%)</td><td align="right">145</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">7.022972054</td><td align="right">0.612s</td><td align="right">5.2G</td><td align="right">201</td><td align="right">4</td><td align="right">7.022972054 (=)</td><td align="right">0.626s (+2.2%)</td><td align="right">5.2G (+0.13%)</td><td align="right">201</td><td align="right">4</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-7.4%)</td><td align="right">0.1G (-1.97%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (-1.8%)</td><td align="right">0.1G (-2.09%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">4.03324474 (-0.3856%)</td><td align="right">0.015s (-35.6%)</td><td align="right">0.2G (-28.88%)</td><td align="right">9</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.237s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.754013143 (+0.7684%)</td><td align="right">0.150s (-36.7%)</td><td align="right">1.7G (-37.68%)</td><td align="right">10</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.059s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.740943136 (=)</td><td align="right">0.070s (+19.7%)</td><td align="right">0.7G (-4.15%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.03s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">7.807937174 (=)</td><td align="right">2.97s (-2.0%)</td><td align="right">33.1G (-2.99%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">19.9s</td><td align="right">181.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.620539396 (+1.1539%)</td><td align="right">15.3s (-23.4%)</td><td align="right">126.7G (-30.27%)</td><td align="right">135</td><td align="right">5</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.013s (+230.6%)</td><td align="right">0.1G (-0.71%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+27.5%)</td><td align="right">0.1G (+0.41%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.67s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.73s (+2.3%)</td><td align="right">30.6G (-2.59%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.50s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.392285003 (=)</td><td align="right">3.22s (-8.0%)</td><td align="right">35.9G (-6.96%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.90s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.574537176 (=)</td><td align="right">3.39s (-13.1%)</td><td align="right">35.2G (-12.73%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.1s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.421664324 (=)</td><td align="right">9.68s (-4.4%)</td><td align="right">95.4G (-2.61%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.36s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (=)</td><td align="right">3.27s (-2.7%)</td><td align="right">34.7G (-2.22%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.70s</td><td align="right">47.5G</td><td align="right">690</td><td align="right">2</td><td align="right">6.731808656 (=)</td><td align="right">4.19s (-10.7%)</td><td align="right">40.3G (-15.24%)</td><td align="right">690</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.63s</td><td align="right">59.8G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.17s (-8.2%)</td><td align="right">55.5G (-7.29%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.29s</td><td align="right">60.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.866901786 (=)</td><td align="right">5.86s (-6.9%)</td><td align="right">56.0G (-7.65%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">6.32s</td><td align="right">59.0G</td><td align="right">303</td><td align="right">2</td><td align="right">6.867301407 (=)</td><td align="right">6.03s (-4.6%)</td><td align="right">54.2G (-8.00%)</td><td align="right">303</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">8.71s</td><td align="right">81.2G</td><td align="right">448</td><td align="right">2</td><td align="right">6.88496897 (=)</td><td align="right">7.44s (-14.6%)</td><td align="right">66.6G (-17.98%)</td><td align="right">448</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">9.01s</td><td align="right">82.7G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">7.94s (-11.9%)</td><td align="right">69.7G (-15.69%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">9.78s</td><td align="right">86.4G</td><td align="right">919</td><td align="right">2</td><td align="right">6.88742315 (=)</td><td align="right">8.82s (-9.8%)</td><td align="right">74.8G (-13.52%)</td><td align="right">919</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.711s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.704s (-1.0%)</td><td align="right">6.9G (-4.02%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.078067323 (-8.7135%)</td><td align="right">0.001s (+8.9%)</td><td align="right">0.1G (+0.64%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.868228367 (+0.0797%)</td><td align="right">0.007s (+4.1%)</td><td align="right">0.1G (+0.09%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.048857953</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">15</td><td align="right">4</td><td align="right">3.892209764 (-3.8689%)</td><td align="right">0.023s (-0.3%)</td><td align="right">0.3G (+0.98%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.717760238</td><td align="right">0.237s</td><td align="right">2.8G</td><td align="right">5</td><td align="right">5</td><td align="right">4.509265423 (-4.4194%)</td><td align="right">0.264s (+11.2%)</td><td align="right">2.9G (+3.34%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.740943136</td><td align="right">0.059s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">6.789241502 (+0.7165%)</td><td align="right">0.061s (+4.2%)</td><td align="right">0.7G (+1.82%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">science2001</td><td align="right">7.807937174</td><td align="right">3.03s</td><td align="right">34.1G</td><td align="right">189</td><td align="right">3</td><td align="right">8.009172258 (+2.5773%)</td><td align="right">2.84s (-6.1%)</td><td align="right">31.2G (-8.70%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">19.9s</td><td align="right">181.8G</td><td align="right">5</td><td align="right">6</td><td align="right">5.512433077 (-0.7917%)</td><td align="right">20.4s (+2.2%)</td><td align="right">183.0G (+0.65%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653 (-0.8182%)</td><td align="right">0.004s (-1.1%)</td><td align="right">0.1G (+0.07%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (-4.1040%)</td><td align="right">0.001s (+4.4%)</td><td align="right">0.1G (+0.80%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.67s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.438064454 (+0.6171%)</td><td align="right">2.95s (+10.6%)</td><td align="right">33.9G (+7.78%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.50s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.378824196 (-0.2496%)</td><td align="right">3.62s (+3.5%)</td><td align="right">38.4G (-0.38%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.90s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.56659036 (-0.1426%)</td><td align="right">3.86s (-1.1%)</td><td align="right">40.1G (-0.62%)</td><td align="right">220</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.1s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.192724425 (-3.0848%)</td><td align="right">8.76s (-13.4%)</td><td align="right">85.9G (-12.24%)</td><td align="right">1910</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.235585529</td><td align="right">3.36s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">8.447745451 (+2.5761%)</td><td align="right">3.39s (+0.9%)</td><td align="right">35.4G (-0.41%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.711s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.903208727 (-0.0795%)</td><td align="right">0.733s (+3.1%)</td><td align="right">6.9G (-4.37%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (-79.8%)</td><td align="right">0.1G (-30.00%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.022s</td><td align="right">0.3G</td><td align="right">5</td><td align="right">2</td><td align="right">6.862755928 (-0.0042%)</td><td align="right">0.007s (-70.3%)</td><td align="right">0.1G (-54.90%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.026557116</td><td align="right">0.119s</td><td align="right">1.3G</td><td align="right">12</td><td align="right">5</td><td align="right">4.048857953 (+0.5538%)</td><td align="right">0.023s (-80.9%)</td><td align="right">0.3G (-76.44%)</td><td align="right">15</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.736412597</td><td align="right">1.89s</td><td align="right">20.2G</td><td align="right">11</td><td align="right">6</td><td align="right">4.717760238 (-0.3938%)</td><td align="right">0.237s (-87.4%)</td><td align="right">2.8G (-86.11%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.738927979</td><td align="right">0.136s</td><td align="right">1.4G</td><td align="right">80</td><td align="right">3</td><td align="right">6.740943136 (+0.0299%)</td><td align="right">0.059s (-57.0%)</td><td align="right">0.7G (-49.70%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.805465772</td><td align="right">7.94s</td><td align="right">63.0G</td><td align="right">220</td><td align="right">4</td><td align="right">7.807937174 (+0.0317%)</td><td align="right">3.03s (-61.9%)</td><td align="right">34.1G (-45.81%)</td><td align="right">189</td><td align="right">3</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.55442136</td><td align="right">155.7s</td><td align="right">1115.4G</td><td align="right">766</td><td align="right">9</td><td align="right">5.556421705 (+0.0360%)</td><td align="right">19.9s (-87.2%)</td><td align="right">181.8G (-83.70%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-67.5%)</td><td align="right">0.1G (-49.47%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-50.7%)</td><td align="right">0.1G (-37.09%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502028585</td><td align="right">9.20s</td><td align="right">78.1G</td><td align="right">144</td><td align="right">3</td><td align="right">7.392442593 (-1.4608%)</td><td align="right">2.67s (-71.0%)</td><td align="right">31.5G (-59.72%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392441014</td><td align="right">11.9s</td><td align="right">120.6G</td><td align="right">251</td><td align="right">3</td><td align="right">5.392285003 (-0.0029%)</td><td align="right">3.50s (-70.6%)</td><td align="right">38.6G (-68.02%)</td><td align="right">257</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578435633</td><td align="right">7.76s</td><td align="right">81.7G</td><td align="right">301</td><td align="right">3</td><td align="right">5.574537176 (-0.0699%)</td><td align="right">3.90s (-49.7%)</td><td align="right">40.4G (-50.58%)</td><td align="right">228</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">8.432467909</td><td align="right">5.55s</td><td align="right">52.1G</td><td align="right">114</td><td align="right">4</td><td align="right">7.421664324 (-11.9870%)</td><td align="right">10.1s (+82.4%)</td><td align="right">97.9G (+87.97%)</td><td align="right">2135</td><td align="right">3</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">7.938575228</td><td align="right">6.93s</td><td align="right">57.0G</td><td align="right">25</td><td align="right">4</td><td align="right">8.235585529 (+3.7414%)</td><td align="right">3.36s (-51.5%)</td><td align="right">35.5G (-37.69%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.88554258</td><td align="right">2.23s</td><td align="right">22.4G</td><td align="right">184</td><td align="right">3</td><td align="right">5.907904741 (+0.3800%)</td><td align="right">0.711s (-68.1%)</td><td align="right">7.2G (-67.95%)</td><td align="right">199</td><td align="right">2</td></tr>
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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.001s (-49.7%)</td><td align="right">0.1G (-36.93%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.863047469</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">5</td><td align="right">2</td><td align="right">6.861229775 (-0.0265%)</td><td align="right">0.007s (-44.9%)</td><td align="right">0.1G (-32.99%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscicoauthor2010</td><td align="right">4.285012668</td><td align="right">0.031s</td><td align="right">0.4G</td><td align="right">56</td><td align="right">2</td><td align="right">4.283072584 (-0.0453%)</td><td align="right">0.009s (-70.1%)</td><td align="right">0.2G (-60.69%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.600443859</td><td align="right">0.698s</td><td align="right">5.9G</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.098s (-86.0%)</td><td align="right">1.1G (-80.54%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">politicalblogs</td><td align="right">6.739721413</td><td align="right">0.168s</td><td align="right">0.8G</td><td align="right">80</td><td align="right">2</td><td align="right">6.739575295 (-0.0022%)</td><td align="right">0.043s (-74.4%)</td><td align="right">0.5G (-35.24%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td align="right">science2001</td><td align="right">7.9500396</td><td align="right">4.41s</td><td align="right">29.3G</td><td align="right">496</td><td align="right">2</td><td align="right">7.949978834 (-0.0008%)</td><td align="right">2.38s (-45.9%)</td><td align="right">23.9G (-18.59%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.742988533</td><td align="right">45.2s</td><td align="right">251.8G</td><td align="right">11809</td><td align="right">2</td><td align="right">6.754216663 (+0.1665%)</td><td align="right">19.4s (-57.1%)</td><td align="right">117.5G (-53.32%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.003s (-52.6%)</td><td align="right">0.1G (-2.86%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-40.2%)</td><td align="right">0.1G (-38.50%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.50595639</td><td align="right">6.58s</td><td align="right">55.2G</td><td align="right">142</td><td align="right">2</td><td align="right">7.400445378 (-1.4057%)</td><td align="right">2.79s (-57.6%)</td><td align="right">32.3G (-41.52%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393312779</td><td align="right">4.70s</td><td align="right">43.9G</td><td align="right">332</td><td align="right">2</td><td align="right">5.393055049 (-0.0048%)</td><td align="right">3.89s (-17.3%)</td><td align="right">41.7G (-4.91%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.579216889</td><td align="right">5.84s</td><td align="right">58.6G</td><td align="right">301</td><td align="right">2</td><td align="right">5.571539329 (-0.1376%)</td><td align="right">3.93s (-32.7%)</td><td align="right">41.2G (-29.65%)</td><td align="right">304</td><td align="right">2</td></tr>
<tr><td align="right">science2001 (pref.)</td><td align="right">8.131110023</td><td align="right">5.04s</td><td align="right">36.7G</td><td align="right">25</td><td align="right">2</td><td align="right">8.235585529 (+1.2849%)</td><td align="right">3.07s (-39.1%)</td><td align="right">31.6G (-14.00%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.892212121</td><td align="right">1.70s</td><td align="right">17.2G</td><td align="right">184</td><td align="right">2</td><td align="right">5.907904741 (+0.2663%)</td><td align="right">0.673s (-60.4%)</td><td align="right">6.9G (-59.98%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>
