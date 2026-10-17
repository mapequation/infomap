#!/bin/sh
# Writes the seed corpus for fuzz_run: each network file prefixed with a few
# 8-byte option headers (see optionsFromHeader() in fuzz_run.cpp), so the
# fuzzer starts from valid networks under the main option combinations.
#
# Usage: seed_run_corpus.sh OUTDIR NETWORK_FILE...
set -eu

outdir=$1
shift
rm -rf "$outdir"
mkdir -p "$outdir"

# Octal bytes 0-7. Byte 7 (037) is 8 core loops and 4 tune iterations.
headers="
default:000:000:000:000:000:000:000:037
directed:001:000:000:000:000:000:000:037
recorded:031:000:000:000:000:000:000:037
regularized:100:000:000:000:000:000:000:037
entropy-two-level:240:000:000:000:000:000:000:037
markov-modules:000:002:002:000:000:000:000:037
meta:000:000:000:000:000:003:000:037
outdirdir-relax:003:000:000:002:000:000:000:037
rawdir-variable-markov:004:005:000:000:000:000:000:037
"

for file in "$@"; do
  [ -f "$file" ] || continue
  for header in $headers; do
    name=${header%%:*}
    bytes=$(printf '%s' "${header#*:}" | sed 's/\([0-7][0-7][0-7]\):*/\\\1/g')
    {
      # shellcheck disable=SC2059 # the format is the octal escapes
      printf "$bytes"
      cat "$file"
    } >"$outdir/$name-$(basename "$file")"
  done
done
