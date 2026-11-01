#!/bin/sh
# rig.sh - the offline rig: the pure parts of the mod (the song table, the picker, the settings file, the list, the save, the buttons, the row
# words) compiled natively and checked against the real game's speed2.exe, the music map files in data/ and the game's font file; then the
# old-settings check (rig/compat_old.c: the parser of an older release reads a settings file written by this one). Two runs must be equal.
# It copies src/ rig/ data/ into a run folder outside the project (default ~/golden_era_scratch/rig) and runs there; reads the game folder only.
# usage: tools/rig.sh [game folder] [run folder]
set -e
P="$(cd "$(dirname "$0")/.." && pwd)"; G="${1:-/mnt/c/NFSU2 Music Mod}"; O="${2:-$HOME/golden_era_scratch/rig}"
case "$O" in "$P"*) echo "ABORT: the run folder must be outside the project folder"; exit 1;; esac
mkdir -p "$O/test"; for d in src rig data; do mkdir -p "$O/$d"; cp -r "$P/$d/." "$O/$d/"; done; cd "$O"
OLD_SHA=$(grep ' rig/oldsrc/settingsfile.h$' rig/oldsrc/SHA256SUMS | cut -c1-64)
[ "$(sha256sum rig/oldsrc/settingsfile.h | cut -c1-64)" = "$OLD_SHA" ] || { echo "ABORT: rig/oldsrc/settingsfile.h is not the recorded older release"; exit 1; }
echo "font file: $G/GLOBAL/GLOBALA.BUN $(stat -c %s "$G/GLOBAL/GLOBALA.BUN") B $(sha256sum "$G/GLOBAL/GLOBALA.BUN" | cut -c1-16).."
gcc -O2 -Wall -Wextra -o test/rigbin_rig rig/rig.c 2> test/rig_wall.txt || { cat test/rig_wall.txt; exit 1; }
echo "rig -Wall -Wextra: $(wc -l < test/rig_wall.txt) lines"
for k in 1 2; do
  ./test/rigbin_rig "$G/speed2.exe" data/MusicSFx.mpf data/MusicSFx_ug2_original.mpf data/MusicSFx_one_mw_song.mpf data/mw_counts.txt data/ug2_counts.txt \
    data/MusicSFx_ug2_mw.mpf data/MusicSFx_ug2_mw_ug1.mpf "$G/GLOBAL/GLOBALA.BUN" > test/rig_out$k.txt || true
done
cmp test/rig_out1.txt test/rig_out2.txt && echo "two runs equal"
echo "PASS $(grep -c '^PASS' test/rig_out1.txt), FAIL $(grep -c '^FAIL' test/rig_out1.txt); $(tail -1 test/rig_out1.txt)"
grep '^FAIL' test/rig_out1.txt || true
# -Wno-unused-function / -Wno-unused-variable: the older header defines static functions + one static variable this check never uses
gcc -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Irig/oldsrc -o test/rigbin_compat rig/compat_old.c 2> test/compat_wall.txt || { cat test/compat_wall.txt; exit 1; }
echo "compat -Wall -Wextra: $(wc -l < test/compat_wall.txt) lines"
./test/rigbin_compat test/pc5_largest.txt | tee test/compat_out.txt
