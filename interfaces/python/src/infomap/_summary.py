"""Summary / ``__repr__`` / HTML rendering for :class:`infomap.Infomap`.

Pure presentation logic: every function takes an Infomap instance and reads from
it; nothing here mutates state or touches the C++ network construction path.
"""

from html import escape

_REPR_MAX_FLOW_FRACTION = 0.90
_REPR_MAX_MODULES = 12
_REPR_COLORS = (
    "#4477aa",
    "#ee6677",
    "#228833",
    "#ccbb44",
    "#66ccee",
    "#aa3377",
    "#bbbbbb",
    "#000000",
    "#994455",
    "#ddcc77",
    "#117733",
    "#88ccee",
)
_REPR_OTHER_COLOR = "#8f9499"


def summary_data(im):
    # Read state via the engine core / Result snapshot, never the deprecated
    # public Infomap accessors, keeping summary()/repr() on the supported surface.
    core = im._core
    data = {
        "nodes": im.num_nodes,
        "links": im.num_links,
        "physical_nodes": im.num_physical_nodes,
        "state_nodes": im.num_nodes - im.num_physical_nodes,
        "higher_order": im.num_nodes != im.num_physical_nodes,
        "status": "not run",
    }

    if core.numTopModules() == 0:
        return data

    result = im._result
    data.update(
        {
            "status": "run",
            "state_input": core.stateInput,
            "multilayer_input": core.multilayerInput,
            "multilayer_network": core.isMultilayerNetwork(),
            "levels": result.num_levels,
            "leaf_nodes": core.numLeafNodes(),
            "top_modules": result.num_top_modules,
            "non_trivial_top_modules": result.num_non_trivial_top_modules,
            "leaf_modules": result.num_leaf_modules,
            "effective_top_modules": result.effective_num_top_modules,
            "effective_leaf_modules": result.effective_num_leaf_modules,
            "codelength": result.codelength,
            "index_codelength": result.index_codelength,
            "module_codelength": result.module_codelength,
            "one_level_codelength": result.one_level_codelength,
            "relative_codelength_savings": result.relative_codelength_savings,
            "entropy_rate": result.entropy_rate,
            "elapsed_time": result.elapsed_time,
        }
    )
    if result.meta_codelength != 0:
        data["meta_codelength"] = result.meta_codelength
    return data


def _format_summary_value(value):
    if isinstance(value, bool):
        return "Yes" if value else "No"
    if isinstance(value, float):
        return f"{value:.6g}"
    return str(value)


def _format_percent(value):
    return f"{100 * value:.3g}%"


def _top_module_flow_bars(im):
    modules = [
        (module.module_id, module.flow)
        for module in im._result.tree(1)
        if module.depth == 1
    ]
    total_flow = sum(flow for _, flow in modules)
    if total_flow <= 0:
        return []

    bars = []
    cumulative_flow = 0.0
    for module_id, flow in sorted(modules, key=lambda item: item[1], reverse=True):
        if bars and (
            cumulative_flow >= _REPR_MAX_FLOW_FRACTION * total_flow
            or len(bars) >= _REPR_MAX_MODULES
        ):
            break
        bars.append(
            {
                "label": f"Module {module_id}",
                "flow": flow,
                "fraction": flow / total_flow,
                "color": _REPR_COLORS[len(bars) % len(_REPR_COLORS)],
            }
        )
        cumulative_flow += flow

    other_flow = total_flow - cumulative_flow
    if other_flow > 1e-12:
        bars.append(
            {
                "label": "Other modules",
                "flow": other_flow,
                "fraction": other_flow / total_flow,
                "color": _REPR_OTHER_COLOR,
            }
        )
    return bars


def _html_metric(label, value, *, value_class=""):
    return (
        '<div class="infomap-metric">'
        f'<span class="infomap-metric-label">{escape(label)}</span>'
        f'<span class="infomap-metric-value {escape(value_class)}">'
        f"{escape(_format_summary_value(value))}"
        "</span>"
        "</div>"
    )


def _html_flow_strip(bars):
    if not bars:
        return ""

    segments = []
    legend = []
    for bar in bars:
        label = escape(bar["label"])
        percent = escape(_format_percent(bar["fraction"]))
        color = escape(bar["color"])
        width = max(bar["fraction"] * 100, 0.25)
        segments.append(
            '<span class="infomap-flow-segment" '
            f'style="width: {width:.6g}%; background: {color};" '
            f'title="{label}: {percent}"></span>'
        )
        legend.append(
            '<span class="infomap-flow-item">'
            f'<span class="infomap-flow-swatch" style="background: {color};"></span>'
            f"{label} {percent}"
            "</span>"
        )

    return (
        '<div class="infomap-flow">'
        '<div class="infomap-section-label">Top-module flow</div>'
        f'<div class="infomap-flow-strip">{"".join(segments)}</div>'
        f'<div class="infomap-flow-legend">{"".join(legend)}</div>'
        "</div>"
    )


def repr_html(im):
    summary = im.summary()
    is_run = summary["status"] == "run"
    status = "Run" if is_run else "Not run"
    status_class = "is-run" if is_run else "is-not-run"

    primary_fields = []
    secondary_fields = [
        ("Nodes", summary["nodes"]),
        ("Links", summary["links"]),
        ("Higher-order", summary["higher_order"]),
    ]
    if summary["physical_nodes"] != summary["nodes"]:
        secondary_fields.insert(1, ("Physical nodes", summary["physical_nodes"]))

    if is_run:
        primary_fields = [
            ("Codelength", summary["codelength"]),
            (
                "Relative savings",
                _format_percent(summary["relative_codelength_savings"]),
            ),
            ("Top modules", summary["top_modules"]),
            ("Effective modules", summary["effective_top_modules"]),
        ]
        secondary_fields.extend(
            [
                ("Levels", summary["levels"]),
                ("Elapsed time", f"{summary['elapsed_time']:.3g}s"),
            ]
        )

    metrics = [_html_metric("Status", status, value_class=status_class)]
    metrics.extend(_html_metric(label, value) for label, value in primary_fields)
    metrics.extend(_html_metric(label, value) for label, value in secondary_fields)
    flow_strip = _html_flow_strip(_top_module_flow_bars(im)) if is_run else ""

    return f"""
<style>
.infomap-card {{
  --infomap-border: color-mix(in srgb, currentColor 18%, transparent);
  --infomap-muted: color-mix(in srgb, currentColor 62%, transparent);
  --infomap-soft: color-mix(in srgb, currentColor 7%, transparent);
  border: 1px solid var(--infomap-border);
  border-radius: 8px;
  color: inherit;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  max-width: 720px;
  padding: 14px 16px 16px;
}}
.infomap-title {{
  font-size: 16px;
  font-weight: 650;
  line-height: 1.25;
  margin: 0 0 12px;
}}
.infomap-grid {{
  display: grid;
  gap: 8px;
  grid-template-columns: repeat(auto-fit, minmax(128px, 1fr));
}}
.infomap-metric {{
  background: var(--infomap-soft);
  border-radius: 6px;
  min-width: 0;
  padding: 8px 10px;
}}
.infomap-metric-label {{
  color: var(--infomap-muted);
  display: block;
  font-size: 11px;
  line-height: 1.25;
  margin-bottom: 3px;
}}
.infomap-metric-value {{
  display: block;
  font-size: 15px;
  font-weight: 620;
  line-height: 1.25;
  overflow-wrap: anywhere;
}}
.infomap-metric-value.is-run {{ color: #228833; }}
.infomap-metric-value.is-not-run {{ color: var(--infomap-muted); }}
.infomap-flow {{ margin-top: 14px; }}
.infomap-section-label {{
  color: var(--infomap-muted);
  font-size: 11px;
  margin-bottom: 6px;
}}
.infomap-flow-strip {{
  border-radius: 999px;
  display: flex;
  height: 12px;
  overflow: hidden;
  width: 100%;
}}
.infomap-flow-segment {{ display: block; min-width: 1px; }}
.infomap-flow-legend {{
  color: var(--infomap-muted);
  display: flex;
  flex-wrap: wrap;
  font-size: 11px;
  gap: 6px 12px;
  line-height: 1.3;
  margin-top: 8px;
}}
.infomap-flow-item {{ white-space: nowrap; }}
.infomap-flow-swatch {{
  border-radius: 999px;
  display: inline-block;
  height: 8px;
  margin-right: 4px;
  vertical-align: -1px;
  width: 8px;
}}
</style>
<div class="infomap-card">
  <div class="infomap-title">Infomap</div>
  <div class="infomap-grid">{"".join(metrics)}</div>
  {flow_strip}
</div>
"""


def repr_text(im):
    summary = summary_data(im)
    fields = (
        "nodes",
        "links",
        "physical_nodes",
        "state_nodes",
        "status",
        "multilayer_network",
        "levels",
        "top_modules",
        "codelength",
    )
    details = [f"{field}={summary[field]!r}" for field in fields if field in summary]
    return f"Infomap({', '.join(details)})"
