## Performance

> **This PR replaces the objective behind `--non-redundant`.** It was `L*`, the sum of the non-redundant codebooks' *entropies*; it is now `L̂*`, what that code costs on the walk. `L*` is removed, not kept behind a second flag. Nothing else changes: no search, no move loop, no pass.
>
> So the evidence has two halves. **One row is meant to move** — `-C --non-redundant` — and the second table shows by how much, on every input. **Everything else must not move at all**, and the first table is what makes that falsifiable: 23 rows, 0e+00 bits.
>
> What the snapshot shows:
> - **The base map equation is bit-identical on every configuration measured, 0e+00 bits on 23 rows**, across undirected, directed, weighted, metadata, multilayer/memory and state input.
> - **`-C --non-redundant` moves on all 13 networks**, as intended, and the partition it finds moves with it — jazz 6 top modules → 9, word_assoc 1789 → 4, malaria 2 → 25, wikispeedia `-d` 199 → 58. The two numbers in that table are different objectives and neither being lower means anything; what it shows is that the new one runs on every input, flat and hierarchical, and costs the same order of time.
> - **Its per-module decomposition sums to its reported total** (asserted in memory at 1e-9 by `test_lhat_star_columnar.cpp`; the residuals in the table are `-o json`'s rounding of the per-module numbers, not a mismatch).
> - **No time change outside this machine's measurement noise.** word_assoc `-d -C` reads 14.84 s old against 8.36 s new in the sweep — a bit-identical configuration, so that is not the code. These runs were interleaved but the machine was loaded; the earlier sweep in this session put the same row at 6.23 s and 5.91 s. Reported rather than explained, because nothing in the diff reaches that path.
>
> Columnar arms measured in one session, 2026-11-02, interleaved per row, `--timing-json`'s `timing.total_s`, `MODE=release OPENMP=0`, `--seed 123`, `-N10`.
> - **Old:** a fresh build of the branch tip `75224e6f` (#1406), md5 `a8063e111289e95f473cd455e9e47c64`.
> - **New:** this branch, md5 `e0cdbf08186907ab4acb9b8bd923d11e`.
>
> **Not measured, and why:** air30k and air30k_usstate, whose inputs need the ~270 MB DB1B fetch, and the `--regularized` / preferred-modules families. Those run code paths this diff does not enter, and the rows below already cover each kind of input they would add. There is no per-feature attribution table because the PR has one feature.
>
> **The deleted section.** This file is a snapshot, not a changelog: `CLAUDE.md` says a merged sub-PR's section is *replaced*, and "delete sections belonging to already-merged sub-PRs rather than relabelling them". #1406 merged at the tip this branch starts from, so its ~830 lines come out here; they stay reachable on that PR's own page, where they are the measurement that justified it.

### Identity: the base map equation and every other objective, unchanged

| network | arm | old codelength | new codelength | delta (bits) | old t (s) | new t (s) |
|---|---|---:|---:|---:|---:|---:|
| ninetriangles | `-C` | 3.371875026385 | 3.371875026385 | 0e+00 | 0.005 | 0.006 |
| ninetriangles | `-C -2` | 3.517754809044 | 3.517754809044 | 0e+00 | 0.002 | 0.003 |
| jazz | `-C` | 6.862755928271 | 6.862755928271 | 0e+00 | 0.015 | 0.015 |
| jazz | `-C -2` | 6.861229774904 | 6.861229774904 | 0e+00 | 0.035 | 0.022 |
| netscience | `-C` | 3.364651183729 | 3.364651183729 | 0e+00 | 0.097 | 0.102 |
| netscience | `-C -2` | 3.529536656393 | 3.529536656393 | 0e+00 | 0.083 | 0.067 |
| powergrid | `-C` | 4.715327642900 | 4.715327642900 | 0e+00 | 0.825 | 0.636 |
| powergrid | `-C -2` | 5.634871396865 | 5.634871396865 | 0e+00 | 0.248 | 0.215 |
| polblogs (-d) | `-d -C` | 7.592773105208 | 7.592773105208 | 0e+00 | 0.133 | 0.119 |
| polblogs (-d) | `-d -C -2` | 7.592588015431 | 7.592588015431 | 0e+00 | 0.131 | 0.113 |
| word_assoc (-d) | `-d -C` | 11.432784943113 | 11.432784943113 | 0e+00 | 14.839 | 8.363 |
| word_assoc (-d) | `-d -C -2` | 11.429933956928 | 11.429933956928 | 0e+00 | 6.148 | 4.755 |
| lazega (metadata) | `--meta-data -C` | 6.017860269386 | 6.017860269386 | 0e+00 | 0.009 | 0.009 |
| lazega (metadata) | `--meta-data -C -2` | 6.017860269386 | 6.017860269386 | 0e+00 | 0.008 | 0.008 |
| malaria (multilayer) | `-C` | 7.392442593241 | 7.392442593241 | 0e+00 | 5.578 | 5.415 |
| malaria (multilayer) | `-C -2` | 7.400445378409 | 7.400445378409 | 0e+00 | 4.684 | 4.727 |
| multilayer | `-C` | 2.011405238446 | 2.011405238446 | 0e+00 | 0.002 | 0.006 |
| multilayer | `-C -2` | 2.011405238446 | 2.011405238446 | 0e+00 | 0.002 | 0.003 |
| wikispeedia | `-2 -d -C` | 5.907904741137 | 5.907904741137 | 0e+00 | 0.949 | 0.928 |
| wikispeedia | `-d -C` | 5.907904741137 | 5.907904741137 | 0e+00 | 0.987 | 1.005 |
| wikispeedia | `-d -C -2` | 5.907904741137 | 5.907904741137 | 0e+00 | 0.925 | 0.992 |
| om2_N256_E50k | `-2 -d -C` | 6.752836582515 | 6.752836582515 | 0e+00 | 4.164 | 4.230 |
| om5_N256_E100k | `-2 -d -C` | 6.880795013315 | 6.880795013315 | 0e+00 | 8.874 | 9.081 |

Worst codelength difference across every row: **0e+00 bits**.

### The row that moves: `-C --non-redundant`

| network | old (L*) | top | lvls | old t (s) | new | top | lvls | new t (s) | per-module sum residual |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| ninetriangles | 3.078067 | 3 | 3 | 0.003 | 3.077553 | 3 | 3 | 0.004 | 2e-06 |
| jazz | 6.858483 | 6 | 2 | 0.018 | 6.630005 | 9 | 2 | 0.020 | 1e-07 |
| netscience | 3.304490 | 272 | 4 | 0.093 | 3.260072 | 272 | 4 | 0.130 | 6e-07 |
| powergrid | 4.471280 | 4 | 7 | 0.431 | 4.331056 | 4 | 7 | 0.421 | 1e-07 |
| polblogs (-d) `-d` | 7.592668 | 3 | 3 | 0.121 | 7.376288 | 3 | 3 | 0.138 | 6e-06 |
| word_assoc (-d) `-d` | 11.957006 | 1789 | 2 | 8.987 | 11.096051 | 4 | 3 | 6.200 | 5e-06 |
| lazega (metadata) `--meta-data` | 5.968625 | 7 | 2 | 0.009 | 5.603695 | 7 | 2 | 0.009 | 3e-06 |
| malaria (multilayer) | 7.438064 | 2 | 3 | 4.731 | 7.555872 | 25 | 3 | 6.017 | 3e-06 |
| multilayer | 1.928857 | 2 | 2 | 0.002 | 1.938531 | 2 | 2 | 0.003 | 7e-07 |
| wikispeedia `-2 -d` | 5.898336 | 196 | 2 | 0.938 | 5.767934 | 195 | 2 | 0.827 | 5e-07 |
| wikispeedia `-d` | 5.903209 | 199 | 2 | 0.951 | 5.776867 | 58 | 3 | 0.839 | 7e-07 |
| om2_N256_E50k `-2 -d` | 6.745782 | 536 | 2 | 4.001 | 6.808949 | 528 | 2 | 4.111 | 7e-07 |
| om5_N256_E100k `-2 -d` | 6.884250 | 286 | 2 | 8.702 | 6.857222 | 271 | 2 | 8.383 | 2e-06 |

### Tests

`make test-native` passes 35/35 in all three configurations — `OPENMP=1`, `OPENMP=0`, and
`FEATURES="lossy-map-equation regularized-multilayer"`.
