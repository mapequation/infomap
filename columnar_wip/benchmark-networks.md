# Benchmark networks (columnar core)

The networks used to benchmark the columnar map-equation engine (`--columnar` / `-C`)
against the object-oriented core. All paths are repo-relative. "Directedness" is how the
network is *run* (the flag passed), not just the file's section header — several files use
`*Edges` but are run directed with `-d` because the underlying relation is directed
(citation, web, hyperlink, blog-roll).

Single-thread convention: `MODE=release OPENMP=0`, `--seed 123`, best-of-N via `-N10`.

| network | path | run flags | directedness | type | size |
|---|---|---|---|---|--:|
| ninetriangles | `examples/networks/ninetriangles.net` | — | undirected | first-order · hierarchical toy | 27 nodes |
| jazz | `networks/arenas-jazz.txt` | — | undirected | first-order · real-world (collaboration) | 198 nodes |
| netscicoauthor2010 | `networks/db/netscicoauthor2010.net` | — | undirected | first-order · real-world (co-authorship) | 552 nodes |
| powergrid | `networks/powergrid.txt` | — | undirected | first-order · real-world (infrastructure) | 4 941 nodes |
| politicalblogs | `networks/db/politicalblogs.net` | `-d` | directed | first-order · real-world (blog links) | 1 046 nodes |
| science2001 | `networks/db/science2001.net` | `-d` | directed | first-order · real-world (journal citation) | 7 170 nodes |
| web-NotreDame | `networks/db/web-NotreDame.net` | `-d` | directed | first-order · real-world (web graph) | 325 729 nodes |
| lazega (metadata) | `networks/meta/lazega.net` (+ `networks/meta/lazega.meta`) | `--meta-data networks/meta/lazega.meta` | undirected | first-order + **metadata** objective | 69 nodes |
| multilayer (example) | `examples/networks/multilayer.net` | — | undirected | **multilayer** / higher-order (memory) toy | 5 physical nodes |
| malaria | `networks/multilayer/real-world/malaria/malaria_PLOSCompBiology_2013.net` | — | undirected | **multilayer** / higher-order (memory) real-world | 307 physical nodes · 9 layers |
| air30k (states) | `networks/states/air2011/air30k.net` | — | undirected | **state / memory** (higher-order) real-world | 183 physical · 13 213 state nodes |
| air30k (regularized) | `networks/states/air2011/air30k.net` | `-d --regularized` | directed | **state / memory** + **recorded teleportation** | 183 physical · 13 213 state nodes |
| science2001 (preferred modules) | `networks/db/science2001.net` | `-d --preferred-number-of-modules 25` | directed | first-order + **preferred-number-of-modules** bias | 7 170 nodes |
| air30k (meta) | `networks/states/air2011/air30k.net` (+ `…/air30k_usstate.meta`) | `--meta-data networks/states/air2011/air30k_usstate.meta` | undirected | **state/memory + metadata** (both codebooks) | 183 physical · 13 213 state nodes |
| overlapping om2 | `networks/debug/Jelena/network_N256_om2_nc64_E{50000,100000}_mu10_sample1.net` | `-2d`, `-d` (each also `--regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 28 203 / 29 282 state nodes (E50000 / E100000) |
| overlapping om3 | `networks/debug/Jelena/network_N256_om3_nc64_E{50000,100000}_mu10_sample1.net` | `-2d`, `-d` (each also `--regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 35 446 / 38 594 state nodes (E50000 / E100000) |
| overlapping om4 | `networks/debug/Jelena/network_N256_om4_nc64_E{50000,100000}_mu10_sample1.net` | `-2d`, `-d` (each also `--regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 40 525 / 45 394 state nodes (E50000 / E100000) |
| overlapping om5 | `networks/debug/Jelena/network_N256_om5_nc64_E{50000,100000}_mu10_sample1.net` | `-2d`, `-d` (each also `--regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 44 261 / 50 133 state nodes (E50000 / E100000) |
| overlapping om6 | `networks/debug/Jelena/network_N256_om6_nc64_E{50000,100000}_mu10_sample1.net` | `-2d`, `-d` (each also `--regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 47 073 / 53 860 state nodes (E50000 / E100000) |
| overlapping om7 | `networks/debug/Jelena/network_N256_om7_nc64_E{50000,100000}_mu10_sample1.net` | `-2d`, `-d` (each also `--regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 49 227 / 56 517 state nodes (E50000 / E100000) |
| overlapping om8 | `networks/debug/Jelena/network_N256_om8_nc64_E{50000,100000}_mu10_sample1.net` | `-2d`, `-d` (each also `--regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 51 134 / 58 505 state nodes (E50000 / E100000) |
| wikispeedia | `../../networks/examples/wikispeedia_states.net` (repo `icelab/code/networks`) | `-2d` (also run `-d`) | directed | **state / memory**, real order-2 path network, zero co-physical links, healthy control for the overlapping rows | 300 physical · 6 475 state nodes |

> **`air30k (meta)` metadata is generated, not checked in.** `networks/` is a data directory outside the
> repo and none of its higher-order networks ships a metadata file, so the row is reconstructed with
> `python3 columnar_wip/make-state-meta.py networks/states/air2011/air30k.net usstate \
> networks/states/air2011/air30k_usstate.meta` — one category per state, the two-letter US state code
> parsed out of the airport's `*Vertices` name (52 categories over 183 airports). It exists because the
> benchmark set had **no** metadata + higher-order configuration, which is exactly why it could not see
> #1012: on that input the physical-node codebook was dropped entirely. The object-oriented arm does not
> finish `-N10` inside 30 minutes on this row and is quoted at `-N1`.

> **The overlapping rows are the group-hysteresis regression guard** (F42, F47). The `om` number is the
> planted module count over four — om2 → 8 planted modules, om8 → 32 — not the overlap; states per
> physical grows with it too (110 at om2 to 229 at om8, E100000). Each physical node appears in
> ~110–230 state nodes spread over the planted communities and **no two co-physical state nodes are
> linked**, so the memory objective's optimum requires merging many flow-connected building blocks at
> once — the regime where a pairwise-greedy search either collapses to one module (om5) or stalls
> fragmented (om6, om8).
>
> **Every om runs at two trigram densities, E50000 and E100000**, because the regularized failures did
> not repeat across densities: om2 `--regularized` failed at E50000 and not at E100000 (F47), om4 / om5
> failed at E100000 (F47, #1042). The density decides the regime — at E50000 the state network has
> 1.75 links per state at om2 and 0.97 at om8, at E100000 3.34 → 1.67 (F55).
>
> **Generator.** Andrea Lancichinetti's trigram sampler (`build_syn_network` in the
> higher-order-regularization paper project), ported as `columnar_wip/make-om-network.py` with the RNG
> seeded by the sample id: `M = 4·om` communities of `nc = 64` physical nodes, every node in exactly
> `om` of them; `(1−mu)·E` trigrams a→b→c drawn inside one community, each ordered pair `{a}_b` owned
> by the community that first drew it; `mu·E` cross-community trigrams stitched from existing pairs so
> mixing adds no state node. The original is unseeded, so om2 E50000 and om4 / om5 / om6 / om8 E100000
> (the files in place before 2026-09-09) cannot be regenerated byte for byte; the other nine were made
> with `python3 columnar_wip/make-om-network.py networks/debug/Jelena 256 <om> 64 <E> 0.1 1`.
>
> **Planted vs search.** The planted partition `planted_partition_<same stem>.clu` (state id → module,
> 0-based) is scored with `-C -2d --no-infomap -c <clu>` — directed, like every row of the family — and
> a healthy search must land at or below it in bits. Both objectives, `--seed 123`, searches `-N10`,
> one binary (tip `d9210b51`, md5 `b0b878cc…`, 2026-09-09, F55). Planted and `-2d` rows are two-level
> by construction; Δ is the search's codelength against the planted one, **bold** where the search is
> worse; a ~~struck~~ planted value is worse than one-level and is no reference on that row (use the
> soft-seeded `-c` score there, F47). Times are not the axis of this table; they are logged in
> `columnar-search-runs.tsv`, batch `om-family-summary`.
>
> **Plain `-d` (unrecorded teleportation)**
>
> | network | one-level | planted | top | search `-2d -N10` | top | Δ vs planted | search `-d -N10` | top | lvls | Δ vs planted |
> |---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
> | om2-E50000 | 7.970163568 | 6.789039995 | 8 | 6.739271968 | 638 | -0.73% | 6.731808656 | 690 | 2 | -0.84% |
> | om3-E50000 | 7.967951370 | 6.850734921 | 12 | 6.258611497 | 3236 | -8.64% | 5.851498646 | 617 | 4 | -14.59% |
> | om4-E50000 | 7.975617984 | 6.913283001 | 16 | 5.454385527 | 4381 | -21.10% | 4.876717868 | 1031 | 4 | -29.46% |
> | om5-E50000 | 7.985831683 | 6.936802565 | 20 | 4.903270512 | 5443 | -29.32% | 4.150346795 | 2171 | 4 | -40.17% |
> | om6-E50000 | 7.985288435 | 6.979560518 | 24 | 4.468778176 | 6379 | -35.97% | 3.617750514 | 3050 | 4 | -48.17% |
> | om7-E50000 | 7.989224103 | 7.022804611 | 28 | 4.143277395 | 7097 | -41.00% | 3.201872271 | 3424 | 6 | -54.41% |
> | om8-E50000 | 7.991346102 | 7.034226119 | 32 | 3.859069372 | 7985 | -45.14% | 2.883308449 | 4367 | 5 | -59.01% |
> | om2-E100000 | 7.960871197 | 6.782220083 | 8 | 6.773456578 | 60 | -0.13% | 6.773456578 | 60 | 2 | -0.13% |
> | om3-E100000 | 7.974899270 | 6.837980937 | 12 | 6.822826504 | 70 | -0.22% | 6.822826504 | 70 | 2 | -0.22% |
> | om4-E100000 | 7.982931800 | 6.880650147 | 16 | 6.866901786 | 173 | -0.20% | 6.866901786 | 173 | 2 | -0.20% |
> | om5-E100000 | 7.989189332 | 6.902222527 | 20 | 6.866617805 | 308 | -0.52% | 6.866617805 | 308 | 2 | -0.52% |
> | om6-E100000 | 7.993397315 | 6.930934993 | 24 | 6.884862145 | 451 | -0.66% | 6.884862145 | 451 | 2 | -0.66% |
> | om7-E100000 | 7.993763018 | 6.957516072 | 28 | 6.889244164 | 675 | -0.98% | 6.888473273 | 680 | 2 | -0.99% |
> | om8-E100000 | 7.994219601 | 6.981034760 | 32 | 6.887234466 | 921 | -1.34% | 6.959413622 | 203 | 4 | -0.31% |
>
> **`--regularized` (recorded teleportation)**
>
> | network | one-level | planted | top | search `-2d -N10` | top | Δ vs planted | search `-d -N10` | top | lvls | Δ vs planted |
> |---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
> | om2-E50000 | 7.970508085 | 7.583820576 | 8 | 7.548721547 | 126 | -0.46% | 7.548721547 | 126 | 2 | -0.46% |
> | om3-E50000 | 7.969664223 | ~~8.141739503~~ | 12 | 7.931196935 | 49 | -2.59% | 7.931196935 | 49 | 2 | -2.59% |
> | om4-E50000 | 7.978790193 | ~~8.594489658~~ | 16 | 7.940858042 | 41 | -7.61% | 7.940858042 | 41 | 2 | -7.61% |
> | om5-E50000 | 7.988627070 | ~~8.930493739~~ | 20 | 7.969030601 | 26 | -10.77% | 7.969030601 | 26 | 2 | -10.77% |
> | om6-E50000 | 7.989209626 | ~~9.181814653~~ | 24 | 7.959010571 | 26 | -13.32% | 7.959010571 | 26 | 2 | -13.32% |
> | om7-E50000 | 7.992344954 | ~~9.387158058~~ | 28 | 7.974818773 | 23 | -15.05% | 7.974818773 | 23 | 2 | -15.05% |
> | om8-E50000 | 7.994534803 | ~~9.614433321~~ | 32 | 7.990101633 | 8 | -16.89% | 7.990101633 | 8 | 2 | -16.89% |
> | om2-E100000 | 7.960610295 | 6.964713526 | 8 | 6.950176925 | 9 | -0.21% | 6.950176925 | 9 | 2 | -0.21% |
> | om3-E100000 | 7.974141750 | 7.283809081 | 12 | 7.260835207 | 29 | -0.32% | 7.260835207 | 29 | 2 | -0.32% |
> | om4-E100000 | 7.982849200 | 7.584396786 | 16 | 7.556894677 | 79 | -0.36% | 7.556653713 | 78 | 2 | -0.37% |
> | om5-E100000 | 7.989613065 | 7.791810113 | 20 | 7.967531078 | 98 | **+2.26%** | 7.965009721 | 106 | 2 | **+2.22%** |
> | om6-E100000 | 7.993490371 | ~~8.025567656~~ | 24 | 7.981574549 | 113 | -0.55% | 7.981063575 | 119 | 2 | -0.55% |
> | om7-E100000 | 7.992395554 | ~~8.132958621~~ | 28 | 7.945712536 | 184 | -2.30% | 7.945712536 | 184 | 2 | -2.30% |
> | om8-E100000 | 7.994735672 | ~~8.294767960~~ | 32 | 7.976140205 | 240 | -3.84% | 7.976681139 | 256 | 2 | -3.83% |
>
> Read across: on the plain objective the search beats the planted partition everywhere, and by
> 8–59% at E50000 for om ≥ 3, where the state network has ≤ 1.4 links per state and the optimum is a
> forest of thousands of small modules (om8 E50000: 4 367 top modules, 5 levels) — the planted cover
> is not what the objective wants there, so those rows guard the search, not the recovery. Under
> `--regularized` the planted partition is worse than one-level on every E50000 row from om3 up and on
> om6 / om7 / om8 at E100000; the search sits below one-level on all of them. The one
> planted-vs-search gap is **om5 E100000 `--regularized`: +2.26% bits** (#1042), the row where a
> partition 2.5% below one-level exists and the search stops 0.28% below it. The one
> hierarchical-vs-two-level gap is **om8 E100000 plain `-d`: 6.959 against 6.887 for `-2d`**
> (+1.05% bits; #1041, whose seeded 6.718 puts the row's true gap at 3.6%) — on every other row `-d`
> is at or below `-2d`. om2 E100000 `--regularized` is the first row where the regularized search
> reaches the planted scale: 9 top modules against 8 planted, 6.950 against 6.965 bits.
>
> **wikispeedia is the healthy control for the same structural family as the overlapping rows** (F44): also order-2 with zero
> co-physical links, but at 21.6 states per physical the memory reward does not dominate, and the
> regroup machinery must leave it bit-identical in bits at `-N10` (5.907904741, 199 modules, `-2d`).

**Coverage rationale**
- **Base map equation, undirected**: ninetriangles (hierarchy), jazz, netscicoauthor2010, powergrid.
- **Base map equation, directed** (where the up/down search and time matter most): politicalblogs,
  science2001, web-NotreDame (the large stress case).
- **Composable objectives** (exercise the correction hooks): lazega + metadata; air30k, multilayer,
  malaria for the memory/higher-order objective (physical-node codebook).
- **Recorded teleportation** (exercises the tele-path move loop): air30k `-d --regularized` — the
  regularized directed flow model turns on recorded teleportation, so the leaf move loop runs the
  teleport-inclusive delta (`deltaCodelengthMovingNodeTele*`) rather than the link-only one.
- **Search-shaping bias**: science2001 `-d --preferred-number-of-modules 25` exercises the columnar
  `|K − K_pref|` bias (`PreferredModulesCorrection`).
- **Scale**: from 5-node toys (fast correctness) to 325k-node web-NotreDame (time/memory).

> **Fixed (see `columnar-rethink-notes.md` F15/F16):** `-C` best-of-N on **politicalblogs**
> previously returned a negative, invalid "best codelength" — a cross-trial materialization bug
> in the reconstructed OO tree. The engine now reports the columnar core's own (always-correct)
> codelength (`columnarL`) rather than re-deriving it from the OO tree, so politicalblogs
> `-C -N>1` is reliable again.
