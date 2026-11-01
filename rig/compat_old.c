// compat_old.c - the old-settings check: the settings-file parser of an older release of this mod (rig/oldsrc/settingsfile.h, hash-checked by tools/rig.sh
// before the build) compiled natively and run on the file this release's rig wrote (test/pc5_largest.txt = header 2 + 82 game lines + 109 ALL lines) = what a
// return to the older release reads: every game line as its shared mode, the ALL lines kept as other lines (64 at most, the rest dropped), the header seen;
// and what the older release writes back (header 1).
// usage: rigbin_compat <file>  (tools/rig.sh builds it as test/rigbin_compat with -Irig/oldsrc); prints PASS / FAIL lines, exit 1 on a FAIL.
#include "settingsfile.h"
#include <stdlib.h>
int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: compat_old <file>\n"); return 2; }
    FILE *f = fopen(argv[1], "rb"); if (!f) { printf("FAIL OLD-0 cannot open %s\n", argv[1]); return 1; }
    static char txt[16384]; int n = (int)fread(txt, 1, sizeof txt - 1, f); fclose(f); txt[n] = 0;
    Set_Settings s; Set_Parse(&s, txt, n);
    int mw = 0, u1 = 0, cb = 0; for (int k = 1; k <= 30; k++) { if (k <= 26 && s.mwMode[k] == 0) mw++; if (k <= 26 && s.u1Mode[k] == 0) u1++; if (s.cbMode[k] == 0) cb++; }
    int allKept = 0; for (int k = 0; k < s.nOther; k++) if (!strncmp(s.other[k], "ALL ", 4)) allKept++;
    static char out[SET_MAXLINES * SET_LINELEN + 256]; int w = Set_Format(&s, out, sizeof out);   // the older release's own buffer size (its ug2music.c SettingsWrite)
    int fails = 0;
#define CHECK(c, ...) do { printf((c) ? "PASS " : "FAIL "); printf(__VA_ARGS__); printf("\n"); if (!(c)) fails++; } while (0)
    CHECK(!strcmp(SET_HEADER, "UG2Music settings 1") && sizeof(Set_Settings) < 8000, "OLD-1 the parser compiled here is the older release's (its header word 'UG2Music settings 1'; no ALL key)");
    CHECK(s.headerSeen && s.soundtrack == SET_ST_CARBON && s.nSong == 82 && mw == 26 && u1 == 26 && cb == 30 && !s.badSong,
          "OLD-2 the older release's parser on %s (%d B): the header seen (it matches the first 17 characters), soundtrack CARBON, %d game lines read as its shared modes (MW %d / UG1 %d / CB %d Off), %d bad", argv[1], n, s.nSong, mw, u1, cb, s.badSong);
    CHECK(s.nOther == 64 && allKept == 64 && s.dropped == 109 - 64,
          "OLD-3 the 109 ALL lines: %d kept as other lines (all of them 'ALL ..'), %d dropped (the older release's 64-line cap; 45 of 109 = the full set) = a revert keeps every game set and All Trax's own set up to 64 lines", s.nOther, s.dropped);
    CHECK(w > 0 && !strncmp(out, "UG2Music settings 1\r\n", 21) && w < (int)sizeof out,
          "OLD-4 the older release's write-back %d B (fits its %d B buffer), header 'UG2Music settings 1' (the revert writes its own header; the 64 kept ALL lines come back as text, read by this release again as All Trax's lines)", w, (int)sizeof out);
    printf("%s (%d failures)\n", fails ? "COMPAT FAIL" : "COMPAT PASS", fails); return fails != 0;
}
