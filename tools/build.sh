#!/bin/sh
# build.sh [build folder] - builds UG2Music.asi + UG2Music.map from src/ in a build folder outside the project (default ~/golden_era_scratch/build).
# Toolchain: i686-w64-mingw32-gcc 13 (Ubuntu 24.04's mingw-w64), -O2, a DLL with static libgcc, unused sections dropped, no exports, a link map;
# linked with -s (no symbol table) and --no-insert-timestamp (the header's time stamp = 0): the file holds nothing of the build's moment or its names.
# A -Wall -Wextra pass runs first and must print nothing (Log() carries the printf format attribute, so a wrong log argument is caught here).
# The output is linked as UG2Music.asi in the build folder's root: the linker takes the DLL's image base from the output name.
# Afterwards: copy the asi + map into the project's record of that build with their SHA-256 (README.md says where).
set -e
P="$(cd "$(dirname "$0")/.." && pwd)"; O="${1:-$HOME/golden_era_scratch/build}"
case "$O" in "$P"*) echo "ABORT: the build folder must be outside the project folder"; exit 1;; esac
mkdir -p "$O/src" "$O/obj"; cp "$P"/src/* "$O/src/"; cd "$O"
CC=i686-w64-mingw32-gcc; OD=i686-w64-mingw32-objdump
$CC -Wall -Wextra -O2 -Isrc -c src/ug2music.c -o obj/ug2music_wall.o 2> wall.txt || { cat wall.txt; exit 1; }
[ ! -s wall.txt ] || { cat wall.txt; echo "ABORT: compiler warnings (wall.txt)"; exit 1; }
echo "-Wall -Wextra: 0 lines"
$CC -O2 -Isrc -c src/ug2music.c -o obj/ug2music.o
$CC -shared -o UG2Music.asi obj/ug2music.o -static-libgcc -Wl,--gc-sections -Wl,--exclude-all-symbols -Wl,-Map,UG2Music.map -s -Wl,--no-insert-timestamp
$OD -p UG2Music.asi | grep -q 'Entry 0 00000000 00000000 Export Directory' && echo "exports: none (the export directory entry is empty)" || { echo "ABORT: the asi has an export directory"; exit 1; }
ls -la UG2Music.asi UG2Music.map; sha256sum UG2Music.asi UG2Music.map
echo "the version line: $(strings -n 12 UG2Music.asi | grep -m1 '^Golden Era Soundtrax [0-9]' || echo 'NOT FOUND')"
