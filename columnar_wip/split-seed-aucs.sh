#!/bin/bash
# #1375: aucs.net -C -N10 over seeds 1..60, tip vs fix, both release builds without sanitizer.
# On the tip, a seed whose search compacts a split seed writes past the move level's arrays.
#   split-seed-aucs.sh <out.tsv> <tip binary> <fix binary>
# Row: seed arm exit codelength total_s instr compactions
# exit is the process status (non-zero = crashed); compactions is read from the fix arm's
# COL_HSPLIT_STATS dump (the tip has no such counter).
set -u
out=$1
tip=$2
fix=$3
net=networks/multilayer/real-world/aucs/aucs.net
dir=$(mktemp -d)
: >"$out"
for seed in $(seq 1 60); do
  for arm in tip fix; do
    bin=$tip
    [ "$arm" = fix ] && bin=$fix
    log=$(COL_HSPLIT_STATS=1 /usr/bin/time -l "$bin" "$net" "$dir" -C -N10 --seed "$seed" --no-file-output --timing-json "$dir/t.json" 2>&1)
    code=$?
    t=NA
    [ "$code" -eq 0 ] && t=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['timing']['total_s'])" "$dir/t.json")
    /bin/rm -f "$dir/t.json"
    cl=$(grep -oE 'Best codelength +[0-9.eE+-]+' <<<"$log" | awk '{print $3}')
    ins=$(grep -oE '[0-9]+ +instructions retired' <<<"$log" | awk '{print $1}')
    sc=$(grep -oE 'seed compactions: [0-9]+' <<<"$log" | awk '{print $3}')
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$seed" "$arm" "$code" "${cl:-NA}" "$t" "${ins:-NA}" "${sc:-0}" >>"$out"
  done
done
