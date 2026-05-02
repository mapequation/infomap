#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
OVERRIDES = REPO_ROOT / "interfaces" / "parameters" / "overrides.json"

PYTHON_OUT = Path("interfaces/python/src/infomap/_options.py")
R_OUT = Path("interfaces/R/infomap/R/options.R")
TS_OUT = Path("interfaces/js/src/arguments.ts")

GROUPS = ("Input", "Output", "Algorithm", "Accuracy")

PY_DEFAULTS = {
    "--meta-data-rate": "1.0",
    "--teleportation-probability": "0.15",
    "--regularization-strength": "1.0",
    "--entropy-correction-strength": "1.0",
    "--markov-time": "1.0",
    "--variable-markov-damping": "1.0",
    "--variable-markov-min-scale": "1.0",
    "--multilayer-relax-rate": "0.15",
    "--multilayer-relax-limit": "-1",
    "--multilayer-relax-limit-up": "-1",
    "--multilayer-relax-limit-down": "-1",
    "--seed": "123",
    "--num-trials": "1",
    "--core-loop-limit": "10",
    "--core-loop-codelength-threshold": "1e-10",
    "--tune-iteration-relative-threshold": "1e-5",
}

R_DEFAULTS = {
    "--meta-data-rate": "DEFAULT_META_DATA_RATE",
    "--teleportation-probability": "DEFAULT_TELEPORTATION_PROB",
    "--regularization-strength": "1.0",
    "--entropy-correction-strength": "1.0",
    "--markov-time": "1.0",
    "--variable-markov-damping": "1.0",
    "--variable-markov-min-scale": "1.0",
    "--multilayer-relax-rate": "DEFAULT_MULTILAYER_RELAX_RATE",
    "--seed": "DEFAULT_SEED",
    "--num-trials": "1L",
    "--core-loop-limit": "10L",
    "--core-loop-codelength-threshold": "DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD",
    "--tune-iteration-relative-threshold": "DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD",
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
                raise RuntimeError(f"bindingOnly entry for {language}:{entry.get('name')} must include a reason")
    return overrides


def binding_only(overrides: dict, language: str) -> list[dict]:
    return overrides.get("bindingOnly", {}).get(language, [])


def binding_only_entry(overrides: dict, language: str, name: str) -> dict:
    for entry in binding_only(overrides, language):
        if entry["name"] == name:
            return entry
    raise RuntimeError(f"Missing bindingOnly override for {language}:{name}")


def runtime_parameters(parameters: list[dict]) -> list[dict]:
    return [param for param in parameters if param["group"] in GROUPS]


def snake_name(flag: str) -> str:
    return flag.removeprefix("--").replace("-", "_")


def camel_name(flag: str) -> str:
    parts = flag.removeprefix("--").split("-")
    return parts[0] + "".join(part.capitalize() for part in parts[1:])


def binding_name(flag: str, language: str, overrides: dict) -> str:
    names = overrides.get("names", {}).get(flag, {})
    if language in names:
        return names[language]
    if language == "ts":
        return camel_name(flag)
    return snake_name(flag)


def param_kind(param: dict) -> str:
    return "value" if param["required"] else "flag"


def py_type(param: dict, overrides: dict) -> str:
    if param["long"] in {"--verbose", "--fast-hierarchical-solution"}:
        return "int | None" if param["long"] == "--fast-hierarchical-solution" else "int"
    if param["long"] == "--directed":
        return "bool | None"
    choices = overrides.get("choices", {}).get(param["long"])
    if choices:
        if param["long"] == "--output":
            return "list[str] | tuple[str, ...] | None"
        return "str | None"
    if not param["required"]:
        return "bool"
    long_type = param.get("longType")
    if long_type in {"integer", "probability", "number"}:
        base = "float" if long_type in {"probability", "number"} else "int"
    elif long_type == "list":
        base = "list[str] | tuple[str, ...]"
    else:
        base = "str"
    return base if py_default(param) != "None" else f"{base} | None"


def ts_type(param: dict, overrides: dict) -> str:
    choices = overrides.get("choices", {}).get(param["long"])
    if param["long"] == "--output":
        return "OutputFormats | OutputFormats[]"
    if choices:
        return "\n    | " + "\n    | ".join(json.dumps(choice) for choice in choices)
    if param["incremental"] and param["long"] == "--fast-hierarchical-solution":
        return "1 | 2 | 3"
    if param["incremental"] and param["long"] == "--verbose":
        return "1 | 2 | 3"
    if not param["required"]:
        return "boolean"
    long_type = param.get("longType")
    return "number" if long_type in {"integer", "probability", "number"} else "string"


def py_default(param: dict) -> str:
    if param["long"] == "--verbose":
        return "1"
    if param["long"] in {"--directed", "--fast-hierarchical-solution"}:
        return "None"
    if not param["required"]:
        return "False"
    return PY_DEFAULTS.get(param["long"], "None")


def py_include(param: dict) -> str:
    default = py_default(param)
    if default == "None":
        return "lambda value: value is not None"
    return f"lambda value: value != {default}"


def r_default(param: dict) -> str:
    if param["long"] == "--verbose":
        return "DEFAULT_VERBOSITY_LEVEL"
    if param["long"] in {"--directed", "--fast-hierarchical-solution"}:
        return "NULL"
    if not param["required"]:
        return "FALSE"
    return R_DEFAULTS.get(param["long"], "NULL")


def r_include(param: dict) -> str:
    default = r_default(param)
    if default == "NULL":
        return ".skip_when_null"
    return f".skip_when_not_equal({default})"


def py_doc_type(param: dict, py_name: str, overrides: dict) -> str:
    if py_name == "include_self_links":
        return "bool, optional"
    if param["long"] in {"--verbose", "--fast-hierarchical-solution"}:
        return "int, optional"
    if param["long"] == "--directed":
        return "bool, optional"
    if param["long"] == "--output":
        return "sequence of str, optional"
    if not param["required"]:
        return "bool, optional"
    long_type = param.get("longType")
    if long_type == "integer":
        return "int, optional"
    if long_type in {"number", "probability"}:
        return "float, optional"
    return "str, optional"


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


def grouped_params(params: list[dict]) -> dict[str, list[dict]]:
    return {group: [param for param in params if param["group"] == group] for group in GROUPS}


def generate_python(params: list[dict], overrides: dict) -> str:
    grouped = grouped_params(params)
    include_self_links = binding_only_entry(overrides, "python", "include_self_links")
    lines = [
        "from __future__ import annotations",
        "",
        "import warnings",
        "from dataclasses import dataclass, fields",
        "",
        "",
        "# Generated by scripts/generate_binding_options.py from ./Infomap --print-json-parameters.",
        "# Edit src/io/Config.cpp or interfaces/parameters/overrides.json, then run",
        "# make build-binding-options.",
        "",
    ]
    for const, value in [
        ("_DEFAULT_META_DATA_RATE", "1.0"),
        ("_DEFAULT_VERBOSITY_LEVEL", "1"),
        ("_DEFAULT_TELEPORTATION_PROB", "0.15"),
        ("_DEFAULT_MULTILAYER_RELAX_RATE", "0.15"),
        ("_DEFAULT_SEED", "123"),
        ("_DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD", "1e-10"),
        ("_DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD", "1e-5"),
    ]:
        lines.append(f"{const} = {value}")
    lines.append("")
    lines.append("")

    for group in GROUPS:
        spec_name = f"_{group.upper()}_OPTION_SPECS"
        lines.append(f"{spec_name} = (")
        for param in grouped[group]:
            flag = param["long"]
            name = binding_name(flag, "python", overrides)
            if flag == "--no-self-links":
                continue
            if flag == "--verbose":
                continue
            if flag == "--output":
                continue
            if flag == "--directed":
                continue
            if flag == "--fast-hierarchical-solution":
                continue
            kind = param_kind(param)
            include = "None" if kind == "flag" else py_include(param)
            lines.append(f'    ("{kind}", "{name}", "{flag}", {include}),')
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
            '        return f" {\' \'.join(parts)}"',
            '    return f"{base_args} {\' \'.join(parts)}"',
            "",
            "",
            "@dataclass(slots=True)",
            "class InfomapOptions:",
            '    """Reusable Infomap keyword options.',
            "",
            "    This class mirrors the keyword arguments accepted by :class:`infomap.Infomap`",
            "    and :meth:`infomap.Infomap.run`. Use :meth:`to_args` to render command-line",
            "    flags, :meth:`from_mapping` to construct options from existing keyword",
            "    dicts, or the convenience methods :meth:`infomap.Infomap.from_options` and",
            "    :meth:`infomap.Infomap.run_with_options` to apply a reusable configuration.",
            "",
            "    Parameters",
            "    ----------",
        ]
    )
    for group in GROUPS:
        for param in grouped[group]:
            name = binding_name(param["long"], "python", overrides)
            if name == "verbosity_level":
                description = "Verbosity level on the console. 1 keeps the default output level, 2 renders -vv and so on."
            elif param["long"] == "--fast-hierarchical-solution":
                description = "Find top modules fast. Use 2 to keep all fast levels and 3 to skip the recursive part."
            else:
                description = param["description"]
            lines.append(f"    {name} : {py_doc_type(param, name, overrides)}")
            lines.extend(wrap_doc(description, "        "))
    lines.append("    include_self_links : bool, optional")
    lines.extend(wrap_doc("Deprecated. Self-links are included by default; use no_self_links=True to exclude them.", "        "))
    lines.extend(['    """', ""])
    for group in GROUPS:
        lines.append(f"    # {group.lower()}")
        if group == "Input":
            lines.append(f"    include_self_links: {include_self_links['type']} = {include_self_links['default']}")
        for param in grouped[group]:
            name = binding_name(param["long"], "python", overrides)
            typ = py_type(param, overrides)
            default = py_default(param)
            if name == "verbosity_level":
                default = "_DEFAULT_VERBOSITY_LEVEL"
            elif param["long"] == "--meta-data-rate":
                default = "_DEFAULT_META_DATA_RATE"
            elif param["long"] == "--teleportation-probability":
                default = "_DEFAULT_TELEPORTATION_PROB"
            elif param["long"] == "--multilayer-relax-rate":
                default = "_DEFAULT_MULTILAYER_RELAX_RATE"
            elif param["long"] == "--seed":
                default = "_DEFAULT_SEED"
            elif param["long"] == "--core-loop-codelength-threshold":
                default = "_DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD"
            elif param["long"] == "--tune-iteration-relative-threshold":
                default = "_DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD"
            lines.append(f"    {name}: {typ} = {default}")
    lines.extend(
        [
            "",
            "    @classmethod",
            "    def from_mapping(cls, mapping):",
            "        return cls(**{name: mapping[name] for name in _OPTION_FIELD_NAMES if name in mapping})",
            "",
            "    def to_kwargs(self):",
            "        return {name: getattr(self, name) for name in _OPTION_FIELD_NAMES}",
            "",
            "    def to_args(self, base_args: str | None = None):",
            "        options = self.to_kwargs()",
            "        rendered_args = []",
            "",
            "        if self.include_self_links is not None:",
            "            warnings.warn(",
            '                "include_self_links is deprecated, use no_self_links to exclude self-links",',
            "                DeprecationWarning,",
            "                stacklevel=2,",
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
            '            rendered_args.append(f"-{\'v\' * self.verbosity_level}")',
            "",
            "        if self.output is not None:",
            '            rendered_args.append(f"--output {\',\'.join(self.output)}")',
            "",
            "        _append_option_specs(rendered_args, options, _ALGORITHM_OPTION_SPECS)",
            "",
            "        if self.directed is not None:",
            '            rendered_args.append("--directed" if self.directed else "--flow-model undirected")',
            "",
            "        _append_option_specs(rendered_args, options, _ACCURACY_OPTION_SPECS)",
            "",
            "        if self.fast_hierarchical_solution is not None:",
            '            rendered_args.append(f"-{\'F\' * self.fast_hierarchical_solution}")',
            "",
            "        return _join_args(base_args, rendered_args)",
            "",
            "",
            "_OPTION_FIELD_NAMES = tuple(field.name for field in fields(InfomapOptions))",
            "",
            "",
            "def _construct_args(",
            "    args=None,",
        ]
    )
    for group in GROUPS:
        lines.append(f"    # {group.lower()}")
        if group == "Input":
            lines.append(f"    include_self_links={include_self_links['default']},")
        for param in grouped[group]:
            name = binding_name(param["long"], "python", overrides)
            default = py_default(param)
            if name == "verbosity_level":
                default = "_DEFAULT_VERBOSITY_LEVEL"
            elif param["long"] == "--meta-data-rate":
                default = "_DEFAULT_META_DATA_RATE"
            elif param["long"] == "--teleportation-probability":
                default = "_DEFAULT_TELEPORTATION_PROB"
            elif param["long"] == "--multilayer-relax-rate":
                default = "_DEFAULT_MULTILAYER_RELAX_RATE"
            elif param["long"] == "--seed":
                default = "_DEFAULT_SEED"
            elif param["long"] == "--core-loop-codelength-threshold":
                default = "_DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD"
            elif param["long"] == "--tune-iteration-relative-threshold":
                default = "_DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD"
            lines.append(f"    {name}={default},")
    lines.extend(
        [
            "):",
            "    return InfomapOptions.from_mapping(locals()).to_args(base_args=args)",
            "",
        ]
    )
    return "\n".join(lines)


def r_value(value: str) -> str:
    return value


def generate_r(params: list[dict], overrides: dict) -> str:
    grouped = grouped_params(params)
    include_self_links = binding_only_entry(overrides, "r", "include_self_links")
    lines = [
        "# Defaults, option specs, and CLI argument construction.",
        "# Generated by scripts/generate_binding_options.py from ./Infomap --print-json-parameters.",
        "# Edit src/io/Config.cpp or interfaces/parameters/overrides.json, then run",
        "# make build-binding-options.",
        "",
        "DEFAULT_META_DATA_RATE <- 1.0",
        "DEFAULT_VERBOSITY_LEVEL <- 1L",
        "DEFAULT_TELEPORTATION_PROB <- 0.15",
        "DEFAULT_MULTILAYER_RELAX_RATE <- 0.15",
        "DEFAULT_SEED <- 123L",
        "DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD <- 1e-10",
        "DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD <- 1e-5",
        "",
        ".skip_when_null <- function(x) !is.null(x)",
        ".skip_when_not_equal <- function(default) function(x) !identical(x, default)",
        "",
    ]
    for group in GROUPS:
        lines.append(f"{group.upper()}_OPTIONS <- list(")
        specs = []
        for param in grouped[group]:
            flag = param["long"]
            if flag in {"--no-self-links", "--verbose", "--output", "--directed", "--fast-hierarchical-solution"}:
                continue
            kind = param_kind(param)
            name = binding_name(flag, "r", overrides)
            default = r_default(param)
            include = "" if kind == "flag" else f", include = {r_include(param)}"
            specs.append(f'  list(type = "{kind}", name = "{name}", flag = "{flag}", default = {default}{include})')
        lines.append(",\n".join(specs))
        lines.append(")")
        lines.append("")
    fields = []
    for group in GROUPS:
        if group == "Input":
            fields.append("include_self_links")
        fields.extend(binding_name(param["long"], "r", overrides) for param in grouped[group])
    lines.append("OPTION_FIELD_NAMES <- c(")
    for i in range(0, len(fields), 4):
        lines.append("  " + ", ".join(json.dumps(field) for field in fields[i : i + 4]) + ("," if i + 4 < len(fields) else ""))
    lines.append(")")
    lines.append("")
    lines.append("OPTION_DEFAULTS <- list(")
    defaults = [f"include_self_links = {include_self_links['default']}"]
    for group in GROUPS:
        for param in grouped[group]:
            name = binding_name(param["long"], "r", overrides)
            defaults.append(f"{name} = {r_default(param)}")
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
            lines.append("#'   \\item{`include_self_links`}{Deprecated. Self-links are included by default; use `no_self_links = TRUE` to exclude them.}")
        for param in grouped[group]:
            name = binding_name(param["long"], "r", overrides)
            desc = (
                param["description"]
                .replace("\\", "\\\\")
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


def generate_ts(params: list[dict], overrides: dict) -> str:
    grouped = grouped_params(params)
    about_options = binding_only(overrides, "ts")
    output_choices = overrides["choices"]["--output"]
    lines = [
        "// Generated by scripts/generate_binding_options.py from ./Infomap --print-json-parameters.",
        "// Edit src/io/Config.cpp or interfaces/parameters/overrides.json, then run",
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
            name = binding_name(param["long"], "ts", overrides)
            typ = ts_type(param, overrides)
            if typ.startswith("\n"):
                lines.append(f"  {name}:{typ};")
            else:
                lines.append(f"  {name}: {typ};")
    lines.append("  // about")
    for entry in about_options:
        lines.append(f"  {entry['name']}: {entry['type']};")
    lines.extend(["}>;", "", "export default function argumentsToString(args: Arguments) {", '  let result = "";', ""])
    for group in GROUPS:
        for param in grouped[group]:
            flag = param["long"]
            name = binding_name(flag, "ts", overrides)
            if flag == "--verbose":
                lines.append(f"  if (args.{name}) result += \" -\" + \"v\".repeat(args.{name});")
            elif flag == "--fast-hierarchical-solution":
                lines.append(f"  if (args.{name}) result += \" -\" + \"F\".repeat(args.{name});")
            elif flag == "--output":
                lines.extend(
                    [
                        f"  if (args.{name} != null) {{",
                        f'    if (typeof args.{name} === "string") result += " --output " + args.{name};',
                        f'    else result += " --output " + args.{name}.join(",");',
                        "  }",
                    ]
                )
            elif not param["required"]:
                lines.append(f'  if (args.{name}) result += " {flag}";')
            else:
                lines.append(f'  if (args.{name} != null) result += " {flag} " + args.{name};')
            lines.append("")
    for entry in about_options:
        if entry["name"] == "help":
            lines.append('  if (args.help) result += args.help === "advanced" ? " -hh" : " -h";')
        elif entry["name"] == "version":
            lines.append('  if (args.version) result += " --version";')
        else:
            raise RuntimeError(f"Unsupported TypeScript binding-only option: {entry['name']}")
        lines.append("")
    lines.extend(["  return result;", "}", ""])
    return "\n".join(lines)


def outputs(infomap_bin: Path) -> dict[Path, str]:
    overrides = load_overrides()
    params = runtime_parameters(load_parameters(infomap_bin))
    return {
        PYTHON_OUT: generate_python(params, overrides),
        R_OUT: generate_r(params, overrides),
        TS_OUT: generate_ts(params, overrides),
    }


def write_outputs(generated: dict[Path, str], output_root: Path) -> None:
    for rel_path, text in generated.items():
        path = output_root / rel_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


def check_outputs(generated: dict[Path, str], output_root: Path) -> int:
    failures = []
    for rel_path, text in generated.items():
        path = output_root / rel_path
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            failures.append(rel_path)
    if not failures:
        print("Tracked binding option outputs are fresh.")
        return 0
    if sys.platform and "GITHUB_ACTIONS" in __import__("os").environ:
        print("::error title=Tracked binding option outputs are stale::Run make build-binding-options and commit the updated files.")
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

    generated = outputs(Path(args.infomap_bin))
    output_root = Path(args.output_root)
    if args.check:
        return check_outputs(generated, output_root)
    write_outputs(generated, output_root)
    for path in generated:
        print(f"Updated {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
