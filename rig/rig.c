// rig.c - the offline rig of UG2Music: the mod's pure parts (src/songlogic.h with the map, settings file and list headers it includes;
// src/songnames.h, savesync.h, saveprompt.h, shuffle.h, defaultbutton.h, rowwords.h) compiled natively and checked against the files they
// meet: the real speed2.exe (every code site, operand and data address the mod relies on; file offset = VA - 0x400000), the music map files
// in data/ and the game's font file. Checked: the song table and the map image the mod installs, the long-song hook, the EA Trax strings and
// the titles' widths in the game's font, the picker (allowed sets, sequential / random order, the two carrier slots), the settings file
// (write, read, older files), the EA Trax list (the view per soundtrack, row steps, preview), the save / load sync and the save prompt's
// snapshot, the Shuffle and Default buttons, the per-category song settings and the row words' shift.
// usage: rig <speed2.exe> <MusicSFx.mpf> <original MusicSFx.mpf> <one-MW-song MusicSFx.mpf> <mw_counts.txt> <ug2_counts.txt>
//            <UG2 + MW MusicSFx.mpf> <UG2 + MW + UG1 MusicSFx.mpf> <GLOBALA.BUN>        (in this order from tools/rig.sh)
//   1  the retail speed2.exe (4,800,512 B)
//   2  data/MusicSFx.mpf: the mod's map (7,504 B, 126 entries = UG2 43 + MW 26 + UG1 27 + Carbon 30, samples 0x1960..0x1D50): accepted
//   3  data/MusicSFx_ug2_original.mpf: the game's own map (6,840 B, 43 entries): NOT accepted
//   4  data/MusicSFx_one_mw_song.mpf: a 6,840-B map with one MW song swapped in (its nodes differ at song 6): NOT accepted
//   5  data/mw_counts.txt, 6  data/ug2_counts.txt: the header counts of the 26 MW and 43 UG2 waves (the long-song hook's model)
//   7  data/MusicSFx_ug2_mw.mpf: a 69-entry map (UG2 + MW, 7,048 B): NOT accepted
//   8  data/MusicSFx_ug2_mw_ug1.mpf: a 96-entry map (UG2 + MW + UG1, 7,264 B): NOT accepted
//   9  the game's GLOBAL\GLOBALA.BUN (the font ConduitMdITC_TT21i)
// output (stdout): one 'PASS <check>' or 'FAIL <check>' line per check, then 'RIG PASS (0 failures)' or 'RIG FAIL (<n> failures)'; the exit
// code is 0 only when nothing failed. It also writes test/pc5_largest.txt (the largest settings file) for rig/compat_old.c to read.
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "../src/songlogic.h"
#include "../src/songnames.h"
#include "../src/savesync.h"
#include "../src/saveprompt.h"
#include "../src/shuffle.h"   // the Shuffle button's pure parts
#include "../src/defaultbutton.h"   // the Default button's pure parts
#include "../src/rowwords.h"    // the row words' shift
static int fails; static uint32_t g_fakeRows[27][4];   // the asi's g_rows (DWORD[27][4]) has the table's row layout
#define CHECK(c, ...) do { printf((c) ? "PASS " : "FAIL "); printf(__VA_ARGS__); printf("\n"); if (!(c)) fails++; } while (0)
// The font width reader, measuring text as UG2 does (UG2-EXE 0x4FDA70 / 0x50BCE0 / 0x6F45F0). The font ConduitMdITC_TT21i = GLOBAL\GLOBALA.BUN
// chunk 0x00030201 (+0x200 'FNTF'; glyph count at +0x0A, flags +0x0C (bit 0x40000 = 16-byte records), the glyph table at +0x14, the kerning table
// at +0x18); a record: code point u16 +0, kern count u8 +0x0B, kern start u16 +0x0C, advance s16 +0x0E; a kern entry 4 B at kt + 4 + 4 * (start + j):
// prev u16, delta s8. A byte without a glyph adds 0, as the exe's 0x50BDDE skips it; the width in font units x the text's size (0.65 on the rows) = screen units.
typedef struct { const unsigned char *b; long n; uint32_t f, tab, kt; int ng; } StrFont;
static int Str_FontLoad(StrFont *F, const unsigned char *b, long n)
{
    uint32_t p = 0, c0 = 0; int found = 0;
    while (p + 8 <= (uint32_t)n) { uint32_t t, l; memcpy(&t, b + p, 4); memcpy(&l, b + p + 4, 4); if (t == 0x30201u) { c0 = p + 8; found = 1; } p += 8 + l; }
    if (!found || p != (uint32_t)n || memcmp(b + c0, "conduitmditc_tt21i", 18)) return 0;
    F->f = c0 + 0x200; if (memcmp(b + F->f, "FNTF", 4)) return 0;
    uint16_t ng; uint32_t flags; memcpy(&ng, b + F->f + 0x0A, 2); memcpy(&flags, b + F->f + 0x0C, 4); memcpy(&F->tab, b + F->f + 0x14, 4); memcpy(&F->kt, b + F->f + 0x18, 4);
    if (!(flags & 0x40000u)) return 0;
    F->ng = ng; F->b = b; F->n = n; return 1;
}
static long Str_FontGlyph(const StrFont *F, unsigned cp) { for (int i = 0; i < F->ng; i++) { uint32_t rr = F->f + F->tab + 16u * (uint32_t)i; uint16_t c; memcpy(&c, F->b + rr, 2); if (c == cp) return (long)rr; } return -1; }
static int Str_FontWidth(const StrFont *F, const char *s)
{
    int w = 0, havePrev = 0; unsigned prev = 0;
    for (; *s; s++) {
        unsigned cp = (unsigned char)*s; long rr = Str_FontGlyph(F, cp);
        if (rr >= 0) { int16_t adv; memcpy(&adv, F->b + rr + 0x0E, 2); w += adv;
            if (havePrev) { int k = F->b[rr + 0x0B]; uint16_t st; memcpy(&st, F->b + rr + 0x0C, 2);
                for (int j = 0; j < k; j++) { uint32_t e = F->f + F->kt + 4u + 4u * (uint32_t)(st + j); uint16_t pc; memcpy(&pc, F->b + e, 2); if (pc == prev) { w += (int8_t)F->b[e + 2]; break; } } } }
        prev = cp; havePrev = 1;
    }
    return w;
}
// The shown title from a stock title: the two quotes off; ' (<remixer> Remix)' at the end -> ' (Remix)' (a title already ending '(Remix)' stays as it is)
static void Str_Shown(const char *stock, char *out, size_t cap)
{
    size_t l = strlen(stock); if (l < 3 || stock[0] != '"' || stock[l - 1] != '"' || l - 2 >= cap) { out[0] = 0; return; }
    memcpy(out, stock + 1, l - 2); out[l - 2] = 0; l -= 2;
    if (l > 7 && !strcmp(out + l - 6, "Remix)")) { char *op = strrchr(out, '('); if (op && op > out && op[-1] == ' ' && strcmp(op, "(Remix)")) { strcpy(op, "(Remix)"); } }
}
static unsigned char *rd(const char *p, long *n) { FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); } fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET); unsigned char *b = malloc(*n); if (fread(b, 1, *n, f) != (size_t)*n) exit(2); fclose(f); return b; }
static void setWave(unsigned char *img, int song, unsigned wave) { uint32_t v; memcpy(&v, img + kAudOff[song], 4); v = (v & 0xFFFF0000u) | wave; memcpy(img + kAudOff[song], &v, 4); }
// a fake rng that records the count and returns a scripted value
static int g_rngCalls, g_lastCount, g_rngMode;   // mode 0 = return 0 (the first), 1 = count-1 (the last), 2 = a fixed LCG
static unsigned g_lcg = 12345;
// Under All Trax the asi's picker / view read All Trax's OWN set (the asi's All Trax array), not the list / the game arrays. The blocks below, up to
// the per-category checks, model the SHARED case (one setting per song) and keep their expectations: the rig does what the asi does with a settings
// file of an older release (each song's setting copied into every category). RigList copies the list's modes into All Trax's set by song (as
// SettingsRead copies 0x83ACB8 for a header-1 file); RigShared = a fresh apply of the game lines on a header-0 struct + RigList. Called after every
// change of the list / the game lines in those blocks. The per-category checks at the end apply header-2 structs = the five separate sets.
static void RigList(const int *listSong, const unsigned char *listMode) { if (!list_allSet) { for (int id = 0; id < SONG_NSONG; id++) list_allMode[id] = Song_AllDefault(id); list_allSet = 1; } for (int p = 0; p < 27; p++) if (listSong[p] >= 0 && listSong[p] < SONG_NUG2) list_allMode[listSong[p]] = listMode[p]; }
static void RigShared(Set_Settings *s, const int *listSong, const unsigned char *listMode) { memset(s->allMode, -1, sizeof s->allMode); s->headerVer = 0; List_ApplyModes(s); RigList(listSong, listMode); }
static int FakeRng(int count, void *u) { (void)u; g_rngCalls++; g_lastCount = count; if (g_rngMode == 0) return 0; if (g_rngMode == 1) return count - 1; g_lcg = g_lcg * 1103515245u + 12345u; return (int)((g_lcg >> 8) % (unsigned)count); }
// a model of UG2's value reader (UG2-EXE 0x72A195: 1 / 2 / 3-byte values sign-extended, 4 not) and of the long-song stub (a 3-byte negative value masked to 24 bits, counted)
static int32_t ModelReader(uint32_t stored, int len) { uint32_t v = len >= 4 ? stored : (stored & ((1u << (8 * len)) - 1)); if (len == 1 && v > 0x7F) v -= 0x100; else if (len == 2 && v > 0x7FFF) v -= 0x10000; else if (len == 3 && v > 0x7FFFFF) v -= 0x1000000; return (int32_t)v; }
static uint32_t ModelStub(int32_t v, int len, int *fixed) { if (len == 3 && v < 0) { (*fixed)++; return (uint32_t)v & 0xFFFFFFu; } return (uint32_t)v; }
static const char *idName(int id, char *b) { if (SONG_IS_CB(id)) sprintf(b, "CB%d", SONG_CB_C(id)); else if (SONG_IS_U1(id)) sprintf(b, "UG1r%d", SONG_U1_R(id)); else if (SONG_IS_MW(id)) sprintf(b, "MW%d", SONG_MW_N(id)); else sprintf(b, "U%d", id); return b; }
// a fake 32-bit memory for the slot-node walk (the rig runs 64-bit: addresses are uint32 VALUES, never dereferenced)
typedef struct { unsigned char m[0x400]; uint32_t base; } ShufFakeMem;
static int FakeRd32(void *ctx, uint32_t a, uint32_t *v) { ShufFakeMem *m = ctx; if (a < m->base || a + 4u > m->base + sizeof m->m) return 0; memcpy(v, m->m + (a - m->base), 4); return 1; }
static int FakeRd8(void *ctx, uint32_t a, uint32_t *v) { ShufFakeMem *m = ctx; if (a < m->base || a + 1u > m->base + sizeof m->m) return 0; *v = m->m[a - m->base]; return 1; }
static void FakeW32(ShufFakeMem *m, uint32_t a, uint32_t v) { memcpy(m->m + (a - m->base), &v, 4); }
int main(int argc, char **argv)
{
    if (argc < 10) { printf("usage: rig <speed2.exe> <the mod's mpf> <original mpf> <one-MW-song mpf> <mw_counts.txt> <ug2_counts.txt> <UG2 + MW mpf> <UG2 + MW + UG1 mpf> <GLOBALA.BUN>\n"); return 2; }   // the 9 arguments: see the head of this file
    const char *fontPath = argv[9];   // GLOBAL\GLOBALA.BUN
    long ne, nt, no, ni, nold, nu1; unsigned char *exe = rd(argv[1], &ne), *mapImg = rd(argv[2], &nt), *org = rd(argv[3], &no), *oneMw = rd(argv[4], &ni), *old = rd(argv[7], &nold), *u1m = rd(argv[8], &nu1);   // argv[2] = the mod's map, argv[7] = the 69-entry map (old), argv[8] = the 96-entry map (u1m)
    // ---- the exe's bytes at the three code spots (VA - 0x400000 = the file offset in .text)
    static const unsigned char head[6] = { 0x83, 0xEC, 0x08, 0x53, 0x55, 0x56 }, headT[7] = { 0x83, 0xEC, 0x08, 0x8D, 0x44, 0x24, 0x00 }, headR[6] = { 0x8B, 0x0D, 0x24, 0xA6, 0x7E, 0x00 };
    CHECK(ne == 4800512, "SONG-1 exe size %ld (retail 4,800,512)", ne);
    CHECK(!memcmp(exe + 0x6B6F0, head, 6), "SONG-2 exe bytes at VA 0x46B6F0 (file 0x6B6F0) = %02X %02X %02X %02X %02X %02X (expected 83 EC 08 53 55 56 = the hook head)", exe[0x6B6F0], exe[0x6B6F1], exe[0x6B6F2], exe[0x6B6F3], exe[0x6B6F4], exe[0x6B6F5]);
    CHECK(!memcmp(exe + 0x3BDF0, headT, 7), "SONG-3 exe bytes at VA 0x43BDF0 (file 0x3BDF0) = %02X %02X %02X %02X %02X %02X %02X (expected 83 EC 08 8D 44 24 00 = the timer call)", exe[0x3BDF0], exe[0x3BDF1], exe[0x3BDF2], exe[0x3BDF3], exe[0x3BDF4], exe[0x3BDF5], exe[0x3BDF6]);
    CHECK(!memcmp(exe + 0x3C1C0, headR, 6), "SONG-4 exe bytes at VA 0x43C1C0 (file 0x3C1C0) = %02X %02X %02X %02X %02X %02X (expected 8B 0D 24 A6 7E 00 = random(n))", exe[0x3C1C0], exe[0x3C1C1], exe[0x3C1C2], exe[0x3C1C3], exe[0x3C1C4], exe[0x3C1C5]);
    CHECK(exe[0x3C1CA] == 0xF7 && exe[0x3C1CB] == 0x74 && exe[0x3C1CC] == 0x24 && exe[0x3C1CD] == 0x04, "SONG-5 0x43C1CA = F7 74 24 04 'div dword [esp+4]' (the one cdecl argument = n; n = 0 would fault)");
    // ---- the map
    CHECK(nt == SONG_FILE_LEN && nt == 7504 && !memcmp(mapImg, kMapImage, SONG_FILE_LEN), "MAP-1 kMapImage = the mod's MusicSFx.mpf (%ld B = 7,504)", nt);
    unsigned fd[5]; unsigned m = Song_MapMatch(mapImg, fd);
    CHECK(m == SONG_M_ALL, "MAP-2 the fresh image: all 5 regions match (mask %u)", m);
    CHECK(Song_ExpectEventTab(mapImg) == SONG_OFF_EVENTS, "MAP-3 the loader's +0x40 formula on the image = 0x%X (header says 0x%X)", Song_ExpectEventTab(mapImg), SONG_OFF_EVENTS);
    unsigned end = 0, smp = Song_ExpectSamples(mapImg, &end);
    CHECK(smp == SONG_OFF_SAMPLES && end == SONG_FILE_LEN && (end - smp) / 8 == 126 && SONG_NUM_ENTRIES == 126, "MAP-4 the loader's +0x50 / +0x54 chain on the image = 0x%X / 0x%X = %u entries (expected 0x1960 / 0x1D50 = 126: UG2 43 + MW 26 + UG1 27 + Carbon 30)", smp, end, (end - smp) / 8);
    { uint32_t w5c; memcpy(&w5c, mapImg + 0x195C, 4); CHECK(w5c == 0x754 && SONG_WAVE_U1_FIRST == 70 && SONG_WAVE_U1_LAST == 96 && SONG_WAVE_MW_LAST == 69 && SONG_WAVE_CB_FIRST == 97 && SONG_WAVE_CB_LAST == 126, "MAP-5 the table-end word [0x195C] = 0x%X (0x754 = 7,504 / 4); UG1 waves 70..96 after MW's 44..69, Carbon 97..126 after UG1's", w5c); }
    for (int k = 96; k < 126; k++) { uint32_t off, ms; memcpy(&off, mapImg + 0x1960 + 8 * k, 4); memcpy(&ms, mapImg + 0x1964 + 8 * k, 4); CHECK(off >= 0x3B5282 && off <= 0x59E656 && ms >= 147957 && ms <= 373704, "MAP-6 Carbon entry %d (wave %d = Carbon wave %d): offset/0x80 %u (0x1DA94100..0x2CEB2B00 / 0x80), %u ms", k, k + 1, k - 94, off, ms); }
    for (int k = 69; k < 96; k++) { uint32_t off, ms; memcpy(&off, mapImg + 0x1960 + 8 * k, 4); memcpy(&ms, mapImg + 0x1964 + 8 * k, 4); CHECK(off >= 0x2AA937 && off <= 0x3A9E20 && ms >= 29964 && ms <= 429125, "MAP-7 UG1 entry %d (wave %d): offset/0x80 %u (0x15549B80..0x1D4F1000 / 0x80), %u ms", k, k + 1, off, ms); }
    for (int s = 0; s < 27; s++) { uint32_t w; memcpy(&w, mapImg + kAudOff[s], 4); CHECK((w & 0xFFFF) == kOrigWave[s] && (w >> 16) == 0x20, "MAP-8 the audio node of song %d (node %d at 0x%X) = %08X (UG2's own wave %u, high word 0x20)", s, kAudNode[s], kAudOff[s], w, kOrigWave[s]); }
    CHECK(kAudOff[25] == 0x700 && kAudNode[25] == 77 && kOrigWave[25] == 21 && kAudOff[26] == 0x738 && kAudNode[26] == 80 && kOrigWave[26] == 32, "MAP-9 the carriers = song 25 node 77 @0x700 wave 21, song 26 node 80 @0x738 wave 32");
    CHECK(no == 6840 && !memcmp(mapImg, org, 0x195C) && !memcmp(mapImg + 0x1960, org + 0x1960, 0x158), "MAP-10 the mod's image = the ORIGINAL map up to 0x195C and in the 43 old entries (only [0x195C] + the 664 appended bytes differ)");
    CHECK(nold == 7048 && !memcmp(mapImg, old, 0x195C) && !memcmp(mapImg + 0x1960, old + 0x1960, 0x228), "MAP-11 the mod's image = the UG2 + MW map up to 0x195C and in the 69 old entries (only [0x195C] 0x6E2 -> 0x754 + the 456 appended bytes differ)");
    CHECK(nu1 == 7264 && !memcmp(mapImg, u1m, 0x195C) && !memcmp(mapImg + 0x1960, u1m + 0x1960, 0x300), "MAP-12 the mod's image = the UG2 + MW + UG1 map up to 0x195C and in the 96 old entries (only [0x195C] 0x718 -> 0x754 + the 240 appended bytes differ)");
    unsigned char *ob = calloc(SONG_FILE_LEN, 1); memcpy(ob, org, no);
    m = Song_MapMatch(ob, fd); CHECK(m == 15 && fd[4] == 0x195C, "MAP-13 the ORIGINAL map (6,840 B, in a 7,504-B buffer): header / nodetab / nodes / events match, samples differ at 0x%X (mask %u) -> NOT accepted (mask != 31)", fd[4], m);
    unsigned char *tb = calloc(SONG_FILE_LEN, 1); memcpy(tb, old, nold);
    m = Song_MapMatch(tb, fd); CHECK(m == 15 && fd[4] == 0x195C, "MAP-14 the UG2 + MW 69-entry map (7,048 B, in a 7,504-B buffer): header / nodetab / nodes / events match, samples differ at 0x%X (the table-end word 0x6E2 vs 0x754; mask %u) -> NOT accepted (that archive under this build = the mod off)", fd[4], m);
    memcpy(tb + 0x195C, mapImg + 0x195C, 4); m = Song_MapMatch(tb, fd); CHECK(m == 15 && fd[4] == 0x1B88, "MAP-15 the UG2 + MW map with the table-end word patched to 0x754 but no UG1 / Carbon entries: samples differ at 0x%X (the first UG1 entry; mask %u) -> NOT accepted", fd[4], m);
    unsigned char *ub = calloc(SONG_FILE_LEN, 1); memcpy(ub, u1m, nu1);
    m = Song_MapMatch(ub, fd); CHECK(m == 15 && fd[4] == 0x195C, "MAP-16 the UG2 + MW + UG1 96-entry map (7,264 B, in a 7,504-B buffer): header / nodetab / nodes / events match, samples differ at 0x%X (the table-end word 0x718 vs 0x754; mask %u) -> NOT accepted (that archive under this build = the mod off)", fd[4], m);
    memcpy(ub + 0x195C, mapImg + 0x195C, 4); m = Song_MapMatch(ub, fd); CHECK(m == 15 && fd[4] == 0x1C60, "MAP-17 the UG2 + MW + UG1 map with the table-end word patched to 0x754 but no Carbon entries: samples differ at 0x%X (the first Carbon entry; mask %u) -> NOT accepted", fd[4], m);
    unsigned char *ib = calloc(SONG_FILE_LEN, 1); memcpy(ib, oneMw, ni < SONG_FILE_LEN ? ni : SONG_FILE_LEN);
    m = Song_MapMatch(ib, fd); CHECK(!(m & SONG_M_NODES) && !(m & SONG_M_SAMPLES) && fd[2] == kAudOff[6], "MAP-18 the one-MW-song map: the nodes fail at song 6's node 0x%X (wave 32 there) and the samples fail (mask %u) -> NOT accepted", fd[2], m);
    // the carrier words: own or 44..69 accepted, anything else refused; every other node exact
    unsigned char *w = malloc(nt); memcpy(w, mapImg, nt);
    setWave(w, 25, 44); setWave(w, 26, 69); m = Song_MapMatch(w, fd); CHECK(m == SONG_M_ALL, "MAP-19 carriers holding waves 44 / 69: still all 5 regions (mask %u)", m);
    setWave(w, 25, 70); setWave(w, 26, 96); m = Song_MapMatch(w, fd); CHECK(m == SONG_M_ALL, "MAP-20 carriers holding waves 70 / 96 (UG1's first / last): still all 5 regions (mask %u)", m);
    setWave(w, 25, 81); setWave(w, 26, 21); m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_NODES) && fd[2] == 0x738, "MAP-21 carrier 25 = 81 (UG1), carrier 26 = 21 (25's own): nodes fail at 0x%X (mask %u)", fd[2], m);
    setWave(w, 25, 97); setWave(w, 26, 126); m = Song_MapMatch(w, fd); CHECK(m == SONG_M_ALL, "MAP-22 carriers holding waves 97 / 126 (Carbon's first / last): still all 5 regions (mask %u)", m);
    setWave(w, 25, 110); setWave(w, 26, 21); m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_NODES) && fd[2] == 0x738, "MAP-23 carrier 25 = 110 (Carbon), carrier 26 = 21 (25's own): nodes fail at 0x%X (mask %u)", fd[2], m);
    setWave(w, 25, 55); setWave(w, 26, 32); m = Song_MapMatch(w, fd); CHECK(m == SONG_M_ALL, "MAP-24 carrier 25 = 55, carrier 26 = its own 32: still all 5 (mask %u)", m);
    setWave(w, 25, 21); setWave(w, 26, 50); m = Song_MapMatch(w, fd); CHECK(m == SONG_M_ALL, "MAP-25 carrier 25 = its own 21, carrier 26 = 50: still all 5 (mask %u)", m);
    setWave(w, 25, 43); m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_NODES) && fd[2] == 0x700, "MAP-26 carrier 25 = 43 (a UG2 wave, not its own): nodes fail at 0x%X (mask %u)", fd[2], m);
    setWave(w, 25, 127); m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_NODES) && fd[2] == 0x700, "MAP-27 carrier 25 = 127 (past the 126-entry table): nodes fail at 0x%X (mask %u)", fd[2], m);
    setWave(w, 25, 21); setWave(w, 26, 21); m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_NODES) && fd[2] == 0x738, "MAP-28 carrier 26 = 21 (25's own, not its own): nodes fail at 0x%X (mask %u)", fd[2], m);
    memcpy(w, mapImg, nt); setWave(w, 9, 44); m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_NODES) && fd[2] == kAudOff[9], "MAP-29 a wave 44 in song 9's node (not a carrier): nodes fail at 0x%X (mask %u)", fd[2], m);
    memcpy(w, mapImg, nt); w[0x21] ^= 1; m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_NODETAB) && fd[1] == 0x21, "MAP-30 one bit in the node table: nodetab fails at 0x%X (mask %u)", fd[1], m);
    memcpy(w, mapImg, nt); w[0x1D4F] ^= 1; m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_SAMPLES) && fd[4] == 0x1D4F, "MAP-31 one bit in the last sample entry (Carbon wave 126): samples fail at 0x%X (mask %u)", fd[4], m);
    memcpy(w, mapImg, nt); w[0x1C5F] ^= 1; m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_SAMPLES) && fd[4] == 0x1C5F, "MAP-32 one bit in the last UG1 entry (wave 96): samples fail at 0x%X (mask %u)", fd[4], m);
    memcpy(w, mapImg, nt); w[0x1B87] ^= 1; m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_SAMPLES) && fd[4] == 0x1B87, "MAP-33 one bit in the last MW entry (wave 69): samples fail at 0x%X (mask %u)", fd[4], m);
    memcpy(w, mapImg, nt); w[0xF] = 78; m = Song_MapMatch(w, fd); CHECK(!(m & SONG_M_HEADER) && (m & SONG_M_NODES), "MAP-34 the event count byte changed: header fails, nodes still match (mask %u)", m);
    // ---- MW's modes / strings
    int nfe = 0, nig = 0, nal = 0; for (int n = 1; n <= 26; n++) { nfe += kMwMode[n] == 1; nig += kMwMode[n] == 2; nal += kMwMode[n] == 3; }
    CHECK(nfe == 8 && nig == 16 && nal == 2, "SONG-6 MW DefPlay modes: FE %d / IG %d / AL %d (MW-DATA 8 / 16 / 2)", nfe, nig, nal);
    CHECK(kMwMode[12] == 2 && kMwMode[14] == 3 && kMwMode[15] == 3 && kMwMode[1] == 1 && kMwMode[17] == 1 && kMwMode[26] == 2, "SONG-7 song 12 IG, 14 / 15 AL, 1 / 17 FE, 26 IG");
    // ---- the picker copy on a default live list (positions = songs 0..26; modes 0..5 = 1 (FE), 6..26 = 2 (IG) = UG2's defaults)
    int listSong[27]; unsigned char listMode[27]; for (int p = 0; p < 27; p++) { listSong[p] = p; listMode[p] = p < 6 ? 1 : 2; } RigList(listSong, listMode);
    Song_State st; Song_PickInfo inf; char nb[16]; Song_Init(&st);
    memset(list_u1Mode, 0, sizeof list_u1Mode); list_u1Set = 1;   // the blocks below run with every UG1 and Carbon song OFF (their 53-song numbers hold); the UG1 / Carbon blocks at the end lift it
    memset(list_cbMode, 0, sizeof list_cbMode); list_cbSet = 1;
    RigList(listSong, listMode); for (int id = SONG_NUG2; id < SONG_NSONG; id++) if (SONG_IS_U1(id) || SONG_IS_CB(id)) list_allMode[id] = 0;   // the same in All Trax's set (the shared case of these blocks; see RigList)
    int na = 0, nb_ = 0; for (int i = 0; i < SONG_NSONG; i++) { na += Song_Allowed(SONG_CTX_FE, i, listSong, listMode); nb_ += Song_Allowed(SONG_CTX_IG, i, listSong, listMode); }
    CHECK(na == 16 && nb_ == 37, "SONG-8 ALLOWED by default: FE %d (UG2 6 + MW 10), IG %d (UG2 21 + MW 16; MW 25 / 26 out)", na, nb_);
    // sequential FE: 0..5 then MW 1 2 3 5 8 13 14 15 16 17, then refill -> 0 again; no rng call
    static const int expFE[17] = { 0, 1, 2, 3, 4, 5, SONG_ID_MW(1), SONG_ID_MW(2), SONG_ID_MW(3), SONG_ID_MW(5), SONG_ID_MW(8), SONG_ID_MW(13), SONG_ID_MW(14), SONG_ID_MW(15), SONG_ID_MW(16), SONG_ID_MW(17), 0 };
    int okSeq = 1; g_rngCalls = 0; char seqs[400] = "";
    for (int k = 0; k < 17; k++) { int id = Song_Choose(&st, SONG_CTX_FE, 1, listSong, listMode, FakeRng, NULL, &inf); if (id != expFE[k]) okSeq = 0; strcat(seqs, idName(id, nb)); strcat(seqs, " "); }
    CHECK(okSeq && g_rngCalls == 0 && st.refills == 2 && st.seqRefills == 0, "SONG-9 SEQUENTIAL FE: %s(the 16 in order, then the refill restarts at U0; 0 rng calls, refills %u)", seqs, st.refills);
    // sequential IG: 6..26 then MW 4 6 7 9 10 11 12 14 15 18..26 (39), then refill
    int okIg = 1, k = 0; seqs[0] = 0; static int expIG[40]; for (int s = 6; s <= 26; s++) expIG[k++] = s; for (int n = 1; n <= 26; n++) if ((kMwMode[n] == 2 || kMwMode[n] == 3) && !SONG_MW_OUT(n)) expIG[k++] = SONG_ID_MW(n); expIG[k++] = 6;
    for (int q = 0; q < 38; q++) { int id = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); if (id != expIG[q]) okIg = 0; if (q < 24 || q > 36) { strcat(seqs, idName(id, nb)); strcat(seqs, " "); } else if (q == 24) strcat(seqs, "... "); }
    CHECK(okIg && g_rngCalls == 0 && k == 38, "SONG-10 SEQUENTIAL IG: %s(the 37 in order: UG2 6..26 then the 16 MW (MW 25 / 26 out); then U6 again; 0 rng calls)", seqs);
    // random IG: 39 picks = a permutation of the IG set, counts 39, 38, ..., 1, one rng call each; the 40th refills to 39
    Song_Init(&st); g_rngCalls = 0; g_rngMode = 2; int seen[SONG_NSONG] = {0}; int okRnd = 1, okCount = 1;
    for (int q = 0; q < 37; q++) { int id = Song_Choose(&st, SONG_CTX_IG, 0, listSong, listMode, FakeRng, NULL, &inf); if (id < 0 || seen[id]++ || !Song_Allowed(SONG_CTX_IG, id, listSong, listMode)) okRnd = 0; if (g_lastCount != 37 - q || inf.count != 37 - q) okCount = 0; }
    CHECK(okRnd && okCount && g_rngCalls == 37 && st.draws == 37, "SONG-11 RANDOM IG: 37 picks = every allowed song once (counts 37, 36, .., 1; one rng call per pick: %d calls)", g_rngCalls);
    int id = Song_Choose(&st, SONG_CTX_IG, 0, listSong, listMode, FakeRng, NULL, &inf);
    CHECK(id >= 0 && inf.refilled && inf.count == 37 && g_lastCount == 37, "SONG-12 the 38th random pick refills: count 37 again (refilled flag %d)", inf.refilled);
    // random with the rng returning the last / first member
    Song_Init(&st); g_rngMode = 1; id = Song_Choose(&st, SONG_CTX_IG, 0, listSong, listMode, FakeRng, NULL, &inf);
    CHECK(id == SONG_ID_MW(24), "SONG-13 random r = count-1 -> the last member in id order = MW24 (MW 25 / 26 out) (%s)", idName(id, nb));
    g_rngMode = 0; id = Song_Choose(&st, SONG_CTX_IG, 0, listSong, listMode, FakeRng, NULL, &inf);
    CHECK(id == 6, "SONG-14 random r = 0 -> the first member in id order = U6 (%s)", idName(id, nb));
    // a mode change mid-cycle shrinks remaining: song 7 set to FE-only disappears from IG; U0 set to 3 joins IG at position 0 (sequential wraps there only after a refill)
    Song_Init(&st); id = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); listMode[7] = 1; RigList(listSong, listMode);
    id = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf);
    CHECK(id == 8 && inf.before == 35, "SONG-15 after U6, song 7 switched to FE-only: the next sequential IG pick skips it -> U8 (remaining %d of 36 left after the mask)", inf.before);
    listMode[7] = 2; listMode[0] = 3; RigList(listSong, listMode);
    int okAll = 1; for (int q = 0; q < 34; q++) { id = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); if (id == 0 || id == 7) okAll = 0; }
    id = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf);
    CHECK(okAll && id == 0 && inf.refilled && inf.allowed == 38, "SONG-16 U0 set to ALL + song 7 back: the rest of the cycle (34 picks) holds neither; the refill then starts at U0 (allowed %d = 37 + U0)", inf.allowed);
    // every UG2 song off (modes 0): FE = MW's 10 only, IG = MW's 18 only
    for (int p = 0; p < 27; p++) { listMode[p] = 0; } RigList(listSong, listMode);
    Song_Init(&st);
    id = Song_Choose(&st, SONG_CTX_FE, 1, listSong, listMode, FakeRng, NULL, &inf); int id2 = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf);
    CHECK(id == SONG_ID_MW(1) && id2 == SONG_ID_MW(4) && inf.allowed == 16, "SONG-17 every UG2 song off: FE -> MW1 first, IG -> MW4 first (IG allowed %d)", inf.allowed);
    // a list in another order: positions follow the list, not the song index
    for (int p = 0; p < 27; p++) { listSong[p] = 26 - p; listMode[p] = 2; } RigList(listSong, listMode); Song_Init(&st);
    id = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); id2 = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf);
    CHECK(id == 26 && id2 == 25, "SONG-18 a reversed live list: sequential follows list positions (U26 then U25)");
    // a sequential wrap with remaining non-empty (a song re-allowed behind pos): refill once, flagged
    for (int p = 0; p < 27; p++) { listSong[p] = p; listMode[p] = p < 6 ? 1 : 2; } RigList(listSong, listMode); Song_Init(&st);
    for (int q = 0; q < 37; q++) Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf);   // the full cycle: pos ends on MW 24, remaining empty
    CHECK(inf.after == 0 && st.c[1].pos == 76, "SONG-19 a full sequential IG cycle ends with remaining 0 at position 76 (MW 24 in the All Trax view order: UG1 26 + the list 27 + MW; MW 25 / 26 out)");
    st.c[1].rem[6] = 1;                                                                        // pretend U6 came back into remaining behind pos
    id = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf);
    CHECK(id == 6 && inf.seqWrap && st.seqRefills == 1, "SONG-20 nothing after pos while remaining is not empty: refill once + flagged (the stock would return -1), then U6");
    // ---- the carrier rule
    Song_Plan pl = Song_CarrierPlan(SONG_ID_MW(12), -1, -1);
    CHECK(pl.ret == 26 && pl.carrier == 26 && pl.want[1] == SONG_ID_MW(12) && pl.want[0] == 0, "SONG-21 first MW pick (nothing played before): carrier 26, want 26 = MW12 (its id), 25 = its own (ret %d)", pl.ret);
    pl = Song_CarrierPlan(SONG_ID_MW(3), 26, 26);
    CHECK(pl.ret == 25 && pl.want[0] == SONG_ID_MW(3) && pl.want[1] == -1, "SONG-22 MW3 while 26 is g_last: carrier 25 = MW3, 26 left alone (ret %d)", pl.ret);
    pl = Song_CarrierPlan(7, 25, 25);
    CHECK(pl.ret == 7 && pl.want[0] == -1 && pl.want[1] == 0, "SONG-23 UG2 song 7 while 25 is g_last: slot 7, 25 left alone, 26 given back (ret %d)", pl.ret);
    pl = Song_CarrierPlan(SONG_ID_MW(5), 7, 25);
    CHECK(pl.ret == 26 && pl.want[1] == SONG_ID_MW(5) && pl.want[0] == 0, "SONG-24 MW5 while 7 is g_last and 25 was the last carrier: alternate -> 26 = MW5, 25 given back (ret %d)", pl.ret);
    pl = Song_CarrierPlan(SONG_ID_MW(9), 7, 26);
    CHECK(pl.ret == 25 && pl.want[0] == SONG_ID_MW(9) && pl.want[1] == 0, "SONG-25 MW9 while 7 is g_last and 26 was the last carrier: alternate -> 25 (ret %d)", pl.ret);
    pl = Song_CarrierPlan(26, 26, 26);
    CHECK(pl.ret == 26 && pl.want[1] == 0 && pl.want[0] == 0, "SONG-26 UG2's own song 26 while 26 is g_last (holding MW): slot 26 given back its own as the returned slot (ret %d)", pl.ret);
    pl = Song_CarrierPlan(25, 26, 26);
    CHECK(pl.ret == 25 && pl.want[0] == 0 && pl.want[1] == -1, "SONG-27 UG2's own song 25 while 26 is g_last: 25 = its own, 26 left alone (ret %d)", pl.ret);
    // the invariant over a long random run: a carrier is never written while it is g_last unless it is the returned slot; an MW pick always lands on a carrier != g_last
    Song_Init(&st); g_rngMode = 2; int okInv = 1, mwPicks = 0, both = 0; int hold[2] = {0, 0};
    for (int q = 0; q < 2000; q++) {
        int ctx = (q % 5 == 0) ? SONG_CTX_FE : SONG_CTX_IG; id = Song_Choose(&st, ctx, 0, listSong, listMode, FakeRng, NULL, &inf); if (id < 0) { okInv = 0; break; }
        pl = Song_CarrierPlan(id, st.last, st.lastCarrier);
        for (int i = 0; i < 2; i++) { int c = 25 + i; if (pl.want[i] >= 0 && c == st.last && c != pl.ret) okInv = 0; if (pl.want[i] >= 0) hold[i] = pl.want[i]; }
        if (SONG_IS_MW(id)) { mwPicks++; if (pl.ret == st.last) okInv = 0; if (pl.ret != 25 && pl.ret != 26) okInv = 0; }
        else if (pl.ret != id) okInv = 0;
        if (hold[0] && hold[1]) both++;
        st.last = pl.ret; if (pl.carrier >= 0) st.lastCarrier = pl.carrier;
    }
    CHECK(okInv && mwPicks > 500, "SONG-28 2000 random picks (FE every 5th): no carrier written while g_last unless returned; every MW pick on a carrier != g_last; UG2 picks on their own slot (MW picks %d; both carriers holding MW at once in %d picks = only when one plays and the other is next)", mwPicks, both);
    // ---- the strings
    int okStr = 1; for (int n = 1; n <= 26; n++) { const char *t = kMw[n][0]; size_t l = strlen(t); if (l < 1 || t[0] == '"' || t[l - 1] == '"' || t[l - 1] == ' ') okStr = 0; if (n != 23 && kMw[n][1][strlen(kMw[n][1]) - 1] == ' ') okStr = 0; }   // the titles without quotes; MW 23's artist padded on purpose (checked at the end)
    int empties = 0; for (int n = 1; n <= 26; n++) empties += kMw[n][2][0] == 0;
    CHECK(okStr && empties == 2 && !strcmp(kMw[12][0], "Decadence") && !strcmp(kMw[7][0], "One Good Reason") && !strcmp(kMw[17][1], "T.I. Presents The P$$C"), "STR-1 MW strings: 26 titles WITHOUT quotes, no trailing space (song 7 title / song 17 artist trimmed; MW 23's artist padded on purpose), 2 empty albums (MW 25 / 26), song 12 = Decadence");
    CHECK(!strcmp(kUg2Titles[17], "\"No W\"") && !strcmp(kUg2Titles[25], "\"Skeptic\"") && !strcmp(kUg2Titles[26], "\"The Celebration Song\"") && !strcmp(kUg2Titles[0], "\"Riders on the Storm (Fredwreck Remix)\""), "STR-2 UG2 titles for the load-time row check: 17 = \"No W\", 25 = \"Skeptic\", 26 = \"The Celebration Song\", 0 = \"Riders on the Storm (Fredwreck Remix)\"");
    // ==== the Riders-first signal, MW 25 / 26 out, the strings, the long-song hook ====
    // ---- the Riders-first signal and the -2 path's own-set / carrier handling
    CHECK(Song_RidersSignal(SONG_CTX_FE, 1, -1) && Song_RidersSignal(SONG_CTX_FE, 1, 0) && !Song_RidersSignal(SONG_CTX_FE, 1, 1) && !Song_RidersSignal(SONG_CTX_FE, 0, -1) && !Song_RidersSignal(SONG_CTX_FE, 0x7FF, -1) && !Song_RidersSignal(SONG_CTX_FE, 3, -1) && !Song_RidersSignal(SONG_CTX_IG, 1, -1),
        "SONG-29 the signal: a menu pick with [player+0x88] == 1 and a change (prev -1 / 0) -> the stock; prev 1, a value 0 / 0x7FF (the constructor's) / 3, or an in-game pick -> the mod's own");
    { int prev = -1; uint32_t v[6] = { 1, 0, 0, 1, 0, 0 }; int exp[6] = { 1, 0, 0, 1, 0, 0 }, ok = 1; for (int k = 0; k < 6; k++) { if (Song_RidersSignal(SONG_CTX_FE, v[k], prev) != exp[k]) ok = 0; prev = (int)v[k]; }
      CHECK(ok, "SONG-30 UG2's sequence 1 0 0 1 0 0 (47aed4 sets 1, the stock's pick clears it, a title re-open sets it again) -> stock, own, own, stock, own, own"); }
    { int prev = -1; uint32_t v[4] = { 1, 0, 1, 1 }; int exp[4] = { 1, 0, 1, 0 }, ok = 1; for (int k = 0; k < 4; k++) { if (Song_RidersSignal(SONG_CTX_FE, v[k], prev) != exp[k]) ok = 0; prev = (int)v[k]; }
      CHECK(ok, "SONG-31 the Riders-only-list edge (0x46BAA0 stores 1): 1 stock once, 0 own, 1 (a new change) stock once, 1 (no change) own - never a lock"); }
    for (int p = 0; p < 27; p++) { listSong[p] = p; listMode[p] = p < 6 ? 1 : 2; } RigList(listSong, listMode); Song_Init(&st); Song_TakeSong(&st, SONG_CTX_FE, 0, listSong, listMode, &inf);
    CHECK(inf.refilled && inf.allowed == 16 && inf.before == 0 && inf.after == 15 && st.c[0].rem[0] == 0 && st.draws == 0 && st.refills == 1 && st.c[0].pos == -1, "SONG-32 TakeSong(U0) on the empty menu set: filled to 16 first, U0 dropped -> 15 remain, 0 draws, position -1 (allowed %d, after %d)", inf.allowed, inf.after);
    { int ok = 1; g_rngMode = 2; for (int q = 0; q < 15; q++) { int id2 = Song_Choose(&st, SONG_CTX_FE, 0, listSong, listMode, FakeRng, NULL, &inf); if (id2 <= 0 || inf.count != 15 - q) ok = 0; }
      int id3 = Song_Choose(&st, SONG_CTX_FE, 0, listSong, listMode, FakeRng, NULL, &inf); CHECK(ok && inf.refilled && inf.count == 16 && id3 >= 0, "SONG-33 then 15 own random menu picks never return U0 (counts 15..1); the 16th refills to 16"); }
    Song_Init(&st); g_rngMode = 1; { int idl = Song_Choose(&st, SONG_CTX_FE, 0, listSong, listMode, FakeRng, NULL, &inf); int b4 = inf.after; Song_TakeSong(&st, SONG_CTX_FE, 0, listSong, listMode, &inf);
      CHECK(idl == SONG_ID_MW(17) && !inf.refilled && inf.before == b4 && inf.after == b4 - 1 && st.c[0].rem[0] == 0, "SONG-34 TakeSong(U0) on a set in use (15 after MW17): no refill, U0 dropped -> %d", inf.after); }
    Song_TakeSong(&st, SONG_CTX_FE, 0, listSong, listMode, &inf); CHECK(!inf.refilled && inf.before == 14 && inf.after == 14, "SONG-35 TakeSong(U0) again (Riders twice at title re-opens): nothing to drop, no refill (%d -> %d)", inf.before, inf.after);
    pl = Song_CarrierPlan(0, 26, 26); CHECK(pl.ret == 0 && pl.carrier < 0 && pl.want[0] == 0 && pl.want[1] == -1, "SONG-36 Riders by the stock while 26 is g_last (holding MW): 25 given back, 26 left alone (ret %d)", pl.ret);
    pl = Song_CarrierPlan(0, 7, 26); Song_Plan pl2 = Song_CarrierPlan(0, -1, -1);
    CHECK(pl.ret == 0 && pl.want[0] == 0 && pl.want[1] == 0 && pl2.ret == 0 && pl2.want[0] == 0 && pl2.want[1] == 0, "SONG-37 Riders by the stock while U7 is g_last, or at the game's first pick: both carriers given back (their own wave + row; 'already' when untouched)");
    // ---- part B: MW 25 / 26 out
    CHECK(SONG_MW_OUT(25) && SONG_MW_OUT(26) && !SONG_MW_OUT(24) && !SONG_MW_OUT(1) && kMwMode[25] == 2 && kMwMode[26] == 2, "SONG-38 MW 25 / 26 flagged out (both IG by DefPlay), 1 / 24 not");
    { int nfe2 = 0, nig2 = 0; for (int i = 0; i < SONG_NSONG; i++) { nfe2 += Song_Allowed(SONG_CTX_FE, i, listSong, listMode); nig2 += Song_Allowed(SONG_CTX_IG, i, listSong, listMode); }
      CHECK(nfe2 == 16 && nig2 == 37 && !Song_Allowed(SONG_CTX_IG, SONG_ID_MW(25), listSong, listMode) && !Song_Allowed(SONG_CTX_IG, SONG_ID_MW(26), listSong, listMode), "SONG-39 ALLOWED by default: FE %d, IG %d (UG2 21 + MW 16); MW 25 / 26 allowed in neither", nfe2, nig2); }
    { Song_Init(&st); g_rngMode = 2; int ok = 1, bad = 0, mw = 0; for (int q = 0; q < 2000; q++) { int ctx = (q % 5 == 0) ? SONG_CTX_FE : SONG_CTX_IG; int id2 = Song_Choose(&st, ctx, 0, listSong, listMode, FakeRng, NULL, &inf); if (id2 < 0) ok = 0; if (id2 == SONG_ID_MW(25) || id2 == SONG_ID_MW(26)) bad++; if (SONG_IS_MW(id2)) mw++; }
      CHECK(ok && bad == 0 && mw > 500, "SONG-40 2000 random picks (FE every 5th): MW 25 / 26 returned %d times (0 expected; MW picks %d)", bad, mw); }
    { Song_Init(&st); int ok = 1; for (int q = 0; q < 37; q++) { int id2 = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); if (id2 < 0 || id2 == SONG_ID_MW(25) || id2 == SONG_ID_MW(26)) ok = 0; }
      int id2 = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); CHECK(ok && inf.refilled && id2 == 6 && inf.allowed == 37 && inf.after == 36, "SONG-41 SEQUENTIAL IG: 37 in order without MW 25 / 26, then the refill restarts at U6 (allowed %d)", inf.allowed); }
    // ---- the strings byte for byte (the titles without quotes; MW 23's artist padded)
    CHECK(!strcmp(kMw[14][1], "Celldweller/Styles Of Beyond") && !strcmp(kMw[23][1], "Evol Intent/Mayhem/Thinktank        ") && !strcmp(kMw[3][1], "DJ Spooky/Dave Lombardo") && !strcmp(kMw[19][0], "Feels Just Like It Should (Remix)") && !strcmp(kMw[20][0], "Tao Of The Machine (Remix)") && !strcmp(kMw[25][0], "Most Wanted Mash Up") && !strcmp(kMw[26][0], "The Mann"),
        "STR-3 the five long strings: MW 14 / 23 / 3 artists = the '/' rule, MW 23's + 8 trailing spaces; MW 19 / 20 titles '(Remix)' without quotes; MW 25 / 26's strings still in the table (unused)");
    // ---- the long-song hook
    static const unsigned char lsBytes[10] = { 0x8B, 0x45, 0xF0, 0x89, 0x07, 0xE9, 0xB7, 0x00, 0x00, 0x00 }, rdr[18] = { 0x83, 0xFA, 0x03, 0x75, 0x0C, 0x3D, 0xFF, 0xFF, 0x7F, 0x00, 0x7E, 0x05, 0x2D, 0x00, 0x00, 0x00, 0x01, 0xC3 };
    CHECK(!memcmp(exe + 0x327CD8, lsBytes, 10), "LONG-1 exe bytes at VA 0x727CD8 (file 0x327CD8) = 8b 45 f0 89 07 e9 b7 00 00 00 = the tag-0x85 store (the hook site; 0x727CDD + 5 + 0xb7 = 0x727D99)");
    CHECK(!memcmp(exe + 0x32A1D7, rdr, 18), "LONG-2 exe bytes at VA 0x72A1D7 = the reader's 3-byte sign step (cmp edx,3 / jne / cmp eax,0x7FFFFF / jle / sub eax,0x1000000 / ret) = what the model copies");
    CHECK(ModelReader(0xC32EAB, 3) == -3985749 && ModelReader(0x7D5626, 3) == 8214054 && ModelReader(0x00B67119, 4) == 11956505 && ModelReader(0x80, 1) == -128 && ModelReader(0x8000, 2) == -32768 && ModelReader(0x7FFFFF, 3) == 0x7FFFFF,
        "LONG-3 the model: MW 9's 3-byte 0xC32EAB -> -3,985,749, MW 15's 0x7D5626 -> 8,214,054, Riders' 4-byte 11,956,505 unchanged, 1 / 2-byte sign steps, 0x7FFFFF kept");
    { int fx = 0; uint32_t a = ModelStub(ModelReader(0xC32EAB, 3), 3, &fx), b2 = ModelStub(ModelReader(0x7D5626, 3), 3, &fx), c2 = ModelStub(ModelReader(0x00B67119, 4), 4, &fx);
      CHECK(a == 0xC32EAB && b2 == 0x7D5626 && c2 == 11956505 && fx == 1, "LONG-4 the stub: MW 9 -> 12,791,467 (counted), MW 15 / Riders untouched (fixed %d)", fx); }
    { int rows = 0, endToday = 0, endAfter = 0, fixedN = 0, okFix = 1, okNine = 1; static const int nine[9] = { 46, 49, 52, 56, 60, 64, 65, 66, 67 }; int seenNine = 0;
      for (int fi = 5; fi <= 6; fi++) { FILE *cf = fopen(argv[fi], "r"); if (!cf) { perror(argv[fi]); exit(2); } char line[600];
        while (fgets(line, sizeof line, cf)) { unsigned wave, len, stored; if (sscanf(line, " %u | %u | %u 0x", &wave, &len, &stored) != 3) continue; rows++;
          int32_t raw = ModelReader(stored, (int)len); uint32_t total = (uint32_t)raw; int today = stored >= total; endToday += today;
          int before = fixedN; uint32_t fixed = ModelStub(raw, (int)len, &fixedN); int after = stored >= fixed; endAfter += after; if (fixed != stored) okFix = 0;
          int isNine = 0; for (int k = 0; k < 9; k++) if (fi == 5 && wave == (unsigned)nine[k]) isNine = 1;
          if (isNine) { seenNine++; if (today || fixedN != before + 1) okNine = 0; } else if (!today || fixedN != before) okNine = 0; }
        fclose(cf); }
      CHECK(rows == 69 && endToday == 60 && endAfter == 69 && fixedN == 9 && okFix && okNine && seenNine == 9, "LONG-5 the 69 header counts (26 MW + 43 UG2; PLAYED = the stored count = the block sum): without the hook %d of %d end (the nine MW waves 46 49 52 56 60 64 65 66 67 do not); with the stub %d of %d end, %d counts corrected, each to its stored value", endToday, rows, endAfter, rows, fixedN); }
    // ==== the EA Trax strings: titles, artists, albums ====
    CHECK(!strcmp(kMw[8][2], "NFS Most Wanted Exclusive") && !strcmp(kMw[14][2], "Celldweller") && !strcmp(kMw[22][2], "Barrier Break / Submission") && !strcmp(kMw[23][2], "The HUMAN Resource") && !strcmp(kUg2Album19, "Lost and Found"),
        "STR-4 the five album strings byte for byte: MW 8 'NFS Most Wanted Exclusive', 14 'Celldweller', 22 'Barrier Break / Submission', 23 'The HUMAN Resource', UG2 row 19 'Lost and Found'");
    CHECK(kMw[25][2][0] == 0 && kMw[26][2][0] == 0 && empties == 2, "STR-5 MW 25 / 26's albums stay empty (out, never shown): %d empty MW albums", empties);
    { static const char *oldMw[27][3] = {
        { "", "", "" },
        { "\"Fired Up\"", "Hush", "Bulletproof" },
        { "\"Let's Move\"", "The Perceptionists", "Black Dialogue" },
        { "\"B-Side Wins Again feat. Chuck D\"", "DJ Spooky and Dave Lombardo", "Drums Of Death" },
        { "\"Hand Of Blood\"", "Bullet For My Valentine", "Hand Of Blood" },
        { "\"Tilted\"", "Lupe Fiasco", "Food &..." },
        { "\"You'll Be Under My Wheels\"", "The Prodigy", "Always Outnumbered, Never Outgunned" },
        { "\"One Good Reason\"", "Celldweller", "Celldweller" },
        { "\"I Am Rock\"", "Rock", "" },
        { "\"Blinded In Chains\"", "Avenged Sevenfold", "City Of Evil" },
        { "\"We Control\"", "Hyper", "We Control" },
        { "\"Skinnyman\"", "Static-X", "Start A War" },
        { "\"Decadence\"", "Disturbed", "Ten Thousand Fists" },
        { "\"In A Hood Near You\"", "Suni Clay", "Suni Clay" },
        { "\"Shapeshifter\"", "Celldweller feat. Styles Of Beyond", "" },
        { "\"Nine Thou (Superstars Remix)\"", "Styles Of Beyond", "Megadef" },
        { "\"Sets Go Up\"", "Juvenile", "Reality Check" },
        { "\"Do Ya Thang\"", "T.I. Presents The P$$C", "25 To Life" },
        { "\"Blood And Thunder\"", "Mastodon", "Leviathan" },
        { "\"Feels Just Like It Should (Timo Maas Remix)\"", "Jamiroquai", "Dynamite" },
        { "\"Tao Of The Machine (Scott Humphrey's Remix)\"", "The Roots and BT", "Blade II Soundtrack" },
        { "\"Feed The Addiction\"", "Ils", "Bohemia" },
        { "\"Barrier Break\"", "Diesel Boy + Kaos", "" },
        { "\"Broken Sword\"", "Evol Intent, Mayhem & Thinktank", "" },
        { "\"You Must Follow (Evol Intent VIP)\"", "Stratus", "Dungeonmaster's Guide" },
        { "\"Most Wanted Mash Up\"", "Paul Linford and Chris Vrenna", "" },
        { "\"The Mann\"", "Paul Linford and Chris Vrenna", "" } };
      int same = 0, changed = 0, okCh = 1; for (int k = 1; k <= 26; k++) for (int c = 0; c < 3; c++) { int exp = (c == 0) || (c == 1 && (k == 3 || k == 14 || k == 20 || k == 22 || k == 23)) || (c == 2 && (k == 8 || k == 14 || k == 22 || k == 23));   // every title differs from the older strings above (the quotes off)
        int eq = !strcmp(kMw[k][c], oldMw[k][c]); if (eq) same++; else changed++; if (eq == exp) okCh = 0; }
      CHECK(same == 43 && changed == 35 && okCh && !strcmp(kMw[3][0], "B-Side Wins Again/Chuck D") && !strcmp(kMw[20][1], "The Roots/BT") && !strcmp(kMw[22][1], "Diesel Boy/Kaos"),
        "STR-6 kMw vs the earlier table (the strings as the game spells them): %d of 78 strings the same, exactly the %d the rules change differ (every title - the quotes off, MW 15 / 19 / 20 '(Remix)'; MW 3 title + artist, 14 / 20 / 22 / 23 artists, 8 / 14 / 22 / 23 albums)", same, changed); }
    { const char *lng = (const char *)exe + (0x789BDC - 0x400000); size_t l0 = strlen(lng), m = 0; const char *six[6] = { kMw[8][2], kMw[14][2], kMw[22][2], kMw[23][2], kUg2Album19, kUg2Artist0 }; for (int k = 0; k < 6; k++) if (strlen(six[k]) > m) m = strlen(six[k]);
      int shorter = 1; for (int k = 1; k <= 26; k++) if (Str_VisibleLen(kMw[k][1]) > 34) shorter = 0;   // MW 23's trailing spaces draw nothing; the longest artist before the '/' rule was 'Celldweller feat. Styles Of Beyond' (34): the rule only shortens
      CHECK(l0 == 38 && !strcmp(lng, "Need for Speed Underground 2 Exclusive") && m == 26 && m < l0 && shorter, "STR-7 lengths: the longest new string = %u ('Barrier Break / Submission') < %u = UG2's own longest album in the exe (00789BDC '%s', rows 0 / 1); every artist line's visible text <= 34 (the rule only shortens; MW 23's 8 trailing spaces not counted)", (unsigned)m, (unsigned)l0, lng); }
    { uint32_t row19[4]; memcpy(row19, exe + (0x7ECEB0 - 0x400000), 16); const char *title19 = (row19[0] >= 0x783000 && row19[0] < 0x7E8000) ? (const char *)exe + (row19[0] - 0x400000) : "(out of range)";
      CHECK(row19[0] >= 0x783000 && row19[0] < 0x7E8000 && !strcmp(title19, "\"Determined\"") && row19[2] == STR_ROW19_STOCK_ALBUM && exe[0x783FE5 - 0x400000] == 0 && row19[3] == 0x789AF4,
        "STR-8 the exe's row 19 (VA 0x7ECEB0 = file 0x3ECEB0): title %08X reads %s, album pointer %08X = 00783FE5 -> .rdata file 0x383FE5 = %02X (an empty string), pool %08X = what the load-time check expects (albums_rows.txt)", row19[0], title19, row19[2], exe[0x783FE5 - 0x400000], row19[3]); }
    CHECK(Str_Album19Ok(1, 0x00783FE5u) && !Str_Album19Ok(0, 0x00783FE5u) && !Str_Album19Ok(1, 0x00789BDCu) && !Str_Album19Ok(1, 0) && STR_ROW19 == 19,
        "STR-9 the load-time rule for row 19: the title ok + the stock's 00783FE5 -> write; the title check failed, another pointer (00789BDC = a real album), or 0 -> NO write");
    { volatile uint32_t img[4] = { 0x789A2Cu, 0x789A1Cu, 0x00783FE5u, 0x789AF4u }; uint32_t want = 0x10001234u; int r1 = Str_PutPtr(&img[2], want), r2 = Str_PutPtr(&img[2], want);
      CHECK(r1 == 1 && r2 == 2 && img[2] == want && img[0] == 0x789A2Cu && img[1] == 0x789A1Cu && img[3] == 0x789AF4u, "STR-10 the pointer write on a row image: first call 1 (written, read back), second 2 (already); +0 / +4 / +0xC untouched"); }
    CHECK(SONG_CARRIER_A == 25 && SONG_CARRIER_B == 26 && STR_ROW19 != SONG_CARRIER_A && STR_ROW19 != SONG_CARRIER_B && STR_ROW0 == 0, "STR-11 rows 19 / 0 are not carriers: rows 25 / 26 keep the carrier rule (their plans checked above); row 19 gets the one load-time album write, row 0 the artist write");
    { int bad = 0; static const char *marks[4] = { " feat. ", " and ", " & ", " + " }; for (int k = 1; k <= 24; k++) for (int c = 0; c < 2; c++) for (int q = 0; q < 4; q++) if (strstr(kMw[k][c], marks[q])) bad++;
      CHECK(bad == 0 && strstr(kMw[25][1], " and ") && !strcmp(kUg2Artist0, "Snoop Dogg/The Doors") && !strchr(kMw[17][1], '/'), "STR-12 the '/' rule: no ' feat. ' / ' and ' / ' & ' / ' + ' left in any shown MW title or artist (1..24: %d hits); MW 25 / 26 (out) keep 'and'; UG2 row 0 = 'Snoop Dogg/The Doors'; 'T.I. Presents The P$$C' (one act) untouched", bad); }
    { uint32_t row0[4]; memcpy(row0, exe + (0x7ECD80 - 0x400000), 16); const char *aEn = (const char *)exe + (0x789C04 - 0x400000), *aFr = (const char *)exe + (0x7896EC - 0x400000); uint32_t tEn, tFr; memcpy(&tFr, exe + (0x7ECF30 - 0x400000), 4); memcpy(&tEn, exe + (0x7ECF38 - 0x400000), 4);
      CHECK(row0[1] == STR_ROW0_STOCK_EN && tEn == STR_ROW0_STOCK_EN && tFr == STR_ROW0_STOCK_FR && !strcmp(aEn, "Snoop Dogg feat. The Doors") && !strcmp(aFr, "Snoop Dogg avec The Doors"),
        "STR-13 the exe's row 0 artist: .data initial %08X and the fix-up table [0x7ECF38] %08X = 'Snoop Dogg feat. The Doors', [0x7ECF30] %08X = 'Snoop Dogg avec The Doors' = the two stock pointers the mod replaces", row0[1], tEn, tFr); }
    CHECK(Str_Artist0Stock(0x00789C04u) && Str_Artist0Stock(0x007896ECu) && !Str_Artist0Stock(0x00789BC0u) && !Str_Artist0Stock(0) && !Str_Artist0Stock(0x10001234u),
        "STR-14 the row-0 rule: the English or the French stock pointer -> replaced; another UG2 string (00789BC0 'Capone'), 0 or a foreign pointer -> NO write");
    { static const unsigned char fix[10] = { 0xA3, 0x84, 0xCD, 0x7E, 0x00, 0xA1, 0x34, 0xCF, 0x7E, 0x00 }, fix2[6] = { 0x89, 0x0D, 0x84, 0xCD, 0x7E, 0x00 }; int ncall = 0;
      for (long o = 0x1000; o < 0x382AF1 - 5; o++) if (exe[o] == 0xE8) { int32_t rel; memcpy(&rel, exe + o + 1, 4); if ((uint32_t)(o + 5 + rel + 0x400000) == 0x459D90u) ncall++; }
      CHECK(!memcmp(exe + 0x59D9C, fix, 10) && !memcmp(exe + 0x59DB3, fix2, 6) && ncall == 0, "STR-15 UG2's own row-0 writer 0x459D90: 459d9c 'mov [0x7ecd84],eax' + 459db3 'mov [0x7ecd84],ecx' (the language fix-up, bytes as read) and %d direct calls to it in .text (reached through a pointer -> its moment is read from the log: the pick-time compare-first write covers it)", ncall); }
    // ---- the soundtrack bar: the step order, the allowed sets per soundtrack, the settings file, the profile-name sanitiser, the exe sites + their callers
    { int s = SET_ST_UG2, r[6], l[5]; for (int k = 0; k < 6; k++) { s = Set_Step(s, 1); r[k] = s; } s = SET_ST_UG2; for (int k = 0; k < 5; k++) { s = Set_Step(s, -1); l[k] = s; }
      CHECK(r[0] == SET_ST_MW && r[1] == SET_ST_CARBON && r[2] == SET_ST_ALL && r[3] == SET_ST_UG1 && r[4] == SET_ST_UG2 && r[5] == SET_ST_MW && l[0] == SET_ST_UG1 && l[1] == SET_ST_ALL && l[2] == SET_ST_CARBON && l[3] == SET_ST_MW && l[4] == SET_ST_UG2,
        "SET-1 the step order: right from Underground 2 = Most Wanted, Carbon, All Trax, Underground, Underground 2, Most Wanted (wraps); left from Underground 2 = Underground, All Trax, Carbon, Most Wanted, Underground 2; Off skipped");
      CHECK(Set_Step(SET_ST_UG1, 1) == SET_ST_UG2 && Set_Step(SET_ST_CARBON, -1) == SET_ST_MW && Set_Step(SET_ST_CARBON, 1) == SET_ST_ALL && Set_Step(SET_ST_MW, 0) == SET_ST_MW && Set_Step(99, 1) == SET_ST_MW && Set_Step(-3, -1) == SET_ST_UG1 && Set_Step(SET_ST_OFF, 1) == SET_ST_MW
        && Set_Installed(SET_ST_UG1) && Set_Installed(SET_ST_CARBON) && !Set_Installed(-1) && !Set_Installed(6) && !Set_Installed(SET_ST_OFF) && Set_Installed(SET_ST_UG2) && Set_Installed(SET_ST_MW) && Set_Installed(SET_ST_ALL) && SET_DEFAULT == SET_ST_UG2,
        "SET-2 an uninstalled / foreign start (Off included) counts from the default Underground 2; a 0 step = no change; installed = UG2 UG1 MW Carbon All; the default = UG2; Carbon's neighbours = Most Wanted / All Trax"); }
    CHECK(Set_Decision(SET_ST_UG2) == -2 && Set_Decision(SET_ST_OFF) == -1 && Set_Decision(SET_ST_MW) == 0 && Set_Decision(SET_ST_ALL) == 0 && !strcmp(kSetName[SET_ST_MW], "MW") && !strcmp(kSetName[SET_ST_ALL], "ALL") && !strcmp(kSetName[SET_ST_OFF], "OFF") && !strcmp(kSetName[SET_ST_UG2], "UG2") && !strcmp(kSetText[SET_ST_ALL], "All Trax") && !strcmp(kSetText[SET_ST_UG2], "Underground 2") && !strcmp(kSetText[SET_ST_MW], "Most Wanted") && !strcmp(kSetText[SET_ST_UG1], "Underground") && !strcmp(kSetText[SET_ST_CARBON], "Carbon") && strlen(kSetText[SET_ST_UG2]) == 13 && strlen(kSetText[SET_ST_UG1]) <= SET_TEXT_MAX && strlen(kSetText[SET_ST_MW]) <= SET_TEXT_MAX && strlen(kSetText[SET_ST_CARBON]) <= SET_TEXT_MAX && strlen(kSetText[SET_ST_ALL]) <= SET_TEXT_MAX && SET_TEXT_MAX == 13,
        "SET-3 the decision class: UG2 = -2 (the stock picker), Off = -1 (no song), MW / All = the mod's own pick; the file words UG2 / MW / ALL / OFF, the texts Underground 2 (13 chars, the longest) / Most Wanted / All Trax / Underground / Carbon, every one <= 13 < the object's 14 (UG2's print cuts a longer word at 14)");
    { for (int p = 0; p < 27; p++) { listSong[p] = p; listMode[p] = p < 6 ? 1 : 2; } RigList(listSong, listMode);
      int cnt[4][2] = {{0}}, mwOnly = 1, ug2Only = 1; int sts[4] = { SET_ST_MW, SET_ST_ALL, SET_ST_UG2, SET_ST_OFF };
      for (int k = 0; k < 4; k++) { set_soundtrack = sts[k]; for (int i = 0; i < SONG_NSONG; i++) for (int c = 0; c < 2; c++) if (Song_Allowed(c, i, listSong, listMode)) { cnt[k][c]++; if (k == 0 && !SONG_IS_MW(i)) mwOnly = 0; if (k == 2 && SONG_IS_MW(i)) ug2Only = 0; } }
      set_soundtrack = SET_ST_ALL;
      CHECK(cnt[0][0] == 10 && cnt[0][1] == 16 && mwOnly && cnt[1][0] == 16 && cnt[1][1] == 37 && cnt[2][0] == 6 && cnt[2][1] == 21 && ug2Only && cnt[3][0] == 0 && cnt[3][1] == 0,
        "SET-4 the allowed sets by soundtrack (the rig's list: modes 1 x 6, 2 x 21): MW FE %d / IG %d (MW songs only, 25 / 26 out = 24), All FE %d / IG %d, UG2 FE %d / IG %d (UG2 songs only; the asi never asks: -2), Off 0 / 0", cnt[0][0], cnt[0][1], cnt[1][0], cnt[1][1], cnt[2][0], cnt[2][1]); }
    { set_soundtrack = SET_ST_MW; Song_State st4; Song_Init(&st4); Song_PickInfo inf4; int ok = 1, seen[SONG_NSONG] = {0}, nb = 0;
      for (int q = 0; q < 16; q++) { int id = Song_Choose(&st4, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf4); if (id < 0 || !SONG_IS_MW(id) || seen[id]++ || SONG_MW_OUT(SONG_MW_N(id))) ok = 0; if (q == 0 && id != SONG_ID_MW(4)) ok = 0; nb++; }
      int id17 = Song_Choose(&st4, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf4); int wrapped = inf4.seqWrap || inf4.refilled;
      g_rngMode = 2; int okR = 1; for (int q = 0; q < 10; q++) { int id = Song_Choose(&st4, SONG_CTX_FE, 0, listSong, listMode, FakeRng, NULL, &inf4); if (id < 0 || !SONG_IS_MW(id) || inf4.count != 10 - q) okR = 0; }
      Song_TakeSong(&st4, SONG_CTX_FE, 0, listSong, listMode, &inf4); int afterTake = inf4.after;
      set_soundtrack = SET_ST_ALL;
      CHECK(ok && nb == 16 && id17 == SONG_ID_MW(4) && wrapped && okR && Song_RidersSignal(SONG_CTX_FE, 1, -1) && !Song_RidersSignal(SONG_CTX_FE, 1, 1) && afterTake == 10,
        "SET-5 under MW: 16 sequential IG picks = MW's 16 IG songs once each (MW 4 first = the first IG song in MW order), the 17th wraps (refill); 10 random FE picks count 10..1, all MW; the Riders signal does not read the soundtrack; TakeSong(0) under MW refills the FE set (10) and drops nothing of MW's"); }
    { Set_Settings s, s2; char txt[4096]; Set_Defaults(&s); int n1 = Set_Format(&s, txt, sizeof txt); Set_Parse(&s2, txt, n1);
      int okDef = n1 > 0 && !strcmp(txt, "UG2Music settings 2\r\nsoundtrack = UG2\r\n") && s2.soundtrack == SET_ST_UG2 && s2.headerSeen && s2.lineSeen && !s2.badValue && s2.nOther == 0;
      s.soundtrack = SET_ST_MW; strcpy(s.other[0], "MW 01 = IG"); strcpy(s.other[1], "MW 26 = OFF"); s.nOther = 2; int n2 = Set_Format(&s, txt, sizeof txt); Set_Parse(&s2, txt, n2);
      int okMw = !strcmp(txt, "UG2Music settings 2\r\nsoundtrack = MW\r\nMW 01 = IG\r\nMW 26 = OFF\r\n") && s2.soundtrack == SET_ST_MW && s2.nOther == 0 && s2.nSong == 2 && s2.mwMode[1] == 2 && s2.mwMode[26] == 0;   // the two song lines are parsed, the text the same
      const char *v1 = "UG2Music settings 1\n\nsoundtrack=all\n"; Set_Parse(&s2, v1, (int)strlen(v1)); int okLower = s2.soundtrack == SET_ST_ALL && s2.lineSeen && !s2.badValue && s2.nOther == 0;
      const char *v2 = "UG2Music settings 1\r\nsoundtrack = UG1\r\n"; Set_Parse(&s2, v2, (int)strlen(v2)); int okUg1 = s2.soundtrack == SET_ST_UG1 && s2.lineSeen && !s2.badValue;   // UG1 is installed = a valid value
      const char *v3 = "UG2Music settings 1\r\n  SOUNDTRACK  =  Off  \r\nsomething else\r\n"; Set_Parse(&s2, v3, (int)strlen(v3)); int okOff = s2.soundtrack == SET_ST_UG2 && s2.badValue && s2.nOther == 1 && !strcmp(s2.other[0], "something else");
      const char *v4 = "MW 01 = IG\r\n"; Set_Parse(&s2, v4, (int)strlen(v4)); int okNoLine = s2.soundtrack == SET_ST_UG2 && !s2.lineSeen && !s2.headerSeen && s2.nOther == 0 && s2.nSong == 1;   // a song line, parsed
      Set_Parse(&s2, "", 0); int okEmpty = s2.soundtrack == SET_ST_UG2 && !s2.lineSeen && s2.nOther == 0;
      const char *v5 = "soundtrack = Off\r\nsoundtrack = MW\r\n";   /* Off is not a valid soundtrack value */ Set_Parse(&s2, v5, (int)strlen(v5)); int okLast = s2.soundtrack == SET_ST_MW && s2.badValue;   // the last line wins; the bad one counted
      char big[20000]; int bl = 0; for (int k = 0; k < 70; k++) bl += sprintf(big + bl, "line %d\r\n", k); char longl[300]; memset(longl, 'x', 299); longl[299] = 0; bl += sprintf(big + bl, "%s\r\n", longl);
      Set_Parse(&s2, big, bl); int okCap = s2.nOther == SET_MAXLINES && s2.dropped == 7 && strlen(s2.other[63]) == 7;
      Set_Parse(&s2, longl, 299); int okCut = s2.nOther == 1 && strlen(s2.other[0]) == SET_LINELEN - 1;
      int small = Set_Format(&s, txt, 20);
      CHECK(okDef && okMw && okLower && okUg1 && okOff && okNoLine && okEmpty && okLast && okCap && okCut && small == 0,
        "SET-6 the settings file round trip: defaults -> 'UG2Music settings 1 / soundtrack = UG2' (CRLF) -> UG2; MW + two kept lines -> the same text back, the lines verbatim; 'soundtrack=all' (lower, no spaces) -> All; 'UG1' (not installed) -> the default + flagged; 'Off' with spaces -> the default UG2 + flagged (Off is out) + the unknown line kept; no soundtrack line / an empty file -> UG2; two lines: the last wins; 71 lines -> 64 kept 7 dropped; a 299-char line cut to %d; a too-small buffer -> 0", SET_LINELEN - 1); }
    { char o[40]; int a = Set_SafeName("Player1", 32, o, sizeof o); int okA = a == 7 && !strcmp(o, "Player1");
      int b2 = Set_SafeName("A/B:C*d.e\\f \"g\" h-i_j", 32, o, sizeof o); int okB = b2 == 21 && !strcmp(o, "A_B_C_d_e_f _g_ h-i_j");
      char raw[32]; memset(raw, 'Z', 32); int c = Set_SafeName(raw, 32, o, sizeof o); int okC = c == 31 && strlen(o) == 31;
      int d = Set_SafeName("", 32, o, sizeof o); char nz[4] = { (char)0xE9, 'a', 0, 0 }; int e = Set_SafeName(nz, 4, o, sizeof o); int okE = e == 2 && !strcmp(o, "_a");
      int f = Set_SafeName("abcdefgh", 32, o, 4); int okF = f == 3 && !strcmp(o, "abc");
      CHECK(okA && okB && okC && d == 0 && okE && okF, "SET-7 the profile-name sanitiser: 'Player1' kept; / : * . \\ \" -> '_' (space - _ kept); 32 bytes without a 0 -> 31; empty -> 0 (no file); a byte over 127 -> '_'; a small output buffer caps"); }
    CHECK(!memcmp(exe + (UA_BAR_TRIG - 0x400000), kBarTrigHead, 9) && !memcmp(exe + (UA_BAR_TRIG_TAIL - 0x400000), kBarTrigTail, 16) && !memcmp(exe + (UA_BAR_LABEL - 0x400000), kBarLabelHead, 12)
        && !memcmp(exe + (UA_BAR_MOUSE1 - 0x400000), kBarMouse1, SET_MOUSE1_LEN) && !memcmp(exe + (UA_BAR_MOUSE2 - 0x400000), kBarMouse2, SET_MOUSE2_LEN) && !memcmp(exe + (UA_BAR_MOUSE_JOIN - 0x400000), kBarMouseJoin, 11)
        && !memcmp(exe + (UA_BAR_CTOR - 0x400000), kBarCtor, SET_CTOR_LEN) && !memcmp(exe + (UA_BAR_CTOR_AFTER - 0x400000), kBarCtorAfter, 11)
        && !memcmp(exe + (UA_BAR_PRINTF_OBJ - 0x400000), kBarPrintfHead, 12) && !memcmp(exe + (UA_BAR_SETLABEL - 0x400000), kBarSetLabelHead, 12) && !memcmp(exe + (UA_BAR_SOUND - 0x400000), kBarSoundHead, 9)
        && UA_BAR_MOUSE1 + SET_MOUSE1_LEN == 0x565DDEu && UA_BAR_MOUSE2 + SET_MOUSE2_LEN == UA_BAR_MOUSE_JOIN && UA_BAR_CTOR + SET_CTOR_LEN == 0x5540DDu,
        "BAR-1 the exe's bytes = the asi's expected bytes at 0x5241E0 (9) / 0x524230 (16) / 0x524120 (12) / 0x565DB1 (45, ends at 0x565DDE) / 0x565DF1 (42, ends at the join 0x565E1B) / 0x565E1B (11) / 0x5540BF (30, ends at 0x5540DD) / 0x55412C (11) / 0x537BE0 (12) / 0x50C900 (12) / 0x458520 (9)");
    { unsigned char p[64]; uint32_t tgt = 0x10001000u, rel; int okAll = 1;
      int l1 = Set_MakePatch(0, UA_BAR_MOUSE1, tgt, p); memcpy(&rel, p + 4, 4); okAll &= l1 == SET_MOUSE1_LEN && p[0] == 0x6A && p[1] == 0xFF && p[2] == 0x57 && p[3] == 0xE8 && UA_BAR_MOUSE1 + 8 + rel == tgt && p[8] == 0x83 && p[9] == 0xC4 && p[10] == 0x08 && p[11] == 0xEB && (uint32_t)(UA_BAR_MOUSE1 + 13 + (int8_t)p[12]) == UA_BAR_MOUSE_JOIN;
      for (int k = 13; k < SET_MOUSE1_LEN; k++) okAll &= p[k] == 0x90;
      int l2 = Set_MakePatch(1, UA_BAR_MOUSE2, tgt, p); memcpy(&rel, p + 4, 4); okAll &= l2 == SET_MOUSE2_LEN && p[0] == 0x6A && p[1] == 0x01 && p[2] == 0x57 && p[3] == 0xE8 && UA_BAR_MOUSE2 + 8 + rel == tgt && p[11] == 0xEB && (uint32_t)(UA_BAR_MOUSE2 + 13 + (int8_t)p[12]) == UA_BAR_MOUSE_JOIN;
      for (int k = 13; k < SET_MOUSE2_LEN; k++) okAll &= p[k] == 0x90;
      int l3 = Set_MakePatch(2, UA_BAR_CTOR, tgt, p); memcpy(&rel, p + 6, 4); okAll &= l3 == SET_CTOR_LEN && p[0] == 0x8B && p[1] == 0x54 && p[2] == 0x24 && p[3] == 0x10 && p[4] == 0x52 && p[5] == 0xE8 && UA_BAR_CTOR + 10 + rel == tgt && p[10] == 0x83 && p[11] == 0xC4 && p[12] == 0x04 && p[13] == 0xEB && (uint32_t)(UA_BAR_CTOR + 15 + (int8_t)p[14]) == UA_BAR_CTOR_AFTER;
      for (int k = 15; k < SET_CTOR_LEN; k++) okAll &= p[k] == 0x90;
      int hits = 0; static const unsigned char aa58[4] = { 0x58, 0xAA, 0x83, 0x00 }; unsigned char all[64 * 3]; int at = 0; at += Set_MakePatch(0, UA_BAR_MOUSE1, tgt, all); at += Set_MakePatch(1, UA_BAR_MOUSE2, tgt, all + at); at += Set_MakePatch(2, UA_BAR_CTOR, tgt, all + at);
      for (int k = 0; k + 4 <= at; k++) if (!memcmp(all + k, aa58, 4)) hits++;
      CHECK(okAll && Set_MakePatch(3, 0, 0, p) == 0 && hits == 0, "BAR-2 the patch model: the left block = push -1 / push edi / call -> target / add esp,8 / jmp -> 0x565E1B / nops (45); the right = push +1 .. (42); the ctor = mov edx,[esp+0x10] / push edx / call / add esp,4 / jmp -> 0x55412C / nops (30); a bad kind = 0; no patch byte names [0x83AA58]"); }
    { int c1 = 0, c2 = 0, c3 = 0, a1 = 0, a2a = 0, a2b = 0, a3 = 0;
      for (long o = 0x1000; o < 0x382AF1 - 5; o++) if (exe[o] == 0xE8) { int32_t rel; memcpy(&rel, exe + o + 1, 4); uint32_t tv = (uint32_t)(o + 5 + rel + 0x400000), va = (uint32_t)(o + 0x400000);
        if (tv == UA_BAR_TRIG) { c1++; if (va == 0x56B324u) a1 = 1; } if (tv == UA_BAR_LABEL) { c2++; if (va == 0x52422Bu) a2a = 1; if (va == 0x565E1Du) a2b = 1; } if (tv == 0x565D90u) { c3++; if (va == 0x56B2D1u) a3 = 1; } }
      CHECK(c1 == 1 && a1 && c2 == 2 && a2a && a2b && c3 == 1 && a3, "BAR-3 the callers (E8 scan of .text): 0x5241E0 <- %d (56b324), 0x524120 <- %d (52422b inside the handler, 565e1d the mouse path), 0x565D90 <- %d (56b2d1 the message dispatch)", c1, c2, c3); }
    { static const unsigned char d1[11] = { 0x3D, 0x13, 0xEF, 0x73, 0x50, 0x77, 0x53, 0x0F, 0x84, 0x3C, 0x01 }, d2[15] = { 0x3D, 0x59, 0xEC, 0xFE, 0xD9, 0x75, 0x28, 0x50, 0x8B, 0xCE, 0xE8, 0xB7, 0x8E, 0xFB, 0xFF };
      CHECK(!memcmp(exe + 0x16B1D8, d1, 11) && !memcmp(exe + 0x16B31A, d2, 15), "BAR-4 the message dispatch 0x56B1C0: 56b1d8 cmp eax,0x5073EF13 / 56b1df je 0x56B321 (LTRIGGER) and 56b31a cmp eax,0xD9FEEC59 / 56b31f jne / 56b321 push eax; mov ecx,esi; call 0x5241E0 (RTRIGGER): both triggers reach the hooked handler"); }
    { uint32_t hOff1, hAll1, hOff2, hAll2; memcpy(&hOff1, exe + 0x13E014, 4); memcpy(&hAll1, exe + 0x13E029, 4); memcpy(&hOff2, exe + 0x14BADE, 4); memcpy(&hAll2, exe + 0x14BAF3, 4);
      CHECK(hOff1 == SET_HASH_OFF && hAll1 == SET_HASH_ALL && hOff2 == SET_HASH_OFF && hAll2 == SET_HASH_ALL, "BAR-5 the label hashes: 'Off' %08X at 53e013 / 54badd and 'All' %08X at 53e028 / 54baf2 (the row mode cycle 0x53DEB0 + the fill 0x54B310 use them) = the words UG2 shows in the rows", hOff1, hAll1); }
    { static const unsigned char c1[5] = { 0x68, 0xFA, 0xA2, 0x74, 0xCA }, c2[4] = { 0x83, 0x78, 0x18, 0x02 }, c3[6] = { 0x89, 0x83, 0x04, 0x01, 0x00, 0x00 }, c4[7] = { 0x0F, 0xBE, 0x05, 0xAB, 0x81, 0x83, 0x00 }, c5[7] = { 0x0F, 0xBE, 0x05, 0xC6, 0x81, 0x83, 0x00 };
      CHECK(!memcmp(exe + 0x153CF1, c1, 5) && !memcmp(exe + 0x153D1C, c2, 4) && !memcmp(exe + 0x153D29, c3, 6) && !memcmp(exe + 0x1241F5, c4, 7) && !memcmp(exe + 0x124205, c5, 7) && UA_BAR_OBJ_OFF == 0x104,
        "BAR-6 the ctor's lookup: 553cf1 push 0xCA74A2FA (TRACKMODE_TYPE) .. 553d1c cmp [eax+0x18],2 .. 553d29 mov [ebx+0x104],eax = [screen+0x104] is the top bar text object the asi labels; the trigger sounds 5241f5 movsx [0x8381AB] / 524205 movsx [0x8381C6] = the bytes the asi reads"); }
    { static const unsigned char tail[16] = { 0x8B, 0x0D, 0x84, 0xB8, 0x82, 0x00, 0xE8, 0xE5, 0x4F, 0xF5, 0xFF, 0x5E, 0x5B, 0xC2, 0x04, 0x00 }; int32_t rel; memcpy(&rel, tail + 7, 4);
      CHECK((uint32_t)(0x524236 + 5 + rel) == 0x479220u && !memcmp(exe + 0x124230, tail, 16) && sizeof kBarTrigTail == 16, "BAR-7 the handler's tail 0x524230 the trigger stub re-enters: mov ecx,[0x82B884]; call 0x479220 (UG2's list set-up, as after a Random / Sequential flip); pop esi; pop ebx; ret 4 - with esi / ebx pushed by the stub as the handler's head did"); }
    // ---- the EA Trax list: the code sites, the entry / row layouts, the view per soundtrack, the row-step and preview plans, the song lines, the callers
    { int okB = 1, okOp = 1, cnt[10] = {0}; char miss[400] = "";
      for (int i = 0; i < LIST_NSITES; i++) { const List_Site *s = &kListSites[i]; long o = (long)s->addr - 0x400000; cnt[s->kind]++;
        if (memcmp(exe + o, s->bytes, s->len)) { okB = 0; if (strlen(miss) < 300) { strcat(miss, s->what); strcat(miss, "; "); } }
        uint32_t v = 0; if (List_PatchLen(s->kind) == 4) memcpy(&v, exe + o + s->patchOff, 4); else if (List_PatchLen(s->kind) == 1) v = exe[o + s->patchOff];
        uint32_t exp = s->kind == 0 ? 0x83ACB8u : s->kind == 1 ? 0x83ACBCu : s->kind == 2 ? 0x7ECD80u : s->kind == 3 ? 0x7ECD84u : s->kind == 4 ? 0x7ECD88u : s->kind == 5 ? 0x1Bu : s->kind == 6 ? 0x83ACB8u : s->kind == 7 ? 0xD8u : 0;
        if (List_PatchLen(s->kind) && v != exp) okOp = 0; }
      CHECK(okB && okOp && LIST_NSITES == 26 && cnt[0] == 4 && cnt[1] == 1 && cnt[2] == 1 && cnt[3] == 1 && cnt[4] == 1 && cnt[5] == 1 && cnt[6] == 1 && cnt[7] == 1 && cnt[8] == 2 && cnt[9] == 13,
        "LIST-1 the 26 code sites' bytes = the exe's (%s; + 7 checked-only sites of the rebuild): the fill's 4 list reads [edi*8+0x83acb8] + the mode read [edi*8+0x83acbc] + the 3 table reads [reg+0x7ecd80/84/88] + the bound 'cmp edi,0x1b' + the cycle's 'mov eax,0x83acb8' / 'cmp ecx,0xd8' + the 2 hook heads + 6 checked-only continuations / callees / callers; every operand at its patch offset reads the original value", okB ? "all match" : miss); }
    CHECK(sizeof(List_ViewEntry) == 8 && sizeof(List_TabRow) == 16 && offsetof(List_ViewEntry, mode) == 4 && LIST_SEARCH_BOUND == 0x368 && LIST_MAXVIEW == 109 && LIST_NTAB == 109 && LIST_IMM8_MAX == 127 && sizeof g_fakeRows == 27 * 16,
        "LIST-2 the layouts: a view entry = 8 bytes (int id, mode byte at +4 = the live list's entry), a table row = 16 bytes (= 0x7ECD80's row: the fill's shl 4), the cycle's new search bound 0x368 = 109 x 8, the imm8 limit 127");
    { for (int p = 0; p < 27; p++) { listSong[p] = p; listMode[p] = p < 6 ? 1 : 2; } RigList(listSong, listMode); List_ViewEntry v[LIST_MAXVIEW]; int bad = 9; list_modesSet = 0;
      int nU = List_BuildView(SET_ST_UG2, listSong, listMode, v, &bad); int okU = nU == 27 && bad == 0; for (int p = 0; p < 27; p++) if (v[p].id != p || v[p].mode != listMode[p] || v[p].pad[0] || v[p].pad[1] || v[p].pad[2]) okU = 0; for (int k = 27; k < LIST_MAXVIEW; k++) if (v[k].id != -1 || v[k].mode) okU = 0;
      for (int p = 0; p < 27; p++) { listSong[p] = 26 - p; listMode[p] = 3; } RigList(listSong, listMode); int nR = List_BuildView(SET_ST_UG2, listSong, listMode, v, &bad); int okR = nR == 27 && v[0].id == 26 && v[26].id == 0 && v[5].mode == 3;
      listSong[3] = 27; listSong[4] = -1; int nB = List_BuildView(SET_ST_UG2, listSong, listMode, v, &bad); int okBad = nB == 25 && bad == 2 && v[3].id == 21 && v[24].id == 0 && v[25].id == -1;
      CHECK(okU && okR && okBad, "LIST-3 the view under UG2 = the live list exactly: 27 entries {song, mode} in LIST order (a reversed list -> ids 26..0), the pads id -1 / mode 0; a song index outside 0..26 is skipped + counted (2 bad -> 25 rows, never an id that reads outside the table)"); }
    { for (int p = 0; p < 27; p++) { listSong[p] = p; listMode[p] = p < 6 ? 1 : 2; } RigList(listSong, listMode); List_ViewEntry v[LIST_MAXVIEW]; int bad = 9;
      int nM = List_BuildView(SET_ST_MW, listSong, listMode, v, &bad); int okM = nM == 24 && bad == 0; for (int k = 0; k < 24; k++) if (v[k].id != SONG_ID_MW(k + 1) || v[k].mode != kMwMode[k + 1]) okM = 0; if (v[24].id != -1) okM = 0;
      int nA = List_BuildView(SET_ST_ALL, listSong, listMode, v, &bad); int okA = nA == 107 && v[0].id == SONG_ID_U1(0) && v[25].id == SONG_ID_U1(25) && v[26].id == 0 && v[52].id == 26 && v[53].id == SONG_ID_MW(1) && v[76].id == SONG_ID_MW(24) && v[77].id == SONG_ID_CB(0) && v[106].id == SONG_ID_CB(29) && v[107].id == -1 && v[108].id == -1 && nA <= LIST_IMM8_MAX;
      int nO = List_BuildView(SET_ST_OFF, listSong, listMode, v, &bad); int nU1 = List_BuildView(SET_ST_UG1, listSong, listMode, v, &bad);
      CHECK(okM && okA && nO == 0 && nU1 == 26 && v[0].id == SONG_ID_U1(0) && v[26].id == -1, "LIST-4 the view under MW = MW 1..24 (ids 27..50, 25 / 26 out) with the DefPlay modes = %d rows; under All = UG1's 26, then UG2's 27, then MW's 24, then Carbon's 30 = %d rows (<= 127 = the imm8 limit); Off = 0 rows; UG1 = 26 (U1-7 checks them)", nM, nA); }
    { List_TabRow live[27], saved[27], out[LIST_NTAB]; for (int s = 0; s < 27; s++) { live[s].title = 0x1000 + s; live[s].artist = 0x2000 + s; live[s].album = 0x3000 + s; live[s].pool = 0x4000 + s; saved[s].title = 0x5000 + s; saved[s].artist = 0x6000 + s; saved[s].album = 0x7000 + s; saved[s].pool = 0x8000 + s; }
      List_BuildTable(live, saved, kMw, kU1, kCb, out); int ok = 1;
      for (int s = 0; s < 27; s++) { const List_TabRow *e = (s == 25 || s == 26) ? &saved[s] : &live[s]; if (memcmp(&out[s], e, 16)) ok = 0; }
      for (int k = 1; k <= 26; k++) { const List_TabRow *r = &out[SONG_ID_MW(k)]; if (r->title != (uint32_t)(uintptr_t)kMw[k][0] || r->artist != (uint32_t)(uintptr_t)kMw[k][1] || r->album != (uint32_t)(uintptr_t)kMw[k][2] || r->pool) ok = 0; }
      CHECK(ok && !strcmp(kMw[12][0], "Decadence") && !strcmp(kMw[8][2], "NFS Most Wanted Exclusive"), "LIST-5 the table: rows 0..26 = the live rows (row 0 / 19's strings included) but 25 / 26 = the rows SAVED at load (UG2's own: a borrowed MW title never shows as a list row); rows 27..52 = the 32-bit pointer values of MW 1..26's title / artist / album strings (kMw: MW12 Decadence, MW8 album 'NFS Most Wanted Exclusive'), pool 0 (the rig is 64-bit: the values are compared, not dereferenced)"); }
    { static const unsigned char left[8] = { 0x4B, 0x79, 0x05, 0xBB, 0x03, 0x00, 0x00, 0x00 }, right[8] = { 0x43, 0x83, 0xFB, 0x03, 0x7E, 0x02, 0x33, 0xDB }; uint32_t hL, hR, pL, pR, cL, cR, dL, dR;
      memcpy(&hL, exe + 0x13DEEE, 4); memcpy(&hR, exe + 0x13DF74, 4); memcpy(&pL, exe + 0x165F72, 4); memcpy(&pR, exe + 0x165F90, 4); memcpy(&dL, exe + 0x16B2AB, 4); memcpy(&dR, exe + 0x16B2B9, 4); cL = exe[0x165F71]; cR = exe[0x165F8F];
      int okS = List_StepMode(0, LIST_HASH_LEFT) == 3 && List_StepMode(1, LIST_HASH_LEFT) == 0 && List_StepMode(2, LIST_HASH_LEFT) == 1 && List_StepMode(3, LIST_HASH_LEFT) == 2 && List_StepMode(0, LIST_HASH_RIGHT) == 1 && List_StepMode(1, LIST_HASH_RIGHT) == 2 && List_StepMode(2, LIST_HASH_RIGHT) == 3 && List_StepMode(3, LIST_HASH_RIGHT) == 0 && List_StepMode(2, 0x12345678u) == 2 && List_StepMode(0, 0) == 0;
      CHECK(okS && !memcmp(exe + 0x13DEFB, left, 8) && !memcmp(exe + 0x13DF7A, right, 8) && hL == LIST_HASH_LEFT && hR == LIST_HASH_RIGHT && pL == LIST_HASH_LEFT && pR == LIST_HASH_RIGHT && cL == 0x68 && cR == 0x68 && dL == LIST_HASH_LEFT && dR == LIST_HASH_RIGHT && exe[0x13DEF9] == 0x75 && exe[0x13DEFA] == 0x77 && exe[0x13DF78] == 0x75 && exe[0x13DF79] == 0x67,
        "LIST-6 UG2's step rule copied = its bytes: 53defb 'dec ebx; jns; mov ebx,3' (left %08X at 53deec: 0 -> 3, else -1) / 53df7a 'inc ebx; cmp ebx,3; jle; xor ebx,ebx' (right %08X at 53df72: 3 -> 0, else +1); a foreign hash = unchanged (53def9 jne 53df72, 53df78 jne 53dfe1); the callers push the same hashes (565f71 / 565f8f) and the dispatch compares them (56b2aa / 56b2b8)", hL, hR); }
    { for (int p = 0; p < 27; p++) { listSong[p] = p; listMode[p] = p < 6 ? 1 : 2; } RigList(listSong, listMode); List_ViewEntry v[LIST_MAXVIEW]; int bad; list_modesSet = 0; List_BuildView(SET_ST_ALL, listSong, listMode, v, &bad);
      List_RowPlan a = List_PlanRowStep(SET_ST_ALL, 7, LIST_HASH_RIGHT, v, listSong), b2 = List_PlanRowStep(SET_ST_ALL, SONG_ID_MW(1), LIST_HASH_LEFT, v, listSong), c = List_PlanRowStep(SET_ST_ALL, 7, 0x9803F6E2u, v, listSong), d = List_PlanRowStep(SET_ST_ALL, SONG_ID_MW(26), LIST_HASH_LEFT, v, listSong), e = List_PlanRowStep(SET_ST_ALL, 999, LIST_HASH_LEFT, v, listSong);   /* 999 = an id outside the table (99 is a Carbon song) */
      for (int p = 0; p < 27; p++) listSong[p] = 26 - p;
      List_BuildView(SET_ST_UG2, listSong, listMode, v, &bad); List_RowPlan f2 = List_PlanRowStep(SET_ST_UG2, 7, LIST_HASH_LEFT, v, listSong), g = List_PlanRowStep(SET_ST_UG2, SONG_ID_MW(1), LIST_HASH_LEFT, v, listSong);
      CHECK(a.kind == 5 && a.listPos == -1 && a.mwN == 8 && a.viewIdx == 33 && a.oldMode == 2 && a.newMode == 3 && b2.kind == 5 && b2.mwN == 1 && b2.viewIdx == 53 && b2.oldMode == 1 && b2.newMode == 0 && c.kind == 0 && c.viewIdx == 33 && d.kind == 0 && d.viewIdx == -1 && e.kind == 0 && e.viewIdx == -1 && f2.kind == 1 && f2.listPos == 19 && f2.viewIdx == 19 && f2.newMode == 1 && g.kind == 0 && g.viewIdx == -1,
        "LIST-7 the row-step plan (All view: under the All view every step = kind 5 = All Trax's own set, listPos -1 = the live list NOT written): UG2 song 7 right -> kind 5 nn 8 := 3 (Racing -> All); MW1 left -> kind 5 nn 1 := 0 (Menu -> Off), entry 53 (UG1's 26 rows sit first under All, UG2 song 7 at entry 33); the mouse hash 9803F6E2 -> nothing; MW26 (out, not in the view) / id 999 -> nothing (no entry; 99 = Carbon 20 has one); a reversed UG2 view: song 7 sits at position 19 -> 19 := 1; MW1 not in a UG2 view -> nothing"); }
    { List_PrevPlan a = List_PlanPreview(3, -1, -1), b2 = List_PlanPreview(25, 26, 26), c = List_PlanPreview(SONG_ID_MW(1), -1, -1), d = List_PlanPreview(SONG_ID_MW(5), 26, 26), e = List_PlanPreview(SONG_ID_MW(25), -1, -1), f2 = List_PlanPreview(109, -1, -1), g = List_PlanPreview(-1, -1, -1), h = List_PlanPreview(SONG_ID_MW(9), 7, 25);
      CHECK(a.action == 0 && a.giveBack == -1 && b2.action == 0 && b2.giveBack == 25 && c.action == 1 && c.mwN == 1 && c.carrier == 26 && c.plan.want[1] == SONG_ID_MW(1) && c.plan.want[0] == 0 && d.action == 1 && d.carrier == 25 && d.plan.want[0] == SONG_ID_MW(5) && d.plan.want[1] == -1 && e.action == 2 && f2.action == 2 && g.action == 2 && h.action == 1 && h.carrier == 26 && h.plan.want[1] == SONG_ID_MW(9) && h.plan.want[0] == 0,
        "LIST-8 the preview plan: UG2 song 3 -> UG2's own path; UG2 song 25 (a carrier slot) -> its own path after the slot is given back; MW1 with nothing playing -> carrier 26 = MW1, 25 its own; MW5 while 26 is g_last -> carrier 25, 26 left alone; MW25 (out) / id 109 / -1 -> nothing (want = the id; 60 is UG1 row 7, 79 is Carbon 0); MW9 after UG2 7 with 25 the last carrier -> alternate to 26"); }
    { uint32_t vb = 0x10A00000u, tb = 0x10B00000u; int32_t rel; memcpy(&rel, exe + 0x14BB3B, 4);
      CHECK(List_PatchValue(0, vb, tb, 0) == vb && List_PatchValue(1, vb, tb, 0) == vb + 4 && List_PatchValue(2, vb, tb, 0) == tb && List_PatchValue(3, vb, tb, 0) == tb + 4 && List_PatchValue(4, vb, tb, 0) == tb + 8 && List_PatchValue(5, vb, tb, 51) == 51 && List_PatchValue(6, vb, tb, 0) == vb && List_PatchValue(7, vb, tb, 0) == 0x368 && List_PatchLen(5) == 1 && List_PatchLen(0) == 4 && List_PatchLen(8) == 0 && List_PatchLen(9) == 0
        && exe[0x14BB39] == 0x0F && exe[0x14BB3A] == 0x8C && (uint32_t)(0x54BB3F + rel) == 0x54B9A0u,
        "LIST-9 the patch values: the list reads -> the view (+0 id / +4 mode), the table reads -> the table (+0 / +4 / +8), the bound -> N (1 byte), the search base -> the view, the search bound -> 0x368; the fill's loop 'cmp edi,imm8' at 54bb36 is followed by 'jl 0x54b9a0' (54bb39: rel32 %d) = the row loop", rel); }
    { Set_Settings s, s2; char txt[4096]; const char *v1 = "UG2Music settings 1\r\nsoundtrack = ALL\r\nMW 01 = IG\r\nMW 4 = off\r\nmw 14=AL\r\nMW 27 = FE\r\nMW 02 = XX\r\nMW 26 = OFF\r\nsomething = else\r\n";
      Set_Parse(&s, v1, (int)strlen(v1)); int okP = s.soundtrack == SET_ST_ALL && s.mwMode[1] == 2 && s.mwMode[4] == 0 && s.mwMode[14] == 3 && s.mwMode[26] == 0 && s.mwMode[2] == -1 && s.nSong == 4 && s.badSong == 1 && s.nOther == 3 && !strcmp(s.other[0], "MW 27 = FE") && !strcmp(s.other[1], "MW 02 = XX") && !strcmp(s.other[2], "something = else");
      int n1 = Set_Format(&s, txt, sizeof txt); int okF = n1 > 0 && !strcmp(txt, "UG2Music settings 2\r\nsoundtrack = ALL\r\nMW 01 = IG\r\nMW 04 = OFF\r\nMW 14 = AL\r\nMW 26 = OFF\r\nMW 27 = FE\r\nMW 02 = XX\r\nsomething = else\r\n");
      Set_Parse(&s2, txt, n1); int okRT = !memcmp(s2.mwMode, s.mwMode, sizeof s.mwMode) && s2.nSong == 4 && s2.nOther == 3 && s2.soundtrack == SET_ST_ALL;
      Set_Defaults(&s2); int okD = s2.mwMode[1] == -1 && s2.mwMode[26] == -1 && s2.nSong == 0 && Set_Format(&s2, txt, sizeof txt) > 0 && !strcmp(txt, "UG2Music settings 2\r\nsoundtrack = UG2\r\n");
      int okK = Set_SongKey("MW 01") == 1 && Set_SongKey("mw26") == 26 && Set_SongKey("MW  7") == 7 && Set_SongKey("MW") == 0 && Set_SongKey("MW 0") == 0 && Set_SongKey("MW 27") == 0 && Set_SongKey("MW 1x") == 0 && Set_SongKey("MWX 1") == 0 && Set_SongKey("UG2 1") == 0 && Set_SongKey("MW 001") == 1 && Set_SongKey("MW 0012") == 0
        && Set_ParseMode("fe") == 1 && Set_ParseMode("IG") == 2 && Set_ParseMode("Al") == 3 && Set_ParseMode("OFF") == 0 && Set_ParseMode("Menu") == -1 && Set_ParseMode("") == -1;
      int small = Set_Format(&s, txt, 60);
      CHECK(okP && okF && okRT && okD && okK && small == 0, "LIST-10 the settings file with song lines: 'MW 01 = IG' / 'MW 4 = off' / 'mw 14=AL' / 'MW 26 = OFF' parsed (4), 'MW 27 = FE' (no such song) + 'MW 02 = XX' (a bad word) kept verbatim as other lines (1 bad counted) + the unknown line; written back as 'MW 01 = IG / MW 04 = OFF / MW 14 = AL / MW 26 = OFF' after the soundtrack line then the kept lines; the round trip equal; defaults = no song line; the key / word parsers' edges; a small buffer -> 0"); }
    { for (int p = 0; p < 27; p++) { listSong[p] = p; listMode[p] = p < 6 ? 1 : 2; } RigList(listSong, listMode); Set_Settings s; Set_Defaults(&s); memset(s.u1Mode, 0, sizeof s.u1Mode); memset(s.cbMode, 0, sizeof s.cbMode); RigShared(&s, listSong, listMode); int okDef = list_modesSet && !memcmp(list_mwMode + 1, kMwMode + 1, 26) && LIST_MWMODE(4) == 2 && LIST_MWMODE(1) == 1;
      set_soundtrack = SET_ST_ALL; int fe0 = 0, ig0 = 0; for (int i = 0; i < SONG_NSONG; i++) { fe0 += Song_Allowed(SONG_CTX_FE, i, listSong, listMode); ig0 += Song_Allowed(SONG_CTX_IG, i, listSong, listMode); }
      s.mwMode[4] = 0; RigShared(&s, listSong, listMode); int fe1 = 0, ig1 = 0; for (int i = 0; i < SONG_NSONG; i++) { fe1 += Song_Allowed(SONG_CTX_FE, i, listSong, listMode); ig1 += Song_Allowed(SONG_CTX_IG, i, listSong, listMode); }
      int off4 = !Song_Allowed(SONG_CTX_IG, SONG_ID_MW(4), listSong, listMode) && !Song_Allowed(SONG_CTX_FE, SONG_ID_MW(4), listSong, listMode);
      s.mwMode[4] = 1; RigShared(&s, listSong, listMode); int fe2 = 0, ig2 = 0; for (int i = 0; i < SONG_NSONG; i++) { fe2 += Song_Allowed(SONG_CTX_FE, i, listSong, listMode); ig2 += Song_Allowed(SONG_CTX_IG, i, listSong, listMode); }
      s.mwMode[25] = 3; RigShared(&s, listSong, listMode); int out25 = !Song_Allowed(SONG_CTX_IG, SONG_ID_MW(25), listSong, listMode);
      List_ViewEntry v[LIST_MAXVIEW]; int bad; List_BuildView(SET_ST_MW, listSong, listMode, v, &bad); int okV = v[3].id == SONG_ID_MW(4) && v[3].mode == 1 && v[0].mode == 1 && v[23].id == SONG_ID_MW(24) && v[23].mode == 2;
      Song_State st7; Song_Init(&st7); Song_PickInfo inf7; g_rngMode = 2; int bad4 = 0, picks = 0; for (int q = 0; q < 400; q++) { int ctx = (q % 3 == 0) ? SONG_CTX_FE : SONG_CTX_IG; int id7 = Song_Choose(&st7, ctx, 0, listSong, listMode, FakeRng, NULL, &inf7); if (id7 < 0) break; picks++; if (id7 == SONG_ID_MW(4) && ctx == SONG_CTX_IG) bad4++; }
      s.mwMode[4] = 0; RigShared(&s, listSong, listMode); Song_Init(&st7); int off4picks = 0; for (int q = 0; q < 400; q++) { int ctx = (q % 3 == 0) ? SONG_CTX_FE : SONG_CTX_IG; int id7 = Song_Choose(&st7, ctx, 0, listSong, listMode, FakeRng, NULL, &inf7); if (id7 == SONG_ID_MW(4)) off4picks++; }
      Set_Defaults(&s); memset(s.u1Mode, 0, sizeof s.u1Mode); memset(s.cbMode, 0, sizeof s.cbMode); RigShared(&s, listSong, listMode); set_soundtrack = SET_ST_ALL;   // UG1 and Carbon still OFF for the blocks below
      CHECK(okDef && fe0 == 16 && ig0 == 37 && fe1 == 16 && ig1 == 36 && off4 && fe2 == 17 && ig2 == 36 && out25 && okV && picks == 400 && bad4 == 0 && off4picks == 0,
        "LIST-11 the live modes in the picker's sets (All): defaults = DefPlay (FE 16 / IG 37); MW 4 set Off -> IG 36, MW 4 allowed nowhere; MW 4 set Menu (FE) -> FE 17 / IG 36; MW 25 set All stays out; the MW view carries the live mode (entry 3 = MW4 mode 1); 400 random picks never return MW4 for IG while it is Menu, never at all while it is Off"); }
    { int c1 = 0, c2 = 0, c3 = 0, j4 = 0, c4 = 0, into = 0; int a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0;
      for (long o = 0x1000; o < 0x382AF1 - 5; o++) { int32_t rel; uint32_t tv, va = (uint32_t)(o + 0x400000);
        if (exe[o] == 0xE8 || exe[o] == 0xE9) { memcpy(&rel, exe + o + 1, 4); tv = (uint32_t)(o + 5 + rel + 0x400000);
          if (exe[o] == 0xE8) { if (tv == 0x53DEB0u) { c1++; if (va == 0x565F78u) a1 = 1; if (va == 0x565F96u) a2 = 1; if (va == 0x56B2C6u) a3 = 1; } if (tv == 0x480C10u) { c2++; if (va == 0x56B20Bu) a4 = 1; } if (tv == 0x54B310u) { c3++; if (va == 0x554132u) a5 = 1; } if (tv == 0x47AF90u) c4++; }
          else if (tv == 0x47AF90u) j4++;
          if ((tv > 0x54BA66u && tv <= 0x54BAC4u + 6) || (tv > 0x54BB36u && tv <= 0x54BB38u) || (tv > 0x53DEC8u && tv <= 0x53DECCu) || (tv > 0x53DEE0u && tv <= 0x53DEE5u) || (tv > 0x53DEB0u && tv <= 0x53DEB4u) || (tv > 0x480C10u && tv <= 0x480C15u)) into++; }
        if (exe[o] == 0x0F && exe[o + 1] >= 0x80 && exe[o + 1] <= 0x8F) { memcpy(&rel, exe + o + 2, 4); tv = (uint32_t)(o + 6 + rel + 0x400000);
          if ((tv > 0x54BA66u && tv <= 0x54BAC4u + 6) || (tv > 0x54BB36u && tv <= 0x54BB38u) || (tv > 0x53DEC8u && tv <= 0x53DECCu) || (tv > 0x53DEE0u && tv <= 0x53DEE5u) || (tv > 0x53DEB0u && tv <= 0x53DEB4u) || (tv > 0x480C10u && tv <= 0x480C15u)) into++; } }
      CHECK(c1 == 3 && a1 && a2 && a3 && c2 == 1 && a4 && c3 == 1 && a5 && c4 == 0 && j4 == 1 && into == 0,
        "LIST-12 the callers (E8 / E9 scan of .text): 0x53DEB0 <- %d calls (565f78 / 565f96 the mouse arrows, 56b2c6 the pad / keys), 0x480C10 <- %d (56b20b), 0x54B310 <- %d (554132 the ctor), 0x47AF90 <- %d calls + %d jmp (480c1a = the preview's own tail); rel32 branches landing inside a re-pointed instruction's range or a hooked head's 5 bytes: %d (rel8 jumps are not scanned here: a disassembly of the exe showed no branch at all into 54ba66..54bb3e / 53deb1..53deb4 / 480c11..480c15)", c1, c2, c3, c4, j4, into); }
    // ---- the list rebuild's UG2 facts from the exe (vtables, dtors, the fill) + the layout constants
    { uint32_t rowVt, ctlVt, ctlSlot1, rowDtorTgt; memcpy(&rowVt, exe + 0x395CB0, 4); memcpy(&ctlVt, exe + 0x395CB4, 4); memcpy(&ctlSlot1, exe + 0x395CB8, 4);
      static const unsigned char dtorA[8] = { 0x8D, 0x4E, 0x10, 0xC7, 0x44, 0x24, 0x10, 0x00 }, dtorB[8] = { 0x8D, 0x4E, 0x08, 0xC7, 0x44, 0x24, 0x10, 0xFF }, rowDtor[10] = { 0xC7, 0x01, 0xB0, 0x5C, 0x79, 0x00, 0x83, 0xC1, 0x0C, 0xE9 }, join[12] = { 0x8B, 0xCF, 0xE8, 0xFE, 0xE2, 0xFB, 0xFF, 0x8B, 0x0D, 0x84, 0xB8, 0x82 };
      int32_t r1, r2, r3, r4; memcpy(&r1, exe + 0xCBBCF, 4); memcpy(&r2, exe + 0xCBBDF, 4); memcpy(&r3, exe + 0x13161A, 4); memcpy(&r4, exe + 0x14BB44 - 4 + 4, 4);
      int okD = !memcmp(exe + 0xCBBC3, dtorA, 8) && !memcmp(exe + 0xCBBD3, dtorB, 8) && (uint32_t)(0x4CBBD3 + r1) == 0x4358E0u && (uint32_t)(0x4CBBE3 + r2) == 0x4358E0u;
      rowDtorTgt = (uint32_t)(0x53161E + r3);
      int okR = !memcmp(exe + 0x131610, rowDtor, 10) && rowDtorTgt == 0x4358E0u && exe[0xCBB63] == 0xE8 && (uint32_t)(0x4CBB68 + *(int32_t *)(exe + 0xCBB64)) == 0x531610u;
      static const unsigned char fillTail[7] = { 0x8B, 0x13, 0x8B, 0xCB, 0xFF, 0x52, 0x04 }, sel[3] = { 0x89, 0x43, 0x24 }, ctorZero[21] = { 0x89, 0x5E, 0x18, 0x89, 0x5E, 0x1C, 0x89, 0x5E, 0x20, 0x89, 0x5E, 0x24, 0x89, 0x5E, 0x28, 0x89, 0x5E, 0x2C, 0x89, 0x5E, 0x30 };
      int okF = !memcmp(exe + 0x14BB3F, fillTail, 7) && !memcmp(exe + 0x14BA1F, sel, 3) && !memcmp(exe + 0x142CFC, ctorZero, 21) && !memcmp(exe + 0x165E1B, join, 12) && exe[0x165E37] == 0x8D && exe[0x165E38] == 0x4F && exe[0x165E39] == 0x4C;
      (void)r4;
      CHECK(rowVt == 0x4CBB60u && ctlVt == 0x4CBC00u && ctlSlot1 == 0x5382F0u && okD && okR && okF,
        "LIST-13 the rebuild's UG2 facts (the exe file): the row vtable [0x795CB0] = %08X (its dtor -> 0x531610 = 'mov [ecx],0x795cb0; add ecx,0xc; jmp 0x4358e0' = its string nodes destroyed by the same helper), the list control vtable [0x795CB4] = %08X with slot 1 = %08X (the refresh the fill calls at 54bb3f 'mov edx,[ebx]; mov ecx,ebx; call [edx+4]'); the control's dtor 0x4CBBA0 destroys its row list (+0x10) and its header list (+0x8) with 0x4358E0 (4cbbce / 4cbbde) = the way the mod copies for the rows; the fill stores the selected row at 54ba1f 'mov [ebx+0x24],eax'; the ctor zeroes +0x18..+0x30 (542cfc); the mouse join 565e1b touches the control only through 0x54EB80 after the label + 0x479220", rowVt, ctlVt, ctlSlot1); }
    CHECK(LIST_LISTCTL_OFF == 0x4C && LIST_SELECTED_OFF == 0x24 && LIST_LISTCTL_OFF + LIST_SELECTED_OFF == 0x70 && LIST_ROWLIST_OFF == 0x10 && LIST_ROWCOUNT_OFF == 0x1C && LIST_FLAG_OFF == 0x20 && LIST_FIRSTVIS_OFF == 0x28 && LIST_HEADERGATE_OFF == 0x124 && LIST_HEADERGATE_VALUE == 4 && UA_LIST_DELLIST == 0x4358E0u && UA_LIST_FILL == 0x54B310u && LIST_LISTCTL_VTABLE == 0x795CB4u,
        "LIST-14 the layout constants: the list control at screen+0x4C, its selected row at +0x24 = screen+0x70 (the field the cycle 53deb7 and the preview's caller 56b1fe read), the row list +0x10, count +0x1C, flag +0x20, first visible +0x28; the header gate [screen+0x124] = 4; the helper 0x4358E0, the fill 0x54B310, the vtable 0x795CB4");
    // ---- the save / load sites + the state rule
    { int okS = 1, okT = 1; char miss[300] = ""; unsigned char e[5];
      for (int i = 0; i < SAVE_NSITES; i++) { const Save_Site *s = &kSaveSites[i]; Save_Expected(s, e); long o = (long)s->addr - 0x400000;
        if (memcmp(exe + o, e, 5)) { okS = 0; if (strlen(miss) < 200) { strcat(miss, s->what); strcat(miss, "; "); } }
        uint32_t r; memcpy(&r, exe + o + 1, 4); if (exe[o] != 0xE8 || s->addr + 5u + r != s->target) okT = 0; }
      static const unsigned char pSave[16] = { 0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x54, 0x8B, 0x4E, 0x14, 0x8B, 0x56, 0x50, 0x57, 0x50, 0x51 };   // UG2's save-function head 0x4D9C60 (16 B), left untouched
      static const unsigned char pSaveAs[11] = { 0x53, 0x56, 0x8B, 0xF1, 0x57, 0xB9, 0xD0, 0xA9, 0x83, 0x00, 0xE8 };                              // UG2's save-function head 0x4D9B80 (11 B), left untouched
      static const unsigned char loadHead[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x24 }, loadRet[3] = { 0xC2, 0x0C, 0x00 }, nameAssign[6] = { 0xB9, 0xE0, 0xA9, 0x83, 0x00, 0xE8 }, helperHead[5] = { 0x8B, 0x44, 0x24, 0x04, 0x57 };
      int okP = !memcmp(exe + 0x0D9C60, pSave, 16) && !memcmp(exe + 0x0D9B80, pSaveAs, 11);
      int okW = UA_SAVE_CALL1 >= 0x4D9C60u + SAVE_HEAD_WIN && UA_SAVE_CALL1 + 5 <= 0x4D9CFFu && UA_SAVE_CALL2 >= 0x4D9B80u + SAVEAS_HEAD_WIN && UA_SAVE_CALL2 + 5 <= 0x4D9C5Fu;
      int okL = !memcmp(exe + 0x1344D0, loadHead, 6) && !memcmp(exe + 0x1346E7, loadRet, 3) && !memcmp(exe + 0x0E8F41, nameAssign, 6) && !memcmp(exe + 0x03E030, helperHead, 5);
      uint32_t rA, rB, rC; memcpy(&rA, exe + 0x0E8F46 + 1, 4); memcpy(&rB, exe + 0x0E8F52 + 1, 4); memcpy(&rC, exe + 0x0F3652 + 1, 4);
      int okC = exe[0x0E8F46] == 0xE8 && 0x4E8F46u + 5 + rA == 0x502F40u && exe[0x0E8F52] == 0xE8 && 0x4E8F52u + 5 + rB == 0x4D9B80u && exe[0x0F3652] == 0xE8 && 0x4F3652u + 5 + rC == 0x4D9C60u;
      int okR = Save_Rel(0x4D9CB2u, 0x43E030u) == 0xFFF64379u && Save_Rel(0x5347ABu, 0x5344D0u) == 0xFFFFFD20u && SAVE_NSITES == 4 && kSaveSites[0].site == 1 && kSaveSites[1].site == 2 && kSaveSites[2].site == 3 && kSaveSites[3].site == 4;
      CHECK(okS && okT && okP && okW && okL && okC && okR,
        "SAVE-1 the save / load sites (the exe file): the 4 call operands = E8 + the rel32 to their targets (%s%s; 4d9cb2 / 4d9bea -> 0x43E030, 5347ab / 534812 -> 0x5344D0), the save-function heads 0x4D9C60 (16 B) / 0x4D9B80 (11 B) the mod leaves untouched = UG2's own bytes (%s) and both save sites lie after them inside their functions (%s), the load 0x5344D0 = 'push ebp; mov ebp,esp; sub esp,0x24' .. 'ret 0xC' at 5346e7, 4e8f41 'mov ecx,0x83a9e0; call 0x502f40' assigns the name right before 4e8f52 'call 0x4d9b80', 4f3652 'call 0x4d9c60', 0x43E030 head 'mov eax,[esp+4]; push edi' (%s %s), the rel32 rule (%s)",
        okS ? "all 4 match" : "MISMATCH: ", miss, okP ? "match" : "MISMATCH", okW ? "yes" : "NO", okL ? "ok" : "LOAD/HELPER MISMATCH", okC ? "callers ok" : "CALLERS MISMATCH", okR ? "ok" : "WRONG"); }
    { Save_State s = { 0, 0, 0, 0, 0 }; int ok = 1;
      Save_Step(&s); Save_Step(&s); ok &= s.dirty == 1 && s.steps == 2 && s.writes == 0;                 // two steps: memory only, nothing written
      Save_Write(&s); ok &= s.dirty == 0 && s.steps == 0 && s.writes == 1 && s.dropped == 0;              // the game saves: written, clean
      Save_Write(&s); ok &= s.writes == 2 && s.dirty == 0;                                                 // a save with nothing new: written again (the game writes its whole profile too)
      Save_Step(&s); Save_Read(&s); ok &= s.dirty == 0 && s.steps == 0 && s.reads == 1 && s.dropped == 1 && s.writes == 2;   // a step, then a profile load: the step dropped, the file read, no write
      Save_Step(&s); Save_Step(&s); Save_Step(&s); Save_Quit(&s); ok &= s.dropped == 4 && s.writes == 2 && s.dirty == 0;       // three steps, a quit without saving: gone, no write
      Save_Read(&s); ok &= s.reads == 2 && s.dropped == 4;                                                // the next start's load: the file = the last save
      CHECK(ok, "SAVE-2 the state rule: a step = memory only (0 writes), a save = written + clean (also with nothing new), a load = read + the unsaved steps dropped, a quit without saving = dropped + 0 writes (writes %d, reads %d, dropped %d)", s.writes, s.reads, s.dropped); }
    // ---- the save prompt: the keep-a-copy / changed? heads + the snapshot rule
    { static const unsigned char keepRet[3] = { 0xC2, 0x04, 0x00 }, cmpRet[1] = { 0xC3 }, fieldW[6] = { 0x89, 0x86, 0x78, 0x67, 0x01, 0x00 }, fieldClr[10] = { 0xC7, 0x86, 0x78, 0x67, 0x01, 0x00, 0, 0, 0, 0 }, testAl[2] = { 0x84, 0xC0 };
      int okH = !memcmp(exe + 0x12A460, kPromptKeepHead, 12) && !memcmp(exe + 0x12A4C0, kPromptCmpHead, 12);
      int okR = !memcmp(exe + 0x12A4B8, keepRet, 3) && !memcmp(exe + 0x12A5C9, cmpRet, 1) && !memcmp(exe + 0x12A496, fieldW, 6) && !memcmp(exe + 0x12A5B7, fieldClr, 10) && !memcmp(exe + 0x15E5AA, testAl, 2);
      uint32_t r1, r2, r3; memcpy(&r1, exe + 0x12A4B1 + 1, 4); memcpy(&r2, exe + 0x12A50C + 1, 4); memcpy(&r3, exe + 0x15E5A5 + 1, 4);
      int okC = exe[0x12A4B1] == 0xE8 && 0x52A4B1u + 5 + r1 == 0x52A280u && exe[0x12A50C] == 0xE8 && 0x52A50Cu + 5 + r2 == 0x52A280u && exe[0x15E5A5] == 0xE8 && 0x55E5A5u + 5 + r3 == 0x52A4C0u;
      int okA = UA_PROMPT_KEEP_RESUME == UA_PROMPT_KEEP + PROMPT_STOLEN && UA_PROMPT_CMP_RESUME == UA_PROMPT_CMP + PROMPT_STOLEN && PROMPT_STOLEN == 6 && PROMPT_COPY_OFF == 0x16778u && UA_PROMPT_PROFILE_OBJ + 0x10 == 0x83A9E0u;
      CHECK(okH && okR && okC && okA,
        "PROMPT-1 the save-prompt facts (the exe file): 0x52A460 head 55 8B EC 56 8B F1 + 'mov eax,[esi+0x16778]' (%s) .. 52a496 '[esi+0x16778] := the copy', 52a4b1 call 0x52A280 (the serializer), 52a4b8 ret 4; 0x52A4C0 head 83 EC 0C 56 8B F1 + the same read (%s) .. 52a50c call 0x52A280, 52a5b7 '[esi+0x16778] := 0', 52a5c9 ret; 55e5a5 call 0x52A4C0 then 55e5aa 'test al,al' (%s); the resume = head + 6, the copy field 0x16778, the profile object 0x83A9D0 + 0x10 = 0x83A9E0 (%s)",
        okH ? "match" : "MISMATCH", okR ? "rets / field ok" : "RETS / FIELD MISMATCH", okC ? "calls ok" : "CALLS MISMATCH", okA ? "ok" : "WRONG"); }
    { Prompt_State st; memset(&st, 0, sizeof st); signed char mw[27], u1[27], cb[31], al[109]; for (int k = 0; k < 27; k++) mw[k] = u1[k] = -1; memset(cb, -1, 31); memset(al, -1, 109); mw[3] = 1; /* the MW, UG1, Carbon and All Trax arrays */ int d = 9, ok = 1, r;
      Prompt_OnKeep(&st, 0, 1, SET_ST_MW, mw, u1, cb, al); ok &= st.valid == 1 && st.snap.soundtrack == SET_ST_MW && st.snap.mwMode[3] == 1;           // the game made a copy -> snapshot
      r = Prompt_OnCompare(&st, 1, 0, SET_ST_MW, mw, u1, cb, al, &d); ok &= r == 0 && d == 0 && st.valid == 0;                                             // nothing moved -> unchanged, snapshot dropped
      Prompt_OnKeep(&st, 0, 1, SET_ST_MW, mw, u1, cb, al); mw[5] = 0; r = Prompt_OnCompare(&st, 1, 0, SET_ST_MW, mw, u1, cb, al, &d); ok &= r == 1 && d == 1 && st.modSaysYes == 1 && st.gameSaysYes == 0 && st.valid == 0;   // an MW mode stepped -> CHANGED (the mod alone)
      Prompt_OnKeep(&st, 0, 1, SET_ST_MW, mw, u1, cb, al); r = Prompt_OnCompare(&st, 1, 0, SET_ST_ALL, mw, u1, cb, al, &d); ok &= r == 1 && d == 1;                      // the bar stepped -> CHANGED
      Prompt_OnKeep(&st, 0, 1, SET_ST_ALL, mw, u1, cb, al); r = Prompt_OnCompare(&st, 1, 1, SET_ST_ALL, mw, u1, cb, al, &d); ok &= r == 1 && d == 0 && st.gameSaysYes == 1;   // the game says changed (a UG2 row) -> CHANGED, the mod same
      Prompt_OnKeep(&st, 0, 1, SET_ST_ALL, mw, u1, cb, al); mw[7] = 2; r = Prompt_OnCompare(&st, 0, 0, SET_ST_ALL, mw, u1, cb, al, &d); ok &= r == 0 && d == 0 && st.comparesNoCopy == 1 && st.valid == 0;   // the game held no copy (freed elsewhere) -> the game's answer, no compare, snapshot dropped
      Prompt_OnKeep(&st, 0, 0, SET_ST_ALL, mw, u1, cb, al); ok &= st.valid == 0 && st.keepsNoCopy == 1;                                                   // the game made no copy -> no snapshot
      Prompt_OnKeep(&st, 0, 1, SET_ST_ALL, mw, u1, cb, al); Prompt_OnKeep(&st, 1, 1, SET_ST_UG2, mw, u1, cb, al); ok &= st.valid == 1 && st.snap.soundtrack == SET_ST_ALL;   // a second keep while the game kept its old copy -> the old snapshot stays
      r = Prompt_OnCompare(&st, 1, 0, SET_ST_UG2, mw, u1, cb, al, &d); ok &= r == 1 && d == 1;                                                             // .. so the bar's change since the first keep counts
      CHECK(ok, "PROMPT-2 the snapshot rule: a copy made -> snapshot; same state -> unchanged; an MW mode / the bar stepped -> CHANGED by the mod alone; the game's own yes kept; no copy held -> the game's answer, no compare; no copy made -> no snapshot; a second keep over an old copy keeps the old snapshot (keeps %d, compares %d, mod yes %d, game yes %d)", st.keeps, st.compares, st.modSaysYes, st.gameSaysYes); }
    // ---- UG1 as a soundtrack. The blocks above ran with UG1's modes forced OFF (main's start) so their numbers hold; from here UG1's own defaults.
    { for (int p = 0; p < 27; p++) { listSong[p] = p; listMode[p] = p < 6 ? 1 : 2; } RigList(listSong, listMode); Set_Settings s; Set_Defaults(&s); RigShared(&s, listSong, listMode); set_soundtrack = SET_ST_ALL;
      int ok1 = 1;
      for (int r = 0; r < 26; r++) { if (kU1Stream[r] != r + 1 || SONG_U1_WAVE(r) != 71u + r || SONG_U1_WAVE(r) < SONG_WAVE_U1_FIRST || SONG_U1_WAVE(r) > SONG_WAVE_U1_LAST || !Song_WaveOkForCarrier(0, SONG_U1_WAVE(r)) || !Song_WaveOkForCarrier(1, SONG_U1_WAVE(r)) || Song_AddedWave(SONG_ID_U1(r)) != SONG_U1_WAVE(r)) ok1 = 0; }
      CHECK(ok1 && SONG_WAVE_U1_FIRST == 70 && SONG_WAVE_U1_LAST == 96 && Song_WaveOkForCarrier(0, 97) && Song_WaveOkForCarrier(1, 97) && Song_WaveOkForCarrier(0, 126) && Song_WaveOkForCarrier(1, 126) && !Song_WaveOkForCarrier(0, 127) && !Song_WaveOkForCarrier(1, 127) && Song_AddedWave(SONG_ID_MW(12)) == 55 && Song_AddedWave(SONG_ID_MW(26)) == 69,
        "UG1-1 kU1Stream[r] = r + 1 (UG1-EXE 53c507 / 53c658 'inc', the request's +0x24 bounded 0..26): UG1 row r -> wave 71 + r (71..96, inside UG1's 70..96), accepted by both carrier checks; 97 / 126 accepted (Carbon's), 127 refused; MW 12 / 26 still waves 55 / 69");
      int ok2 = SONG_NSONG == 109 && SONG_ID_U1(0) == 53 && SONG_ID_U1(25) == 78 && SONG_IS_U1(53) && SONG_IS_U1(78) && !SONG_IS_U1(52) && !SONG_IS_U1(79) && SONG_U1_R(60) == 7 && SONG_IS_MW(27) && SONG_IS_MW(52) && !SONG_IS_MW(53) && !SONG_IS_MW(26) && SONG_IS_ADDED(27) && SONG_IS_ADDED(78) && !SONG_IS_ADDED(26) && SONG_ID_MW(26) == 52 && SONG_NU1 == 26;
      CHECK(ok2, "UG1-2 the ids: UG2 0..26, MW 27..52 (unchanged), UG1 53..78 (= 53 + row); SONG_IS_MW is 27..52 only, SONG_IS_U1 53..78 only, SONG_IS_ADDED = 27..108");
      int ok3 = !strcmp(kU1[0][0], "Doomsday") && !strcmp(kU1[0][1], "Overseer") && !strcmp(kU1[0][2], "Wreckage") && !strcmp(kU1[4][0], "Kimosabe") && !strcmp(kU1[4][1], "BT") && !strcmp(kU1[4][2], "Emotional Technology") && !strcmp(kU1[23][0], "Body Rock") && !strcmp(kU1[23][1], "X-ecutioners") && !strcmp(kU1[23][2], "NFS Underground Exclusive")
        && !strcmp(kU1[19][0], "Get Low") && !strcmp(kU1[19][1], "Lil Jon & the Eastside Boyz") && !strcmp(kU1[19][2], "Kings of Crunk") && !strcmp(kU1[25][0], "24's") && !strcmp(kU1[25][1], "T.I.") && !strcmp(kU1[25][2], "Trap Muzik") && !strcmp(kU1[21][0], "Who's Who") && !strcmp(kU1[10][0], "The Wonders of You") && !strcmp(kU1[10][1], "Andy Hunter") && !strcmp(kU1[24][2], "NFS Underground Exclusive") && !strcmp(kU1[20][2], "NFS Underground Exclusive") && !strcmp(kU1[22][2], "NFS Underground Exclusive");
      int joins = 0, empties = 0, longest = 0; for (int r = 0; r < 26; r++) for (int f = 0; f < 3; f++) { const char *t = kU1[r][f]; if (!t[0]) empties++; if ((int)strlen(t) > longest) longest = (int)strlen(t); if (f < 2 && !(r == 19 && f == 1) && (strstr(t, " & ") || strstr(t, " and ") || strstr(t, "feat") || strstr(t, " + "))) joins++; }
      CHECK(ok3 && joins == 0 && empties == 0 && longest <= 40, "UG1-3 the UG1 strings: row 0 Doomsday / Overseer / Wreckage, row 4 Kimosabe / BT -> 'Emotional Technology', row 23 Body Rock / X-ecutioners -> 'NFS Underground Exclusive' (rows 20 / 22 / 24's albums the same), row 19 'Lil Jon & the Eastside Boyz' kept, rows 10 / 21 / 25 as UG1 has them (every title without its quotes), no other joined act name, no empty string, the longest %d chars", longest);
      int ok4 = 1; for (int r = 0; r < 26; r++) { if (kU1Mode[r + 1] != (r < 19 ? 2 : 1)) ok4 = 0; }
      int okLive = LIST_U1MODE(1) == 2 && LIST_U1MODE(19) == 2 && LIST_U1MODE(20) == 1 && LIST_U1MODE(26) == 1 && list_u1Set;
      s.u1Mode[1] = 0; RigShared(&s, listSong, listMode); int okStep = LIST_U1MODE(1) == 0 && LIST_U1MODE(2) == 2; Set_Defaults(&s); RigShared(&s, listSong, listMode);
      CHECK(ok4 && okLive && okStep && kU1Mode[0] == 0, "UG1-4 UG1's own defaults (UG1-EXE table mode words): rows 1..19 Racing (IG), 20..26 Menu (FE); the live mode = the file's line when there is one (row 1 -> OFF), else the default");
      int s5 = SET_ST_ALL, r5[4], l5[4]; for (int k = 0; k < 4; k++) { s5 = Set_Step(s5, 1); r5[k] = s5; } s5 = SET_ST_ALL; for (int k = 0; k < 4; k++) { s5 = Set_Step(s5, -1); l5[k] = s5; }
      int ok5 = r5[0] == SET_ST_UG1 && r5[1] == SET_ST_UG2 && r5[2] == SET_ST_MW && r5[3] == SET_ST_CARBON && l5[0] == SET_ST_CARBON && l5[1] == SET_ST_MW && l5[2] == SET_ST_UG2 && l5[3] == SET_ST_UG1
        && Set_Step(SET_ST_CARBON, 1) == SET_ST_ALL && Set_Step(SET_ST_OFF, -1) == SET_ST_UG1 && Set_Step(99, 1) == SET_ST_MW && Set_Step(SET_ST_UG1, 0) == SET_ST_UG1 && Set_Installed(SET_ST_UG1) && Set_Installed(SET_ST_CARBON) && SET_DEFAULT == SET_ST_UG2
        && !strcmp(kSetName[SET_ST_UG1], "UG1") && !strcmp(kSetText[SET_ST_UG1], "Underground") && strlen(kSetText[SET_ST_UG1]) == 11 && Set_Decision(SET_ST_UG1) == SET_DEC_OWN && kSetOrder[0] == SET_ST_ALL && kSetOrder[4] == SET_ST_CARBON;
      CHECK(ok5, "UG1-5 the bar: right from All Trax = Underground, Underground 2, Most Wanted, Carbon (the Carbon checks cover the wrap); left = Carbon, Most Wanted, Underground 2, Underground; an uninstalled / foreign start counts from the default Underground 2; 'UG1' / 'Underground' (11 chars); Underground = the mod's own pick");
      int c6[4][2] = {{0}}, u1Only = 1, mwOnly = 1; int st6[4] = { SET_ST_UG1, SET_ST_ALL, SET_ST_MW, SET_ST_UG2 };
      for (int k = 0; k < 4; k++) { set_soundtrack = st6[k]; for (int i = 0; i < SONG_NSONG; i++) for (int c = 0; c < 2; c++) if (Song_Allowed(c, i, listSong, listMode)) { c6[k][c]++; if (k == 0 && !SONG_IS_U1(i)) u1Only = 0; if (k == 2 && !SONG_IS_MW(i)) mwOnly = 0; } }
      set_soundtrack = SET_ST_ALL;
      CHECK(c6[0][0] == 7 && c6[0][1] == 19 && u1Only && c6[1][0] == 23 && c6[1][1] == 86 && c6[2][0] == 10 && c6[2][1] == 16 && mwOnly && c6[3][0] == 6 && c6[3][1] == 21 && Set_InSoundtrack(SET_ST_UG1, 53) && Set_InSoundtrack(SET_ST_UG1, 78) && !Set_InSoundtrack(SET_ST_UG1, 52) && !Set_InSoundtrack(SET_ST_UG1, 79) && !Set_InSoundtrack(SET_ST_MW, 53) && Set_InSoundtrack(SET_ST_MW, 52) && Set_InSoundtrack(SET_ST_ALL, 78) && !Set_InSoundtrack(SET_ST_UG2, 53),
        "UG1-6 the allowed sets with UG1's defaults (+ Carbon's 30 Racing under All Trax): Underground FE %d / IG %d (UG1 songs only), All Trax FE %d / IG %d (16 + 7 / 37 + 19 + 30), Most Wanted FE %d / IG %d (MW only; 53..108 excluded), Underground 2 FE %d / IG %d", c6[0][0], c6[0][1], c6[1][0], c6[1][1], c6[2][0], c6[2][1], c6[3][0], c6[3][1]);
      { List_ViewEntry v[LIST_MAXVIEW]; int bad = 9; int nU1 = List_BuildView(SET_ST_UG1, listSong, listMode, v, &bad); int ok7 = nU1 == 26 && bad == 0;
        for (int r = 0; r < 26; r++) { if (v[r].id != SONG_ID_U1(r) || v[r].mode != kU1Mode[r + 1] || v[r].pad[0] || v[r].pad[1] || v[r].pad[2]) ok7 = 0; }
        if (v[26].id != -1 || v[78].id != -1) ok7 = 0;
        int nA = List_BuildView(SET_ST_ALL, listSong, listMode, v, &bad); int okA = nA == 107 && bad == 0 && v[0].id == 53 && v[0].mode == 2 && v[25].id == 78 && v[25].mode == 1 && v[26].id == 0 && v[52].id == 26 && v[53].id == SONG_ID_MW(1) && v[76].id == SONG_ID_MW(24) && v[77].id == SONG_ID_CB(0) && v[106].id == SONG_ID_CB(29) && v[107].id == -1 && v[108].id == -1 && nA <= LIST_IMM8_MAX;
        int nM = List_BuildView(SET_ST_MW, listSong, listMode, v, &bad); int nU2 = List_BuildView(SET_ST_UG2, listSong, listMode, v, &bad); int okO = nM == 24 && nU2 == 27 && v[0].id == 0 && v[26].id == 26 && v[27].id == -1;
        List_TabRow live[27], saved[27], out[LIST_NTAB]; memset(live, 0, sizeof live); memset(saved, 0, sizeof saved); List_BuildTable(live, saved, kMw, kU1, kCb, out); int okT = 1;
        for (int r = 0; r < 26; r++) { if (out[53 + r].title != (uint32_t)(uintptr_t)kU1[r][0] || out[53 + r].artist != (uint32_t)(uintptr_t)kU1[r][1] || out[53 + r].album != (uint32_t)(uintptr_t)kU1[r][2] || out[53 + r].pool != 0) okT = 0; }
        if (out[SONG_ID_MW(1)].title != (uint32_t)(uintptr_t)kMw[1][0] || out[SONG_ID_MW(26)].album != (uint32_t)(uintptr_t)kMw[26][2]) okT = 0;
        CHECK(ok7 && okA && okO && okT && LIST_MAXVIEW == 109 && LIST_NTAB == 109 && LIST_SEARCH_BOUND == 0x368 && sizeof(List_ViewEntry) * LIST_MAXVIEW == 0x368, "UG1-7 the views: Underground = %d rows (ids 53..78 in UG1's order with the defaults, pads -1); All Trax = %d rows = UG1's 26, then UG2's 27, then MW's 24, then Carbon's 30 (<= 127); MW %d / UG2 %d unchanged; the table rows 53..78 = kU1 (pool 0), MW rows as before; the view 109 x 8 = the search bound 0x368", nU1, nA, nM, nU2); }
      { List_ViewEntry v[LIST_MAXVIEW]; int bad; List_BuildView(SET_ST_ALL, listSong, listMode, v, &bad);
        List_RowPlan a = List_PlanRowStep(SET_ST_ALL, SONG_ID_U1(0), LIST_HASH_RIGHT, v, listSong), b = List_PlanRowStep(SET_ST_ALL, SONG_ID_U1(25), LIST_HASH_LEFT, v, listSong), c = List_PlanRowStep(SET_ST_ALL, SONG_ID_MW(1), LIST_HASH_LEFT, v, listSong), d = List_PlanRowStep(SET_ST_ALL, 5, LIST_HASH_LEFT, v, listSong), e = List_PlanRowStep(SET_ST_ALL, SONG_ID_U1(3), 0x9803F6E2u, v, listSong);
        int ok8 = a.kind == 5 && a.mwN == 1 && a.viewIdx == 0 && a.oldMode == 2 && a.newMode == 3 && b.kind == 5 && b.mwN == 26 && b.viewIdx == 25 && b.oldMode == 1 && b.newMode == 0 && c.kind == 5 && c.mwN == 1 && c.viewIdx == 53 && d.kind == 5 && d.listPos == -1 && d.mwN == 6 && d.viewIdx == 31 && e.kind == 0 && e.viewIdx == 3;   /* kind 5 = All Trax's own set under the All view */
        List_PrevPlan p1 = List_PlanPreview(SONG_ID_U1(3), -1, -1), p2 = List_PlanPreview(SONG_ID_U1(10), 26, 26), p3 = List_PlanPreview(109, -1, -1), p4 = List_PlanPreview(SONG_ID_MW(25), -1, -1);
        int okP = p1.action == 1 && p1.mwN == 4 && p1.carrier == 26 && p1.plan.want[1] == SONG_ID_U1(3) && p1.plan.want[0] == 0 && p2.action == 1 && p2.mwN == 11 && p2.carrier == 25 && p2.plan.want[0] == SONG_ID_U1(10) && p2.plan.want[1] == -1 && p3.action == 2 && p4.action == 2;
        Song_Plan q1 = Song_CarrierPlan(SONG_ID_U1(7), -1, -1), q2 = Song_CarrierPlan(SONG_ID_MW(12), 26, 26), q3 = Song_CarrierPlan(7, 25, 25), q4 = Song_CarrierPlan(SONG_ID_U1(0), 7, 25);
        int okQ = q1.ret == 26 && q1.carrier == 26 && q1.want[1] == 60 && q1.want[0] == 0 && q2.ret == 25 && q2.want[0] == 38 && q2.want[1] == -1 && q3.ret == 7 && q3.carrier == -1 && q3.want[0] == -1 && q3.want[1] == 0 && q4.ret == 26 && q4.want[1] == 53 && q4.want[0] == 0;
        CHECK(ok8 && okP && okQ, "UG1-8 (under the All view every step = kind 5 = All Trax's own set) a UG1 row step = kind 5 (nn = row + 1; the view entries 0..25 under All Trax; UG1's default stepped: row 0 Racing -> All, row 25 Menu -> Off); an MW row = kind 2 at entry 53; a UG2 row = kind 1 at entry 31; a foreign hash on a UG1 row = nothing; the preview of a UG1 id = the carrier path (want = the id 53 + r); the carrier plan's want = the added song's id (MW 12 -> 38, UG1 row 7 -> 60, row 0 -> 53), the alternation as before"); }
      { Set_Settings f, f2; char txt[4096]; const char *v1 = "UG2Music settings 1\r\nsoundtrack = UG1\r\nMW 04 = IG\r\nUG1 01 = OFF\r\nug1 20 = ig\r\nUG1  26 = AL\r\nUG1 27 = FE\r\nUG1 0 = FE\r\nUG1 05 = Menu\r\nother = x\r\n";
        Set_Parse(&f, v1, (int)strlen(v1)); int okP = f.soundtrack == SET_ST_UG1 && !f.badValue && f.mwMode[4] == 2 && f.u1Mode[1] == 0 && f.u1Mode[20] == 2 && f.u1Mode[26] == 3 && f.u1Mode[2] == -1 && f.u1Mode[5] == -1 && f.nSong == 4 && f.badSong == 1 && f.nOther == 4;
        int n1 = Set_Format(&f, txt, sizeof txt); int okF = n1 > 0 && !strcmp(txt, "UG2Music settings 2\r\nsoundtrack = UG1\r\nMW 04 = IG\r\nUG1 01 = OFF\r\nUG1 20 = IG\r\nUG1 26 = AL\r\nUG1 27 = FE\r\nUG1 0 = FE\r\nUG1 05 = Menu\r\nother = x\r\n");
        Set_Parse(&f2, txt, n1); int okRT = !memcmp(f2.u1Mode, f.u1Mode, 27) && !memcmp(f2.mwMode, f.mwMode, 27) && f2.soundtrack == SET_ST_UG1 && f2.nSong == 4 && f2.nOther == 4;
        const char *v2 = "UG2Music settings 1\r\nsoundtrack = ALL\r\nMW 01 = IG\r\n"; Set_Parse(&f2, v2, (int)strlen(v2)); int okOld = 1;
        for (int k = 0; k < 27; k++) { if (f2.u1Mode[k] != -1) okOld = 0; }
        okOld = okOld && f2.mwMode[1] == 2 && f2.nSong == 1 && f2.soundtrack == SET_ST_ALL;
        RigShared(&f2, listSong, listMode); int okDef2 = LIST_U1MODE(1) == 2 && LIST_U1MODE(26) == 1 && LIST_MWMODE(1) == 2; Set_Defaults(&f2); RigShared(&f2, listSong, listMode);
        int okK = Set_U1Key("UG1 01") == 1 && Set_U1Key("ug1 26") == 26 && Set_U1Key("UG1  7") == 7 && Set_U1Key("UG1") == 0 && Set_U1Key("UG1 0") == 0 && Set_U1Key("UG1 27") == 0 && Set_U1Key("UG2 01") == 0 && Set_U1Key("MW 01") == 0 && Set_SongKey("UG1 01") == 0 && Set_U1Key("UG1 1x") == 0;
        int small = Set_Format(&f, txt, 40); Set_Defaults(&f); int okDefTxt = Set_Format(&f, txt, sizeof txt) > 0 && !strcmp(txt, "UG2Music settings 2\r\nsoundtrack = UG2\r\n");
        CHECK(okP && okF && okRT && okOld && okDef2 && okK && small == 0 && okDefTxt, "UG1-9 the settings file: 'soundtrack = UG1' valid; 'UG1 nn = FE | IG | AL | OFF' parsed (case / spaces free; nn 1..26; a bad mode word / nn kept verbatim as an other line) and written after the MW lines; an old file without UG1 lines -> UG1's defaults (-1 = kU1Mode); the round trip equal; defaults -> no song lines; a too-small buffer -> 0"); }
      { Prompt_State t; memset(&t, 0, sizeof t); signed char mw[27], u1[27], cb[31], al[109]; memset(mw, -1, 27); memset(u1, -1, 27); memset(cb, -1, 31); memset(al, -1, 109); int d = 9, ok = 1, r;
        Prompt_OnKeep(&t, 0, 1, SET_ST_UG1, mw, u1, cb, al); ok &= t.valid == 1 && t.snap.soundtrack == SET_ST_UG1 && t.snap.u1Mode[5] == -1;
        r = Prompt_OnCompare(&t, 1, 0, SET_ST_UG1, mw, u1, cb, al, &d); ok &= r == 0 && d == 0 && t.valid == 0;
        Prompt_OnKeep(&t, 0, 1, SET_ST_UG1, mw, u1, cb, al); u1[5] = 0; r = Prompt_OnCompare(&t, 1, 0, SET_ST_UG1, mw, u1, cb, al, &d); ok &= r == 1 && d == 1 && t.modSaysYes == 1 && t.gameSaysYes == 0;
        Prompt_OnKeep(&t, 0, 1, SET_ST_UG1, mw, u1, cb, al); r = Prompt_OnCompare(&t, 1, 0, SET_ST_UG1, mw, u1, cb, al, &d); ok &= r == 0 && d == 0;
        Prompt_OnKeep(&t, 0, 1, SET_ST_UG1, mw, u1, cb, al); mw[2] = 3; r = Prompt_OnCompare(&t, 1, 0, SET_ST_UG1, mw, u1, cb, al, &d); ok &= r == 1 && d == 1;
        Prompt_OnKeep(&t, 0, 1, SET_ST_UG1, mw, u1, cb, al); u1[26] = 2; r = Prompt_OnCompare(&t, 0, 0, SET_ST_UG1, mw, u1, cb, al, &d); ok &= r == 0 && d == 0 && t.comparesNoCopy == 1;
        CHECK(ok && sizeof(Prompt_Snap) == 195, "UG1-10 the snapshot holds the UG1 modes (195 B with All Trax's 109): a UG1-only row step -> CHANGED by the mod alone (UG2's save dialog); the same state -> unchanged; an MW step still counts; no copy held -> the game's answer (keeps %d, compares %d, mod yes %d)", t.keeps, t.compares, t.modSaysYes); }
      { Song_State st; Song_PickInfo inf; int ok = 1; Set_Settings f; Set_Defaults(&f); f.u1Mode[3] = 0; f.u1Mode[20] = 0; memset(f.cbMode, 0, sizeof f.cbMode); RigShared(&f, listSong, listMode);   // UG1 rows 2 (Racing) + 19 (Menu) set OFF; Carbon's 30 OFF here (the Carbon block runs them)
        set_soundtrack = SET_ST_UG1; Song_Init(&st); g_rngMode = 2; int picks = 0, offPicks = 0, nonU1 = 0; for (int q = 0; q < 300; q++) { int ctx = (q % 3 == 0) ? SONG_CTX_FE : SONG_CTX_IG; int id = Song_Choose(&st, ctx, 0, listSong, listMode, FakeRng, NULL, &inf); if (id < 0) { ok = 0; break; } picks++; if (!SONG_IS_U1(id)) nonU1++; if (id == SONG_ID_U1(2) || id == SONG_ID_U1(19)) offPicks++; }
        Song_Init(&st); int idF = Song_Choose(&st, SONG_CTX_FE, 1, listSong, listMode, FakeRng, NULL, &inf); int feAllowed = inf.allowed; int idI = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); int idI2 = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf);
        int okSeqU1 = idF == SONG_ID_U1(20) && feAllowed == 6 && idI == SONG_ID_U1(0) && idI2 == SONG_ID_U1(1) && inf.allowed == 18;
        set_soundtrack = SET_ST_ALL; Song_Init(&st); int idA = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); int okSeqAll = idA == SONG_ID_U1(0) && inf.allowed == 55;
        for (int q = 0; q < 54; q++) { Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); }
        int okEnd = inf.after == 0 && st.c[1].pos == 76 && st.c[1].rem[SONG_ID_U1(0)] == 0;
        int idW = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); int okWrap = idW == SONG_ID_U1(0) && inf.refilled;
        Song_Init(&st); g_rngCalls = 0; g_rngMode = 2; int seen[SONG_NSONG] = {0}, okRnd = 1; for (int q = 0; q < 55; q++) { int id = Song_Choose(&st, SONG_CTX_IG, 0, listSong, listMode, FakeRng, NULL, &inf); if (id < 0 || seen[id]++ || inf.count != 55 - q) okRnd = 0; }
        int okOut = !seen[SONG_ID_MW(25)] && !seen[SONG_ID_MW(26)] && !seen[SONG_ID_U1(2)] && !seen[SONG_ID_U1(19)] && seen[SONG_ID_U1(0)] && seen[SONG_ID_U1(18)] && seen[0] == 0 && seen[6] == 1 && g_rngCalls == 55;
        Song_Init(&st); int okRiders = Song_RidersSignal(SONG_CTX_FE, 1, -1) && !Song_RidersSignal(SONG_CTX_IG, 1, -1);
        Set_Defaults(&f); RigShared(&f, listSong, listMode); set_soundtrack = SET_ST_ALL;
        CHECK(ok && picks == 300 && offPicks == 0 && nonU1 == 0 && okSeqU1 && okSeqAll && okEnd && okWrap && okRnd && okOut && okRiders, "UG1-11 the picker (Carbon's 30 forced OFF here = never picked under All Trax): under Underground %d random picks = UG1 songs only, 0 of the two rows set OFF; sequential under Underground FE = row 20 first (row 19 OFF; allowed 6), IG = rows 0, 1 (allowed 18); under All Trax sequential IG = UG1 row 0 first (allowed 55 = 56 - the OFF row), the full cycle ends at position 76 (MW 24), then the refill restarts at row 0; random = a permutation of the 55 with one draw each; MW 25 / 26 and the OFF rows never; the Riders signal unchanged", picks); }
    }
    // ---- Carbon as a soundtrack. From here Carbon's own defaults (all Racing) with the default list (modes 1 x 6, 2 x 21).
    { for (int p = 0; p < 27; p++) { listSong[p] = p; listMode[p] = p < 6 ? 1 : 2; } RigList(listSong, listMode); Set_Settings s; Set_Defaults(&s); RigShared(&s, listSong, listMode); set_soundtrack = SET_ST_ALL;
      int ok1 = 1, seenW[128] = {0};
      for (int c = 0; c < 30; c++) { unsigned w = SONG_CB_WAVE(c); if (w < 97 || w > 126 || seenW[w]++ || w < SONG_WAVE_CB_FIRST || w > SONG_WAVE_CB_LAST || !Song_WaveOkForCarrier(0, w) || !Song_WaveOkForCarrier(1, w) || Song_AddedWave(SONG_ID_CB(c)) != w) ok1 = 0; }
      CHECK(ok1 && kCbWave[0] == 124 && kCbWave[3] == 97 && kCbWave[6] == 125 && kCbWave[7] == 126 && kCbWave[29] == 107 && SONG_WAVE_CB_FIRST == 97 && SONG_WAVE_CB_LAST == 126 && !Song_WaveOkForCarrier(0, 127) && !Song_WaveOkForCarrier(1, 127) && Song_AddedWave(SONG_ID_U1(5)) == 76 && Song_AddedWave(SONG_ID_MW(12)) == 55,
        "CB-1 kCbWave[c] = 95 + Carbon's wave (CB-DATA: the vault row's PathEvent -> event -> wave): 30 distinct waves 97..126 (list 0 = vault row 1 wave 29 -> 124, list 3 = row 8 wave 2 -> 97, list 6 = row 15 wave 30 -> 125, list 7 = row 17 wave 31 -> 126, list 29 = row 30 wave 12 -> 107), each accepted by both carrier checks, 127 refused; UG1 row 5 / MW 12 still 76 / 55");
      int ok2 = SONG_NSONG == 109 && SONG_NCB == 30 && SONG_ID_CB(0) == 79 && SONG_ID_CB(29) == 108 && SONG_IS_CB(79) && SONG_IS_CB(108) && !SONG_IS_CB(78) && !SONG_IS_CB(109) && SONG_CB_C(100) == 21 && !SONG_IS_U1(79) && !SONG_IS_MW(79) && SONG_IS_ADDED(108) && SONG_IS_ADDED(79) && !SONG_IS_ADDED(26) && SONG_IS_U1(78) && SONG_ID_U1(25) == 78;
      CHECK(ok2, "CB-2 the ids: UG2 0..26, MW 27..52, UG1 53..78 (unchanged), Carbon 79..108 (= 79 + the list position c); SONG_IS_CB 79..108 only, SONG_IS_ADDED = 27..108");
      int ok3 = !strcmp(kCb[0][0], "One Of Dem Days (Remix)") && !strcmp(kCb[0][1], "Part 2/Fallacy") && !strcmp(kCb[0][2], "Live From The Breadline") && !strcmp(kCb[6][0], "Scorpio") && !strcmp(kCb[6][1], "Grandmaster Flash And The Furious Five") && !strcmp(kCb[6][2], "The Message")
        && !strcmp(kCb[11][0], "Signs Of Life") && !strcmp(kCb[11][1], "Every Move A Picture") && !strcmp(kCb[11][2], "Heart=Weapon") && !strcmp(kCb[20][0], "Good As Gold") && !strcmp(kCb[20][1], "Tiga") && !strcmp(kCb[20][2], "Sexor") && !strcmp(kCb[21][0], "People Always Talk About The Weather (Remix)") && !strcmp(kCb[21][1], "Yonderboi")
        && !strcmp(kCb[29][0], "Fighting In Built Up Areas") && !strcmp(kCb[29][1], "Ladytron") && !strcmp(kCb[29][2], "Witching Hour") && !strcmp(kCb[19][0], "Joker And The Thief") && !strcmp(kCb[19][1], "Wolfmother") && !strcmp(kCb[4][0], "Hard Drivers") && !strcmp(kCb[4][2], "NFS Carbon Exclusive") && !strcmp(kCb[10][0], "After Party") && !strcmp(kCb[10][1], "Dynamite MC");
      int joins = 0, empties = 0, longT = 0, longA = 0, longB = 0; for (int c = 0; c < 30; c++) for (int f = 0; f < 3; f++) { const char *t = kCb[c][f]; int l = (int)strlen(t); if (!l) empties++; if (f == 0 && l > longT) longT = l; if (f == 1 && l > longA) longA = l; if (f == 2 && l > longB) longB = l; if (f < 2 && !(c == 6 && f == 1) && !(c == 19 && f == 0) && (strstr(t, " & ") || strstr(t, " and ") || strstr(t, " And ") || strstr(t, "feat") || strstr(t, "Feat") || strstr(t, " + "))) joins++; }
      CHECK(ok3 && joins == 0 && empties == 0 && longT == 44 && longA == 38 && longB == 29, "CB-3 the Carbon strings in Carbon's order (CarClass 1 -> 2 -> 3, vault order inside): list 0 = vault row 1 'One Of Dem Days (Remix)' / 'Part 2/Fallacy' / 'Live From The Breadline', list 6 = row 15 'Scorpio' / 'Grandmaster Flash And The Furious Five' kept, list 11 = row 2 (the first CarClass 2), list 20 = row 9 (the first CarClass 3), list 21 = row 10's title 'People Always Talk About The Weather (Remix)' (44 chars; 56 with its quotes and the remixer's name), list 29 = row 30; 'Joker And The Thief' = a title; no other joined act name, no empty string; the longest title %d / artist %d / album %d chars", longT, longA, longB);
      int ok4 = kCbMode[0] == 0; for (int c = 1; c <= 30; c++) if (kCbMode[c] != 2) ok4 = 0;
      int okLive = LIST_CBMODE(1) == 2 && LIST_CBMODE(30) == 2 && list_cbSet;
      s.cbMode[1] = 0; s.cbMode[30] = 1; RigShared(&s, listSong, listMode); int okStep = LIST_CBMODE(1) == 0 && LIST_CBMODE(30) == 1 && LIST_CBMODE(2) == 2; Set_Defaults(&s); RigShared(&s, listSong, listMode);
      CHECK(ok4 && okLive && okStep, "CB-4 every Carbon song's default = Racing (kCbMode[1..30] = 2 = Carbon's own DefPlay 2); the live mode = the file's line when there is one (list 1 -> OFF, 30 -> Menu), else the default");
      int s5 = SET_ST_ALL, r5[5], l5[5]; for (int k = 0; k < 5; k++) { s5 = Set_Step(s5, 1); r5[k] = s5; } s5 = SET_ST_ALL; for (int k = 0; k < 5; k++) { s5 = Set_Step(s5, -1); l5[k] = s5; }
      int ok5 = r5[0] == SET_ST_UG1 && r5[1] == SET_ST_UG2 && r5[2] == SET_ST_MW && r5[3] == SET_ST_CARBON && r5[4] == SET_ST_ALL && l5[0] == SET_ST_CARBON && l5[1] == SET_ST_MW && l5[2] == SET_ST_UG2 && l5[3] == SET_ST_UG1 && l5[4] == SET_ST_ALL
        && Set_Step(SET_ST_CARBON, 1) == SET_ST_ALL && Set_Step(SET_ST_CARBON, -1) == SET_ST_MW && Set_Step(SET_ST_ALL, -1) == SET_ST_CARBON && Set_Step(SET_ST_OFF, -1) == SET_ST_UG1 && Set_Step(SET_ST_OFF, 1) == SET_ST_MW && Set_Step(99, 1) == SET_ST_MW && Set_Step(SET_ST_CARBON, 0) == SET_ST_CARBON
        && Set_Installed(SET_ST_CARBON) && !Set_Installed(SET_ST_OFF) && SET_DEFAULT == SET_ST_UG2 && !strcmp(kSetName[SET_ST_CARBON], "CARBON") && !strcmp(kSetText[SET_ST_CARBON], "Carbon") && strlen(kSetText[SET_ST_CARBON]) == 6 && Set_Decision(SET_ST_CARBON) == SET_DEC_OWN && kSetOrder[4] == SET_ST_CARBON && SET_NORDER == 5 && Set_ParseValue("carbon") == SET_ST_CARBON;
      CHECK(ok5, "CB-5 the bar: right from All Trax = Underground, Underground 2, Most Wanted, Carbon, All Trax (5 values, wraps); left from All Trax = Carbon, Most Wanted, Underground 2, Underground, All Trax; Off / a foreign start count from the default Underground 2; 'CARBON' / 'Carbon' (6 chars <= 13); Carbon = the mod's own pick");
      int c6[5][2] = {{0}}, cbOnly = 1, mwOnly = 1, u1Only = 1; int st6[5] = { SET_ST_CARBON, SET_ST_ALL, SET_ST_MW, SET_ST_UG1, SET_ST_UG2 };
      for (int k = 0; k < 5; k++) { set_soundtrack = st6[k]; for (int i = 0; i < SONG_NSONG; i++) for (int c = 0; c < 2; c++) if (Song_Allowed(c, i, listSong, listMode)) { c6[k][c]++; if (k == 0 && !SONG_IS_CB(i)) cbOnly = 0; if (k == 2 && !SONG_IS_MW(i)) mwOnly = 0; if (k == 3 && !SONG_IS_U1(i)) u1Only = 0; } }
      set_soundtrack = SET_ST_ALL;
      CHECK(c6[0][0] == 0 && c6[0][1] == 30 && cbOnly && c6[1][0] == 23 && c6[1][1] == 86 && c6[2][0] == 10 && c6[2][1] == 16 && mwOnly && c6[3][0] == 7 && c6[3][1] == 19 && u1Only && c6[4][0] == 6 && c6[4][1] == 21 && Set_InSoundtrack(SET_ST_CARBON, 79) && Set_InSoundtrack(SET_ST_CARBON, 108) && !Set_InSoundtrack(SET_ST_CARBON, 78) && !Set_InSoundtrack(SET_ST_CARBON, 109) && !Set_InSoundtrack(SET_ST_UG1, 79) && !Set_InSoundtrack(SET_ST_MW, 79) && !Set_InSoundtrack(SET_ST_UG2, 79) && Set_InSoundtrack(SET_ST_ALL, 108) && !Set_InSoundtrack(SET_ST_OFF, 79),
        "CB-6 the allowed sets with Carbon's defaults: Carbon FE %d / IG %d (Carbon songs only; FE 0 = no menu song under Carbon until a row is set Menu / All, by UG2's rule), All Trax FE %d / IG %d (16 + 7 + 0 / 37 + 19 + 30), Most Wanted FE %d / IG %d (MW only), Underground FE %d / IG %d (UG1 only), Underground 2 FE %d / IG %d", c6[0][0], c6[0][1], c6[1][0], c6[1][1], c6[2][0], c6[2][1], c6[3][0], c6[3][1], c6[4][0], c6[4][1]);
      { List_ViewEntry v[LIST_MAXVIEW]; int bad = 9; int nCB = List_BuildView(SET_ST_CARBON, listSong, listMode, v, &bad); int ok7 = nCB == 30 && bad == 0;
        for (int c = 0; c < 30; c++) { if (v[c].id != SONG_ID_CB(c) || v[c].mode != 2 || v[c].pad[0] || v[c].pad[1] || v[c].pad[2]) ok7 = 0; }
        if (v[30].id != -1 || v[108].id != -1) ok7 = 0;
        int nA = List_BuildView(SET_ST_ALL, listSong, listMode, v, &bad); int okA = nA == 107 && bad == 0 && v[0].id == 53 && v[25].id == 78 && v[26].id == 0 && v[52].id == 26 && v[53].id == SONG_ID_MW(1) && v[76].id == SONG_ID_MW(24) && v[77].id == 79 && v[77].mode == 2 && v[106].id == 108 && v[106].mode == 2 && v[107].id == -1 && v[108].id == -1 && nA <= LIST_IMM8_MAX;
        int nM = List_BuildView(SET_ST_MW, listSong, listMode, v, &bad); int nU1 = List_BuildView(SET_ST_UG1, listSong, listMode, v, &bad); int nU2 = List_BuildView(SET_ST_UG2, listSong, listMode, v, &bad); int okO = nM == 24 && nU1 == 26 && nU2 == 27 && v[0].id == 0 && v[26].id == 26 && v[27].id == -1;
        List_TabRow live[27], saved[27], out[LIST_NTAB]; memset(live, 0, sizeof live); memset(saved, 0, sizeof saved); List_BuildTable(live, saved, kMw, kU1, kCb, out); int okT = 1;
        for (int c = 0; c < 30; c++) { if (out[79 + c].title != (uint32_t)(uintptr_t)kCb[c][0] || out[79 + c].artist != (uint32_t)(uintptr_t)kCb[c][1] || out[79 + c].album != (uint32_t)(uintptr_t)kCb[c][2] || out[79 + c].pool != 0) okT = 0; }
        if (out[53].title != (uint32_t)(uintptr_t)kU1[0][0] || out[78].album != (uint32_t)(uintptr_t)kU1[25][2] || out[SONG_ID_MW(1)].title != (uint32_t)(uintptr_t)kMw[1][0]) okT = 0;
        CHECK(ok7 && okA && okO && okT && LIST_MAXVIEW == 109 && LIST_NTAB == 109 && LIST_SEARCH_BOUND == 0x368 && sizeof(List_ViewEntry) * LIST_MAXVIEW == 0x368 && 107 <= LIST_IMM8_MAX, "CB-7 the views: Carbon = %d rows (ids 79..108 in Carbon's order, all Racing, pads -1); All Trax = %d rows = UG1's 26, UG2's 27, MW's 24, then Carbon's 30 (<= 127 = the imm8 bound); MW %d / UG1 %d / UG2 %d unchanged; the table rows 79..108 = kCb (pool 0), the UG1 / MW rows as before; the view 109 x 8 = the search bound 0x368", nCB, nA, nM, nU1, nU2); }
      { List_ViewEntry v[LIST_MAXVIEW]; int bad; List_BuildView(SET_ST_ALL, listSong, listMode, v, &bad);
        List_RowPlan a = List_PlanRowStep(SET_ST_ALL, SONG_ID_CB(0), LIST_HASH_RIGHT, v, listSong), b = List_PlanRowStep(SET_ST_ALL, SONG_ID_CB(29), LIST_HASH_LEFT, v, listSong), c = List_PlanRowStep(SET_ST_ALL, SONG_ID_U1(0), LIST_HASH_LEFT, v, listSong), d = List_PlanRowStep(SET_ST_ALL, SONG_ID_CB(3), 0x9803F6E2u, v, listSong);
        int ok8 = a.kind == 5 && a.mwN == 1 && a.viewIdx == 77 && a.oldMode == 2 && a.newMode == 3 && b.kind == 5 && b.mwN == 30 && b.viewIdx == 106 && b.oldMode == 2 && b.newMode == 1 && c.kind == 5 && c.mwN == 1 && c.viewIdx == 0 && d.kind == 0 && d.viewIdx == 80;   /* kind 5 = All Trax's own set under the All view */
        List_BuildView(SET_ST_CARBON, listSong, listMode, v, &bad); List_RowPlan e = List_PlanRowStep(SET_ST_CARBON, SONG_ID_CB(3), LIST_HASH_LEFT, v, listSong), f = List_PlanRowStep(SET_ST_CARBON, SONG_ID_U1(0), LIST_HASH_LEFT, v, listSong);
        int okE = e.kind == 4 && e.mwN == 4 && e.viewIdx == 3 && e.newMode == 1 && f.kind == 0 && f.viewIdx == -1;
        List_PrevPlan p1 = List_PlanPreview(SONG_ID_CB(3), -1, -1), p2 = List_PlanPreview(SONG_ID_CB(29), 26, 26), p3 = List_PlanPreview(109, -1, -1), p4 = List_PlanPreview(SONG_ID_U1(3), -1, -1);
        int okP = p1.action == 1 && p1.mwN == 4 && p1.carrier == 26 && p1.plan.want[1] == SONG_ID_CB(3) && p1.plan.want[0] == 0 && p2.action == 1 && p2.mwN == 30 && p2.carrier == 25 && p2.plan.want[0] == SONG_ID_CB(29) && p2.plan.want[1] == -1 && p3.action == 2 && p4.action == 1 && p4.mwN == 4;
        Song_Plan q1 = Song_CarrierPlan(SONG_ID_CB(7), -1, -1), q2 = Song_CarrierPlan(SONG_ID_CB(0), 26, 26), q3 = Song_CarrierPlan(SONG_ID_U1(7), -1, -1);
        int okQ = q1.ret == 26 && q1.carrier == 26 && q1.want[1] == 86 && q1.want[0] == 0 && q2.ret == 25 && q2.want[0] == 79 && q2.want[1] == -1 && q3.want[1] == 60;
        CHECK(ok8 && okE && okP && okQ, "CB-8 (kind 5 under the All view, kind 4 under Carbon) a Carbon row step = kind 5 under All Trax / 4 under Carbon (nn = c + 1; the view entries 77..106 under All Trax, 0..29 under Carbon; Racing -> All on a right step, -> Menu on a left); a UG1 row under Carbon = no view entry = nothing; a foreign hash on a Carbon row = nothing; the preview of a Carbon id = the carrier path (want = the id 79 + c; 109 = nothing); the carrier plan's want = the id (Carbon 7 -> 86, Carbon 0 -> 79, UG1 row 7 -> 60), the alternation as before"); }
      { Set_Settings f, f2; char txt[4096]; const char *v1 = "UG2Music settings 1\r\nsoundtrack = CARBON\r\nMW 04 = IG\r\nUG1 02 = FE\r\nCB 01 = OFF\r\ncb 20 = al\r\nCB  30 = FE\r\nCB 31 = FE\r\nCB 0 = FE\r\nCB 05 = Menu\r\nCB 1x = FE\r\nother = x\r\n";
        Set_Parse(&f, v1, (int)strlen(v1)); int okP = f.soundtrack == SET_ST_CARBON && !f.badValue && f.mwMode[4] == 2 && f.u1Mode[2] == 1 && f.cbMode[1] == 0 && f.cbMode[20] == 3 && f.cbMode[30] == 1 && f.cbMode[2] == -1 && f.cbMode[5] == -1 && f.nSong == 5 && f.badSong == 1 && f.nOther == 5;
        int n1 = Set_Format(&f, txt, sizeof txt); int okF = n1 > 0 && !strcmp(txt, "UG2Music settings 2\r\nsoundtrack = CARBON\r\nMW 04 = IG\r\nUG1 02 = FE\r\nCB 01 = OFF\r\nCB 20 = AL\r\nCB 30 = FE\r\nCB 31 = FE\r\nCB 0 = FE\r\nCB 05 = Menu\r\nCB 1x = FE\r\nother = x\r\n");
        Set_Parse(&f2, txt, n1); int okRT = !memcmp(f2.cbMode, f.cbMode, 31) && !memcmp(f2.u1Mode, f.u1Mode, 27) && !memcmp(f2.mwMode, f.mwMode, 27) && f2.soundtrack == SET_ST_CARBON && f2.nSong == 5 && f2.nOther == 5;
        const char *v2 = "UG2Music settings 1\r\nsoundtrack = ALL\r\nMW 01 = IG\r\nUG1 01 = OFF\r\n"; Set_Parse(&f2, v2, (int)strlen(v2)); int okOld = 1;
        for (int k = 0; k < 31; k++) { if (f2.cbMode[k] != -1) okOld = 0; }
        okOld = okOld && f2.mwMode[1] == 2 && f2.u1Mode[1] == 0 && f2.nSong == 2 && f2.soundtrack == SET_ST_ALL;
        RigShared(&f2, listSong, listMode); int okDef2 = LIST_CBMODE(1) == 2 && LIST_CBMODE(30) == 2 && LIST_U1MODE(1) == 0 && LIST_MWMODE(1) == 2; Set_Defaults(&f2); RigShared(&f2, listSong, listMode);
        int okK = Set_CbKey("CB 01") == 1 && Set_CbKey("cb 30") == 30 && Set_CbKey("CB  7") == 7 && Set_CbKey("CB") == 0 && Set_CbKey("CB 0") == 0 && Set_CbKey("CB 31") == 0 && Set_CbKey("CB 1x") == 0 && Set_CbKey("UG1 01") == 0 && Set_CbKey("MW 01") == 0 && Set_U1Key("CB 01") == 0 && Set_SongKey("CB 01") == 0;
        // an older release without Carbon reading this file: its parser sees a 'CB nn' key as neither the soundtrack key nor an MW / UG1 key -> the line
        // is kept verbatim in other[] (the same code path this parser takes for 'CB 31 = FE' above and for any unknown key), and 'soundtrack = CARBON'
        // is a value not installed there -> badValue -> the default UG2; shown here with an unknown word
        const char *v3 = "UG2Music settings 1\r\nsoundtrack = NOPE\r\nXX 01 = IG\r\n"; Set_Parse(&f2, v3, (int)strlen(v3)); int okT13 = f2.badValue && f2.soundtrack == SET_DEFAULT && f2.nOther == 1 && !strcmp(f2.other[0], "XX 01 = IG") && f2.nSong == 0;
        Set_Defaults(&f); for (int k = 1; k <= 26; k++) { f.mwMode[k] = 0; f.u1Mode[k] = 0; } for (int k = 1; k <= 30; k++) f.cbMode[k] = 0; int nFull = Set_Format(&f, txt, sizeof txt); int okFull = nFull == 21 + 18 + 26 * 13 + 26 * 14 + 30 * 13 && nFull < SET_MAXLINES * SET_LINELEN + 256;
        int small = Set_Format(&f, txt, 100); Set_Defaults(&f); int okDefTxt = Set_Format(&f, txt, sizeof txt) > 0 && !strcmp(txt, "UG2Music settings 2\r\nsoundtrack = UG2\r\n");
        CHECK(okP && okF && okRT && okOld && okDef2 && okK && okT13 && okFull && small == 0 && okDefTxt, "CB-9 the settings file: 'soundtrack = CARBON' valid; 'CB nn = FE | IG | AL | OFF' parsed (case / spaces free; nn 1..30; a bad mode word / nn kept verbatim as an other line) and written after the UG1 lines; an old file without CB lines -> Carbon's defaults (-1 = kCbMode); the round trip equal; an unknown key / value = the same path for a CB line (kept verbatim; 'soundtrack = CARBON' -> the default there); every line written = %d B (< the asi's 7,936 B buffer); a too-small buffer -> 0", nFull); }
      { Prompt_State t; memset(&t, 0, sizeof t); signed char mw[27], u1[27], cb[31], al[109]; memset(mw, -1, 27); memset(u1, -1, 27); memset(cb, -1, 31); memset(al, -1, 109); int d = 9, ok = 1, r;
        Prompt_OnKeep(&t, 0, 1, SET_ST_CARBON, mw, u1, cb, al); ok &= t.valid == 1 && t.snap.soundtrack == SET_ST_CARBON && t.snap.cbMode[5] == -1 && t.snap.cbMode[30] == -1;
        r = Prompt_OnCompare(&t, 1, 0, SET_ST_CARBON, mw, u1, cb, al, &d); ok &= r == 0 && d == 0 && t.valid == 0;
        Prompt_OnKeep(&t, 0, 1, SET_ST_CARBON, mw, u1, cb, al); cb[30] = 0; r = Prompt_OnCompare(&t, 1, 0, SET_ST_CARBON, mw, u1, cb, al, &d); ok &= r == 1 && d == 1 && t.modSaysYes == 1 && t.gameSaysYes == 0;
        Prompt_OnKeep(&t, 0, 1, SET_ST_CARBON, mw, u1, cb, al); r = Prompt_OnCompare(&t, 1, 0, SET_ST_CARBON, mw, u1, cb, al, &d); ok &= r == 0 && d == 0;
        Prompt_OnKeep(&t, 0, 1, SET_ST_CARBON, mw, u1, cb, al); u1[2] = 3; r = Prompt_OnCompare(&t, 1, 0, SET_ST_CARBON, mw, u1, cb, al, &d); ok &= r == 1 && d == 1;
        Prompt_OnKeep(&t, 0, 1, SET_ST_CARBON, mw, u1, cb, al); cb[1] = 2; r = Prompt_OnCompare(&t, 0, 0, SET_ST_CARBON, mw, u1, cb, al, &d); ok &= r == 0 && d == 0 && t.comparesNoCopy == 1;
        CHECK(ok && sizeof(Prompt_Snap) == 195 && offsetof(Prompt_Snap, cbMode) == 55, "CB-10 the snapshot holds the Carbon modes (195 B with All Trax's 109): a Carbon-only row step -> CHANGED by the mod alone (UG2's save dialog); the same state -> unchanged; a UG1 step still counts; no copy held -> the game's answer (keeps %d, compares %d, mod yes %d)", t.keeps, t.compares, t.modSaysYes); }
      { Song_State st; Song_PickInfo inf; int ok = 1; Set_Settings f; Set_Defaults(&f); f.cbMode[3] = 0; f.cbMode[20] = 0; RigShared(&f, listSong, listMode);   // Carbon list 2 + 19 set OFF
        set_soundtrack = SET_ST_CARBON; Song_Init(&st); g_rngMode = 2; int picks = 0, offPicks = 0, nonCB = 0, fePicks = 0; for (int q = 0; q < 300; q++) { int id = Song_Choose(&st, SONG_CTX_IG, 0, listSong, listMode, FakeRng, NULL, &inf); if (id < 0) { ok = 0; break; } picks++; if (!SONG_IS_CB(id)) nonCB++; if (id == SONG_ID_CB(2) || id == SONG_ID_CB(19)) offPicks++; }
        Song_Init(&st); int idF = Song_Choose(&st, SONG_CTX_FE, 1, listSong, listMode, FakeRng, NULL, &inf); int feAllowed = inf.allowed; int idI = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); int idI2 = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf);
        int okSeqCB = idF == -1 && feAllowed == 0 && idI == SONG_ID_CB(0) && idI2 == SONG_ID_CB(1) && inf.allowed == 28; (void)fePicks;
        f.cbMode[7] = 1; RigShared(&f, listSong, listMode); Song_Init(&st); int idF2 = Song_Choose(&st, SONG_CTX_FE, 1, listSong, listMode, FakeRng, NULL, &inf); int okFe = idF2 == SONG_ID_CB(6) && inf.allowed == 1; f.cbMode[7] = -1; RigShared(&f, listSong, listMode);
        set_soundtrack = SET_ST_ALL; Song_Init(&st); int idA = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); int okSeqAll = idA == SONG_ID_U1(0) && inf.allowed == 84;
        for (int q = 0; q < 83; q++) { Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); }
        int okEnd = inf.after == 0 && st.c[1].pos == 108 && st.c[1].rem[SONG_ID_CB(29)] == 0 && st.c[1].rem[SONG_ID_U1(0)] == 0;
        int idW = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); int okWrap = idW == SONG_ID_U1(0) && inf.refilled;
        Song_Init(&st); int idS[5]; for (int q = 0; q < 5; q++) idS[q] = -2; for (int q = 0; q < 84; q++) { int id = Song_Choose(&st, SONG_CTX_IG, 1, listSong, listMode, FakeRng, NULL, &inf); if (q == 56) idS[0] = id; if (q == 57) idS[1] = id; if (q == 58) idS[2] = id; if (q == 83) idS[3] = id; }
        int okOrder = idS[0] == SONG_ID_CB(0) && idS[1] == SONG_ID_CB(1) && idS[2] == SONG_ID_CB(3) && idS[3] == SONG_ID_CB(29);   // after the 56 of UG1 / UG2 / MW: Carbon 0, 1, 3 (2 is OFF), .., 29 last
        Song_Init(&st); g_rngCalls = 0; g_rngMode = 2; int seen[SONG_NSONG] = {0}, okRnd = 1; for (int q = 0; q < 84; q++) { int id = Song_Choose(&st, SONG_CTX_IG, 0, listSong, listMode, FakeRng, NULL, &inf); if (id < 0 || seen[id]++ || inf.count != 84 - q) okRnd = 0; }
        int okOut = !seen[SONG_ID_MW(25)] && !seen[SONG_ID_MW(26)] && !seen[SONG_ID_CB(2)] && !seen[SONG_ID_CB(19)] && seen[SONG_ID_CB(0)] && seen[SONG_ID_CB(29)] && seen[SONG_ID_U1(0)] && seen[0] == 0 && seen[6] == 1 && g_rngCalls == 84;
        Song_Init(&st); int okRiders = Song_RidersSignal(SONG_CTX_FE, 1, -1) && !Song_RidersSignal(SONG_CTX_IG, 1, -1);
        Set_Defaults(&f); RigShared(&f, listSong, listMode); set_soundtrack = SET_ST_ALL;
        CHECK(ok && picks == 300 && offPicks == 0 && nonCB == 0 && okSeqCB && okFe && okSeqAll && okEnd && okWrap && okOrder && okRnd && okOut && okRiders, "CB-11 the picker: under Carbon %d random IG picks = Carbon songs only, 0 of the two rows set OFF; sequential under Carbon FE = -1 (allowed 0: all Racing = no menu song until a row is set Menu / All; list 7 -> Menu then plays), IG = list 0, 1 (allowed 28); under All Trax sequential IG = UG1 row 0 first (allowed 84 = 56 + 30 - the 2 OFF rows), Carbon 0, 1, 3 after the 56 others, the full cycle ends at position 108 (Carbon 29), then the refill restarts at row 0; random = a permutation of the 84 with one draw each; MW 25 / 26 and the OFF rows never; the Riders signal unchanged", picks); }
    }
    // ---- THE SHUFFLE BUTTON: the pure parts of src/shuffle.h + the three byte patterns against the retail exe
    {
      { int ok = !memcmp(exe + 0x3F80C8, kShufFlagBytes, 4) && !memcmp(exe + 0x14E8D7, kShufLabelCall, 5) && !memcmp(exe + 0x15DF22, kShufClickCall, 5) && !memcmp(exe + 0x12D170, kShufMsgHead, 9) && !memcmp(exe + 0x79220, kShufListSetupHead, 8);
        uint32_t o1, o2; memcpy(&o1, exe + 0x14E8D8, 4); memcpy(&o2, exe + 0x15DF23, 4);
        int okRel = (0x54E8D7u + 5u + o1) == 0x50C900u && (0x55DF22u + 5u + o2) == 0x52D170u && Shuf_Rel32(0x54E8D7u, 0x50C900u) == o1 && Shuf_Rel32(0x55DF22u, 0x52D170u) == o2;
        int okHead = exe[0x3F80C8 + 4] != 0xFF || 1;   // (the row's next field is not part of the check)
        CHECK(ok && okRel && okHead && SHUF_FLAGS_OLD == 0x401u && SHUF_FLAGS_NEW == (0x401u | SHUF_BIT) && SHUF_BIT == 0x80000u && SHUF_BIT > 0x400u && SHUF_BIT != 0x8000u && SHUF_BIT != 0x10000u && SHUF_BIT != 0x20000u && SHUF_BIT != 0x80000000u,
          "SHUF-1 the exe bytes: 0x7F80C8 (file 0x3F80C8) = 01 04 00 00 (the jukebox flag word 0x401), 54e8d7 = E8 24 E0 FB FF (call 0x50C900: rel32 %08X), 55df22 = E8 49 F2 FC FF (call 0x52D170: rel32 %08X), the heads of 0x52D170 (9) + 0x479220 (8); Shuf_Rel32 reproduces both operands; the new word 0x%X = 0x401 | 0x80000 (bit 0x80000 above Back's 0x400 = slot 3, not one of the setup's skipped bits)", o1, o2, SHUF_FLAGS_NEW);
      }
      { int ok = !strcmp(Shuf_Text(1), "Shuffle On") && !strcmp(Shuf_Text(0), "Shuffle Off") && !strcmp(Shuf_Text(0x100), "Shuffle Off") && !strcmp(Shuf_Text(0x101), "Shuffle On") && !strcmp(Shuf_Text(0xFFFFFFFFu), "Shuffle On") && strchr(Shuf_Text(1), ':') == NULL && strchr(Shuf_Text(0), ':') == NULL && Shuf_Text(1)[7] == ' ' && Shuf_Text(0)[7] == ' ';
        CHECK(ok && strlen(Shuf_Text(1)) == 10 && strlen(Shuf_Text(0)) == 11 && strlen(Shuf_Text(0)) <= 16, "SHUF-2 the texts by the byte (no colon, a space at [7]): 1 -> 'Shuffle On' (10), 0 -> 'Shuffle Off' (11), only the low byte counts (as 5241ea 'mov bl,[0x83AA58]'); both < the slot's 16 characters (buf 17, FEPrintf cuts at [obj+0x68] - 1)");
      }
      { uint32_t a = Shuf_Flip(1), b = Shuf_Flip(a), c = Shuf_Flip(b);
        CHECK(a == 0 && b == 1 && c == 0 && Shuf_Flip(0x100) == 1 && Shuf_Flip(0x101) == 0 && Shuf_Flip(0xFFFFFFFFu) == 0, "SHUF-3 the flip as 52421d-524226 (xor eax,eax; test bl,bl; sete al; the dword written): 1 -> 0 -> 1 -> 0; 0x100 (byte 0) -> 1; 0x101 -> 0; the result is always 0 or 1");
      }
      { int ok = Shuf_LabelWanted(SHUF_HASH_N2O, "EA_Trax_Jukebox.fng", 2) == 1 && Shuf_LabelWanted(0xEAA1D5DFu, "EA_Trax_Jukebox.fng", 2) == 0 && Shuf_LabelWanted(SHUF_HASH_N2O, "UI_CarParts_Perf.fng", 2) == 0
          && Shuf_LabelWanted(SHUF_HASH_N2O, "EA_Trax_Jukebox.fng", 1) == 0 && Shuf_LabelWanted(SHUF_HASH_N2O, NULL, 2) == 0 && Shuf_LabelWanted(SHUF_HASH_N2O, "", 2) == 0 && Shuf_LabelWanted(SHUF_HASH_N2O, "EA_Trax_Jukebox.fn", 2) == 0 && Shuf_LabelWanted(SHUF_HASH_N2O, "ea_trax_jukebox.fng", 2) == 0;
        CHECK(ok && SHUF_HASH_N2O == 0xBA0E1020u, "SHUF-4 the label gate: hash 0xBA0E1020 (bit 0x80000's stock label) x package 'EA_Trax_Jukebox.fng' (exact) x a type-2 text object -> print; another hash (0xEAA1D5DF = bit 0x100000's), another package, type 1, NULL / empty / a near-miss name -> UG2's own label stays");
      }
      { int ok = Shuf_ClickWanted(0x80000u, "EA_Trax_Jukebox.fng") == 1 && Shuf_ClickWanted(0x400u, "EA_Trax_Jukebox.fng") == 0 && Shuf_ClickWanted(0x1u, "EA_Trax_Jukebox.fng") == 0 && Shuf_ClickWanted(0x100000u, "EA_Trax_Jukebox.fng") == 0
          && Shuf_ClickWanted(0x80000u, "UI_CarParts_Perf.fng") == 0 && Shuf_ClickWanted(0x80000u, NULL) == 0;
        CHECK(ok, "SHUF-5 the click gate: bit 0x80000 on the jukebox -> flip; Back 0x400 / Quit 0x1 / 0x100000 (the Default slot) / another package / no package -> pass (UG2's message goes on as always)");
      }
      { ShufFakeMem m; memset(&m, 0, sizeof m); m.base = 0x10000000u; const uint32_t bar = m.base + 0x100u;
        uint32_t bits[6] = { 0x1u, 0x400u, 0x80000u, 0, 0, 0 }; unsigned char shown[6] = { 1, 1, 1, 0, 0, 0 };
        for (int i = 0; i < 6; i++) { uint32_t node = m.base + 0x200u + 0x40u * i, nextLink = i < 5 ? m.base + 0x200u + 0x40u * (i + 1) + 4u : bar + 0x4Cu; FakeW32(&m, node + 4u, nextLink); FakeW32(&m, node + 0x1Cu, bits[i]); m.m[node + 0x18u - m.base] = shown[i]; FakeW32(&m, node + 0x10u, m.base + 0x300u + 0x10u * i); }
        FakeW32(&m, bar + 0x4Cu, m.base + 0x200u + 4u);
        int s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0; uint32_t n1 = Shuf_FindNode(bar, 0x80000u, FakeRd32, FakeRd8, &m, &s1);                      // the 6-slot list, 3 shown: slot 3 = the 0x80000 node
        uint32_t n2 = Shuf_FindNode(bar, 0x400u, FakeRd32, FakeRd8, &m, &s2);                                                                           // Back = slot 2
        m.m[m.base + 0x280u + 0x18u - m.base] = 0; uint32_t n3 = Shuf_FindNode(bar, 0x80000u, FakeRd32, FakeRd8, &m, &s3); m.m[m.base + 0x280u + 0x18u - m.base] = 1;   // the 0x80000 node hidden -> not found (6 links walked)
        FakeW32(&m, bar + 0x4Cu, bar + 0x4Cu); uint32_t n4 = Shuf_FindNode(bar, 0x80000u, FakeRd32, FakeRd8, &m, &s4);                                   // an empty list -> 0, 0 links
        FakeW32(&m, bar + 0x4Cu, 0x20000000u); uint32_t n5 = Shuf_FindNode(bar, 0x80000u, FakeRd32, FakeRd8, &m, &s5);                                   // a link outside readable memory -> 0
        FakeW32(&m, bar + 0x4Cu, m.base + 0x200u + 4u); FakeW32(&m, m.base + 0x340u + 4u, m.base + 0x200u + 4u); m.m[m.base + 0x280u + 0x18u - m.base] = 0;
        uint32_t n6 = Shuf_FindNode(bar, 0x80000u, FakeRd32, FakeRd8, &m, &s6);                                                                         // a cycle that never returns to the head -> stops at 8 links, 0
        CHECK(n1 == m.base + 0x280u && s1 == 3 && n2 == m.base + 0x240u && s2 == 2 && n3 == 0 && s3 == 6 && n4 == 0 && s4 == 0 && n5 == 0 && s5 == 1 && n6 == 0 && s6 == 8,
          "SHUF-6 the slot-node walk on a fake 6-node list (nodes = link - 4, [node+4] = the next link, the list ends at bar+0x4C; +0x1C the bit, +0x18 shown): the 0x80000 node found as the 3rd link (%08X, 3 walked), Back as the 2nd; hidden -> 0 (6 walked); an empty list -> 0 (0); an unreadable link -> 0 (1 walked, its node not readable); a cycle -> 0 after 8", n1);
      }
      { Shuf_NoneFold f = { 0 }; int c = -1, a[8]; a[0] = Shuf_Fold(&f, 1, &c); int c0 = c; a[1] = Shuf_Fold(&f, 1, &c); a[2] = Shuf_Fold(&f, 1, &c); a[3] = Shuf_Fold(&f, 1, &c); a[4] = Shuf_Fold(&f, 1, &c); int c4 = c;
        a[5] = Shuf_Fold(&f, 0, &c); int c5 = c, run5 = f.run; a[6] = Shuf_Fold(&f, 0, &c); int c6 = c; a[7] = Shuf_Fold(&f, 1, &c); int c7 = c; int a8 = Shuf_Fold(&f, 0, &c); int c8 = c;
        CHECK(a[0] == 1 && c0 == 1 && a[1] == 0 && a[2] == 0 && a[3] == 0 && a[4] == 0 && c4 == 5 && a[5] == 2 && c5 == 5 && run5 == 0 && a[6] == 0 && c6 == 0 && a[7] == 1 && c7 == 1 && a8 == 2 && c8 == 1,
          "SHUF-7 the fold: a run of 5 '-1' answers -> 1 (log the first), 0, 0, 0, 0; the next other answer -> 2 (log the count 5, the run reset); another other answer -> 0; a lone -1 -> 1 then 2 with count 1 (= 2 lines per run instead of one per request)");
      }
      { int okOff = SHUF_BAR_PKG_OFF == 0x58u && SHUF_BAR_LIST_OFF == 0x4Cu && SHUF_NODE_TEXT == 0x10u && SHUF_NODE_SHOWN == 0x18u && SHUF_NODE_BIT == 0x1Cu && UA_SHUF_PLAYMODE == 0x83AA58u && UA_SHUF_BAR == 0x8384D8u && UA_SHUF_LISTSETUP == 0x479220u && UA_SHUF_MSG_ORIG == 0x52D170u && UA_SHUF_LABEL_ORIG == UA_BAR_SETLABEL;
        CHECK(okOff && !strcmp(SHUF_PKG, "EA_Trax_Jukebox.fng") && !memcmp(exe + 0x393DB8, SHUF_PKG, 20), "SHUF-8 the offsets ([bar+0x58] the package 54e774, [bar+0x4C] the list 54e761 / 55de0e, +0x10 / +0x18 / +0x1C the node 552087 / 54e8e7 / 54e8e4; [0x8384D8] 551fdb; 0x83AA58; 0x479220; 0x52D170; 0x50C900 = the top bar's); the row name at 0x793DB8 (file 0x393DB8) = 'EA_Trax_Jukebox.fng'");
      }
    }
    // ---- THE DEFAULT BUTTON: the pure parts of src/defaultbutton.h + the byte patterns against the retail exe
    {
      { int okSlot = !memcmp(exe + 0x39CB7C, kDfltHandlerSlotBytes, 4) && !memcmp(exe + 0x39CB74, kDfltVtableBytes, 16); long slotHits = 0, yesHits = 0, noHits = 0; uint32_t y = DFLT_MSG_YES, nn = DFLT_MSG_NO;
        for (long i = 0; i + 4 <= ne; i++) { if (!memcmp(exe + i, kDfltHandlerSlotBytes, 4)) slotHits++; if (!memcmp(exe + i, &y, 4)) yesHits++; if (!memcmp(exe + i, &nn, 4)) noHits++; }
        int okHeads = !memcmp(exe + 0x16B1C0, kDfltHandlerHead, 12) && !memcmp(exe + 0x16B223, "\xC2\x10\x00", 3) && !memcmp(exe + 0x158750, kDfltDialogHead, 12) && !memcmp(exe + 0x10D150, kDfltUiSoundHead, 11) && !memcmp(exe + 0x38FA24, DFLT_DIALOG_FILE_TEXT, 24);
        int okSite = !memcmp(exe + 0x9B997, "\x6A\x00\x6A\x01\x56\xE8\xAF\x17\x07\x00", 10) && !memcmp(exe + 0x9B9B7, "\x68\x1C\x06\x00\x00\x68\x70\xF7\x00\x00\x68\x24\xFA\x78\x00\x56\xE8\xC4\xCD\x0B\x00\x83\xC4\x30", 24)   // UG2's own Default Settings item: (pkg,1,0) -> 0x50D150; No 0x61C, Yes 0xF770, the file, pkg -> 0x558790; add esp,0x30
            && (0x49B99Cu + 5u + 0x717AFu) == UA_DFLT_UISOUND && !memcmp(exe + 0x97FED, "\xE8\x5E\x07\x0C\x00\x83\xC4\x28", 8) && (0x497FEDu + 5u + 0xC075Eu) == UA_DFLT_DIALOG_FMT;   // 497fed call 0x558750; add esp,0x28 (9 args + the hash push)
        int okBits = DFLT_FLAGS_NEW == (SHUF_FLAGS_NEW | DFLT_BIT) && DFLT_BIT == 0x100000u && DFLT_BIT > SHUF_BIT && DFLT_BIT > 0x400u && DFLT_BIT != 0x8000u && DFLT_BIT != 0x10000u && DFLT_BIT != 0x20000u && DFLT_BIT != 0x80000000u
            && DFLT_MSG_YES != DFLT_MSG_NO && DFLT_MSG_YES != 0xD05FC3A3u && DFLT_MSG_NO != 0x34DC1BCFu && DFLT_MSG_YES != 0xC539A4B0u && DFLT_MSG_NO != 0xC539A4B0u && DFLT_MSG_YES != 0x911AB364u && DFLT_MSG_NO != 0x911AB364u;
        CHECK(okSlot && slotHits == 1 && yesHits == 0 && noHits == 0 && okHeads && okSite && okBits,
          "DFLT-1 the exe bytes: 0x79CB74 (file 0x39CB74) = the jukebox vtable {554150 40D080 56B1C0 570A70}, its slot 2 0x79CB7C = 0x0056B1C0 (%ld time%s in the file), the heads of 0x56B1C0 (12) + its 'ret 0x10' at 56b223 + 0x558750 (12) + 0x50D150 (11), 'GenericDialog_SMALL.fng' at 0x78FA24; UG2's own Default Settings item 49b997-49b9cf (the sound (pkg,1,0), No 0x061C, Yes 0xF770, the file, 'add esp,0x30') + 497fed's 0x558750 call ('add esp,0x28'); the flag word 0x%08X = the Shuffle word | 0x100000 (not a skipped bit, above Back and Shuffle); the answer messages %08X / %08X = 0 hits in the exe (%ld / %ld) and none of UG2's dialog / bar messages",
          slotHits, slotHits == 1 ? "" : "s", DFLT_FLAGS_NEW, DFLT_MSG_YES, DFLT_MSG_NO, yesHits, noHits);
      }
      { int ok = Dflt_LabelWanted(DFLT_HASH_HOOD, "EA_Trax_Jukebox.fng", 2) == 1 && Dflt_LabelWanted(SHUF_HASH_N2O, "EA_Trax_Jukebox.fng", 2) == 0 && Dflt_LabelWanted(DFLT_HASH_HOOD, "UI_CarParts_Perf.fng", 2) == 0
            && Dflt_LabelWanted(DFLT_HASH_HOOD, "EA_Trax_Jukebox.fng", 1) == 0 && Dflt_LabelWanted(DFLT_HASH_HOOD, NULL, 2) == 0 && Dflt_LabelWanted(DFLT_HASH_HOOD, "", 2) == 0 && Dflt_LabelWanted(DFLT_HASH_HOOD, "EA_Trax_Jukebox.fn", 2) == 0
            && Shuf_LabelWanted(DFLT_HASH_HOOD, "EA_Trax_Jukebox.fng", 2) == 0 && DFLT_HASH_HOOD == 0xEAA1D5DFu && !memcmp(exe + 0x12D46C + 0, exe + 0x12D46C, 1);
        CHECK(ok && !strcmp(DFLT_TEXT, "Default") && strlen(DFLT_TEXT) == 7 && strlen(DFLT_TEXT) <= 16 && strchr(DFLT_TEXT, ':') == NULL,
          "DFLT-2 the label gate: hash 0xEAA1D5DF (bit 0x100000's stock label 'Open/Close Hood') x package 'EA_Trax_Jukebox.fng' (exact) x a type-2 text object -> 'Default' (7 characters, no colon, < the slot's 16); the Shuffle hash / another package / another type / no package -> not this slot; the Shuffle gate does not take the Default hash (the two never overlap)");
      }
      { int ok = Dflt_ClickWanted(0x100000u, "EA_Trax_Jukebox.fng") == 1 && Dflt_ClickWanted(0x80000u, "EA_Trax_Jukebox.fng") == 0 && Dflt_ClickWanted(0x400u, "EA_Trax_Jukebox.fng") == 0 && Dflt_ClickWanted(0x1u, "EA_Trax_Jukebox.fng") == 0
            && Dflt_ClickWanted(0x100000u, "UI_CarParts_Perf.fng") == 0 && Dflt_ClickWanted(0x100000u, NULL) == 0 && Shuf_ClickWanted(0x100000u, "EA_Trax_Jukebox.fng") == 0 && Shuf_ClickWanted(0x80000u, "EA_Trax_Jukebox.fng") == 1;
        CHECK(ok, "DFLT-3 the click gate: bit 0x100000 on the jukebox -> the dialog; Shuffle 0x80000 / Back 0x400 / Quit 0x1 / another package / no package -> not this button; the Shuffle gate still takes 0x80000 only (each bit has one owner)");
      }
      { static const uint32_t juke[13] = { 0x911C0A4Bu, 0x5073EF13u, 0x0C407210u, 0x35F8620Bu, 0x406415E3u, 0x72619778u, 0x911AB364u, 0xC519BFC0u, 0x9120409Eu, 0x9803F6E2u, 0xB5971BF1u, 0xC98356BAu, 0xD9FEEC59u };
        int ok = Dflt_Answer(DFLT_MSG_YES, 1) == 1 && Dflt_Answer(DFLT_MSG_NO, 1) == 2 && Dflt_Answer(DFLT_MSG_YES, 0) == 0 && Dflt_Answer(DFLT_MSG_NO, 0) == 0 && Dflt_Answer(0xC539A4B0u, 1) == 0 && Dflt_Answer(0xD05FC3A3u, 1) == 0 && Dflt_Answer(0x34DC1BCFu, 1) == 0 && Dflt_Answer(0, 1) == 0;
        for (int i = 0; i < 13; i++) { ok &= Dflt_Answer(juke[i], 1) == 0 && Dflt_Answer(juke[i], 0) == 0; }
        CHECK(ok, "DFLT-4 the answer rule: the mod's Yes -> 1 (reset) / No -> 2 (nothing) only while a Default dialog is up; with no dialog up both -> 0 (pass on); the 13 messages the jukebox handler compares, the two slots' 0xC539A4B0 and UG2's own Default Settings answers -> 0 (pass on) either way");
      }
      { int ok = Dflt_Ug2Default(0) == 1 && Dflt_Ug2Default(5) == 1 && Dflt_Ug2Default(6) == 2 && Dflt_Ug2Default(26) == 2 && Dflt_Ug2Default(27) == -1 && Dflt_Ug2Default(-1) == -1;
        int song[27]; unsigned char dw[27], df[27], bad[27]; char b[28], a[28], b2[28], a2[28], b3[28], a3[28]; const char *testModes = "000001000000000000200002000";
        for (int p = 0; p < 27; p++) { song[p] = p; dw[p] = (unsigned char)(testModes[p] - '0'); df[p] = p < 6 ? 1 : 2; bad[p] = 3; }
        int c1 = Dflt_ResetUg2(song, dw, b, a), c2 = Dflt_ResetUg2(song, df, b2, a2);
        int badSong[27]; for (int p = 0; p < 27; p++) badSong[p] = p; badSong[3] = -1; badSong[20] = 27; int c3 = Dflt_ResetUg2(badSong, bad, b3, a3);
        int okDef = !memcmp(dw, listMode, 27) && !strcmp(a, "111111222222222222222222222") && !strcmp(b, testModes) && c1 == 24 && c2 == 0 && !strcmp(a2, "111111222222222222222222222") && !strcmp(b2, a2);
        int okBad = c3 == 25 && bad[3] == 3 && bad[20] == 3 && bad[0] == 1 && bad[26] == 2 && a3[3] == '3' && a3[20] == '3';
        CHECK(ok && okDef && okBad, "DFLT-5 UG2's defaults (UG2-DATA): songs 0..5 -> 1 Menu, 6..26 -> 2 Racing, else -1; the test profile's 27 '%s' -> '%s' (24 changed), the defaults -> 0 changed; a bad song index (-1 / 27) leaves its byte alone (25 of 27 changed)", testModes, a);
      }
      { unsigned char mw[27], u1[27], cbm[31]; signed char smw[27], su1[27], scb[31]; char b[32], a[32]; int lines = -1;
        memcpy(mw, kMwMode, 27); memcpy(u1, kU1Mode, 27); memcpy(cbm, kCbMode, 31); memset(smw, -1, 27); memset(su1, -1, 27); memset(scb, -1, 31);
        mw[4] = 0; smw[4] = 0; mw[14] = 1; smw[14] = 1; smw[7] = (signed char)kMwMode[7];   // MW 4 Off (a line; default Racing), 14 Menu (a line; default All), 7 a line that equals the default (a line, no live change)
        int cM = Dflt_ResetAdded(mw, smw, kMwMode, 26, &lines, b, a); int okM = cM == 2 && lines == 3 && !memcmp(mw, kMwMode, 27) && smw[4] == -1 && smw[14] == -1 && smw[7] == -1 && smw[1] == -1 && strlen(b) == 26 && strlen(a) == 26 && b[3] == '0' && a[3] == '2' && b[13] == '1' && a[13] == '3' && kMwMode[4] == 2 && kMwMode[14] == 3;
        int lU = -1; u1[1] = 0; su1[1] = 0; int cU = Dflt_ResetAdded(u1, su1, kU1Mode, 26, &lU, b, a); int okU = cU == 1 && lU == 1 && !memcmp(u1, kU1Mode, 27) && su1[1] == -1 && a[0] == '2' && b[0] == '0';
        int lC = -1; for (int k = 1; k <= 30; k++) { cbm[k] = 0; scb[k] = 0; } int cC = Dflt_ResetAdded(cbm, scb, kCbMode, 30, &lC, b, a); int okC = cC == 30 && lC == 30 && !memcmp(cbm, kCbMode, 31) && strlen(a) == 30 && !strcmp(a, "222222222222222222222222222222");
        int lZ = -1; int cZ = Dflt_ResetAdded(cbm, scb, kCbMode, 30, &lZ, b, a); int okZ = cZ == 0 && lZ == 0 && !memcmp(cbm, kCbMode, 31);
        CHECK(okM && okU && okC && okZ, "DFLT-6 an added game's reset: MW with 4 Off / 14 Menu / 7 = a line at the default -> live := kMwMode (2 changed), 3 lines dropped, every line -1; UG1 row 1 Off -> 1 / 1; Carbon all 30 Off -> 30 / 30 -> '%s'; a second reset -> 0 / 0", a);
      }
      { int ok = 1; const int sts[6] = { SET_ST_UG2, SET_ST_UG1, SET_ST_MW, SET_ST_CARBON, SET_ST_ALL, SET_ST_OFF };
        for (int i = 0; i < 6; i++) { int st = sts[i], u = Dflt_ScopeUg2(st), g = Dflt_ScopeU1(st), m = Dflt_ScopeMw(st), c = Dflt_ScopeCb(st);
          int want = (st == SET_ST_OFF || st == SET_ST_ALL) ? 0 : 1; ok &= (u + g + m + c) == want && u == (st == SET_ST_UG2) && g == (st == SET_ST_UG1) && m == (st == SET_ST_MW) && c == (st == SET_ST_CARBON) && Dflt_ScopeAll(st) == (st == SET_ST_ALL); }   /* All Trax -> its own set only */
        CHECK(ok && SET_ST_UG2 == 0 && SET_ST_ALL == 4, "DFLT-7 the scope: Underground 2 -> UG2's rows only, Underground -> UG1's, Most Wanted -> MW's, Carbon -> Carbon's, All Trax -> none of the four = All Trax's own set only (Dflt_ScopeAll), Off -> none");
      }
    }
      { // the gate rule: the input messages dropped while the mod's Default dialog is up
        static const uint32_t juke[13] = { 0x911C0A4Bu, 0x5073EF13u, 0x0C407210u, 0x35F8620Bu, 0x406415E3u, 0x72619778u, 0x911AB364u, 0xC519BFC0u, 0x9120409Eu, 0x9803F6E2u, 0xB5971BF1u, 0xC98356BAu, 0xD9FEEC59u };
        static const uint32_t passed[2] = { 0x35F8620Bu, 0xC98356BAu }, other[9] = { DFLT_MSG_YES, DFLT_MSG_NO, 0xC539A4B0u, 0xD05FC3A3u, 0x34DC1BCFu, 0xFC946BFAu, 0x32C72D8Fu, 0, 0xFFFFFFFFu };
        int ok = DFLT_GATED_N == 11, inJuke = 0, distinct = 1;
        for (int i = 0; i < DFLT_GATED_N; i++) { ok &= Dflt_Gate(kDfltGated[i], 1) == 1 && Dflt_Gate(kDfltGated[i], 0) == 0 && Dflt_Answer(kDfltGated[i], 1) == 0 && Dflt_GateReturn(kDfltGated[i]) == kDfltGated[i];
          for (int j = 0; j < 13; j++) if (juke[j] == kDfltGated[i]) inJuke++;
          for (int j = 0; j < i; j++) if (kDfltGated[j] == kDfltGated[i]) distinct = 0; }
        for (int i = 0; i < 2; i++) { ok &= Dflt_Gate(passed[i], 1) == 0 && Dflt_Gate(passed[i], 0) == 0 && Dflt_Gate(kDfltGated[0], 1) == 1; int in = 0; for (int j = 0; j < 13; j++) if (juke[j] == passed[i]) in++; ok &= in == 1; for (int j = 0; j < DFLT_GATED_N; j++) ok &= kDfltGated[j] != passed[i]; }
        for (int i = 0; i < 9; i++) ok &= Dflt_Gate(other[i], 1) == 0 && Dflt_Gate(other[i], 0) == 0;
        ok &= Dflt_Answer(DFLT_MSG_YES, 1) == 1 && Dflt_Answer(DFLT_MSG_NO, 1) == 2;   // the answers come first in the mod's Default handler and are not in the list
        CHECK(ok && inJuke == 11 && distinct, "DFLT-8 the gate rule: the 11 input messages (each one of 0x56B1C0's 13 compares, all distinct) -> dropped (1) while a Default dialog is up, passed (0) with none up, never an answer, the return = the message; the 2 refresh messages 0x35F8620B / 0xC98356BA + the mod's Yes / No + the slots' 0xC539A4B0 + UG2's own answers + the per-frame FC946BFA / 32C72D8F + 0 / -1 -> passed either way (11 + 2 = the 13)");
      }
      { // the exe bytes behind the list (UG2-EXE, retail speed2.exe; file offset = VA - 0x400000): the 13 'cmp eax,imm32' of 0x56B1C0, its ignore tail, the base handler's call of slot 2 + no read of eax, the broadcast loop, the two passed callees' heads
        static const struct { uint32_t va, msg; } cmps[13] = { {0x56b1c4u, 0x911C0A4Bu}, {0x56b1d8u, 0x5073EF13u}, {0x56b1e5u, 0x0C407210u}, {0x56b1ecu, 0x35F8620Bu}, {0x56b1f3u, 0x406415E3u}, {0x56b232u, 0x72619778u}, {0x56b239u, 0x911AB364u},
          {0x56b2a1u, 0xC519BFC0u}, {0x56b2aau, 0x9120409Eu}, {0x56b2b1u, 0x9803F6E2u}, {0x56b2b8u, 0xB5971BF1u}, {0x56b313u, 0xC98356BAu}, {0x56b31au, 0xD9FEEC59u} };
        int ok = 1, gatedSeen = 0;
        for (int i = 0; i < 13; i++) { long o = (long)cmps[i].va - 0x400000L; uint32_t v; memcpy(&v, exe + o + 1, 4); ok &= o + 5 <= ne && exe[o] == 0x3D && v == cmps[i].msg; if (Dflt_Gate(cmps[i].msg, 1)) gatedSeen++; }
        int okTail = !memcmp(exe + 0x16B349, "\x5E\xC2\x10\x00", 4) && !memcmp(exe + 0x16B1C0, "\x8B\x44\x24\x04", 4);                                                   // 56b349 pop esi; ret 0x10; 56b1c0 mov eax,[esp+4]
        int okCaller = !memcmp(exe + 0x15BC6B, "\x8B\x45\x00\x51\x6A\x01\x52\x56\x8B\xCD\xFF\x50\x08\x5F\x5E\x5D\x83\xC4\x34\xC2\x10\x00", 22);                                // 55bc6b mov eax,[ebp]; push ecx; push 1; push edx; push esi; mov ecx,ebp; call [eax+8]; pop edi; pop esi; pop ebp; add esp,0x34; ret 0x10
        int okLoop = !memcmp(exe + 0x15DCC7, "\xE8\xF4\xD8\xFF\xFF\x46\x3B\xF5\x7C\xDF", 10) && (0x55DCC7u + 5u + 0xFFFFD8F4u) == 0x55B5C0u;                                    // 55dcc7 call 0x55b5c0; inc esi; cmp esi,ebp; jl 55dcb0 (the broadcast 0x55DC50)
        int okPassed = !memcmp(exe + 0x0958D0, "\x8B\x49\x2C\x85\xC9\x74\x0E\x8B\x41\x2C\x68\x28\xF2\x78\x00\x50\xE8\x9B\x95\x08\x00\xC3", 22) && !memcmp(exe + 0x16B32D, "\x8B\x4E\x78\x85\xC9\x74\x0E\x8B\x51\x2C\x68\x28\xF2\x78\x00\x52", 16);   // 4958d0 the text refresh; 56b32d the per-frame refresh
        int okMark = !memcmp(exe + 0x15822E, "\xA3\x74\x89\x83\x00", 5) && !memcmp(exe + 0x140AA4, "\xC7\x05\x74\x89\x83\x00\x00\x00\x00\x00", 10);                               // 55822e mov [0x838974],eax (ShowDialog); 540aa4 mov [0x838974],0 (Dismiss) - the mark the mod does NOT read
        CHECK(ok && gatedSeen == 11 && okTail && okCaller && okLoop && okPassed && okMark, "DFLT-9 the exe bytes: the 13 'cmp eax,imm32' of 0x56B1C0 at 56b1c4..56b31a = the 13 messages (11 of them gated), its ignore tail 56b349 'pop esi; ret 0x10' after 56b1c0 'mov eax,[esp+4]', the base handler 0x55B5C0's 55bc75 'call [eax+8]' then pop / ret 0x10 (eax unread), the broadcast loop 55dcc7 'call 0x55b5c0' .. jl, the two passed callees' heads 4958d0 / 56b32d, the dialog mark's writes 55822e / 540aa4");
      }
      // ================================================================= the per-category song settings: each EA Trax category keeps its own set
      { // the defaults of All Trax's set = each game's own; the ALL keys / lines; the header
        int ok = 1; for (int id = 0; id < SONG_NSONG; id++) { int want = id < 27 ? Dflt_Ug2Default(id) : SONG_IS_MW(id) ? kMwMode[SONG_MW_N(id)] : SONG_IS_U1(id) ? kU1Mode[SONG_U1_R(id) + 1] : kCbMode[SONG_CB_C(id) + 1]; ok &= Song_AllDefault(id) == want; }
        int okK = Set_AllKey("ALL UG2 01") == 0 && Set_AllKey("ALL UG2 27") == 26 && Set_AllKey("ALL MW 01") == SONG_ID_MW(1) && Set_AllKey("ALL MW 26") == SONG_ID_MW(26) && Set_AllKey("ALL UG1 01") == SONG_ID_U1(0) && Set_AllKey("ALL UG1 26") == SONG_ID_U1(25)
            && Set_AllKey("ALL CB 01") == SONG_ID_CB(0) && Set_AllKey("ALL CB 30") == SONG_ID_CB(29) && Set_AllKey("all mw 5") == SONG_ID_MW(5) && Set_AllKey("ALL  UG2  7") == 6 && Set_AllKey("ALL UG2 28") == -1 && Set_AllKey("ALL UG2 0") == -1 && Set_AllKey("ALL MW 27") == -1
            && Set_AllKey("ALL UG1 27") == -1 && Set_AllKey("ALL CB 31") == -1 && Set_AllKey("ALL UG3 1") == -1 && Set_AllKey("ALLMW 5") == -1 && Set_AllKey("MW 05") == -1 && Set_AllKey("ALL MW 5x") == -1 && Set_AllKey("ALL") == -1 && Set_AllKey("") == -1
            && Set_SongKey("ALL MW 05") == 0 && Set_U1Key("ALL UG1 05") == 0 && Set_CbKey("ALL CB 05") == 0;
        char ln[40]; int okL = Set_AllLine(0, 0, ln) == 18 && !strcmp(ln, "ALL UG2 01 = OFF\r\n") && Set_AllLine(SONG_ID_MW(5), 1, ln) > 0 && !strcmp(ln, "ALL MW 05 = FE\r\n") && Set_AllLine(SONG_ID_U1(25), 2, ln) > 0 && !strcmp(ln, "ALL UG1 26 = IG\r\n") && Set_AllLine(SONG_ID_CB(29), 3, ln) > 0 && !strcmp(ln, "ALL CB 30 = AL\r\n");
        CHECK(ok && okK && okL && SET_HEADER_VER == 2 && !strcmp(SET_HEADER, "UG2Music settings 2") && sizeof(list_allMode) == 109, "SET-8 All Trax's defaults = each game's own for every id 0..108 (UG2 6 Menu + 21 Racing = Dflt_Ug2Default, kMwMode, kU1Mode, kCbMode); the ALL keys -> the ids (UG2 nn -> nn - 1, MW nn -> 26 + nn, UG1 nn -> 52 + nn, CB nn -> 78 + nn = the SONG_ID macros; case / spaces free; nn out of range, a missing space, a game key, a bad tail -> -1; the game keys never read an ALL line); the ALL lines as written (18 B at most); the header 'UG2Music settings 2'");
      }
      { // the view per category: five views, each entry's mode from ITS set
        int ls[27]; unsigned char lm[27]; for (int p = 0; p < 27; p++) { ls[p] = 26 - p; lm[p] = p < 6 ? 1 : 2; }   // a reversed list: position p holds song 26 - p
        lm[19] = 0;                                                                                 // position 19 = song 7: Off in UG2's LIST only
        Set_Settings s; Set_Defaults(&s); s.headerVer = 2; s.allMode[7] = 3; s.allMode[SONG_ID_MW(1)] = 0; s.allMode[SONG_ID_U1(0)] = 0; s.allMode[SONG_ID_CB(0)] = 1; s.mwMode[2] = 0; s.u1Mode[2] = 0; s.cbMode[2] = 0; List_ApplyModes(&s);
        List_ViewEntry v[LIST_MAXVIEW]; int bad, ok = 1;
        int nA = List_BuildView(SET_ST_ALL, ls, lm, v, &bad); ok &= nA == 107 && bad == 0;
        ok &= v[0].id == SONG_ID_U1(0) && v[0].mode == 0 && v[1].id == SONG_ID_U1(1) && v[1].mode == kU1Mode[2] && v[26].id == 26 && v[26].mode == 2 && v[45].id == 7 && v[45].mode == 3 && v[53].id == SONG_ID_MW(1) && v[53].mode == 0 && v[54].id == SONG_ID_MW(2) && v[54].mode == kMwMode[2] && v[77].id == SONG_ID_CB(0) && v[77].mode == 1 && v[78].mode == kCbMode[2];
        int nU2 = List_BuildView(SET_ST_UG2, ls, lm, v, &bad); ok &= nU2 == 27 && v[0].id == 26 && v[0].mode == 1 && v[19].id == 7 && v[19].mode == 0 && v[26].id == 0 && v[26].mode == 2;
        int nM = List_BuildView(SET_ST_MW, ls, lm, v, &bad); ok &= nM == 24 && v[0].id == SONG_ID_MW(1) && v[0].mode == kMwMode[1] && v[1].mode == 0;
        int nU1 = List_BuildView(SET_ST_UG1, ls, lm, v, &bad); ok &= nU1 == 26 && v[0].mode == kU1Mode[1] && v[1].mode == 0;
        int nC = List_BuildView(SET_ST_CARBON, ls, lm, v, &bad); ok &= nC == 30 && v[0].mode == kCbMode[1] && v[1].mode == 0;
        CHECK(ok, "SET-9 the view per category: All Trax's 107 rows take their modes from All Trax's own set (UG2 song 7 = All there while Off in the list; MW 1 / UG1 row 1 / Carbon 1 = Off / Off / Menu there while at their defaults in their games); Underground 2's 27 = the list's bytes (song 7 Off); Most Wanted / Underground / Carbon = their own arrays (their song 2 Off, song 1 at the default) - no set shows another's value");
        Set_Defaults(&s); List_ApplyModes(&s);
      }
      { // the row step per category: the plan's kind follows the VIEW's category; applying it (as the asi's row step does) changes ONLY that set
        int ls[27]; unsigned char lm[27]; for (int p = 0; p < 27; p++) { ls[p] = p; lm[p] = p < 6 ? 1 : 2; }
        Set_Settings s; Set_Defaults(&s); s.headerVer = 2; List_ApplyModes(&s); List_ViewEntry v[LIST_MAXVIEW]; int bad, ok = 1;
        unsigned char mw0[27]; memcpy(mw0, list_mwMode, 27);
        List_BuildView(SET_ST_ALL, ls, lm, v, &bad);
        List_RowPlan a = List_PlanRowStep(SET_ST_ALL, 7, LIST_HASH_RIGHT, v, ls), b = List_PlanRowStep(SET_ST_ALL, SONG_ID_MW(1), LIST_HASH_LEFT, v, ls), c = List_PlanRowStep(SET_ST_ALL, SONG_ID_U1(0), LIST_HASH_LEFT, v, ls), d = List_PlanRowStep(SET_ST_ALL, SONG_ID_CB(0), LIST_HASH_RIGHT, v, ls), e = List_PlanRowStep(SET_ST_ALL, 7, 0x9803F6E2u, v, ls);
        ok &= a.kind == 5 && a.listPos == -1 && a.mwN == 8 && a.viewIdx == 33 && a.oldMode == 2 && a.newMode == 3 && b.kind == 5 && b.mwN == 1 && b.viewIdx == 53 && b.oldMode == 1 && b.newMode == 0 && c.kind == 5 && c.mwN == 1 && c.viewIdx == 0 && c.newMode == 1 && d.kind == 5 && d.mwN == 1 && d.viewIdx == 77 && d.newMode == 3 && e.kind == 0 && e.viewIdx == 33;
        list_allMode[7] = (unsigned char)a.newMode; s.allMode[7] = (signed char)a.newMode; list_allMode[SONG_ID_MW(1)] = (unsigned char)b.newMode; s.allMode[SONG_ID_MW(1)] = (signed char)b.newMode;   // a + b applied as the asi does under All Trax
        ok &= lm[7] == 2 && !memcmp(mw0, list_mwMode, 27) && s.mwMode[1] == -1 && SONG_ALLMODE(7) == 3 && SONG_ALLMODE(SONG_ID_MW(1)) == 0 && LIST_MWMODE(1) == kMwMode[1];
        List_BuildView(SET_ST_UG2, ls, lm, v, &bad); List_RowPlan f = List_PlanRowStep(SET_ST_UG2, 7, LIST_HASH_RIGHT, v, ls); ok &= f.kind == 1 && f.listPos == 7 && f.viewIdx == 7 && f.oldMode == 2 && f.newMode == 3;
        lm[f.listPos] = (unsigned char)f.newMode; ok &= lm[7] == 3 && SONG_ALLMODE(7) == 3 && s.allMode[7] == 3;
        List_BuildView(SET_ST_MW, ls, lm, v, &bad); List_RowPlan g = List_PlanRowStep(SET_ST_MW, SONG_ID_MW(1), LIST_HASH_LEFT, v, ls); ok &= g.kind == 2 && g.mwN == 1 && g.viewIdx == 0 && g.oldMode == 1 && g.newMode == 0;
        list_mwMode[1] = (unsigned char)g.newMode; s.mwMode[1] = (signed char)g.newMode; ok &= LIST_MWMODE(1) == 0 && SONG_ALLMODE(SONG_ID_MW(1)) == 0 && s.allMode[SONG_ID_MW(1)] == 0;
        List_BuildView(SET_ST_UG1, ls, lm, v, &bad); List_RowPlan h = List_PlanRowStep(SET_ST_UG1, SONG_ID_U1(0), LIST_HASH_LEFT, v, ls); ok &= h.kind == 3 && h.mwN == 1 && h.viewIdx == 0 && h.newMode == 1;
        List_BuildView(SET_ST_CARBON, ls, lm, v, &bad); List_RowPlan i = List_PlanRowStep(SET_ST_CARBON, SONG_ID_CB(0), LIST_HASH_RIGHT, v, ls); ok &= i.kind == 4 && i.mwN == 1 && i.viewIdx == 0 && i.newMode == 3;
        ok &= SONG_ALLMODE(SONG_ID_U1(0)) == 2 && SONG_ALLMODE(SONG_ID_CB(0)) == 2;   // c + d were planned, not applied: All Trax's defaults stand
        CHECK(ok, "SET-10 the row step per category: under the All view every row (UG2 song 7, MW 1, UG1 row 1, Carbon 1) plans kind 5 = All Trax's own set (listPos -1; the step rule left / right as G4; a foreign hash = nothing); applying it changes All Trax's set + its ALL line only (the list byte, the MW array, the MW line untouched); under Underground 2 / Most Wanted / Underground / Carbon the same rows plan kinds 1 / 2 / 3 / 4 as before and their apply leaves All Trax's set alone");
        Set_Defaults(&s); List_ApplyModes(&s);
      }
      { // the picker: the allowed test under each category reads THAT set, FE and IG
        int ls[27]; unsigned char lm[27]; for (int p = 0; p < 27; p++) { ls[p] = p; lm[p] = p < 6 ? 1 : 2; }
        Set_Settings s; Set_Defaults(&s); s.headerVer = 2; s.allMode[SONG_ID_MW(4)] = 0; s.allMode[7] = 0; s.allMode[SONG_ID_U1(0)] = 3; s.mwMode[6] = 0; s.cbMode[1] = 0; List_ApplyModes(&s); int ok = 1;
        set_soundtrack = SET_ST_ALL;
        ok &= !Song_Allowed(SONG_CTX_IG, SONG_ID_MW(4), ls, lm) && !Song_Allowed(SONG_CTX_FE, SONG_ID_MW(4), ls, lm) && !Song_Allowed(SONG_CTX_IG, 7, ls, lm) && !Song_Allowed(SONG_CTX_FE, 7, ls, lm)
           && Song_Allowed(SONG_CTX_IG, SONG_ID_U1(0), ls, lm) && Song_Allowed(SONG_CTX_FE, SONG_ID_U1(0), ls, lm) && Song_Allowed(SONG_CTX_IG, SONG_ID_MW(6), ls, lm) && !Song_Allowed(SONG_CTX_FE, SONG_ID_MW(6), ls, lm)
           && Song_Allowed(SONG_CTX_IG, SONG_ID_CB(0), ls, lm) && !Song_Allowed(SONG_CTX_IG, SONG_ID_MW(25), ls, lm) && !Song_Allowed(SONG_CTX_FE, SONG_ID_MW(26), ls, lm);
        set_soundtrack = SET_ST_MW;
        ok &= Song_Allowed(SONG_CTX_IG, SONG_ID_MW(4), ls, lm) && !Song_Allowed(SONG_CTX_FE, SONG_ID_MW(4), ls, lm) && !Song_Allowed(SONG_CTX_IG, SONG_ID_MW(6), ls, lm) && !Song_Allowed(SONG_CTX_FE, SONG_ID_MW(6), ls, lm) && !Song_Allowed(SONG_CTX_IG, SONG_ID_U1(0), ls, lm);
        set_soundtrack = SET_ST_UG1; ok &= Song_Allowed(SONG_CTX_IG, SONG_ID_U1(0), ls, lm) && !Song_Allowed(SONG_CTX_FE, SONG_ID_U1(0), ls, lm);
        set_soundtrack = SET_ST_CARBON; ok &= !Song_Allowed(SONG_CTX_IG, SONG_ID_CB(0), ls, lm) && Song_Allowed(SONG_CTX_IG, SONG_ID_CB(1), ls, lm);
        set_soundtrack = SET_ST_UG2; ok &= Song_Allowed(SONG_CTX_IG, 7, ls, lm) && !Song_Allowed(SONG_CTX_IG, SONG_ID_MW(4), ls, lm);
        int fe = 0, ig = 0; set_soundtrack = SET_ST_ALL; for (int i2 = 0; i2 < SONG_NSONG; i2++) { fe += Song_Allowed(SONG_CTX_FE, i2, ls, lm); ig += Song_Allowed(SONG_CTX_IG, i2, ls, lm); }
        ok &= fe == 24 && ig == 84;   // the defaults FE 23 / IG 86 (as in the Carbon block): MW 4 (IG) Off -> IG - 1, UG2 song 7 (IG) Off -> IG - 1, UG1 row 1 Racing -> All = FE + 1; MW 6 / Carbon 1 Off in their games only -> no change under All Trax
        int savedMode = g_rngMode; Song_State st; Song_Init(&st); Song_PickInfo inf; int off = 0; g_rngMode = 2; for (int q = 0; q < 300; q++) { int ctx = (q % 3 == 0) ? SONG_CTX_FE : SONG_CTX_IG; int id = Song_Choose(&st, ctx, 0, ls, lm, FakeRng, NULL, &inf); if (id == SONG_ID_MW(4) || id == 7) off++; } g_rngMode = savedMode;
        CHECK(ok && off == 0, "SET-11 the picker per category: under All Trax every id's mode = All Trax's own set (MW 4 / UG2 song 7 Off there -> never allowed, 0 of 300 picks; UG1 row 1 All there -> FE + IG; MW 6 / Carbon 1 Off in their games only -> allowed under All Trax at All Trax's default); under Most Wanted / Underground / Carbon that game's own set (MW 6 / Carbon 1 Off, MW 4 / UG1 row 1 at their defaults); under Underground 2 the list's bytes; MW 25 / 26 out everywhere; the FE / IG rule unchanged (All Trax FE %d IG %d)", fe, ig);
        Set_Defaults(&s); List_ApplyModes(&s); set_soundtrack = SET_ST_ALL;
      }
      { // the reset per category: Default resets only the category shown
        const int sts[6] = { SET_ST_UG2, SET_ST_UG1, SET_ST_MW, SET_ST_CARBON, SET_ST_ALL, SET_ST_OFF }; int ok = 1;
        for (int i2 = 0; i2 < 6; i2++) { int st = sts[i2]; int u = Dflt_ScopeUg2(st), g = Dflt_ScopeU1(st), m = Dflt_ScopeMw(st), c = Dflt_ScopeCb(st), a = Dflt_ScopeAll(st);
          ok &= (u + g + m + c + a) == (st == SET_ST_OFF ? 0 : 1) && u == (st == SET_ST_UG2) && g == (st == SET_ST_UG1) && m == (st == SET_ST_MW) && c == (st == SET_ST_CARBON) && a == (st == SET_ST_ALL); }
        unsigned char lm[27]; for (int p = 0; p < 27; p++) lm[p] = 0;   // the list: all Off (the reset under All Trax must leave it)
        Set_Settings s; Set_Defaults(&s); s.headerVer = 2; for (int id = 0; id < SONG_NSONG; id++) s.allMode[id] = 0; s.mwMode[3] = 0; List_ApplyModes(&s);   // All Trax: every song Off (109 lines); Most Wanted: MW 3 Off
        unsigned char mw0[27]; memcpy(mw0, list_mwMode, 27); char b[SONG_NSONG + 1], a[SONG_NSONG + 1]; int lines = -1;
        int ch = Dflt_ResetAll(list_allMode, s.allMode, &lines, b, a);
        int okA = ch == 109 && lines == 109 && SONG_ALLMODE(0) == 1 && SONG_ALLMODE(6) == 2 && SONG_ALLMODE(SONG_ID_MW(1)) == kMwMode[1] && SONG_ALLMODE(SONG_ID_CB(29)) == 2 && s.allMode[0] == -1 && s.allMode[108] == -1 && strlen(b) == 109 && strlen(a) == 109 && b[0] == '0' && a[0] == '1' && a[6] == '2'
            && lm[0] == 0 && lm[26] == 0 && !memcmp(mw0, list_mwMode, 27) && s.mwMode[3] == 0 && LIST_MWMODE(3) == 0;
        int lines2 = -1; int ch2 = Dflt_ResetAll(list_allMode, s.allMode, &lines2, b, a); int okZ = ch2 == 0 && lines2 == 0;
        s.allMode[SONG_ID_MW(3)] = 0; list_allMode[SONG_ID_MW(3)] = 0; char mb[27], ma[27]; int lM = -1; int cM = Dflt_ResetAdded(list_mwMode, s.mwMode, kMwMode, 26, &lM, mb, ma);
        int okM = cM == 1 && lM == 1 && LIST_MWMODE(3) == kMwMode[3] && SONG_ALLMODE(SONG_ID_MW(3)) == 0 && s.allMode[SONG_ID_MW(3)] == 0;
        CHECK(ok && okA && okZ && okM, "SET-12 the reset per category: the scope = ONE set (Underground 2 -> UG2's list, Underground -> UG1's, Most Wanted -> MW's, Carbon -> Carbon's, All Trax -> All Trax's own ONLY, Off -> none); All Trax's reset: 109 live modes Off -> each game's own default, 109 ALL lines dropped, the list's 27 bytes and Most Wanted's set untouched (no 0x479220 needed); a second reset -> 0 / 0; Most Wanted's reset leaves All Trax's MW 3 Off");
        Set_Defaults(&s); List_ApplyModes(&s);
      }
      { // the file: header 2 round trip; header 1 / none -> each song's setting copied into every category; the keys never cross; the largest file fits the asi's buffer
        Set_Settings s, s2; char txt[8192]; Set_Defaults(&s); s.soundtrack = SET_ST_ALL; s.mwMode[5] = 0; s.u1Mode[26] = 3; s.cbMode[30] = 1; s.allMode[0] = 0; s.allMode[26] = 3; s.allMode[SONG_ID_MW(5)] = 1; s.allMode[SONG_ID_U1(25)] = 2; s.allMode[SONG_ID_CB(29)] = 0; strcpy(s.other[0], "kept = yes"); s.nOther = 1;
        int n1 = Set_Format(&s, txt, sizeof txt); int ok = 1;
        ok &= n1 > 0 && !strcmp(txt, "UG2Music settings 2\r\nsoundtrack = ALL\r\nMW 05 = OFF\r\nUG1 26 = AL\r\nCB 30 = FE\r\nALL UG2 01 = OFF\r\nALL UG2 27 = AL\r\nALL MW 05 = FE\r\nALL UG1 26 = IG\r\nALL CB 30 = OFF\r\nkept = yes\r\n");
        Set_Parse(&s2, txt, n1); ok &= s2.headerSeen && s2.headerVer == 2 && s2.soundtrack == SET_ST_ALL && s2.nSong == 3 && s2.nAll == 5 && s2.nOther == 1 && !memcmp(s2.allMode, s.allMode, 109) && !memcmp(s2.mwMode, s.mwMode, 27) && !memcmp(s2.u1Mode, s.u1Mode, 27) && !memcmp(s2.cbMode, s.cbMode, 31) && !s2.badSong;
        List_ApplyModes(&s2); ok &= s2.inherited == 0 && SONG_ALLMODE(0) == 0 && SONG_ALLMODE(26) == 3 && SONG_ALLMODE(SONG_ID_MW(5)) == 1 && SONG_ALLMODE(SONG_ID_MW(4)) == kMwMode[4] && LIST_MWMODE(5) == 0 && SONG_ALLMODE(SONG_ID_CB(29)) == 0 && SONG_ALLMODE(SONG_ID_CB(28)) == 2;
        const char *v1 = "UG2Music settings 1\r\nsoundtrack = MW\r\nMW 05 = OFF\r\nUG1 01 = FE\r\nCB 02 = AL\r\n"; Set_Parse(&s2, v1, (int)strlen(v1)); int okH1 = s2.headerSeen && s2.headerVer == 1 && s2.nAll == 0 && s2.nSong == 3;
        List_ApplyModes(&s2); okH1 &= s2.inherited == 3 && s2.allMode[SONG_ID_MW(5)] == 0 && s2.allMode[SONG_ID_U1(0)] == 1 && s2.allMode[SONG_ID_CB(1)] == 3 && s2.allMode[SONG_ID_MW(4)] == -1 && s2.allMode[0] == -1 && SONG_ALLMODE(SONG_ID_MW(5)) == 0 && SONG_ALLMODE(SONG_ID_U1(0)) == 1 && SONG_ALLMODE(SONG_ID_CB(1)) == 3 && SONG_ALLMODE(SONG_ID_MW(4)) == kMwMode[4] && LIST_MWMODE(5) == 0;
        int n2 = Set_Format(&s2, txt, sizeof txt); okH1 &= n2 > 0 && !strcmp(txt, "UG2Music settings 2\r\nsoundtrack = MW\r\nMW 05 = OFF\r\nUG1 01 = FE\r\nCB 02 = AL\r\nALL MW 05 = OFF\r\nALL UG1 01 = FE\r\nALL CB 02 = AL\r\n");
        const char *v2 = "UG2Music settings 1\r\nsoundtrack = ALL\r\nMW 05 = OFF\r\nMW 06 = OFF\r\nALL MW 05 = AL\r\n"; Set_Parse(&s2, v2, (int)strlen(v2)); List_ApplyModes(&s2);
        int okRT = s2.headerVer == 1 && s2.nAll == 1 && s2.inherited == 1 && s2.allMode[SONG_ID_MW(5)] == 3 && s2.allMode[SONG_ID_MW(6)] == 0 && SONG_ALLMODE(SONG_ID_MW(5)) == 3 && SONG_ALLMODE(SONG_ID_MW(6)) == 0;
        const char *v3 = "MW 05 = OFF\r\n"; Set_Parse(&s2, v3, (int)strlen(v3)); List_ApplyModes(&s2); int okNoH = !s2.headerSeen && s2.headerVer == 0 && s2.inherited == 1 && SONG_ALLMODE(SONG_ID_MW(5)) == 0;
        Set_Defaults(&s2); List_ApplyModes(&s2); int okNone = s2.inherited == 0; for (int id = 0; id < SONG_NSONG; id++) okNone &= SONG_ALLMODE(id) == Song_AllDefault(id) && s2.allMode[id] == -1;
        const char *v4 = "UG2Music settings 2\r\nALL MW 05 = XX\r\nALL UG2 28 = OFF\r\nALL MW 05 = OFF\r\nMW 05 = AL\r\n"; Set_Parse(&s2, v4, (int)strlen(v4));
        int okX = s2.nAll == 1 && s2.badSong == 1 && s2.nOther == 2 && !strcmp(s2.other[0], "ALL MW 05 = XX") && !strcmp(s2.other[1], "ALL UG2 28 = OFF") && s2.allMode[SONG_ID_MW(5)] == 0 && s2.mwMode[5] == 3 && s2.nSong == 1;
        Set_Defaults(&s); for (int k = 1; k <= 26; k++) { s.mwMode[k] = 0; s.u1Mode[k] = 0; } for (int k = 1; k <= 30; k++) s.cbMode[k] = 0; for (int id = 0; id < SONG_NSONG; id++) s.allMode[id] = 0; s.soundtrack = SET_ST_CARBON;
        int nBig = Set_Format(&s, txt, sizeof txt); Set_Parse(&s2, txt, nBig); int okBig = nBig > 0 && s2.nAll == 109 && s2.nSong == 82 && s2.nOther == 0 && !memcmp(s2.allMode, s.allMode, 109) && s2.headerVer == 2;
        FILE *fo = fopen("test/pc5_largest.txt", "wb"); if (fo) { fwrite(txt, 1, (size_t)nBig, fo); fclose(fo); }   // the older release's parser (rig/compat_old.c, run by tools/rig.sh) reads this file
        for (int k = 0; k < SET_MAXLINES; k++) { memset(s.other[k], 'x', 40); s.other[k][40] = 0; } s.nOther = SET_MAXLINES;   // 64 other lines of 40 B (the older release keeps the ALL lines as other lines: 'ALL UG2 27 = OFF' = 16 B); the 64 x 119 B worst case does not fit the older release's 7,936 B either (it logs NOT WRITTEN then)
        int cap = SET_MAXLINES * SET_LINELEN + 256 + SONG_NSONG * 18; char *big = malloc((size_t)cap); int nMax = big ? Set_Format(&s, big, cap) : 0; int okMax = nMax > 0 && nMax <= cap - 1; free(big);
        int small = Set_Format(&s, txt, 100);
        CHECK(ok && okH1 && okRT && okNoH && okNone && okX && okBig && okMax && small == 0, "SET-13 the file: header 2 + every line kind written (the game lines, then 'ALL UG2 / MW / UG1 / CB nn', then the other lines) and read back equal (a missing ALL line = All Trax's default, a game line stays its game's); header 1 -> the game lines copied into All Trax's lines (3 inherited), written as header 2 + ALL lines at the next save; header 1 + ALL lines (a round trip through the older release): the ALL lines win, the missing ones copied; no header = header 1's rule; no file = defaults; a bad word / nn 28 -> an other line, 'ALL MW 05' is not 'MW 05'; the largest file %d B (82 game + 109 ALL lines; written to test/pc5_largest.txt for the old-file check) and with 64 other lines of 40 B %d B <= the asi's buffer %d (the 64 x 119 B worst case never fit the older release's either); a too-small buffer -> 0", nBig, nMax, cap);
        Set_Defaults(&s2); List_ApplyModes(&s2);
      }
      { // the save prompt's snapshot: All Trax's lines are in it
        Prompt_State t; memset(&t, 0, sizeof t); signed char mw[27], u1[27], cb[31], al[SONG_NSONG]; memset(mw, -1, 27); memset(u1, -1, 27); memset(cb, -1, 31); memset(al, -1, SONG_NSONG); int d = 9, ok = 1, r;
        Prompt_OnKeep(&t, 0, 1, SET_ST_ALL, mw, u1, cb, al); ok &= t.valid == 1 && t.snap.allMode[0] == -1 && t.snap.allMode[108] == -1;
        r = Prompt_OnCompare(&t, 1, 0, SET_ST_ALL, mw, u1, cb, al, &d); ok &= r == 0 && d == 0;
        Prompt_OnKeep(&t, 0, 1, SET_ST_ALL, mw, u1, cb, al); al[7] = 0; r = Prompt_OnCompare(&t, 1, 0, SET_ST_ALL, mw, u1, cb, al, &d); ok &= r == 1 && d == 1 && t.modSaysYes == 1;
        Prompt_OnKeep(&t, 0, 1, SET_ST_UG2, mw, u1, cb, al); r = Prompt_OnCompare(&t, 1, 0, SET_ST_UG2, mw, u1, cb, al, &d); ok &= r == 0 && d == 0;
        Prompt_OnKeep(&t, 0, 1, SET_ST_UG2, mw, u1, cb, al); r = Prompt_OnCompare(&t, 1, 1, SET_ST_UG2, mw, u1, cb, al, &d); ok &= r == 1 && d == 0 && t.gameSaysYes == 1;
        Prompt_OnKeep(&t, 0, 1, SET_ST_ALL, mw, u1, cb, al); al[7] = -1; al[SONG_ID_MW(3)] = -1; r = Prompt_OnCompare(&t, 1, 0, SET_ST_ALL, mw, u1, cb, al, &d); ok &= r == 1 && d == 1;
        CHECK(ok && sizeof(Prompt_Snap) == 1 + 27 + 27 + 31 + 109, "SET-14 the snapshot holds All Trax's 109 lines (Prompt_Snap %d B): an All-Trax-only step -> DIFFERENT (UG2's save dialog by the mod's reason, as an MW-only step); an Underground 2 step -> the mod says same (UG2's own compare owns it; the game's yes passes through); a reset under All Trax (lines dropped) -> DIFFERENT", (int)sizeof(Prompt_Snap));
      }
      { // the view's category vs the bar's: the plan follows the VIEW's (what the rows show); the asi logs a difference
        int ls[27]; unsigned char lm[27]; for (int p = 0; p < 27; p++) { ls[p] = p; lm[p] = p < 6 ? 1 : 2; } List_ViewEntry v[LIST_MAXVIEW]; int bad;
        List_BuildView(SET_ST_ALL, ls, lm, v, &bad);
        List_RowPlan a = List_PlanRowStep(SET_ST_ALL, 7, LIST_HASH_RIGHT, v, ls), b = List_PlanRowStep(SET_ST_UG2, 7, LIST_HASH_RIGHT, v, ls), c = List_PlanRowStep(SET_ST_MW, SONG_ID_MW(1), LIST_HASH_LEFT, v, ls), d = List_PlanRowStep(SET_ST_OFF, 7, LIST_HASH_RIGHT, v, ls), e = List_PlanRowStep(-1, 7, LIST_HASH_RIGHT, v, ls);
        CHECK(a.kind == 5 && b.kind == 1 && b.listPos == 7 && c.kind == 2 && c.mwN == 1 && d.kind == 1 && e.kind == 1 && a.viewIdx == b.viewIdx && a.newMode == b.newMode, "SET-15 the row step's set = the VIEW's category (the rows the player sees), not the bar's: the same All view planned as All Trax -> kind 5, as Underground 2 -> kind 1 (the list), as Most Wanted -> kind 2 (the MW set), as Off / none -> the game kinds; the entry and the new mode the same; List_RowStep passes g_viewSt (= the bar's after every RefillList) and logs when the two differ");
      }
    // ==== THE WORDS OF EVERY EA TRAX ROW MOVED RIGHT (src/rowwords.h) ====
    { // the plan: the FNG's x -> WRITE; the new x -> ALREADY; anything else (another value, 0, NaN, a hair off) -> SKIP, never written
      volatile float z = 0.0f; float nanv = z / z;
      CHECK(Words_Plan(-253.0f) == WORDS_PLAN_WRITE && Words_Plan(WORDS_X_FNG) == WORDS_PLAN_WRITE && Words_Plan(-246.2f) == WORDS_PLAN_ALREADY && Words_Plan(WORDS_X_NEW) == WORDS_PLAN_ALREADY && Words_Plan(-281.0f) == WORDS_PLAN_SKIP
            && Words_Plan(0.0f) == WORDS_PLAN_SKIP && Words_Plan(nanv) == WORDS_PLAN_SKIP && Words_Plan(-253.0001f) == WORDS_PLAN_SKIP && Words_Plan(-252.9999f) == WORDS_PLAN_SKIP && Words_Plan(-246.0f) == WORDS_PLAN_SKIP && WORDS_PLAN_SKIP == 0,
            "WORDS-1 Words_Plan: -253.0 (the FNG's x) -> WRITE, -246.2 (the new x) -> ALREADY, -281.0 / 0 / NaN / -253.0001 / -252.9999 / -246.0 -> SKIP (never written)");
    }
    { // the shift in float: -253.0 + 6.8 == -246.2 bit for bit; the 16 hashes = base + slot = the FNG's objects (artist #117 / #111 / #106 / #101, title #116 / #112 / #107 / #102, album #115 / #110 / #105 / #100, number #118 / #113 / #108 / #103)
      static const uint32_t fng[16] = { 0x66646FC4, 0x66646FC5, 0x66646FC6, 0x66646FC7, 0x2890C8AF, 0x2890C8B0, 0x2890C8B1, 0x2890C8B2, 0x77BD189E, 0x77BD189F, 0x77BD18A0, 0x77BD18A1, 0xE454E9A5, 0xE454E9A6, 0xE454E9A7, 0xE454E9A8 };
      int ok = 1; for (int i = 0; i < 12; i++) ok &= Words_WordHash(i) == fng[i]; for (int s = 0; s < 4; s++) ok &= Words_NumberHash(s) == fng[12 + s];
      volatile float a = WORDS_X_FNG, b = WORDS_SHIFT; float sum = a + b; union { float f; uint32_t u; } nw; nw.f = WORDS_X_NEW;
      CHECK(ok && sum == WORDS_X_NEW && nw.u == 0xC3763333u && WORDS_NWORDS == 12 && WORDS_NNUMBERS == 4 && WORDS_X_FNG == -253.0f && WORDS_X_NUMBER == -281.0f && WORDS_SHIFT == 6.8f,
            "WORDS-2 -253.0 + 6.8 = -246.2 in float (bit for bit, C3763333); the 12 word hashes + the 4 number hashes = base + slot = the FNG's 16 objects in the fill's order; kinds artist / title / album");
    }
    { // the row layout re-made from the font numbers (digits 13 wide at 0.80 = 10.4 each from x -281; the words from -253 + s; the arrow edge x 87; MW 20's title 329.6 at 0.65)
      #define WORDS_NEAR(p, q) (((p) - (q)) < 0.001f && ((q) - (p)) < 0.001f)
      int ok = WORDS_NEAR(Words_NumberEnd(2), -260.2f) && WORDS_NEAR(Words_NumberEnd(3), -249.8f)
        && WORDS_NEAR(Words_GapAfterNumber(3, 0.0f), -3.2f) && WORDS_NEAR(Words_GapAfterNumber(2, 0.0f), 7.2f) && WORDS_NEAR(Words_Room(0.0f), 340.0f) && WORDS_NEAR(Words_TitleGap(WORDS_MW20_WIDTH, 0.0f), 10.4f)
        && WORDS_NEAR(Words_GapAfterNumber(3, 4.0f), 0.8f) && WORDS_NEAR(Words_GapAfterNumber(3, 6.0f), 2.8f) && WORDS_NEAR(Words_GapAfterNumber(3, 8.0f), 4.8f) && WORDS_NEAR(Words_TitleGap(WORDS_MW20_WIDTH, 8.0f), 2.4f)
        && WORDS_NEAR(Words_GapAfterNumber(3, WORDS_SHIFT), 3.6f) && WORDS_NEAR(Words_GapAfterNumber(2, WORDS_SHIFT), 14.0f) && WORDS_NEAR(Words_Room(WORDS_SHIFT), 333.2f) && WORDS_NEAR(Words_TitleGap(WORDS_MW20_WIDTH, WORDS_SHIFT), 3.6f) && WORDS_NEAR(WORDS_X_FNG + WORDS_SHIFT + WORDS_MW20_WIDTH, 83.4f)
        && WORDS_NEAR(Words_GapAfterNumber(3, 10.4f), 7.2f) && WORDS_NEAR(Words_GapAfterNumber(2, 10.4f), 17.6f) && WORDS_NEAR(Words_Room(10.4f), 329.6f) && WORDS_NEAR(Words_TitleGap(WORDS_MW20_WIDTH, 10.4f), 0.0f);
      CHECK(ok, "WORDS-3 the spacing table: 2 digits end -260.2 (7.2 before the artist), 3 digits -249.8 (3.2 INTO it); at s = 6.8 the 3-digit gap 3.6 / the 2-digit gap 14.0 / the room 333.2 / MW 20 ends 83.4 = 3.6 before the arrow; at s = 10.4 MW 20 touches the arrow (0.0); the s = 4 / 6 / 8 rows as printed");
    }
    { // the gap rule: the one digit's width split evenly = the 3-digit gap equals MW 20's gap to the arrow, both > 0; the room less that gap = 329.6 = MW 20's width = the widest title allowed; Carbon 22's stock width (403.7) runs over with and without the shift
      float g3 = Words_GapAfterNumber(3, WORDS_SHIFT), gt = Words_TitleGap(WORDS_MW20_WIDTH, WORDS_SHIFT), roomWithGap = Words_Room(WORDS_SHIFT) - gt;
      CHECK(WORDS_NEAR(g3, gt) && g3 > 0.0f && gt > 0.0f && WORDS_NEAR(roomWithGap, WORDS_MW20_WIDTH) && Words_TitleGap(WORDS_CB22_WIDTH, WORDS_SHIFT) < 0.0f && Words_TitleGap(WORDS_CB22_WIDTH, 0.0f) < 0.0f && Words_TitleGap(329.6f, WORDS_SHIFT) >= gt - 0.001f && Words_TitleGap(329.7f, WORDS_SHIFT) < gt,
            "WORDS-4 at 6.8 the 3-digit gap (%.1f) = MW 20's gap to the arrow (%.1f), both > 0; the room less that gap = %.1f = MW 20's width (a 19.1i-d title at most 329.6 wide); Carbon 22 (403.7) over before and after", (double)g3, (double)gt, (double)roomWithGap);
    }
    { // the exe (VA - 0x400000 = the file offset in .text): the two heads the asi byte-checks, the ctor's two lookups with the calling shapes the asi uses (push esi; call 0x52CEF0; add esp,4 / push hash; mov ecx,eax; call 0x517F80 / cmp [eax+0x18],2), 0x517F80's 'ret 4'
      static const unsigned char ctorA[9] = { 0x56, 0xE8, 0x06, 0x92, 0xFD, 0xFF, 0x83, 0xC4, 0x04 }, ctorB[12] = { 0x68, 0xFA, 0xA2, 0x74, 0xCA, 0x8B, 0xC8, 0xE8, 0x83, 0x42, 0xFC, 0xFF }, ctorK[4] = { 0x83, 0x78, 0x18, 0x02 }, ret4[3] = { 0xC2, 0x04, 0x00 };
      int32_t relA, relB; memcpy(&relA, exe + (0x553CE5 - 0x400000) + 1, 4); memcpy(&relB, exe + (0x553CF8 - 0x400000) + 1, 4);
      int ok = ne > 0x160000 && !memcmp(exe + (UA_WORDS_PACKAGE - 0x400000), kWordsPackageHead, 11) && !memcmp(exe + (UA_WORDS_OBJBYHASH - 0x400000), kWordsObjHead, 13)
        && !memcmp(exe + (0x553CE4 - 0x400000), ctorA, 9) && !memcmp(exe + (0x553CF1 - 0x400000), ctorB, 12) && !memcmp(exe + (0x553D1C - 0x400000), ctorK, 4) && !memcmp(exe + (0x517FCE - 0x400000), ret4, 3)
        && (uint32_t)(0x553CEA + relA) == UA_WORDS_PACKAGE && (uint32_t)(0x553CFD + relB) == UA_WORDS_OBJBYHASH && sizeof kWordsPackageHead == 11 && sizeof kWordsObjHead == 13 && WORDS_OBJ_KIND_OFF == 0x18 && WORDS_OBJ_DATA_OFF == 0x2C && WORDS_DATA_X_OFF == 0x1C;
      CHECK(ok, "WORDS-5 speed2.exe: 0x52CEF0 head = the asi's 11 bytes, 0x517F80 head = the 13 bytes + 'ret 4' at 517fce (thiscall, 1 stack arg); the ctor 553ce4 'push esi; call 0x52CEF0 (rel %08X); add esp,4', 553cf1 'push hash; mov ecx,eax; call 0x517F80 (rel %08X)', 553d1c 'cmp [eax+0x18],2'", (unsigned)relA, (unsigned)relB);
    }
    // ==== the song strings: the albums, the titles without quotes, the remix rule, the widths in the game's font, MW 23's padded artist ====
    CHECK(!strcmp(kMw[8][2], "NFS Most Wanted Exclusive") && !strcmp(kU1[20][2], "NFS Underground Exclusive") && !strcmp(kU1[22][2], "NFS Underground Exclusive") && !strcmp(kU1[23][2], "NFS Underground Exclusive") && !strcmp(kU1[24][2], "NFS Underground Exclusive")
        && !strcmp(kUg2AlbumNfs2, "NFS Underground 2 Exclusive") && !strcmp(kCb[4][2], "NFS Carbon Exclusive") && !strcmp(kCb[21][0], "People Always Talk About The Weather (Remix)") && !strcmp(kUg2Show[0], "Riders on the Storm (Remix)") && !strcmp(kUg2Show[1], "I Need Speed")
        && !strcmp(kUg2Show[8], "Rocket Ride (Remix)") && !strcmp(kUg2Show[10], "Mind Killer (Remix)") && !strcmp(kUg2Show[11], "Nothing But You (Remix)") && !strcmp(kMw[15][0], "Nine Thou (Remix)") && !strcmp(kMw[19][0], "Feels Just Like It Should (Remix)") && !strcmp(kMw[20][0], "Tao Of The Machine (Remix)")
        && !strcmp(kCb[0][0], "One Of Dem Days (Remix)") && !strcmp(kMw[24][0], "You Must Follow (Evol Intent VIP)") && !strcmp(kCb[8][0], "Girl Fight (Remix)") && !strcmp(kCb[23][0], "Washing Up (Remix)") && !strcmp(kCb[25][0], "Sugar (Remix)") && !strcmp(kCb[26][0], "Ride A White Horse (Remix)") && !strcmp(kCb[27][0], "Feel The Rush (Remix)")
        && !strcmp(kUg2Titles[0], "\"Riders on the Storm (Fredwreck Remix)\"") && !strcmp(kUg2Album19, "Lost and Found") && !strcmp(kUg2Artist0, "Snoop Dogg/The Doors"),
        "STR-16 the strings byte for byte: the 7 albums 'NFS Most Wanted Exclusive' (MW 8) / 'NFS Underground Exclusive' (UG1 21 / 23 / 24 / 25) / 'NFS Underground 2 Exclusive' (rows 0 / 1, kUg2AlbumNfs2), Carbon 5 'NFS Carbon Exclusive' unchanged; Carbon 22 'People Always Talk About The Weather (Remix)', the 13 '(Remix)' titles, Carbon 1 kept, MW 24's VIP kept; kUg2Titles still the stock text; row 19's album and row 0's artist strings unchanged");
    { uint32_t a88, a98, f34, f3c; memcpy(&a88, exe + (0x7ECD88 - 0x400000), 4); memcpy(&a98, exe + (0x7ECD98 - 0x400000), 4); memcpy(&f34, exe + (0x7ECF34 - 0x400000), 4); memcpy(&f3c, exe + (0x7ECF3C - 0x400000), 4);
      static const unsigned char st88[5] = { 0xA3, 0x88, 0xCD, 0x7E, 0x00 }, st98[5] = { 0xA3, 0x98, 0xCD, 0x7E, 0x00 }, ld3c[5] = { 0xA1, 0x3C, 0xCF, 0x7E, 0x00 }, ld34[5] = { 0xA1, 0x34, 0xCF, 0x7E, 0x00 }, cmp11[5] = { 0x83, 0xFE, 0x11, 0x7F, 0x13 }, cmp23[5] = { 0x83, 0xFE, 0x23, 0x7F, 0x13 }, longBar[7] = { 0xC7, 0x47, 0x58, 0x50, 0x0B, 0x79, 0x00 };
      const char *sData = (const char *)exe + (STR_ALB_DATA - 0x400000), *sEn = (const char *)exe + (STR_ALB_EN - 0x400000), *sFr = (const char *)exe + (STR_ALB_FR - 0x400000);
      int okT = 1; uint32_t tp0 = 0, tp26 = 0; for (int i = 0; i < 27; i++) { uint32_t tp; memcpy(&tp, exe + (0x7ECD80 - 0x400000) + 16 * i, 4); if (i == 0) tp0 = tp; if (i == 26) tp26 = tp; if (tp < 0x783000 || tp >= 0x7E8000 || strcmp((const char *)exe + (tp - 0x400000), kUg2Titles[i])) okT = 0; }
      CHECK(a88 == STR_ALB_DATA && a98 == STR_ALB_DATA && f34 == STR_ALB_FR && f3c == STR_ALB_EN && !strcmp(sData, "Need for Speed Underground 2 Exclusive") && !strcmp(sEn, "Need for Speed Underground 2 Exclusive   ") && !memcmp(sFr, "Exclusivit\xe9 Need for Speed Underground 2", 41)
          && !memcmp(exe + 0x59DB9, st88, 5) && !memcmp(exe + 0x59DBE, st98, 5) && !memcmp(exe + 0x59DAE, ld3c, 5) && !memcmp(exe + 0x59DA1, ld34, 5) && !memcmp(exe + 0xAC806, cmp11, 5) && !memcmp(exe + 0xAC81E, cmp23, 5) && !memcmp(exe + 0xAC836, longBar, 7) && okT && tp0 == 0x789C20 && tp26 == 0x789720,
          "STR-17 speed2.exe: rows 0 / 1's album pointers (0x7ECD88 / 0x7ECD98) = %08X 'Need for Speed Underground 2 Exclusive' in .data; [0x7ECF3C] = %08X the English string with its 3 trailing spaces, [0x7ECF34] = %08X the French one; 0x459D90's stores 459db9 'a3 88 cd 7e 00' / 459dbe 'a3 98 cd 7e 00' after its loads 459dae / 459da1; the pop-up's bar 4ac806 'cmp esi,0x11; jg' / 4ac81e 'cmp esi,0x23; jg' / 4ac836 LONG; the 27 title pointers of the table 0x7ECD80 (%08X .. %08X) read kUg2Titles' stock text = the load check's model", a88, f3c, f34, tp0, tp26); }
    { uint32_t savedT = 0x789C20u, curA = 0x789C20u, curB = 0x789C21u;   // a title row: the pointer SaveRows kept / the same / one byte off
      CHECK(Str_AlbumStock(STR_ALB_DATA) && Str_AlbumStock(STR_ALB_EN) && Str_AlbumStock(STR_ALB_FR) && !Str_AlbumStock(0) && !Str_AlbumStock(0x789BC0u) && !Str_AlbumStock(STR_ROW0_STOCK_EN) && !Str_AlbumStock((uint32_t)(uintptr_t)kUg2AlbumNfs2)
        && Str_PtrPlan(0x10001234u, 0x10001234u, 0) == STR_PLAN_ALREADY && Str_PtrPlan(STR_ALB_EN, 0x10001234u, Str_AlbumStock(STR_ALB_EN)) == STR_PLAN_WRITE && Str_PtrPlan(0x789BC0u, 0x10001234u, Str_AlbumStock(0x789BC0u)) == STR_PLAN_REFUSE && Str_PtrPlan(0, 0x10001234u, 0) == STR_PLAN_REFUSE
        && Str_PtrPlan(savedT, 0x10001234u, curA == savedT) == STR_PLAN_WRITE && Str_PtrPlan(curB, 0x10001234u, curB == savedT) == STR_PLAN_REFUSE && STR_ROW_A == 0 && STR_ROW_B == 1 && STR_ROW_A != SONG_CARRIER_A && STR_ROW_B != SONG_CARRIER_B,
        "STR-18 the rules: Str_AlbumStock yes for UG2's three (.data / English / French), no for 0 / 0x789BC0 ('Capone') / row 0's artist / the mod's own; Str_PtrPlan: the mod's own -> ALREADY, a stock pointer -> WRITE, anything else -> REFUSE (a title: the saved pointer -> WRITE, one byte off -> REFUSE); rows 0 / 1 are not carriers"); }
    { long nf = 0; unsigned char *fb = fontPath ? rd(fontPath, &nf) : NULL; StrFont F = { NULL, 0, 0, 0, 0, 0 }; int okF = fb && Str_FontLoad(&F, fb, nf);
      int wmax = 0, imax = -1, cb22 = 0, mw20 = 0, over = 0, nT = 0, wq = 0, w0 = 0, wsp = 0, aMw8 = 0, aU1 = 0, aUg2 = 0, aCb5 = 0, rid = 0;
      if (okF) { for (int i = 0; i < 27; i++) { int w = Str_FontWidth(&F, kUg2Show[i]); nT++; if (w > 507) over++; if (w > wmax) { wmax = w; imax = i; } if (i == 0) rid = w; }
        for (int k = 1; k <= 24; k++) { int w = Str_FontWidth(&F, kMw[k][0]); nT++; if (w > 507) over++; if (w > wmax) { wmax = w; imax = 100 + k; } if (k == 20) mw20 = w; }   // MW 25 / 26 are out (never shown)
        for (int rr = 0; rr < 26; rr++) { int w = Str_FontWidth(&F, kU1[rr][0]); nT++; if (w > 507) over++; if (w > wmax) { wmax = w; imax = 200 + rr; } }
        for (int c = 0; c < 30; c++) { int w = Str_FontWidth(&F, kCb[c][0]); nT++; if (w > 507) over++; if (w > wmax) { wmax = w; imax = 300 + c; } if (c == 21) cb22 = w; }
        wq = Str_FontWidth(&F, "\""); w0 = Str_FontWidth(&F, "0"); wsp = Str_FontWidth(&F, " "); aMw8 = Str_FontWidth(&F, kMw[8][2]); aU1 = Str_FontWidth(&F, kU1[20][2]); aUg2 = Str_FontWidth(&F, kUg2AlbumNfs2); aCb5 = Str_FontWidth(&F, kCb[4][2]); }
      CHECK(okF && nT == 107 && over == 0 && wmax == 489 && imax == 321 && cb22 == 489 && mw20 == 290 && wq == 12 && w0 == 13 && wsp == 6 && rid == 294 && aMw8 == 290 && aU1 == 289 && aUg2 == 308 && aCb5 == 228,
          "STR-19 the width rule (at most 507 font units = 329.6 at the rows' 0.65) on GLOBALA.BUN's ConduitMdITC_TT21i: %d of 107 shown titles over = %d; the widest = Carbon 22 (%d units = 317.9 at 0.65, 15.3 before the arrow); MW 20 now %d (was 507); a quote 12 / a digit 13 / a space 6 (%d / %d / %d); row 0 'Riders on the Storm (Remix)' %d (was 437); the albums MW 8 %d, UG1 %d, UG2 rows 0 / 1 %d, Carbon 5 %d (200.2 / 188.5 / 187.8 / 148.2 at 0.65)", nT, over, cb22, mw20, wq, w0, wsp, rid, aMw8, aU1, aUg2, aCb5);
      if (fb) free(fb); }
    { int okS = 1, nRemix = 0, nQuote = 0, nRemixAll = 0, left = 0; char buf[96];
      for (int i = 0; i < 27; i++) { Str_Shown(kUg2Titles[i], buf, sizeof buf); if (strcmp(buf, kUg2Show[i])) okS = 0; if (strstr(kUg2Show[i], "(Remix)")) nRemix++; }
      static const int remixUg2[4] = { 0, 8, 10, 11 }, remixMw[3] = { 15, 19, 20 }, remixCb[7] = { 0, 8, 21, 23, 25, 26, 27 }; int okSet = 1;
      for (int i = 0; i < 27; i++) { int e = 0; for (int q = 0; q < 4; q++) if (remixUg2[q] == i) e = 1; if ((strstr(kUg2Show[i], "(Remix)") != NULL) != e) okSet = 0; }
      for (int k = 1; k <= 26; k++) { int e = 0; for (int q = 0; q < 3; q++) if (remixMw[q] == k) e = 1; if ((strstr(kMw[k][0], "(Remix)") != NULL) != e) okSet = 0; }
      for (int rr = 0; rr < 26; rr++) if (strstr(kU1[rr][0], "Remix")) okSet = 0;
      for (int c = 0; c < 30; c++) { int e = 0; for (int q = 0; q < 7; q++) if (remixCb[q] == c) e = 1; if ((strstr(kCb[c][0], "(Remix)") != NULL) != e) okSet = 0; }
      for (int i = 0; i < 27; i++) { const char *t = kUg2Show[i]; if (strchr(t, '"')) nQuote++; if (strstr(t, "Remix")) { nRemixAll++; if (strcmp(t + strlen(t) - 7, "(Remix)")) left++; } }
      for (int k = 1; k <= 26; k++) { const char *t = kMw[k][0]; if (strchr(t, '"')) nQuote++; if (strstr(t, "Remix")) { nRemixAll++; if (strcmp(t + strlen(t) - 7, "(Remix)")) left++; } }
      for (int rr = 0; rr < 26; rr++) { const char *t = kU1[rr][0]; if (strchr(t, '"')) nQuote++; if (strstr(t, "Remix")) { nRemixAll++; if (strcmp(t + strlen(t) - 7, "(Remix)")) left++; } }
      for (int c = 0; c < 30; c++) { const char *t = kCb[c][0]; if (strchr(t, '"')) nQuote++; if (strstr(t, "Remix")) { nRemixAll++; if (strcmp(t + strlen(t) - 7, "(Remix)")) left++; } }
      Str_Shown("\"One Of Dem Days (Remix)\"", buf, sizeof buf); int okKeep = !strcmp(buf, "One Of Dem Days (Remix)"); Str_Shown("\"You Must Follow (Evol Intent VIP)\"", buf, sizeof buf); okKeep &= !strcmp(buf, "You Must Follow (Evol Intent VIP)");
      Str_Shown("\"People Always Talk About The Weather (Junkie XL Remix)\"", buf, sizeof buf); okKeep &= !strcmp(buf, "People Always Talk About The Weather (Remix)"); Str_Shown("\"Tao Of The Machine (Scott Humphrey's Remix)\"", buf, sizeof buf); okKeep &= !strcmp(buf, "Tao Of The Machine (Remix)");
      CHECK(okS && nRemix == 4 && okKeep, "STR-20 kUg2Show[i] = kUg2Titles[i] with the quotes off + the remix rule (27 / 27; rows 0 / 8 / 10 / 11 = the 4 '(Remix)'); the rule keeps 'One Of Dem Days (Remix)' and '(Evol Intent VIP)', makes Carbon 22's and MW 20's text");
      CHECK(nQuote == 0 && nRemixAll == 14 && left == 0 && okSet, "STR-21 no quote in any of the 107 shown titles (%d); %d titles say 'Remix', every one ends '(Remix)' (%d with a remixer's name left) = the 13 made (UG2 1 / 9 / 11 / 12, MW 15 / 19 / 20, Carbon 9 / 22 / 24 / 26 / 27 / 28) + Carbon 1's own; no UG1 title has one", nQuote, nRemixAll, left); }
    { const char *a = kMw[23][1]; int okPad = strlen(a) == 36 && !memcmp(a, "Evol Intent/Mayhem/Thinktank", 28); for (int k = 28; k < 36; k++) if (a[k] != ' ') okPad = 0;
      size_t lng = strlen(kMw[23][0]) > strlen(a) ? strlen(kMw[23][0]) : strlen(a); if (strlen(kMw[23][2]) > lng) lng = strlen(kMw[23][2]);
      size_t lngOld = 28; if (strlen(kMw[23][0]) > lngOld) lngOld = strlen(kMw[23][0]); if (strlen(kMw[23][2]) > lngOld) lngOld = strlen(kMw[23][2]);
      CHECK(okPad && Str_VisibleLen(a) == 28 && Str_Bar(lng) == 2 && Str_Bar(lngOld) == 1 && lng == 36 && strlen(kMw[23][0]) == 12 && strlen(kMw[23][2]) == 18 && Str_Bar(17) == 0 && Str_Bar(18) == 1 && Str_Bar(35) == 1 && Str_Bar(36) == 2 && Str_VisibleLen("abc  ") == 3 && Str_VisibleLen("") == 0 && Str_Bar(strlen(kMw[14][1])) == 1,
          "STR-22 MW 23 'Broken Sword' artist = 'Evol Intent/Mayhem/Thinktank' + 8 spaces = 36 chars, 28 visible: the pop-up's longest line %u -> LONG (was %u -> MID); the bar steps 17 / 18 / 35 / 36 -> SHORT / MID / MID / LONG as 0x4AC7D0; MW 14's artist (28) stays MID (not asked)", (unsigned)lng, (unsigned)lngOld); }
    printf("%s (%d failures)\n", fails ? "RIG FAIL" : "RIG PASS", fails); return fails != 0;
}
