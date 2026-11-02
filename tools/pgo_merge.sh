#!/bin/bash
# Profile-guided optimisation, step 2 of 3 (2026-11-01; sa2-nx's tools/win/pgo_merge.ps1 for the cloud build).
# Step 1: the instrumented NRO (cmake -DLOS_PGO=generate in out/nx), played on the console: it writes
# pgo/los_pgo_<N>.stream next to the NRO (one per session, app/src/los_pgo.cpp). Copy those files here and run:
#   bash tools/pgo_merge.sh <folder with the .stream files>
# Every session's counts are added into the .gcda files of pgo/ (the gcov-tool of devkitA64 under Wine, merge-stream:
# the streams name the files by the paths the instrumented build's compiler saw), then their summary is filled in
# (sa2-nx's tools/pgo_summary.py: the stream API writes it as zero). Step 3: cmake -DLOS_PGO=use, build.
# merge-stream ADDS to existing .gcda files: it stops when pgo/ already has some (move them aside first).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
STREAMS="$(cd "$1" && pwd)"
OUT="$REPO/pgo"
TOOL=/opt/devkitpro-win/devkitA64/bin/aarch64-none-elf-gcov-tool.exe
mkdir -p "$OUT"
if ls "$OUT"/*.gcda >/dev/null 2>&1 || find "$OUT" -name '*.gcda' | grep -q .; then
  echo "pgo_merge: $OUT already has .gcda files: move them aside first" >&2; exit 1
fi
n=0
for f in "$STREAMS"/los_pgo_*.stream; do
  echo "merging $(basename "$f") ($(du -h "$f" | cut -f1))"
  (cd "$REPO" && WINEDEBUG=-all /usr/lib/wine/wine64 "$TOOL" merge-stream "$f")
  n=$((n + 1))
done
count=$(find "$OUT" -name '*.gcda' | wc -l)
[ "$count" -gt 0 ] || { echo "pgo_merge: no .gcda in $OUT after merging (the streams name another folder?)" >&2; exit 1; }
python3 "$REPO/tools/pgo_summary.py" "$OUT" "$n"
echo "$n session(s) merged into $count file(s) in $OUT"
