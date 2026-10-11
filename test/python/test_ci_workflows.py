"""Shape checks on the workflow YAML that actionlint cannot make.

actionlint validates workflow syntax, expressions and shell, but it does not know
what a third-party action's inputs mean. So an input that carries a *list* rather
than a script accepts a comment line as data: `extra-packages` on
``r-lib/actions/setup-r-dependencies`` is newline-separated package refs, and a
literal block scalar keeps ``#`` verbatim, so

    extra-packages: |
      any::testthat
      # Pinned because ...
      roxygen2@7.3.3

fails the whole install with ``Cannot parse packages: #, Pinned,`` and takes every
R leg with it. Caught in CI once; pinned here so the next one is caught locally.
"""

from __future__ import annotations

import re
import tomllib
from pathlib import Path

import pytest
import yaml

pytestmark = pytest.mark.fast

REPO_ROOT = Path(__file__).resolve().parents[2]
WORKFLOW_DIR = REPO_ROOT / ".github" / "workflows"

# A pak/remotes package ref: "pkg", "any::pkg", "pkg@1.2.3", "user/repo", with an
# optional trailing comment-free version or source qualifier.
PACKAGE_REF = re.compile(r"^[A-Za-z0-9._/-]+(::[A-Za-z0-9._/-]+)?(@[A-Za-z0-9._-]+)?$")

# Inputs whose value is a newline-separated list of items rather than a script.
LIST_VALUED_INPUTS = ("extra-packages", "packages")


def _steps(workflow: dict):
    for job in (workflow.get("jobs") or {}).values():
        yield from job.get("steps") or []


def _workflows():
    if not WORKFLOW_DIR.is_dir():
        pytest.skip("workflows not available in this layout")
    for path in sorted(WORKFLOW_DIR.glob("*.yml")):
        yield path, yaml.safe_load(path.read_text())


def test_list_valued_action_inputs_carry_only_list_items():
    """No comment lines or prose inside an input that is parsed as a list."""
    offenders: list[str] = []
    for path, workflow in _workflows():
        for step in _steps(workflow):
            with_block = step.get("with") or {}
            for input_name in LIST_VALUED_INPUTS:
                value = with_block.get(input_name)
                if not isinstance(value, str):
                    continue
                for line in value.splitlines():
                    item = line.strip()
                    if not item:
                        continue
                    if not PACKAGE_REF.match(item):
                        offenders.append(
                            f"{path.name}: {input_name} entry {item!r} is not a package ref"
                        )
    assert not offenders, (
        "these entries sit inside an input that is parsed as a list, so they are "
        "read as items rather than comments:\n" + "\n".join(offenders)
    )


def test_the_roxygen_pin_matches_the_makefile():
    """The two roxygen2 pins have to agree, or CI regenerates different output.

    roxygen's formatting is version-dependent, so the tracked man pages match one
    version only. `make test-r-man-freshness` asserts the installed version against
    R_ROXYGEN_VERSION; this asserts CI installs that same one.
    """
    makefile = (REPO_ROOT / "mk" / "r.mk").read_text()
    match = re.search(r"^R_ROXYGEN_VERSION\s*:=\s*([0-9.]+)", makefile, re.MULTILINE)
    assert match, "R_ROXYGEN_VERSION not found in mk/r.mk"
    pinned = match.group(1)

    refs = [
        item.strip()
        for _, workflow in _workflows()
        for step in _steps(workflow)
        for item in str((step.get("with") or {}).get("extra-packages", "")).splitlines()
        if "roxygen2" in item
    ]
    assert refs, "no roxygen2 ref found in any workflow"
    # Compare the version, not the whole ref: setup-r-dependencies also accepts a
    # source qualifier ("any::roxygen2@7.3.3"), and adding one should not fail a
    # check about the pinned version.
    ref_pattern = re.compile(
        r"^(?:[A-Za-z0-9._-]+::)?roxygen2(?:@(?P<version>[A-Za-z0-9._-]+))?$"
    )
    for ref in refs:
        match = ref_pattern.match(ref)
        assert match, f"cannot read a version from the roxygen2 ref {ref!r}"
        version = match.group("version")
        assert version is not None, (
            f"workflow installs {ref!r} with no version, but the tracked man pages "
            f"only match roxygen2 {pinned}"
        )
        assert version == pinned, (
            f"workflow installs roxygen2 {version} but mk/r.mk pins {pinned}; the "
            "tracked man pages only match one version"
        )


def test_the_ruff_pin_matches_the_pre_commit_hook():
    """The pre-commit ruff hook has to run the ruff that CI runs.

    CI installs the `test` extra, so its ruff is the pyproject.toml pin; the hook
    runs whatever `ruff-pre-commit` rev it names. Dependabot bumps only the first,
    which is how they drifted apart (0.16.9 vs v0.16.0) unnoticed.
    """
    pyproject = tomllib.loads((REPO_ROOT / "pyproject.toml").read_text())
    pins = [
        req.removeprefix("ruff==")
        for req in pyproject["project"]["optional-dependencies"]["test"]
        if req.startswith("ruff==")
    ]
    assert len(pins) == 1, f"expected one ruff== pin in the test extra, found {pins}"

    config = yaml.safe_load((REPO_ROOT / ".pre-commit-config.yaml").read_text())
    revs = [
        repo["rev"]
        for repo in config["repos"]
        if repo["repo"] == "https://github.com/astral-sh/ruff-pre-commit"
    ]
    assert len(revs) == 1, f"expected one ruff-pre-commit repo, found {len(revs)}"

    assert revs[0] == f"v{pins[0]}", (
        f"pyproject.toml pins ruff=={pins[0]} but .pre-commit-config.yaml runs "
        f"ruff-pre-commit {revs[0]}; bump both together"
    )
