#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path

from parameter_catalog import GROUPS, ParameterCatalog


REPO_ROOT = Path(__file__).resolve().parents[1]
OVERRIDES = REPO_ROOT / "interfaces" / "parameters" / "overrides.json"

PYTHON_OUT = Path("interfaces/python/src/infomap/_options.py")
R_OUT = Path("interfaces/R/infomap/R/options.R")
TS_OUT = Path("interfaces/js/src/arguments.ts")
FACADE_OUT = Path("interfaces/python/src/infomap/_facade.py")

FACADE_BEGIN = (
    "    # === BEGIN generated: Infomap option signatures "
    "(scripts/generate_binding_options.py) ==="
)
FACADE_END = "    # === END generated ==="

# Facade-only parameters that are NOT catalog options (rendered verbatim).
# `pretty` is a deprecated no-op kept for backward compatibility. It is inserted
# into the natural catalog order right after the catalog param named here.
_FACADE_ONLY_PARAMS = {
    "pretty": {
        # None (not False) so an explicitly passed value is distinguishable
        # from the default and can trigger the DeprecationWarning below --
        # the same sentinel trick as include_self_links.
        "default": "None",
        "doc_type": "bool | None, optional",
        "doc": (
            "Deprecated. Accepted for backward compatibility; has no effect. "
            "Passing it explicitly emits a DeprecationWarning."
        ),
        # Deprecated no-op; never part of the common signature tier.
        "tier": "advanced",
    },
}

# Emitted verbatim into the generated Infomap.__init__/run bodies, at the
# public boundary so stacklevel=2 attributes the warning to user code.
_PRETTY_WARNING_LINES = [
    "        if pretty is not None:",
    "            warnings.warn(",
    '                "pretty is deprecated and has no effect",',
    "                DeprecationWarning,",
    "                stacklevel=2,",
    "            )",
]

# Where each facade-only param is spliced into the natural catalog order: it is
# emitted immediately after this catalog parameter in Infomap.__init__/run.
_FACADE_ONLY_AFTER = {
    "pretty": "silent",
}


def load_parameters(infomap_bin: Path) -> list[dict]:
    result = subprocess.run(
        [str(infomap_bin.resolve()), "--print-json-parameters"],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(result.stdout)["parameters"]


def load_overrides() -> dict:
    overrides = json.loads(OVERRIDES.read_text(encoding="utf-8"))
    for language, entries in overrides.get("bindingOnly", {}).items():
        for entry in entries:
            if not entry.get("reason"):
                raise RuntimeError(
                    f"bindingOnly entry for {language}:{entry.get('name')} must include a reason"
                )
    for language, entries in overrides.get("hiddenBindings", {}).items():
        for entry in entries:
            if not entry.get("reason"):
                raise RuntimeError(
                    f"hiddenBindings entry for {language}:{entry.get('flag')} must include a reason"
                )
    return overrides


def wrap_doc(text: str, prefix: str, width: int = 88) -> list[str]:
    words = text.replace("`", "``").split()
    lines: list[str] = []
    line = prefix
    for word in words:
        candidate = f"{line} {word}" if line.strip() else f"{prefix}{word}"
        if len(candidate) > width and line != prefix:
            lines.append(line.rstrip())
            line = f"{prefix}{word}"
        else:
            line = candidate
    lines.append(line.rstrip() if line.strip() else prefix.rstrip())
    return lines


def _facade_params(catalog: ParameterCatalog):
    """Natural-order facade parameters: ``(ordered_names, info_by_name)``.

    The order is the catalog order -- the same order the generator emits the
    dataclass fields and ``_construct_args`` -- with ``include_self_links``
    leading the ``Input`` group and each facade-only param (e.g. ``pretty``)
    spliced in right after its anchor catalog param.
    """
    index = {}
    names: list[str] = []
    include_self_links = catalog.binding_only_entry("python", "include_self_links")
    index["include_self_links"] = {
        "type": include_self_links.type,
        "default": include_self_links.default,
        "run_default": include_self_links.default,
        "doc_type": "bool, optional",
        "doc": (
            "Deprecated. Self-links are included by default; use "
            "no_self_links=True to exclude them."
        ),
        # Deprecated alias; never part of the common signature tier.
        "tier": "advanced",
    }
    # Map each anchor catalog name to the facade-only params that follow it.
    after = {}
    for facade_name, anchor in _FACADE_ONLY_AFTER.items():
        after.setdefault(anchor, []).append(facade_name)
    for facade_name, info in _FACADE_ONLY_PARAMS.items():
        index[facade_name] = info

    catalog_names = set()
    for group in GROUPS:
        if group == "Input":
            names.append("include_self_links")
        for param in catalog.grouped()[group]:
            name = param.name("python")
            catalog_names.add(name)
            index[name] = {
                "type": param.python_type(),
                "default": param.python_default_expr(),
                # Signature-tier metadata (issue #738/#739): "common" params
                # stay as real keyword parameters in the 3.0 signatures;
                # "advanced" params move to Options. Not yet consumed by the
                # emitters -- the deprecation pass (#741) and the 3.0
                # signature cut (#748) read it from here.
                "tier": param.tier("python"),
                # ``Infomap.run`` re-renders its keywords on top of the
                # constructed state, and a rendered flag can only switch on. A
                # generic boolean flag whose binding default is truthy
                # (silent=True) would re-render every run and override the
                # constructor's choice, so the run signature keeps the flag's
                # no-op default. Other policies (repeated_short etc.) render
                # nothing at their binding default already.
                "run_default": (
                    "False"
                    if param.render_policy == "flag"
                    else param.python_default_expr()
                ),
                "doc_type": param.python_doc_type(),
                "doc": param.python_doc_description(),
            }
            names.append(name)
            for facade_name in after.get(name, ()):
                names.append(facade_name)
    # A facade-only param is spliced in after its anchor catalog param; if the
    # anchor were renamed/removed the param would be silently dropped. Fail loud.
    missing_anchors = set(after) - catalog_names
    if missing_anchors:
        raise ValueError(
            "facade-only param anchor(s) not present in the catalog -- the "
            f"param(s) would be silently dropped: {sorted(missing_anchors)}"
        )
    return names, index


def generate_python(catalog: ParameterCatalog) -> str:
    grouped = catalog.grouped()
    include_self_links = catalog.binding_only_entry("python", "include_self_links")
    lines = [
        "from __future__ import annotations",
        "",
        "import os",
        "import sys",
        "import warnings",
        "from dataclasses import dataclass, fields",
        "",
        "",
        "# Generated by scripts/generate_binding_options.py from ./Infomap --print-json-parameters.",
        "# Edit src/io/ParameterCatalog.cpp or interfaces/parameters/overrides.json, then run",
        "# make build-binding-options.",
        "",
        "_PACKAGE_PREFIX = os.path.dirname(os.path.abspath(__file__)) + os.sep",
        "",
        "",
        "def _external_stacklevel() -> int:",
        "    # Compute a warnings.warn stacklevel that attributes the warning to",
        "    # the first caller frame outside this package, whichever public entry",
        "    # point (Infomap(), Infomap.run(), infomap.run(), Options.to_args())",
        "    # reached this module. Python 3.12's skip_file_prefixes does this",
        "    # natively; the supported floor is 3.11.",
        "    frame = sys._getframe(1)",
        "    level = 1",
        "    while frame is not None and frame.f_code.co_filename.startswith(",
        "        _PACKAGE_PREFIX",
        "    ):",
        "        frame = frame.f_back",
        "        level += 1",
        "    return level",
        "",
    ]
    for group in GROUPS:
        spec_name = f"_{group.upper()}_OPTION_SPECS"
        lines.append(f"{spec_name} = (")
        for param in grouped[group]:
            if not param.uses_generic_spec():
                continue
            name = param.name("python")
            kind = param.spec_kind
            include = "None" if kind == "flag" else param.python_include_expr()
            lines.append(f'    ("{kind}", "{name}", "{param.flag}", {include}),')
        lines.append(")")
        lines.append("")

    lines.extend(
        [
            "",
            "def _append_option_specs(parts, options, specs):",
            "    for option_type, option_name, flag, include in specs:",
            "        value = options[option_name]",
            '        if option_type == "flag":',
            "            if value:",
            "                parts.append(flag)",
            "            continue",
            "        if include(value):",
            '            parts.append(f"{flag} {value}")',
            "",
            "",
            "def _join_args(base_args, parts):",
            "    if not parts:",
            '        return "" if base_args is None else base_args',
            "    if not base_args:",
            "        return f\" {' '.join(parts)}\"",
            "    return f\"{base_args} {' '.join(parts)}\"",
            "",
            "",
            "@dataclass(frozen=True, slots=True)",
            "class Options:",
            '    """Reusable Infomap keyword options.',
            "",
            "    This class mirrors the keyword arguments accepted by :class:`infomap.Infomap`",
            "    and :meth:`infomap.Infomap.run`. Construct it like any dataclass",
            "    (``Options(num_trials=10)``) -- unknown keys raise, as usual -- use",
            "    :meth:`to_args` to render command-line flags, or pass an instance to",
            "    :func:`infomap.run` via ``infomap.run(input, options=options)`` to apply a",
            "    reusable configuration.",
            "",
            "    Parameters",
            "    ----------",
        ]
    )
    for group in GROUPS:
        for param in grouped[group]:
            name = param.name("python")
            description = param.python_doc_description()
            lines.append(f"    {name} : {param.python_doc_type()}")
            lines.extend(wrap_doc(description, "        "))
    lines.append("    include_self_links : bool, optional")
    lines.extend(
        wrap_doc(
            "Deprecated. Self-links are included by default; use no_self_links=True to exclude them.",
            "        ",
        )
    )
    lines.extend(['    """', ""])
    for group in GROUPS:
        lines.append(f"    # {group.lower()}")
        if group == "Input":
            lines.append(
                f"    include_self_links: {include_self_links.type} = {include_self_links.default}"
            )
        for param in grouped[group]:
            lines.append(
                f"    {param.name('python')}: {param.python_type()} = {param.python_default_expr()}"
            )
    lines.extend(
        [
            "",
            "    @classmethod",
            "    def _from_locals(cls, mapping):",
            "        # Internal: build from a locals() dict, dropping the non-field",
            "        # entries (self/args/cls). Public construction is the dataclass",
            "        # constructor, Options(**mapping), which rejects unknown keys.",
            "        return cls(**{name: mapping[name] for name in _OPTION_FIELD_NAMES if name in mapping})",
            "",
            "    def to_kwargs(self):",
            '        """Return the options as a keyword-argument dict.',
            "",
            "        Returns",
            "        -------",
            "        dict",
            "            A dict with one entry per option field, suitable for",
            "            ``Infomap(**options.to_kwargs())``.",
            '        """',
            "        return {name: getattr(self, name) for name in _OPTION_FIELD_NAMES}",
            "",
            "    def to_args(self, base_args: str | None = None):",
            '        """Render the options as an Infomap command-line argument string.',
            "",
            "        Options that keep their default values render no flags.",
            "",
            "        Parameters",
            "        ----------",
            "        base_args : str, optional",
            "            Raw Infomap arguments to prepend before the rendered flags.",
            "",
            "        Returns",
            "        -------",
            "        str",
            "            The rendered argument string, prefixed by ``base_args`` if",
            "            given.",
            '        """',
            "        options = self.to_kwargs()",
            "        rendered_args = []",
            "",
            "        if self.include_self_links is not None:",
            "            warnings.warn(",
            '                "include_self_links is deprecated, use no_self_links to exclude self-links",',
            "                DeprecationWarning,",
            "                stacklevel=_external_stacklevel(),",
            "            )",
            "",
            "        _append_option_specs(rendered_args, options, _INPUT_OPTION_SPECS)",
            "",
            "        if (self.include_self_links is not None and not self.include_self_links) or self.no_self_links:",
            '            rendered_args.append("--no-self-links")',
            "",
            "        _append_option_specs(rendered_args, options, _OUTPUT_OPTION_SPECS)",
            "",
            "        if self.verbosity_level > 1:",
            "            rendered_args.append(f\"-{'v' * self.verbosity_level}\")",
            "",
            "        if self.output is not None:",
            "            rendered_args.append(f\"--output {','.join(self.output)}\")",
            "",
            "        _append_option_specs(rendered_args, options, _ALGORITHM_OPTION_SPECS)",
            "",
            "        if self.directed is not None:",
            '            rendered_args.append("--directed" if self.directed else "--flow-model undirected")',
            "",
            "        _append_option_specs(rendered_args, options, _ACCURACY_OPTION_SPECS)",
            "",
            "        if self.fast_hierarchical_solution is not None:",
            "            rendered_args.append(f\"-{'F' * self.fast_hierarchical_solution}\")",
            "",
            "        return _join_args(base_args, rendered_args)",
            "",
            "",
            "_OPTION_FIELD_NAMES = tuple(field.name for field in fields(Options))",
            "",
            "",
            "# Options is the canonical public name; InfomapOptions stays as a back-compat alias.",
            "InfomapOptions = Options",
            "",
            "",
            "def _construct_args(",
            "    args=None,",
        ]
    )
    for group in GROUPS:
        lines.append(f"    # {group.lower()}")
        if group == "Input":
            lines.append(f"    include_self_links={include_self_links.default},")
        for param in grouped[group]:
            lines.append(f"    {param.name('python')}={param.python_default_expr()},")
    lines.extend(
        [
            "):",
            "    return Options._from_locals(locals()).to_args(base_args=args)",
            "",
        ]
    )
    return "\n".join(lines)


def generate_r(catalog: ParameterCatalog) -> str:
    grouped = catalog.grouped()
    include_self_links = catalog.binding_only_entry("r", "include_self_links")
    lines = [
        "# Defaults, option specs, and CLI argument construction.",
        "# Generated by scripts/generate_binding_options.py from ./Infomap --print-json-parameters.",
        "# Edit src/io/ParameterCatalog.cpp or interfaces/parameters/overrides.json, then run",
        "# make build-binding-options.",
        "",
    ]
    lines.extend(
        [
            ".skip_when_null <- function(x) !is.null(x)",
            ".skip_when_not_equal <- function(default) function(x) !identical(x, default)",
            "",
        ]
    )
    for group in GROUPS:
        lines.append(f"{group.upper()}_OPTIONS <- list(")
        specs = []
        for param in grouped[group]:
            if not param.uses_generic_spec():
                continue
            kind = param.spec_kind
            name = param.name("r")
            default = param.r_default()
            include = "" if kind == "flag" else f", include = {param.r_include_expr()}"
            specs.append(
                f'  list(type = "{kind}", name = "{name}", flag = "{param.flag}", default = {default}{include})'
            )
        lines.append(",\n".join(specs))
        lines.append(")")
        lines.append("")
    fields = []
    for group in GROUPS:
        if group == "Input":
            fields.append("include_self_links")
        fields.extend(param.name("r") for param in grouped[group])
    lines.append("OPTION_FIELD_NAMES <- c(")
    for i in range(0, len(fields), 4):
        lines.append(
            "  "
            + ", ".join(json.dumps(field) for field in fields[i : i + 4])
            + ("," if i + 4 < len(fields) else "")
        )
    lines.append(")")
    lines.append("")
    lines.append("OPTION_DEFAULTS <- list(")
    defaults = [f"include_self_links = {include_self_links.default}"]
    for group in GROUPS:
        for param in grouped[group]:
            defaults.append(f"{param.name('r')} = {param.r_default()}")
    for i, default in enumerate(defaults):
        suffix = "," if i + 1 < len(defaults) else ""
        lines.append(f"  {default}{suffix}")
    lines.append(")")
    lines.extend(
        [
            "",
            "#' Build a reusable Infomap options list",
            "#'",
            "#' Returns a named list with one entry per Infomap CLI option. All",
            "#' arguments default to the Infomap-internal defaults, so callers only",
            "#' need to supply the options they want to override. The returned list",
            "#' is accepted by [Infomap()] and `Infomap$run()`.",
            "#'",
            "#' Argument names mirror the Infomap CLI flags (with hyphens replaced by",
            "#' underscores) and the keyword arguments accepted by the Python interface.",
            "#'",
            "#' @param ... Named option overrides. See **Options** below.",
            "#'",
            "#' @section Options:",
        ]
    )
    for group in GROUPS:
        lines.append(f"#' {group}")
        lines.append("#' \\describe{")
        if group == "Input":
            lines.append(
                "#'   \\item{`include_self_links`}{Deprecated. Self-links are included by default; use `no_self_links = TRUE` to exclude them.}"
            )
        for param in grouped[group]:
            name = param.name("r")
            desc = (
                param.description.replace("\\", "\\\\")
                .replace("{", "\\{")
                .replace("}", "\\}")
                .replace("[", "\\[")
                .replace("]", "\\]")
            )
            lines.append(f"#'   \\item{{`{name}`}}{{{desc}}}")
        lines.append("#' }")
        lines.append("#'")
    lines.extend(
        [
            "#' @return A named list of options.",
            "#' @examples",
            "#' opts <- infomap_options(num_trials = 10, two_level = TRUE)",
            "#' im <- Infomap(opts = opts, silent = TRUE)",
            "#' @export",
            "infomap_options <- function(...) {",
            "  overrides <- list(...)",
            "  unknown <- setdiff(names(overrides), OPTION_FIELD_NAMES)",
            "  if (length(unknown) > 0L) {",
            "    stop(",
            '      "Unknown infomap options: ", paste(unknown, collapse = ", "),',
            '      ". See ?infomap_options for the full list.",',
            "      call. = FALSE",
            "    )",
            "  }",
            "  opts <- OPTION_DEFAULTS",
            "  for (name in names(overrides)) {",
            "    opts[[name]] <- overrides[[name]]",
            "  }",
            "  opts",
            "}",
            "",
            ".append_specs <- function(parts, opts, specs) {",
            "  for (spec in specs) {",
            "    value <- opts[[spec$name]]",
            '    if (identical(spec$type, "flag")) {',
            "      if (isTRUE(value)) parts <- c(parts, spec$flag)",
            "      next",
            "    }",
            "    include <- if (is.null(spec$include)) function(v) !is.null(v) && !identical(v, spec$default) else spec$include",
            "    if (isTRUE(include(value))) {",
            "      parts <- c(parts, paste(spec$flag, format_value(value)))",
            "    }",
            "  }",
            "  parts",
            "}",
            "",
            "format_value <- function(value) {",
            "  if (is.character(value)) return(value)",
            '  if (is.logical(value)) return(if (isTRUE(value)) "true" else "false")',
            "  if (is.numeric(value)) {",
            "    if (is.integer(value) || (is.finite(value) && value == as.integer(value))) {",
            "      return(format(as.integer(value), scientific = FALSE))",
            "    }",
            "    return(format(value, scientific = FALSE, trim = TRUE))",
            "  }",
            "  as.character(value)",
            "}",
            "",
            "#' Render an Infomap options list to a CLI argument string",
            "#'",
            "#' This is exported for advanced use; most callers should pass options",
            "#' directly to [Infomap()] or `Infomap$run()`.",
            "#'",
            "#' @param args Optional raw argument string to prepend.",
            "#' @param opts Options list from [infomap_options()] (or `NULL`).",
            "#'",
            "#' @return A single character string of CLI arguments.",
            "#' @export",
            "construct_args <- function(args = NULL, opts = NULL) {",
            "  if (is.null(opts)) opts <- OPTION_DEFAULTS",
            "  parts <- character(0)",
            "",
            "  if (!is.null(opts$include_self_links)) {",
            "    .Deprecated(",
            '      new = "no_self_links",',
            '      package = "infomap",',
            '      old = "include_self_links",',
            '      msg = "include_self_links is deprecated; use no_self_links = TRUE to exclude self-links."',
            "    )",
            "  }",
            "",
            "  parts <- .append_specs(parts, opts, INPUT_OPTIONS)",
            "",
            "  if ((!is.null(opts$include_self_links) && !isTRUE(opts$include_self_links)) ||",
            "      isTRUE(opts$no_self_links)) {",
            '    parts <- c(parts, "--no-self-links")',
            "  }",
            "",
            "  parts <- .append_specs(parts, opts, OUTPUT_OPTIONS)",
            "",
            "  if (isTRUE(opts$verbosity_level > 1L)) {",
            '    parts <- c(parts, paste0("-", strrep("v", opts$verbosity_level)))',
            "  }",
            "",
            "  if (!is.null(opts$output)) {",
            '    parts <- c(parts, paste("--output", paste(opts$output, collapse = ",")))',
            "  }",
            "  parts <- .append_specs(parts, opts, ALGORITHM_OPTIONS)",
            "",
            "  if (!is.null(opts$directed)) {",
            '    parts <- c(parts, if (isTRUE(opts$directed)) "--directed" else "--flow-model undirected")',
            "  }",
            "  parts <- .append_specs(parts, opts, ACCURACY_OPTIONS)",
            "",
            "  if (!is.null(opts$fast_hierarchical_solution)) {",
            '    parts <- c(parts, paste0("-", strrep("F", opts$fast_hierarchical_solution)))',
            "  }",
            "",
            '  rendered <- paste(parts, collapse = " ")',
            "  if (is.null(args) || !nzchar(args)) {",
            "    rendered",
            "  } else if (length(parts) == 0L) {",
            "    args",
            "  } else {",
            "    paste(args, rendered)",
            "  }",
            "}",
            "",
        ]
    )
    return "\n".join(lines)


def generate_ts(catalog: ParameterCatalog) -> str:
    visible_parameters = catalog.visible_parameters("ts")
    grouped = {
        group: [param for param in visible_parameters if param.group == group]
        for group in GROUPS
    }
    about_options = catalog.binding_only("ts")
    output_param = next(param for param in visible_parameters if param.flag == "--output")
    output_choices = output_param.choices or []
    lines = [
        "// Generated by scripts/generate_binding_options.py from ./Infomap --print-json-parameters.",
        "// Edit src/io/ParameterCatalog.cpp or interfaces/parameters/overrides.json, then run",
        "// make build-binding-options.",
        "",
        "type OutputFormats =",
    ]
    for choice in output_choices:
        lines.append(f'  | "{choice}"')
    lines[-1] += ";"
    lines.extend(["", "export type Arguments = Partial<{"])
    for group in GROUPS:
        lines.append(f"  // {group.lower()}")
        for param in grouped[group]:
            name = param.name("ts")
            typ = param.ts_type()
            if typ.startswith("\n"):
                lines.append(f"  {name}:{typ};")
            else:
                lines.append(f"  {name}: {typ};")
    lines.append("  // about")
    for entry in about_options:
        lines.append(f"  {entry.name}: {entry.type};")
    lines.extend(
        [
            "}>;",
            "",
            "export default function argumentsToString(args: Arguments) {",
            '  let result = "";',
            "",
        ]
    )

    def append_result(condition: str, expression: str) -> None:
        one_line = f"  if ({condition}) result += {expression};"
        if len(one_line) <= 80:
            lines.append(one_line)
            return

        lines.append(f"  if ({condition})")
        assignment = f"    result += {expression};"
        if len(assignment) <= 80:
            lines.append(assignment)
            return

        lines.append("    result +=")
        if len(f"      {expression};") <= 80:
            lines.append(f"      {expression};")
            return
        if " + " in expression:
            left, right = expression.rsplit(" + ", 1)
            if len(f"      {left} +") <= 80 and len(f"      {right};") <= 80:
                lines.append(f"      {left} +")
                lines.append(f"      {right};")
                return
        lines.append(f"      {expression};")

    for group in GROUPS:
        for param in grouped[group]:
            flag = param.flag
            name = param.name("ts")
            if param.render_policy == "repeated_short" and flag == "--verbose":
                append_result(
                    f"args.{name}", f'" -" + "v".repeat(args.{name})'
                )
            elif param.render_policy == "repeated_short":
                append_result(
                    f"args.{name}", f'" -" + "F".repeat(args.{name})'
                )
            elif param.render_policy == "comma_list":
                lines.extend(
                    [
                        f"  if (args.{name} != null) {{",
                        f'    if (typeof args.{name} === "string") result += " --output " + args.{name};',
                        f'    else result += " --output " + args.{name}.join(",");',
                        "  }",
                    ]
                )
            elif not param.required:
                append_result(f"args.{name}", f'" {flag}"')
            else:
                append_result(
                    f"args.{name} != null", f'" {flag} " + args.{name}'
                )
            lines.append("")
    for entry in about_options:
        if entry.name == "help":
            lines.append(
                '  if (args.help) result += args.help === "advanced" ? " -hh" : " -h";'
            )
        elif entry.name == "version":
            lines.append('  if (args.version) result += " --version";')
        else:
            raise RuntimeError(
                f"Unsupported TypeScript binding-only option: {entry.name}"
            )
        lines.append("")
    lines.extend(["  return result;", "}", ""])
    return "\n".join(lines)


def _render_facade_signature(names, index, indent="        ", context="init"):
    lines = []
    for name in names:
        if name in _FACADE_ONLY_PARAMS:
            default = _FACADE_ONLY_PARAMS[name]["default"]
        elif context == "run":
            default = index[name]["run_default"]
        else:
            default = index[name]["default"]
        lines.append(f"{indent}{name}={default},")
    return lines


def _render_facade_docstring_params(names, index):
    lines = []
    for name in names:
        info = index[name]
        if info["doc"] is None:
            continue
        lines.append(f"        {name} : {info['doc_type']}")
        lines.extend(wrap_doc(info["doc"], "            "))
    return lines


def generate_facade(catalog: ParameterCatalog) -> str:
    names, index = _facade_params(catalog)

    lines = [FACADE_BEGIN]
    # ---- __init__ ----
    lines.append("    def __init__(")
    lines.append("        self,")
    lines.append("        args=None,")
    lines.extend(_render_facade_signature(names, index))
    lines.append("    ):")
    lines.append('        """Create a new Infomap instance.')
    lines.append("")
    lines.append("        Keyword arguments mirror the Infomap CLI flags. Use")
    lines.append("        :class:`Options` for a reusable")
    lines.append("        configuration object and the full parameter reference.")
    lines.append("")
    lines.append("        Parameters")
    lines.append("        ----------")
    lines.append("        args : str, optional")
    lines.extend(
        wrap_doc(
            "Raw Infomap arguments to prepend before rendered keyword options.",
            "            ",
        )
    )
    lines.extend(_render_facade_docstring_params(names, index))
    lines.append('        """')
    lines.extend(_PRETTY_WARNING_LINES)
    lines.append("        options = Options._from_locals(locals())")
    lines.append("        self._init_from_options(args, options)")
    lines.append("")
    # ---- run ----
    lines.append("    def run(")
    lines.append("        self,")
    lines.append("        args=None,")
    lines.append("        initial_partition=None,")
    lines.extend(_render_facade_signature(names, index, context="run"))
    lines.append("    ):")
    lines.append('        """Run Infomap.')
    lines.append("")
    lines.append("        Keyword arguments mirror the Infomap CLI flags. Use")
    lines.append("        :class:`Options` for the full parameter reference and")
    lines.append(
        "        :func:`infomap.run` with ``options=`` when reusing a saved configuration."
    )
    lines.append("")
    lines.append(
        "        Boolean flags default to off here and render only when set; a"
    )
    lines.append(
        "        flag chosen at construction stays in effect for every run."
    )
    lines.append("")
    lines.append("        Parameters")
    lines.append("        ----------")
    lines.append("        args : str, optional")
    lines.extend(
        wrap_doc(
            "Raw Infomap arguments to prepend before rendered keyword options.",
            "            ",
        )
    )
    lines.append("        initial_partition : dict, optional")
    lines.extend(
        wrap_doc(
            "Initial partition to use for this run only. See initial_partition.",
            "            ",
        )
    )
    lines.extend(_render_facade_docstring_params(names, index))
    lines.append("")
    lines.append("        Returns")
    lines.append("        -------")
    lines.append("        Result")
    lines.append("            The result of this run. See :class:`~infomap.Result`.")
    lines.append("")
    lines.append("        See Also")
    lines.append("        --------")
    lines.append("        initial_partition")
    lines.append('        """')
    lines.extend(_PRETTY_WARNING_LINES)
    lines.append("        options = Options._from_locals(locals())")
    lines.append(
        "        return self._run_from_options(args, initial_partition, options)"
    )
    lines.append(FACADE_END)
    return "\n".join(lines)


def _splice_marker_block(path: Path, block: str) -> str:
    text = path.read_text(encoding="utf-8")
    begin_idx = text.find(FACADE_BEGIN)
    end_idx = text.find(FACADE_END)
    if begin_idx == -1 or end_idx == -1:
        raise RuntimeError(
            f"Marker block not found in {path}. Expected lines:\n"
            f"  {FACADE_BEGIN}\n  {FACADE_END}"
        )
    end_idx += len(FACADE_END)
    return text[:begin_idx] + block + text[end_idx:]


def outputs(infomap_bin: Path):
    overrides = load_overrides()
    catalog = ParameterCatalog(load_parameters(infomap_bin), overrides)
    generated = {
        PYTHON_OUT: generate_python(catalog),
        R_OUT: generate_r(catalog),
        TS_OUT: generate_ts(catalog),
    }
    facade_block = generate_facade(catalog)
    return generated, facade_block


def write_outputs(generated, facade_block, output_root: Path) -> None:
    # newline="\n": emit LF on every platform so regenerating on Windows can't
    # introduce CRLF diffs against the committed (LF) outputs.
    for rel_path, text in generated.items():
        path = output_root / rel_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
    facade_path = output_root / FACADE_OUT
    facade_path.write_text(
        _splice_marker_block(facade_path, facade_block),
        encoding="utf-8",
        newline="\n",
    )


def check_outputs(generated, facade_block, output_root: Path) -> int:
    failures = []
    for rel_path, text in generated.items():
        path = output_root / rel_path
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            failures.append(rel_path)
    facade_path = output_root / FACADE_OUT
    if not facade_path.exists():
        failures.append(FACADE_OUT)
    else:
        spliced = _splice_marker_block(facade_path, facade_block)
        if facade_path.read_text(encoding="utf-8") != spliced:
            failures.append(FACADE_OUT)
    if not failures:
        print("Tracked binding option outputs are fresh.")
        return 0
    if "GITHUB_ACTIONS" in os.environ:
        print(
            "::error title=Tracked binding option outputs are stale::"
            "Run make build-binding-options and commit the updated files."
        )
    print("Tracked binding option outputs are stale:")
    for path in failures:
        print(f"  {path}")
    print()
    print("Run: make build-binding-options")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--infomap-bin", default="./Infomap")
    parser.add_argument("--output-root", default=".")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    generated, facade_block = outputs(Path(args.infomap_bin))
    output_root = Path(args.output_root)
    if args.check:
        return check_outputs(generated, facade_block, output_root)
    write_outputs(generated, facade_block, output_root)
    for path in generated:
        print(f"Updated {path}")
    print(f"Updated {FACADE_OUT} (generated signature block)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
