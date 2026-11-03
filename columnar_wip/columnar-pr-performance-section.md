## Performance

> **This PR replaces the objective behind `--non-redundant`.** It was `L*`, the sum of the non-redundant codebooks' *entropies*; it is now the generative map equation, what that code costs on the walk. `L*` is removed, not kept behind a second flag. Nothing else changes: no search, no move loop, no pass.
>
> The evidence has two halves. **One row is meant to move** — `-C --non-redundant` — and the second table shows by how much, on every input. **Everything else must not move at all**, and the first table is what makes that falsifiable.
>
> What the snapshot shows:
> - **Every other configuration is unchanged in codelength, tree AND config fingerprint**: 0e+00 bits on 24 rows, across undirected, directed, weighted, metadata, multilayer/memory and state input, with `configFingerprint` identical on every one. (An earlier cut of this PR added a `non_redundant_score` key to `canonicalConfigJson`, which rotated the fingerprint for every run including the base map equation — required behaviour, but it made "bit-identical" too strong a word. The option is gone, and with it the rotation.)
> - **`-C --non-redundant` moves on all 12 networks**, as intended, and so does the partition it finds — jazz 6 top modules → 9, word_assoc 1789 → 2, malaria 2 → 25, wikispeedia 199 → 58. The two codelength columns are different objectives and neither being lower means anything, so no bits delta is shown for them; what the table shows is that the new objective runs on every input, flat and hierarchical, and costs the same order of time.
> - **The time column is noise at this machine's resolution and should not be read row by row.** The ±% spans −77% to +99% on configurations that are bit-identical and whose code path this diff does not enter, on rows of 0.002–0.008 s. Three repeats on powergrid `-C` spanned 0.403–0.545 s, which covers every sub-second gap in the table. Reported rather than explained, and not re-timed, because there is no mechanism in the diff that could reach those paths.
>
> Both arms measured in one session, 2026-11-02, interleaved per row, `--timing-json`'s `timing.total_s`, `-N10 --seed 123`, with the per-network flags from [`columnar_wip/benchmark-networks.md`](benchmark-networks.md) spelled out in the tables.
> - **Old:** a fresh build of the branch tip `75224e6f` (#1406), md5 `a8063e111289e95f473cd455e9e47c64`.
> - **New:** this branch, md5 `bb05de73a360434c8a85885e9efb11ff`.
> - Both built `MODE=release OPENMP=0` (object set `release-omp0-arch0-features-none`, checked on each).
>
> **Not measured, and why:** air30k and air30k_usstate, whose inputs need the ~270 MB DB1B fetch, and the `--regularized` and preferred-modules families. Those run code paths this diff does not enter, and the rows below already cover each kind of input they would add. There is no per-feature attribution table because the PR has one feature.
>
> **The deleted section.** This file is a snapshot, not a changelog: `CLAUDE.md` says a merged sub-PR's section is *replaced*, and "delete sections belonging to already-merged sub-PRs rather than relabelling them". #1406 merged at the tip this branch starts from, so its ~830 lines come out here; they stay reachable on that PR's own page, where they are the measurement that justified it.

### Identity: every configuration that is not `--non-redundant`

| network | flags | arm | old | new | delta (bits) | old t (s) | new t (s) | t ±% | fingerprint |
|---|---|---|---:|---:|---:|---:|---:|---:|---|
| ninetriangles | `` | `-C` | 3.371875026385 | 3.371875026385 | 0e+00 | 0.003 | 0.005 | +45.5% | same |
| ninetriangles | `` | `-C -2` | 3.517754809044 | 3.517754809044 | 0e+00 | 0.008 | 0.002 | -77.5% | same |
| jazz | `` | `-C` | 6.862755928271 | 6.862755928271 | 0e+00 | 0.013 | 0.013 | -1.3% | same |
| jazz | `` | `-C -2` | 6.861229774904 | 6.861229774904 | 0e+00 | 0.012 | 0.015 | +19.8% | same |
| netscience | `` | `-C` | 3.364651183729 | 3.364651183729 | 0e+00 | 0.082 | 0.080 | -2.4% | same |
| netscience | `` | `-C -2` | 3.529536656393 | 3.529536656393 | 0e+00 | 0.059 | 0.059 | -1.2% | same |
| powergrid | `` | `-C` | 4.715327642900 | 4.715327642900 | 0e+00 | 0.392 | 0.401 | +2.2% | same |
| powergrid | `` | `-C -2` | 5.634871396865 | 5.634871396865 | 0e+00 | 0.242 | 0.211 | -12.7% | same |
| polblogs | `-d` | `-C` | 7.592773105208 | 7.592773105208 | 0e+00 | 0.120 | 0.114 | -5.0% | same |
| polblogs | `-d` | `-C -2` | 7.592588015431 | 7.592588015431 | 0e+00 | 0.114 | 0.135 | +19.0% | same |
| word_assoc | `-d` | `-C` | 11.432784943113 | 11.432784943113 | 0e+00 | 7.541 | 8.247 | +9.4% | same |
| word_assoc | `-d` | `-C -2` | 11.429933956928 | 11.429933956928 | 0e+00 | 6.426 | 5.810 | -9.6% | same |
| lazega (metadata) | `--meta-data` | `-C` | 6.017860269386 | 6.017860269386 | 0e+00 | 0.009 | 0.009 | -3.5% | same |
| lazega (metadata) | `--meta-data` | `-C -2` | 6.017860269386 | 6.017860269386 | 0e+00 | 0.007 | 0.007 | +5.4% | same |
| malaria (multilayer) | `` | `-C` | 7.392442593241 | 7.392442593241 | 0e+00 | 7.171 | 7.055 | -1.6% | same |
| malaria (multilayer) | `` | `-C -2` | 7.400445378409 | 7.400445378409 | 0e+00 | 6.170 | 6.976 | +13.1% | same |
| multilayer | `` | `-C` | 2.011405238446 | 2.011405238446 | 0e+00 | 0.002 | 0.003 | +23.8% | same |
| multilayer | `` | `-C -2` | 2.011405238446 | 2.011405238446 | 0e+00 | 0.003 | 0.007 | +99.1% | same |
| wikispeedia | `-d` | `-C` | 5.907904741137 | 5.907904741137 | 0e+00 | 1.188 | 1.237 | +4.1% | same |
| wikispeedia | `-d` | `-C -2` | 5.907904741137 | 5.907904741137 | 0e+00 | 1.135 | 1.720 | +51.5% | same |
| om2_N256_E50k | `-d` | `-C` | 6.752836582515 | 6.752836582515 | 0e+00 | 5.819 | 5.907 | +1.5% | same |
| om2_N256_E50k | `-d` | `-C -2` | 6.752836582515 | 6.752836582515 | 0e+00 | 5.038 | 5.077 | +0.8% | same |
| om5_N256_E100k | `-d` | `-C` | 6.880795013315 | 6.880795013315 | 0e+00 | 11.457 | 11.058 | -3.5% | same |
| om5_N256_E100k | `-d` | `-C -2` | 6.880795013315 | 6.880795013315 | 0e+00 | 12.240 | 11.758 | -3.9% | same |

Worst codelength difference across every row: **0e+00 bits**. Config fingerprint identical on every row.

### The row that moves: `-C --non-redundant`

Not a comparison — the two columns are different objectives, and neither being lower says anything.

| network | flags | L* (old) | top | lvls | t (s) | generative (new) | top | lvls | t (s) | t ±% |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| ninetriangles | `` | 3.078067 | 3 | 3 | 0.004 | 3.077553 | 3 | 3 | 0.003 | -19.2% |
| jazz | `` | 6.858483 | 6 | 2 | 0.018 | 6.630005 | 9 | 2 | 0.013 | -27.2% |
| netscience | `` | 3.304490 | 272 | 4 | 0.107 | 3.260072 | 272 | 4 | 0.108 | +1.0% |
| powergrid | `` | 4.471280 | 4 | 7 | 0.488 | 4.331056 | 4 | 7 | 0.436 | -10.8% |
| polblogs | `-d` | 7.592668 | 3 | 3 | 0.126 | 7.472675 | 3 | 3 | 0.159 | +26.3% |
| word_assoc | `-d` | 11.957006 | 1789 | 2 | 11.471 | 10.922395 | 2 | 3 | 5.381 | -53.1% |
| lazega (metadata) | `--meta-data` | 5.968625 | 7 | 2 | 0.009 | 5.603695 | 7 | 2 | 0.008 | -11.6% |
| malaria (multilayer) | `` | 7.438064 | 2 | 3 | 4.826 | 7.349343 | 25 | 3 | 6.052 | +25.4% |
| multilayer | `` | 1.928857 | 2 | 2 | 0.002 | 1.938531 | 2 | 2 | 0.002 | -3.4% |
| wikispeedia | `-d` | 5.903209 | 199 | 2 | 1.015 | 5.765598 | 58 | 3 | 0.746 | -26.5% |
| om2_N256_E50k | `-d` | 6.745782 | 536 | 2 | 4.815 | 6.722820 | 536 | 2 | 4.632 | -3.8% |
| om5_N256_E100k | `-d` | 6.884250 | 286 | 2 | 10.157 | 6.860184 | 283 | 2 | 9.947 | -2.1% |

### Tests

`make test-native` passes 35/35 in all three configurations — `OPENMP=1`, `OPENMP=0`, and
`FEATURES="lossy-map-equation regularized-multilayer"`.
