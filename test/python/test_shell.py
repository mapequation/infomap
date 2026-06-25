from __future__ import annotations


from infomap import shell


class FakeInfomap:
    def __init__(self, *, pretty=False):
        self.pretty = pretty
        self.loaded = []
        self.num_nodes = 0
        self.num_links = 0
        self.num_physical_nodes = 0
        self.num_top_modules = 0
        self.stateInput = False
        self.multilayerInput = False
        self.num_levels = 0
        self.num_non_trivial_top_modules = 0
        self.num_leaf_modules = 0
        self.effective_num_top_modules = 1.0
        self.effective_num_leaf_modules = 1.0
        self.codelength = 0.0
        self.index_codelength = 0.0
        self.module_codelength = 0.0
        self.one_level_codelength = 0.0
        self.relative_codelength_savings = 0.0
        self.entropy_rate = 0.0
        self.elapsed_time = 0.0
        self.meta_codelength = 0.0

    @property
    def _core(self):
        return self

    def read_file(self, filename):
        self.loaded.append(filename)
        self.num_nodes = 6
        self.num_links = 7
        self.num_physical_nodes = 6

    def isMultilayerNetwork(self):
        return False

    def numLeafNodes(self):
        return self.num_nodes


def test_create_namespace_preloads_default_infomap(monkeypatch, tmp_path):
    network_file = tmp_path / "network.net"
    network_file.write_text("*Vertices 0\n", encoding="utf-8")
    monkeypatch.setattr(shell, "Infomap", FakeInfomap)

    namespace = shell.create_namespace(network_file)

    assert namespace["im"].loaded == [str(network_file)]
    assert namespace["InfomapOptions"] is shell.InfomapOptions


def test_summary_prints_partial_state_before_run(monkeypatch, capsys):
    monkeypatch.setattr(shell, "Infomap", FakeInfomap)
    namespace = shell.create_namespace()
    namespace["im"].num_nodes = 3
    namespace["im"].num_links = 2
    namespace["im"].num_physical_nodes = 3

    data = namespace["summary"]()

    assert data == {
        "nodes": 3,
        "links": 2,
        "physical_nodes": 3,
        "state_nodes": 0,
        "higher_order": False,
        "status": "not run",
    }
    assert "status: not run" in capsys.readouterr().out


def test_summary_prints_run_state(monkeypatch, capsys):
    monkeypatch.setattr(shell, "Infomap", FakeInfomap)
    namespace = shell.create_namespace()
    namespace["im"].num_nodes = 6
    namespace["im"].num_links = 7
    namespace["im"].num_physical_nodes = 6
    namespace["im"].num_top_modules = 2
    namespace["im"].num_levels = 2
    namespace["im"].num_non_trivial_top_modules = 2
    namespace["im"].num_leaf_modules = 2
    namespace["im"].effective_num_top_modules = 2.0
    namespace["im"].effective_num_leaf_modules = 2.0
    namespace["im"].codelength = 1.0
    namespace["im"].index_codelength = 0.25
    namespace["im"].module_codelength = 0.75
    namespace["im"].one_level_codelength = 2.0
    namespace["im"].relative_codelength_savings = 0.5
    namespace["im"].entropy_rate = 1.25
    namespace["im"].elapsed_time = 0.01

    data = namespace["summary"]()

    assert data["status"] == "run"
    assert data["codelength"] == 1.0
    assert data["index_codelength"] == 0.25
    assert data["module_codelength"] == 0.75
    assert data["top_modules"] == 2
    assert data["levels"] == 2
    assert data["entropy_rate"] == 1.25
    output = capsys.readouterr().out
    assert "codelength: 1" in output
    assert "top_modules: 2" in output
    assert "entropy_rate: 1.25" in output


def test_options_delegates_to_generated_options(monkeypatch):
    monkeypatch.setattr(shell, "Infomap", FakeInfomap)
    namespace = shell.create_namespace()

    assert namespace["options"]() is shell.InfomapOptions


def test_main_loads_file_and_uses_injected_launcher(monkeypatch, tmp_path):
    network_file = tmp_path / "network.net"
    network_file.write_text("*Vertices 0\n", encoding="utf-8")
    monkeypatch.setattr(shell, "Infomap", FakeInfomap)
    launched = {}

    def launcher(namespace, banner, *, use_ipython):
        launched["namespace"] = namespace
        launched["banner"] = banner
        launched["use_ipython"] = use_ipython

    result = shell.main(["--no-ipython", str(network_file)], launcher=launcher)

    assert result == 0
    assert launched["use_ipython"] is False
    assert launched["namespace"]["im"].loaded == [str(network_file)]
    assert f"Loaded: {network_file}" in launched["banner"]


def test_main_rejects_missing_preload_file(capsys):
    result = shell.main(["missing.net"], launcher=lambda *args, **kwargs: None)

    assert result == 1
    assert "Error: No such file: missing.net" in capsys.readouterr().err
