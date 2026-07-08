from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any


GROUPS = ("Input", "Output", "Algorithm", "Accuracy")

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
        binding_names = self.raw.get("bindingNames", {})
        if language in binding_names:
            return binding_names[language]
        names = self.overrides.get("names", {}).get(self.flag, {})
        if language in names:
            return names[language]
        if language == "ts":
            return camel_name(self.flag)
        return snake_name(self.flag)

    def tier(self, language: str) -> str:
        """The binding-surface tier of this parameter: ``common`` or ``advanced``.

        Tier membership is declared per language in the overrides file
        (``tiers.<language>.common``, keyed by CLI flag); every parameter not
        listed there is ``advanced``. The tier decides which parameters stay
        as real keyword parameters in the 3.0 signatures (see issue #738) —
        everything else is carried by the options object.
        """
        common = self.overrides.get("tiers", {}).get(language, {}).get("common", [])
        return "common" if self.flag in common else "advanced"

    def python_inert_without_outdir(self) -> bool:
        """True for output-artifact parameters that are inert via kwargs.

        In library mode (``isCLI=false``, the only mode the Python API uses)
        the engine forces ``noFileOutput`` on when no output directory is
        given (``src/io/Config.cpp``), and the directory can only arrive via
        the positional ``args`` argument. The flags are declared in the
        overrides file (``apiNotes.python.inertWithoutOutputDir``).
        """
        inert = (
            self.overrides.get("apiNotes", {})
            .get("python", {})
            .get("inertWithoutOutputDir", [])
        )
        return self.flag in inert

    def uses_generic_spec(self) -> bool:
        return self.render_policy in {"flag", "value"}

    def binding_default(self, language: str) -> dict[str, str]:
        return self.raw.get("bindingDefaults", {}).get(language, {})

    def python_default_value(self) -> str:
        # ``is not None`` (not truthiness): a future binding default of "" / "0"
        # is a real value, not "unset".
        value = self.binding_default("python").get("value")
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
        python_docs = self.raw.get("bindingDocs", {}).get("python", {})
        if description := python_docs.get("description"):
            return description
        return self.description

    def r_default(self) -> str:
        value = self.binding_default("r").get("value")
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
        self.hidden_bindings = {
            language: {entry["flag"] for entry in entries}
            for language, entries in overrides.get("hiddenBindings", {}).items()
        }
        self._validate_tiers()

    def _validate_tiers(self) -> None:
        # Fail loud on typos: every tier entry must name a real catalog flag,
        # and only the known tier names are accepted. A silently dropped tier
        # entry would put a parameter in the wrong 3.0 signature.
        known_flags = {param.flag for param in self.parameters}
        for language, tiers in self.overrides.get("tiers", {}).items():
            unknown_tiers = set(tiers) - {"common"}
            if unknown_tiers:
                raise RuntimeError(
                    f"Unknown tier name(s) for {language}: {sorted(unknown_tiers)}; "
                    "only 'common' is declared (everything else is advanced)"
                )
            missing = sorted(set(tiers.get("common", [])) - known_flags)
            if missing:
                raise RuntimeError(
                    f"tiers.{language}.common lists flag(s) not in the "
                    f"parameter catalog: {missing}"
                )
        for language, notes in self.overrides.get("apiNotes", {}).items():
            missing = sorted(set(notes.get("inertWithoutOutputDir", [])) - known_flags)
            if missing:
                raise RuntimeError(
                    f"apiNotes.{language}.inertWithoutOutputDir lists flag(s) "
                    f"not in the parameter catalog: {missing}"
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
