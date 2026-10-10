#!/usr/bin/env python3
"""Fetch or build every input of the columnar benchmark set.

    python columnar_wip/fetch-benchmark-networks.py [--out networks/columnar-benchmark] [NAME ...]

Reads columnar_wip/benchmark-networks.toml, writes each entry's file(s) under ``--out``
(default ``networks/columnar-benchmark``) and checks them against
columnar_wip/benchmark-networks.sha256, so a benchmark run on the result uses the same
bytes as the published numbers. A file already present with the pinned checksum is not
rebuilt. ``NAME`` restricts the run to those manifest entries; ``--update-checksums``
rewrites the pins from what was built instead of checking them.

Needs Python 3.11+ and the mapequation-networks package (the version is pinned in
columnar_wip/benchmark-networks.md). Downloads go to that package's cache. air30k is the
slow one: about 270 MB of DB1B coupons, grouped into 10 M itineraries (~15 min once).
"""

import argparse
import hashlib
import re
import shutil
import subprocess
import sys
import tomllib
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
MANIFEST = HERE / "benchmark-networks.toml"
CHECKSUMS = HERE / "benchmark-networks.sha256"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def outputs(entry):
    """The files an entry writes, relative to the output directory."""
    if entry["recipe"] == "overlapping-memory":
        return [
            f"om/{om_name(om, e, entry['seed'])}.{ext}"
            for om in entry["memberships"]
            for e in entry["n_trigrams"]
            for ext in ("net", "clu")
        ]
    return [entry["file"]] + ([entry["meta"]] if "meta" in entry else [])


def om_name(om, n_trigrams, seed):
    return f"overlapping-memory-n256-om{om}-nc64-E{n_trigrams}-mu0.1-seed{seed}"


def build_repo(entry, out, manifest):
    shutil.copyfile(REPO / entry["path"], out / entry["file"])


def build_netzschleuder(entry, out, manifest):
    from networks import netzschleuder

    netzschleuder.load(entry["dataset"]).to_link_list(
        out, name=Path(entry["file"]).stem
    )


def build_lazega(entry, out, manifest):
    from networks import netzschleuder

    net = netzschleuder.load("law_firm")
    pairs = set()
    for source, target, _weight, layer in net.rows():
        if layer == "2":
            pairs |= {(int(source), int(target)), (int(target), int(source))}
    with open(out / entry["file"], "w", newline="\n") as f:
        f.writelines(f"{a} {b}\n" for a, b in sorted(pairs))
    gender = net.nodes()["nodeGender"]
    with open(out / entry["meta"], "w", newline="\n") as f:
        f.writelines(f"{node} {value}\n" for node, value in sorted(gender.items()))


def build_malaria(entry, out, manifest):
    from networks import netzschleuder

    links, n_nodes = [], 0
    for layer in range(1, 10):
        net = netzschleuder.load("malaria_genes", subgraph=f"HVR_{layer}")
        n_nodes = max(n_nodes, len(net.nodes()["name"]))
        pairs = {(int(a) + 1, int(b) + 1) for a, b in net.edges()}
        pairs |= {(b, a) for a, b in pairs}
        links += [(layer, a, b) for a, b in sorted(pairs)]
    with open(out / entry["file"], "w", newline="\n") as f:
        f.write(f"*Vertices {n_nodes}\n")
        f.writelines(f'{i} "{i}"\n' for i in range(1, n_nodes + 1))
        f.write("*Intra\n")
        f.writelines(f"{layer} {a} {b} 1\n" for layer, a, b in links)


def build_snap(entry, out, manifest):
    from networks import snap

    shutil.copyfile(snap.load(entry["dataset"]).path, out / entry["file"])


def build_wikispeedia(entry, out, manifest):
    from networks import paths

    memory = paths.load("wikispeedia").to_state_network(order=2, max_nodes=300)
    memory.write(out / entry["file"])


def build_db1b_air30k(entry, out, manifest):
    """Second-order airport network from the DB1B coupons, as in Rosvall et al. 2014.

    Every passenger-weighted itinerary of 2011 Q1-Q3 contributes its consecutive airport
    triples; triples outside the listed airports are dropped. Written in the layout of the
    paper's air30k.net: vertices in IATA order, states sorted by (previous, current), links
    sorted by state id, weights in %g.
    """
    from networks import paths

    airports = [
        line.split("\t")
        for line in (REPO / entry["airports"]).read_text(encoding="utf-8").splitlines()
        if not line.startswith("#")
    ]
    vertex = {code: k for k, (code, _name) in enumerate(airports, start=1)}
    weight = defaultdict(float)
    for quarter in (1, 2, 3):
        coupons = paths.load("db1b-coupon", year=2011, quarter=quarter, progress=False)
        for nodes, passengers in coupons.paths():
            for a, b, c in zip(nodes, nodes[1:], nodes[2:], strict=False):
                if a != b and a in vertex and b in vertex and c in vertex:
                    weight[(vertex[a], vertex[b]), (vertex[b], vertex[c])] += passengers
    states = sorted({state for link in weight for state in link})
    state_id = {state: k for k, state in enumerate(states, start=1)}
    name = {k: n for k, (_code, n) in enumerate(airports, start=1)}
    with open(out / entry["file"], "w", newline="\n") as f:
        f.write(f"*Vertices {len(airports)}\n")
        f.writelines(f'{k} "{name[k]}"\n' for k in name)
        f.write("*States\n")
        f.writelines(
            f'{state_id[(p, c)]} {c} "{{{name[p]}}}_{name[c]}"\n' for p, c in states
        )
        f.write("*Links\n")
        links = sorted(weight, key=lambda link: (state_id[link[0]], state_id[link[1]]))
        f.writelines(
            f"{state_id[s]} {state_id[t]} {weight[s, t]:g}\n" for s, t in links
        )


def build_state_meta(entry, out, manifest):
    (source,) = [e for e in manifest["network"] if e["name"] == entry["of"]]
    subprocess.run(
        [
            sys.executable,
            HERE / "make-state-meta.py",
            out / source["file"],
            entry["mode"],
            out / entry["file"],
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )


def build_overlapping_memory(entry, out, manifest):
    from networks.generate import overlapping_memory_benchmark

    (out / "om").mkdir(exist_ok=True)
    for om in entry["memberships"]:
        for n_trigrams in entry["n_trigrams"]:
            net = overlapping_memory_benchmark(om, n_trigrams, seed=entry["seed"])
            stem = f"om/{om_name(om, n_trigrams, entry['seed'])}"
            net.write(out / f"{stem}.net")
            with open(out / f"{stem}.clu", "w", newline="\n") as f:
                f.writelines(f"{s} {m}\n" for s, m in sorted(net.partition.items()))


BUILD = {
    "repo": build_repo,
    "netzschleuder": build_netzschleuder,
    "lazega": build_lazega,
    "malaria": build_malaria,
    "snap": build_snap,
    "wikispeedia": build_wikispeedia,
    "db1b-air30k": build_db1b_air30k,
    "state-meta": build_state_meta,
    "overlapping-memory": build_overlapping_memory,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("names", nargs="*", help="manifest entries (default: all)")
    parser.add_argument(
        "--out", type=Path, default=REPO / "networks" / "columnar-benchmark"
    )
    parser.add_argument("--update-checksums", action="store_true")
    args = parser.parse_args()

    manifest = tomllib.loads(MANIFEST.read_text(encoding="utf-8"))
    pinned = {}
    if CHECKSUMS.exists():
        for line in CHECKSUMS.read_text(encoding="utf-8").splitlines():
            digest, name = re.match(r"([0-9a-f]{64})  (.+)", line).groups()
            pinned[name] = digest
    entries = [
        e for e in manifest["network"] if not args.names or e["name"] in args.names
    ]
    if unknown := set(args.names) - {e["name"] for e in entries}:
        sys.exit(f"not in the manifest: {', '.join(sorted(unknown))}")

    args.out.mkdir(parents=True, exist_ok=True)
    failed = []
    for entry in entries:
        files = outputs(entry)
        fresh = all(
            (args.out / name).exists() and pinned.get(name) == sha256(args.out / name)
            for name in files
        )
        if not fresh or args.update_checksums:
            print(f"{entry['name']}: building ({entry['recipe']})", flush=True)
            BUILD[entry["recipe"]](entry, args.out, manifest)
        for name in files:
            digest = sha256(args.out / name)
            if args.update_checksums:
                pinned[name] = digest
            elif pinned.get(name) != digest:
                failed.append(name)
                print(f"  {name}: sha256 {digest} does not match the pin", flush=True)
        print(f"{entry['name']}: {', '.join(files)}", flush=True)

    if args.update_checksums:
        CHECKSUMS.write_text(
            "".join(f"{pinned[name]}  {name}\n" for name in sorted(pinned)),
            encoding="utf-8",
        )
    if failed:
        sys.exit(
            f"{len(failed)} file(s) differ from columnar_wip/benchmark-networks.sha256"
        )


if __name__ == "__main__":
    main()
