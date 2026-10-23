"""Snapshot row label -> input network, shared by the snapshot drivers and table builders.

A row label is a network name from columnar_wip/benchmark-networks.md, optionally followed by
" `flags`", or a member of the overlapping-memory family (`om5_N256_E100k ...`). Inputs are the
files fetch-benchmark-networks.py writes under networks/columnar-benchmark/.
"""

import re

NB = "networks/columnar-benchmark"  # fetch-benchmark-networks.py output
BASE = {  # snapshot label (before any " `flags`" suffix) -> input network
    "ninetriangles": f"{NB}/ninetriangles.net",
    "jazz": f"{NB}/jazz.txt",
    "netscience": f"{NB}/netscience.txt",
    "powergrid": f"{NB}/powergrid.txt",
    "polblogs": f"{NB}/polblogs.txt",
    "word_assoc": f"{NB}/word_assoc.txt",
    "word_assoc (pref.)": f"{NB}/word_assoc.txt",
    "web-NotreDame": f"{NB}/web-NotreDame.txt",
    "lazega": f"{NB}/lazega.net",
    "multilayer (ex.)": f"{NB}/multilayer.net",
    "malaria": f"{NB}/malaria.net",
    "air30k": f"{NB}/air30k.net",
    "air30k (reg.)": f"{NB}/air30k.net",
    "air30k (meta)": f"{NB}/air30k.net",
    "wikispeedia": f"{NB}/wikispeedia_states.net",
}
OM = re.compile(r"^(?:overlapping )?om(\d)_N256_E(\d+)k\b")  # family member labels


def network(label):
    """Input network of a snapshot label, or None if the label is not known."""
    m = OM.match(label)
    if m:
        om, e = int(m[1]), int(m[2]) * 1000
        return f"{NB}/om/overlapping-memory-n256-om{om}-nc64-E{e}-mu0.1-seed1.net"
    return BASE.get(label.split(" `")[0])
