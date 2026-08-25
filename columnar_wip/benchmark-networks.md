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
| overlapping om2 | `networks/debug/Jelena/network_N256_om2_nc64_E50000_mu10_sample1.net` | `-2d` (also run `-2d --regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 28 203 state nodes |
| overlapping om4 | `networks/debug/Jelena/network_N256_om4_nc64_E100000_mu10_sample1.net` | `-2d` (also run `-2d --regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 45 394 state nodes |
| overlapping om5 | `networks/debug/Jelena/network_N256_om5_nc64_E100000_mu10_sample1.net` | `-2d` (also run `-d`, `-2d --regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 50 133 state nodes |
| overlapping om6 | `networks/debug/Jelena/network_N256_om6_nc64_E100000_mu10_sample1.net` | `-2d` (also run `-d`, `-2d --regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 53 860 state nodes |
| overlapping om8 | `networks/debug/Jelena/network_N256_om8_nc64_E100000_mu10_sample1.net` | `-2d` (also run `-2d --regularized`) | directed | **state / memory**, planted overlapping communities, zero co-physical links | 256 physical · 58 505 state nodes |
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
> physical grows with it too (110 at om2 to 229 at om8). Each physical node appears in ~110–230 state
> nodes spread over the planted communities and **no two co-physical state nodes are linked**, so the
> memory objective's optimum requires merging many flow-connected building blocks at once — the regime
> where a pairwise-greedy search either collapses to one module (om5) or stalls fragmented (om6, om8).
> The planted partition `planted_partition_<same stem>.clu` is the quality reference under
> `-C -2d --no-infomap -c`, and a healthy search must land at or below it:
>
> | | om2 | om4 | om5 | om6 | om8 |
> |---|--:|--:|--:|--:|--:|
> | planted, `-2d` | 6.789039995 | 6.880650147 | 6.902222527 | 6.930934993 | 6.981034760 |
> | planted, `-2d --regularized` | 7.583820576 | 7.584396786 | 7.791810113 | 8.025567656 | 8.294767960 |
> | one-level, `-2d --regularized` | 7.970508085 | 7.982849200 | 7.989613065 | 7.993490371 | 7.994735672 |
>
> **Under `--regularized` the planted partition of om6 and om8 is worse than one-level**, so on those two
> rows it is not the reference for anything — soft-seeding om8 from it collapses to the one-level bound.
> Use the soft-seeded (`-c`) score as the reference there instead. `--regularized` is a distinct
> configuration on this family, not a variant: it turns on recorded teleportation, and F47's probe defect
> lived only there.
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
