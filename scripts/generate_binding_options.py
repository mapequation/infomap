#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import subprocess
from pathlib import Path

from parameter_catalog import GROUPS, ParameterCatalog
from render_parameter_policy import render as render_parameter_policy_md


REPO_ROOT = Path(__file__).resolve().parents[1]
OVERRIDES = REPO_ROOT / "interfaces" / "parameters" / "overrides.json"

PYTHON_OUT = Path("interfaces/python/src/infomap/_options.py")
R_OUT = Path("interfaces/R/infomap/R/options.R")
TS_OUT = Path("interfaces/js/src/arguments.ts")
FACADE_OUT = Path("interfaces/python/src/infomap/_facade.py")
# The 3.0 parameter-policy matrix (issue #755), rendered from the same catalog
# so it stays fresh under ``make test-binding-options-freshness``.
PARAMETER_POLICY_OUT = Path("interfaces/parameters/policy.md")

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
        "type": "bool | None",
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
    # Hidden bindings are derived from the policy section (hide decisions);
    # their rationale is the schema-mandated replacement text, validated by
    # ParameterCatalog._validate_policy.
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
                # "advanced" params are docs-only deprecated on the facade
                # (#741) and move to Options in 3.0 (#748).
                "tier": param.tier("python"),
                # Output-artifact params are inert through kwargs (library
                # mode forces noFileOutput without an output directory).
                "inert_without_outdir": param.python_inert_without_outdir(),
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
                # The 3.0 cleanup-policy decision (issue #755) drives both the
                # docstring migration note and the runtime PendingDeprecationWarning
                # so the two never diverge from render_parameter_policy.py.
                "policy": param.policy("python"),
                "init_default": param.python_default_expr(),
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


def _python_literal_alias_lines(catalog: ParameterCatalog) -> list[str]:
    """Module-level ``Alias = Literal[...]`` definitions for choice params.

    The aliases keep the generated Options fields and facade signatures short
    (``flow_model: FlowModel | None``) while giving type checkers the exact
    value set from the catalog.
    """
    lines: list[str] = []
    seen: set[str] = set()
    for group in GROUPS:
        for param in catalog.grouped()[group]:
            literal_alias = param.python_literal_alias()
            if literal_alias is None:
                continue
            alias, literal = literal_alias
            if alias in seen:
                continue
            seen.add(alias)
            choices = literal.removeprefix("Literal[").removesuffix("]")
            lines.append(f"{alias} = Literal[")
            lines.append(f"    {choices}")
            lines.append("]")
            lines.append("")
    return lines


# Static runtime helper (issue #741): warn when an advanced-tier keyword is set
# on a *direct* Infomap()/run() call. The message shape mirrors
# _advanced_tier_directive() -- kept/aliased keywords point at Options, every
# other action carries its policy replacement -- so the docstring and the
# warning never diverge.
_ADVANCED_TIER_WARNING_HELPER = [
    "def _warn_advanced_tier_kwargs(passed, context):",
    "    # Advanced-tier keywords are docs-only deprecated on the Infomap()/run()",
    "    # signatures and move off them in 3.0. Emit a",
    "    # PendingDeprecationWarning -- silent by default, so it nags no one until",
    "    # 3.0 nears -- when one is set to a non-default value on a direct call.",
    "    # Internal funnels (the Options path builds Infomap(**resolved), the graph",
    "    # adapters, the from_* class methods) reach the same methods from inside",
    "    # the package; the caller-frame check skips them so only user-typed",
    "    # keywords are flagged.",
    "    caller = sys._getframe(2)",
    "    if caller.f_code.co_filename.startswith(_PACKAGE_PREFIX):",
    "        return",
    '    baseline = 1 if context == "run" else 0',
    "    for name, spec in _ADVANCED_TIER_KWARGS.items():",
    "        default = spec[baseline]",
    "        if passed.get(name, default) != default:",
    "            action, replacement = spec[2], spec[3]",
    "            lead = (",
    "                f\"'{name}' is deprecated on the Infomap() and run() \"",
    '                "signatures and leaves them in 3.0. "',
    "            )",
    '            if action in ("keep", "alias"):',
    "                guidance = (",
    '                    "Pass it via Options to infomap.run() or "',
    '                    "Network.run() instead."',
    "                )",
    "            elif replacement is not None:",
    "                guidance = replacement",
    "            else:",
    '                guidance = ""',
    "            warnings.warn(",
    "                lead + guidance, PendingDeprecationWarning, stacklevel=3",
    "            )",
]


# Maps each special-render-policy parameter to the ``_OptionSpec.render`` kind
# its to_args() branch dispatches on. ``repeated_short`` splits per parameter:
# ``--verbose`` renders ``-vv...`` above its default, while
# ``--fast-hierarchical-solution`` validates 1..3 before rendering ``-F...``.
_SPECIAL_RENDER_KINDS = {
    "--no-self-links": "no_self_links",
    "--verbose": "verbosity",
    "--output": "comma_list",
    "--directed": "directed",
    "--fast-hierarchical-solution": "fast_hierarchical",
}

# Rendered-argument order of the special-policy parameters within each catalog
# group. The generic (flag/value) parameters render first, in catalog order;
# these follow. Preserves the historical argument order (e.g.
# ``--silent -vvv --output clu,tree``). A catalog parameter with a special
# render policy that is missing here fails loud in _option_table_lines instead
# of being silently dropped from the render loop.
_SPECIAL_RENDER_ORDER = {
    "Input": ["--no-self-links"],
    "Output": ["--verbose", "--output"],
    "Algorithm": ["--directed"],
    "Accuracy": ["--fast-hierarchical-solution"],
}


def _option_table_row(param, kind: str) -> str:
    """One generated ``_OPTION_TABLE`` entry, kwargs only where they differ
    from the ``_OptionSpec`` defaults so each row shows the data that matters."""
    parts = [json.dumps(param.flag), json.dumps(kind), param.python_default_expr()]
    domain = param.python_domain()
    if domain is not None:
        low, high = domain
        parts.append(f"domain=({low}, {high})")
    if param.choices:
        rendered = ", ".join(json.dumps(choice) for choice in param.choices)
        parts.append(f"choices=({rendered},)")
    if kind == "value" and not param.choices:
        if param.long_type == "path":
            parts.append("path=True")
        elif param.long_type == "string":
            parts.append("free_string=True")
    if param.tier("python") == "common":
        parts.append("common=True")
    policy = param.policy("python")
    action = policy.get("action", "keep")
    if action != "keep":
        parts.append(f"action={json.dumps(action)}")
    if action not in {"keep", "alias"}:
        parts.append(f"replacement={json.dumps(policy['replacement'])}")
    return f'    "{param.name("python")}": _OptionSpec({", ".join(parts)}),'


_OPTION_SPEC_CLASS = [
    "class _OptionSpec(NamedTuple):",
    '    """Per-option record: how one Options field renders and validates.',
    "",
    "    The single generated per-option source; the specialized lookups the",
    "    validators and the deprecation machinery use are derived from it",
    "    below. ``render`` selects the to_args() branch: the generic",
    '    "flag"/"value" kinds cover most options, the remaining kinds are the',
    "    catalog's one-off render policies. ``common`` marks the parameters",
    "    that stay real keyword parameters in the 3.0 signatures;",
    "    ``action``/``replacement`` carry the 3.0 cleanup-policy decision.",
    '    """',
    "",
    "    flag: str",
    "    render: str",
    "    default: object = None",
    "    # Inclusive numeric bounds from the catalog (min/max), or None.",
    "    domain: tuple | None = None",
    "    # Allowed values for choice-typed options.",
    "    choices: tuple | None = None",
    "    # Path-typed (os.fsdecode'd at the Options boundary) and free-string",
    "    # values; both travel the whitespace-split argument string unquoted.",
    "    path: bool = False",
    "    free_string: bool = False",
    "    common: bool = False",
    '    action: str = "keep"',
    "    replacement: str | None = None",
]

# Derived views of _OPTION_TABLE, keeping the consumer-facing names (and their
# historical iteration orders) as cheap comprehensions over the single record.
_OPTION_TABLE_VIEWS = [
    "_OPTION_DOMAINS = {",
    "    name: spec.domain",
    "    for name, spec in _OPTION_TABLE.items()",
    "    if spec.domain is not None",
    "}",
    "",
    "_OPTION_CHOICES = {",
    '    name: (spec.choices, spec.render == "comma_list")',
    "    for name, spec in _OPTION_TABLE.items()",
    "    if spec.choices is not None",
    "}",
    "",
    "_OPTION_PATHS = tuple(name for name, spec in _OPTION_TABLE.items() if spec.path)",
    "",
    "# Free-string value options, rendered verbatim into the argument string.",
    "_OPTION_FREE_STRINGS = tuple(",
    "    name for name, spec in _OPTION_TABLE.items() if spec.free_string",
    ")",
    "",
    "# Advanced-tier keywords on the Infomap()/run() signatures:",
    "# the init/run 'unset' defaults plus the policy action/replacement that",
    "# shape the PendingDeprecationWarning. Infomap.run() re-renders keywords on",
    "# top of the constructed state and a rendered flag can only switch on, so a",
    "# truthy-by-default flag (silent) keeps the no-op False default in run",
    "# context.",
    "_ADVANCED_TIER_KWARGS = {",
    "    name: (",
    "        spec.default,",
    '        False if spec.render == "flag" else spec.default,',
    "        spec.action,",
    "        spec.replacement,",
    "    )",
    "    for name, spec in _OPTION_TABLE.items()",
    "    if not spec.common",
    "}",
]


def _option_table_lines(catalog: ParameterCatalog) -> list[str]:
    """The consolidated per-option table, its derived views, and the runtime
    helpers that read them.

    Emits ``_OptionSpec`` + ``_OPTION_TABLE`` (one row per Options field, in
    rendered-argument order; the deprecated ``include_self_links`` alias is
    handled explicitly in ``to_args``) and derives ``_OPTION_DOMAINS``,
    ``_OPTION_CHOICES``, ``_OPTION_PATHS``, ``_OPTION_FREE_STRINGS`` and
    ``_ADVANCED_TIER_KWARGS`` from it, so per-option data appears exactly once
    in the generated module.
    """
    grouped = catalog.grouped()
    lines: list[str] = list(_OPTION_SPEC_CLASS)
    lines.extend(["", ""])
    lines.append("_OPTION_TABLE = {")
    for group in GROUPS:
        lines.append(f"    # {group.lower()}")
        specials = {
            param.flag: param
            for param in grouped[group]
            if not param.uses_generic_spec()
        }
        expected = _SPECIAL_RENDER_ORDER.get(group, [])
        if set(specials) != set(expected):
            raise RuntimeError(
                f"special render policies in group {group!r} changed: catalog "
                f"has {sorted(specials)}, _SPECIAL_RENDER_ORDER declares "
                f"{sorted(expected)}. Wire the new policy into "
                "_SPECIAL_RENDER_KINDS/_SPECIAL_RENDER_ORDER and to_args()."
            )
        for param in grouped[group]:
            if param.uses_generic_spec():
                lines.append(_option_table_row(param, param.spec_kind))
        for flag in expected:
            lines.append(
                _option_table_row(specials[flag], _SPECIAL_RENDER_KINDS[flag])
            )
    lines.append("}")
    lines.extend(["", ""])
    lines.extend(_OPTION_TABLE_VIEWS)
    lines.extend(["", ""])
    lines.extend(_ADVANCED_TIER_WARNING_HELPER)
    lines.extend(["", ""])
    lines.extend(_VALIDATE_OPTION_DOMAINS_HELPER)
    lines.extend(["", ""])
    lines.extend(_VALIDATE_OPTION_CHOICES_HELPER)
    lines.extend(["", ""])
    lines.extend(_NORMALIZE_OPTION_PATHS_HELPER)
    lines.extend(["", ""])
    lines.extend(_VALIDATE_OPTION_ARG_STRINGS_HELPER)
    lines.extend(["", ""])
    return lines


# Static runtime helper: validate numeric options against the catalog's
# inclusive min/max bounds. The engine parser rejects an out-of-domain value
# with a generic "Cannot parse '<value>' as argument to option '<flag>'"
# (ProgramInterface.cpp) -- misleading, since the value is a valid number and
# only violates the option's range. This turns that into a clear ValueError
# naming the option and its range, at the single to_args() render funnel every
# entry point passes through (Infomap()/run()/infomap.run()/Network.run()).
_VALIDATE_OPTION_DOMAINS_HELPER = [
    "def _validate_option_domains(options):",
    '    """Reject out-of-range numeric options with a clear ValueError.',
    "",
    "    The engine parser rejects an out-of-domain value (seed < 1, a",
    "    probability outside [0, 1], a negative markov time, ...) with a generic",
    "    \"Cannot parse '<value>' as argument to option '<flag>'\" -- misleading,",
    "    since the value parses fine as a number and only violates the option's",
    "    range. Catch it here first, naming the option and its valid range. Bounds",
    "    are inclusive, mirroring the C++ parser (reject value < min or > max).",
    '    """',
    "    for name, (low, high) in _OPTION_DOMAINS.items():",
    "        value = options.get(name)",
    "        if (",
    "            value is None",
    "            or isinstance(value, bool)",
    "            or not isinstance(value, (int, float))",
    "        ):",
    "            continue",
    "        if (low is not None and value < low) or (",
    "            high is not None and value > high",
    "        ):",
    "            if low is not None and high is not None:",
    '                bounds = f"between {low} and {high} (inclusive)"',
    "            elif low is not None:",
    '                bounds = f">= {low}"',
    "            else:",
    '                bounds = f"<= {high}"',
    "            raise ValueError(",
    '                f"{name}={value!r} is out of range: {name} must be {bounds}."',
    "            )",
]


# Static runtime helper: validate choice-typed options against the catalog's
# allowed value set. The engine rejects an unknown value late and generically
# (e.g. RuntimeError("Unrecognized flow model: 'undirekted'")) without listing
# the legal values, and a plausible-but-wrong string constructs fine and only
# fails after the whole network is built. Catch it at the same to_args() funnel
# as the numeric domains, naming the option and echoing the valid set.
_VALIDATE_OPTION_CHOICES_HELPER = [
    "def _validate_option_choices(options):",
    '    """Reject unrecognized choice/enum options with a clear ValueError.',
    "",
    "    Mirrors the catalog's allowed value set (the same values the type",
    "    checker sees via the generated Literals). Scalar choices (flow_model)",
    "    are checked directly; sequence choices (output) are checked per element.",
    "    num_threads is validated here too: 'auto' or a positive integer.",
    '    """',
    "    for name, (choices, is_sequence) in _OPTION_CHOICES.items():",
    "        value = options.get(name)",
    "        if value is None:",
    "            continue",
    "        if is_sequence and isinstance(value, str):",
    "            # A single format string is a natural guess, but a sequence",
    "            # choice iterates it per character ('tree' -> 't','r',...),",
    "            # yielding a baffling \"'t' is not valid\". Steer to a list.",
    "            hint = f\"[{value!r}]\" if value in choices else \"['tree', 'clu']\"",
    "            raise ValueError(",
    "                f\"{name}={value!r} must be a list or tuple of formats \"",
    "                f\"(e.g. {name}={hint}), not a single string.\"",
    "            )",
    "        values = value if is_sequence else (value,)",
    "        for item in values:",
    "            if item not in choices:",
    '                allowed = ", ".join(repr(choice) for choice in choices)',
    "                raise ValueError(",
    '                    f"{name}={item!r} is not valid: choose one of {allowed}."',
    "                )",
    '    num_threads = options.get("num_threads")',
    "    if num_threads is not None:",
    "        valid = (",
    '            num_threads == "auto"',
    "            or (",
    "                isinstance(num_threads, int)",
    "                and not isinstance(num_threads, bool)",
    "                and num_threads > 0",
    "            )",
    "            or (",
    "                isinstance(num_threads, str)",
    "                and num_threads.isdigit()",
    "                and int(num_threads) > 0",
    "            )",
    "        )",
    "        if not valid:",
    "            raise ValueError(",
    "                f\"num_threads={num_threads!r} is not valid: use 'auto' or a \"",
    '                "positive integer."',
    "            )",
]


# Static runtime helpers: the path-typed options follow the package-wide
# file-path contract (str | os.PathLike, decoded with os.fsdecode like the
# file readers and writers), and -- together with the free-string options --
# must survive the rendered argument string, which the engine splits on
# whitespace with no quoting support. Normalize and validate at the Options
# boundary so every entry point (Options()/Infomap()/run()/Network.run())
# gets the same contract and a clear error instead of a silent misparse.
_NORMALIZE_OPTION_PATHS_HELPER = [
    "def _normalize_option_paths(options):",
    '    """Decode path-typed option values (os.PathLike -> str) in place.',
    "",
    "    The path-typed options follow the package-wide file-path contract",
    "    (str | os.PathLike, decoded with os.fsdecode like the file readers",
    "    and writers); the rendered argument string needs plain str. A value",
    "    that is not path-like gets a TypeError naming the option here,",
    "    instead of a confusing engine parse error downstream.",
    '    """',
    "    for name in _OPTION_PATHS:",
    "        value = options.get(name)",
    "        if value is None or isinstance(value, str):",
    "            continue",
    "        try:",
    "            options[name] = os.fsdecode(value)",
    "        except TypeError:",
    "            raise TypeError(",
    '                f"{name}={value!r} is not a valid path: pass a str or "',
    '                "os.PathLike."',
    "            ) from None",
]


_VALIDATE_OPTION_ARG_STRINGS_HELPER = [
    "def _validate_option_arg_strings(options):",
    '    """Reject string values the rendered argument string cannot carry.',
    "",
    "    Rendered options travel to the engine as a whitespace-separated",
    "    argument string with no quoting support, so a value containing",
    "    whitespace would be silently split into separate tokens (a",
    "    cluster_data path 'my file.clu' reads as 'my'). Reject it here,",
    "    naming the option and the reason.",
    '    """',
    "    for name in _OPTION_PATHS + _OPTION_FREE_STRINGS:",
    "        value = options.get(name)",
    "        if isinstance(value, str) and any(ch.isspace() for ch in value):",
    "            raise ValueError(",
    '                f"{name}={value!r} contains whitespace, which the engine "',
    '                "argument string cannot carry (arguments are split on "',
    '                "whitespace, with no quoting). Use a whitespace-free value."',
    "            )",
]


def _python_domain_doc(param) -> str:
    """A one-line 'Valid range: ...' note for a numerically bounded option.

    The bounds are enforced at construction/render (``_validate_option_domains``);
    surfacing them in the docstring keeps a surprising-but-valid constraint like
    ``seed >= 1`` from only showing up as a runtime error.
    """
    domain = param.python_domain()
    if domain is None:
        return ""
    low, high = domain
    if low != "None" and high != "None":
        return f"Valid range: between {low} and {high} (inclusive)."
    if low != "None":
        return f"Valid range: >= {low}."
    return f"Valid range: <= {high}."


def _python_engine_default_doc(param) -> str:
    """A one-line 'Engine default: N.' note for a None-sentinel numeric option.

    Options whose Python default is a ``None`` sentinel render no flag unless
    set, so the reference shows ``= None`` and hides the concrete value the
    engine falls back to (e.g. ``clu_level`` -> 1, ``node_limit`` -> 0 meaning
    "no limit"). Surface the catalog default so a reader of the parameter
    reference isn't left guessing. Restricted to numeric defaults to avoid
    noise on choice/string options.
    """
    if param.python_default_value() != "None":
        return ""
    default = param.raw.get("default")
    if default is None or default == "" or isinstance(default, bool):
        return ""
    # Catalog defaults arrive as strings; surface only numeric ones so choice,
    # boolean, or path defaults don't produce a noisy or misleading note
    # (float(False) succeeds, hence the explicit bool guard above).
    try:
        value = float(default)
    except (TypeError, ValueError):
        return ""
    # Skip a sentinel default that falls outside the documented valid range
    # (e.g. node_limit defaults to 0 meaning "no limit" while a set value must
    # be >= 1) -- printing it next to "Valid range: >= 1" reads as a
    # contradiction. Only surface a default the user could actually set.
    domain = param.python_domain()
    if domain is not None:
        low, high = domain
        if (low != "None" and value < float(low)) or (
            high != "None" and value > float(high)
        ):
            return ""
    return f"Engine default: {default}."


def _options_doc_policy_note(policy: dict) -> str:
    """A short note for the Options docstring flagging fields that are not a
    first-class engine option on the Python library surface.

    Someone reading ``inspect.getdoc(infomap.Options)`` as the parameter
    reference would otherwise read args-only / removed / alias fields (``tree``,
    ``silent``, ``out_name``, ``threads`` ...) as usable library knobs. Reuse the
    policy's own replacement guidance so the note stays in step with the facade
    docstrings and the runtime advanced-tier warning.
    """
    action = (policy or {}).get("action", "keep")
    if action == "keep":
        return ""
    replacement = (policy or {}).get("replacement", "").strip()
    if action == "args-only" and (policy or {}).get("inertWithoutOutputDir"):
        # The standard inert output flags (tree, clu, output, ...) all share
        # one long replacement string; repeating it per field bloats the
        # reference users are pointed to. State the rationale once in the
        # class docstring and keep the per-field note to a short pointer.
        # ``hide_bipartite_nodes`` is args-only but writer-effective (no
        # inertWithoutOutputDir), so it falls through and keeps its own note.
        return "Args-only in library mode (see the note above)."
    lead = {
        "args-only": "Args-only on the Python library surface.",
        "remove": "Not a Python library option; it leaves the surface in 3.0.",
        "alias": "Compatibility alias.",
        "deprecate": "Deprecated.",
        "hide": "Not exposed on the Python surface.",
    }.get(action, "")
    return f"{lead} {replacement}".strip()


def generate_python(catalog: ParameterCatalog) -> str:
    grouped = catalog.grouped()
    include_self_links = catalog.binding_only_entry("python", "include_self_links")
    lines = [
        "from __future__ import annotations",
        "",
        "import os",
        "import sys",
        "import warnings",
        "from collections.abc import Mapping",
        "from dataclasses import dataclass, fields, replace",
        "from typing import Literal, NamedTuple",
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
    lines.extend(_python_literal_alias_lines(catalog))
    lines.extend(_option_table_lines(catalog))
    lines.extend(
        [
            "def _join_args(base_args, parts):",
            "    if not parts:",
            '        return "" if base_args is None else base_args',
            "    if not base_args:",
            "        return f\" {' '.join(parts)}\"",
            "    return f\"{base_args} {' '.join(parts)}\"",
            "",
            "",
            "class _OptionsMeta(type):",
            "    # Intercept construction so an unknown keyword gets the same",
            "    # actionable 'did you mean' guidance as infomap.run(), instead of",
            "    # the bare dataclass 'unexpected keyword argument' TypeError.",
            "    def __call__(cls, *args, **kwargs):",
            "        unknown = [key for key in kwargs if key not in _OPTION_FIELD_NAMES]",
            "        if unknown:",
            "            import difflib",
            "",
            "            rendered = []",
            "            for key in unknown:",
            "                near = difflib.get_close_matches(key, _OPTION_FIELD_NAMES, n=1)",
            "                rendered.append(",
            "                    f\"{key!r}\"",
            "                    + (f\" (did you mean {near[0]!r}?)\" if near else \"\")",
            "                )",
            "            raise TypeError(",
            '                "Options() got unknown option(s): "',
            '                + ", ".join(rendered)',
            '                + ". See inspect.getdoc(infomap.Options) for the full "',
            '                "list of option names."',
            "            )",
            "        return super().__call__(*args, **kwargs)",
            "",
            "",
            "@dataclass(frozen=True, slots=True, repr=False)",
            "class Options(metaclass=_OptionsMeta):",
            '    """Reusable Infomap keyword options.',
            "",
            "    This class mirrors the keyword arguments accepted by :class:`infomap.Infomap`",
            "    and :meth:`infomap.Infomap.run`. Construct it like any dataclass",
            "    (``Options(num_trials=10)``) -- unknown keys raise, as usual -- use",
            "    :meth:`to_args` to render command-line flags, or pass an instance to",
            "    :func:`infomap.run` via ``infomap.run(input, options=options)`` to apply a",
            "    reusable configuration.",
            "",
            "    Instances are immutable (frozen). Derive a tweaked copy with",
            "    :meth:`replace` rather than mutating in place::",
            "",
            "        base = Options(num_trials=20, seed=123)",
            '        directed = base.replace(flow_model="directed")   # base is unchanged',
            "",
            "    A field marked *args-only in library mode* below drives the engine's",
            "    own file writer, which runs only with an output directory passed via",
            "    the raw ``args`` escape hatch; on the normal library surface it writes",
            "    nothing. Write results from the ``Result`` (``result.write_tree`` /",
            "    ``write_clu`` / ...) or the ``Network`` (``write_pajek`` /",
            "    ``write_state_network``) instead.",
            "",
            "    Parameters",
            "    ----------",
        ]
    )
    for group in GROUPS:
        for param in grouped[group]:
            name = param.name("python")
            description = param.python_doc_description()
            bound = _python_domain_doc(param)
            if bound:
                description = f"{description} {bound}"
            engine_default = _python_engine_default_doc(param)
            if engine_default:
                description = f"{description} {engine_default}"
            lines.append(f"    {name} : {param.python_doc_type()}")
            lines.extend(wrap_doc(description, "        "))
            note = _options_doc_policy_note(param.policy("python"))
            if note:
                lines.append("")
                lines.extend(wrap_doc(note, "        "))
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
            "    def __post_init__(self):",
            "        # Normalize the path-typed options (os.PathLike -> str) before",
            "        # validating, so validation, repr, equality and rendering all",
            "        # see plain strings. Then validate at construction so a bad",
            "        # value fails where it is written, not later at the to_args()",
            "        # render funnel. to_args() revalidates the merged result, so",
            "        # overrides are covered too.",
            "        options = self.to_kwargs()",
            "        _normalize_option_paths(options)",
            "        for name in _OPTION_PATHS:",
            "            if options[name] is not getattr(self, name):",
            "                object.__setattr__(self, name, options[name])",
            "        _validate_option_domains(options)",
            "        _validate_option_choices(options)",
            "        _validate_option_arg_strings(options)",
            "",
            "    def __repr__(self) -> str:",
            "        # Show only the fields set away from their defaults; the full",
            "        # ~70-field dataclass dump is unreadable and buries the few",
            "        # options that matter. eval(repr(o)) still round-trips, since",
            "        # omitted fields carry their defaults.",
            "        changed = [",
            '            f"{f.name}={getattr(self, f.name)!r}"',
            "            for f in fields(self)",
            "            if getattr(self, f.name) != f.default",
            "        ]",
            "        return f\"Options({', '.join(changed)})\"",
            "",
            '    def replace(self, **changes) -> "Options":',
            '        """Return a copy of these options with ``changes`` applied.',
            "",
            "        The ergonomic way to derive a variant of a base configuration without",
            "        mutating it (instances are frozen). Validates like construction, so an",
            "        out-of-range or misspelled change raises here::",
            "",
            "            base = Options(num_trials=20, seed=123)",
            "            faster = base.replace(num_trials=5)   # base is unchanged",
            "",
            "        Parameters",
            "        ----------",
            "        **changes",
            "            Option fields to override, by name. Unknown names raise",
            "            ``TypeError``; out-of-range values raise ``ValueError``.",
            "",
            "        Returns",
            "        -------",
            "        Options",
            "            A new ``Options`` with the given fields replaced.",
            '        """',
            "        return replace(self, **changes)",
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
            "        _normalize_option_paths(options)",
            "        _validate_option_domains(options)",
            "        _validate_option_choices(options)",
            "        _validate_option_arg_strings(options)",
            "        if self.directed is not None and self.flow_model is not None:",
            "            # Only warn on a genuine conflict. directed=True is shorthand for",
            '            # flow_model="directed" (False -> "undirected"), so pairing it with',
            "            # the matching flow_model is redundant but consistent -- warning",
            "            # there second-guesses valid input. Warn only when the two disagree,",
            "            # where 'directed' silently wins and the flow_model is really lost.",
            '            implied_flow_model = "directed" if self.directed else "undirected"',
            "            if self.flow_model != implied_flow_model:",
            "                warnings.warn(",
            "                    f\"both 'directed' and 'flow_model' are set and disagree; \"",
            "                    f\"'directed' takes precedence and flow_model=\"",
            "                    f\"{self.flow_model!r} is ignored by the engine. Set only \"",
            '                    "one (directed=True is shorthand for flow_model=\'directed\').",',
            "                    UserWarning,",
            "                    stacklevel=_external_stacklevel(),",
            "                )",
            "        rendered_args = []",
            "",
            "        if self.include_self_links is not None:",
            "            warnings.warn(",
            '                "include_self_links is deprecated, use no_self_links to exclude self-links",',
            "                DeprecationWarning,",
            "                stacklevel=_external_stacklevel(),",
            "            )",
            "",
            "        for name, spec in _OPTION_TABLE.items():",
            "            value = options[name]",
            '            if spec.render == "flag":',
            "                if value:",
            "                    rendered_args.append(spec.flag)",
            '            elif spec.render == "value":',
            "                if value != spec.default:",
            '                    rendered_args.append(f"{spec.flag} {value}")',
            '            elif spec.render == "no_self_links":',
            "                # Both a first-class flag and the rendering of the",
            "                # deprecated include_self_links=False alias; at most once.",
            "                if (self.include_self_links is not None and not self.include_self_links) or value:",
            "                    rendered_args.append(spec.flag)",
            '            elif spec.render == "verbosity":',
            "                if value > 1:",
            "                    rendered_args.append(f\"-{'v' * value}\")",
            '            elif spec.render == "comma_list":',
            "                if value is not None:",
            "                    rendered_args.append(f\"{spec.flag} {','.join(value)}\")",
            '            elif spec.render == "directed":',
            "                if value is not None:",
            '                    rendered_args.append("--directed" if value else "--flow-model undirected")',
            '            else:  # "fast_hierarchical"',
            "                if value is not None:",
            "                    if value not in (1, 2, 3):",
            "                        raise ValueError(",
            '                            "fast_hierarchical_solution="',
            '                            f"{value!r} is not valid: use "',
            '                            "1 (find top modules fast), 2 (keep all fast levels), "',
            '                            "or 3 (skip the recursive part), or None to disable."',
            "                        )",
            "                    rendered_args.append(f\"-{'F' * value}\")",
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
            "_OPTION_DEFAULTS = Options()",
            "# Facade keywords whose Infomap.run() default differs from Infomap.__init__():",
            "# the flags that default to on at construction (silent). run() re-renders its",
            "# keywords on top of the constructed state and a rendered flag can only switch",
            "# on, so the run signature keeps the no-op False default. A keyword left at",
            "# its run-context default must defer to the options= carrier rather than",
            "# spuriously override it.",
            "_RUN_DEFAULT_OVERRIDES = {",
            "    name: False",
            "    for name, spec in _OPTION_TABLE.items()",
            '    if spec.render == "flag" and spec.default',
            "}",
            "",
            "",
            "def _context_default(name, context):",
            '    if context == "run" and name in _RUN_DEFAULT_OVERRIDES:',
            "        return _RUN_DEFAULT_OVERRIDES[name]",
            "    return getattr(_OPTION_DEFAULTS, name)",
            "",
            "",
            "def _merge_options(base, keyword_options, context):",
            '    """Merge the ``options=`` carrier with the facade keyword options.',
            "",
            "    ``base`` is the ``options=`` argument to ``Infomap()`` / ``Infomap.run()``",
            "    (an :class:`Options`, a mapping, or ``None``); ``keyword_options`` is the",
            "    :class:`Options` built from the facade's per-keyword parameters. A keyword",
            "    left at its (context-appropriate) default defers to ``base``; a keyword",
            "    set to a non-default value is an explicit override and wins -- mirroring",
            '    the "overrides win over options" rule of :func:`infomap.run`.',
            '    """',
            "    if base is None:",
            "        return keyword_options",
            "    if isinstance(base, Mapping):",
            "        base = Options(**base)",
            "    elif not isinstance(base, Options):",
            "        raise TypeError(",
            '            "options must be an Options instance, a mapping, or None"',
            "        )",
            "    overrides = {",
            "        name: getattr(keyword_options, name)",
            "        for name in _OPTION_FIELD_NAMES",
            "        if getattr(keyword_options, name) != _context_default(name, context)",
            "    }",
            "    return replace(base, **overrides)",
            "",
            "",
            "def _construct_args(args=None, **options):",
            "    # Internal rendered-args funnel behind Infomap()/run()/Network.run().",
            "    # Unknown option names raise through the Options constructor, with",
            "    # the same 'did you mean' guidance as direct construction.",
            "    return Options(**options).to_args(base_args=args)",
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
            info = _FACADE_ONLY_PARAMS[name]
            default = info["default"]
        else:
            info = index[name]
            default = info["run_default"] if context == "run" else info["default"]
        lines.append(f"{indent}{name}: {info['type']} = {default},")
    return lines


# The release that recorded the advanced-tier facade-kwarg migration (issue
# #741): kept kwargs get a ``.. versionchanged::`` note, removed/args-only ones
# a ``.. deprecated::``. Bump alongside the next such pass, if any.
_ADVANCED_TIER_CHANGED_IN = "2.15"

# Parameters whose docstrings already carry their own deprecation text and
# migration path; the generic advanced-tier directive would be noise on top.
_SELF_DEPRECATED_PARAMS = {"include_self_links", "pretty"}

_INERT_WITHOUT_OUTDIR_NOTE = (
    "Has no effect in the Python API unless an output directory is passed "
    "via `args` (library mode disables file output otherwise; use the "
    "`write_*` methods to write results)."
)

_FACADE_OPTIONS_DOC = (
    "A reusable `Options` object (or a mapping) applied as the base "
    "configuration; any keyword argument set to a non-default value overrides "
    "it. This is the canonical, warning-free carrier for the advanced options "
    "that leave the signature in 3.0."
)

# A kept keyword is permanent -- only its entry point relocates to Options in
# 3.0 -- so its docstring uses ``.. versionchanged::`` rather than
# ``.. deprecated::``. Marking a still-supported tuning knob "deprecated" makes
# tools and linters (which read the directive token literally) steer users
# away from a valid option.
_ADVANCED_TIER_KEEP_NOTE = "Pass it via `Options`; moves off this signature in 3.0."


def _advanced_tier_directive(policy):
    """The ``(directive, note)`` for an advanced-tier parameter's docstring.

    A ``keep`` parameter stays fully supported and only relocates to
    ``Options`` in 3.0, so it gets ``versionchanged``. Every other action
    (``remove``, ``args-only``, ...) drops the keyword from the API, so it
    gets ``deprecated`` plus its policy ``replacement`` migration path (the
    #755 acceptance criterion); telling the user to "pass it via Options"
    would contradict both the policy and render_parameter_policy.py.
    """
    action = (policy or {}).get("action", "keep")
    if action == "keep":
        return "versionchanged", _ADVANCED_TIER_KEEP_NOTE
    return "deprecated", (
        "This keyword leaves the `Infomap` signature in 3.0. "
        + policy["replacement"]
    )


def _render_facade_docstring_params(names, index):
    lines = []
    for name in names:
        info = index[name]
        if info["doc"] is None:
            continue
        lines.append(f"        {name} : {info['doc_type']}")
        lines.extend(wrap_doc(info["doc"], "            "))
        if info.get("inert_without_outdir"):
            lines.append("")
            lines.extend(wrap_doc(_INERT_WITHOUT_OUTDIR_NOTE, "            "))
        if info.get("tier") == "advanced" and name not in _SELF_DEPRECATED_PARAMS:
            directive, note = _advanced_tier_directive(info.get("policy"))
            lines.append("")
            lines.append(
                f"            .. {directive}:: {_ADVANCED_TIER_CHANGED_IN}"
            )
            lines.extend(wrap_doc(note, "                "))
    return lines


def generate_facade(catalog: ParameterCatalog) -> str:
    names, index = _facade_params(catalog)

    lines = [FACADE_BEGIN]
    # ---- __init__ ----
    lines.append("    def __init__(")
    lines.append("        self,")
    lines.append("        args: str | None = None,")
    lines.extend(_render_facade_signature(names, index))
    lines.append("        options: Options | Mapping | None = None,")
    lines.append("    ) -> None:")
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
    lines.append("        options : Options, mapping, or None, optional")
    lines.extend(wrap_doc(_FACADE_OPTIONS_DOC, "            "))
    lines.extend(_render_facade_docstring_params(names, index))
    lines.append('        """')
    lines.extend(_PRETTY_WARNING_LINES)
    lines.append('        _warn_advanced_tier_kwargs(locals(), "init")')
    lines.append(
        '        options = _merge_options(options, Options._from_locals(locals()), "init")'
    )
    lines.append("        self._init_from_options(args, options)")
    lines.append("")
    # ---- run ----
    lines.append("    def run(")
    lines.append("        self,")
    lines.append("        args: str | None = None,")
    lines.append("        initial_partition: dict | None = None,")
    lines.extend(_render_facade_signature(names, index, context="run"))
    lines.append("        options: Options | Mapping | None = None,")
    lines.append('    ) -> "Result":')
    lines.append('        """Run Infomap.')
    lines.append("")
    lines.append(
        "        The per-option keyword arguments match :class:`Infomap` and are"
    )
    lines.append(
        "        documented there; :class:`Options` is the full parameter"
    )
    lines.append(
        "        reference. Reuse a saved configuration by passing"
    )
    lines.append("        :func:`infomap.run` an ``options=`` carrier.")
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
    lines.append("        options : Options, mapping, or None, optional")
    lines.extend(wrap_doc(_FACADE_OPTIONS_DOC, "            "))
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
    lines.append('        _warn_advanced_tier_kwargs(locals(), "run")')
    lines.append(
        '        options = _merge_options(options, Options._from_locals(locals()), "run")'
    )
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
    # The policy matrix is rendered from the same catalog and carried as one of
    # the generated outputs, so ``make build-binding-options`` writes it and
    # ``test-binding-options-freshness`` checks it like every other artifact.
    policy_matrix = (
        "<!-- Generated by scripts/generate_binding_options.py via "
        "scripts/render_parameter_policy.py. Do not edit by hand: change "
        "src/io/ParameterCatalog.cpp or interfaces/parameters/overrides.json, "
        "then run `make build-binding-options`. -->\n\n"
        + render_parameter_policy_md(catalog).rstrip("\n")
        + "\n"
    )
    generated = {
        PYTHON_OUT: generate_python(catalog),
        R_OUT: generate_r(catalog),
        TS_OUT: generate_ts(catalog),
        PARAMETER_POLICY_OUT: policy_matrix,
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
