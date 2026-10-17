#!/bin/sh
# Writes the seed corpus for fuzz_cluster: each cluster file prefixed with a
# few 3-byte option headers (see optionsFromHeader() in fuzz_cluster.cpp), so
# the fuzzer starts from valid cluster data under the main ways it is used.
#
# A file named <n>-<name>.<ext> in test/fuzz/cluster_seeds is Infomap's own
# output for network n of the harness (kNetworks), and is seeded on that
# network. They were written by `Infomap <network> -N 2 -s 1 -o clu,tree,ftree`
# and `Infomap <network> -s 1 --two-level -o clu,tree` (with -d on network 4),
# with the provenance header lines dropped. A twotriangles_* fixture is seeded
# on network 0, any other file on every network.
#
# Usage: seed_cluster_corpus.sh OUTDIR CLUSTER_FILE...
set -eu

outdir=$1
shift
rm -rf "$outdir"
mkdir -p "$outdir"

# name:byte0 flags:byte1:byte2, in octal. Byte 0 also takes the network
# (bits 0-2) and the format (bits 3-4); byte 2 is the seed (0 here) and the
# -F level (bits 6-7).
headers="
soft:000:000:000
no-infomap:100:000:000
two-level:200:000:000
hard:040:000:000
no-infomap-directed:100:001:000
entropy:000:002:000
no-infomap-regularized:100:004:000
no-infomap-meta:100:012:000
modules:000:006:000
trials:000:040:000
fast:000:000:100
fast-keep-levels:000:000:200
"

for file in "$@"; do
  [ -f "$file" ] || continue
  base=$(basename "$file")
  case "$base" in
    *.clu) format=0 ;;
    *.tree) format=1 ;;
    *.ftree) format=2 ;;
    *) continue ;;
  esac
  case "$base" in
    [0-7]-*) networks=${base%%-*} ;;
    twotriangles_*) networks=0 ;;
    *) networks="0 1 2 3 4 5 6 7" ;;
  esac
  for network in $networks; do
    for header in $headers; do
      name=${header%%:*}
      flags=${header#*:}
      byte0=$(printf '%03o' $((0${flags%%:*} | network | format << 3)))
      byte1=${flags#*:}
      byte2=${byte1#*:}
      byte1=${byte1%%:*}
      {
        # shellcheck disable=SC2059 # the format is the octal escapes
        printf "\\$byte0\\$byte1\\$byte2"
        cat "$file"
      } >"$outdir/$network-$name-$base"
    done
  done
done
