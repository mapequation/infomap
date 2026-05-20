from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any


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

PY_DEFAULT_CONSTANTS = {
    "--verbose": "_DEFAULT_VERBOSITY_LEVEL",
    "--meta-data-rate": "_DEFAULT_META_DATA_RATE",
    "--teleportation-probability": "_DEFAULT_TELEPORTATION_PROB",
    "--multilayer-relax-rate": "_DEFAULT_MULTILAYER_RELAX_RATE",
    "--seed": "_DEFAULT_SEED",
    "--core-loop-codelength-threshold": "_DEFAULT_CORE_LOOP_CODELENGTH_THRESHOLD",
    "--tune-iteration-relative-threshold": "_DEFAULT_TUNE_ITER_RELATIVE_THRESHOLD",
}


def snake_name(flag: str) -> str:
    return flag.removeprefix("--").replace("-", "_")


def camel_name(flag: str) -> str:
    parts = flag.removeprefix("--").split("-")
    return parts[0] + "".join(part.capitalize() for part in parts[1:])


def _render_policy(raw: dict[str, Any]) -> str:
    flag = raw["long"]
    if flag in {"--verbose", "--fast-hierarchical-solution"}:
        return "repeated_short"
    if flag == "--output":
        return "comma_list"
    if flag == "--directed":
        return "directed_alias"
    if flag == "--no-self-links":
        return "deprecated_alias_target"
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
        return self.overrides.get("choices", {}).get(self.flag)

    def name(self, language: str) -> str:
        names = self.overrides.get("names", {}).get(self.flag, {})
        if language in names:
            return names[language]
        if language == "ts":
            return camel_name(self.flag)
        return snake_name(self.flag)

    def uses_generic_spec(self) -> bool:
        return self.render_policy in {"flag", "value"}

    def python_default_value(self) -> str:
        if self.flag == "--verbose":
            return "1"
        if self.flag in {"--directed", "--fast-hierarchical-solution"}:
            return "None"
        if not self.required:
            return "False"
        return PY_DEFAULTS.get(self.flag, "None")

    def python_default_expr(self) -> str:
        return PY_DEFAULT_CONSTANTS.get(self.flag, self.python_default_value())

    def python_include_expr(self) -> str:
        default = self.python_default_value()
        if default == "None":
            return "lambda value: value is not None"
        return f"lambda value: value != {default}"

    def python_type(self) -> str:
        if self.render_policy == "repeated_short":
            return (
                "int | None" if self.flag == "--fast-hierarchical-solution" else "int"
            )
        if self.render_policy == "directed_alias":
            return "bool | None"
        if self.choices:
            if self.render_policy == "comma_list":
                return "list[str] | tuple[str, ...] | None"
            return "str | None"
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
        if self.name("python") == "verbosity_level":
            return "Verbosity level on the console. 1 keeps the default output level, 2 renders -vv and so on."
        if self.flag == "--fast-hierarchical-solution":
            return "Find top modules fast. Use 2 to keep all fast levels and 3 to skip the recursive part."
        return self.description

    def r_default(self) -> str:
        if self.flag == "--verbose":
            return "DEFAULT_VERBOSITY_LEVEL"
        if self.flag in {"--directed", "--fast-hierarchical-solution"}:
            return "NULL"
        if not self.required:
            return "FALSE"
        return R_DEFAULTS.get(self.flag, "NULL")

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
