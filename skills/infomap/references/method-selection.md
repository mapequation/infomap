# Method Selection

Use this reference when the user needs help turning a research problem into an Infomap analysis design.

## Survey frame

The survey article (`https://doi.org/10.1145/3779648`) organizes map equation workflows as:

1. **Network representation**: decide what nodes and links mean.
2. **Flow modeling**: decide how flows move on that representation.
3. **Flow mapping**: run Infomap to identify modules that compress those flows.

Ask only the questions needed for the task. If the user wants code immediately, infer reasonable defaults and state them briefly.

## Representation choices

- **Simple pairwise network**: use when interactions are ordinary node-node links. Preserve weights and direction when they are meaningful and available.
- **Directed network**: use when link orientation represents the process, such as citations, transitions, web links, pathways, or asymmetric movement.
- **Weighted network**: use when link strength represents frequency, affinity, capacity, similarity, or confidence. Check whether larger weights mean stronger flow.
- **State or memory network**: use when the next step depends on previous context, sequence history, or when one physical node can participate in different flow contexts.
- **Multilayer network**: use when the same physical entities appear in layers such as time, modality, context, data source, transport mode, or interaction type.
- **Metadata network**: use when node attributes should influence the module description, such as known categories, labels, environments, or annotations.
- **Bipartite network**: use when links only connect two node types, such as documents-terms, plants-pollinators, users-items, or features-samples.
- **Incomplete-data workflow**: use when observed links are sparse samples of an underlying flow process and regularization or Bayesian prior assumptions matter.

## Metadata and bipartite prompts

For metadata workflows, clarify:

- what the metadata categories represent;
- whether metadata should influence the compression objective or only be used for post hoc annotation;
- whether metadata is categorical and available for every relevant node;
- which interface supports the needed input path: Python can set metadata programmatically with `set_meta_data()`, while CLI/R workflows may use metadata option files or R6 methods depending on the current API.

For bipartite workflows, clarify:

- the two node types and whether links only cross between types;
- whether the reported modules should include both node types or hide/project the second type;
- how the second node type is identified, such as `bipartite_start_id` in programmatic APIs or a bipartite input heading in files;
- whether bipartite flow adjustment, bipartite teleportation, or hidden bipartite-node output is part of the research question.

## Flow-model choices

- For undirected pairwise structure, the default unbiased random-walk model is usually enough.
- For directed links, use directed flow modeling instead of symmetrizing unless the research question explicitly ignores direction.
- For observed flows or transition counts, consider whether precomputed or raw directed flow better matches the data; verify current option names from CLI help or API docs.
- For higher-order, temporal, or multi-context data, model state nodes or layers explicitly instead of flattening if flattening would mix distinct flows.
- For metadata or bipartite workflows, verify the current interface support before generating code because details differ across CLI, Python, and R.

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
- Which environment should produce the result: CLI, Python, R, or notebook?
- What artifacts are needed for the paper or workflow: tables, tree/clu files, graph export, plots, or method text?
