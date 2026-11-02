## Performance

> **This PR makes `--non-redundant` report `L̂*`, what the non-redundant code costs on the walk, and keeps `L*` behind `--non-redundant-score entropy`.** It adds a second scorer beside `scoreStackLstar` and one line of dispatch; it changes no search, no move loop and no pass. So the evidence it owes is **identity on every path it does not mean to touch**, measured old-vs-new, rather than a codelength/time sweep looking for drift — there is no mechanism by which the base engine's arithmetic could have moved, and the table below is what makes that falsifiable.
>
> What the snapshot shows:
> - **Every unchanged configuration is bit-identical, 0e+00 bits on all 34 rows**, across undirected, directed, weighted, metadata, multilayer/memory and state input. That includes `-C --non-redundant` re-run as `-C --non-redundant --non-redundant-score entropy`: the entropy score reproduces the old `--non-redundant` number exactly, which is the claim that lets the pending depth comparison be a strict A/B on one binary.
> - **No time change outside the measurement noise of this machine.** The largest apparent gap in the sweep, powergrid `-C -N10` 0.328 s old against 0.508 s new, did not reproduce: re-measured interleaved, 3 runs each, old 0.226/0.365/0.290 s against new 0.242/0.230/0.229 s — a 0.14 s spread on a 0.23 s run, with new never outside the old arm's own range. Reported rather than explained, because nothing in the diff reaches that path.
> - **The new arm runs everywhere the old one did**, flat and hierarchical, and its per-module decomposition sums to its reported total (asserted in memory at 1e-9 by `test_lhat_star_columnar.cpp`, since `-o json` rounds the per-module numbers).
>
> Columnar arms measured in one session, 2026-11-02, interleaved per row, `--timing-json`'s `timing.total_s`, with no other benchmark or test suite running.
> - **Old:** a fresh build of the branch tip `75224e6f` (#1406), md5 `a8063e111289e95f473cd455e9e47c64`.
> - **New:** this branch, md5 `820c5e30da2462db92b8e8b2d1daf36f`.
> - Both `MODE=release OPENMP=0`, `--seed 123`.
>
> **Not measured, and why:** air30k and air30k_usstate, whose inputs need the ~270 MB DB1B fetch, and the `--regularized` / preferred-modules families. Every one of them runs a code path this diff does not enter, and the 34 rows below already cover each kind of input and objective they would add. There is no per-feature attribution table because the PR has one feature.

### Identity: old vs this branch, `-N10 --seed 123`

| network | arm | old codelength | new codelength | delta (bits) | old t (s) | new t (s) |
|---|---|---:|---:|---:|---:|---:|
| ninetriangles | `-C` | 3.371875026385 | 3.371875026385 | 0e+00 | 0.008 | 0.007 |
| ninetriangles | `-C -2` | 3.517754809044 | 3.517754809044 | 0e+00 | 0.002 | 0.003 |
| ninetriangles | `-C --non-redundant` (score entropy) | 3.078067323022 | 3.078067323022 | 0e+00 | 0.003 | 0.003 |
| jazz | `-C` | 6.862755928271 | 6.862755928271 | 0e+00 | 0.012 | 0.013 |
| jazz | `-C -2` | 6.861229774904 | 6.861229774904 | 0e+00 | 0.013 | 0.019 |
| jazz | `-C --non-redundant` (score entropy) | 6.858483286533 | 6.858483286533 | 0e+00 | 0.017 | 0.017 |
| netscience | `-C` | 3.364651183729 | 3.364651183729 | 0e+00 | 0.074 | 0.076 |
| netscience | `-C -2` | 3.529536656393 | 3.529536656393 | 0e+00 | 0.053 | 0.055 |
| netscience | `-C --non-redundant` (score entropy) | 3.304489766503 | 3.304489766503 | 0e+00 | 0.089 | 0.090 |
| powergrid | `-C` | 4.715327642900 | 4.715327642900 | 0e+00 | 0.328 | 0.508 |
| powergrid | `-C -2` | 5.634871396865 | 5.634871396865 | 0e+00 | 0.209 | 0.193 |
| powergrid | `-C --non-redundant` (score entropy) | 4.471279533063 | 4.471279533063 | 0e+00 | 0.398 | 0.393 |
| polblogs (`-d`) | `-C` | 7.592773105208 | 7.592773105208 | 0e+00 | 0.113 | 0.114 |
| polblogs (`-d`) | `-C -2` | 7.592588015431 | 7.592588015431 | 0e+00 | 0.120 | 0.109 |
| polblogs (`-d`) | `-C --non-redundant` (score entropy) | 7.592668282184 | 7.592668282184 | 0e+00 | 0.121 | 0.121 |
| word_assoc (`-d`) | `-C` | 11.432784943113 | 11.432784943113 | 0e+00 | 6.227 | 5.910 |
| word_assoc (`-d`) | `-C -2` | 11.429933956928 | 11.429933956928 | 0e+00 | 4.458 | 4.211 |
| word_assoc (`-d`) | `-C --non-redundant` (score entropy) | 11.957005837509 | 11.957005837509 | 0e+00 | 8.738 | 8.689 |
| lazega (metadata) | `-C` | 6.017860269386 | 6.017860269386 | 0e+00 | 0.008 | 0.008 |
| lazega (metadata) | `-C -2` | 6.017860269386 | 6.017860269386 | 0e+00 | 0.006 | 0.007 |
| lazega (metadata) | `-C --non-redundant` (score entropy) | 5.968624652712 | 5.968624652712 | 0e+00 | 0.007 | 0.008 |
| malaria (multilayer) | `-C` | 7.392442593241 | 7.392442593241 | 0e+00 | 4.893 | 5.074 |
| malaria (multilayer) | `-C -2` | 7.400445378409 | 7.400445378409 | 0e+00 | 4.442 | 4.607 |
| malaria (multilayer) | `-C --non-redundant` (score entropy) | 7.438064454032 | 7.438064454032 | 0e+00 | 4.694 | 4.631 |
| multilayer | `-C` | 2.011405238446 | 2.011405238446 | 0e+00 | 0.004 | 0.005 |
| multilayer | `-C --non-redundant` (score entropy) | 1.928856577758 | 1.928856577758 | 0e+00 | 0.002 | 0.003 |
| wikispeedia | `-2d -C` | 5.907904741137 | 5.907904741137 | 0e+00 | 0.855 | 0.900 |
| wikispeedia | `-2d -C --non-redundant` (score entropy) | 5.898336492688 | 5.898336492688 | 0e+00 | 0.786 | 0.787 |
| wikispeedia | `-d -C` | 5.907904741137 | 5.907904741137 | 0e+00 | 0.985 | 0.949 |
| wikispeedia | `-d -C --non-redundant` (score entropy) | 5.903208726500 | 5.903208726500 | 0e+00 | 0.953 | 0.908 |
| om2_N256_E50k | `-2d -C` | 6.752836582515 | 6.752836582515 | 0e+00 | 4.087 | 3.827 |
| om2_N256_E50k | `-2d -C --non-redundant` (score entropy) | 6.745782182416 | 6.745782182416 | 0e+00 | 4.143 | 3.871 |
| om5_N256_E100k | `-2d -C` | 6.880795013315 | 6.880795013315 | 0e+00 | 8.024 | 8.056 |
| om5_N256_E100k | `-2d -C --non-redundant` (score entropy) | 6.884249872034 | 6.884249872034 | 0e+00 | 8.002 | 7.901 |

Worst codelength difference across every row: **0e+00 bits**.

### The new arm: one code, priced both ways (`-C --non-redundant -N10 --seed 123`)

Not a comparison — `L̂*` and `L*` are different numbers for different quantities, and neither being lower
says anything. What the table is for is that the new arm runs on every input, finds structure on each, and
costs the same order of time as the old one.

| network | `L̂*` | top | lvls | t (s) | `L*` | top | lvls | t (s) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| ninetriangles | 3.077553 | 3 | 3 | 0.004 | 3.078067 | 3 | 3 | 0.003 |
| jazz | 6.630005 | 9 | 2 | 0.014 | 6.858483 | 6 | 2 | 0.016 |
| netscience | 3.260072 | 272 | 4 | 0.097 | 3.304490 | 272 | 4 | 0.090 |
| powergrid | 4.331056 | 4 | 7 | 0.389 | 4.471280 | 4 | 7 | 0.423 |
| polblogs (`-d`) | 7.376288 | 3 | 3 | 0.136 | 7.592668 | 3 | 3 | 0.119 |
| word_assoc (`-d`) | 11.096051 | 4 | 3 | 5.997 | 11.957006 | 1789 | 2 | 8.384 |
| lazega (metadata) | 5.603695 | 7 | 2 | 0.008 | 5.968625 | 7 | 2 | 0.007 |
| malaria (multilayer) | 7.555872 | 25 | 3 | 5.919 | 7.438064 | 2 | 3 | 4.683 |

**The one time cell where the new score is slower than the old:** malaria `--non-redundant` 5.919 s against
4.683 s, +26%. It is not the scorer — `scoreStackLhat` makes fewer `plogp` calls per module than
`scoreStackLstar` does — it is the search going somewhere else: 25 top modules against 2, so the structural
operators do more work before converging. The two numbers are not an A/B of the same computation and are not
a trade to accept or reject; `-C --non-redundant --non-redundant-score entropy` on this input is unchanged
from the tip to the bit and to within noise in seconds (4.694 s against 4.631 s in the identity table above).

### Tests

`make test-native` passes in all three configurations — `OPENMP=1`, `OPENMP=0`, and
`FEATURES="lossy-map-equation regularized-multilayer"` — 35 of 35, including the five the project's notes
list as environmental (this machine's CMake interpreter has `jsonschema`). The new file is
`test/cpp/test_lhat_star_columnar.cpp`; `test_non_redundant_columnar.cpp` keeps every `L*` golden it had,
each call now spelling out `--non-redundant-score entropy`.
