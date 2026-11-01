#!/bin/sh
# build_archive.sh - builds the mod's SDATA/sdat.viv from the games' own files in one run:
#   step 1 add_mw_songs.py     UG2's original sdat.viv + MW's 26 songs                  -> OUT/1_mw/sdat.viv      (491,477,696 B)
#   step 2 add_ug1_songs.py    + UG1's 27 music streams                                -> OUT/2_ug1/sdat.viv     (631,243,008 B)
#   step 3 add_carbon_songs.py + Carbon's 30 songs                                     -> OUT/3_carbon/sdat.viv  (893,398,656 B = the mod's archive)
# Each step checks its inputs' SHA-256 and every number of its result before it writes; after each step the decode check proves the added
# streams decode to the same PCM as the source game's own; at the end the map header is generated from the run's maps and compared with
# src/musicmap.h. The game folders are only read; every output goes into OUT. Needs python3, gcc and about 4 GB of free RAM (step 3).
# usage: tools/archive/build_archive.sh [UG2 folder] [MW folder] [UG1 folder] [Carbon folder] [OUT]
set -e
T="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$T/../.." && pwd)"
G="${1:-/mnt/c/Need for Speed Underground 2}"
MW="${2:-/mnt/c/Program Files (x86)/NFS Most Wanted}"
U1="${3:-/mnt/c/NFS Underground}"
CB="${4:-/mnt/c/Need for Speed Carbon Collectors Edition}"
OUT="${5:-$HOME/golden_era_scratch/archive_out}"
VIV0="$G/SDATA/sdat.viv"
MWMPF="$MW/SOUND/PFDATA/MW_Music.mpf"; MWMUS="$MW/SOUND/PFDATA/MW_Music.mus"
U1AST="$U1/SNDSTREAMS/STRM_Music_UG0_MB.ast"; U1ABK="$U1/Sound/STRMBANK/STRM_Music_UG0_MB.abk"
CBMPF="$CB/SOUND/PFDATA/cb_mus_0.mpf"; CBMUS="$CB/SOUND/PFDATA/cb_mus_0.mus"
PY="python3 -I -B"
for f in "$VIV0" "$MWMPF" "$MWMUS" "$U1AST" "$U1ABK" "$CBMPF" "$CBMUS"; do [ -f "$f" ] || { echo "ABORT: input missing: $f"; exit 1; }; done
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
for d in "$G" "$MW" "$U1" "$CB" "$ROOT"; do
  D="$(cd "$d" && pwd)"; case "$OUT/" in "$D"/*) echo "ABORT: OUT ($OUT) is inside $D; give an OUT outside the game folders and the project"; exit 1;; esac
done
start=$(date +%s)
echo "== inputs"
sha256sum "$VIV0" "$MWMPF" "$MWMUS" "$U1AST" "$U1ABK" "$CBMPF" "$CBMUS" | tee "$OUT/inputs_sha256.txt"
echo "== the decoders"
mkdir -p "$OUT/bin"
gcc -O2 -Wall -Wextra -o "$OUT/bin/streamdec" "$T/streamdec.c" 2> "$OUT/bin/streamdec_wall.txt"
gcc -O2 -Wall -Wextra -o "$OUT/bin/bardec" "$T/bardec.c" 2> "$OUT/bin/bardec_wall.txt"
echo "streamdec -Wall -Wextra: $(wc -l < "$OUT/bin/streamdec_wall.txt") lines; bardec: $(wc -l < "$OUT/bin/bardec_wall.txt") lines"
echo "== step 1: MW's 26 songs"
mkdir -p "$OUT/1_mw"
$PY "$T/add_mw_songs.py" "$VIV0" "$MWMPF" "$MWMUS" "$OUT/1_mw" > "$OUT/1_mw/stdout.txt"; tail -1 "$OUT/1_mw/build_log.txt"
cmp "$OUT/1_mw/MusicSFx.mpf" "$ROOT/data/MusicSFx_ug2_mw.mpf" || { echo "FAIL: step 1's map != data/MusicSFx_ug2_mw.mpf"; exit 1; }
cmp "$OUT/1_mw/MusicSFx_original.mpf" "$ROOT/data/MusicSFx_ug2_original.mpf" || { echo "FAIL: UG2's map != data/MusicSFx_ug2_original.mpf"; exit 1; }
echo "step 1 maps = data/MusicSFx_ug2_mw.mpf + data/MusicSFx_ug2_original.mpf"
sh "$T/check_decode_mw.sh" "$OUT/bin/streamdec" "$OUT/bin/bardec" "$OUT/1_mw" "$MWMUS" "$OUT/1_mw/decode_check.txt"
echo "== step 2: UG1's 27 streams"
mkdir -p "$OUT/2_ug1"
$PY "$T/add_ug1_songs.py" "$OUT/1_mw/sdat.viv" "$U1AST" "$U1ABK" "$ROOT/data/MusicSFx_ug2_mw_ug1.mpf" "$OUT/2_ug1" > "$OUT/2_ug1/stdout.txt"; tail -1 "$OUT/2_ug1/build_log.txt"
sh "$T/check_decode_ug1.sh" "$OUT/bin/streamdec" "$OUT/2_ug1" "$OUT/2_ug1/decode_check.txt"
echo "== step 3: Carbon's 30 songs"
mkdir -p "$OUT/3_carbon"
$PY "$T/add_carbon_songs.py" "$OUT/2_ug1/sdat.viv" "$CBMPF" "$CBMUS" "$ROOT/data/MusicSFx.mpf" "$ROOT/data/carbon_streams_sha256.txt" "$OUT/3_carbon" > "$OUT/3_carbon/stdout.txt"; tail -1 "$OUT/3_carbon/build_log.txt"
sh "$T/check_decode_carbon.sh" "$OUT/bin/streamdec" "$OUT/3_carbon" "$ROOT/data/carbon_pcm_sha256.txt" "$OUT/3_carbon/decode_check.txt"
echo "== the map header"
$PY "$T/make_map_header.py" "$OUT/3_carbon/MusicSFx.mpf" "$OUT/2_ug1/MusicSFx.mpf" "$OUT/1_mw/MusicSFx.mpf" "$OUT/1_mw/MusicSFx_original.mpf" "$OUT/musicmap.h" > "$OUT/musicmap_log.txt"
cmp "$OUT/musicmap.h" "$ROOT/src/musicmap.h" || { echo "FAIL: OUT/musicmap.h != src/musicmap.h"; exit 1; }
echo "OUT/musicmap.h = src/musicmap.h"
echo "== the outputs"
for s in 1_mw 2_ug1 3_carbon; do
  for f in sdat.viv MusicSFx.mus MusicSFx.mpf; do echo "$s/$f $(stat -c %s "$OUT/$s/$f") B $(sha256sum "$OUT/$s/$f" | cut -c1-64)"; done
done | tee "$OUT/outputs_sha256.txt"
V=$(sha256sum "$OUT/3_carbon/sdat.viv" | cut -c1-64); S=$(sha256sum "$OUT/3_carbon/MusicSFx.mus" | cut -c1-64); P=$(sha256sum "$OUT/3_carbon/MusicSFx.mpf" | cut -c1-64)
[ "$V" = 91b9fd489ff6566d1bab415d52eaa47c4865583d2db5a399c5cdd399d11638f9 ] && [ "$(stat -c %s "$OUT/3_carbon/sdat.viv")" = 893398656 ] \
  && [ "$S" = d5d5889b23ca062da335e3fd1dc3e0c458c43baffc856ce35fc3fe87d54c78ae ] && [ "$P" = 34cba3025417d8057959990ed3019a6ffb1a3e2bb4ebea11ef045bb30e7ff409 ] \
  || { echo "FAIL: the final archive is not the mod's (sdat.viv $V)"; exit 1; }
echo "BUILD PASS: $OUT/3_carbon/sdat.viv 893,398,656 B SHA-256 $V = the mod's archive ($(( $(date +%s) - start )) s)"
