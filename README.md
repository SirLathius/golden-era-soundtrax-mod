# Golden Era Soundtrax Mod

Adds the soundtracks of NFS Underground, Most Wanted and Carbon to Underground 2, extends the EA Trax interface with new songs and features.

Designed, checked and tested by Lathius; the code and documentation were written using an AI assistant.

 https://mega.nz/file/lNUE1IRL#RydHxImeB12CkEIwIILS8wPW0CWWRNIjKcBVCJ4edvE

## Files

    src/            the mod's source (C): ug2music.c and its headers; the game's addresses it uses are listed in SITES.md
    rig/            the offline test rig: the mod's pure parts compiled natively and checked against the retail speed2.exe
    checks/         check_log.py: reads a play session's UG2Music.log and checks it
    data/           the music map files the rig and the archive build read
    tools/          build.sh (the asi), rig.sh (the rig), pe_equal.py / pe_sections.py (build comparisons), archive/ (the song-archive build)
    SITES.md        every address in speed2.exe the mod reads or writes, and how

## Build

Needs WSL Ubuntu 24.04 (or any Linux) with `gcc-mingw-w64-i686` (i686-w64-mingw32-gcc 13).

    tools/build.sh                     # -> ~/golden_era_scratch/build/UG2Music.asi
    tools/rig.sh <game folder>         # the offline rig against the retail speed2.exe: every line PASS, two runs equal

## License

MIT, see LICENSE. The mod ships the Ultimate ASI Loader by ThirteenAG (MIT).
