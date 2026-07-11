from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any


GROUPS = ("Input", "Output", "Algorithm", "Accuracy")

def resolve_policy_decision(
    overrides: dict[str, Any], flag: str, surface: str
) -> dict[str, Any]:
    """Resolve the policy decision for ``flag`` on ``surface``.

    Precedence: per-parameter entry, then group rule, then the ``keep``
    default. Group uniqueness per (flag, surface) is enforced by
    ``ParameterCatalog._validate_policy``.
    """
    policy = overrides.get("policy", {})
    entry = policy.get("parameters", {}).get(flag, {}).get(surface)
    if entry is not None:
        return entry
    for group in policy.get("groups", {}).values():
        if flag in group.get("parameters", []) and surface in group:
            return group[surface]
    return {"action": policy.get("default", "keep")}


def snake_name(flag: str) -> str:
    return flag.removeprefix("--").replace("-", "_")


def camel_name(flag: str) -> str:
    parts = flag.removeprefix("--").split("-")
    return parts[0] + "".join(part.capitalize() for part in parts[1:])


def _render_policy(raw: dict[str, Any]) -> str:
    if raw.get("renderPolicy"):
        return raw["renderPolicy"]
    if raw["required"]:
        return "value"
    return "flag"


@dataclass(frozen=True)
class BindingOnlyParameter:
    language: str
    name: str
    flag: str
    reason: str
    type: str | None = None
    default: str | None = None
    render_policy: str = "binding_only"

    @classmethod
    def from_override(
        cls, language: str, entry: dict[str, Any]
    ) -> BindingOnlyParameter:
        policy = (
            "deprecated_alias"
            if entry["name"] == "include_self_links"
            else "binding_only"
        )
        return cls(
            language=language,
            name=entry["name"],
            flag=entry["flag"],
            reason=entry["reason"],
            type=entry.get("type"),
            default=entry.get("default"),
            render_policy=policy,
        )


@dataclass(frozen=True)
class Parameter:
    raw: dict[str, Any]
    overrides: dict[str, Any]

    @property
    def flag(self) -> str:
        return self.raw["long"]

    @property
    def short_flag(self) -> str:
        return self.raw["short"]

    @property
    def group(self) -> str:
        return self.raw["group"]

    @property
    def description(self) -> str:
        return self.raw["description"]

    @property
    def required(self) -> bool:
        return self.raw["required"]

    @property
    def incremental(self) -> bool:
        return self.raw["incremental"]

    @property
    def long_type(self) -> str | None:
        return self.raw.get("longType")

    @property
    def render_policy(self) -> str:
        return _render_policy(self.raw)

    @property
    def spec_kind(self) -> str:
        return "value" if self.required else "flag"

    @property
    def choices(self) -> list[str] | None:
        return self.raw.get("choices") or self.overrides.get("choices", {}).get(self.flag)

    def name(self, language: str) -> str:
        names = self.overrides.get("names", {}).get(self.flag, {})
        if language in names:
            return names[language]
        if language == "ts":
            return camel_name(self.flag)
        return snake_name(self.flag)

    def tier(self, language: str) -> str:
        """The binding-surface tier of this parameter: ``common`` or ``advanced``.

        Declared as a ``tier`` attribute on the parameter's policy decision
        (``policy.parameters.<flag>.<language>.tier``); every parameter
        without one is ``advanced``. The tier decides which parameters stay
        as real keyword parameters in the 3.0 signatures (see issue #738) —
        everything else is carried by the options object.
        """
        return self.policy(language).get("tier", "advanced")

    def policy(self, surface: str) -> dict[str, str]:
        """The 3.0 cleanup-policy decision for this parameter on ``surface``.

        Declared in the overrides file (issue #755): per-parameter entries in
        ``policy.parameters`` (keyed by CLI flag) take precedence, then group
        rules in ``policy.groups`` (one decision applied to a parameter
        list), then the ``keep`` default. Returns a dict with at least
        ``action`` and, for every non-keep action, a ``replacement`` (plus
        ``aliasOf`` for ``alias``).
        """
        return resolve_policy_decision(self.overrides, self.flag, surface)

    def python_inert_without_outdir(self) -> bool:
        """True for output-artifact parameters that are inert via kwargs.

        In library mode (``isCLI=false``, the only mode the Python API uses)
        the engine forces ``noFileOutput`` on when no output directory is
        given (``src/io/Config.cpp``), and the directory can only arrive via
        the positional ``args`` argument. Declared as
        ``inertWithoutOutputDir: true`` on the python policy decision.
        """
        return bool(self.policy("python").get("inertWithoutOutputDir"))

    def uses_generic_spec(self) -> bool:
        return self.render_policy in {"flag", "value"}

    def binding_default(self, language: str) -> str | None:
        return self.overrides.get("defaults", {}).get(self.flag, {}).get(language)

    def python_default_value(self) -> str:
        # ``is not None`` (not truthiness): a binding default of "" / "0" is a
        # real value, not "unset".
        value = self.binding_default("python")
        if value is not None:
            return value
        if not self.required:
            return "False"
        return "None"

    def python_default_expr(self) -> str:
        return self.python_default_value()

    def python_include_expr(self) -> str:
        default = self.python_default_value()
        if default == "None":
            return "lambda value: value is not None"
        return f"lambda value: value != {default}"

    def python_domain(self) -> tuple[str, str] | None:
        """The ``(low, high)`` numeric-domain literals for validation, or None.

        Rendered only for numeric value-typed parameters that declare a bound in
        the catalog (``min``/``max`` from ``--print-json-parameters``). Bounds
        are inclusive, mirroring the C++ parser (``ProgramInterface.h`` rejects
        ``value < min`` or ``value > max``). Each end is a Python literal, or
        ``"None"`` for an absent bound. Special render policies (the repeated
        ``-F``/``-v`` shorts, the ``--output`` list, the ``--directed`` alias)
        carry no plain numeric range and are skipped.
        """
        if self.render_policy != "value":
            return None
        if self.long_type not in {"integer", "number", "probability"}:
            return None
        low, high = self.raw.get("min"), self.raw.get("max")
        if low is None and high is None:
            return None

        def literal(bound: str | None) -> str:
            if bound is None:
                return "None"
            return str(int(bound)) if self.long_type == "integer" else repr(float(bound))

        return literal(low), literal(high)

    def python_literal_alias(self) -> tuple[str, str] | None:
        """The ``(alias_name, Literal[...])`` pair for a choices parameter.

        Choice parameters are typed through a generated module-level alias
        (``FlowModel = Literal["undirected", ...]``) instead of an inline
        ``Literal`` so the generated signatures stay within the line-length
        budget. Returns ``None`` for parameters without choices.
        """
        if not self.choices:
            return None
        camel = "".join(part.capitalize() for part in self.name("python").split("_"))
        # A comma_list alias names the *element* type (list[OutputFormat]).
        alias = f"{camel}Format" if self.render_policy == "comma_list" else camel
        literal = "Literal[" + ", ".join(json.dumps(c) for c in self.choices) + "]"
        return alias, literal

    def python_type(self) -> str:
        # A per-language override widens/pins an annotation the derived rules
        # cannot express -- e.g. num_threads accepts 'auto' (str) or a positive
        # integer, so its Python annotation is str | int | None even though the
        # engine option is string-typed.
        override = self.overrides.get("types", {}).get(self.flag, {}).get("python")
        if override is not None:
            return override
        if self.render_policy == "repeated_short":
            return "int | None" if self.python_default_value() == "None" else "int"
        if self.render_policy == "directed_alias":
            return "bool | None"
        if self.choices:
            literal_alias = self.python_literal_alias()
            assert literal_alias is not None
            alias, _ = literal_alias
            if self.render_policy == "comma_list":
                return f"list[{alias}] | tuple[{alias}, ...] | None"
            return f"{alias} | None"
        if not self.required:
            return "bool"
        if self.long_type in {"integer", "probability", "number"}:
            base = "float" if self.long_type in {"probability", "number"} else "int"
        elif self.long_type == "list":
            base = "list[str] | tuple[str, ...]"
        else:
            base = "str"
        return base if self.python_default_value() != "None" else f"{base} | None"

    def python_doc_type(self) -> str:
        if self.render_policy == "repeated_short":
            return "int, optional"
        if self.render_policy == "directed_alias":
            return "bool, optional"
        if self.render_policy == "comma_list":
            return "sequence of str, optional"
        if not self.required:
            return "bool, optional"
        if self.long_type == "integer":
            return "int, optional"
        if self.long_type in {"number", "probability"}:
            return "float, optional"
        return "str, optional"

    def python_doc_description(self) -> str:
        docs = self.overrides.get("docs", {}).get(self.flag, {})
        if description := docs.get("python"):
            return description
        return self.description

    def r_default(self) -> str:
        value = self.binding_default("r")
        if value is not None:
            return value
        if not self.required:
            return "FALSE"
        return "NULL"

    def r_include_expr(self) -> str:
        default = self.r_default()
        if default == "NULL":
            return ".skip_when_null"
        return f".skip_when_not_equal({default})"

    def ts_type(self) -> str:
        if self.render_policy == "comma_list":
            return "OutputFormats | OutputFormats[]"
        if self.choices:
            return "\n    | " + "\n    | ".join(
                json.dumps(choice) for choice in self.choices
            )
        if self.render_policy == "repeated_short":
            return "1 | 2 | 3"
        if not self.required:
            return "boolean"
        return (
            "number"
            if self.long_type in {"integer", "probability", "number"}
            else "string"
        )


class ParameterCatalog:
    def __init__(
        self, parameters: list[dict[str, Any]], overrides: dict[str, Any]
    ) -> None:
        self.overrides = overrides
        self.parameters = [
            Parameter(param, overrides)
            for param in parameters
            if param["group"] in GROUPS
        ]
        self.binding_only_parameters = {
            language: [
                BindingOnlyParameter.from_override(language, entry) for entry in entries
            ]
            for language, entries in overrides.get("bindingOnly", {}).items()
        }
        self._validate_policy()
        self._validate_override_sections()
        # Hidden bindings are derived from the policy: a `hide` decision on a
        # generated surface removes the parameter from that surface (the
        # policy is the single per-parameter record; see issue #755).
        surfaces = overrides.get("policy", {}).get("surfaces", [])
        self.hidden_bindings = {
            language: {
                param.flag
                for param in self.parameters
                if param.policy(language)["action"] == "hide"
            }
            for language in surfaces
            if language != "cli"
        }

    def _known_flags(self) -> set[str]:
        known = {param.flag for param in self.parameters}
        known |= {
            entry.flag
            for entries in self.binding_only_parameters.values()
            for entry in entries
        }
        known |= set(self.overrides.get("policy", {}).get("cliOnlyParameters", []))
        return known

    def _validate_override_sections(self) -> None:
        # The names/defaults/docs sections are keyed by CLI flag and then by
        # binding language. They are inputs to the generator, not policy, so
        # _validate_policy does not reach them -- but a typo'd flag or language
        # key would silently fall back to a generated default (the same silent
        # drift the policy check guards against, issue #755), so validate them
        # here too.
        surfaces = set(self.overrides.get("policy", {}).get("surfaces", []))
        languages = surfaces - {"cli"} if surfaces else {"python", "r", "ts"}
        known_flags = self._known_flags()
        for section in ("names", "defaults", "docs", "types"):
            for flag, per_language in self.overrides.get(section, {}).items():
                if flag not in known_flags:
                    raise RuntimeError(
                        f"overrides.{section} lists unknown flag {flag!r} (not in "
                        "the parameter catalog, bindingOnly, or cliOnlyParameters)"
                    )
                if not isinstance(per_language, dict):
                    raise RuntimeError(
                        f"overrides.{section}[{flag!r}] must map language to value"
                    )
                for language in per_language:
                    if language not in languages:
                        raise RuntimeError(
                            f"overrides.{section}[{flag!r}] names unknown language "
                            f"{language!r}; known languages: {sorted(languages)}"
                        )
        # `choices` is keyed by CLI flag but maps to a plain list of allowed
        # values (flag -> list[str]), not a per-language dict, so it is validated
        # separately from the sections above.
        for flag, values in self.overrides.get("choices", {}).items():
            if flag not in known_flags:
                raise RuntimeError(
                    f"overrides.choices lists unknown flag {flag!r} (not in the "
                    "parameter catalog, bindingOnly, or cliOnlyParameters)"
                )
            if not isinstance(values, list) or not all(
                isinstance(value, str) for value in values
            ):
                raise RuntimeError(
                    f"overrides.choices[{flag!r}] must be a list of strings"
                )
        # `bindingOnly` is keyed by binding language and lists parameters that
        # exist only on that surface. Validate the language keys and reject stray
        # entry keys so a typo fails loud instead of silently building a binding
        # no generator consumes (issue #755).
        allowed_entry_keys = {"name", "flag", "type", "default", "reason"}
        for language, entries in self.overrides.get("bindingOnly", {}).items():
            if language not in languages:
                raise RuntimeError(
                    f"overrides.bindingOnly names unknown language {language!r}; "
                    f"known languages: {sorted(languages)}"
                )
            for entry in entries:
                unknown = set(entry) - allowed_entry_keys
                if unknown:
                    raise RuntimeError(
                        f"overrides.bindingOnly[{language!r}] entry {entry.get('name', entry)!r} "
                        f"has unknown keys {sorted(unknown)}; allowed: "
                        f"{sorted(allowed_entry_keys)}"
                    )

    def _validate_policy(self) -> None:
        # Fail loud on typos and incomplete decisions: the policy is the 3.0
        # contract input (#755), so an unknown flag, surface, or action -- or
        # a removal without replacement guidance -- must break the build, not
        # silently fall back to "keep".
        policy = self.overrides.get("policy")
        if policy is None:
            return
        surfaces = set(policy.get("surfaces", []))
        actions = set(policy.get("vocabulary", {}))
        known_flags = {param.flag for param in self.parameters}
        binding_only_flags = {
            entry.flag
            for entries in self.binding_only_parameters.values()
            for entry in entries
        }
        cli_only_flags = set(policy.get("cliOnlyParameters", []))
        known_flags |= binding_only_flags | cli_only_flags

        def check_decision(flag: str, surface: str, decision: dict[str, Any]) -> None:
            if surface not in surfaces:
                raise RuntimeError(
                    f"policy for {flag} names unknown surface {surface!r}; "
                    f"declared surfaces: {sorted(surfaces)}"
                )
            if flag in cli_only_flags and surface != "cli":
                raise RuntimeError(
                    f"policy for {flag} carries a {surface} decision, but "
                    "cliOnlyParameters declares it CLI-only"
                )
            action = decision.get("action")
            if action not in actions:
                raise RuntimeError(
                    f"policy for {flag}/{surface} has unknown action "
                    f"{action!r}; declared actions: {sorted(actions)}"
                )
            if action != "keep" and not decision.get("replacement"):
                raise RuntimeError(
                    f"policy for {flag}/{surface} ({action}) is missing "
                    "the required replacement guidance"
                )
            if action == "alias" and not decision.get("aliasOf"):
                raise RuntimeError(
                    f"policy for {flag}/{surface} is an alias but is "
                    "missing aliasOf"
                )
            if action == "alias" and decision.get("aliasOf") not in known_flags:
                raise RuntimeError(
                    f"policy for {flag}/{surface} aliases unknown flag "
                    f"{decision.get('aliasOf')!r}"
                )
            tier = decision.get("tier")
            if tier is not None and tier != "common":
                raise RuntimeError(
                    f"policy for {flag}/{surface} declares unknown tier "
                    f"{tier!r}; only 'common' is declared (everything else "
                    "is advanced)"
                )
            if tier is not None and action not in {"keep", "alias"}:
                raise RuntimeError(
                    f"policy for {flag}/{surface} declares a tier on action "
                    f"{action!r}; tiers only apply to kept parameters"
                )

        for flag, per_surface in policy.get("parameters", {}).items():
            if flag not in known_flags:
                raise RuntimeError(
                    f"policy.parameters lists unknown flag {flag!r} (not in the "
                    "parameter catalog, bindingOnly, or cliOnlyParameters)"
                )
            for surface, decision in per_surface.items():
                check_decision(flag, surface, decision)

        # Group rules: every listed flag must exist, decisions must validate,
        # and no two groups may decide the same (flag, surface) -- silent
        # shadowing between groups would make the resolution order-dependent.
        claimed: dict[tuple[str, str], str] = {}
        for group_name, group in policy.get("groups", {}).items():
            group_flags = group.get("parameters", [])
            if not group_flags:
                raise RuntimeError(f"policy group {group_name!r} lists no parameters")
            decision_surfaces = [key for key in group if key != "parameters"]
            for flag in group_flags:
                if flag not in known_flags:
                    raise RuntimeError(
                        f"policy group {group_name!r} lists unknown flag {flag!r}"
                    )
                for surface in decision_surfaces:
                    check_decision(flag, surface, group[surface])
                    previous = claimed.setdefault((flag, surface), group_name)
                    if previous != group_name:
                        raise RuntimeError(
                            f"policy groups {previous!r} and {group_name!r} both "
                            f"decide {flag}/{surface}"
                        )

    def grouped(self) -> dict[str, list[Parameter]]:
        return {
            group: [param for param in self.parameters if param.group == group]
            for group in GROUPS
        }

    def binding_only(self, language: str) -> list[BindingOnlyParameter]:
        return self.binding_only_parameters.get(language, [])

    def binding_only_entry(self, language: str, name: str) -> BindingOnlyParameter:
        for entry in self.binding_only(language):
            if entry.name == name:
                return entry
        raise RuntimeError(f"Missing bindingOnly override for {language}:{name}")

    def visible_parameters(self, language: str) -> list[Parameter]:
        hidden = self.hidden_bindings.get(language, set())
        return [param for param in self.parameters if param.flag not in hidden]
