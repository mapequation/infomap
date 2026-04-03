from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import pytest
from infomap import Infomap


@dataclass(frozen=True)
class TestPaths:
    repo_root: Path
    fixtures: Path
    example_networks: Path


def _normalized_pajek_lines(path: Path) -> list[str]:
    lines = path.read_text().splitlines()
    if lines and lines[0].startswith("# v"):
        lines = lines[1:]
    return lines


@pytest.fixture(scope="session")
def test_paths() -> TestPaths:
    repo_root = Path(__file__).resolve().parents[2]
    return TestPaths(
        repo_root=repo_root,
        fixtures=repo_root / "test" / "fixtures",
        example_networks=repo_root / "examples" / "networks",
    )


@pytest.fixture
def fixture_path(test_paths: TestPaths):
    def _fixture_path(name: str) -> Path:
        return test_paths.fixtures / name

    return _fixture_path


@pytest.fixture
def example_network_path(test_paths: TestPaths):
    def _example_network_path(name: str) -> Path:
        return test_paths.example_networks / name

    return _example_network_path


@pytest.fixture
def output_dir(tmp_path: Path) -> Path:
    path = tmp_path / "output"
    path.mkdir()
    return path


@pytest.fixture
def make_infomap():
    def _make_infomap(*, seed: int = 123, num_trials: int = 1, silent: bool = True, **kwargs) -> Infomap:
        return Infomap(seed=seed, num_trials=num_trials, silent=silent, **kwargs)

    return _make_infomap


@pytest.fixture
def assert_pajek_matches_fixture():
    def _assert_pajek_matches_fixture(actual_path: Path, expected_path: Path) -> None:
        assert _normalized_pajek_lines(actual_path) == _normalized_pajek_lines(expected_path)

    return _assert_pajek_matches_fixture
