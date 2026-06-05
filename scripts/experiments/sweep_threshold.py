#!/usr/bin/env python3
"""Compare absolute vs relative core-loop convergence thresholds on a planted corpus.

For each network x config x seed: run Infomap, parse the top-level partition,
compute AMI against ground truth, and capture codelength + moveEvals (work).
Reports the quality(AMI)/work tradeoff so we can see whether a *single* relative
threshold holds AMI across difficulties where a single absolute one cannot.
"""
import glob
import os
import re
import subprocess
import sys
import numpy as np
from sklearn.metrics import adjusted_mutual_info_score

INFOMAP = "./Infomap"
CORPUS = "build/benchmarks/planted"
TMP = "/tmp/sweep_clu"
SEEDS = list(range(1, 11))  # 10 seeds

# config name -> (env overrides, extra CLI args)
CONFIGS = {
    "baseline (sweep)": ({}, []),
    "queue (Leiden-LM)": ({"INFOMAP_QUEUE_LOCAL_MOVE": "1"}, []),
}


def load_labels(path):
    truth = {}
    with open(path) as f:
        for line in f:
            nid, c = line.split()
            truth[int(nid)] = int(c)
    return truth


def parse_clu(path):
    pred, codelength = {}, None
    with open(path) as f:
        for line in f:
            if line.startswith("#"):
                m = re.search(r"codelength ([\d.]+) bits", line)
                if m:
                    codelength = float(m.group(1))
                continue
            parts = line.split()
            if len(parts) >= 2:
                pred[int(parts[0])] = int(parts[1])
    return pred, codelength


def run(net, cfg_env, cfg_args, seed):
    env = dict(os.environ, OMP_NUM_THREADS="1", **cfg_env)
    name = os.path.splitext(os.path.basename(net))[0]
    cmd = [INFOMAP, net, TMP, "--silent", "--clu", "--seed", str(seed),
           "--num-trials", "1"] + cfg_args
    r = subprocess.run(cmd, env=env, capture_output=True, text=True)
    me = re.search(r"moveEvals=(\d+)", r.stderr)
    move_evals = int(me.group(1)) if me else None
    pred, codelength = parse_clu(os.path.join(TMP, f"{name}.clu"))
    return pred, codelength, move_evals


def main():
    os.makedirs(TMP, exist_ok=True)
    nets = sorted(glob.glob(os.path.join(CORPUS, "*.net")))
    if not nets:
        sys.exit(f"No networks in {CORPUS} — run gen_planted.py first")

    for net in nets:
        name = os.path.splitext(os.path.basename(net))[0]
        truth_map = load_labels(net.replace(".net", ".labels"))
        nodes = sorted(truth_map)
        truth = [truth_map[n] for n in nodes]
        print(f"\n=== {name}  (n={len(nodes)}) ===")
        print(f"  {'config':<20} {'AMI mean±std':<16} {'AMI min':<9} "
              f"{'codelen mean':<14} {'moveEvals mean':<15} {'work':<7}")
        base_work = None
        for cfg, (cenv, cargs) in CONFIGS.items():
            amis, cls, evals = [], [], []
            for seed in SEEDS:
                pred, cl, me = run(net, cenv, cargs, seed)
                amis.append(adjusted_mutual_info_score(
                    truth, [pred.get(n, -1) for n in nodes]))
                if cl is not None:
                    cls.append(cl)
                if me is not None:
                    evals.append(me)
            ami = np.array(amis)
            mean_ev = np.mean(evals) if evals else float("nan")
            if base_work is None:
                base_work = mean_ev
            work = mean_ev / base_work if base_work else float("nan")
            print(f"  {cfg:<20} {ami.mean():.4f}±{ami.std():.4f}   "
                  f"{ami.min():<9.4f} {np.mean(cls):<14.6f} "
                  f"{mean_ev:<15.0f} {work:.2f}x")


if __name__ == "__main__":
    main()
