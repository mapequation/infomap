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

**Coverage rationale**
- **Base map equation, undirected**: ninetriangles (hierarchy), jazz, netscicoauthor2010, powergrid.
- **Base map equation, directed** (where the up/down search and time matter most): politicalblogs,
  science2001, web-NotreDame (the large stress case).
- **Composable objectives** (exercise the correction hooks): lazega + metadata; air30k, multilayer,
  malaria for the memory/higher-order objective (physical-node codebook).
- **Scale**: from 5-node toys (fast correctness) to 325k-node web-NotreDame (time/memory).

> **Fixed (see `columnar-rethink-notes.md` F15/F16):** `-C` best-of-N on **politicalblogs**
> previously returned a negative, invalid "best codelength" — a cross-trial materialization bug
> in the reconstructed OO tree. The engine now reports the columnar core's own (always-correct)
> codelength (`columnarL`) rather than re-deriving it from the OO tree, so politicalblogs
> `-C -N>1` is reliable again.
