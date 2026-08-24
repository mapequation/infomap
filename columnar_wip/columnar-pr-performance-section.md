## Performance

> Manual benchmark comparing the two engines on the same binary. This is **not** the CI `perf-pr.yml` check (which only sees the default OO path, since the new core is flag-gated) — it's included here so reviewers can see how the opt-in `--columnar` engine compares to the default.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123 -N10` — **best of 10 trials**, the way Infomap is normally run. Codelength in bits; `time` = `--timing-json`'s `timing.total_s` (the engine's own wall — process wall carries ~30 ms of startup that swamps the sub-0.1 s rows), minimum of 3 interleaved repetitions; `top`/`lvls` = top modules / levels of the best partition. See [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md) for paths and full details.

> **This is the snapshot for the master sync that brings #1033 into the branch.** Master now charges the
> entropy-bias correction in bits rather than nats (Miller–Madow's `(K−1)/(2n)` is a nats expression) and
> stops charging codewords that cannot exist; the columnar copy of that term moves with it in this PR so
> the two engines keep computing the same formula. The change is reachable **only through
> `--entropy-corrected`**, which no benchmark row below uses, so the claim this snapshot has to support is
> a negative one: nothing else moved.
>
> It doesn't. All **76** paired columnar configurations are **bit-identical in bits** between the two
> arms — at `-N10`, at `-N1`, two-level, `-F`, `--non-redundant`, and on the overlapping/`-c` rows — and
> every codelength is identical across all three repetitions of both arms. Times: **median +0.29 %, mean
> +0.53 %** over the 76 pairs. Five rows read above +5 % in the 3-rep batch and none survived
> re-measurement with 7–10 interleaved repetitions: air30k `-C -2 -N10` +5.1 % → **−0.9 %**, om5
> `-C -2d -N10` +3.7 % → **−3.1 %**, politicalblogs `-C --non-redundant` +5.1 % → **−0.3 %**, air30k
> `-C -N1` **−0.1 %**; powergrid `-C -2 -N10` settles at **+0.5 %** (min of 10) / +1.3 % (median), inside
> its own ±0.006 s spread on a 0.106 s row. With bit-identical output and a code path that cannot execute
> without the flag, the residue is binary layout, not work.
>
> **Old** = a fresh `MODE=release OPENMP=0` build of the `columnar-hierarchical-core` tip `6a9608a6`,
> md5 `50ef742d84a04dfff2471ba5e895c676`; **new** = this PR (`a102a7a9`), md5
> `45d8f427c293fa9f0a4b8e3e64eecb61`. OO rows measured on the new binary (the OO path's only change is
> the correction itself). One session, one instrument, desktop load ~3–5 of 10 cores.

### What the change does move: `--entropy-corrected`

The correction is now `(N + m − 1) / (2 D ln 2)` bits, with `D = Σ k_α` the total degree, and a codebook
that cannot produce an event does not charge for it (a module holding the whole network has no exit
codeword; a single module's index codebook has nothing to estimate). Exact on the two-triangle example:
one module of six nodes costs `H + 5/(28 ln 2) = 2.814280822` bits, tracked and reported agreeing; two
modules cost `L + 7/(28 ln 2) = 2.681404117`.

On real networks the reported codelength therefore rises, most on sparse ones — this is the corrected
estimate, not a worse partition: **the partitions are unchanged** (top-module counts identical on every
row below).

| network | flags | old bits | new bits | Δbits | old t | new t | Δt | top |
|---|---|--:|--:|--:|--:|--:|--:|--:|
| jazz | `-C -N10 --entropy-corrected` | 6.88135549 | 6.88945789 | +0.118% | 0.008s | 0.008s | +0.2% | 6 → 6 |
| powergrid | `-C -N10 --entropy-corrected` | 4.97131925 | 5.07319384 | +2.049% | 0.243s | 0.242s | −0.7% | 5 → 5 |
| science2001 | `-C -N10 --entropy-corrected` | 7.83373299 | 7.83386414 | +0.002% | 2.93s | 2.90s | −0.8% | 15 → 15 |
| air30k | `-C -N10 --entropy-corrected` | 5.39284340 | 5.39290575 | +0.001% | 3.72s | 3.68s | −1.2% | 18 → 18 |
| jazz | `-N10 --entropy-corrected` (OO) | 6.88148446 | 6.88963766 | +0.119% | 0.022s | 0.022s | +1.9% | 5 → 5 |
| powergrid | `-N10 --entropy-corrected` (OO) | 6.96191568 | 7.56924857 | +8.724% | 0.520s | 0.506s | −2.7% | 37 → 37 |

The size of the term tracks the sample: powergrid has 4941 nodes over D = 13188 walk steps, so the
correction is worth 0.33 bits there and 0.0001 bits on air30k, which is exactly what it is for.

> ⚠️ **The last row is not the correction being large — it is #1034, filed from this session.** On a
> *hierarchical* OO run the corrected search returns a partition ~2.5 bits worse than one the same
> binary scores: powergrid `-N10 --entropy-corrected` gives 7.569248565 (37 modules, 2 levels), while the
> OO engine's own uncorrected 7-level partition scores **5.086858089** under the same corrected
> objective. The correction on this network cannot exceed 0.33 bits, so the gap is the *search* being
> steered by something other than the reported objective — most likely the level-wise correction each
> sub/super instance carries with the whole network's node count. It is pre-existing (the same shape on
> the old tip: 6.961915684 against 4.986208728, a 1.98-bit gap) and charging in bits scales it by 1/ln 2,
> to 2.48 bits. **The columnar engine does not show it**: `-C -N10 --entropy-corrected` returns 5 top
> modules over 5 levels at 5.073193845, i.e. the uncorrected `-C` partition plus 0.33 bits, and within
> 0.014 bits of what OO scores for its own uncorrected tree. Two-level OO is also unaffected
> (5.924258027 against 5.600443859 uncorrected). Tracked in #1034, not fixed here.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.002s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (-12.1%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.007s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86275593 (=)</td><td align="right">0.007s (+1.0%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.025s</td><td align="right">2</td><td align="right">4</td><td align="right">4.05454025 (=)</td><td align="right">0.024s (-2.0%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.243s</td><td align="right">5</td><td align="right">5</td><td align="right">4.74107206 (=)</td><td align="right">0.242s (-0.1%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.059s</td><td align="right">81</td><td align="right">2</td><td align="right">6.74094314 (=)</td><td align="right">0.060s (+0.7%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">2.89s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">2.89s (=)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">18.5s</td><td align="right">5</td><td align="right">6</td><td align="right">5.56852929 (=)</td><td align="right">18.6s (+0.5%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.005s (+1.8%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (+3.4%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.47022351</td><td align="right">2.94s</td><td align="right">9</td><td align="right">3</td><td align="right">7.47022351 (=)</td><td align="right">2.96s (+0.7%)</td><td align="right">9</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39270252</td><td align="right">3.63s</td><td align="right">18</td><td align="right">3</td><td align="right">5.39270252 (=)</td><td align="right">3.60s (-0.8%)</td><td align="right">18</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57842117</td><td align="right">3.93s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57842117 (=)</td><td align="right">3.91s (-0.7%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.43135341</td><td align="right">8.98s</td><td align="right">19</td><td align="right">3</td><td align="right">7.43135341 (=)</td><td align="right">8.88s (-1.1%)</td><td align="right">19</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.12s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.13s (+0.4%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-d`</td><td align="right">7.10873240</td><td align="right">7.82s</td><td align="right">130</td><td align="right">2</td><td align="right">7.10873240 (=)</td><td align="right">7.86s (+0.5%)</td><td align="right">130</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-d`</td><td align="right">6.91113734</td><td align="right">8.17s</td><td align="right">191</td><td align="right">2</td><td align="right">6.91113734 (=)</td><td align="right">8.13s (-0.4%)</td><td align="right">191</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">5.91084928</td><td align="right">0.686s</td><td align="right">188</td><td align="right">2</td><td align="right">5.91084928 (=)</td><td align="right">0.681s (-0.8%)</td><td align="right">188</td><td align="right">2</td></tr>
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
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.001s</td><td align="right">9</td><td align="right">2</td><td align="right">3.51775481 (=)</td><td align="right">0.001s (+7.9%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td>jazz</td><td align="right">6.86122977</td><td align="right">0.007s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86122977 (=)</td><td align="right">0.008s (+0.6%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28307258</td><td align="right">0.010s</td><td align="right">59</td><td align="right">2</td><td align="right">4.28307258 (=)</td><td align="right">0.010s (+1.6%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td>powergrid</td><td align="right">5.63729688</td><td align="right">0.105s</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (=)</td><td align="right">0.111s (+6.3%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73957529</td><td align="right">0.046s</td><td align="right">81</td><td align="right">2</td><td align="right">6.73957529 (=)</td><td align="right">0.045s (-0.8%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.94997883</td><td align="right">2.24s</td><td align="right">506</td><td align="right">2</td><td align="right">7.94997883 (=)</td><td align="right">2.19s (-2.0%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td>web-NotreDame</td><td align="right">6.75421666</td><td align="right">17.2s</td><td align="right">11991</td><td align="right">2</td><td align="right">6.75421666 (=)</td><td align="right">17.2s (+0.1%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.004s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.004s (+3.3%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (+3.8%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.48674709</td><td align="right">2.56s</td><td align="right">158</td><td align="right">2</td><td align="right">7.48674709 (=)</td><td align="right">2.53s (-1.3%)</td><td align="right">158</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39306097</td><td align="right">3.36s</td><td align="right">334</td><td align="right">2</td><td align="right">5.39306097 (=)</td><td align="right">3.53s (+5.1%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57896653</td><td align="right">3.50s</td><td align="right">300</td><td align="right">2</td><td align="right">5.57896653 (=)</td><td align="right">3.50s (-0.1%)</td><td align="right">300</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.42680610</td><td align="right">9.57s</td><td align="right">2219</td><td align="right">2</td><td align="right">7.42680610 (=)</td><td align="right">9.60s (+0.2%)</td><td align="right">2219</td><td align="right">2</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">2.86s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">2.86s (-0.3%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-2d`</td><td align="right">7.10873240</td><td align="right">6.60s</td><td align="right">130</td><td align="right">2</td><td align="right">7.10873240 (=)</td><td align="right">6.84s (+3.7%)</td><td align="right">130</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d`</td><td align="right">6.91113734</td><td align="right">6.94s</td><td align="right">191</td><td align="right">2</td><td align="right">6.91113734 (=)</td><td align="right">6.89s (-0.7%)</td><td align="right">191</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-2d`</td><td align="right">5.91084928</td><td align="right">0.643s</td><td align="right">188</td><td align="right">2</td><td align="right">5.91084928 (=)</td><td align="right">0.638s (-0.8%)</td><td align="right">188</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

`-N1` is where a time difference is code and nothing else: both binaries compute the same partition.
The `-2d -c` rows seed the search with the planted partition (soft `-c`, #1028).

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
<tr><td>lazega</td><td align="right">6.06055879</td><td align="right">0.001s</td><td align="right">6</td><td align="right">2</td><td align="right">6.06055879 (=)</td><td align="right">0.001s (-2.5%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (+4.4%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50222401</td><td align="right">0.349s</td><td align="right">8</td><td align="right">3</td><td align="right">7.50222401 (=)</td><td align="right">0.343s (-1.7%)</td><td align="right">8</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.47323105</td><td align="right">0.339s</td><td align="right">22</td><td align="right">3</td><td align="right">5.47323105 (=)</td><td align="right">0.346s (+1.9%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.66687303</td><td align="right">0.462s</td><td align="right">15</td><td align="right">3</td><td align="right">5.66687303 (=)</td><td align="right">0.440s (-4.7%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.58076831</td><td align="right">0.838s</td><td align="right">43</td><td align="right">3</td><td align="right">7.58076831 (=)</td><td align="right">0.841s (+0.3%)</td><td align="right">43</td><td align="right">3</td></tr>
<tr><td>overlapping om5 `-2d`</td><td align="right">7.17509147</td><td align="right">1.79s</td><td align="right">135</td><td align="right">2</td><td align="right">7.17509147 (=)</td><td align="right">1.83s (+1.9%)</td><td align="right">135</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d`</td><td align="right">6.91326445</td><td align="right">1.74s</td><td align="right">193</td><td align="right">2</td><td align="right">6.91326445 (=)</td><td align="right">1.76s (+0.8%)</td><td align="right">193</td><td align="right">2</td></tr>
<tr><td>wikispeedia `-2d`</td><td align="right">5.92071240</td><td align="right">0.093s</td><td align="right">184</td><td align="right">2</td><td align="right">5.92071240 (=)</td><td align="right">0.094s (+1.0%)</td><td align="right">184</td><td align="right">2</td></tr>
<tr><td>overlapping om5 `-d`</td><td align="right">7.88481128</td><td align="right">0.755s</td><td align="right">66</td><td align="right">4</td><td align="right">7.88481128 (=)</td><td align="right">0.744s (-1.4%)</td><td align="right">66</td><td align="right">4</td></tr>
<tr><td>overlapping om6 `-d`</td><td align="right">7.47937471</td><td align="right">0.782s</td><td align="right">92</td><td align="right">4</td><td align="right">7.47937471 (=)</td><td align="right">0.813s (+4.0%)</td><td align="right">92</td><td align="right">4</td></tr>
<tr><td>wikispeedia `-d`</td><td align="right">6.11316741</td><td align="right">0.076s</td><td align="right">47</td><td align="right">3</td><td align="right">6.11316741 (=)</td><td align="right">0.077s (+1.5%)</td><td align="right">47</td><td align="right">3</td></tr>
<tr><td>overlapping om5 `-2d -c` planted</td><td align="right">6.85777811</td><td align="right">1.14s</td><td align="right">296</td><td align="right">2</td><td align="right">6.85777811 (=)</td><td align="right">1.16s (+1.0%)</td><td align="right">296</td><td align="right">2</td></tr>
<tr><td>overlapping om6 `-2d -c` planted</td><td align="right">6.87337875</td><td align="right">1.25s</td><td align="right">446</td><td align="right">2</td><td align="right">6.87337875 (=)</td><td align="right">1.25s (=)</td><td align="right">446</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar

air30k (meta) is quoted at `-N1` on both sides: the OO arm does not finish `-N10` inside 30 minutes on
that row.

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">object-oriented</th>
<th colspan="4">columnar <code>-C</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.005s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (-72.8%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.023s</td><td align="right">5</td><td align="right">2</td><td align="right">6.86275593 (-0.0042%)</td><td align="right">0.007s (-69.0%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.04354934</td><td align="right">0.121s</td><td align="right">2</td><td align="right">5</td><td align="right">4.05454025 (+0.2718%)</td><td align="right">0.024s (-79.9%)</td><td align="right">2</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.75872920</td><td align="right">1.86s</td><td align="right">5</td><td align="right">4</td><td align="right">4.74107206 (-0.3710%)</td><td align="right">0.242s (-86.9%)</td><td align="right">5</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73892798</td><td align="right">0.135s</td><td align="right">80</td><td align="right">2</td><td align="right">6.74094314 (+0.0299%)</td><td align="right">0.060s (-55.7%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.83638921</td><td align="right">7.07s</td><td align="right">11</td><td align="right">3</td><td align="right">7.83343660 (-0.0377%)</td><td align="right">2.89s (-59.1%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56592477</td><td align="right">137.3s</td><td align="right">17</td><td align="right">5</td><td align="right">5.56852929 (+0.0468%)</td><td align="right">18.6s (-86.5%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.013s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.005s (-61.3%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.002s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (-21.2%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50242050</td><td align="right">8.25s</td><td align="right">142</td><td align="right">2</td><td align="right">7.47022351 (-0.4292%)</td><td align="right">2.96s (-64.2%)</td><td align="right">9</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39287115</td><td align="right">10.9s</td><td align="right">16</td><td align="right">3</td><td align="right">5.39270252 (-0.0031%)</td><td align="right">3.60s (-67.1%)</td><td align="right">18</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57843563</td><td align="right">7.16s</td><td align="right">301</td><td align="right">2</td><td align="right">5.57842117 (-0.0003%)</td><td align="right">3.91s (-45.4%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">7.94035360</td><td align="right">7.54s</td><td align="right">25</td><td align="right">3</td><td align="right">8.23558553 (+3.7181%)</td><td align="right">3.13s (-58.5%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td>air30k (meta)</td><td align="right">8.43783233</td><td align="right">5.18s</td><td align="right">35</td><td align="right">3</td><td align="right">7.58076831 (-10.1574%)</td><td align="right">0.841s (-83.8%)</td><td align="right">43</td><td align="right">3</td></tr>
</tbody>
</table>

### The fast dial `-F`

`-F` (`--fast-hierarchical-solution`) skips the interior-layer refinement in favour of a single bottom
re-partition within grandparents plus the module-level coarsening loop. Measured as `-C -F`: **`-F` alone
does not select the columnar engine**.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">3</td><td align="right">3</td><td align="right">3.38583082 (=)</td><td align="right">0.001s (-3.7%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.007s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86275593 (=)</td><td align="right">0.007s (-3.2%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.024s</td><td align="right">2</td><td align="right">4</td><td align="right">4.06300588 (+0.2088%)</td><td align="right">0.015s (-38.0%)</td><td align="right">4</td><td align="right">4</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.242s</td><td align="right">5</td><td align="right">5</td><td align="right">4.77402224 (+0.6950%)</td><td align="right">0.149s (-38.4%)</td><td align="right">4</td><td align="right">5</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.060s</td><td align="right">81</td><td align="right">2</td><td align="right">6.74094314 (=)</td><td align="right">0.056s (-5.4%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">2.89s</td><td align="right">15</td><td align="right">3</td><td align="right">7.83343660 (=)</td><td align="right">2.76s (-4.3%)</td><td align="right">15</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">18.6s</td><td align="right">5</td><td align="right">6</td><td align="right">5.62506198 (+1.0152%)</td><td align="right">14.7s (-20.5%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.004s (-11.0%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (-4.9%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.47022351</td><td align="right">2.96s</td><td align="right">9</td><td align="right">3</td><td align="right">7.47022351 (=)</td><td align="right">2.93s (-1.1%)</td><td align="right">9</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39270252</td><td align="right">3.60s</td><td align="right">18</td><td align="right">3</td><td align="right">5.39270252 (=)</td><td align="right">3.40s (-5.5%)</td><td align="right">18</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57842117</td><td align="right">3.91s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57842117 (=)</td><td align="right">3.42s (-12.6%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.43135341</td><td align="right">8.88s</td><td align="right">19</td><td align="right">3</td><td align="right">7.43135341 (=)</td><td align="right">8.71s (-1.8%)</td><td align="right">19</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.13s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (=)</td><td align="right">3.04s (-2.7%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

### Two-level clustering (`-2`)

<table>
<thead>
<tr>
<th rowspan="2">network</th>
<th colspan="4">object-oriented <code>-2</code></th>
<th colspan="4">columnar <code>-C -2</code></th>
</tr>
<tr>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
<th>codelength</th><th>time</th><th>top</th><th>lvls</th>
</tr>
</thead>
<tbody>
<tr><td>ninetriangles</td><td align="right">3.51775481</td><td align="right">0.002s</td><td align="right">9</td><td align="right">2</td><td align="right">3.51775481 (=)</td><td align="right">0.001s (-47.4%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td>jazz</td><td align="right">6.86304747</td><td align="right">0.013s</td><td align="right">5</td><td align="right">2</td><td align="right">6.86122977 (-0.0265%)</td><td align="right">0.008s (-41.0%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.28501267</td><td align="right">0.031s</td><td align="right">56</td><td align="right">2</td><td align="right">4.28307258 (-0.0453%)</td><td align="right">0.010s (-68.1%)</td><td align="right">59</td><td align="right">2</td></tr>
<tr><td>powergrid</td><td align="right">5.60044386</td><td align="right">0.560s</td><td align="right">419</td><td align="right">2</td><td align="right">5.63729688 (+0.6580%)</td><td align="right">0.111s (-80.1%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td>politicalblogs</td><td align="right">6.73972141</td><td align="right">0.076s</td><td align="right">80</td><td align="right">2</td><td align="right">6.73957529 (-0.0022%)</td><td align="right">0.045s (-40.0%)</td><td align="right">81</td><td align="right">2</td></tr>
<tr><td>science2001</td><td align="right">7.95003960</td><td align="right">3.66s</td><td align="right">496</td><td align="right">2</td><td align="right">7.94997883 (-0.0008%)</td><td align="right">2.19s (-40.1%)</td><td align="right">506</td><td align="right">2</td></tr>
<tr><td>web-NotreDame</td><td align="right">6.74298853</td><td align="right">41.2s</td><td align="right">11809</td><td align="right">2</td><td align="right">6.75421666 (+0.1665%)</td><td align="right">17.2s (-58.3%)</td><td align="right">11991</td><td align="right">2</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.008s</td><td align="right">7</td><td align="right">2</td><td align="right">6.01786027 (=)</td><td align="right">0.004s (-41.9%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">2</td><td align="right">2</td><td align="right">2.01140524 (=)</td><td align="right">0.001s (-10.9%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.50595639</td><td align="right">6.01s</td><td align="right">142</td><td align="right">2</td><td align="right">7.48674709 (-0.2559%)</td><td align="right">2.53s (-57.9%)</td><td align="right">158</td><td align="right">2</td></tr>
<tr><td>air30k</td><td align="right">5.39331278</td><td align="right">4.34s</td><td align="right">332</td><td align="right">2</td><td align="right">5.39306097 (-0.0047%)</td><td align="right">3.53s (-18.7%)</td><td align="right">334</td><td align="right">2</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57921689</td><td align="right">5.36s</td><td align="right">301</td><td align="right">2</td><td align="right">5.57896653 (-0.0045%)</td><td align="right">3.50s (-34.8%)</td><td align="right">300</td><td align="right">2</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.13096953</td><td align="right">5.55s</td><td align="right">25</td><td align="right">2</td><td align="right">8.23558553 (+1.2866%)</td><td align="right">2.86s (-48.5%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>

### The non-redundant map equation L\* (`--non-redundant`)

L\* excludes the module just left from a module's exit codebook and gives the first visit after entering
its own enter codebook, so it is a different objective — the bits are not comparable to the column on
the left, only the times are.

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
<tr><td>ninetriangles</td><td align="right">3.38583082</td><td align="right">0.001s</td><td align="right">3</td><td align="right">3</td><td align="right">3.07806732 (-9.0897%)</td><td align="right">0.001s (-0.1%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td>jazz</td><td align="right">6.86275593</td><td align="right">0.007s</td><td align="right">6</td><td align="right">2</td><td align="right">6.86822837 (+0.0797%)</td><td align="right">0.008s (+4.6%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>netscicoauthor2010</td><td align="right">4.05454025</td><td align="right">0.024s</td><td align="right">2</td><td align="right">4</td><td align="right">3.89220976 (-4.0037%)</td><td align="right">0.024s (-1.7%)</td><td align="right">2</td><td align="right">5</td></tr>
<tr><td>powergrid</td><td align="right">4.74107206</td><td align="right">0.242s</td><td align="right">5</td><td align="right">5</td><td align="right">4.50926542 (-4.8893%)</td><td align="right">0.237s (-2.2%)</td><td align="right">3</td><td align="right">7</td></tr>
<tr><td>politicalblogs</td><td align="right">6.74094314</td><td align="right">0.060s</td><td align="right">81</td><td align="right">2</td><td align="right">6.78924150 (+0.7165%)</td><td align="right">0.062s (+4.3%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td>science2001</td><td align="right">7.83343660</td><td align="right">2.89s</td><td align="right">15</td><td align="right">3</td><td align="right">8.00917226 (+2.2434%)</td><td align="right">2.64s (-8.7%)</td><td align="right">22</td><td align="right">3</td></tr>
<tr><td>web-NotreDame</td><td align="right">5.56852929</td><td align="right">18.6s</td><td align="right">5</td><td align="right">6</td><td align="right">5.51707363 (-0.9240%)</td><td align="right">19.4s (+4.3%)</td><td align="right">5</td><td align="right">6</td></tr>
<tr><td>lazega</td><td align="right">6.01786027</td><td align="right">0.005s</td><td align="right">7</td><td align="right">2</td><td align="right">5.96862465 (-0.8182%)</td><td align="right">0.004s (-9.3%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td>multilayer (ex.)</td><td align="right">2.01140524</td><td align="right">0.001s</td><td align="right">2</td><td align="right">2</td><td align="right">1.92885658 (-4.1040%)</td><td align="right">0.001s (-1.9%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td>malaria</td><td align="right">7.47022351</td><td align="right">2.96s</td><td align="right">9</td><td align="right">3</td><td align="right">7.48730919 (+0.2287%)</td><td align="right">2.96s (+0.2%)</td><td align="right">9</td><td align="right">3</td></tr>
<tr><td>air30k</td><td align="right">5.39270252</td><td align="right">3.60s</td><td align="right">18</td><td align="right">3</td><td align="right">5.37926811 (-0.2491%)</td><td align="right">3.68s (+2.4%)</td><td align="right">18</td><td align="right">3</td></tr>
<tr><td>air30k (reg.)</td><td align="right">5.57842117</td><td align="right">3.91s</td><td align="right">11</td><td align="right">3</td><td align="right">5.57094996 (-0.1339%)</td><td align="right">3.89s (-0.4%)</td><td align="right">11</td><td align="right">3</td></tr>
<tr><td>air30k (meta)</td><td align="right">7.43135341</td><td align="right">8.88s</td><td align="right">19</td><td align="right">3</td><td align="right">7.21545172 (-2.9053%)</td><td align="right">7.84s (-11.7%)</td><td align="right">32</td><td align="right">3</td></tr>
<tr><td>science2001 (pref.)</td><td align="right">8.23558553</td><td align="right">3.13s</td><td align="right">25</td><td align="right">2</td><td align="right">8.44774545 (+2.5761%)</td><td align="right">3.11s (-0.5%)</td><td align="right">25</td><td align="right">2</td></tr>
</tbody>
</table>
