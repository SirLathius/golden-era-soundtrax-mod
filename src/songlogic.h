// songlogic.h - the song logic: the PURE parts (no Windows calls, no game memory), so the offline rig (rig/rig.c) runs the same code natively.
// The song ids: 0..26 = UG2's own 27 songs (the song index), 27..52 = MW songs 1..26, 53..78 = UG1 rows 0..25, 79..108 = Carbon list positions 0..29.
//  (1) The map check: is the image at 'base' the mod's music map (data/MusicSFx.mpf, 7,504 B)? Five regions compared byte for byte against kMapImage: the header
//      (0..0x20), the node offset table (0x20..0x164), the nodes (0x164..0xD34), the events (0xD34..0xDD4), the words + sample table (0x1950..0x1D50 = 126 entries).
//      In the nodes the TWO CARRIER words (song 25's audio node at 0x700, song 26's at 0x738) compare on their high 16 bits only; their low 16 (the wave) must be
//      the carrier's own wave (21 / 32) or an added song's wave (MW 44..69, UG1 70..96, Carbon 97..126) = the mod's own earlier writes. Every other node word is
//      exact (UG2's own waves).
//  (2) The picker: the stock picker's rules (UG2-EXE) copied over all 109 songs. ALLOWED(ctx) = a song of the soundtrack on the bar whose live mode is 1 / 3 (FE)
//      or 2 / 3 (IG): a UG2 song by the live list 0x83ACB8 (27 x {song, mode}), an added song by its game's live modes, every song under All Trax by All Trax's
//      own set. Per context: remaining &= allowed; empty -> refill = allowed, position = -1 (46b73a-46b740). SEQUENTIAL ([player+0x9C] == 0): the positions =
//      the All Trax view order (UG1 26, the live list 27, MW 26, Carbon 30); the first position > pos whose song is in remaining (46b760-46b7b5); none -> refill
//      once + flag. RANDOM: count = |remaining|, r = rng(count) ONCE (= 0x43BDF0(); 0x43C1C0(count)), the r-th member in id order (46b7c1-46b8d2), removed.
//  (3) The carrier rule: a UG2 song -> its own slot; an added song -> the carrier (slot 25 / 26) that is not g_last (neither: 26 first, then alternate); the
//      returned carrier gets the song's wave + row; a carrier that is g_last is left alone; the other one is given back its own wave + row.
#include <stdint.h>
#include <string.h>
#include "musicmap.h"
#define SONG_M_HEADER 1
#define SONG_M_NODETAB 2
#define SONG_M_NODES 4
#define SONG_M_EVENTS 8
#define SONG_M_SAMPLES 16
#define SONG_M_ALL 31                                          // all five required: the sample table must hold the 126 entries (UG2 43 + MW 26 + UG1 27 + Carbon 30) before a wave 44..126 is ever written
#define SONG_NSONG 109                                         // UG2 27 + MW 26 + UG1 26 + Carbon 30 (ids 0..26 / 27..52 / 53..78 / 79..108)
#define SONG_NUG2 27
#define SONG_ID_MW(n) (26 + (n))                               // MW song n (1..26) -> id 27..52
#define SONG_IS_MW(id) ((id) >= SONG_NUG2 && (id) < SONG_NUG2 + 26)   // 27..52 only (UG1 = 53..78 below)
#define SONG_MW_N(id) ((id) - 26)
#define SONG_MW_WAVE(n) (43 + (n))                             // MW song n -> wave 44..69
// UG1 row r (0..25, UG1's own list order = its table 0x6F4D08) -> id 53 + r; its stream in UG1's bank = kU1Stream[r]; its wave in the mod's archive =
// 70 + stream (bank stream s = wave 70 + s). An added song (MW, UG1 or Carbon) is played through a carrier slot 25 / 26.
#define SONG_NU1 26
#define SONG_ID_U1(r) (SONG_NUG2 + 26 + (r))
#define SONG_IS_U1(id) ((id) >= SONG_NUG2 + 26 && (id) < SONG_NUG2 + 52)   // 53..78 only (Carbon = 79..108 below)
#define SONG_U1_R(id) ((id) - SONG_NUG2 - 26)
#define SONG_IS_ADDED(id) ((id) >= SONG_NUG2)
#define SONG_WAVE_U1_BASE 70
// UG1-EXE (UG1's retail exe 8234b86f..): the music player's pick 0x53C3A0 reads the live list entry's song r (53c4fb 'mov ecx,[edx*8+0x758b48]'; entry r =
// {song r, mode}), adds one (53c507 'inc ecx' -> 53c613 '[esp+0x10] := ecx' -> 53c648 / 53c659 '[ebx+0x54] / [ebx+0x4c] / [ebx+0x58] := eax'; the direct play
// 0x53BEE0 (the EA Trax preview: 4d5dda -> 51c720 -> vtable slot 0x98) stores the row's song at [state+0xf8] with mode 0x63 and the pick adds one at 53c658
// 'inc eax'), and hands r + 1, bounded 0..0x1A (53c7d8 'cmp eax,0x1a'), to the stream request's +0x24 (53c7e9 'mov [esi+0x24],eax') for the 'MUSIC_STREAM'
// bank object 0x73D7BC (53c7f8 'push 0x6f2210' = the bank name, 0x64C1A0 the play; the bank = STRM_Music_UG0_MB.abk's 27 entries 0..26). The allowed masks
// 0x758920 / 0x758924 use bit r + 1 too (520eaf / 520ebf / 520ecd 'lea ecx,[eax+1]'), the per-index table 0x6F6F20 has 27 entries (entry 0 = 0.0, 1..26 =
// 100.0). => row r = stream r + 1; stream 0 = the 30-s piece outside UG1's list. The library's own step from the request index to the bank entry was not read
// byte for byte (the index is the request's only song-dependent field and its bound 0..26 = the 27 entries); in game each pop-up names the song heard.
static const unsigned char kU1Stream[SONG_NU1] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26 };
#define SONG_U1_WAVE(r) ((unsigned)SONG_WAVE_U1_BASE + kU1Stream[r])
// UG1-EXE (the table's mode words 0x6F4D08 + 16 r + 0xC; its list builder 4aa8cc 'FE' -> 1, 4aa8fc 'IG' -> 2): rows 0..18 Racing (IG = 2), rows 19..25 Menu
// (FE = 1) = UG1's own defaults = the mod's default for each UG1 song (each game keeps its own rules). Index nn = r + 1 (0 unused), as kMwMode / the file's 'UG1 nn' lines.
static const unsigned char kU1Mode[27] = { 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1 };
// Carbon list position c (0..29) -> id 79 + c; its wave in the mod's archive = kCbWave[c] = 95 + Carbon's own wave (read from Carbon's vault: the music row's
// PathEvent -> the cb_mus_0.mpf event -> the audio node's wave 2..31; Carbon wave w = UG2 wave 95 + w, 97..126). The list order = CarClass 1 -> 2 -> 3, vault
// order inside. An added song (MW / UG1 / Carbon) is played through a carrier slot.
#define SONG_NCB 30
#define SONG_ID_CB(c) (SONG_NUG2 + 52 + (c))
#define SONG_IS_CB(id) ((id) >= SONG_NUG2 + 52 && (id) < SONG_NSONG)
#define SONG_CB_C(id) ((id) - SONG_NUG2 - 52)
#define SONG_WAVE_CB_BASE 95
static const unsigned char kCbWave[SONG_NCB] = { 124, 108, 116, 97, 114, 123, 125, 126, 119, 109, 113, 105, 98, 120, 117, 104, 115, 118, 110, 112, 106, 121, 111, 102, 101, 99, 100, 122, 103, 107 };
#define SONG_CB_WAVE(c) ((unsigned)kCbWave[c])
// every Carbon song starts on Racing (IG = 2 = Carbon's own 'on' value: DefPlay 2 on all 30 in Carbon's vault). Index nn = c + 1 (0 unused),
// as kMwMode / kU1Mode and the file's 'CB nn' lines.
static const unsigned char kCbMode[31] = { 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 };
// an added song's wave from its id (the carrier's node word low 16): MW n -> 43 + n, UG1 row r -> 70 + kU1Stream[r], Carbon c -> kCbWave[c]
static unsigned Song_AddedWave(int id) { return SONG_IS_CB(id) ? SONG_CB_WAVE(SONG_CB_C(id)) : SONG_IS_U1(id) ? SONG_U1_WAVE(SONG_U1_R(id)) : (unsigned)SONG_MW_WAVE(SONG_MW_N(id)); }
#define SONG_CARRIER_A 25
#define SONG_CARRIER_B 26
// the soundtrack picker and the settings file (the values, the step, the filter, the settings file text, the exe bytes + the patch model)
#include "settingsfile.h"
// UG2's row 19 'Determined' = the one UG2 row with an empty album (the stock exe: +8 = 00783FE5, an empty string in .rdata; every other UG2 row
// has its album). The load-time rule: the row read UG2's own title (rowOk, SaveRows) AND its saved album pointer is the stock's 00783FE5 ->
// the pointer is written to the asi's own string ONCE at load; anything else (a patched exe, any other string) -> no write, the pointer logged.
#define STR_ROW19 19
#define STR_ROW19_STOCK_ALBUM 0x00783FE5u
static int Str_Album19Ok(int rowOk, uint32_t savedAlbum) { return rowOk && savedAlbum == STR_ROW19_STOCK_ALBUM; }
// row 0 'Riders' artist pointer (+4 = 0x7ECD84): UG2-EXE 0x459D90 (the language fix-up) stores [0x7ECF30] = 007896EC 'Snoop Dogg avec The Doors' (arg 1)
// or [0x7ECF38] = 00789C04 'Snoop Dogg feat. The Doors' there (459d9c / 459db3) - after the asi loaded. The rule: the pointer found there must be one of those two
// stock strings (the .data initial value is 00789C04 too) -> written to the asi's own string; the asi's own -> nothing to do; anything else -> no write (logged once).
#define STR_ROW0 0
#define STR_ROW0_STOCK_EN 0x00789C04u
#define STR_ROW0_STOCK_FR 0x007896ECu
static int Str_Artist0Stock(uint32_t p) { return p == STR_ROW0_STOCK_EN || p == STR_ROW0_STOCK_FR; }
// one pointer write, compare-first + read-back (the asi on the row, the rig on a plain buffer): 2 = already there, 1 = written and read back, 0 = the read-back differs
static int Str_PutPtr(volatile uint32_t *p, uint32_t want) { if (*p == want) return 2; *p = want; return *p == want ? 1 : 0; }
// ---- the song strings the EA Trax rows and the song pop-up show. UG2-EXE (retail speed2.exe): rows 0 / 1's album pointers (0x7ECD88 / 0x7ECD98) hold
// 0x00789BDC in .data and are re-written by UG2's language fix-up 0x459D90 after the asi loaded (459db9 'a3 88 cd 7e 00' / 459dbe 'a3 98 cd 7e 00' store
// [0x7ECF3C] = 0x00789694 English (459dae) or [0x7ECF34] = 0x007896C0 French (459da1); its one way in is the language set 0x512360 -> 0x468F30) -> only one of
// those three pointers is replaced (as row 0's artist above), at load + compare-first at every pick + every screen open. The 27 title pointers (+0) have no
// store in UG2's .text (the 8 '[reg+0x7ecd80]' sites are reads; 0x459D90 writes +4 / +8 only) -> written once at load when each still holds the pointer
// SaveRows checked by text (isStock = cur == the saved pointer). The pop-up's bar: 0x4AC7D0 takes the longest strlen of the three lines (4ac806 'cmp esi,0x11;
// jg' SHORT <= 17, 4ac81e 'cmp esi,0x23; jg' MID <= 35, else LONG) - MW 23's artist is padded to 36 so that song's pop-up uses the LONG bar (the trailing spaces
// draw nothing: left-aligned).
#define STR_ROW_A 0
#define STR_ROW_B 1
#define STR_ALB_DATA 0x00789BDCu        // 'Need for Speed Underground 2 Exclusive' = the .data initial of 0x7ECD88 / 0x7ECD98
#define STR_ALB_EN 0x00789694u          // 'Need for Speed Underground 2 Exclusive   ' (3 trailing spaces) = [0x7ECF3C], stored by 0x459D90 when the language is not French
#define STR_ALB_FR 0x007896C0u          // 'Exclusivite Need for Speed Underground 2' = [0x7ECF34], French
static int Str_AlbumStock(uint32_t p) { return p == STR_ALB_DATA || p == STR_ALB_EN || p == STR_ALB_FR; }
// one pointer's plan: 2 = already the mod's (nothing to do), 1 = UG2's own (write it), 0 = anything else (no write, said once)
#define STR_PLAN_ALREADY 2
#define STR_PLAN_WRITE 1
#define STR_PLAN_REFUSE 0
static int Str_PtrPlan(uint32_t cur, uint32_t want, int isStock) { return cur == want ? STR_PLAN_ALREADY : isStock ? STR_PLAN_WRITE : STR_PLAN_REFUSE; }
// the pop-up's bar by the longest line (strlen, trailing spaces counted - the padded MW 23 artist relies on it): 0 SHORT (<= 17) / 1 MID (<= 35) / 2 LONG
#define STR_BAR_SHORT_MAX 17
#define STR_BAR_MID_MAX 35
static inline int Str_Bar(size_t longest) { return longest <= STR_BAR_SHORT_MAX ? 0 : longest <= STR_BAR_MID_MAX ? 1 : 2; }
static inline size_t Str_VisibleLen(const char *s) { size_t l = strlen(s); while (l && s[l - 1] == ' ') l--; return l; }   // the drawn text: trailing spaces are invisible
#define SONG_CTX_FE 0
#define SONG_CTX_IG 1
// MW 25 'Most Wanted Mash Up' / 26 'The Mann' are left OUT of ALLOWED: never picked, never shown; their strings stay in the table unused; the archive keeps
// waves 68 / 69.
#define SONG_MW_OUT(n) ((n) == 25 || (n) == 26)
// MW's DefPlay (MW's attributes.bin 'music' rows) of MW songs 1..26 as UG2 modes: FE 1 / IG 2 / AL 3 (index 0 unused). Counts FE 8 / IG 16 / AL 2.
static const unsigned char kMwMode[27] = { 0, 1, 1, 1, 2, 1, 2, 2, 1, 2, 2, 2, 2, 1, 3, 3, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2 };
// the LIVE modes of MW 1..26 = kMwMode until the profile's file / a row step says otherwise (set in tracklist.h); the picker reads these
static unsigned char list_mwMode[27]; static int list_modesSet;
#define LIST_MWMODE(n) (list_modesSet ? list_mwMode[n] : kMwMode[n])
// the LIVE modes of UG1 rows (nn = row + 1) = kU1Mode until the profile's file / a row step says otherwise (tracklist.h sets the flags)
static unsigned char list_u1Mode[27]; static int list_u1Set;
#define LIST_U1MODE(nn) (list_u1Set ? list_u1Mode[nn] : kU1Mode[nn])
// the LIVE modes of Carbon rows (nn = c + 1) = kCbMode until the profile's file / a row step says otherwise (tracklist.h sets the flags)
static unsigned char list_cbMode[31]; static int list_cbSet;
#define LIST_CBMODE(nn) (list_cbSet ? list_cbMode[nn] : kCbMode[nn])
// ALL TRAX's OWN set: each category keeps the player's settings for that category until they change them or reset it to default. One live mode per song id
// 0..108 (UG2 songs 0..26 by the SONG index, not the list position; MW 27..52; UG1 53..78; Carbon 79..108); its default = each game's own default (UG2 songs 0..5
// Menu / 6..26 Racing, as defaultbutton.h; kMwMode; kU1Mode; kCbMode) until the profile's file ('ALL UG2 / UG1 / MW / CB nn' lines, or the copy of an older
// release's shared settings) / a row step under All Trax / a reset under All Trax says otherwise (tracklist.h sets the flag). Read by the view build and the
// allowed rule under All Trax ONLY; the four game categories keep their own: the arrays above / UG2's list 0x83ACB8.
static unsigned char list_allMode[SONG_NSONG]; static int list_allSet;
static inline unsigned char Song_AllDefault(int id) { return id < SONG_NUG2 ? (unsigned char)(id < 6 ? 1 : 2) : SONG_IS_MW(id) ? kMwMode[SONG_MW_N(id)] : SONG_IS_U1(id) ? kU1Mode[SONG_U1_R(id) + 1] : kCbMode[SONG_CB_C(id) + 1]; }
#define SONG_ALLMODE(id) (list_allSet ? list_allMode[id] : Song_AllDefault(id))
static int Song_CarrierIndex(unsigned off) { return off == kAudOff[SONG_CARRIER_A] ? 0 : off == kAudOff[SONG_CARRIER_B] ? 1 : -1; }
static int Song_WaveOkForCarrier(int ci, unsigned wave) { return wave == kOrigWave[SONG_CARRIER_A + ci] || (wave >= SONG_WAVE_MW_FIRST && wave <= SONG_WAVE_MW_LAST)
    || (wave >= SONG_WAVE_U1_FIRST && wave <= SONG_WAVE_U1_LAST)   // a carrier may hold a UG1 wave 70..96 too
    || (wave >= SONG_WAVE_CB_FIRST && wave <= SONG_WAVE_CB_LAST); }   // or a Carbon wave 97..126
static int Song_RegionEq(const uint8_t *base, unsigned a, unsigned b, unsigned *firstDiff)
{
    for (unsigned o = a; o < b; o++) {
        int ci = (o & 3) == 0 ? Song_CarrierIndex(o) : -1;
        if (ci >= 0) {
            uint32_t w; memcpy(&w, base + o, 4); uint32_t e; memcpy(&e, kMapImage + o, 4);
            if ((w >> 16) != (e >> 16) || !Song_WaveOkForCarrier(ci, w & 0xFFFF)) { *firstDiff = o; return 0; }
            o += 3; continue;
        }
        if (base[o] != kMapImage[o]) { *firstDiff = o; return 0; }
    }
    return 1;
}
static unsigned Song_MapMatch(const uint8_t *base, unsigned firstDiff[5])      // -> bitmask of matching regions
{
    unsigned m = 0;
    if (Song_RegionEq(base, 0, SONG_OFF_NODETAB, &firstDiff[0])) m |= SONG_M_HEADER;
    if (Song_RegionEq(base, SONG_OFF_NODETAB, SONG_OFF_NODES, &firstDiff[1])) m |= SONG_M_NODETAB;
    if (Song_RegionEq(base, SONG_OFF_NODES, SONG_OFF_EVENTS, &firstDiff[2])) m |= SONG_M_NODES;
    if (Song_RegionEq(base, SONG_OFF_EVENTS, SONG_OFF_EVENTS_END, &firstDiff[3])) m |= SONG_M_EVENTS;
    if (Song_RegionEq(base, SONG_OFF_WORDS, SONG_FILE_LEN, &firstDiff[4])) m |= SONG_M_SAMPLES;
    return m;
}
// the loader's own derivation of the slot's pointers from the image (+0x40: from a decompile, not checked in the exe, FUN_0073a7c0; +0x4C / +0x50 / +0x54: UG2-EXE)
static inline unsigned Song_ExpectEventTab(const uint8_t *base)
{
    unsigned nn = base[0x12] | (base[0x13] << 8);
    unsigned last = (base[0x20 + 2 * (nn - 1)] | (base[0x21 + 2 * (nn - 1)] << 8)) * 4;
    uint32_t w1; memcpy(&w1, base + last + 4, 4);
    return (last + 0x13 + ((w1 >> 12) & 0x1F) * 4) & ~3u;
}
static inline unsigned Song_ExpectSamples(const uint8_t *base, unsigned *end)     // +0x50 and +0x54 as offsets (73a954 / 73a962 / 73a975)
{
    uint32_t w54; memcpy(&w54, base + 0x1954, 4); unsigned p4c = w54 * 4 + base[0x10] * 4;
    if (p4c + base[0xD] * 4 + 4 > SONG_FILE_LEN) { *end = 0; return 0; }           // not the mod's map words: stay inside the image
    uint32_t wEnd; memcpy(&wEnd, base + p4c + base[0xD] * 4, 4); *end = wEnd * 4;
    return p4c + 4 + base[0xD] * 4;
}
// ---- (2) the picker copy
typedef struct { unsigned char rem[SONG_NSONG]; int pos; int filled; } Song_CtxState;
typedef struct { Song_CtxState c[2]; int last; int lastCarrier; unsigned picks, draws, refills, seqRefills; } Song_State;
typedef struct { int ctx, seq, allowed, before, after, count, r, refilled, seqWrap; } Song_PickInfo;
static void Song_Init(Song_State *st) { memset(st, 0, sizeof *st); st->c[0].pos = st->c[1].pos = -1; st->last = -1; st->lastCarrier = -1; }
// the live list: listSong[p] / listMode[p] for list position p = 0..26 (0x83ACB8 + p*8: int song, byte mode; 479248 / 47924d)
static int Song_Allowed(int ctx, int id, const int *listSong, const unsigned char *listMode)
{
    unsigned char m = 0;
    if (!Set_InSoundtrack(set_soundtrack, id)) return 0;                                   // the soundtrack chosen on the screen (one game's songs only, All Trax = every song)
    if (SONG_IS_MW(id) && SONG_MW_OUT(SONG_MW_N(id))) return 0;                                      // MW 25 / 26 out under every category
    if (set_soundtrack == SET_ST_ALL) m = SONG_ALLMODE(id);                                       // under All Trax EVERY id's mode = All Trax's own set (UG2's songs too: not the list's bytes)
    else if (SONG_IS_MW(id)) m = LIST_MWMODE(SONG_MW_N(id));                                         // the live MW mode (the screen's steps + the file) = Most Wanted's own set
    else if (SONG_IS_U1(id)) m = LIST_U1MODE(SONG_U1_R(id) + 1);                                      // the live UG1 mode (UG1's own default, the file's line, a row step)
    else if (SONG_IS_CB(id)) m = LIST_CBMODE(SONG_CB_C(id) + 1);                                      // the live Carbon mode (Racing by default, the file's line, a row step)
    else { for (int p = 0; p < SONG_NUG2; p++) if (listSong[p] == id) { m = listMode[p]; break; } }
    return ctx == SONG_CTX_FE ? (m == 1 || m == 3) : (m == 2 || m == 3);
}
static int Song_Count(const unsigned char *rem) { int n = 0; for (int i = 0; i < SONG_NSONG; i++) n += rem[i] != 0; return n; }
// the SEQUENTIAL positions = the All Trax view order: 0..25 = UG1 rows 0..25, 26..52 = the live list in list order, 53..78 = MW 1..26, 79..108 = Carbon 0..29
// (Carbon last). Under one soundtrack the allowed set leaves only its own songs, so the order only shows under All Trax.
static int Song_PosId(int p, const int *listSong) { return p < SONG_NU1 ? SONG_ID_U1(p) : p < SONG_NU1 + SONG_NUG2 ? listSong[p - SONG_NU1] : p < SONG_NU1 + SONG_NUG2 + 26 ? SONG_ID_MW(p - SONG_NU1 - SONG_NUG2 + 1) : SONG_ID_CB(p - SONG_NU1 - SONG_NUG2 - 26); }
// -> the chosen id (0..108) or -1 (nothing allowed). rng(count) = one draw of 0..count-1 (the hook: 0x43BDF0(); 0x43C1C0(count)); called ONCE per random pick, never with 0.
static int Song_Choose(Song_State *st, int ctx, int seq, const int *listSong, const unsigned char *listMode, int (*rng)(int count, void *u), void *u, Song_PickInfo *inf)
{
    Song_CtxState *c = &st->c[ctx]; unsigned char allowed[SONG_NSONG]; int na = 0;
    memset(inf, 0, sizeof *inf); inf->ctx = ctx; inf->seq = seq; inf->r = -1; st->picks++;
    for (int i = 0; i < SONG_NSONG; i++) { allowed[i] = Song_Allowed(ctx, i, listSong, listMode) != 0; na += allowed[i]; }
    inf->allowed = na;
    if (!na) return -1;                                                                      // the empty set (46b72a / 46b8f8 -> -1)
    for (int i = 0; i < SONG_NSONG; i++) c->rem[i] = c->rem[i] && allowed[i];                // remaining &= allowed (a mode change shrinks it)
    inf->before = Song_Count(c->rem);
    if (!inf->before) { memcpy(c->rem, allowed, SONG_NSONG); c->pos = -1; c->filled = 1; st->refills++; inf->refilled = 1; }   // the refill (46b73a-46b740)
    if (seq) {
        for (int round = 0; round < 2; round++) {
            for (int p = c->pos + 1; p < SONG_NSONG; p++) {                                    // positions 0..25 = UG1, 26..52 = the list in list order, 53..78 = MW 1..26, 79..108 = Carbon
                int id = Song_PosId(p, listSong);
                if (id < 0 || id >= SONG_NSONG || !c->rem[id]) continue;
                c->pos = p; c->rem[id] = 0; inf->after = Song_Count(c->rem); return id;
            }
            if (round == 0) { memcpy(c->rem, allowed, SONG_NSONG); c->pos = -1; st->seqRefills++; inf->seqWrap = 1; }   // nothing after pos (the stock would return -1): refill once
        }
        return -1;
    }
    int count = Song_Count(c->rem); inf->count = count;
    if (count <= 0) return -1;                                                               // cannot happen after the refill; never call rng with 0
    int r = rng(count, u); st->draws++; inf->r = r;
    if (r < 0 || r >= count) r = ((r % count) + count) % count;                              // 0x43C1C0 returns state % count: always in range; kept safe
    for (int i = 0; i < SONG_NSONG; i++) if (c->rem[i] && r-- == 0) { c->rem[i] = 0; inf->after = Song_Count(c->rem); return i; }
    return -1;
}
// ---- UG2's RIDERS-FIRST signal (UG2-EXE 47aed4: the title screen's constructor 0x4A90F0 -> 0x480C30(-2) -> 0x47AC50 state 1 with -2
// sets the stock picker's FE remaining set [player+0x88] := 1 = song 0 only). Honoured at a MENU pick only when the value is exactly 1 AND a CHANGE since the
// mod's last menu pick (prev = the value that pick read; -1 = none yet): the stock picker then answers (-2) = Riders, and clears the bit itself (46b94d sequential /
// 46ba66 random) -> the next menu pick reads 0 = the mod's own. A list whose FE set is Riders alone also puts 1 there (0x46BAA0 46bad5): honoured once per change,
// never a lock (after the stock's own pick the value is 0; a 1 that stays across two menu picks is the plain list value, not the signal).
static int Song_RidersSignal(int ctx, uint32_t v88, int prev) { return ctx == SONG_CTX_FE && v88 == 1 && prev != 1; }
// the mod's own set takes the song the stock played (song 0): remaining &= allowed, a refill if empty (as a pick would), then the song dropped; no draw, no position change
static void Song_TakeSong(Song_State *st, int ctx, int id, const int *listSong, const unsigned char *listMode, Song_PickInfo *inf)
{
    Song_CtxState *c = &st->c[ctx]; unsigned char allowed[SONG_NSONG]; int na = 0;
    memset(inf, 0, sizeof *inf); inf->ctx = ctx; inf->r = -1;
    for (int i = 0; i < SONG_NSONG; i++) { allowed[i] = Song_Allowed(ctx, i, listSong, listMode) != 0; na += allowed[i]; }
    inf->allowed = na;
    for (int i = 0; i < SONG_NSONG; i++) c->rem[i] = c->rem[i] && allowed[i];
    inf->before = Song_Count(c->rem);
    if (!inf->before && na) { memcpy(c->rem, allowed, SONG_NSONG); c->pos = -1; c->filled = 1; st->refills++; inf->refilled = 1; }
    if (id >= 0 && id < SONG_NSONG) c->rem[id] = 0;
    inf->after = Song_Count(c->rem);
}
// ---- (3) the carrier rule. want[i] for carrier 25+i: -1 = leave it (it is g_last and not returned), 0 = its own wave + row, id >= 27 = that ADDED song's wave + row
// (27..52 MW / 53..78 UG1 / 79..108 Carbon - the added-wave helper above + the asi's AddedRow resolve it).
typedef struct { int ret; int carrier; int want[2]; } Song_Plan;
static Song_Plan Song_CarrierPlan(int id, int last, int lastCarrier)
{
    Song_Plan pl; pl.carrier = -1;
    if (SONG_IS_ADDED(id)) pl.ret = pl.carrier = (last == SONG_CARRIER_A) ? SONG_CARRIER_B : (last == SONG_CARRIER_B) ? SONG_CARRIER_A : (lastCarrier == SONG_CARRIER_B) ? SONG_CARRIER_A : SONG_CARRIER_B;
    else pl.ret = id;
    for (int i = 0; i < 2; i++) {
        int c = SONG_CARRIER_A + i;
        if (c == pl.ret) pl.want[i] = SONG_IS_ADDED(id) ? id : 0;
        else if (c == last) pl.want[i] = -1;
        else pl.want[i] = 0;
    }
    return pl;
}
// the list / modes / preview pure parts (needs everything above)
#include "tracklist.h"
