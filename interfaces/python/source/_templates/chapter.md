---
jupytext:
  text_representation:
    format_name: myst
kernelspec:
  display_name: Python 3
  language: python
  name: python3
# Chapter authoring template: COPY this file, do not edit it in place.
# Fill every section. Keep prose digestible: intuition and the runnable
# example come BEFORE heavy math. Put derivations behind togglebuttons.
#
# NAME EVERY HEADING AFTER ITS CONTENT. The generic labels below (in <angle
# brackets>) are prompts, not headings to keep. A reader who opens the page
# mid-scroll should know from the page-TOC alone what each section says. The
# published chapters model this: "When flow remembers where it came from",
# "Two questions you can ask of a network", "Nine triangles, three super-groups".
# Never ship a heading literally called Motivation / Intuition / Theory.
---

# {Chapter title}

{Kind badge on the first line, one of:
`{bdg-info-line}\`Concept\`` · `{bdg-warning-line}\`Flow model\`` ·
`{bdg-success-line}\`How-to\`` · `{bdg-primary-line}\`Workflow\`` ·
`{bdg-secondary-line}\`Robustness\``}

```{admonition} At a glance
:class: tip
{One or two plain sentences: what this chapter covers and why you care. No
triads, no slogans.}
```

## <The question this answers: name it after the content>

{2–4 short paragraphs. Open with the question the representation or feature
answers, framed through flow, before any mechanics.}

## <The mental model: name it after the analogy or idea>

{Plain-language picture, before any equation. Earn the reader's trust with an
analogy the way the map-equation chapter uses street names.}

:::{toggle}
**{Name this toggle after the derivation it hides}**

{Heavy math here, hidden by default so the page breathes.}
:::

## <The worked example: name it after what it shows>

```{code-cell} python
# Self-contained, runs at build time. Uses its own small dataset, NOT a copy of
# the matching survey notebook. Construct any random graph with Python's
# `random` (version-stable), and consume a Result before the next run() on the
# same Network. If prose reads a specific number off the run, assert it here.
import infomap
# ...
```

```{code-cell} python
from docs_viz import draw_partition
fig = draw_partition(graph, modules)
fig  # rendered inline
```

## Options

{Flow-model chapters: a table of the options this model adds, each row linking
the option to {func}\`infomap.run\` or {class}\`infomap.Network\`. How-to and
workflow chapters: end with a short "## Pitfalls" section only when the chapter
has concrete gotchas worth flagging -- it is optional, not required.}

## API pointers

- {`{func}` / `{class}` / `{meth}` / `{attr}` links into the API reference.}

## Going deeper

{Two or three bullets, ordered by usefulness for THIS chapter — usually the
source paper first, then the survey section with its companion notebook folded
into the same bullet. Vary the sentence shapes between chapters; identical
fill-in-the-blank bullets across chapters read as mail-merge. Citation form:
parenthetical {cite:p} when the finding is the point, narrative {cite:t} when
the paper itself is the subject. The one hard rule: the prose never
congratulates its own authors.}

- Source paper for {topic} {cite:p}`{key}`.
- The survey (§{N}) covers {topic} {cite:p}`smiljanic2026survey`; its companion
  notebook is {`examples/notebooks/<name>.ipynb`}.
