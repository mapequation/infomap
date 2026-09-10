"""Standalone port of build_syn_network (Andrea Lancichinetti's trigram generator) from
paper-projects/higher-order-regularization/src/main.py, with one change: the RNG is seeded
with the sample id so the file is reproducible.  Everything else follows the source
line by line.  Writes <out>/network<suffix>.net and <out>/planted_partition<suffix>.clu
(state_id module, 0-based module ids -- same shape as the existing .clu files).

usage: python3 columnar_wip/make-om-network.py OUTDIR N om nc E mu r
"""

import random
import sys
from pathlib import Path

import numpy as np


def get_communities(mems):
    communities = {}
    for n in range(len(mems)):
        for c in mems[n]:
            communities.setdefault(c, []).append(n)
    return communities


def check_pair_is_ok(pair_one, rc, pairs_coms):
    return pair_one not in pairs_coms or pairs_coms[pair_one] == rc


def select_random_trigram_incommunities(communities, n_sample):
    pairs_coms = {}
    trigrams = []
    community_ids = list(communities.keys())
    random_communities = [random.sample(community_ids, 1)[0] for _ in range(n_sample)]
    rejections = 0
    for rc in random_communities:
        while True:
            trial_tri = random.sample(communities[rc], 3)
            pair_one = (trial_tri[0], trial_tri[1])
            pair_two = (trial_tri[1], trial_tri[2])
            if check_pair_is_ok(pair_one, rc, pairs_coms) and check_pair_is_ok(
                pair_two, rc, pairs_coms
            ):
                trigrams.append(trial_tri)
                pairs_coms[pair_one] = rc
                pairs_coms[pair_two] = rc
                break
            rejections += 1
    return trigrams, pairs_coms


def compute_mems(N, M, nc):
    all_mems = []
    for i in range(M):
        for _n in range(nc):
            all_mems.append(i)
    all_mems = random.sample(all_mems, len(all_mems))
    all_nodes = random.sample(range(N), N)
    mems = [[] for _ in range(N)]
    try_counter = 0
    while True:
        discarded = []
        for counter, m in enumerate(all_mems):
            node = counter % len(all_nodes)
            if m not in mems[all_nodes[node]]:
                mems[all_nodes[node]].append(m)
            else:
                discarded.append(m)
        all_mems = list(discarded)
        all_nodes = random.sample(range(N), N)
        if try_counter > 100:
            break
        if len(discarded) == 0:
            break
    return mems, get_communities(mems)


def select_random_trigram_outcommunities_without_creating_more_state_nodes(
    pairs_coms, n_sample
):
    node_pairs_map = {}
    for pair in pairs_coms:
        node_pairs_map.setdefault(pair[0], []).append(pair)
    keys = list(
        pairs_coms.keys()
    )  # built once; the source rebuilds it per draw (same distribution)
    num_trials = 0
    max_num_trials = n_sample * 10
    num_invalid = 0
    trigrams = []
    while len(trigrams) < n_sample:
        num_trials += 1
        if num_trials > max_num_trials:
            print(
                f"Warning: max trials reached with {len(trigrams)}/{n_sample} out-community trigrams"
            )
            break
        pair_one = random.sample(keys, 1)[0]
        module_one = pairs_coms[pair_one]
        cands = [
            p
            for p in node_pairs_map.get(pair_one[1], [])
            if pairs_coms[p] != module_one
        ]
        if not cands:
            num_invalid += 1
            continue
        pair_two = random.sample(cands, 1)[0]
        trigrams.append((pair_one[0], pair_one[1], pair_two[1]))
    return trigrams


def get_suffix(N, om, nc, E, mu, r):
    return f"_N{N}_om{om}_nc{nc}_E{E}_mu{int(mu * 100)}_sample{r}"


def build_syn_network(out, N, om, nc, E, mu, r):
    random.seed(r)
    M = om * 4
    mems, communities = compute_mems(N, M, nc)
    trigrams_all, pairs_coms = select_random_trigram_incommunities(
        communities, int((1 - mu) * E)
    )
    trigrams_all += (
        select_random_trigram_outcommunities_without_creating_more_state_nodes(
            pairs_coms, int(mu * E)
        )
    )
    suffix = get_suffix(N, om, nc, E, mu, r)

    trigrams_all_hist = {}
    for t in trigrams_all:
        trigrams_all_hist[tuple(t)] = trigrams_all_hist.get(tuple(t), 0) + 1

    states = np.zeros(N * N).astype(int).reshape(N, N)
    state_ID = 0
    with open(out / f"planted_partition{suffix}.clu", "w") as clu:
        for t, rc in pairs_coms.items():
            state_ID += 1
            clu.write(f"{state_ID} {rc}\n")
            states[t[0]][t[1]] = state_ID
    extra = 0
    for t in trigrams_all_hist:
        for a, b in ((t[0], t[1]), (t[1], t[2])):
            if states[a][b] == 0:
                state_ID += 1
                states[a][b] = state_ID
                extra += 1

    with open(out / f"network{suffix}.net", "w") as f:
        f.write(f"*Vertices {N}\n")
        f.writelines(f'{node_ID} "{node_ID}"\n' for node_ID in range(1, N + 1))
        f.write("*States\n")
        for i in range(1, N + 1):
            for j in range(1, N + 1):
                sid = states[i - 1][j - 1]
                if sid > 0:
                    f.write(f'{sid} {j} "{{{i}}}_{j}"\n')
        f.write("*Links\n")
        for t, w in trigrams_all_hist.items():
            f.write(f"{states[t[0]][t[1]]} {states[t[1]][t[2]]} {w}\n")

    sizes = sorted(len(v) for v in communities.values())
    print(
        f"{suffix}: modules={len(communities)} sizes={sizes[0]}..{sizes[-1]} "
        f"states={state_ID} (unplanted extra={extra}) links={len(trigrams_all_hist)} "
        f"trigrams={len(trigrams_all)} mems/node={np.mean([len(m) for m in mems]):.2f}"
    )


if __name__ == "__main__":
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    N, om, nc, E = map(int, sys.argv[2:6])
    mu = float(sys.argv[6])
    r = int(sys.argv[7])
    build_syn_network(out, N, om, nc, E, mu, r)
