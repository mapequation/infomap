#!/usr/bin/env python3
"""Generate a state-id-keyed metadata file for a higher-order benchmark network.

    python3 columnar_wip/make-state-meta.py <net-with-*States> <mode> <out.meta>

Writes clu format -- one "<stateId> <category>" line per state -- which is what
``--meta-data`` reads. It exists because no metadata file for any higher-order
benchmark network is checked in (``networks/`` is a data directory outside the
repo, and its only metadata network, lazega, is first-order), so the meta +
higher-order configuration has to be reconstructed rather than referenced.

Modes:

``usstate``
    Category is the two-letter US state code parsed out of the physical node's
    ``*Vertices`` name (``"City,ST:Airport"``), mapped to an integer by sorted
    order of the distinct codes. Real geographic metadata for air30k: 52
    categories over 183 airports.
``phys``
    Category is the state's physical node id.
``mod<K>``
    Category is ``stateId % K`` -- structure-independent control metadata.
``pmod<K>``
    Category is ``physicalId % K``, so all states of a physical node share a
    category. This is the shape a real node attribute has on multilayer input.

Multilayer input has no ``*States`` section of its own; the state ids are the
ones the expansion assigns. Materialize them first and generate from that file:

    Infomap <net> <dir> -o states --out-name X      ->  X_states.net
    python3 columnar_wip/make-state-meta.py X_states.net pmod8 <out.meta>

then pass ``--meta-data <out.meta>`` to a run on the *original* network.
"""

import re
import sys


def read_states(path):
    """Return ({physicalId: name}, [(stateId, physicalId), ...])."""
    names, states, section = {}, [], None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            if s.startswith("*"):
                section = s.split()[0].lower()
                continue
            if section == "*vertices":
                m = re.match(r'(\d+)\s+"(.*)"', s)
                if m:
                    names[int(m.group(1))] = m.group(2)
            elif section == "*states":
                p = s.split()
                states.append((int(p[0]), int(p[1])))
    return names, states


def category_function(mode, names, states):
    if mode == "usstate":
        code = {}
        for pid, name in names.items():
            m = re.search(r",([A-Z]{2})\b", name)
            code[pid] = m.group(1) if m else "??"
        order = {c: i for i, c in enumerate(sorted(set(code.values())))}
        return lambda sid, pid: order[code.get(pid, "??")]
    if mode == "phys":
        return lambda sid, pid: pid
    if re.fullmatch(r"mod\d+", mode):
        k = int(mode[3:])
        return lambda sid, pid: sid % k
    if re.fullmatch(r"pmod\d+", mode):
        k = int(mode[4:])
        return lambda sid, pid: pid % k
    sys.exit("unknown mode '%s'" % mode)


def main(argv):
    if len(argv) != 4:
        sys.exit(__doc__)
    net, mode, out = argv[1], argv[2], argv[3]
    names, states = read_states(net)
    if not states:
        sys.exit("no *States section in %s -- expand multilayer input with -o states first" % net)
    category = category_function(mode, names, states)

    with open(out, "w") as f:
        f.write("# stateId metaCategory (make-state-meta.py %s %s)\n" % (net.split("/")[-1], mode))
        for sid, pid in states:
            f.write("%d %d\n" % (sid, category(sid, pid)))
    print("%d states, %d categories -> %s"
          % (len(states), len({category(s, p) for s, p in states}), out))


if __name__ == "__main__":
    main(sys.argv)
