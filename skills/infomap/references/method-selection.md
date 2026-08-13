# Method Selection

Use this reference when the user needs help turning a research problem into an Infomap analysis design.

## Is Infomap the right tool?

There is no single "true" partition of a network; the method must match the question.

- **Use Infomap (flow-based) when a real dynamical process moves on the network** — movement, dispersal, biomass or gene flow, citations, traffic, spreading, money, web transitions — and when direction and weights carry that process. The map equation finds modules that trap that flow.
- **Prefer modularity (Q) or a stochastic block model when the question is purely structural** — density-based groups relative to a random-expectation null (modularity), or structurally-equivalent *roles* (SBM).
- Because Infomap follows flow, strongly connected groups can **merge** once the flow between them grows past a threshold — a modeling consequence of following flow, not a bug — whereas unweighted modularity can keep them split. If a user expects fixed structural blocks regardless of flow strength, flow-based clustering can be the wrong choice (see Farage et al. 2021).

## Survey frame

The survey article (`https://doi.org/10.1145/3779648`) organizes map equation workflows as:

1. **Network representation**: decide what nodes and links mean.
2. **Flow modeling**: decide how flows move on that representation.
3. **Flow mapping**: run Infomap to identify modules that compress those flows.

Ask only the questions needed for the task. If the user wants code immediately, infer reasonable defaults and state them briefly.

## Choosing a representation

Balance simplicity and accuracy. The map equation selects models by description length. The best model gives the shortest code, trading model fit against model complexity, so too-few modules underfit and too-many overfit.

- **Incorporate weights and direction when they are available and meaningful** — they usually improve accuracy at little cost.
- **Reach for richer representations** (memory/state, multilayer, metadata, bipartite) **when the data actually encode that structure or the question requires it** — not by reflex. Let the research question drive the choice.
- Two practical cautions. First, richer representations create many more state nodes and links and are **harder to optimize**, so they often need more trials to reach a good solution. Second, use codelength to compare **module counts or regularization strength within one representation**. Do not use it to pick between representations (that is a modeling decision driven by the question and the data).

Representation options:

- **Simple pairwise network**: use when interactions are ordinary node-node links. Preserve weights and direction when they are meaningful and available.
- **Directed network**: use when link orientation represents the process, such as citations, transitions, web links, pathways, or asymmetric movement.
- **Weighted network**: use when link strength represents frequency, affinity, capacity, similarity, or confidence. Check whether larger weights mean stronger flow.
- **State or memory network**: use when the next step depends on previous context, sequence history, or when one physical node can participate in different flow contexts.
- **Multilayer network**: use when the same physical entities appear in layers such as time, modality, context, data source, transport mode, or interaction type.
- **Metadata network**: use when node attributes must influence the module description, such as known categories, labels, environments, or annotations.
- **Bipartite network**: use when links only connect two node types, such as documents-terms, plants-pollinators, users-items, or features-samples.
- **Incomplete-data workflow**: use when observed links are sparse samples of an underlying flow process and regularization or Bayesian prior assumptions matter.

## Scale and resolution

How fine or coarse the modules come out is a modeling choice, not just an output. The main levers:

- **Markov time** is the scale knob. Longer Markov time favors **fewer, larger** modules (coarser, shallower hierarchies); shorter Markov time (< 1) favors **more, smaller** modules (finer, deeper). It changes the model, so report it whenever it is non-default (Kheirkhahzadeh et al. 2016).
- **Field-of-view limit**: on large, *sparse* structures a random walker stays local and can shatter one big sparse community into many small look-alike ones (overfitting). If a large sparse network — street grids, sparse occurrence or bipartite data, brain pathways — fragments into many tiny modules that shift across seeds, suspect the field-of-view limit. Look for a *variable Markov time* option rather than only adding trials (Edler et al. 2022).
- **Resolution limit**: the map equation's resolution limit depends on the **cut size** (the number of links *between* modules), not on the total number of links as in modularity — so it is comparatively mild. If you suspect the resolution limit merges genuinely small modules, prefer the **multilevel (hierarchical)** solution and/or sweep Markov time before you force a target module count. The hierarchical remedy works when real nested structure exists (Kawamoto et al. 2015).
- A unipartite **projection of a bipartite network behaves roughly like doubling the Markov time**, that is, it coarsens — prefer modeling the bipartite structure directly when scale matters (Kheirkhahzadeh et al. 2016).

Treat forced module counts (for example a preferred-number-of-modules knob) as sensitivity analysis, not as evidence the data contain that many modules.

## Higher-order, multilayer, and temporal configuration

- **Memory / state networks** need actual pathway or sequence data (trajectories of steps); you **cannot recover a second-order model from an aggregated first-order network**. Expect smaller, often **overlapping** modules and shifted node rankings. A second-order model usually captures most of the gain, with diminishing returns at higher orders. Prefer second order unless the data clearly justify more (Rosvall et al. 2014).
- **Multilayer coupling** depends on the inter-layer **relax rate** `r`: `r = 0` keeps layers fully separate, `r = 1` fully aggregates them. The right value is **system-dependent** — common working values are `r = 0.15` (a frequent default), with results often robust around `0.25`. Tune to the data and use explicit inter-layer links when you actually have them (De Domenico et al. 2015; Edler et al. 2017).
- **Intermittent or asynchronous communities**: uniform (whole-layer) or adjacent-layer coupling can dilute boundaries and merge communities that recur at different times. For communities that appear intermittently, prefer **node-level similarity-based coupling** (neighborhood / Jensen-Shannon) over uniform coupling (Aslak et al. 2017).

Verify the exact option names for Markov time, variable Markov time, relax rate, and coupling from the installed CLI help or API before you generate code. Names and availability differ across versions and interfaces.

## Metadata and bipartite prompts

For metadata workflows, clarify:

- what the metadata categories represent;
- whether metadata must influence the compression objective or only provide post hoc annotation;
- whether metadata is categorical and available for every relevant node;
- which interface supports the needed input path: Python can set metadata programmatically (the stateful builder's `set_meta_data()`, or a metadata file via `Options` — confirm the current spelling from installed help), while CLI/R workflows can use metadata option files or R6 methods depending on the current API.

For bipartite workflows, clarify:

- the two node types and whether links only cross between types;
- whether the reported modules must include both node types or hide/project the second type;
- how you declare the second node type, such as `bipartite_start_id` in programmatic APIs or a bipartite input heading in files;
- whether bipartite flow adjustment, bipartite teleportation, or hidden bipartite-node output is part of the research question.

## Flow-model choices

- For undirected pairwise structure, the default unbiased random-walk model is usually enough; teleportation is not needed on undirected networks.
- For directed links, use directed flow modeling instead of symmetrized links unless the research question explicitly ignores direction. Teleportation choice affects directed clustering — prefer the established default (unrecorded-link teleportation) over hand-tuning the teleportation rate (Lambiotte & Rosvall 2012).
- For observed flows or transition counts, consider whether precomputed or raw directed flow better matches the data; verify current option names from CLI help or API docs.
- For higher-order, temporal, or multi-context data, model state nodes or layers explicitly; do not flatten if flattening would mix distinct flows.
- For metadata or bipartite workflows, verify the current interface support before you generate code because details differ across CLI, Python, and R.

## Output choices

- Use top-level modules when the downstream analysis expects one label per node.
- Use hierarchical paths when the research question needs multiscale modules.
- Use state-level outputs when the same physical node can belong to different modules across contexts.
- Use physical-level outputs when the final report needs one merged assignment per physical entity.
- Use codelength and number of modules for run summaries; avoid treating codelength differences as a universal statistical test unless the user asks for model comparison.

## Questions to ask when planning

- What do nodes and links represent?
- Are links directed, weighted, or observed flows?
- Does a node appear in multiple contexts, layers, or histories?
- Is the desired result one module per physical node, per state node, or per layer-node state?
- Which environment will produce the result: CLI, Python, R, or notebook?
- What artifacts does the paper or workflow need: tables, tree/clu files, graph export, plots, or method text?
