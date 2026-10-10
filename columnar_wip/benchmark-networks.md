# Benchmark networks (columnar core)

The networks used to benchmark the columnar map-equation engine (`--columnar` / `-C`)
against the object-oriented core. "Directedness" is how the network is *run* (the flag
passed), not just the file's section header — several files use `*Edges` but are run
directed with `-d` because the underlying relation is directed (citation, web, hyperlink,
blog-roll).

Single-thread convention: `MODE=release OPENMP=0`, `--seed 123`, best-of-N via `-N10`.

**Getting the inputs.** Every input is fetched or built by one script, from the manifest
[`columnar_wip/benchmark-networks.toml`](benchmark-networks.toml), into
`networks/columnar-benchmark/` (git-ignored):

```
pip install "mapequation-networks @ git+https://github.com/mapequation/networks@0e3646eb5baf38b7be80310073c06c049a599747"
python columnar_wip/fetch-benchmark-networks.py
```

It needs Python 3.11+. Each file is checked against
[`columnar_wip/benchmark-networks.sha256`](benchmark-networks.sha256), so a run on the
result uses the bytes the published numbers were measured on. Every input comes through the
[`mapequation-networks`](https://github.com/mapequation/networks) package: public
repositories (netzschleuder, SNAP, the BTS DB1B coupons, Wikispeedia) and the package's
generator for the synthetic family; ninetriangles and multilayer are in this repository.
air30k is the slow one: the first run downloads ~270 MB of DB1B coupons and groups ~10 M
itineraries (~15 min), cached afterwards. The networks package is pinned to a commit on
its main branch, the first with `generate.overlapping_memory_benchmark`
(mapequation/networks#13), until a release includes it.

| network | file in `networks/columnar-benchmark/` | source | run flags | directedness | type | size |
|---|---|---|---|---|---|--:|
| ninetriangles | `ninetriangles.net` | this repo, `examples/networks/` | — | undirected | first-order · hierarchical toy | 27 nodes |
| jazz | `jazz.txt` | netzschleuder `jazz_collab` | — | undirected | first-order · real-world (collaboration) | 198 nodes |
| netscience | `netscience.txt` | netzschleuder `netscience` | — | undirected | first-order · real-world (co-authorship, weighted; 396 components) | 1 461 nodes · 2 742 links |
| powergrid | `powergrid.txt` | netzschleuder `power` | — | undirected | first-order · real-world (infrastructure) | 4 941 nodes |
| polblogs | `polblogs.txt` | netzschleuder `polblogs` (Adamic–Glance) | `-d` | directed | first-order · real-world (blog links; two-level optimum) | 1 224 nodes · 19 025 links |
| word_assoc | `word_assoc.txt` | netzschleuder `word_assoc` | `-d` | directed | first-order · real-world (word association, weighted) | 23 132 nodes · 312 310 links |
| web-NotreDame | `web-NotreDame.txt` | SNAP `web-NotreDame`, as distributed | `-d` | directed | first-order · real-world (web graph) | 325 729 nodes · 1 497 134 links |
| lazega (metadata) | `lazega.net` (+ `lazega.meta`) | netzschleuder `law_firm`, layer 2 (friendship) + `nodeGender` | `--meta-data networks/columnar-benchmark/lazega.meta` | undirected | first-order + **metadata** objective | 69 nodes |
| multilayer (example) | `multilayer.net` | this repo, `examples/networks/` | — | undirected | **multilayer** / higher-order (memory) toy | 5 physical nodes |
| malaria | `malaria.net` | netzschleuder `malaria_genes`, `HVR_1`–`HVR_9` as layers 1–9 | — | undirected | **multilayer** / higher-order (memory) real-world | 307 physical nodes · 9 layers |
| air30k (states) | `air30k.net` | DB1B 2011 Q1–Q3 (`networks.paths`), second order on the 183 airports of [`air30k-airports.tsv`](air30k-airports.tsv) | — | undirected | **state / memory** (higher-order) real-world | 183 physical · 13 212 state nodes |
| air30k (regularized) | `air30k.net` | as above | `-d --regularized` | directed | **state / memory** + **recorded teleportation** | 183 physical · 13 212 state nodes |
| word_assoc (preferred modules) | `word_assoc.txt` | netzschleuder `word_assoc` | `-d --preferred-number-of-modules 25` | directed | first-order + **preferred-number-of-modules** bias | 23 132 nodes |
| air30k (meta) | `air30k.net` (+ `air30k_usstate.meta`) | as above; metadata from `columnar_wip/make-state-meta.py` | `--meta-data networks/columnar-benchmark/air30k_usstate.meta` | undirected | **state/memory + metadata** (both codebooks) | 183 physical · 13 212 state nodes |
| overlapping om2–om8 | `om/overlapping-memory-n256-om<om>-nc64-E<E>-mu0.1-seed1.net` (+ planted `.clu`), E = 50000 and 100000 | `networks.generate.overlapping_memory_benchmark(om, E, seed=1)` | `-2d`, `-d` (each also `--regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 28–59 k state nodes (table below) |
| wikispeedia | `wikispeedia_states.net` | `networks.paths` wikispeedia, `to_state_network(order=2, max_nodes=300)` | `-2d` (also run `-d`) | directed | **state / memory**, real order-2 path network, zero co-physical links, healthy control for the overlapping rows | 300 physical · 6 475 state nodes |

> **Inputs changed on 2026-10-10** (F64, F65). Before that the rows ran on local files. The
> snapshot of that PR compares old and new on one binary:
> - **web-NotreDame** was a DAG: SNAP with self-loops dropped, reciprocal arcs merged to
>   weight 2 and every edge pointing from low to high id. It is now the SNAP file as
>   distributed.
> - **air30k** was the file from Rosvall et al. 2014, which no public data reproduces
>   exactly. It is now rebuilt from the DB1B coupons (F64). The paper's file is kept in
>   networks-store as `memory/air2011/air30k.net`.
> - **jazz, powergrid and malaria** are the same networks with other node ids or line
>   orders.
> - **om2 E50000 and om4 / om5 / om6 / om8 E100000** were unseeded draws of the generator
>   and are now seed 1. The nine seeded om files are the same networks in the package's
>   file layout.
> - **Three networks were replaced** (F65) so that every input comes from the package:
>   - netscicoauthor2010 (2010, 552 nodes, no public source) by netzschleuder's `netscience`
>     (2006). Their authors overlap, but the 2010 file is a later compilation, not a subset.
>   - politicalblogs, a network of Swedish blogs, by the Adamic–Glance `polblogs`. Both are
>     directed with a two-level optimum, the property that exposed F15.
>   - science2001, a journal citation network derived from licensed Journal Citation
>     Reports data, by `word_assoc`. Both are directed and weighted with multi-second runs,
>     and word_assoc is a row where columnar ends above OO.

> **`air30k (meta)` metadata is generated, not checked in.** None of the higher-order inputs ships a
> metadata file, so the fetch script reconstructs it with `columnar_wip/make-state-meta.py <air30k.net>
> usstate <air30k_usstate.meta>` — one category per state, the two-letter US state code parsed out of
> the airport's `*Vertices` name (52 categories over 183 airports). It exists because the
> benchmark set had **no** metadata + higher-order configuration, which is exactly why it could not see
> #1012: on that input the physical-node codebook was dropped entirely. The object-oriented arm does not
> finish `-N10` inside 30 minutes on this row and is quoted at `-N1`. On the DB1B rebuild it does not
> finish `-N1` with `--seed 123` either: killed after 9 h 25 min, while seeds 456 and 7 take 10 s and
> 6 s (#1134).

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
> higher-order-regularization paper project), ported to the networks package as
> `networks.generate.overlapping_memory_benchmark(om, E, seed=1)`: `M = 4·om` communities of `nc = 64`
> physical nodes, every node in exactly `om` of them; `(1−mu)·E` trigrams a→b→c drawn inside one
> community, each ordered pair `{a}_b` owned by the community that first drew it; `mu·E`
> cross-community trigrams stitched from existing pairs so mixing adds no state node. The port draws
> the original's random numbers in the original's order, so a seed gives the same network as the
> original script with that seed. All fourteen networks are seed 1 since 2026-10-10 (F64). Until then
> om2 E50000 and om4 / om5 / om6 / om8 E100000 were unseeded draws that could not be regenerated.
>
> **Planted vs search.** The planted partition (the `.clu` next to each network: state id → module,
> 0-based) is scored with `-C -2d --no-infomap -c <clu>` — directed, like every row of the family — and
> a healthy search must land at or below it in bits. Both objectives, `--seed 123`, searches `-N10`,
> one binary (tip `c9ce210b`, md5 `54c8b6b2…`, 2026-10-10, F64). Planted and `-2d` rows are two-level
> by construction; Δ is the search's codelength against the planted one, **bold** where the search is
> worse; a ~~struck~~ planted value is worse than one-level and is no reference on that row (use the
> soft-seeded `-c` score there, F47). Search times are `timing.total_s` from the F64 snapshot session
> (batch `inputs-snapshot` in `columnar-search-runs.tsv`), at load 5–14: read them as the cost of
> the row, not to compare rows. One-level and planted codelengths come from a check run on the same
> binary just before the session. † marks the five networks that changed in F64.
>
> **Plain `-d` (unrecorded teleportation)**
>
> | network | one-level | planted | top | search `-2d -N10` | top | Δ vs planted | search `-d -N10` | top | lvls | Δ vs planted | time `-2d` / `-d` (s) |
> |---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
> | om2-E50000 † | 7.957873782 | 6.778901842 | 8 | 6.752836583 | 536 | -0.38% | 6.752836583 | 536 | 2 | -0.38% | 3.34 / 3.76 |
> | om3-E50000 | 7.967951370 | 6.850734921 | 12 | 6.258611497 | 3236 | -8.64% | 5.851498646 | 617 | 4 | -14.59% | 4.37 / 4.72 |
> | om4-E50000 | 7.975617984 | 6.913283001 | 16 | 5.454385527 | 4381 | -21.10% | 4.873775655 | 1200 | 4 | -29.50% | 4.59 / 5.42 |
> | om5-E50000 | 7.985831683 | 6.936802565 | 20 | 4.903270512 | 5443 | -29.32% | 4.150346795 | 2171 | 4 | -40.17% | 4.35 / 5.85 |
> | om6-E50000 | 7.985288435 | 6.979560518 | 24 | 4.468778176 | 6379 | -35.97% | 3.617750514 | 3050 | 4 | -48.17% | 4.45 / 6.03 |
> | om7-E50000 | 7.989224103 | 7.022804611 | 28 | 4.143277395 | 7097 | -41.00% | 3.201872271 | 3424 | 6 | -54.41% | 4.71 / 7.57 |
> | om8-E50000 | 7.991346102 | 7.034226119 | 32 | 3.859069372 | 7985 | -45.14% | 2.883308449 | 4367 | 5 | -59.01% | 4.52 / 8.08 |
> | om2-E100000 | 7.960871197 | 6.782220083 | 8 | 6.773456578 | 60 | -0.13% | 6.773456578 | 60 | 2 | -0.13% | 4.01 / 3.27 |
> | om3-E100000 | 7.974899270 | 6.837980937 | 12 | 6.822832994 | 69 | -0.22% | 6.822832994 | 69 | 2 | -0.22% | 6.92 / 5.55 |
> | om4-E100000 † | 7.984181438 | 6.873858104 | 16 | 6.859843831 | 173 | -0.20% | 6.860084625 | 176 | 2 | -0.20% | 7.45 / 6.20 |
> | om5-E100000 † | 7.989304757 | 6.911031376 | 20 | 6.880795013 | 283 | -0.44% | 6.880795013 | 283 | 2 | -0.44% | 7.61 / 6.22 |
> | om6-E100000 † | 7.990780342 | 6.936312189 | 24 | 6.893434836 | 440 | -0.62% | 6.892138569 | 431 | 2 | -0.64% | 7.60 / 8.62 |
> | om7-E100000 | 7.993763018 | 6.957516072 | 28 | 6.88992089 | 663 | -0.97% | 6.88945591 | 666 | 2 | -0.98% | 7.86 / 9.51 |
> | om8-E100000 † | 7.994997224 | 6.966510195 | 32 | 6.873356654 | 848 | -1.34% | 6.87278322 | 868 | 2 | -1.35% | 8.40 / 9.68 |
>
> **`--regularized` (recorded teleportation)**
>
> | network | one-level | planted | top | search `-2d -N10` | top | Δ vs planted | search `-d -N10` | top | lvls | Δ vs planted | time `-2d` / `-d` (s) |
> |---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
> | om2-E50000 † | 7.958565403 | 7.569509705 | 8 | 7.53510563 | 119 | -0.45% | 7.53510563 | 119 | 2 | -0.45% | 2.61 / 2.44 |
> | om3-E50000 | 7.969664223 | ~~8.141739503~~ | 12 | 7.925216272 | 89 | -2.66% | 7.925216272 | 89 | 2 | -2.66% | 2.53 / 2.53 |
> | om4-E50000 | 7.978790193 | ~~8.594489658~~ | 16 | 7.92621405 | 110 | -7.78% | 7.92621405 | 110 | 2 | -7.78% | 2.76 / 2.77 |
> | om5-E50000 | 7.988627070 | ~~8.930493739~~ | 20 | 7.956672505 | 101 | -10.90% | 7.956672505 | 101 | 2 | -10.90% | 2.79 / 2.94 |
> | om6-E50000 | 7.989209626 | ~~9.181814653~~ | 24 | 7.944047825 | 94 | -13.48% | 7.944047825 | 94 | 2 | -13.48% | 2.76 / 2.88 |
> | om7-E50000 | 7.992344954 | ~~9.387158058~~ | 28 | 7.957532546 | 112 | -15.23% | 7.957532546 | 112 | 2 | -15.23% | 2.99 / 3.09 |
> | om8-E50000 | 7.994534803 | ~~9.614433321~~ | 32 | 7.978376075 | 77 | -17.02% | 7.978376075 | 77 | 2 | -17.02% | 2.88 / 2.86 |
> | om2-E100000 | 7.960610295 | 6.964713526 | 8 | 6.950176925 | 9 | -0.21% | 6.950176925 | 9 | 2 | -0.21% | 3.87 / 3.37 |
> | om3-E100000 | 7.974141750 | 7.283809081 | 12 | 7.260835207 | 29 | -0.32% | 7.260835207 | 29 | 2 | -0.32% | 6.06 / 4.78 |
> | om4-E100000 † | 7.984230648 | 7.565393576 | 16 | 7.534667081 | 72 | -0.41% | 7.535170166 | 71 | 2 | -0.40% | 5.44 / 5.30 |
> | om5-E100000 † | 7.989226720 | 7.825625970 | 20 | 7.973194647 | 98 | **+1.89%** | 7.973146326 | 98 | 2 | **+1.89%** | 4.81 / 4.73 |
> | om6-E100000 † | 7.991148021 | ~~8.018519168~~ | 24 | 7.979716886 | 121 | -0.48% | 7.981023534 | 118 | 2 | -0.47% | 5.15 / 5.05 |
> | om7-E100000 | 7.992395554 | ~~8.132958621~~ | 28 | 7.948827768 | 166 | -2.26% | 7.948827768 | 166 | 2 | -2.26% | 5.11 / 4.94 |
> | om8-E100000 † | 7.995153760 | ~~8.282251146~~ | 32 | 7.984638887 | 203 | -3.59% | 7.985651781 | 203 | 2 | -3.58% | 5.05 / 4.91 |
>
> Read across: on the plain objective the search beats the planted partition everywhere, and by
> 8–59% at E50000 for om ≥ 3, where the state network has ≤ 1.4 links per state and the optimum is a
> forest of thousands of small modules (om8 E50000: 4 367 top modules, 5 levels) — the planted cover
> is not what the objective wants there, so those rows guard the search, not the recovery. Under
> `--regularized` the planted partition is worse than one-level on every E50000 row from om3 up and on
> om6 / om7 / om8 at E100000; the search sits below one-level on all of them. The one
> planted-vs-search gap is **om5 E100000 `--regularized`: +1.89% bits** (#1042), on the new draw as
> on the old one (+2.26%): a partition 2.0% below one-level exists and the search stops 0.20% below
> one-level. #1041's hierarchical-vs-two-level gap (om8 E100000 plain `-d` at 6.959 against 6.887 for
> `-2d` on the old draw) does not appear on the new draw: `-d` 6.872783, `-2d` 6.873357 bits. On every
> row `-d` is below `-2d` or within 0.02% of it. om2 E100000 `--regularized` is the first row where
> the regularized search reaches the planted scale: 9 top modules against 8 planted, 6.950 against
> 6.965 bits.
>
> The previous version of these tables was measured on tip `bc31f036` (F58). Its seeded rows differ
> from these only by the engine changes since then (#1123–#1128, each measured in its own PR): those
> files give the same bits here as the package's.

> **wikispeedia is the healthy control for the same structural family as the overlapping rows** (F44): also order-2 with zero
> co-physical links, but at 21.6 states per physical the memory reward does not dominate, and the
> regroup machinery must leave it bit-identical in bits at `-N10` (5.907904741, 199 modules, `-2d`).

**Coverage rationale**
- **Base map equation, undirected**: ninetriangles (hierarchy), jazz, netscience (disconnected:
  396 components), powergrid.
- **Base map equation, directed** (where the up/down search and time matter most): polblogs (a
  two-level optimum), word_assoc (weighted, 23k nodes), web-NotreDame (the large stress case).
- **Composable objectives** (exercise the correction hooks): lazega + metadata; air30k, multilayer,
  malaria for the memory/higher-order objective (physical-node codebook).
- **Recorded teleportation** (exercises the tele-path move loop): air30k `-d --regularized` — the
  regularized directed flow model turns on recorded teleportation, so the leaf move loop runs the
  teleport-inclusive delta (`deltaCodelengthMovingNodeTele*`) rather than the link-only one.
- **Search-shaping bias**: word_assoc `-d --preferred-number-of-modules 25` exercises the columnar
  `|K − K_pref|` bias (`PreferredModulesCorrection`).
- **Scale**: from 5-node toys (fast correctness) to 325k-node web-NotreDame (time/memory).

> **Fixed (see `columnar-rethink-notes.md` F15/F16):** `-C` best-of-N on **politicalblogs** (the
> Swedish blog network the set used until F65)
> previously returned a negative, invalid "best codelength" — a cross-trial materialization bug
> in the reconstructed OO tree. The engine now reports the columnar core's own (always-correct)
> codelength (`columnarL`) rather than re-deriving it from the OO tree, so politicalblogs
> `-C -N>1` is reliable again.
