from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Union

import pytest
from infomap import Infomap


@dataclass(frozen=True)
class TestPaths:
    repo_root: Path
    fixtures_root: Path
    network_fixtures: Path
    graph_fixtures: Path
    expected_fixtures: Path
    example_networks: Path


def _normalized_pajek_lines(path: Path) -> list[str]:
    lines = path.read_text().splitlines()
    if lines and lines[0].startswith("# v"):
        lines = lines[1:]
    return lines


@pytest.fixture(scope="session")
def test_paths() -> TestPaths:
    repo_root = Path(__file__).resolve().parents[2]
    fixtures_root = repo_root / "test" / "fixtures"
    return TestPaths(
        repo_root=repo_root,
        fixtures_root=fixtures_root,
        network_fixtures=fixtures_root / "networks",
        graph_fixtures=fixtures_root / "graphs",
        expected_fixtures=fixtures_root / "expected",
        example_networks=repo_root / "examples" / "networks",
    )


@pytest.fixture
def network_fixture_path(test_paths: TestPaths):
    def _network_fixture_path(name: str) -> Path:
        return test_paths.network_fixtures / name

    return _network_fixture_path


@pytest.fixture
def graph_fixture_path(test_paths: TestPaths):
    def _graph_fixture_path(name: str) -> Path:
        return test_paths.graph_fixtures / name

    return _graph_fixture_path


@pytest.fixture
def expected_fixture_path(test_paths: TestPaths):
    def _expected_fixture_path(name: str) -> Path:
        return test_paths.expected_fixtures / name

    return _expected_fixture_path


@pytest.fixture
def example_network_path(test_paths: TestPaths):
    def _example_network_path(name: str) -> Path:
        return test_paths.example_networks / name

    return _example_network_path


def _parse_graph_fixture(path: Path) -> list[tuple[int, ...]]:
    links: list[tuple[int, ...]] = []
    for line_number, raw_line in enumerate(path.read_text().splitlines(), start=1):
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue

        parts = line.split()
        if len(parts) == 2:
            links.append((int(parts[0]), int(parts[1])))
        elif len(parts) == 3:
            links.append((int(parts[0]), int(parts[1]), float(parts[2])))
        else:
            raise ValueError(f"Invalid graph fixture line {line_number} in {path}: {raw_line!r}")

    return links


@pytest.fixture
def graph_fixture_links(graph_fixture_path):
    def _graph_fixture_links(name: str) -> list[tuple[int, ...]]:
        return _parse_graph_fixture(graph_fixture_path(name))

    return _graph_fixture_links


@pytest.fixture
def load_graph_fixture(graph_fixture_links):
    def _load_graph_fixture(im: Infomap, name: str, *, method: str = "add_links") -> list[tuple[int, ...]]:
        links = graph_fixture_links(name)
        if method == "add_links":
            im.add_links(links)
        elif method == "add_link":
            for link in links:
                im.add_link(*link)
        else:
            raise ValueError(f"Unsupported graph fixture load method: {method}")
        return links

    return _load_graph_fixture


def _canonical_modules(modules: Union[Iterable[tuple[int, int]], dict[int, int]]) -> list[list[int]]:
    grouped: dict[int, list[int]] = {}
    items = modules.items() if isinstance(modules, dict) else modules
    for node_id, module_id in items:
        grouped.setdefault(module_id, []).append(node_id)
    return sorted(sorted(group) for group in grouped.values())


@pytest.fixture
def canonical_modules():
    return _canonical_modules


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
