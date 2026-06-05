#!/usr/bin/env python3
"""Evaluate Infomap configs on a ground-truth corpus: AMI vs truth + work (moveEvals).

Reusable harness for result-changing experiments (warm start, parallel local-moving).
A config is (label, env-overrides, extra-CLI-args). Reports AMI distribution and mean
moveEvals over seeds, so quality (AMI) and work can be judged independent of wall time.

Usage: eval_corpus.py [--corpus DIR] [--seeds N]
Add configs in CONFIGS below as new directions come online.
"""
import argparse
import glob
import os
import re
import subprocess
import sys
import numpy as np
from sklearn.metrics import adjusted_mutual_info_score

INFOMAP = "./Infomap"
TMP = "/tmp/eval_corpus_clu"

# label -> (env overrides, extra CLI args)
CONFIGS = {
    "baseline": ({}, []),
    # "warmstart": ({"INFOMAP_SPECTRAL_WARMSTART": "1"}, []),  # enable when built
}


def load_labels(path):
    truth = {}
    with open(path) as f:
        for line in f:
            nid, c = line.split()
            truth[int(nid)] = int(c)
    return truth


def parse_clu(path):
    pred = {}
    with open(path) as f:
        for line in f:
            if line.startswith("#"):
                continue
            p = line.split()
            if len(p) >= 2:
                pred[int(p[0])] = int(p[1])
    return pred


def run(net, cfg_env, cfg_args, seed):
    env = dict(os.environ, OMP_NUM_THREADS="1", **cfg_env)
    name = os.path.splitext(os.path.basename(net))[0]
    cmd = [INFOMAP, net, TMP, "--silent", "--clu", "--seed", str(seed),
           "--num-trials", "1"] + cfg_args
    r = subprocess.run(cmd, env=env, capture_output=True, text=True)
    me = re.search(r"moveEvals=(\d+)", r.stderr)
    return parse_clu(os.path.join(TMP, f"{name}.clu")), (int(me.group(1)) if me else None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", default="build/benchmarks/lfr")
    ap.add_argument("--seeds", type=int, default=10)
    args = ap.parse_args()
    os.makedirs(TMP, exist_ok=True)
    seeds = list(range(1, args.seeds + 1))

    nets = sorted(glob.glob(os.path.join(args.corpus, "*.net")))
    if not nets:
        sys.exit(f"No networks in {args.corpus}")

    for net in nets:
        name = os.path.splitext(os.path.basename(net))[0]
        labels_path = net.replace(".net", ".labels")
        if not os.path.exists(labels_path):
            print(f"{name}: no .labels, skipping")
            continue
        truth_map = load_labels(labels_path)
        nodes = sorted(truth_map)
        truth = [truth_map[n] for n in nodes]
        print(f"\n=== {name} (n={len(nodes)}, k={len(set(truth))}) ===")
        print(f"  {'config':<12} {'AMI mean±std':<16} {'AMI min':<9} {'moveEvals mean':<15} work")
        base_ev = None
        for cfg, (cenv, cargs) in CONFIGS.items():
            amis, evals = [], []
            for seed in seeds:
                pred, me = run(net, cenv, cargs, seed)
                amis.append(adjusted_mutual_info_score(truth, [pred.get(n, -1) for n in nodes]))
                if me is not None:
                    evals.append(me)
            ami = np.array(amis)
            mean_ev = np.mean(evals) if evals else float("nan")
            if base_ev is None:
                base_ev = mean_ev
            work = mean_ev / base_ev if base_ev else float("nan")
            print(f"  {cfg:<12} {ami.mean():.4f}±{ami.std():.4f}   {ami.min():<9.4f} "
                  f"{mean_ev:<15.0f} {work:.2f}x")


if __name__ == "__main__":
    main()
