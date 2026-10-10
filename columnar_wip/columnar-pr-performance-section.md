## Performance

> Manual old-vs-new benchmark of the `--columnar` engine over the set in [`columnar_wip/benchmark-networks.md`](columnar_wip/benchmark-networks.md). This is **not** the CI `perf-pr.yml` check, which only sees the default OO path since the new core is flag-gated.

Single-threaded (`MODE=release OPENMP=0`), `--seed 123`. Codelength in bits. **`instr` is instructions retired** (`/usr/bin/time -l`). `time` is `--timing-json`'s `timing.total_s`.
- `-N10` rows run once (deterministic; `instr` carries the comparison).
- `-N1` rows are the interleaved minimum of 3.
- Driver: [`columnar_wip/bench-inputs.py`](columnar_wip/bench-inputs.py), writing [`columnar_wip/inputs-ab-results.tsv`](columnar_wip/inputs-ab-results.tsv).
- The tables are built from [`columnar_wip/inputs-snapshot.tsv`](columnar_wip/inputs-snapshot.tsv), which [`columnar_wip/combine-inputs-snapshot.py`](columnar_wip/combine-inputs-snapshot.py) writes.

> **This PR makes the benchmark inputs fetchable from one package (F64, F65).**
> - [`columnar_wip/benchmark-networks.toml`](columnar_wip/benchmark-networks.toml) names a source in the `mapequation-networks` package for every input: public repositories (netzschleuder, SNAP, the DB1B coupons, Wikispeedia) and the package's overlapping-memory generator.
> - [`columnar_wip/fetch-benchmark-networks.py`](columnar_wip/fetch-benchmark-networks.py) builds them into `networks/columnar-benchmark/` and checks each file against [`columnar_wip/benchmark-networks.sha256`](columnar_wip/benchmark-networks.sha256).
>
> No engine code changes, and the binary is the #1128 snapshot's. What changes is the inputs:
> - **Different networks:**
>   - Three networks with no public source are replaced (F65):
>     - netscicoauthor2010 by netzschleuder's `netscience`;
>     - politicalblogs, a Swedish blog network, by the Adamic–Glance `polblogs`;
>     - science2001, derived from licensed Journal Citation Reports data, by `word_assoc` (also in the preferred-modules row).
>   - web-NotreDame is now the SNAP file as distributed. The old file was a DAG: SNAP with every edge oriented from low to high id.
>   - air30k and its two variants are rebuilt from the DB1B coupons on the same 183 airports.
>   - The five om files that were never seeded (om2 E50000, om4 / om5 / om6 / om8 E100000) are now seed 1.
> - **Same networks, other bytes:**
>   - jazz is 0-based with another line order, and powergrid is relabelled. Both search like another seed.
>   - malaria has another line order, and the nine seeded om files number their states in planting order and carry a header. These are bit-identical on every configuration.
> - **Byte-identical:** ninetriangles, multilayer, lazega, wikispeedia.

> **Old** = the old input file, **new** = the fetched one. Both run on `columnar-hierarchical-core` tip `c9ce210b`, md5 `54c8b6b21ab1b54dd3931a9c7f79833e`, which is the #1128 snapshot's new binary.
> - **Rows whose input changed** were measured in this session (2026-10-10), the two inputs interleaved per row, `-N1` rows as the minimum of 3 reps spread across the batch, `-N10` rows once. Two cells were re-measured as the minimum of 4 (below).
> - **The three replaced networks** were measured the same way in a second pass the same day. Each old arm is the replaced file, run under the new label.
> - **The old-input column reproduces #1128's new column** in bits, top modules and levels on all 164 configurations (325 runs, every rep) the session repeats.
> - **Rows whose input is byte-identical** are carried from #1128's session ([`columnar_wip/1127-ab-results.tsv`](columnar_wip/1127-ab-results.tsv)) as both arms. It is the same binary on the same bytes, so their old and new cells are one measurement.
> - **Load** was 5–14 during the first pass and about 5 during the second, so read `instr` before `time`.
> - **OO arms:** the OO engine is unchanged, so its cells are carried from the #1079-day session as before, except the rows whose input changed. Those this session measured on the new input: eleven rows in the OO table, ten in the two-level one.
>   - One of them did not finish. OO `air30k (meta)` `-N1` ran for 9 h 25 min on seed 123 and was killed (#1134).
>   - Seeds 456 and 7 finish it in 10 s and 6 s, and the original file in 6 s.

### What the change moves

Every configuration where the old and new inputs give different bits, both arms. All 88 are on inputs that changed:
- jazz and powergrid (relabelled);
- web-NotreDame, air30k and the five unseeded om networks (different networks);
- netscience, polblogs and word_assoc (replacements, old arm on the replaced file).

The other 76 configurations this session measured on both inputs are bit-identical: the nine seeded om networks (71), malaria (3) and jazz (2). Those are at −1.36% to +0.19% in instructions (median +0.01%).

| network | table | old bits | new bits | Δbits | old instr | new instr | Δinstr | old time | new time | Δtime |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| jazz | `-C -N1` | 6.899367957 | **6.896294162** | **-0.0446%** | 0.1G | 0.1G | -3.20% | 0.001s | 0.001s | +4.5% |
| powergrid | `-C -N1` | 4.730850312 | **4.729861209** | **-0.0209%** | 0.4G | 0.4G | -0.77% | 0.026s | 0.025s | -1.6% |
| web-NotreDame | `-C -N1` | 5.556421705 | **6.218856777** | **+11.9220%** | 24.4G | 25.1G | +2.81% | 2.42s | 2.49s | +2.9% |
| air30k | `-C -N1` | 5.470440768 | **5.456991462** | **-0.2459%** | 4.5G | 4.7G | +3.41% | 0.374s | 0.412s | +10.2% |
| air30k (reg.) | `-C -N1` | 5.657913279 | **5.666386425** | **+0.1498%** | 5.9G | 5.6G | -3.91% | 0.489s | 0.468s | -4.4% |
| air30k (meta) | `-C -N1` | 7.546335898 | **7.662468548** | **+1.5389%** | 9.2G | 7.7G | -17.03% | 0.892s | 0.738s | -17.3% |
| overlapping om2 `-2d` | `-C -N1` | 6.739607071 | **6.744558723** | **+0.0735%** | 14.0G | 13.0G | -7.37% | 1.30s | 1.20s | -7.8% |
| overlapping om2 `-d` | `-C -N1` | 7.29196634 | **7.295306496** | **+0.0458%** | 3.7G | 3.7G | -0.01% | 0.382s | 0.364s | -4.8% |
| overlapping om2 `-2d -c` planted | `-C -N1` | 6.744721993 | **6.734990605** | **-0.1443%** | 6.2G | 6.3G | +1.93% | 0.560s | 0.573s | +2.3% |
| overlapping om4 `-2d` | `-C -N1` | 6.861732911 | **6.858825005** | **-0.0424%** | 15.7G | 13.5G | -14.52% | 1.50s | 1.35s | -10.5% |
| overlapping om4 `-d` | `-C -N1` | 6.861732911 | **6.858825005** | **-0.0424%** | 20.7G | 18.8G | -9.53% | 1.95s | 1.80s | -7.5% |
| overlapping om4 `-2d -c` planted | `-C -N1` | 6.856285862 | **6.850471471** | **-0.0848%** | 7.3G | 9.5G | +29.81% | 0.705s | 0.899s | +27.5% |
| overlapping om5 `-2d` | `-C -N1` | 6.868180827 | **6.948978287** | **+1.1764%** | 17.2G | 15.9G | -7.70% | 1.70s | 1.62s | -4.9% |
| overlapping om5 `-d` | `-C -N1` | 7.812252898 | **7.795491139** | **-0.2146%** | 7.2G | 7.1G | -1.22% | 0.758s | 0.749s | -1.2% |
| overlapping om5 `-2d -c` planted | `-C -N1` | 6.857876353 | **6.867718448** | **+0.1435%** | 9.5G | 9.8G | +3.95% | 0.921s | 0.947s | +2.8% |
| overlapping om6 `-2d` | `-C -N1` | 6.884236095 | **6.894797134** | **+0.1534%** | 16.3G | 15.2G | -7.21% | 1.69s | 1.56s | -7.5% |
| overlapping om6 `-d` | `-C -N1` | 7.440161581 | **7.478431518** | **+0.5144%** | 7.5G | 7.3G | -2.47% | 0.804s | 0.783s | -2.6% |
| overlapping om6 `-2d -c` planted | `-C -N1` | 6.873464257 | **6.87656878** | **+0.0452%** | 11.3G | 9.7G | -13.90% | 1.11s | 0.968s | -13.1% |
| overlapping om8 `-2d` | `-C -N1` | 6.894258582 | **6.876045079** | **-0.2642%** | 17.9G | 18.6G | +3.90% | 1.97s | 1.99s | +1.3% |
| overlapping om8 `-d` | `-C -N1` | 6.967535764 | **6.938347922** | **-0.4189%** | 7.5G | 7.8G | +4.39% | 0.805s | 0.830s | +3.0% |
| overlapping om8 `-2d -c` planted | `-C -N1` | 6.875540042 | **6.863878689** | **-0.1696%** | 11.8G | 12.2G | +3.55% | 1.26s | 1.29s | +2.3% |
| netscience | `-C -N1` | 4.047459862 | **3.364566019** | **-16.8722%** | 0.1G | 0.1G | +61.62% | 0.003s | 0.007s | +172.3% |
| polblogs | `-C -N1` | 6.758265349 | **7.593689164** | **+12.3615%** | 0.2G | 0.3G | +37.61% | 0.009s | 0.015s | +59.5% |
| word_assoc | `-C -N1` | 7.807937174 | **11.70862241** | **+49.9579%** | 5.7G | 3.4G | -40.29% | 0.410s | 0.272s | -33.6% |
| word_assoc (pref.) | `-C -N1` | 8.460796773 | **11.94886955** | **+41.2263%** | 5.8G | 4.4G | -24.03% | 0.412s | 0.420s | +1.8% |
| web-NotreDame | `-C` | 5.556421705 | **6.174545892** | **+11.1245%** | 182.7G | 186.6G | +2.13% | 21.2s | 20.6s | -2.9% |
| air30k (meta) | `-C` | 7.421664324 | **7.414540615** | **-0.0960%** | 97.9G | 92.6G | -5.40% | 10.1s | 9.89s | -2.3% |
| air30k (reg.) | `-C` | 5.574537176 | **5.578852599** | **+0.0774%** | 40.4G | 40.8G | +1.07% | 3.89s | 3.88s | -0.3% |
| overlapping om2 `-d` | `-C` | 6.731808656 | **6.752836583** | **+0.3124%** | 47.5G | 38.5G | -19.07% | 4.62s | 3.76s | -18.7% |
| powergrid | `-C` | 4.712391773 | **4.713040685** | **+0.0138%** | 2.8G | 2.8G | +0.13% | 0.250s | 0.250s | -0.3% |
| overlapping om8 `-d` | `-C` | 6.88742315 | **6.87278322** | **-0.2126%** | 86.5G | 86.7G | +0.20% | 9.65s | 9.68s | +0.2% |
| overlapping om4 `-d` | `-C` | 6.866901786 | **6.860084625** | **-0.0993%** | 60.7G | 61.3G | +0.92% | 8.01s | 6.20s | -22.6% |
| air30k | `-C` | 5.392285003 | **5.39323549** | **+0.0176%** | 38.6G | 38.6G | -0.01% | 3.61s | 3.56s | -1.3% |
| overlapping om6 `-d` | `-C` | 6.88496897 | **6.892138569** | **+0.1041%** | 81.3G | 79.6G | -2.12% | 8.93s | 8.62s | -3.4% |
| overlapping om5 `-d` | `-C` | 6.867301407 | **6.880795013** | **+0.1965%** | 59.0G | 58.6G | -0.62% | 6.38s | 6.22s | -2.5% |
| polblogs | `-C` | 6.740943136 | **7.592773105** | **+12.6367%** | 0.7G | 0.9G | +34.14% | 0.064s | 0.082s | +29.0% |
| word_assoc (pref.) | `-C` | 8.235585529 | **11.73480892** | **+42.4891%** | 35.5G | 49.2G | +38.57% | 3.29s | 4.70s | +43.1% |
| netscience | `-C` | 4.023567105 | **3.363918326** | **-16.3946%** | 0.3G | 0.8G | +159.36% | 0.023s | 0.066s | +189.4% |
| word_assoc | `-C` | 7.807937174 | **11.43278494** | **+46.4252%** | 34.2G | 44.5G | +30.20% | 3.05s | 4.19s | +37.4% |
| overlapping om8 `-2d` | `-C -2` | 6.88742315 | **6.873356654** | **-0.2042%** | 74.5G | 72.6G | -2.52% | 8.90s | 8.40s | -5.7% |
| web-NotreDame | `-C -2` | 6.754216663 | **6.889480271** | **+2.0027%** | 117.5G | 95.3G | -18.89% | 19.3s | 15.7s | -18.4% |
| overlapping om5 `-2d` | `-C -2` | 6.867301407 | **6.880795013** | **+0.1965%** | 68.4G | 66.8G | -2.33% | 7.95s | 7.61s | -4.3% |
| overlapping om6 `-2d` | `-C -2` | 6.88496897 | **6.893434836** | **+0.1230%** | 69.0G | 68.2G | -1.09% | 7.65s | 7.60s | -0.7% |
| air30k (reg.) | `-C -2` | 5.571539329 | **5.57079319** | **-0.0134%** | 41.2G | 40.8G | -0.94% | 3.89s | 3.86s | -0.6% |
| overlapping om2 `-2d` | `-C -2` | 6.740761645 | **6.752836583** | **+0.1791%** | 35.6G | 33.1G | -6.95% | 3.61s | 3.34s | -7.6% |
| air30k (meta) | `-C -2` | 7.424143707 | **7.416311014** | **-0.1055%** | 104.5G | 102.9G | -1.51% | 10.7s | 10.5s | -2.3% |
| overlapping om4 `-2d` | `-C -2` | 6.866901786 | **6.859843831** | **-0.1028%** | 70.6G | 71.7G | +1.44% | 7.33s | 7.45s | +1.7% |
| air30k | `-C -2` | 5.393055049 | **5.39136505** | **-0.0313%** | 41.8G | 42.4G | +1.65% | 3.92s | 3.70s | -5.6% |
| powergrid | `-C -2` | 5.63729688 | **5.634871397** | **-0.0430%** | 1.1G | 1.2G | +1.20% | 0.099s | 0.101s | +2.2% |
| netscience | `-C -2` | 4.283072584 | **3.529536656** | **-17.5933%** | 0.2G | 0.3G | +76.21% | 0.009s | 0.020s | +130.4% |
| polblogs | `-C -2` | 6.739575295 | **7.592588015** | **+12.6568%** | 0.5G | 0.8G | +56.91% | 0.043s | 0.068s | +59.4% |
| word_assoc (pref.) | `-C -2` | 8.235585529 | **11.73480892** | **+42.4891%** | 31.5G | 45.4G | +43.82% | 3.06s | 3.98s | +30.0% |
| word_assoc | `-C -2` | 7.949978834 | **11.42993396** | **+43.7731%** | 23.8G | 34.9G | +46.15% | 2.32s | 3.09s | +33.4% |
| overlapping om2 `-d` | `-C -F -N1` | 7.321678354 | **7.317528373** | **-0.0567%** | 2.6G | 2.6G | -0.20% | 0.272s | 0.266s | -2.1% |
| overlapping om4 `-d` | `-C -F -N1` | 6.861732911 | **6.858825005** | **-0.0424%** | 18.4G | 16.3G | -11.55% | 1.74s | 1.58s | -9.1% |
| overlapping om5 `-d` | `-C -F -N1` | 7.798283664 | **7.829247983** | **+0.3971%** | 5.8G | 5.0G | -13.98% | 0.658s | 0.555s | -15.6% |
| overlapping om6 `-d` | `-C -F -N1` | 7.507066407 | **7.546115138** | **+0.5202%** | 5.8G | 4.9G | -16.07% | 0.679s | 0.562s | -17.3% |
| overlapping om8 `-d` | `-C -F -N1` | 7.022972054 | **7.007657776** | **-0.2181%** | 5.2G | 6.1G | +16.82% | 0.606s | 0.730s | +20.5% |
| overlapping om2 `-d` | `-C -F` | 6.731808656 | **6.752836583** | **+0.3124%** | 40.3G | 32.5G | -19.24% | 3.96s | 3.45s | -12.7% |
| overlapping om4 `-d` | `-C -F` | 6.866901786 | **6.860084625** | **-0.0993%** | 56.0G | 56.8G | +1.33% | 6.01s | 5.93s | -1.4% |
| overlapping om8 `-d` | `-C -F` | 6.88742315 | **6.87278322** | **-0.2126%** | 74.8G | 72.3G | -3.32% | 9.58s | 8.41s | -12.2% |
| overlapping om5 `-d` | `-C -F` | 6.867301407 | **6.880795013** | **+0.1965%** | 54.3G | 53.8G | -0.89% | 5.71s | 5.55s | -2.8% |
| overlapping om6 `-d` | `-C -F` | 6.88496897 | **6.892138569** | **+0.1041%** | 66.6G | 65.5G | -1.76% | 7.58s | 7.20s | -5.1% |
| om2 `-2d --regularized -N1` | family | 7.548863183 | **7.930395953** | **+5.0542%** | 7.2G | 3.1G | -56.67% | 0.739s | 0.331s | -55.2% |
| om2 `-d --regularized -N1` | family | 7.548863183 | **7.930395953** | **+5.0542%** | 8.5G | 4.4G | -48.01% | 0.868s | 0.476s | -45.1% |
| om2 planted, `-2d --no-infomap -c` | family | 6.789039995 | **6.778901842** | **-0.1493%** | 0.6G | 0.6G | +0.01% | 0.055s | 0.049s | -11.4% |
| om4 `-2d --regularized -N1` | family | 7.556894677 | **7.535458803** | **-0.2837%** | 9.1G | 10.2G | +12.18% | 0.967s | 1.08s | +11.4% |
| om4 `-d --regularized -N1` | family | 7.556894677 | **7.535458803** | **-0.2837%** | 11.7G | 12.9G | +9.65% | 1.22s | 1.31s | +7.5% |
| om4 planted, `-2d --no-infomap -c` | family | 6.880650147 | **6.873858104** | **-0.0987%** | 1.0G | 1.0G | +0.21% | 0.110s | 0.106s | -4.1% |
| om5 `-2d --regularized -N1` | family | 7.968629202 | **7.974202977** | **+0.0699%** | 7.8G | 6.1G | -21.21% | 0.889s | 0.706s | -20.6% |
| om5 `-d --regularized -N1` | family | 7.968629202 | **7.974202977** | **+0.0699%** | 10.5G | 8.8G | -15.74% | 1.13s | 0.962s | -15.1% |
| om5 planted, `-2d --no-infomap -c` | family | 6.902222527 | **6.911031376** | **+0.1276%** | 1.1G | 1.1G | +0.00% | 0.137s | 0.117s | -14.6% |
| om6 `-2d --regularized -N1` | family | 7.982650944 | **7.979716886** | **-0.0368%** | 6.0G | 6.7G | +10.27% | 0.727s | 0.766s | +5.4% |
| om6 `-d --regularized -N1` | family | 7.982650944 | **7.979716886** | **-0.0368%** | 8.7G | 9.3G | +7.15% | 0.989s | 1.05s | +6.4% |
| om6 planted, `-2d --no-infomap -c` | family | 6.930934993 | **6.936312189** | **+0.0776%** | 1.1G | 1.1G | -0.62% | 0.130s | 0.122s | -6.5% |
| om8 `-2d --regularized -N1` | family | 7.983599366 | **7.984638887** | **+0.0130%** | 6.2G | 6.1G | -0.28% | 0.763s | 0.738s | -3.3% |
| om8 `-d --regularized -N1` | family | 7.983599366 | **7.984638887** | **+0.0130%** | 8.9G | 8.8G | -0.60% | 1.03s | 0.996s | -2.9% |
| om8 planted, `-2d --no-infomap -c` | family | 6.98103476 | **6.966510195** | **-0.2081%** | 1.2G | 1.2G | +0.36% | 0.146s | 0.137s | -5.9% |
| om4 `-d --regularized -N10` | family | 7.556653713 | **7.535170166** | **-0.2843%** | 48.7G | 46.8G | -3.73% | 5.36s | 5.30s | -1.2% |
| om6 `-2d --regularized -N10` | family | 7.982650944 | **7.979716886** | **-0.0368%** | 41.2G | 42.0G | +1.85% | 5.11s | 5.15s | +0.9% |
| om2 `-2d --regularized -N10` | family | 7.548721547 | **7.53510563** | **-0.1804%** | 24.8G | 24.6G | -0.78% | 2.64s | 2.61s | -0.9% |
| om6 `-d --regularized -N10` | family | 7.984417755 | **7.981023534** | **-0.0425%** | 41.3G | 42.0G | +1.57% | 4.95s | 5.05s | +1.9% |
| om8 `-2d --regularized -N10` | family | 7.979831947 | **7.984638887** | **+0.0602%** | 40.7G | 40.9G | +0.33% | 5.12s | 5.05s | -1.5% |
| om5 `-d --regularized -N10` | family | 7.968320007 | **7.973146326** | **+0.0606%** | 42.6G | 41.0G | -3.86% | 4.89s | 4.73s | -3.2% |
| om5 `-2d --regularized -N10` | family | 7.968061942 | **7.973194647** | **+0.0644%** | 43.1G | 40.9G | -4.94% | 4.98s | 4.81s | -3.5% |
| om2 `-d --regularized -N10` | family | 7.548721547 | **7.53510563** | **-0.1804%** | 23.9G | 23.9G | +0.08% | 2.53s | 2.44s | -3.5% |
| om4 `-2d --regularized -N10` | family | 7.556894677 | **7.534667081** | **-0.2941%** | 48.9G | 49.0G | +0.02% | 5.46s | 5.44s | -0.3% |
| om8 `-d --regularized -N10` | family | 7.978630877 | **7.985651781** | **+0.0880%** | 41.3G | 40.6G | -1.73% | 5.04s | 4.91s | -2.6% |

**Every cell where new is worse than old, and why.**

- **Different networks** (60 om rows, 9 air30k rows, 3 web-NotreDame rows). A different network has a different optimum, so these cells are new baselines, not regressions. Each one is read against that network's own references:
  - **web-NotreDame:** +11.1% in bits at `-C` (5.556 → 6.175 bits, 21.2 → 20.6 s) and +2.0% at `-C -2`. That is the price of the cycles the DAG did not have. On the new file OO lands at 6.177 bits in 169 s, against columnar's 6.175 bits in 20.6 s.
  - **air30k, `-N10`:** within 0.1% in bits in all three variants and both tables, and within ±6% in seconds.
  - **air30k, `-N1`:** single trials spread wider, −0.25% to +1.54% in bits.
  - **air30k (reg.) `-C`:** now 0.14% above OO and 0.15% above its own `-2` row, on three seeds (#1135). On the original file it was 0.07% *below* OO.
  - **om, plain objective:**
    - Old-vs-new deltas run from −0.42% to +1.18% in bits.
    - Every `-N10` search on the new draws ends below its planted partition (om2 E50000 −0.38%, om8 E100000 −1.35%).
    - #1041's om8 E100000 `-d` gap does not appear on the new draw: `-d` reaches 6.872783 bits, `-2d` 6.873357 bits.
  - **om, `--regularized`:**
    - #1042's mode survives on the new om5 E100000: the search ends 1.89% in bits above planted (7.973 vs 7.826).
    - The largest old-vs-new delta, +5.05% in bits, is om2 E50000 `--regularized -N1`. Seed 123 stops at 7.930 bits, near one-level (7.959). Seeds 456, 7, 11 and 99 reach 7.532–7.539, and OO returns one-level on all five. So it is a single trial's luck, not a columnar gap.
  - **Instructions and seconds** move with the network. For example, om4 E100000 `-2d -c planted -N1` takes +29.8% in instructions and om2 E50000 `-d` takes −19.1%.
- **Replaced networks** (12 rows: netscience, polblogs, word_assoc and its preferred-modules row). Every cell compares two different networks, so the deltas (−17.6% to +50.0% in bits) mean nothing; the new network's own references are what count.
  - **Against OO** at `-N10` on the new networks:
    - netscience −0.018% in bits (0.066 s vs 0.178 s);
    - polblogs −0.006% (0.082 s vs 0.250 s);
    - word_assoc +0.069% (4.19 s vs 7.22 s).
  - **word_assoc (pref.)** sits +0.91% in bits above OO (4.70 s vs 13.1 s), both at 25 top modules. That is the known difference in where the two engines charge `--preferred-number-of-modules` (#1068). science2001 (pref.) was +3.74%.
  - **polblogs keeps politicalblogs' role:** a directed network whose `-N10` optimum is two-level (91 top modules), the property that exposed F15 and F33.
- **Relabelled networks** (jazz, powergrid) search like another seed:
  - **Bits:** jazz `-N1` −0.045%; powergrid `-N1` −0.021%, `-C` +0.014%, `-C -2` −0.043%. The only worse cell in bits, powergrid `-C`, is under 0.1%.
  - **powergrid `-C -2` seconds:** +2.2% at +1.20% in instructions, as the minimum of 4. The first reading was +58.6% on a 0.1 s row. The relabelled input takes a different path to a different partition.
  - **jazz `-N1` seconds:** +4.5% on a 0.001 s row, at −3.2% in instructions.
- **Seconds on bit-identical rows:** 11 of the 76 read more than 1% slower, all at ≤ +0.13% in instructions.
  - The largest, om4 E50000 `-2d --regularized -N10`, read +11.1% and was re-measured: +1.3% as the minimum of 4, at +0.01% in instructions.
  - The rest are load: one binary on the same networks.

### Old vs new columnar — standard search (`-C -N10`)

Overlapping and wikispeedia rows run `-C -d -N10`. A row on a byte-identical input shows one carried
measurement in both columns. web-NotreDame, the three air30k rows, powergrid, the unseeded om rows and
the replaced networks (netscience, polblogs, word_assoc) move (above). jazz, malaria and the seeded om
rows are bit-identical.

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.001s (+0.0%)</td><td align="right">0.1G (+0.00%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.007s (-4.5%)</td><td align="right">0.1G (-1.07%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscience</td><td align="right">4.023567105</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">11</td><td align="right">4</td><td align="right">3.363918326 (-16.3946%)</td><td align="right">0.066s (+189.4%)</td><td align="right">0.8G (+159.36%)</td><td align="right">278</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.712391773</td><td align="right">0.250s</td><td align="right">2.8G</td><td align="right">10</td><td align="right">5</td><td align="right">4.713040685 (+0.0138%)</td><td align="right">0.250s (-0.3%)</td><td align="right">2.8G (+0.13%)</td><td align="right">6</td><td align="right">5</td></tr>
<tr><td align="right">polblogs</td><td align="right">6.740943136</td><td align="right">0.064s</td><td align="right">0.7G</td><td align="right">81</td><td align="right">2</td><td align="right">7.592773105 (+12.6367%)</td><td align="right">0.082s (+29.0%)</td><td align="right">0.9G (+34.14%)</td><td align="right">91</td><td align="right">2</td></tr>
<tr><td align="right">word_assoc</td><td align="right">7.807937174</td><td align="right">3.05s</td><td align="right">34.2G</td><td align="right">189</td><td align="right">3</td><td align="right">11.43278494 (+46.4252%)</td><td align="right">4.19s (+37.4%)</td><td align="right">44.5G (+30.20%)</td><td align="right">762</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">21.2s</td><td align="right">182.7G</td><td align="right">5</td><td align="right">6</td><td align="right">6.174545892 (+11.1245%)</td><td align="right">20.6s (-2.9%)</td><td align="right">186.6G (+2.13%)</td><td align="right">505</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.005s (+0.0%)</td><td align="right">0.1G (+0.00%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+0.0%)</td><td align="right">0.1G (+0.00%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.64s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.68s (+1.4%)</td><td align="right">31.5G (+0.02%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.392285003</td><td align="right">3.61s</td><td align="right">38.6G</td><td align="right">257</td><td align="right">3</td><td align="right">5.39323549 (+0.0176%)</td><td align="right">3.56s (-1.3%)</td><td align="right">38.6G (-0.01%)</td><td align="right">260</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.574537176</td><td align="right">3.89s</td><td align="right">40.4G</td><td align="right">228</td><td align="right">3</td><td align="right">5.578852599 (+0.0774%)</td><td align="right">3.88s (-0.3%)</td><td align="right">40.8G (+1.07%)</td><td align="right">225</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.421664324</td><td align="right">10.1s</td><td align="right">97.9G</td><td align="right">2135</td><td align="right">3</td><td align="right">7.414540615 (-0.0960%)</td><td align="right">9.89s (-2.3%)</td><td align="right">92.6G (-5.40%)</td><td align="right">2238</td><td align="right">3</td></tr>
<tr><td align="right">word_assoc (pref.)</td><td align="right">8.235585529</td><td align="right">3.29s</td><td align="right">35.5G</td><td align="right">25</td><td align="right">2</td><td align="right">11.73480892 (+42.4891%)</td><td align="right">4.70s (+43.1%)</td><td align="right">49.2G (+38.57%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">4.62s</td><td align="right">47.5G</td><td align="right">690</td><td align="right">2</td><td align="right">6.752836583 (+0.3124%)</td><td align="right">3.76s (-18.7%)</td><td align="right">38.5G (-19.07%)</td><td align="right">536</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.55s</td><td align="right">59.9G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.55s (-0.0%)</td><td align="right">59.9G (-0.01%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">8.01s</td><td align="right">60.7G</td><td align="right">173</td><td align="right">2</td><td align="right">6.860084625 (-0.0993%)</td><td align="right">6.20s (-22.6%)</td><td align="right">61.3G (+0.92%)</td><td align="right">176</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">6.38s</td><td align="right">59.0G</td><td align="right">303</td><td align="right">2</td><td align="right">6.880795013 (+0.1965%)</td><td align="right">6.22s (-2.5%)</td><td align="right">58.6G (-0.62%)</td><td align="right">283</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">8.93s</td><td align="right">81.3G</td><td align="right">448</td><td align="right">2</td><td align="right">6.892138569 (+0.1041%)</td><td align="right">8.62s (-3.4%)</td><td align="right">79.6G (-2.12%)</td><td align="right">431</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">10.2s</td><td align="right">82.8G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">9.51s (-6.8%)</td><td align="right">82.8G (+0.05%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">9.65s</td><td align="right">86.5G</td><td align="right">919</td><td align="right">2</td><td align="right">6.87278322 (-0.2126%)</td><td align="right">9.68s (+0.2%)</td><td align="right">86.7G (+0.20%)</td><td align="right">868</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">6.773456578</td><td align="right">3.31s</td><td align="right">33.3G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">3.27s (-1.0%)</td><td align="right">33.3G (-0.01%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.851498646</td><td align="right">4.83s</td><td align="right">44.7G</td><td align="right">617</td><td align="right">4</td><td align="right">5.851498646 (=)</td><td align="right">4.72s (-2.4%)</td><td align="right">44.7G (-0.01%)</td><td align="right">617</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.873775655</td><td align="right">5.47s</td><td align="right">48.0G</td><td align="right">1200</td><td align="right">4</td><td align="right">4.873775655 (=)</td><td align="right">5.42s (-1.0%)</td><td align="right">48.1G (+0.06%)</td><td align="right">1200</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.150346795</td><td align="right">5.87s</td><td align="right">51.4G</td><td align="right">2171</td><td align="right">4</td><td align="right">4.150346795 (=)</td><td align="right">5.85s (-0.4%)</td><td align="right">51.4G (+0.05%)</td><td align="right">2171</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.617750514</td><td align="right">6.19s</td><td align="right">51.9G</td><td align="right">3050</td><td align="right">4</td><td align="right">3.617750514 (=)</td><td align="right">6.03s (-2.5%)</td><td align="right">51.9G (-0.03%)</td><td align="right">3050</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.201872271</td><td align="right">7.63s</td><td align="right">65.4G</td><td align="right">3424</td><td align="right">6</td><td align="right">3.201872271 (=)</td><td align="right">7.57s (-0.7%)</td><td align="right">65.4G (+0.00%)</td><td align="right">3424</td><td align="right">6</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.883308449</td><td align="right">8.10s</td><td align="right">70.0G</td><td align="right">4367</td><td align="right">5</td><td align="right">2.883308449 (=)</td><td align="right">8.08s (-0.1%)</td><td align="right">70.0G (-0.04%)</td><td align="right">4367</td><td align="right">5</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.715s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.715s (+0.0%)</td><td align="right">7.2G (+0.00%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Old vs new columnar — two-level (`-C -2 -N10`)

Overlapping and wikispeedia rows as `-C -2d -N10`. The same rows move as in the standard search (above),
and everything else is bit-identical.

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
<tr><td align="right">ninetriangles</td><td align="right">3.517754809</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">9</td><td align="right">2</td><td align="right">3.517754809 (=)</td><td align="right">0.000s (+0.0%)</td><td align="right">0.1G (+0.00%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">jazz</td><td align="right">6.861229775</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (=)</td><td align="right">0.007s (-1.2%)</td><td align="right">0.1G (-1.36%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscience</td><td align="right">4.283072584</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">59</td><td align="right">2</td><td align="right">3.529536656 (-17.5933%)</td><td align="right">0.020s (+130.4%)</td><td align="right">0.3G (+76.21%)</td><td align="right">312</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.63729688</td><td align="right">0.099s</td><td align="right">1.1G</td><td align="right">419</td><td align="right">2</td><td align="right">5.634871397 (-0.0430%)</td><td align="right">0.101s (+2.2%)</td><td align="right">1.2G (+1.20%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">polblogs</td><td align="right">6.739575295</td><td align="right">0.043s</td><td align="right">0.5G</td><td align="right">81</td><td align="right">2</td><td align="right">7.592588015 (+12.6568%)</td><td align="right">0.068s (+59.4%)</td><td align="right">0.8G (+56.91%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">word_assoc</td><td align="right">7.949978834</td><td align="right">2.32s</td><td align="right">23.8G</td><td align="right">506</td><td align="right">2</td><td align="right">11.42993396 (+43.7731%)</td><td align="right">3.09s (+33.4%)</td><td align="right">34.9G (+46.15%)</td><td align="right">764</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.754216663</td><td align="right">19.3s</td><td align="right">117.5G</td><td align="right">11991</td><td align="right">2</td><td align="right">6.889480271 (+2.0027%)</td><td align="right">15.7s (-18.4%)</td><td align="right">95.3G (-18.89%)</td><td align="right">10362</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.004s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (+0.0%)</td><td align="right">0.1G (+0.00%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (+0.0%)</td><td align="right">0.1G (+0.00%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.400445378</td><td align="right">2.79s</td><td align="right">32.3G</td><td align="right">168</td><td align="right">2</td><td align="right">7.400445378 (=)</td><td align="right">2.84s (+1.5%)</td><td align="right">32.3G (+0.01%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393055049</td><td align="right">3.92s</td><td align="right">41.8G</td><td align="right">334</td><td align="right">2</td><td align="right">5.39136505 (-0.0313%)</td><td align="right">3.70s (-5.6%)</td><td align="right">42.4G (+1.65%)</td><td align="right">336</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571539329</td><td align="right">3.89s</td><td align="right">41.2G</td><td align="right">304</td><td align="right">2</td><td align="right">5.57079319 (-0.0134%)</td><td align="right">3.86s (-0.6%)</td><td align="right">40.8G (-0.94%)</td><td align="right">305</td><td align="right">2</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.424143707</td><td align="right">10.7s</td><td align="right">104.5G</td><td align="right">2237</td><td align="right">2</td><td align="right">7.416311014 (-0.1055%)</td><td align="right">10.5s (-2.3%)</td><td align="right">102.9G (-1.51%)</td><td align="right">2331</td><td align="right">2</td></tr>
<tr><td align="right">word_assoc (pref.)</td><td align="right">8.235585529</td><td align="right">3.06s</td><td align="right">31.5G</td><td align="right">25</td><td align="right">2</td><td align="right">11.73480892 (+42.4891%)</td><td align="right">3.98s (+30.0%)</td><td align="right">45.4G (+43.82%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.740761645</td><td align="right">3.61s</td><td align="right">35.6G</td><td align="right">625</td><td align="right">2</td><td align="right">6.752836583 (+0.1791%)</td><td align="right">3.34s (-7.6%)</td><td align="right">33.1G (-6.95%)</td><td align="right">536</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.822832994</td><td align="right">6.88s</td><td align="right">77.3G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">6.92s (+0.6%)</td><td align="right">77.3G (-0.02%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.866901786</td><td align="right">7.33s</td><td align="right">70.6G</td><td align="right">173</td><td align="right">2</td><td align="right">6.859843831 (-0.1028%)</td><td align="right">7.45s (+1.7%)</td><td align="right">71.7G (+1.44%)</td><td align="right">173</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.867301407</td><td align="right">7.95s</td><td align="right">68.4G</td><td align="right">303</td><td align="right">2</td><td align="right">6.880795013 (+0.1965%)</td><td align="right">7.61s (-4.3%)</td><td align="right">66.8G (-2.33%)</td><td align="right">283</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.88496897</td><td align="right">7.65s</td><td align="right">69.0G</td><td align="right">448</td><td align="right">2</td><td align="right">6.893434836 (+0.1230%)</td><td align="right">7.60s (-0.7%)</td><td align="right">68.2G (-1.09%)</td><td align="right">440</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.88992089</td><td align="right">7.81s</td><td align="right">69.2G</td><td align="right">663</td><td align="right">2</td><td align="right">6.88992089 (=)</td><td align="right">7.86s (+0.7%)</td><td align="right">69.2G (+0.00%)</td><td align="right">663</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.88742315</td><td align="right">8.90s</td><td align="right">74.5G</td><td align="right">919</td><td align="right">2</td><td align="right">6.873356654 (-0.2042%)</td><td align="right">8.40s (-5.7%)</td><td align="right">72.6G (-2.52%)</td><td align="right">848</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 E100000 `-2d`</td><td align="right">6.773456578</td><td align="right">3.98s</td><td align="right">37.8G</td><td align="right">60</td><td align="right">2</td><td align="right">6.773456578 (=)</td><td align="right">4.01s (+0.7%)</td><td align="right">37.8G (+0.03%)</td><td align="right">60</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 E50000 `-2d`</td><td align="right">6.258611497</td><td align="right">4.43s</td><td align="right">36.8G</td><td align="right">3236</td><td align="right">2</td><td align="right">6.258611497 (=)</td><td align="right">4.37s (-1.3%)</td><td align="right">36.8G (-0.01%)</td><td align="right">3236</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 E50000 `-2d`</td><td align="right">5.454385527</td><td align="right">6.90s</td><td align="right">33.8G</td><td align="right">4381</td><td align="right">2</td><td align="right">5.454385527 (=)</td><td align="right">4.59s (-33.5%)</td><td align="right">33.7G (-0.28%)</td><td align="right">4381</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 E50000 `-2d`</td><td align="right">4.903270512</td><td align="right">4.40s</td><td align="right">31.9G</td><td align="right">5443</td><td align="right">2</td><td align="right">4.903270512 (=)</td><td align="right">4.35s (-1.3%)</td><td align="right">31.9G (+0.02%)</td><td align="right">5443</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 E50000 `-2d`</td><td align="right">4.468778176</td><td align="right">4.46s</td><td align="right">32.4G</td><td align="right">6379</td><td align="right">2</td><td align="right">4.468778176 (=)</td><td align="right">4.45s (-0.2%)</td><td align="right">32.4G (-0.01%)</td><td align="right">6379</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 E50000 `-2d`</td><td align="right">4.143277395</td><td align="right">4.99s</td><td align="right">31.2G</td><td align="right">7097</td><td align="right">2</td><td align="right">4.143277395 (=)</td><td align="right">4.71s (-5.6%)</td><td align="right">31.2G (-0.06%)</td><td align="right">7097</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 E50000 `-2d`</td><td align="right">3.859069372</td><td align="right">4.47s</td><td align="right">31.6G</td><td align="right">7985</td><td align="right">2</td><td align="right">3.859069372 (=)</td><td align="right">4.52s (+1.1%)</td><td align="right">31.6G (+0.05%)</td><td align="right">7985</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.907904741</td><td align="right">0.735s</td><td align="right">6.9G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.735s (+0.0%)</td><td align="right">6.9G (+0.00%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### Single-trial runs (`-C -N1`)

Interleaved minimum of 3 per arm. jazz, powergrid, web-NotreDame, the three air30k rows, the
unseeded om rows and the replaced networks move (above). The `-d -N1` rows of om2 / om5–om8 still return a refined hierarchical
build 0.9–12.2% in bits above `-2d -N1`, the `-N1` property #1121 documents (F59); it was 1–14% on the
old draws.

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
<tr><td align="right">ninetriangles</td><td align="right">3.371875026</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">5</td><td align="right">3</td><td align="right">3.371875026 (=)</td><td align="right">0.000s (+0.0%)</td><td align="right">0.1G (+0.00%)</td><td align="right">5</td><td align="right">3</td></tr>
<tr><td align="right">jazz</td><td align="right">6.899367957</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">11</td><td align="right">2</td><td align="right">6.896294162 (-0.0446%)</td><td align="right">0.001s (+4.5%)</td><td align="right">0.1G (-3.20%)</td><td align="right">10</td><td align="right">2</td></tr>
<tr><td align="right">netscience</td><td align="right">4.047459862</td><td align="right">0.003s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">4</td><td align="right">3.364566019 (-16.8722%)</td><td align="right">0.007s (+172.3%)</td><td align="right">0.1G (+61.62%)</td><td align="right">278</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.730850312</td><td align="right">0.026s</td><td align="right">0.4G</td><td align="right">12</td><td align="right">5</td><td align="right">4.729861209 (-0.0209%)</td><td align="right">0.025s (-1.6%)</td><td align="right">0.4G (-0.77%)</td><td align="right">17</td><td align="right">5</td></tr>
<tr><td align="right">polblogs</td><td align="right">6.758265349</td><td align="right">0.009s</td><td align="right">0.2G</td><td align="right">3</td><td align="right">3</td><td align="right">7.593689164 (+12.3615%)</td><td align="right">0.015s (+59.5%)</td><td align="right">0.3G (+37.61%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">word_assoc</td><td align="right">7.807937174</td><td align="right">0.410s</td><td align="right">5.7G</td><td align="right">189</td><td align="right">3</td><td align="right">11.70862241 (+49.9579%)</td><td align="right">0.272s (-33.6%)</td><td align="right">3.4G (-40.29%)</td><td align="right">1817</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">5.556421705</td><td align="right">2.42s</td><td align="right">24.4G</td><td align="right">5</td><td align="right">6</td><td align="right">6.218856777 (+11.9220%)</td><td align="right">2.49s (+2.9%)</td><td align="right">25.1G (+2.81%)</td><td align="right">399</td><td align="right">5</td></tr>
<tr><td align="right">lazega</td><td align="right">6.041117399</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.041117399 (=)</td><td align="right">0.001s (+0.0%)</td><td align="right">0.1G (+0.00%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.000s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (+0.0%)</td><td align="right">0.1G (+0.00%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.491980364</td><td align="right">0.374s</td><td align="right">5.3G</td><td align="right">148</td><td align="right">3</td><td align="right">7.491980364 (=)</td><td align="right">0.371s (-0.7%)</td><td align="right">5.3G (-0.05%)</td><td align="right">148</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.470440768</td><td align="right">0.374s</td><td align="right">4.5G</td><td align="right">242</td><td align="right">3</td><td align="right">5.456991462 (-0.2459%)</td><td align="right">0.412s (+10.2%)</td><td align="right">4.7G (+3.41%)</td><td align="right">203</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.657913279</td><td align="right">0.489s</td><td align="right">5.9G</td><td align="right">197</td><td align="right">3</td><td align="right">5.666386425 (+0.1498%)</td><td align="right">0.468s (-4.4%)</td><td align="right">5.6G (-3.91%)</td><td align="right">203</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.546335898</td><td align="right">0.892s</td><td align="right">9.2G</td><td align="right">1614</td><td align="right">3</td><td align="right">7.662468548 (+1.5389%)</td><td align="right">0.738s (-17.3%)</td><td align="right">7.7G (-17.03%)</td><td align="right">1680</td><td align="right">3</td></tr>
<tr><td align="right">word_assoc (pref.)</td><td align="right">8.460796773</td><td align="right">0.412s</td><td align="right">5.8G</td><td align="right">5</td><td align="right">3</td><td align="right">11.94886955 (+41.2263%)</td><td align="right">0.420s (+1.8%)</td><td align="right">4.4G (-24.03%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-2d`</td><td align="right">6.739607071</td><td align="right">1.30s</td><td align="right">14.0G</td><td align="right">669</td><td align="right">2</td><td align="right">6.744558723 (+0.0735%)</td><td align="right">1.20s (-7.8%)</td><td align="right">13.0G (-7.37%)</td><td align="right">606</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d`</td><td align="right">6.823562921</td><td align="right">1.12s</td><td align="right">12.9G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.13s (+0.6%)</td><td align="right">12.9G (+0.03%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d`</td><td align="right">6.861732911</td><td align="right">1.50s</td><td align="right">15.7G</td><td align="right">139</td><td align="right">2</td><td align="right">6.858825005 (-0.0424%)</td><td align="right">1.35s (-10.5%)</td><td align="right">13.5G (-14.52%)</td><td align="right">179</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d`</td><td align="right">6.868180827</td><td align="right">1.70s</td><td align="right">17.2G</td><td align="right">291</td><td align="right">2</td><td align="right">6.948978287 (+1.1764%)</td><td align="right">1.62s (-4.9%)</td><td align="right">15.9G (-7.70%)</td><td align="right">309</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d`</td><td align="right">6.884236095</td><td align="right">1.69s</td><td align="right">16.3G</td><td align="right">453</td><td align="right">2</td><td align="right">6.894797134 (+0.1534%)</td><td align="right">1.56s (-7.5%)</td><td align="right">15.2G (-7.21%)</td><td align="right">447</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d`</td><td align="right">6.889674586</td><td align="right">1.80s</td><td align="right">17.2G</td><td align="right">649</td><td align="right">2</td><td align="right">6.889674586 (=)</td><td align="right">1.80s (+0.1%)</td><td align="right">17.2G (-0.02%)</td><td align="right">649</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d`</td><td align="right">6.894258582</td><td align="right">1.97s</td><td align="right">17.9G</td><td align="right">907</td><td align="right">2</td><td align="right">6.876045079 (-0.2642%)</td><td align="right">1.99s (+1.3%)</td><td align="right">18.6G (+3.90%)</td><td align="right">864</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.29196634</td><td align="right">0.382s</td><td align="right">3.7G</td><td align="right">321</td><td align="right">4</td><td align="right">7.295306496 (+0.0458%)</td><td align="right">0.364s (-4.8%)</td><td align="right">3.7G (-0.01%)</td><td align="right">268</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.823562921</td><td align="right">1.54s</td><td align="right">17.0G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.48s (-3.5%)</td><td align="right">17.0G (-0.04%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.861732911</td><td align="right">1.95s</td><td align="right">20.7G</td><td align="right">139</td><td align="right">2</td><td align="right">6.858825005 (-0.0424%)</td><td align="right">1.80s (-7.5%)</td><td align="right">18.8G (-9.53%)</td><td align="right">179</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.812252898</td><td align="right">0.758s</td><td align="right">7.2G</td><td align="right">66</td><td align="right">4</td><td align="right">7.795491139 (-0.2146%)</td><td align="right">0.749s (-1.2%)</td><td align="right">7.1G (-1.22%)</td><td align="right">80</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.440161581</td><td align="right">0.804s</td><td align="right">7.5G</td><td align="right">92</td><td align="right">4</td><td align="right">7.478431518 (+0.5144%)</td><td align="right">0.783s (-2.6%)</td><td align="right">7.3G (-2.47%)</td><td align="right">112</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.192756466</td><td align="right">0.807s</td><td align="right">7.5G</td><td align="right">149</td><td align="right">4</td><td align="right">7.192756466 (=)</td><td align="right">0.800s (-0.9%)</td><td align="right">7.5G (+0.03%)</td><td align="right">149</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.967535764</td><td align="right">0.805s</td><td align="right">7.5G</td><td align="right">206</td><td align="right">4</td><td align="right">6.938347922 (-0.4189%)</td><td align="right">0.830s (+3.0%)</td><td align="right">7.8G (+4.39%)</td><td align="right">186</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om2 E100000 `-d`</td><td align="right">7.144517469</td><td align="right">0.431s</td><td align="right">4.6G</td><td align="right">3</td><td align="right">3</td><td align="right">7.144517469 (=)</td><td align="right">0.429s (-0.4%)</td><td align="right">4.6G (-0.01%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om3 E50000 `-d`</td><td align="right">5.854829799</td><td align="right">0.442s</td><td align="right">4.4G</td><td align="right">537</td><td align="right">4</td><td align="right">5.854829799 (=)</td><td align="right">0.463s (+4.8%)</td><td align="right">4.4G (+0.12%)</td><td align="right">537</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om4 E50000 `-d`</td><td align="right">4.898784516</td><td align="right">0.448s</td><td align="right">4.2G</td><td align="right">1867</td><td align="right">3</td><td align="right">4.898784516 (=)</td><td align="right">0.441s (-1.4%)</td><td align="right">4.2G (+0.08%)</td><td align="right">1867</td><td align="right">3</td></tr>
<tr><td align="right">overlapping om5 E50000 `-d`</td><td align="right">4.1554786</td><td align="right">0.552s</td><td align="right">5.0G</td><td align="right">2156</td><td align="right">4</td><td align="right">4.1554786 (=)</td><td align="right">0.546s (-1.1%)</td><td align="right">5.0G (-0.13%)</td><td align="right">2156</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 E50000 `-d`</td><td align="right">3.620157111</td><td align="right">0.687s</td><td align="right">6.0G</td><td align="right">3056</td><td align="right">4</td><td align="right">3.620157111 (=)</td><td align="right">0.668s (-2.7%)</td><td align="right">5.9G (-0.17%)</td><td align="right">3056</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 E50000 `-d`</td><td align="right">3.21639819</td><td align="right">0.639s</td><td align="right">5.6G</td><td align="right">3417</td><td align="right">5</td><td align="right">3.21639819 (=)</td><td align="right">0.630s (-1.4%)</td><td align="right">5.6G (+0.04%)</td><td align="right">3417</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om8 E50000 `-d`</td><td align="right">2.885785338</td><td align="right">0.924s</td><td align="right">8.1G</td><td align="right">4371</td><td align="right">5</td><td align="right">2.885785338 (=)</td><td align="right">0.912s (-1.3%)</td><td align="right">8.1G (+0.08%)</td><td align="right">4371</td><td align="right">5</td></tr>
<tr><td align="right">overlapping om2 `-2d -c` planted</td><td align="right">6.744721993</td><td align="right">0.560s</td><td align="right">6.2G</td><td align="right">476</td><td align="right">2</td><td align="right">6.734990605 (-0.1443%)</td><td align="right">0.573s (+2.3%)</td><td align="right">6.3G (+1.93%)</td><td align="right">454</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-2d -c` planted</td><td align="right">6.820929855</td><td align="right">0.413s</td><td align="right">4.3G</td><td align="right">50</td><td align="right">2</td><td align="right">6.820929855 (=)</td><td align="right">0.398s (-3.6%)</td><td align="right">4.3G (-0.07%)</td><td align="right">50</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-2d -c` planted</td><td align="right">6.856285862</td><td align="right">0.705s</td><td align="right">7.3G</td><td align="right">129</td><td align="right">2</td><td align="right">6.850471471 (-0.0848%)</td><td align="right">0.899s (+27.5%)</td><td align="right">9.5G (+29.81%)</td><td align="right">126</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-2d -c` planted</td><td align="right">6.857876353</td><td align="right">0.921s</td><td align="right">9.5G</td><td align="right">287</td><td align="right">2</td><td align="right">6.867718448 (+0.1435%)</td><td align="right">0.947s (+2.8%)</td><td align="right">9.8G (+3.95%)</td><td align="right">290</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-2d -c` planted</td><td align="right">6.873464257</td><td align="right">1.11s</td><td align="right">11.3G</td><td align="right">444</td><td align="right">2</td><td align="right">6.87656878 (+0.0452%)</td><td align="right">0.968s (-13.1%)</td><td align="right">9.7G (-13.90%)</td><td align="right">430</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-2d -c` planted</td><td align="right">6.881554086</td><td align="right">1.06s</td><td align="right">9.6G</td><td align="right">624</td><td align="right">2</td><td align="right">6.881554086 (=)</td><td align="right">0.969s (-8.1%)</td><td align="right">9.6G (+0.00%)</td><td align="right">624</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-2d -c` planted</td><td align="right">6.875540042</td><td align="right">1.26s</td><td align="right">11.8G</td><td align="right">885</td><td align="right">2</td><td align="right">6.863878689 (-0.1696%)</td><td align="right">1.29s (+2.3%)</td><td align="right">12.2G (+3.55%)</td><td align="right">829</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">6.066305904</td><td align="right">0.074s</td><td align="right">0.8G</td><td align="right">187</td><td align="right">3</td><td align="right">6.066305904 (=)</td><td align="right">0.074s (+0.0%)</td><td align="right">0.8G (+0.00%)</td><td align="right">187</td><td align="right">3</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.91901362</td><td align="right">0.094s</td><td align="right">1.0G</td><td align="right">184</td><td align="right">2</td><td align="right">5.91901362 (=)</td><td align="right">0.094s (+0.0%)</td><td align="right">1.0G (+0.00%)</td><td align="right">184</td><td align="right">2</td></tr>
</tbody>
</table>

### The overlapping family in full

Every configuration of the planted overlapping state networks, both arms, at both trigram densities
(F55). The five unseeded networks (om2 E50000, om4 / om5 / om6 / om8 E100000) are new draws and move
(above). The nine seeded ones are bit-identical on every row.

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
<tr><td align="right">om2 E100000 `-2d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">3.75s</td><td align="right">36.8G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.87s (+3.1%)</td><td align="right">36.8G (+0.02%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 E100000 `-d --regularized -N10`</td><td align="right">6.950176925</td><td align="right">3.34s</td><td align="right">33.5G</td><td align="right">9</td><td align="right">2</td><td align="right">6.950176925 (=)</td><td align="right">3.37s (+0.8%)</td><td align="right">33.5G (+0.00%)</td><td align="right">9</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.64s</td><td align="right">24.8G</td><td align="right">126</td><td align="right">2</td><td align="right">7.53510563 (-0.1804%)</td><td align="right">2.61s (-0.9%)</td><td align="right">24.6G (-0.78%)</td><td align="right">119</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-2d --regularized -N1`</td><td align="right">7.548863183</td><td align="right">0.739s</td><td align="right">7.2G</td><td align="right">121</td><td align="right">2</td><td align="right">7.930395953 (+5.0542%)</td><td align="right">0.331s (-55.2%)</td><td align="right">3.1G (-56.67%)</td><td align="right">107</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N10`</td><td align="right">7.548721547</td><td align="right">2.53s</td><td align="right">23.9G</td><td align="right">126</td><td align="right">2</td><td align="right">7.53510563 (-0.1804%)</td><td align="right">2.44s (-3.5%)</td><td align="right">23.9G (+0.08%)</td><td align="right">119</td><td align="right">2</td></tr>
<tr><td align="right">om2 `-d --regularized -N1`</td><td align="right">7.548863183</td><td align="right">0.868s</td><td align="right">8.5G</td><td align="right">121</td><td align="right">2</td><td align="right">7.930395953 (+5.0542%)</td><td align="right">0.476s (-45.1%)</td><td align="right">4.4G (-48.01%)</td><td align="right">107</td><td align="right">2</td></tr>
<tr><td align="right">om2 planted, `-2d --no-infomap -c`</td><td align="right">6.789039995</td><td align="right">0.055s</td><td align="right">0.6G</td><td align="right">8</td><td align="right">2</td><td align="right">6.778901842 (-0.1493%)</td><td align="right">0.049s (-11.4%)</td><td align="right">0.6G (+0.01%)</td><td align="right">8</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N10`</td><td align="right">7.925216272</td><td align="right">2.55s</td><td align="right">22.5G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">2.53s (-0.6%)</td><td align="right">22.5G (+0.03%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N10`</td><td align="right">7.925216272</td><td align="right">2.60s</td><td align="right">22.8G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">2.53s (-2.5%)</td><td align="right">22.8G (+0.01%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">5.84s</td><td align="right">48.0G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">6.06s (+3.9%)</td><td align="right">48.0G (+0.07%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-2d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">0.913s</td><td align="right">9.2G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">0.914s (+0.1%)</td><td align="right">9.2G (+0.02%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N10`</td><td align="right">7.260835207</td><td align="right">4.94s</td><td align="right">45.1G</td><td align="right">29</td><td align="right">2</td><td align="right">7.260835207 (=)</td><td align="right">4.78s (-3.4%)</td><td align="right">45.1G (-0.00%)</td><td align="right">29</td><td align="right">2</td></tr>
<tr><td align="right">om3 `-d --regularized -N1`</td><td align="right">7.261268486</td><td align="right">1.16s</td><td align="right">12.0G</td><td align="right">34</td><td align="right">2</td><td align="right">7.261268486 (=)</td><td align="right">1.17s (+0.7%)</td><td align="right">12.0G (+0.02%)</td><td align="right">34</td><td align="right">2</td></tr>
<tr><td align="right">om3 planted, `-2d --no-infomap -c`</td><td align="right">6.837980937</td><td align="right">0.098s</td><td align="right">0.9G</td><td align="right">12</td><td align="right">2</td><td align="right">6.837980937 (=)</td><td align="right">0.088s (-10.8%)</td><td align="right">0.9G (-0.06%)</td><td align="right">12</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N10`</td><td align="right">7.92621405</td><td align="right">2.73s</td><td align="right">23.0G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">2.76s (+1.3%)</td><td align="right">23.0G (+0.01%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N10`</td><td align="right">7.92621405</td><td align="right">2.76s</td><td align="right">22.9G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">2.77s (+0.2%)</td><td align="right">22.9G (+0.02%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N10`</td><td align="right">7.556894677</td><td align="right">5.46s</td><td align="right">48.9G</td><td align="right">79</td><td align="right">2</td><td align="right">7.534667081 (-0.2941%)</td><td align="right">5.44s (-0.3%)</td><td align="right">49.0G (+0.02%)</td><td align="right">72</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-2d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">0.967s</td><td align="right">9.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.535458803 (-0.2837%)</td><td align="right">1.08s (+11.4%)</td><td align="right">10.2G (+12.18%)</td><td align="right">71</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N10`</td><td align="right">7.556653713</td><td align="right">5.36s</td><td align="right">48.7G</td><td align="right">78</td><td align="right">2</td><td align="right">7.535170166 (-0.2843%)</td><td align="right">5.30s (-1.2%)</td><td align="right">46.8G (-3.73%)</td><td align="right">71</td><td align="right">2</td></tr>
<tr><td align="right">om4 `-d --regularized -N1`</td><td align="right">7.556894677</td><td align="right">1.22s</td><td align="right">11.7G</td><td align="right">79</td><td align="right">2</td><td align="right">7.535458803 (-0.2837%)</td><td align="right">1.31s (+7.5%)</td><td align="right">12.9G (+9.65%)</td><td align="right">71</td><td align="right">2</td></tr>
<tr><td align="right">om4 planted, `-2d --no-infomap -c`</td><td align="right">6.880650147</td><td align="right">0.110s</td><td align="right">1.0G</td><td align="right">16</td><td align="right">2</td><td align="right">6.873858104 (-0.0987%)</td><td align="right">0.106s (-4.1%)</td><td align="right">1.0G (+0.21%)</td><td align="right">16</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N10`</td><td align="right">7.956672505</td><td align="right">2.91s</td><td align="right">23.3G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">2.79s (-4.2%)</td><td align="right">23.3G (-0.05%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N10`</td><td align="right">7.956672505</td><td align="right">2.86s</td><td align="right">22.6G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">2.94s (+2.8%)</td><td align="right">22.6G (+0.05%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N10`</td><td align="right">7.968061942</td><td align="right">4.98s</td><td align="right">43.1G</td><td align="right">79</td><td align="right">2</td><td align="right">7.973194647 (+0.0644%)</td><td align="right">4.81s (-3.5%)</td><td align="right">40.9G (-4.94%)</td><td align="right">98</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-2d --regularized -N1`</td><td align="right">7.968629202</td><td align="right">0.889s</td><td align="right">7.8G</td><td align="right">79</td><td align="right">2</td><td align="right">7.974202977 (+0.0699%)</td><td align="right">0.706s (-20.6%)</td><td align="right">6.1G (-21.21%)</td><td align="right">93</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N10`</td><td align="right">7.968320007</td><td align="right">4.89s</td><td align="right">42.6G</td><td align="right">81</td><td align="right">2</td><td align="right">7.973146326 (+0.0606%)</td><td align="right">4.73s (-3.2%)</td><td align="right">41.0G (-3.86%)</td><td align="right">98</td><td align="right">2</td></tr>
<tr><td align="right">om5 `-d --regularized -N1`</td><td align="right">7.968629202</td><td align="right">1.13s</td><td align="right">10.5G</td><td align="right">79</td><td align="right">2</td><td align="right">7.974202977 (+0.0699%)</td><td align="right">0.962s (-15.1%)</td><td align="right">8.8G (-15.74%)</td><td align="right">93</td><td align="right">2</td></tr>
<tr><td align="right">om5 planted, `-2d --no-infomap -c`</td><td align="right">6.902222527</td><td align="right">0.137s</td><td align="right">1.1G</td><td align="right">20</td><td align="right">2</td><td align="right">6.911031376 (+0.1276%)</td><td align="right">0.117s (-14.6%)</td><td align="right">1.1G (+0.00%)</td><td align="right">20</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N10`</td><td align="right">7.944047825</td><td align="right">2.74s</td><td align="right">21.9G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">2.76s (+1.0%)</td><td align="right">21.9G (+0.03%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N10`</td><td align="right">7.944047825</td><td align="right">3.32s</td><td align="right">21.6G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">2.88s (-13.2%)</td><td align="right">21.6G (-0.05%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N10`</td><td align="right">7.982650944</td><td align="right">5.11s</td><td align="right">41.2G</td><td align="right">105</td><td align="right">2</td><td align="right">7.979716886 (-0.0368%)</td><td align="right">5.15s (+0.9%)</td><td align="right">42.0G (+1.85%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-2d --regularized -N1`</td><td align="right">7.982650944</td><td align="right">0.727s</td><td align="right">6.0G</td><td align="right">105</td><td align="right">2</td><td align="right">7.979716886 (-0.0368%)</td><td align="right">0.766s (+5.4%)</td><td align="right">6.7G (+10.27%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N10`</td><td align="right">7.984417755</td><td align="right">4.95s</td><td align="right">41.3G</td><td align="right">102</td><td align="right">2</td><td align="right">7.981023534 (-0.0425%)</td><td align="right">5.05s (+1.9%)</td><td align="right">42.0G (+1.57%)</td><td align="right">118</td><td align="right">2</td></tr>
<tr><td align="right">om6 `-d --regularized -N1`</td><td align="right">7.982650944</td><td align="right">0.989s</td><td align="right">8.7G</td><td align="right">105</td><td align="right">2</td><td align="right">7.979716886 (-0.0368%)</td><td align="right">1.05s (+6.4%)</td><td align="right">9.3G (+7.15%)</td><td align="right">121</td><td align="right">2</td></tr>
<tr><td align="right">om6 planted, `-2d --no-infomap -c`</td><td align="right">6.930934993</td><td align="right">0.130s</td><td align="right">1.1G</td><td align="right">24</td><td align="right">2</td><td align="right">6.936312189 (+0.0776%)</td><td align="right">0.122s (-6.5%)</td><td align="right">1.1G (-0.62%)</td><td align="right">24</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N10`</td><td align="right">7.957532546</td><td align="right">3.13s</td><td align="right">23.3G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">2.99s (-4.4%)</td><td align="right">23.3G (-0.01%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N10`</td><td align="right">7.957532546</td><td align="right">3.17s</td><td align="right">22.4G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">3.09s (-2.2%)</td><td align="right">22.4G (-0.00%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N10`</td><td align="right">7.948827768</td><td align="right">5.11s</td><td align="right">41.2G</td><td align="right">166</td><td align="right">2</td><td align="right">7.948827768 (=)</td><td align="right">5.11s (-0.0%)</td><td align="right">41.2G (-0.01%)</td><td align="right">166</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-2d --regularized -N1`</td><td align="right">7.950842444</td><td align="right">0.792s</td><td align="right">6.4G</td><td align="right">168</td><td align="right">2</td><td align="right">7.950842444 (=)</td><td align="right">0.741s (-6.4%)</td><td align="right">6.4G (+0.08%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N10`</td><td align="right">7.948827768</td><td align="right">4.93s</td><td align="right">41.2G</td><td align="right">166</td><td align="right">2</td><td align="right">7.948827768 (=)</td><td align="right">4.94s (+0.0%)</td><td align="right">41.2G (+0.03%)</td><td align="right">166</td><td align="right">2</td></tr>
<tr><td align="right">om7 `-d --regularized -N1`</td><td align="right">7.950842444</td><td align="right">1.02s</td><td align="right">9.1G</td><td align="right">168</td><td align="right">2</td><td align="right">7.950842444 (=)</td><td align="right">1.04s (+1.3%)</td><td align="right">9.1G (+0.06%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">om7 planted, `-2d --no-infomap -c`</td><td align="right">6.957516072</td><td align="right">0.166s</td><td align="right">1.3G</td><td align="right">28</td><td align="right">2</td><td align="right">6.957516072 (=)</td><td align="right">0.145s (-12.5%)</td><td align="right">1.3G (+0.19%)</td><td align="right">28</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N10`</td><td align="right">7.978376075</td><td align="right">3.04s</td><td align="right">22.6G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">2.88s (-5.2%)</td><td align="right">22.6G (-0.03%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N10`</td><td align="right">7.978376075</td><td align="right">2.87s</td><td align="right">21.8G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">2.86s (-0.3%)</td><td align="right">21.8G (+0.02%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N10`</td><td align="right">7.979831947</td><td align="right">5.12s</td><td align="right">40.7G</td><td align="right">221</td><td align="right">2</td><td align="right">7.984638887 (+0.0602%)</td><td align="right">5.05s (-1.5%)</td><td align="right">40.9G (+0.33%)</td><td align="right">203</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-2d --regularized -N1`</td><td align="right">7.983599366</td><td align="right">0.763s</td><td align="right">6.2G</td><td align="right">216</td><td align="right">2</td><td align="right">7.984638887 (+0.0130%)</td><td align="right">0.738s (-3.3%)</td><td align="right">6.1G (-0.28%)</td><td align="right">203</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N10`</td><td align="right">7.978630877</td><td align="right">5.04s</td><td align="right">41.3G</td><td align="right">236</td><td align="right">2</td><td align="right">7.985651781 (+0.0880%)</td><td align="right">4.91s (-2.6%)</td><td align="right">40.6G (-1.73%)</td><td align="right">203</td><td align="right">2</td></tr>
<tr><td align="right">om8 `-d --regularized -N1`</td><td align="right">7.983599366</td><td align="right">1.03s</td><td align="right">8.9G</td><td align="right">216</td><td align="right">2</td><td align="right">7.984638887 (+0.0130%)</td><td align="right">0.996s (-2.9%)</td><td align="right">8.8G (-0.60%)</td><td align="right">203</td><td align="right">2</td></tr>
<tr><td align="right">om8 planted, `-2d --no-infomap -c`</td><td align="right">6.98103476</td><td align="right">0.146s</td><td align="right">1.2G</td><td align="right">32</td><td align="right">2</td><td align="right">6.966510195 (-0.2081%)</td><td align="right">0.137s (-5.9%)</td><td align="right">1.2G (+0.36%)</td><td align="right">32</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-2d --regularized -N1`</td><td align="right">7.925216272</td><td align="right">0.602s</td><td align="right">5.3G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">0.593s (-1.5%)</td><td align="right">5.3G (+0.11%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om3 E50000 `-d --regularized -N1`</td><td align="right">7.925216272</td><td align="right">0.741s</td><td align="right">6.6G</td><td align="right">89</td><td align="right">2</td><td align="right">7.925216272 (=)</td><td align="right">0.739s (-0.3%)</td><td align="right">6.6G (+0.04%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-2d --regularized -N1`</td><td align="right">7.92621405</td><td align="right">0.746s</td><td align="right">6.2G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">0.727s (-2.5%)</td><td align="right">6.2G (-0.00%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om4 E50000 `-d --regularized -N1`</td><td align="right">7.92621405</td><td align="right">0.880s</td><td align="right">7.6G</td><td align="right">110</td><td align="right">2</td><td align="right">7.92621405 (=)</td><td align="right">0.886s (+0.6%)</td><td align="right">7.6G (+0.06%)</td><td align="right">110</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-2d --regularized -N1`</td><td align="right">7.956672505</td><td align="right">0.752s</td><td align="right">6.3G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">0.749s (-0.4%)</td><td align="right">6.3G (+0.10%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om5 E50000 `-d --regularized -N1`</td><td align="right">7.956672505</td><td align="right">0.898s</td><td align="right">7.7G</td><td align="right">101</td><td align="right">2</td><td align="right">7.956672505 (=)</td><td align="right">0.871s (-3.0%)</td><td align="right">7.7G (-0.01%)</td><td align="right">101</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-2d --regularized -N1`</td><td align="right">7.944047825</td><td align="right">0.632s</td><td align="right">5.2G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">0.617s (-2.3%)</td><td align="right">5.2G (+0.04%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om6 E50000 `-d --regularized -N1`</td><td align="right">7.944047825</td><td align="right">0.686s</td><td align="right">5.6G</td><td align="right">94</td><td align="right">2</td><td align="right">7.944047825 (=)</td><td align="right">0.693s (+1.1%)</td><td align="right">5.6G (+0.07%)</td><td align="right">94</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-2d --regularized -N1`</td><td align="right">7.957532546</td><td align="right">0.711s</td><td align="right">5.8G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">0.701s (-1.5%)</td><td align="right">5.8G (+0.13%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om7 E50000 `-d --regularized -N1`</td><td align="right">7.957532546</td><td align="right">0.841s</td><td align="right">6.2G</td><td align="right">112</td><td align="right">2</td><td align="right">7.957532546 (=)</td><td align="right">0.764s (-9.1%)</td><td align="right">6.2G (+0.03%)</td><td align="right">112</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-2d --regularized -N1`</td><td align="right">7.978376075</td><td align="right">0.657s</td><td align="right">5.2G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">0.630s (-4.1%)</td><td align="right">5.2G (-0.01%)</td><td align="right">77</td><td align="right">2</td></tr>
<tr><td align="right">om8 E50000 `-d --regularized -N1`</td><td align="right">7.978376075</td><td align="right">0.690s</td><td align="right">5.5G</td><td align="right">77</td><td align="right">2</td><td align="right">7.978376075 (=)</td><td align="right">0.677s (-1.9%)</td><td align="right">5.5G (+0.07%)</td><td align="right">77</td><td align="right">2</td></tr>
</tbody>
</table>

### `-F` on the family, old vs new

Only the unseeded networks move (above). `-N10`:

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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.731808656</td><td align="right">3.96s</td><td align="right">40.3G</td><td align="right">690</td><td align="right">2</td><td align="right">6.752836583 (+0.3124%)</td><td align="right">3.45s (-12.7%)</td><td align="right">32.5G (-19.24%)</td><td align="right">536</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.23s</td><td align="right">55.5G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.28s (+1.0%)</td><td align="right">55.5G (+0.00%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.866901786</td><td align="right">6.01s</td><td align="right">56.0G</td><td align="right">173</td><td align="right">2</td><td align="right">6.860084625 (-0.0993%)</td><td align="right">5.93s (-1.4%)</td><td align="right">56.8G (+1.33%)</td><td align="right">176</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.867301407</td><td align="right">5.71s</td><td align="right">54.3G</td><td align="right">303</td><td align="right">2</td><td align="right">6.880795013 (+0.1965%)</td><td align="right">5.55s (-2.8%)</td><td align="right">53.8G (-0.89%)</td><td align="right">283</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.88496897</td><td align="right">7.58s</td><td align="right">66.6G</td><td align="right">448</td><td align="right">2</td><td align="right">6.892138569 (+0.1041%)</td><td align="right">7.20s (-5.1%)</td><td align="right">65.5G (-1.76%)</td><td align="right">431</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">8.43s</td><td align="right">69.7G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">8.06s (-4.4%)</td><td align="right">69.7G (+0.00%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.88742315</td><td align="right">9.58s</td><td align="right">74.8G</td><td align="right">919</td><td align="right">2</td><td align="right">6.87278322 (-0.2126%)</td><td align="right">8.41s (-12.2%)</td><td align="right">72.3G (-3.32%)</td><td align="right">868</td><td align="right">2</td></tr>
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
<tr><td align="right">overlapping om2 `-d`</td><td align="right">7.321678354</td><td align="right">0.272s</td><td align="right">2.6G</td><td align="right">436</td><td align="right">4</td><td align="right">7.317528373 (-0.0567%)</td><td align="right">0.266s (-2.1%)</td><td align="right">2.6G (-0.20%)</td><td align="right">338</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.823562921</td><td align="right">1.47s</td><td align="right">16.6G</td><td align="right">70</td><td align="right">2</td><td align="right">6.823562921 (=)</td><td align="right">1.45s (-1.6%)</td><td align="right">16.6G (+0.02%)</td><td align="right">70</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.861732911</td><td align="right">1.74s</td><td align="right">18.4G</td><td align="right">139</td><td align="right">2</td><td align="right">6.858825005 (-0.0424%)</td><td align="right">1.58s (-9.1%)</td><td align="right">16.3G (-11.55%)</td><td align="right">179</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">7.798283664</td><td align="right">0.658s</td><td align="right">5.8G</td><td align="right">96</td><td align="right">4</td><td align="right">7.829247983 (+0.3971%)</td><td align="right">0.555s (-15.6%)</td><td align="right">5.0G (-13.98%)</td><td align="right">93</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">7.507066407</td><td align="right">0.679s</td><td align="right">5.8G</td><td align="right">87</td><td align="right">4</td><td align="right">7.546115138 (+0.5202%)</td><td align="right">0.562s (-17.3%)</td><td align="right">4.9G (-16.07%)</td><td align="right">103</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">7.238675817</td><td align="right">0.611s</td><td align="right">5.2G</td><td align="right">145</td><td align="right">4</td><td align="right">7.238675817 (=)</td><td align="right">0.601s (-1.7%)</td><td align="right">5.2G (+0.03%)</td><td align="right">145</td><td align="right">4</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">7.022972054</td><td align="right">0.606s</td><td align="right">5.2G</td><td align="right">201</td><td align="right">4</td><td align="right">7.007657776 (-0.2181%)</td><td align="right">0.730s (+20.5%)</td><td align="right">6.1G (+16.82%)</td><td align="right">179</td><td align="right">4</td></tr>
</tbody>
</table>

### The fast dial `-F`

`-F` skips the interior-layer refinement. Both columns are the new input.

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
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.006s (-3.2%)</td><td align="right">0.1G (-0.66%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscience</td><td align="right">3.363918326</td><td align="right">0.066s</td><td align="right">0.8G</td><td align="right">278</td><td align="right">4</td><td align="right">3.364888761 (+0.0288%)</td><td align="right">0.038s (-42.1%)</td><td align="right">0.5G (-38.79%)</td><td align="right">283</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.713040685</td><td align="right">0.250s</td><td align="right">2.8G</td><td align="right">6</td><td align="right">5</td><td align="right">4.752564739 (+0.8386%)</td><td align="right">0.150s (-39.9%)</td><td align="right">1.8G (-37.15%)</td><td align="right">15</td><td align="right">5</td></tr>
<tr><td align="right">polblogs</td><td align="right">7.592773105</td><td align="right">0.082s</td><td align="right">0.9G</td><td align="right">91</td><td align="right">2</td><td align="right">7.592773105 (=)</td><td align="right">0.079s (-3.6%)</td><td align="right">0.9G (-2.96%)</td><td align="right">91</td><td align="right">2</td></tr>
<tr><td align="right">word_assoc</td><td align="right">11.43278494</td><td align="right">4.19s</td><td align="right">44.5G</td><td align="right">762</td><td align="right">2</td><td align="right">11.43278494 (=)</td><td align="right">3.48s (-16.9%)</td><td align="right">39.0G (-12.33%)</td><td align="right">762</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.174545892</td><td align="right">20.6s</td><td align="right">186.6G</td><td align="right">505</td><td align="right">6</td><td align="right">6.263424919 (+1.4394%)</td><td align="right">14.5s (-29.6%)</td><td align="right">120.0G (-35.69%)</td><td align="right">661</td><td align="right">4</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-12.0%)</td><td align="right">0.1G (-1.11%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (+0.6%)</td><td align="right">0.1G (-0.91%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.68s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.392442593 (=)</td><td align="right">2.57s (-4.1%)</td><td align="right">30.6G (-2.66%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.39323549</td><td align="right">3.56s</td><td align="right">38.6G</td><td align="right">260</td><td align="right">3</td><td align="right">5.39323549 (=)</td><td align="right">3.40s (-4.6%)</td><td align="right">35.9G (-7.00%)</td><td align="right">260</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578852599</td><td align="right">3.88s</td><td align="right">40.8G</td><td align="right">225</td><td align="right">3</td><td align="right">5.578852599 (=)</td><td align="right">3.42s (-12.0%)</td><td align="right">35.7G (-12.67%)</td><td align="right">225</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.414540615</td><td align="right">9.89s</td><td align="right">92.6G</td><td align="right">2238</td><td align="right">3</td><td align="right">7.414540615 (=)</td><td align="right">8.98s (-9.1%)</td><td align="right">90.1G (-2.78%)</td><td align="right">2238</td><td align="right">3</td></tr>
<tr><td align="right">word_assoc (pref.)</td><td align="right">11.73480892</td><td align="right">4.70s</td><td align="right">49.2G</td><td align="right">25</td><td align="right">2</td><td align="right">11.73480892 (=)</td><td align="right">4.35s (-7.6%)</td><td align="right">44.5G (-9.49%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om2 `-d`</td><td align="right">6.752836583</td><td align="right">3.76s</td><td align="right">38.5G</td><td align="right">536</td><td align="right">2</td><td align="right">6.752836583 (=)</td><td align="right">3.45s (-8.0%)</td><td align="right">32.5G (-15.40%)</td><td align="right">536</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om3 `-d`</td><td align="right">6.822832994</td><td align="right">5.55s</td><td align="right">59.9G</td><td align="right">69</td><td align="right">2</td><td align="right">6.822832994 (=)</td><td align="right">5.28s (-4.9%)</td><td align="right">55.5G (-7.24%)</td><td align="right">69</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om4 `-d`</td><td align="right">6.860084625</td><td align="right">6.20s</td><td align="right">61.3G</td><td align="right">176</td><td align="right">2</td><td align="right">6.860084625 (=)</td><td align="right">5.93s (-4.4%)</td><td align="right">56.8G (-7.33%)</td><td align="right">176</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om5 `-d`</td><td align="right">6.880795013</td><td align="right">6.22s</td><td align="right">58.6G</td><td align="right">283</td><td align="right">2</td><td align="right">6.880795013 (=)</td><td align="right">5.55s (-10.7%)</td><td align="right">53.8G (-8.23%)</td><td align="right">283</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om6 `-d`</td><td align="right">6.892138569</td><td align="right">8.62s</td><td align="right">79.6G</td><td align="right">431</td><td align="right">2</td><td align="right">6.892138569 (=)</td><td align="right">7.20s (-16.5%)</td><td align="right">65.5G (-17.72%)</td><td align="right">431</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om7 `-d`</td><td align="right">6.88945591</td><td align="right">9.51s</td><td align="right">82.8G</td><td align="right">666</td><td align="right">2</td><td align="right">6.88945591 (=)</td><td align="right">8.06s (-15.3%)</td><td align="right">69.7G (-15.78%)</td><td align="right">666</td><td align="right">2</td></tr>
<tr><td align="right">overlapping om8 `-d`</td><td align="right">6.87278322</td><td align="right">9.68s</td><td align="right">86.7G</td><td align="right">868</td><td align="right">2</td><td align="right">6.87278322 (=)</td><td align="right">8.41s (-13.0%)</td><td align="right">72.3G (-16.56%)</td><td align="right">868</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.715s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.907904741 (=)</td><td align="right">0.713s (-0.3%)</td><td align="right">6.9G (-4.03%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### The non-redundant map equation L\* (`--non-redundant`)

L\* is a different objective, so a lower number is not a better partition of the same objective. Both
columns are the new input.

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
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">6</td><td align="right">2</td><td align="right">6.858483287 (-0.0623%)</td><td align="right">0.007s (+2.8%)</td><td align="right">0.1G (+0.73%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscience</td><td align="right">3.363918326</td><td align="right">0.066s</td><td align="right">0.8G</td><td align="right">278</td><td align="right">4</td><td align="right">3.313421599 (-1.5011%)</td><td align="right">0.054s (-18.4%)</td><td align="right">0.7G (-17.24%)</td><td align="right">272</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.713040685</td><td align="right">0.250s</td><td align="right">2.8G</td><td align="right">6</td><td align="right">5</td><td align="right">4.50699368 (-4.3718%)</td><td align="right">0.253s (+1.2%)</td><td align="right">3.0G (+4.61%)</td><td align="right">3</td><td align="right">6</td></tr>
<tr><td align="right">polblogs</td><td align="right">7.592773105</td><td align="right">0.082s</td><td align="right">0.9G</td><td align="right">91</td><td align="right">2</td><td align="right">7.592668282 (-0.0014%)</td><td align="right">0.080s (-2.4%)</td><td align="right">0.9G (-0.98%)</td><td align="right">3</td><td align="right">3</td></tr>
<tr><td align="right">word_assoc</td><td align="right">11.43278494</td><td align="right">4.19s</td><td align="right">44.5G</td><td align="right">762</td><td align="right">2</td><td align="right">11.95700584 (+4.5852%)</td><td align="right">4.37s (+4.2%)</td><td align="right">48.5G (+9.13%)</td><td align="right">1789</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.174545892</td><td align="right">20.6s</td><td align="right">186.6G</td><td align="right">505</td><td align="right">6</td><td align="right">6.18859115 (+0.2275%)</td><td align="right">21.1s (+2.7%)</td><td align="right">182.3G (-2.34%)</td><td align="right">510</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.005s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">5.968624653 (-0.8182%)</td><td align="right">0.004s (-15.0%)</td><td align="right">0.1G (-0.50%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">1.928856578 (-4.1040%)</td><td align="right">0.001s (+8.9%)</td><td align="right">0.1G (+0.05%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.392442593</td><td align="right">2.68s</td><td align="right">31.5G</td><td align="right">164</td><td align="right">3</td><td align="right">7.438064454 (+0.6171%)</td><td align="right">2.96s (+10.7%)</td><td align="right">33.9G (+7.72%)</td><td align="right">2</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.39323549</td><td align="right">3.56s</td><td align="right">38.6G</td><td align="right">260</td><td align="right">3</td><td align="right">5.379735352 (-0.2503%)</td><td align="right">3.49s (-1.9%)</td><td align="right">38.6G (+0.01%)</td><td align="right">260</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.578852599</td><td align="right">3.88s</td><td align="right">40.8G</td><td align="right">225</td><td align="right">3</td><td align="right">5.571921279 (-0.1242%)</td><td align="right">3.83s (-1.3%)</td><td align="right">40.4G (-1.10%)</td><td align="right">227</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">7.414540615</td><td align="right">9.89s</td><td align="right">92.6G</td><td align="right">2238</td><td align="right">3</td><td align="right">7.228774064 (-2.5054%)</td><td align="right">9.07s (-8.3%)</td><td align="right">83.0G (-10.39%)</td><td align="right">2236</td><td align="right">3</td></tr>
<tr><td align="right">word_assoc (pref.)</td><td align="right">11.73480892</td><td align="right">4.70s</td><td align="right">49.2G</td><td align="right">25</td><td align="right">2</td><td align="right">12.7626516 (+8.7589%)</td><td align="right">4.79s (+1.8%)</td><td align="right">49.2G (+0.00%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.907904741</td><td align="right">0.715s</td><td align="right">7.2G</td><td align="right">199</td><td align="right">2</td><td align="right">5.903208727 (-0.0795%)</td><td align="right">0.683s (-4.5%)</td><td align="right">6.9G (-4.45%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar

OO cells are carried from the #1079-day session, except jazz, powergrid, web-NotreDame, malaria, the
three air30k rows and the replaced networks (netscience, polblogs, word_assoc and its preferred-modules
row), which this session measured on the new input (see above). The columnar cells are the
new column of the tables above. air30k (meta) OO is `-N1`, since it does not finish `-N10` in budget; on
the rebuild it did not finish `-N1` either (#1134).
- **Columnar ends above OO in bits** on air30k (reg.) by +0.14%, at −43% in seconds. On the original file
  columnar was 0.07% below OO; three seeds confirm the gap (#1135).
- **Columnar ends above OO in bits** on word_assoc (pref.) by +0.91%, at −64% in seconds: where the two
  engines charge the preferred-modules term (#1068). science2001 (pref.) was +3.74%.
- web-NotreDame, air30k, netscience, polblogs and word_assoc agree with OO within 0.07% in bits.

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
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.023s</td><td align="right">0.3G</td><td align="right">6</td><td align="right">2</td><td align="right">6.862755928 (=)</td><td align="right">0.007s (-71.3%)</td><td align="right">0.1G (-55.19%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscience</td><td align="right">3.364514863</td><td align="right">0.178s</td><td align="right">1.9G</td><td align="right">281</td><td align="right">5</td><td align="right">3.363918326 (-0.0177%)</td><td align="right">0.066s (-62.8%)</td><td align="right">0.8G (-57.77%)</td><td align="right">278</td><td align="right">4</td></tr>
<tr><td align="right">powergrid</td><td align="right">4.733338969</td><td align="right">1.96s</td><td align="right">20.0G</td><td align="right">10</td><td align="right">7</td><td align="right">4.713040685 (-0.4288%)</td><td align="right">0.250s (-87.3%)</td><td align="right">2.8G (-85.89%)</td><td align="right">6</td><td align="right">5</td></tr>
<tr><td align="right">polblogs</td><td align="right">7.593226496</td><td align="right">0.250s</td><td align="right">2.6G</td><td align="right">86</td><td align="right">3</td><td align="right">7.592773105 (-0.0060%)</td><td align="right">0.082s (-67.2%)</td><td align="right">0.9G (-63.72%)</td><td align="right">91</td><td align="right">2</td></tr>
<tr><td align="right">word_assoc</td><td align="right">11.4248698</td><td align="right">7.22s</td><td align="right">55.2G</td><td align="right">617</td><td align="right">3</td><td align="right">11.43278494 (+0.0693%)</td><td align="right">4.19s (-42.0%)</td><td align="right">44.5G (-19.42%)</td><td align="right">762</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.176776443</td><td align="right">169.1s</td><td align="right">1217.7G</td><td align="right">505</td><td align="right">9</td><td align="right">6.174545892 (-0.0361%)</td><td align="right">20.6s (-87.8%)</td><td align="right">186.6G (-84.67%)</td><td align="right">505</td><td align="right">6</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.005s (-61.8%)</td><td align="right">0.1G (-49.77%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.001s (-49.8%)</td><td align="right">0.1G (-37.18%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.502028585</td><td align="right">9.20s</td><td align="right">78.1G</td><td align="right">144</td><td align="right">3</td><td align="right">7.392442593 (-1.4608%)</td><td align="right">2.68s (-70.9%)</td><td align="right">31.5G (-59.70%)</td><td align="right">164</td><td align="right">3</td></tr>
<tr><td align="right">air30k</td><td align="right">5.391983421</td><td align="right">12.1s</td><td align="right">124.2G</td><td align="right">256</td><td align="right">3</td><td align="right">5.39323549 (+0.0232%)</td><td align="right">3.56s (-70.6%)</td><td align="right">38.6G (-68.93%)</td><td align="right">260</td><td align="right">3</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.57096117</td><td align="right">6.86s</td><td align="right">73.6G</td><td align="right">303</td><td align="right">3</td><td align="right">5.578852599 (+0.1417%)</td><td align="right">3.88s (-43.4%)</td><td align="right">40.8G (-44.51%)</td><td align="right">225</td><td align="right">3</td></tr>
<tr><td align="right">air30k (meta)</td><td align="right">did not finish, #1134</td><td align="right">&gt;9 h</td><td align="right">&gt;376474G</td><td align="right">—</td><td align="right">—</td><td align="right">7.414540615</td><td align="right">9.89s</td><td align="right">92.6G</td><td align="right">2238</td><td align="right">3</td></tr>
<tr><td align="right">word_assoc (pref.)</td><td align="right">11.62899476</td><td align="right">13.1s</td><td align="right">101.6G</td><td align="right">25</td><td align="right">2</td><td align="right">11.73480892 (+0.9099%)</td><td align="right">4.70s (-64.1%)</td><td align="right">49.2G (-51.58%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-d`</td><td align="right">5.88554258</td><td align="right">2.23s</td><td align="right">22.4G</td><td align="right">184</td><td align="right">3</td><td align="right">5.907904741 (+0.3800%)</td><td align="right">0.715s (-67.9%)</td><td align="right">7.2G (-67.94%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>

### OO vs columnar — two-level (`-2`)

Carried and measured as in the OO table above. Three of the changed rows put columnar more than 0.1%
above OO in bits:
- powergrid: +0.63%, at −81% in seconds. It was +0.66% on the old file, so it is the same gap on a
  relabelled network.
- web-NotreDame: +0.33%, at −65% in seconds. It was +0.17% on the DAG.
- word_assoc (pref.): +0.91%, at −66% in seconds. This is #1068, as in the OO table; science2001 (pref.)
  was +1.28%.

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
<tr><td align="right">jazz</td><td align="right">6.862755928</td><td align="right">0.012s</td><td align="right">0.2G</td><td align="right">6</td><td align="right">2</td><td align="right">6.861229775 (-0.0222%)</td><td align="right">0.007s (-44.4%)</td><td align="right">0.1G (-33.33%)</td><td align="right">6</td><td align="right">2</td></tr>
<tr><td align="right">netscience</td><td align="right">3.527414842</td><td align="right">0.093s</td><td align="right">1.0G</td><td align="right">311</td><td align="right">2</td><td align="right">3.529536656 (+0.0602%)</td><td align="right">0.020s (-78.5%)</td><td align="right">0.3G (-72.30%)</td><td align="right">312</td><td align="right">2</td></tr>
<tr><td align="right">powergrid</td><td align="right">5.599608906</td><td align="right">0.538s</td><td align="right">5.6G</td><td align="right">413</td><td align="right">2</td><td align="right">5.634871397 (+0.6297%)</td><td align="right">0.101s (-81.2%)</td><td align="right">1.2G (-79.23%)</td><td align="right">419</td><td align="right">2</td></tr>
<tr><td align="right">polblogs</td><td align="right">7.59324763</td><td align="right">0.126s</td><td align="right">1.3G</td><td align="right">86</td><td align="right">2</td><td align="right">7.592588015 (-0.0087%)</td><td align="right">0.068s (-46.0%)</td><td align="right">0.8G (-37.48%)</td><td align="right">89</td><td align="right">2</td></tr>
<tr><td align="right">word_assoc</td><td align="right">11.42488467</td><td align="right">5.17s</td><td align="right">42.5G</td><td align="right">617</td><td align="right">2</td><td align="right">11.42993396 (+0.0442%)</td><td align="right">3.09s (-40.1%)</td><td align="right">34.9G (-17.99%)</td><td align="right">764</td><td align="right">2</td></tr>
<tr><td align="right">web-NotreDame</td><td align="right">6.866831383</td><td align="right">45.2s</td><td align="right">237.8G</td><td align="right">10136</td><td align="right">2</td><td align="right">6.889480271 (+0.3298%)</td><td align="right">15.7s (-65.2%)</td><td align="right">95.3G (-59.92%)</td><td align="right">10362</td><td align="right">2</td></tr>
<tr><td align="right">lazega</td><td align="right">6.017860269</td><td align="right">0.007s</td><td align="right">0.1G</td><td align="right">7</td><td align="right">2</td><td align="right">6.017860269 (=)</td><td align="right">0.004s (-49.2%)</td><td align="right">0.1G (-3.02%)</td><td align="right">7</td><td align="right">2</td></tr>
<tr><td align="right">multilayer (ex.)</td><td align="right">2.011405238</td><td align="right">0.001s</td><td align="right">0.1G</td><td align="right">2</td><td align="right">2</td><td align="right">2.011405238 (=)</td><td align="right">0.000s (-57.6%)</td><td align="right">0.1G (-38.66%)</td><td align="right">2</td><td align="right">2</td></tr>
<tr><td align="right">malaria</td><td align="right">7.50595639</td><td align="right">6.45s</td><td align="right">55.2G</td><td align="right">142</td><td align="right">2</td><td align="right">7.400445378 (-1.4057%)</td><td align="right">2.84s (-56.0%)</td><td align="right">32.3G (-41.51%)</td><td align="right">168</td><td align="right">2</td></tr>
<tr><td align="right">air30k</td><td align="right">5.393899751</td><td align="right">4.39s</td><td align="right">42.1G</td><td align="right">328</td><td align="right">2</td><td align="right">5.39136505 (-0.0470%)</td><td align="right">3.70s (-15.8%)</td><td align="right">42.4G (+0.81%)</td><td align="right">336</td><td align="right">2</td></tr>
<tr><td align="right">air30k (reg.)</td><td align="right">5.571742293</td><td align="right">5.89s</td><td align="right">52.3G</td><td align="right">303</td><td align="right">2</td><td align="right">5.57079319 (-0.0170%)</td><td align="right">3.86s (-34.4%)</td><td align="right">40.8G (-21.93%)</td><td align="right">305</td><td align="right">2</td></tr>
<tr><td align="right">word_assoc (pref.)</td><td align="right">11.62899476</td><td align="right">11.6s</td><td align="right">86.2G</td><td align="right">25</td><td align="right">2</td><td align="right">11.73480892 (+0.9099%)</td><td align="right">3.98s (-65.7%)</td><td align="right">45.4G (-47.36%)</td><td align="right">25</td><td align="right">2</td></tr>
<tr><td align="right">wikispeedia `-2d`</td><td align="right">5.892212121</td><td align="right">1.70s</td><td align="right">17.2G</td><td align="right">184</td><td align="right">2</td><td align="right">5.907904741 (+0.2663%)</td><td align="right">0.735s (-56.8%)</td><td align="right">6.9G (-59.97%)</td><td align="right">199</td><td align="right">2</td></tr>
</tbody>
</table>
