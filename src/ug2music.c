// ug2music.c - UG2Music.asi for Need for Speed Underground 2: the songs of NFS Underground, Most Wanted and Carbon on UG2's own EA Trax screen.
//
// WHAT IT DOES
//   UG2 plays its own 27 EA Trax songs. This mod adds the songs of NFS Underground (26), Most Wanted (24) and Carbon (30) next to them, inside
//   UG2's own music system: they play in the menus and in races, show in UG2's song pop-up and are set up on the EA Trax screen like UG2's own.
//   - The top bar of the EA Trax screen picks the soundtrack: All Trax / Underground / Underground 2 / Most Wanted / Carbon. Under Underground 2
//     the game's own picker decides, exactly as in the stock game; under the others the mod picks with a copy of UG2's rules (front end / in game,
//     random / sequential, each song's Off / Menu / Racing / All mode, Riders on the Storm first in the menus as UG2 does it).
//   - The list shows the chosen soundtrack's songs. Each row's mode is stepped with UG2's own arrows and words; the preview plays the row.
//     Each EA Trax category keeps its own song settings.
//   - Two buttons on the screen's bottom bar: Shuffle (UG2's own Random / Sequential byte) and Default (UG2's yes / no dialog, then the songs of
//     the category shown go back to their defaults; Default resets only the category shown).
//   - The settings are saved when the game saves the profile and read when a profile loads; UG2's own 'save changes?' prompt covers them too.
//   - An added song plays through one of two carrier slots (UG2 songs 25 / 26): the slot's wave in the loaded music map and its song-table row
//     are pointed at the added song for that play, and given back to their own song when no longer needed.
//   - Long songs: UG2's reader sign-extends a 3-byte sample count, so a song over 233 s at 36,000 Hz would never end; a small hook masks the count to 24 bits.
//   - Song strings: titles without their quotes, '(Remix)' where it belongs, the 'NFS <game> Exclusive' albums; the row words sit 6.8 units right.
//
// HOW IT HOOKS UG2 (every address = the retail exe's)
//   the song picker 0x46B6F0 (a 5-byte jump to the decision) | the music header parser's tag-0x85 store 0x727CD8 (the long-song fix) |
//   the top bar: the trigger handler 0x5241E0, the label routine 0x524120, the two mouse blocks 0x565DB1 / 0x565DF1, the screen ctor's label
//   block 0x5540BF | the list: the fill 0x54B310 (8 operands + the row bound 0x54BB38), the row cycle 0x53DEB0 (2 search operands + a head hook),
//   the preview 0x480C10 (a head hook) | the save sync: the 'call 0x43E030' operands 4d9cb2 / 4d9bea inside the save functions 0x4D9C60 /
//   0x4D9B80 and the 'call 0x5344D0' operands 5347ab / 534812 (the profile load) | the save prompt: the heads of 0x52A460 / 0x52A4C0 |
//   the buttons: the bar's flag word 0x7F80C8, the calls 54e8d7 / 55df22, the jukebox vtable's handler slot 0x79CB7C | data: the song
//   table 0x7ECD80's string pointers, the loaded music map's two carrier nodes, the x of the row words at each screen open.
//   Every site is byte-checked before any write. Each part installs all or nothing: one mismatch = that part is skipped (nothing of it written)
//   and the log says which site did not match and what was found. The list and the Shuffle button need the top bar, the Default button needs
//   the Shuffle button.
//
// FILES
//   scripts\UG2Music.log               - the log, next to UG2Music.asi: one plain sentence per line after the time of day. Each game start
//                                        begins a new log; the one before is kept as scripts\UG2Music.previous.log. A problem line carries one
//                                        word in capitals (NOT or FAILED); a problem that could repeat is said once per start.
//   scripts\UG2Music\<profile>.txt     - the settings per profile (the soundtrack + each category's song modes; other lines kept),
//                                        written via <profile>.txt.tmp + MoveFileExA when the game saves the profile.
//
// NEEDS
//   UG2 retail v1.2 speed2.exe (4,800,512 B) and the rebuilt SDATA\sdat.viv (893,398,656 B: UG2's own archive with the added songs appended as
//   waves 44..126 = MW 44..69, UG1 70..96, Carbon 97..126, and the grown music map MusicSFx.mpf, 7,504 B, 126 sample entries). The mod finds the
//   loaded map by matching it against the image in musicmap.h in all five regions; no match = the stock picker decides every pick.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include "songlogic.h"
#include "savesync.h"     // the save / load sites, the rel32 rule, the state rule
#include "saveprompt.h"   // the keep-a-copy / changed? heads, the snapshot rule
#include "shuffle.h"  // the Shuffle button's sites + bytes, the pure logic (the text, the flip, the gates, the node walk), the fold of the 'no song' line
#include "defaultbutton.h"  // the Default button's sites + bytes, the pure logic (the gates for the second bit, the answer, the scope, the resets)
#include "rowwords.h"   // the row words' shift, the 16 hashes, the two lookups + their heads, the plan per word
#define MOD_VERSION "Golden Era Soundtrax 1.0"   // the log's first line; tools/build.sh looks for it in the built file
#define UA_PICKER 0x0046B6F0u
#define UA_PICKER_RESUME 0x0046B6F5u
#define UA_RET_FE 0x0047B0D7u
#define UA_RET_IG 0x0047B1CEu
#define UA_TIMER 0x0043BDF0u          // QueryPerformanceCounter + 0x75C600; no argument; called before each random draw (46b869)
#define UA_RANDOM 0x0043C1C0u         // random(n) = [0x7EA624] % n, cdecl 1 arg, steps the state; n = 0 faults -> never called with 0
#define UA_7ECD80 0x007ECD80u         // the song table 27 x {title, artist, album, pool}
#define UA_82B884 0x0082B884u         // the sound manager; [+0x90] == 1 = front end (46b6f8-46b713)
#define UA_89E7B0 0x0089E7B0u         // the game state; == 8 = front end (46b704-46b71c)
#define UA_LIST 0x0083ACB8u           // the live EA Trax list: 27 x {int song, byte mode}
#define UA_82BA08 0x0082BA08u         // the stream buffer size (Track Overhead + BufferOverhead; 46b57e) - named in the log at the map find
#define UA_82B984 0x0082B984u         // the stream buffer handle (0 = the sound pool had no room; 483ffb) - checked at the map find
#define UA_MAPSLOT0 0x008B5710u
#define MAPSLOT_STRIDE 0x928u
#define UA_LS_SITE 0x00727CD8u        // the long-song site = 0x727A46's tag-0x85 store: mov eax,[ebp-0x10] (the value); mov [edi],eax (the total); jmp 0x727D99
#define UA_LS_RESUME 0x00727CDDu      // the original 'jmp 0x727D99' (left as it is; the stub jumps there)
#define EXE_SIZE_RETAIL 4800512u      // the retail v1.2 speed2.exe
#define VIV_SIZE_MOD 893398656u       // the mod's SDATA\sdat.viv
#define VIV_SIZE_ORIGINAL 259484480u  // UG2's own SDATA\sdat.viv
static const unsigned char kLsBytes[10] = { 0x8B, 0x45, 0xF0, 0x89, 0x07, 0xE9, 0xB7, 0x00, 0x00, 0x00 };
static const unsigned char kHead[6] = { 0x83, 0xEC, 0x08, 0x53, 0x55, 0x56 };
static const unsigned char kHeadTimer[7] = { 0x83, 0xEC, 0x08, 0x8D, 0x44, 0x24, 0x00 };      // 43bdf0 sub esp,8 / lea eax,[esp+0]
static const unsigned char kHeadRandom[6] = { 0x8B, 0x0D, 0x24, 0xA6, 0x7E, 0x00 };           // 43c1c0 mov ecx,[0x7EA624]
#include "songnames.h"
static const char *const kModeShown[4] = { "Off", "Menu", "Racing", "All" };   // UG2's own mode words, as the rows show them (0..3)
static char g_dir[MAX_PATH], g_logPath[MAX_PATH]; static CRITICAL_SECTION g_logLock; static BOOL g_logReady; static DWORD g_logMoveError;   // the previous log's move failed = its error (0 = moved)
static DWORD g_rows[27][4]; static BOOL g_rowOk[27];          // UG2's own rows, read at load before any write
unsigned char *g_tramp; static BOOL g_hooked, g_rngOk, g_carrierRowsOk;   // the picker's trampoline (non-static: the asm reads it)
static int g_mapSlot = -1; static uintptr_t g_mapBase; static BOOL g_mapLogged; static int g_findFails;   // the map slot that matched; the 'loaded' line said; the picks that found no map
static Song_State g_st;                                        // the picker copy's state (remaining / position per context, g_last, the last carrier used)
static int g_prev88 = -1;                                      // [player+0x88] as read at the mod's last menu pick (-1 = none yet)
// the settings (the choice in memory + the kept lines), the profile name as last read (32 bytes, compared whole), the folder + file path
static Set_Settings g_set; static char g_profile[32]; static BOOL g_profileEver, g_setDirOk; static char g_setDir[MAX_PATH], g_setPath[MAX_PATH];
static int g_offRun; static DWORD g_offLastLog;                // the picks under Off in a row; when the Off line was last written
static BOOL g_barInstalled, g_labelAfterStep;
DWORD g_trigTail = UA_BAR_TRIG_TAIL;                       // the trigger stub's jump target after the step (absolute, indirect): the handler's own tail 0x524230
static int g_artist0Refused, g_strAlbRefused;              // row 0's artist / rows 0 and 1's album pointer refused (a non-stock pointer): said once
static int g_lastSong = -1;                                // the song the last pick or preview started (named in the long-song line)
// the long-song fix: written by the stub (the sound library's thread), read by the game thread at the next pick; g_lsResume is the stub's jump target (absolute, indirect)
volatile LONG g_lsFixes; volatile DWORD g_lsLastRaw, g_lsLastFixed; DWORD g_lsResume = UA_LS_RESUME; static LONG g_lsSeen; static BOOL g_lsHooked;
// the list: the view UG2's fill reads (its 8 operands re-pointed at load), the table, the counts, the two trampolines (tracklist.h)
#define UA_LIST_PLAYSLOT 0x0047AF90u    // thiscall(player, slot) ret 4: stops the old song, PATH_events the slot, the pop-up of its row (47af90-47b075; 0x480C10 jumps there)
#define UA_LIST_BOUND 0x0054BB38u       // the imm8 of 54bb36 'cmp edi,0x1b' = the fill's row count (:= N at every screen open)
#define UA_LIST_ROWFN 0x0053DEB0u
#define UA_LIST_ROWRESUME 0x0053DEB5u
#define UA_LIST_PREVFN 0x00480C10u
#define UA_LIST_PREVRESUME 0x00480C16u
static List_ViewEntry g_view[LIST_MAXVIEW]; static List_TabRow g_vtab[LIST_NTAB]; static int g_viewN, g_viewSt = -1;
static BOOL g_listInstalled; DWORD g_rowTramp, g_prevTramp;
// the Shuffle button (shuffle.h): installed?; the fold state of the 'no song' line (+ g_noneHit = this pick answered -1 from the no-song path)
static BOOL g_shufInstalled; static Shuf_NoneFold g_noneFold; static int g_noneHit;
// the Default button (defaultbutton.h): installed? (the flag word's second bit + the handler slot), a dialog up?, the scope at the click
static BOOL g_dfltInstalled, g_dfltDialogUp; static int g_dfltScope = -1;
// the save sync: the in-memory state vs the file. g_setDirty = a step since the last write / read; g_dirtySteps its count; the two absolute targets the stubs
// jump to / call (non-static: the asm reads them)
static BOOL g_setDirty, g_saveInstalled; static int g_dirtySteps; DWORD g_saveFn = UA_SAVE_FN, g_loadFn = UA_LOAD_FN;
// the save prompt: the snapshot state, the copy-field reading taken before each hooked call, the two trampolines (non-static: the asm reads them)
static Prompt_State g_prompt; static BOOL g_promptInstalled; static int g_promptHadCopy; DWORD g_keepTramp, g_cmpTramp;
static void BuildView(void);
static void RefillList(uintptr_t screen);
// ---- the log: one line per event, 'HH:MM:SS ' + a plain sentence. A problem that could repeat is said once per start: g_said holds what was said.
enum { SAID_PROFILE = 1, SAID_MAP = 2, SAID_PLAYER = 4, SAID_LIST = 8, SAID_WRITE = 16, SAID_OFF = 32,
       SAID_E_BARTEXT = 64, SAID_E_WORDS = 128, SAID_E_BOUND = 256, SAID_E_VIEW = 512, SAID_E_REFILL = 1024, SAID_E_ROWSTEP = 2048,
       SAID_E_SHUFLABEL = 4096, SAID_E_SHUFCLICK = 8192, SAID_E_DFLTLABEL = 16384, SAID_E_RESET = 32768 };
static unsigned g_said;
__attribute__((format(printf, 1, 2))) void Log(const char *fmt, ...)   // the compiler checks every Log call's arguments against its format (-Wall)
{
    if (!g_logReady) return;
    EnterCriticalSection(&g_logLock);
    FILE *f = fopen(g_logPath, "a");
    if (f) { SYSTEMTIME t; GetLocalTime(&t); va_list ap; va_start(ap, fmt); fprintf(f, "%02u:%02u:%02u ", (unsigned)t.wHour, (unsigned)t.wMinute, (unsigned)t.wSecond); vfprintf(f, fmt, ap); va_end(ap); fputc('\n', f); fflush(f); fclose(f); }
    LeaveCriticalSection(&g_logLock);
}
// at each game start the previous log is moved to UG2Music.previous.log before the first line is written; a failed move = the new lines append to the old file
static void LogStart(void)
{
    char prev[MAX_PATH]; snprintf(prev, sizeof prev, "%s\\UG2Music.previous.log", g_dir);
    if (GetFileAttributesA(g_logPath) != INVALID_FILE_ATTRIBUTES && !MoveFileExA(g_logPath, prev, MOVEFILE_REPLACE_EXISTING)) { g_logMoveError = GetLastError(); if (!g_logMoveError) g_logMoveError = 1; }
}
static const char *FmtNum(unsigned long long v, char *out)   // 4800512 -> '4,800,512' (out: 32 bytes)
{
    char d[24]; int n = 0, k = 0;
    do { d[n++] = (char)('0' + (int)(v % 10)); v /= 10; } while (v);
    for (int i = n - 1; i >= 0; i--) { out[k++] = d[i]; if (i && i % 3 == 0) out[k++] = ','; }
    out[k] = 0; return out;
}
// an EA Trax problem a player would see, said once per kind: 'EA Trax: <what> did NOT work: <why>. <what the player sees>'
static void TraxProblem(unsigned bit, const char *what, const char *why, const char *sees)
{
    if (g_said & bit) return;
    g_said |= bit; Log("EA Trax: %s did NOT work: %s. %s", what, why, sees);
}
static BOOL Accessible(uintptr_t a, size_t n, BOOL write)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (a < 0x10000 || !VirtualQuery((void *)a, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD)) return FALSE;
    DWORD p = mbi.Protect & 0xFF;
    BOOL rd = p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY || p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
    BOOL wr = p == PAGE_READWRITE || p == PAGE_WRITECOPY || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
    if (a + n > (uintptr_t)mbi.BaseAddress + mbi.RegionSize) return FALSE;
    return write ? wr : rd;
}
static BOOL ReadCStr(uintptr_t a, char *out, int n)
{
    for (int i = 0; i < n - 1; i++) { if (!Accessible(a + i, 1, FALSE)) return FALSE; out[i] = *(volatile char *)(a + i); if (!out[i]) return TRUE; }
    out[n - 1] = 0; return TRUE;
}
static BOOL RdDword(uintptr_t a, DWORD *v) { if (!Accessible(a, 4, FALSE)) return FALSE; *v = *(volatile DWORD *)a; return TRUE; }
static BOOL RdByte(uintptr_t a, DWORD *v) { if (!Accessible(a, 1, FALSE)) return FALSE; *v = *(volatile unsigned char *)a; return TRUE; }
static void Hex(uintptr_t a, char *out, int n)   // n bytes of the game's code as hex (' 8B 45 ..'), guarded; printed only when a check failed
{
    out[0] = 0;
    for (int i = 0; i < n; i++) { if (!Accessible(a + i, 1, FALSE)) { strcat(out, " ??"); break; } sprintf(out + strlen(out), " %02X", *(volatile unsigned char *)(a + i)); }
}
// ---- the game's two files, named at the start: the exe's size (the retail v1.2 size or not) and the song archive's size (the mod's, UG2's original, another)
static void LogGameFiles(void)
{
    char exe[MAX_PATH] = "", viv[MAX_PATH], num[32]; WIN32_FILE_ATTRIBUTE_DATA fa;
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof exe); const char *name = strrchr(exe, '\\'); name = name ? name + 1 : exe;
    if (!n || !GetFileAttributesExA(exe, GetFileExInfoStandard, &fa)) Log("The game's %s size could NOT be read (Windows error %lu).", n ? name : "speed2.exe", GetLastError());
    else {
        unsigned long long sz = ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
        if (sz == EXE_SIZE_RETAIL) Log("The game: %s, %s bytes (the retail v1.2 size).", name, FmtNum(sz, num));
        else Log("The game: %s, %s bytes - NOT the retail v1.2 size (4,800,512 bytes). Each part below checks the game's code itself.", name, FmtNum(sz, num));
    }
    size_t dir = (size_t)(name - exe); if (dir >= sizeof viv - 20) dir = 0;
    memcpy(viv, exe, dir); strcpy(viv + dir, "SDATA\\sdat.viv");
    if (!GetFileAttributesExA(viv, GetFileExInfoStandard, &fa)) { Log("The song archive SDATA\\sdat.viv could NOT be read (Windows error %lu).", GetLastError()); return; }
    unsigned long long sz = ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    if (sz == VIV_SIZE_MOD) Log("The song archive: SDATA\\sdat.viv, %s bytes (the mod's archive).", FmtNum(sz, num));
    else if (sz == VIV_SIZE_ORIGINAL) Log("The song archive: SDATA\\sdat.viv, %s bytes - UG2's original archive, NOT the mod's: only UG2's own songs can play.", FmtNum(sz, num));
    else Log("The song archive: SDATA\\sdat.viv, %s bytes - NOT the mod's archive (893,398,656 bytes): the added songs may not play.", FmtNum(sz, num));
}
// ---- the rows (read at load, before any write)
static void SaveRows(void)
{
    int ok = 0;
    for (int i = 0; i < 27; i++) {
        uintptr_t row = UA_7ECD80 + 16 * i; char t[80] = {0};
        if (!Accessible(row, 16, FALSE)) { Log("UG2's song table: song %d could NOT be read; its name is left as it is.", i + 1); continue; }
        memcpy(g_rows[i], (void *)row, 16);
        if (!ReadCStr(g_rows[i][0], t, sizeof t) || strcmp(t, kUg2Titles[i])) {
            Log("UG2's song table: song %d is NOT UG2's own (its title reads '%s'); %s.", i + 1, t, (i == SONG_CARRIER_A || i == SONG_CARRIER_B) ? "the added songs cannot play: UG2 picks every song" : "its name is left as it is");
            continue;
        }
        g_rowOk[i] = TRUE; ok++;
    }
    g_carrierRowsOk = g_rowOk[SONG_CARRIER_A] && g_rowOk[SONG_CARRIER_B];
    Log("UG2's song table: %d of 27 songs as expected.", ok);
}
// ---- UG2's own row 19 'Determined' has an empty album in the stock exe; its +8 (0x7ECEB8) is pointed at kUg2Album19 ONCE at load, after
// SaveRows read the row: the saved pointer must be the stock's 00783FE5, else no write and the line says what was there. Readers of that +8:
// the pop-up 0x4AC9F0 (4aca20 mov ecx,[eax+8]; callers 47b108 / 47b202 / 47b026) and the EA Trax screen's fill 0x54B310 (54baaa mov ecx,[eax+0x7ecd88]).
// No stock writer of row 19 (UG2's 0x459D90 writes rows 0 / 1 / 17 only). The four MW albums (8 / 14 / 22 / 23) are
// static strings in kMw: they reach the carrier rows through ApplyCarrier. -> 1 = the album is set, 0 = not
static int WriteAlbum19(void)
{
    uintptr_t p = UA_7ECD80 + 16 * STR_ROW19 + 8; char was[80] = {0}; int r;
    if (!Str_Album19Ok(g_rowOk[STR_ROW19], g_rows[STR_ROW19][2])) {
        ReadCStr(g_rows[STR_ROW19][2], was, sizeof was);
        Log("Song name NOT set: the album of '%s' (it reads '%s', not UG2's own).", kUg2Show[STR_ROW19], was);
        return 0;
    }
    if (!Accessible(p, 4, TRUE)) { Log("Song name NOT set: the album of '%s' (the song table could not be written).", kUg2Show[STR_ROW19]); return 0; }
    r = Str_PutPtr((volatile uint32_t *)p, (uint32_t)(uintptr_t)kUg2Album19);
    if (r == 0) { Log("Song name NOT set: the album of '%s' (the write did not take).", kUg2Show[STR_ROW19]); return 0; }
    return 1;
}
// ---- UG2's own row 0 'Riders' artist pointer (+4 = 0x7ECD84) -> kUg2Artist0. UG2-EXE 0x459D90 (the language fix-up; 459d9c / 459db3) stores the French
// 007896EC or the English 00789C04 there after the asi loaded (no direct call to it in .text: reached through a pointer), hence the write at load AND
// compare-first at every pick (Decide, before anything else: the pop-up 0x4AC9F0 follows the pick at 47b115 / 47b20f). Only a stock pointer is replaced; the
// asi's own = nothing to do; any other pointer = no write, said once. Readers: the pop-up (4aca23 mov edx,[eax+4]), the EA Trax screen's fill (54ba72 mov
// eax,[edx+0x7ecd84]); the stock picker's print reads +0 (46b793 ...). Nothing else of the row is written here. -> 1 = the artist line is set, 0 = not
static int WriteArtist0(void)
{
    uintptr_t p = UA_7ECD80 + 16 * STR_ROW0 + 4; DWORD cur = 0; char was[80] = {0}; int r;
    if (!g_rowOk[STR_ROW0]) { if (!g_artist0Refused++) Log("Song name NOT set: the artist of '%s' (the song's own title was not found).", kUg2Show[STR_ROW0]); return 0; }
    if (!RdDword(p, &cur)) { if (!g_artist0Refused++) Log("Song name NOT set: the artist of '%s' (the song table could not be read).", kUg2Show[STR_ROW0]); return 0; }
    if (cur == (DWORD)(uintptr_t)kUg2Artist0) return 1;                      // the asi's own string is there: nothing to do (the common case at a pick)
    ReadCStr(cur, was, sizeof was);
    if (!Str_Artist0Stock(cur)) { if (!g_artist0Refused++) Log("Song name NOT set: the artist of '%s' (it reads '%s', not UG2's own).", kUg2Show[STR_ROW0], was); return 0; }
    if (!Accessible(p, 4, TRUE)) { if (!g_artist0Refused++) Log("Song name NOT set: the artist of '%s' (the song table could not be written).", kUg2Show[STR_ROW0]); return 0; }
    r = Str_PutPtr((volatile uint32_t *)p, (uint32_t)(uintptr_t)kUg2Artist0);
    if (r == 0) { if (!g_artist0Refused++) Log("Song name NOT set: the artist of '%s' (the write did not take).", kUg2Show[STR_ROW0]); return 0; }
    return 1;
}
// ---- every UG2 row's title pointer (+0) -> kUg2Show[i] (the stock text without its two quotes; rows 0 / 8 / 10 / 11 '(Remix)'), ONCE at load after SaveRows
// checked each row's stock text and after the album / artist writes: only the pointer SaveRows saw is replaced (isStock = cur == the saved pointer; the row's title check must
// have passed); the saved row g_rows[i][0] := the new pointer too, so a carrier (25 / 26) given back after a borrowed song (ApplyCarrier want 0) and the list's rows 25 / 26
// (the table's saved rows) show the title without quotes. UG2-EXE: no .text store names a row's +0 (the 8 '[reg+0x7ecd80]' sites are reads, 0x459D90 writes +4 / +8 only)
// -> once is enough. Readers of +0: the pop-up 0x4AC9F0 (4aca26), the list via g_vtab (BuildView copies the live rows at every open), the stock picker's debug prints.
// -> the number of titles set (written or already the mod's)
static int WriteTitlesUg2(void)
{
    int set = 0;
    for (int i = 0; i < 27; i++) {
        uintptr_t p = UA_7ECD80 + 16 * i; DWORD cur = 0, want = (DWORD)(uintptr_t)kUg2Show[i]; int plan, r;
        if (!g_rowOk[i]) continue;                                            // said by SaveRows
        if (!RdDword(p, &cur)) { Log("Song name NOT set: the title of UG2's song %d (the song table could not be read).", i + 1); continue; }
        plan = Str_PtrPlan(cur, want, cur == g_rows[i][0]);
        if (plan == STR_PLAN_ALREADY) { set++; g_rows[i][0] = want; continue; }
        if (plan == STR_PLAN_REFUSE) { char t[80] = {0}; ReadCStr(cur, t, sizeof t); Log("Song name NOT set: the title of UG2's song %d (it reads '%s', not UG2's own).", i + 1, t); continue; }
        if (!Accessible(p, 4, TRUE)) { Log("Song name NOT set: the title of UG2's song %d (the song table could not be written).", i + 1); continue; }
        r = Str_PutPtr((volatile uint32_t *)p, want);
        if (r != 1) { Log("Song name NOT set: the title of UG2's song %d (the write did not take).", i + 1); continue; }
        set++; g_rows[i][0] = want;
    }
    return set;
}
// ---- rows 0 'Riders on the Storm' / 1 'I Need Speed' album pointers (+8 = 0x7ECD88 / 0x7ECD98) -> kUg2AlbumNfs2 'NFS Underground 2 Exclusive'. UG2-EXE: .data
// holds 0x00789BDC there; UG2's language fix-up 0x459D90 stores [0x7ECF3C] 0x00789694 (English) or [0x7ECF34] 0x007896C0 (French) into both after the asi loaded (459db9 / 459dbe;
// once per start between the load and the first pick = the same call that re-writes row 0's artist) -> WriteArtist0's way: at load, compare-first at every pick (before the
// pop-up that follows it) and at every screen open (before BuildView copies the rows). Only one of UG2's own three pointers is replaced; the asi's own = nothing to do (the
// common case); any other = no write, said once. Readers of +8: the pop-up 0x4AC9F0 (4aca20), the list's fill through g_vtab (BuildView); the pool loop 0x515B46 reads +0xC
// only. A French game gets the English mod string, as row 0's artist does. -> the number of the two albums set at this call
static int WriteAlbumsUg2(void)
{
    int set = 0;
    for (int k = 0; k < 2; k++) {
        int row = k ? STR_ROW_B : STR_ROW_A; uintptr_t p = UA_7ECD80 + 16 * row + 8; DWORD cur = 0, want = (DWORD)(uintptr_t)kUg2AlbumNfs2; char was[80] = {0}; int plan, r;
        if (!g_rowOk[row]) { if (!g_strAlbRefused++) Log("Song name NOT set: the album of '%s' (the song's own title was not found).", kUg2Show[row]); continue; }
        if (!RdDword(p, &cur)) { if (!g_strAlbRefused++) Log("Song name NOT set: the album of '%s' (the song table could not be read).", kUg2Show[row]); continue; }
        plan = Str_PtrPlan(cur, want, Str_AlbumStock(cur));
        if (plan == STR_PLAN_ALREADY) { set++; continue; }                      // the asi's own string is there: nothing to do (the common case at a pick / a screen open)
        ReadCStr(cur, was, sizeof was);
        if (plan == STR_PLAN_REFUSE) { if (!g_strAlbRefused++) Log("Song name NOT set: the album of '%s' (it reads '%s', not UG2's own).", kUg2Show[row], was); continue; }
        if (!Accessible(p, 4, TRUE)) { if (!g_strAlbRefused++) Log("Song name NOT set: the album of '%s' (the song table could not be written).", kUg2Show[row]); continue; }
        r = Str_PutPtr((volatile uint32_t *)p, want);
        if (r == 0) { if (!g_strAlbRefused++) Log("Song name NOT set: the album of '%s' (the write did not take).", kUg2Show[row]); continue; }
        set++;
    }
    return set;
}
// ---- the map: the slot whose image matches the mod's map in all five regions. Said once per start: 'loaded' at the first find (with the stream buffer the game
// sized from it), 'NOT found' at the first miss (the later tries stay silent). A re-find after a reload is silent; a failed re-find = the 'NOT found' line.
static BOOL FindMap(void)
{
    g_mapSlot = -1; g_mapBase = 0;
    for (int n = 0; n < 4; n++) {
        uintptr_t slot = UA_MAPSLOT0 + n * MAPSLOT_STRIDE; DWORD base = 0;
        if (!RdDword(slot + 0x34, &base) || !base) continue;
        if (!Accessible(base, SONG_FILE_LEN, FALSE)) continue;
        unsigned fd[5] = {0}; unsigned m = Song_MapMatch((const uint8_t *)(uintptr_t)base, fd);
        if (m == SONG_M_ALL && g_mapSlot < 0) { g_mapSlot = n; g_mapBase = base; }
    }
    if (g_mapSlot < 0) {
        g_findFails++;
        if (!(g_said & SAID_MAP)) { g_said |= SAID_MAP; Log("The music map was NOT found: the loaded music data is not the mod's (is the mod's SDATA\\sdat.viv installed?). UG2 plays only its own songs."); }
        return FALSE;
    }
    if (!g_mapLogged) {   // once: the stream buffer UG2 sized from the grown table, and whether the sound pool gave it
        DWORD sz = 0, h = 0; BOOL a = RdDword(UA_82BA08, &sz), b = RdDword(UA_82B984, &h); char num[32], tries[48] = ""; g_mapLogged = TRUE;
        if (g_findFails) snprintf(tries, sizeof tries, ", found after %d tries", g_findFails + 1);
        Log("The music map is loaded: %d song entries, as the mod expects (music stream buffer %s bytes)%s.", SONG_NUM_ENTRIES, a ? FmtNum(sz, num) : "?", tries);
        if (b && !h) Log("The music stream buffer could NOT be made (the game's sound memory was full): no music will play.");
    }
    return TRUE;
}
static BOOL MapReady(void)
{
    if (g_mapSlot >= 0) {
        DWORD base = 0;
        if (RdDword(UA_MAPSLOT0 + g_mapSlot * MAPSLOT_STRIDE + 0x34, &base) && base == g_mapBase) return TRUE;
        return FindMap();   // the slot changed (a reload): found again
    }
    return FindMap();
}
// ---- the random draw = the stock's own two calls (46b869 / 46b86f): 0x43BDF0() then r = 0x43C1C0(count) (cdecl), once per random pick, never with 0
static int Rng(int count, void *u)
{
    (void)u; if (count <= 0) return 0;
    ((void (__cdecl *)(void))UA_TIMER)();
    return ((int (__cdecl *)(int))UA_RANDOM)(count);
}
// an added song by its id: its three strings (MW n -> kMw[n], UG1 row r -> kU1[r], Carbon c -> kCb[c])
static const char *const *AddedRow(int id) { return SONG_IS_CB(id) ? kCb[SONG_CB_C(id)] : SONG_IS_U1(id) ? kU1[SONG_U1_R(id)] : kMw[SONG_MW_N(id)]; }
static const char *StText(int st) { return st >= 0 && st < SET_NST ? kSetText[st] : "none"; }   // a soundtrack value -> the bar's word (-1 = no view yet)
#define NAME_LEN 96
// a song's name for the log: '<game> <n> <title>' = the game as the top bar spells it, the song's number in that game's own list (1-based), the title as shown
static const char *SongName(int id, char *out)
{
    if (id < 0 || id >= SONG_NSONG) { snprintf(out, NAME_LEN, "song %d", id); return out; }
    const char *game = SONG_IS_CB(id) ? "Carbon" : SONG_IS_U1(id) ? "Underground" : SONG_IS_MW(id) ? "Most Wanted" : "Underground 2";
    int n = SONG_IS_CB(id) ? SONG_CB_C(id) + 1 : SONG_IS_U1(id) ? SONG_U1_R(id) + 1 : SONG_IS_MW(id) ? SONG_MW_N(id) : id + 1;
    snprintf(out, NAME_LEN, "%s %d %s", game, n, SONG_IS_ADDED(id) ? AddedRow(id)[0] : kUg2Show[id]); return out;
}
// ---- one carrier write (compare-first, read back): want 0 = its own wave + row, id >= 27 = that added song (MW 27..52 / UG1 53..78 / Carbon 79..108). -> 1 ok, 0 failed
static int ApplyCarrier(int c, int want)
{
    uintptr_t node = g_mapBase + kAudOff[c]; unsigned wave = want ? Song_AddedWave(want) : kOrigWave[c];
    DWORD w0 = 0, rb = 0; int ok = 1, waveWritten = 0;
    if (!RdDword(node, &w0)) return 0;
    if ((w0 & 0xFFFF) != wave) {
        if (!Accessible(node, 4, TRUE)) return 0;
        *(volatile DWORD *)node = (w0 & 0xFFFF0000u) | wave; rb = *(volatile DWORD *)node; waveWritten = 1; if ((rb & 0xFFFF) != wave) ok = 0;
    }
    uintptr_t row = UA_7ECD80 + 16 * c; DWORD want3[3], got3[3] = {0, 0, 0};
    if (want) { const char *const *ar = AddedRow(want); want3[0] = (DWORD)(uintptr_t)ar[0]; want3[1] = (DWORD)(uintptr_t)ar[1]; want3[2] = (DWORD)(uintptr_t)ar[2]; }
    else { want3[0] = g_rows[c][0]; want3[1] = g_rows[c][1]; want3[2] = g_rows[c][2]; }
    if (ok) {
        if (!g_rowOk[c]) ok = 0;                                              // the row was not UG2's at load: never written
        else if (!Accessible(row, 12, TRUE)) ok = 0;
        else {
            for (int k = 0; k < 3; k++) { if (*(volatile DWORD *)(row + 4 * k) != want3[k]) *(volatile DWORD *)(row + 4 * k) = want3[k]; got3[k] = *(volatile DWORD *)(row + 4 * k); }
            if (got3[0] != want3[0] || got3[1] != want3[1] || got3[2] != want3[2]) ok = 0;
        }
        if (!ok && waveWritten && Accessible(node, 4, TRUE)) *(volatile DWORD *)node = w0;   // the row failed: the wave put back
    }
    return ok;
}
// ---- the long-song fix: the counter the stub keeps (one per corrected count), reported by the game thread at the next pick (the line names the song the last pick
// or preview started = the stream that started after it)
static void LsReport(void)
{
    LONG n = g_lsFixes; if (n == g_lsSeen) return;
    DWORD fx = g_lsLastFixed; unsigned long secs = fx / 36000u; char nm[NAME_LEN];
    Log("Long song: %s - its length was corrected to %lu:%02lu.", g_lastSong >= 0 ? SongName(g_lastSong, nm) : "(unknown)", secs / 60, secs % 60);
    g_lsSeen = n;
}
// ---- THE SETTINGS FILE PER PROFILE. The key = the profile name char[32] at 0x83A9E0 (UG2-EXE: 44 readers); compared whole (32 bytes) at each pick and each
// screen open: a change -> the file <scripts>\UG2Music\<safe name>.txt is read (absent = defaults); a profile load re-reads it too. Written when the game saves the
// profile: the whole text to <name>.txt.tmp, then MoveFileExA(REPLACE_EXISTING | WRITE_THROUGH). An empty name (before a profile is loaded) = defaults in memory,
// nothing written. The text: settingsfile.h's parse / format (the pure parts).
static BOOL ReadProfileName(char *out)        // the 32 bytes, guarded; FALSE = not readable
{
    memset(out, 0, 32);
    if (!Accessible(UA_BAR_PROFILE, 32, FALSE)) return FALSE;
    memcpy(out, (const void *)UA_BAR_PROFILE, 32); out[31] = 0; return TRUE;
}
static void SettingsRead(void)
{
    static char text[16384]; char safe[40], num[32]; int nm = Set_SafeName(g_profile, 32, safe, sizeof safe);
    Set_Defaults(&g_set); g_setPath[0] = 0; set_soundtrack = g_set.soundtrack; List_ApplyModes(&g_set);   // every set back to its game's defaults until the file says otherwise
    if (g_dirtySteps) Log("%d unsaved song setting change%s dropped: a profile was loaded without saving.", g_dirtySteps, g_dirtySteps == 1 ? "" : "s");
    g_setDirty = FALSE; g_dirtySteps = 0;   // a read = the saved state
    if (!nm) return;                        // no profile name yet: the defaults in memory, no file read or written until a profile is loaded
    snprintf(g_setPath, sizeof g_setPath, "%s\\%s.txt", g_setDir, safe);
    HANDLE h = CreateFileA(g_setPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) Log("Profile '%s': no settings file yet - the defaults (soundtrack Underground 2). The file is made at the first save.", g_profile);
        else Log("Profile '%s': the settings file could NOT be opened (Windows error %lu) - the defaults.", g_profile, e);
        return;
    }
    DWORD rd = 0; BOOL ok = ReadFile(h, text, sizeof text - 1, &rd, NULL); CloseHandle(h);
    if (!ok) { Log("Profile '%s': the settings file could NOT be read (Windows error %lu) - the defaults.", g_profile, GetLastError()); return; }
    text[rd] = 0; Set_Parse(&g_set, text, (int)rd); set_soundtrack = g_set.soundtrack; List_ApplyModes(&g_set);   // the file's mode lines -> the live modes, All Trax's own set too (header 2 = as written; header 1 / none = the shared settings copied into All Trax, below)
    int listUnread = 0;
    if (g_set.headerVer < 2) {   // a version-1 file (header 1 / none): one shared setting per song for every category -> copied into every category, All Trax included. The added games: their lines were copied when the modes were applied (g_set.inherited). UG2's 27 songs: their shared modes live in UG2's list 0x83ACB8 (loaded by UG2's own 0x5344D0 before the profile-load hook runs; the same guarded read as BuildView); a mode that is not the default becomes an 'ALL UG2 nn' line in memory, written as header 2 at the next save = the copy is made once
        if (Accessible(UA_LIST, 27 * 8, FALSE)) { for (int p = 0; p < 27; p++) { int song = *(volatile int *)(UA_LIST + 8 * p); unsigned char m = *(volatile unsigned char *)(UA_LIST + 8 * p + 4); if (song < 0 || song >= SONG_NUG2 || m > 3) continue; if (g_set.allMode[song] >= 0) continue; list_allMode[song] = m; if (m != Song_AllDefault(song)) g_set.allMode[song] = (signed char)m; } }
        else listUnread = 1;
    }
    Log("Profile '%s': song settings read from UG2Music\\%s.txt (%s bytes, %d songs set, soundtrack %s).", g_profile, safe, FmtNum(rd, num), g_set.nSong + g_set.nAll, kSetText[g_set.soundtrack]);
    if (g_set.badValue) Log("The settings file's soundtrack line was NOT understood - Underground 2 is used.");
    if (g_set.badSong == 1) Log("1 line of the settings file was NOT understood - it is kept as it is.");
    else if (g_set.badSong > 1) Log("%d lines of the settings file were NOT understood - they are kept as they are.", g_set.badSong);
    if (g_set.dropped) Log("The settings file has more than 64 other lines - the rest were NOT kept.");
    if (g_set.headerVer < 2) Log("The settings file is from an older version: its song settings were copied into every category; the next save writes the new form.");
    if (listUnread) Log("UG2's song list could NOT be read: UG2's songs keep All Trax's defaults in that copy.");
}
static void SettingsSync(void)    // the live name vs the one last read; a change -> the file for the new name
{
    char live[32];
    if (!ReadProfileName(live)) { if (!(g_said & SAID_PROFILE)) { g_said |= SAID_PROFILE; Log("The profile name could NOT be read - the song settings in memory stay."); } return; }
    if (g_profileEver && !memcmp(live, g_profile, 32)) return;
    memcpy(g_profile, live, 32); g_profileEver = TRUE;
    SettingsRead();
}
static BOOL SettingsWrite(void)
{
    static char text[SET_MAXLINES * SET_LINELEN + 256 + SONG_NSONG * 18]; char tmp[MAX_PATH + 8], safe[40], num[32];   // + the 109 ALL lines (18 B at most each)
    if (!g_setPath[0]) { Log("The game saved, but the song settings were NOT written: no profile name yet."); return FALSE; }
    Set_SafeName(g_profile, 32, safe, sizeof safe);
    if (!g_setDirOk) { Log("The game saved, but the song settings were NOT written: the settings folder is missing."); return FALSE; }
    int len = Set_Format(&g_set, text, sizeof text);
    if (!len) { Log("The game saved, but the song settings were NOT written: the text was too long."); return FALSE; }
    snprintf(tmp, sizeof tmp, "%s.tmp", g_setPath);
    HANDLE h = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { Log("The game saved, but the song settings were NOT written: could not create %s.txt.tmp (Windows error %lu).", safe, GetLastError()); return FALSE; }
    DWORD wr = 0; BOOL ok = WriteFile(h, text, (DWORD)len, &wr, NULL) && wr == (DWORD)len; DWORD e1 = ok ? 0 : GetLastError(); FlushFileBuffers(h); CloseHandle(h);
    if (ok) { ok = MoveFileExA(tmp, g_setPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH); if (!ok) e1 = GetLastError(); }
    if (!ok) { Log("The game saved, but the song settings were NOT written: writing %s.txt FAILED (Windows error %lu).", safe, e1); return FALSE; }
    int lines = 0; for (int k = 1; k <= 30; k++) { if (k <= 26 && g_set.mwMode[k] >= 0) lines++; if (k <= 26 && g_set.u1Mode[k] >= 0) lines++; if (g_set.cbMode[k] >= 0) lines++; }   // the song lines written (the four game sets + All Trax's)
    for (int id = 0; id < SONG_NSONG; id++) if (g_set.allMode[id] >= 0) lines++;
    g_setDirty = FALSE; g_dirtySteps = 0;                            // the file = the memory now
    Log("The game saved profile '%s': song settings written to UG2Music\\%s.txt (%s bytes, %d songs set, soundtrack %s).", g_profile, safe, FmtNum((unsigned long long)len, num), lines, kSetText[g_set.soundtrack]);
    return TRUE;
}
// a step on the bar or on an added song's row = memory only; the file waits for the game's save (UG2's own modes do the same: the profile block is written
// at a save, read at a load, lost on a quit without saving)
static void SettingsDirty(void) { g_setDirty = TRUE; g_dirtySteps++; }
static void SettingsInit(void)
{
    snprintf(g_setDir, sizeof g_setDir, "%s\\UG2Music", g_dir);
    BOOL made = CreateDirectoryA(g_setDir, NULL); DWORD e = made ? 0 : GetLastError(); g_setDirOk = made || e == ERROR_ALREADY_EXISTS;
    if (g_setDirOk) Log("Settings folder: %s (%s).", g_setDir, made ? "created" : "found");
    else Log("Settings folder %s could NOT be created (Windows error %lu): song settings cannot be saved.", g_setDir, e);
    SettingsSync();
}
// ---- THE LABEL. [screen+0x104] = the top bar text object TRACKMODE_TYPE (the ctor 553cf1 push 0xCA74A2FA .. 553d1c 'cmp [eax+0x18],2' .. 553d29
// mov [ebx+0x104],eax). Off = UG2's own label by hash via 0x50C900(obj, hash), as UG2's 0x524120 sets Random / Sequential; the soundtracks have no label ->
// 0x537BE0(obj, "%s", text) = FEPrintf on the object (-> 0x537B30: the text set, +0x1C |= 2); +0x18 must read 2 (a text object) first.
static void SetLabel(uintptr_t screen)
{
    DWORD obj = 0, kind = 0; int snd = g_set.soundtrack;
    if (!RdDword(screen + UA_BAR_OBJ_OFF, &obj) || !obj) { TraxProblem(SAID_E_BARTEXT, "the top bar's text", "the screen has no text object for it", "The bar keeps the words it had."); return; }
    if (!RdDword(obj + 0x18, &kind)) { TraxProblem(SAID_E_BARTEXT, "the top bar's text", "its text object could not be read", "The bar keeps the words it had."); return; }
    if (snd == SET_ST_OFF) { ((void (__cdecl *)(DWORD, DWORD))UA_BAR_SETLABEL)(obj, SET_HASH_OFF); return; }   // Off is out of the cycle = unreachable; every shown value is printed text
    if (kind != 2) { TraxProblem(SAID_E_BARTEXT, "the top bar's text", "the object there is not a text object", "The bar keeps the words it had."); return; }
    int r = ((int (__cdecl *)(DWORD, const char *, ...))UA_BAR_PRINTF_OBJ)(obj, "%s", kSetText[snd]);
    if (r <= 0) TraxProblem(SAID_E_BARTEXT, "the top bar's text", "the game's print returned 0", "The bar keeps the words it had.");
}
// the trigger sound as the handler plays it (5241f5-524218 / 565db1-565dca): the byte [0x8381AB] (left) / [0x8381C6] (right), movsx; -1 = none; 0x458520 thiscall(ecx = [0x82B884], id)
static void PlayUiSound(int dir)
{
    DWORD mgr = 0, b = 0;
    if (!RdByte(dir < 0 ? UA_BAR_SND_LEFT : UA_BAR_SND_RIGHT, &b) || !RdDword(UA_BAR_SOUNDMGR, &mgr) || !mgr) return;
    int id = (int)(signed char)b; if (id == -1) return;
    ((void (__attribute__((thiscall)) *)(DWORD, int))UA_BAR_SOUND)(mgr, id);
}
static void DoStep(uintptr_t screen, int dir, BOOL setLabel)
{
    SettingsSync();
    int old = g_set.soundtrack, nw = Set_Step(old, dir); g_set.soundtrack = nw; set_soundtrack = nw;
    Log("Soundtrack: %s -> %s.", StText(old), StText(nw));
    SettingsDirty();   // memory only; the file waits for the game's save
    if (setLabel) SetLabel(screen); else g_labelAfterStep = TRUE;    // the mouse path reaches the hooked 0x524120 right after (565e1d)
    RefillList(screen);                                              // the rows follow the bar at once
}
void __cdecl Bar_Trigger(void *screen, unsigned hash)     // from the trigger stub = the whole handler 0x5241E0 up to its tail
{
    int dir = hash == SET_HASH_LTRIGGER ? -1 : hash == SET_HASH_RTRIGGER ? 1 : 0;
    if (!dir) { SetLabel((uintptr_t)screen); return; }    // neither trigger: no step, the label re-set
    PlayUiSound(dir); DoStep((uintptr_t)screen, dir, TRUE);
}
// ==== THE WORDS OF EVERY EA TRAX ROW MOVED RIGHT (rowwords.h) ====
// At every screen open (the label routine from the ctor patch 0x5540BF: after BuildView, before the fill 0x54B310 at 0x55412C) the 12 word objects (artist / title / album of the 4 slots)
// are looked up the ctor's own way (package = 0x52CEF0([screen+4]) cdecl as 553ce4-553cea; object = 0x517F80(package, hash) thiscall as 553cf1-553cf8; [obj+0x18] == 2 as 553d1c) and
// their x (data = [obj+0x2C], x = data+0x1C = the FNG's 'SA' copy) := -246.2 when it reads the FNG's -253.0 (-246.2 already = counted; anything else = skipped, never written). The
// game puts the FNG's x back at every open (the package load), so the write repeats per open; nothing of the screen's own code / scripts moves these 12 while it is open (their
// tracks = colour only; 0 position stores in 45 functions). The two lookups' heads are byte-checked at the install (a mismatch -> the words stay where UG2 puts them; the top bar
// installs either way). Every read guarded (Accessible / RdDword); the one write = the 12 floats per open.
static BOOL g_wordsHeadsOk;
static DWORD Words_Object(DWORD pkg, uint32_t hash) { return ((DWORD (__attribute__((thiscall)) *)(DWORD, DWORD))UA_WORDS_OBJBYHASH)(pkg, hash); }
static void Words_MoveWords(uintptr_t screen)
{
    DWORD name = 0, pkg = 0; int skipped = 0;
    if (!g_wordsHeadsOk) return;                                            // said at the start: the words stay where UG2 puts them
    if (!RdDword(screen + 4, &name) || !name) { TraxProblem(SAID_E_WORDS, "the rows' text shift", "the screen has no package name", "The rows' text stays where UG2 puts it."); return; }
    pkg = ((DWORD (__cdecl *)(DWORD))UA_WORDS_PACKAGE)(name);
    if (!pkg) { TraxProblem(SAID_E_WORDS, "the rows' text shift", "the screen's package was not found", "The rows' text stays where UG2 puts it."); return; }
    for (int i = 0; i < WORDS_NWORDS; i++) {
        uint32_t h = Words_WordHash(i); DWORD obj = Words_Object(pkg, h), kind = 0, data = 0;
        if (!obj) { skipped++; continue; }
        if (!RdDword(obj + WORDS_OBJ_KIND_OFF, &kind) || kind != 2) { skipped++; continue; }
        if (!RdDword(obj + WORDS_OBJ_DATA_OFF, &data) || !data || !Accessible(data, WORDS_DATA_LEN, TRUE)) { skipped++; continue; }
        volatile float *px = (volatile float *)(data + WORDS_DATA_X_OFF); float x = *px; int plan = Words_Plan(x);
        if (plan == WORDS_PLAN_WRITE) { *px = WORDS_X_NEW; float rb = *px; if (rb != WORDS_X_NEW) skipped++; }
        else if (plan != WORDS_PLAN_ALREADY) skipped++;
    }
    if (skipped) { char why[80]; snprintf(why, sizeof why, "%d of the 12 word objects could not be moved", skipped); TraxProblem(SAID_E_WORDS, "the rows' text shift", why, "Some rows' text stays where UG2 puts it."); }
}
void __cdecl Bar_MouseStep(void *screen, int dir)         // from the patched inline blocks of 0x565D90 (edi = the screen)
{
    PlayUiSound(dir); DoStep((uintptr_t)screen, dir, FALSE);
}
void __cdecl Bar_LabelCdecl(void *screen)                 // from the label stub (0x524120: 565e1d after a mouse step) and from the ctor patch (0x5540BF: the screen open)
{
    int screenOpen = !g_labelAfterStep; g_labelAfterStep = FALSE;
    SettingsSync(); SetLabel((uintptr_t)screen);
    if (screenOpen) {   // rows 0 / 1's album pointers compare-first BEFORE the view copies the rows; the view the fill 0x54B310 reads next (0x55412C follows this patch) + its row bound; never after a bar step mid-screen (the rows shown keep their ids); the 12 word objects' x := -246.2, at the open only
        WriteAlbumsUg2(); BuildView(); Words_MoveWords((uintptr_t)screen);
        Log("EA Trax opened: %s, %d songs.", StText(g_viewSt), g_viewN);
    }
}
// the carriers given back under 'UG2' / 'Off' (compare-first: no write when they already hold their own wave + row); g_last cleared (nothing of the mod's plays next)
static void GiveBackCarriers(void)
{
    if (g_mapSlot < 0) return;             // nothing borrowed yet (no map found so far)
    if (!MapReady()) return;               // the map moved and was not found again: nothing given back
    ApplyCarrier(SONG_CARRIER_A, 0); ApplyCarrier(SONG_CARRIER_B, 0);
    g_st.last = -1;
}
// a pick left to UG2's own picker for a reason the player should know: said once per reason per start
static void LeftToUg2(unsigned bit, const char *why)
{
    if (g_said & bit) return;
    g_said |= bit; Log("Song pick left to UG2: %s.", why);
}
// ---- the decision: -> a slot 0..26, -1 (no song), or -2 (let the stock picker run). Decide = the wrapper that folds the 'no song' line (the first of a run is said
// inside; when a later pick answers otherwise, ONE count line); DecideInner = the decision itself (it sets g_noneHit, the fold gates its first line).
static int DecideInner(void *player, unsigned ret);
int __cdecl Decide(void *player, unsigned ret)
{
    g_noneHit = 0; int r = DecideInner(player, ret), fc = 0;
    if (Shuf_Fold(&g_noneFold, g_noneHit, &fc) == 2) Log("That run had %d picks without a song.", fc);
    return r;
}
static int DecideInner(void *player, unsigned ret)
{
    DWORD gm = 0, fe = 0xFFFFFFFF, st = 0xFFFFFFFF, cur = 0xFFFFFFFF, gate = 0xFFFFFFFF, rs = 0xFFFFFFFF, r88 = 0xFFFFFFFF, ra8 = 0xFFFFFFFF;
    uintptr_t pl = (uintptr_t)player; char nm[NAME_LEN]; (void)ret;
    WriteArtist0();   // row 0's artist line before the pop-up that follows this pick (compare-first; a write only after UG2's own re-write)
    WriteAlbumsUg2(); // rows 0 / 1's album line the same way
    SettingsSync(); int snd = g_set.soundtrack; set_soundtrack = snd;   // the soundtrack chosen on the screen (the file re-read when the profile changed)
    int dec = Set_Decision(snd);                                        // -2 the stock picker (Underground 2) / -1 no song (Off) / 0 the mod's own pick (every other soundtrack)
    if (dec == SET_DEC_NONE) {                                          // 'Off' = -1 every time = UG2's own no-song path (47b0d7 / 47b1ce); said once per run, then every 10 s
        DWORD now = GetTickCount(); g_offRun++; GiveBackCarriers();
        if (g_offRun == 1 || now - g_offLastLog >= 10000) { g_offLastLog = now; Log("No song: the soundtrack is Off."); }
        return -1;
    }
    g_offRun = 0;
    if (!g_rngOk || !g_carrierRowsOk) { LeftToUg2(SAID_OFF, !g_rngOk ? "the mod is off: the random-draw check failed" : "the mod is off: the song-table check failed"); return -2; }
    if (!RdDword(UA_82B884, &gm) || !gm || !RdDword(gm + 0x90, &fe) || !RdDword(UA_89E7B0, &st) || !RdDword(pl + 0x14, &cur) || !RdByte(pl + 0xA5, &gate) || !RdDword(pl + 0x9C, &rs) || !RdDword(pl + 0x88, &r88) || !RdDword(pl + 0xA8, &ra8)) {
        LeftToUg2(SAID_PLAYER, "the game's music player could NOT be read"); return -2; }
    int ctx = (fe == 1 || st == 8) ? SONG_CTX_FE : SONG_CTX_IG;                                  // 46b70d-46b71c
    const char *ctxWord = ctx == SONG_CTX_FE ? "menu" : "race";
    if (dec == SET_DEC_STOCK) {                                                                // 'Underground 2' = the stock picker decides every pick (its own state, its own Riders first, its own draw)
        GiveBackCarriers(); if (ctx == SONG_CTX_FE) g_prev88 = (int)r88; LsReport();
        Log("Now playing (%s): UG2's own pick (Underground 2).", ctxWord);
        return -2;
    }
    if (ctx == SONG_CTX_FE && !(gate == 1 || st == 8)) { Log("Menu song left to UG2: UG2 is not ready for a menu song yet."); return -2; }   // 46b8de-46b8ea: the stock picker returns -1 there
    if (!MapReady()) { LeftToUg2(SAID_MAP, "the music map is NOT found"); return -2; }
    int listSong[27]; unsigned char listMode[27];
    if (!Accessible(UA_LIST, 27 * 8, FALSE)) { LeftToUg2(SAID_LIST, "UG2's song list could NOT be read"); return -2; }
    for (int p = 0; p < 27; p++) { listSong[p] = *(volatile int *)(UA_LIST + 8 * p); listMode[p] = *(volatile unsigned char *)(UA_LIST + 8 * p + 4); }
    LsReport();
    if (Song_RidersSignal(ctx, r88, g_prev88)) {                                            // UG2's Riders-first signal -> the stock picker answers
        Song_PickInfo ti; Song_TakeSong(&g_st, ctx, 0, listSong, listMode, &ti);
        Song_Plan plan = Song_CarrierPlan(0, g_st.last, g_st.lastCarrier);
        for (int i = 0; i < 2; i++) if (plan.want[i] >= 0) ApplyCarrier(SONG_CARRIER_A + i, plan.want[i]);
        g_st.last = 0; g_prev88 = 1; g_lastSong = 0;
        Log("Now playing (menu): %s - UG2's first song (%s).", SongName(0, nm), kSetText[snd]);
        return -2;
    }
    if (ctx == SONG_CTX_FE) g_prev88 = (int)r88;
    Song_PickInfo inf; int id = Song_Choose(&g_st, ctx, rs == 0, listSong, listMode, Rng, NULL, &inf);
    if (id < 0) { g_noneHit = 1; if (g_noneFold.run == 0) Log("No song (%s): every song of %s is Off for the %s.", ctxWord, kSetText[snd], ctx == SONG_CTX_FE ? "menus" : "races"); return -1; }   // -1 as the stock would (46b72a / 46b8f8)
    Song_Plan plan = Song_CarrierPlan(id, g_st.last, g_st.lastCarrier);
    if (plan.carrier >= 0) {                                                                 // the returned carrier first: on a failure nothing else is touched
        int i = plan.carrier - SONG_CARRIER_A;
        if (!ApplyCarrier(plan.carrier, plan.want[i])) { char why[NAME_LEN + 64]; snprintf(why, sizeof why, "playing %s FAILED (the music map could not be changed)", SongName(id, nm)); LeftToUg2(SAID_WRITE, why); return -2; }
    }
    for (int i = 0; i < 2; i++) if (SONG_CARRIER_A + i != plan.carrier && plan.want[i] >= 0) ApplyCarrier(SONG_CARRIER_A + i, plan.want[i]);
    g_st.last = plan.ret; if (plan.carrier >= 0) g_st.lastCarrier = plan.carrier;
    g_lastSong = id;
    Log("Now playing (%s): %s - %s, shuffle %s.", ctxWord, SongName(id, nm), kSetText[snd], rs == 0 ? "off" : "on");
    return plan.ret;
}
// the stub: ecx = the player (fastcall). Decide(player, caller's return address) FIRST: eax != -2 -> ret with it (the stack as the caller left it); -2 -> jmp the
// trampoline (the stock picker's 5 head bytes + jmp 0x46B6F5) with ecx = the player again: the stock picker runs and returns to the caller itself.
extern void PickStub(void);
__asm__(".text\n.globl _PickStub\n_PickStub:\n"
        "  pushl %ecx\n"
        "  pushl 4(%esp)\n"
        "  pushl %ecx\n"
        "  call _Decide\n"
        "  addl $8, %esp\n"
        "  popl %ecx\n"
        "  cmpl $-2, %eax\n"
        "  je 1f\n"
        "  ret\n"
        "1:\n"
        "  jmp *_g_tramp\n");
// the long-song stub: replaces 'mov eax,[ebp-0x10]; mov [edi],eax' at 0x727CD8: the value; when the tag's byte length [ebp-0x8] is 3 and the value is
// negative (the reader's sign step) -> the raw value kept, masked to 24 bits, the fixed value kept, the counter up; store to [edi]; jmp 0x727CDD (= jmp 0x727D99).
// No call, no I/O, no stack use; eax is the only register touched (as the original); the flags are not read at 0x727D99 (lea / push / call).
extern void LsStub(void);
__asm__(".text\n.globl _LsStub\n_LsStub:\n"
        "  movl -0x10(%ebp), %eax\n"
        "  cmpl $3, -0x8(%ebp)\n"
        "  jne 1f\n"
        "  testl %eax, %eax\n"
        "  jns 1f\n"
        "  movl %eax, _g_lsLastRaw\n"
        "  andl $0x00FFFFFF, %eax\n"
        "  movl %eax, _g_lsLastFixed\n"
        "  incl _g_lsFixes\n"
        "1:\n"
        "  movl %eax, (%edi)\n"
        "  jmp *_g_lsResume\n");
// the top bar's stubs. The trigger stub replaces the handler 0x5241E0 (thiscall(screen, hash) ret 4): the original's 'push ebx; push esi; mov esi,ecx' re-done, the trigger
// function (screen, hash) called, then a jump to the handler's own tail 0x524230 = 'mov ecx,[0x82B884]; call 0x479220; pop esi; pop ebx; ret 4' (UG2's list set-up as after a flip;
// the stack exactly as the tail expects). The label stub replaces 0x524120 (thiscall(screen), a leaf): the label routine (screen); ret.
extern void BarTrigStub(void); extern void BarLabelStub(void);
__asm__(".text\n.globl _BarTrigStub\n_BarTrigStub:\n"
        "  pushl %ebx\n"
        "  pushl %esi\n"
        "  movl %ecx, %esi\n"
        "  pushl 12(%esp)\n"
        "  pushl %ecx\n"
        "  call _Bar_Trigger\n"
        "  addl $8, %esp\n"
        "  jmp *_g_trigTail\n");
__asm__(".text\n.globl _BarLabelStub\n_BarLabelStub:\n"
        "  pushl %ecx\n"
        "  call _Bar_LabelCdecl\n"
        "  addl $4, %esp\n"
        "  ret\n");
// a part's site check: the FIRST site that did not match is kept (its address and up to 16 bytes found) for that part's 'NOT installed' line
typedef struct { uintptr_t addr; char found[64]; } SiteFail;
static BOOL CheckSite(uintptr_t a, const unsigned char *exp, int n, SiteFail *f)
{
    BOOL ok = Accessible(a, n, FALSE) && !memcmp((const void *)a, exp, n);
    if (!ok && !f->addr) { f->addr = a; Hex(a, f->found, n > 16 ? 16 : n); }
    return ok;
}
static BOOL WriteCode(uintptr_t a, const unsigned char *bytes, int n, const char *part)
{
    DWORD old = 0;
    if (!VirtualProtect((void *)a, n, PAGE_EXECUTE_READWRITE, &old)) { Log("%s: a write to the game's code at 0x%06lX FAILED (Windows error %lu).", part, (unsigned long)a, GetLastError()); return FALSE; }
    memcpy((void *)a, bytes, n); VirtualProtect((void *)a, n, old, &old); FlushInstructionCache(GetCurrentProcess(), (void *)a, n);
    if (memcmp((const void *)a, bytes, n)) { Log("%s: a write to the game's code at 0x%06lX FAILED (the read-back differs).", part, (unsigned long)a); return FALSE; }
    return TRUE;
}
// the two hooks + three patches (settingsfile.h: the sites' bytes, the patch model), all or nothing: every site, the tail, the join, the continuation and the three callees
// are byte-checked first; one mismatch = nothing written (the top bar keeps UG2's Random / Sequential; the soundtrack = the settings file's, default Underground 2)
static SiteFail g_wordsFail;   // the row-word lookups' first mismatch (named in the row text shift's line)
static void InstallBar(void)
{
    SiteFail f = { 0, "" }; BOOL ok = TRUE;
    ok &= CheckSite(UA_BAR_TRIG, kBarTrigHead, 9, &f);
    ok &= CheckSite(UA_BAR_TRIG_TAIL, kBarTrigTail, 16, &f);
    ok &= CheckSite(UA_BAR_LABEL, kBarLabelHead, 12, &f);
    ok &= CheckSite(UA_BAR_MOUSE1, kBarMouse1, SET_MOUSE1_LEN, &f);
    ok &= CheckSite(UA_BAR_MOUSE2, kBarMouse2, SET_MOUSE2_LEN, &f);
    ok &= CheckSite(UA_BAR_MOUSE_JOIN, kBarMouseJoin, 11, &f);
    ok &= CheckSite(UA_BAR_CTOR, kBarCtor, SET_CTOR_LEN, &f);
    ok &= CheckSite(UA_BAR_CTOR_AFTER, kBarCtorAfter, 11, &f);
    ok &= CheckSite(UA_BAR_PRINTF_OBJ, kBarPrintfHead, 12, &f);
    ok &= CheckSite(UA_BAR_SETLABEL, kBarSetLabelHead, 12, &f);
    ok &= CheckSite(UA_BAR_SOUND, kBarSoundHead, 9, &f);
    { BOOL hA = CheckSite(UA_WORDS_PACKAGE, kWordsPackageHead, 11, &g_wordsFail), hB = CheckSite(UA_WORDS_OBJBYHASH, kWordsObjHead, 13, &g_wordsFail);
      g_wordsHeadsOk = hA && hB; }   // the ctor's own two lookups: NOT part of the bar's all-or-nothing (a mismatch -> the words stay where UG2 puts them)
    if (!ok) { Log("EA Trax top bar NOT installed: the game's code at 0x%06lX is not the retail exe's (found%s). The bar stays UG2's Random / Sequential; the list, Shuffle and Default need it.", (unsigned long)f.addr, f.found); return; }
    unsigned char p1[SET_MOUSE1_LEN], p2[SET_MOUSE2_LEN], p3[SET_CTOR_LEN], j1[5] = { 0xE9, 0, 0, 0, 0 }, j2[5] = { 0xE9, 0, 0, 0, 0 };
    Set_MakePatch(0, UA_BAR_MOUSE1, (uint32_t)(uintptr_t)Bar_MouseStep, p1); Set_MakePatch(1, UA_BAR_MOUSE2, (uint32_t)(uintptr_t)Bar_MouseStep, p2); Set_MakePatch(2, UA_BAR_CTOR, (uint32_t)(uintptr_t)Bar_LabelCdecl, p3);
    *(DWORD *)(j1 + 1) = (DWORD)((uintptr_t)BarTrigStub - (UA_BAR_TRIG + 5)); *(DWORD *)(j2 + 1) = (DWORD)((uintptr_t)BarLabelStub - (UA_BAR_LABEL + 5));
    int w = 0;
    w += WriteCode(UA_BAR_TRIG, j1, 5, "EA Trax top bar"); w += WriteCode(UA_BAR_LABEL, j2, 5, "EA Trax top bar");
    w += WriteCode(UA_BAR_MOUSE1, p1, SET_MOUSE1_LEN, "EA Trax top bar"); w += WriteCode(UA_BAR_MOUSE2, p2, SET_MOUSE2_LEN, "EA Trax top bar"); w += WriteCode(UA_BAR_CTOR, p3, SET_CTOR_LEN, "EA Trax top bar");
    g_barInstalled = w == 5;
    if (g_barInstalled) Log("EA Trax top bar: installed.");
    else Log("EA Trax top bar NOT fully installed: %d of 5 writes FAILED (the lines above).", 5 - w);
}
static void LogWordsPart(void)   // the row text shift: it runs from the top bar's screen-open hook, with the two lookups' heads checked by InstallBar
{
    if (!g_barInstalled) Log("Row text shift NOT installed: it needs the EA Trax top bar.");
    else if (g_wordsHeadsOk) Log("Row text shift: installed.");
    else Log("Row text shift NOT installed: the game's code at 0x%06lX is not the retail exe's (found%s). The rows' text stays where UG2 puts it.", (unsigned long)g_wordsFail.addr, g_wordsFail.found);
}
// ==== THE LIST, THE ADDED SONGS' MODES, THE PREVIEW (tracklist.h) ====
// the fill's row bound (imm8 at 0x54BB38) := the view's N; checked first (0x54BB36 = 83 FF), compare-first
static void WriteBound(void)
{
    DWORD o = 0, c0 = 0, c1 = 0; int n = g_viewN;
    if (!RdByte(UA_LIST_BOUND - 2, &c0) || !RdByte(UA_LIST_BOUND - 1, &c1) || c0 != 0x83 || c1 != 0xFF || !RdByte(UA_LIST_BOUND, &o)) { TraxProblem(SAID_E_BOUND, "the row count", "the game's code at 0x54BB36 is not what the mod expects", "The list shows the wrong number of rows."); return; }
    if (n < 0 || n > LIST_IMM8_MAX) { char why[64]; snprintf(why, sizeof why, "%d rows is outside 0..%d", n, LIST_IMM8_MAX); TraxProblem(SAID_E_BOUND, "the row count", why, "The list shows the wrong number of rows."); return; }
    if ((int)o == n) return;                                             // already the count the fill builds
    unsigned char b = (unsigned char)n;
    WriteCode(UA_LIST_BOUND, &b, 1, "EA Trax list");
}
// the view + the table from the live list / table (guarded reads). Called at every screen open (the label routine, BEFORE the fill), after a bar step or a reset and once at load (InstallList).
static void BuildView(void)
{
    int listSong[27]; unsigned char listMode[27]; List_TabRow live[27], saved[27]; int bad = 0;
    if (!Accessible(UA_LIST, 27 * 8, FALSE) || !Accessible(UA_7ECD80, 27 * 16, FALSE)) { TraxProblem(SAID_E_VIEW, "the list's rows", "UG2's song list or song table could not be read", "The list keeps its last rows."); return; }
    for (int p = 0; p < 27; p++) { listSong[p] = *(volatile int *)(UA_LIST + 8 * p); listMode[p] = *(volatile unsigned char *)(UA_LIST + 8 * p + 4); }
    memcpy(live, (const void *)UA_7ECD80, 27 * 16); memcpy(saved, g_rows, 27 * 16);
    int st = g_set.soundtrack; List_BuildTable(live, saved, kMw, kU1, kCb, g_vtab); int n = List_BuildView(st, listSong, listMode, g_view, &bad);   // the table: UG2's 27, MW (rows 27..52), UG1 (53..78), Carbon (79..108)
    g_viewN = n; g_viewSt = st;
    if (g_listInstalled) WriteBound();
}
// ==== THE ROWS FOLLOW THE TOP BAR AT ONCE. UG2 builds the rows once per screen open (the ctor -> the fill); after a bar step the mod rebuilds them with UG2's own
// code: the list control at screen+0x4C (vtable checked = 0x795CB4), its row list destroyed by 0x4358E0 (the helper its own dtor uses: each row unlinked, then deleted through
// its dtor 0x4CBB60 -> 0x531610 -> the string nodes freed the same way), the four fields zeroed as the ctor had them, the view rebuilt (+ the bound := N), then the fill 0x54B310
// with [screen+0x124] = 0 for the call (54b32b 'mov eax,[ecx+0x124]' / 54b341 'jle 0x54b994' = the header part skipped; it built the 4 header items at the open and must not run
// twice) = the row loop + the refresh [vtable+4] + the scrollbar / arrows tail, exactly as at a screen open; +0x124 restored. The selected row (+0x24 = screen+0x70) and the first
// visible (+0x28) are set by the fill to the new first row (54ba1f / 54ba01 when 0).
static void RefillList(uintptr_t screen)
{
    uintptr_t lc = screen + LIST_LISTCTL_OFF; DWORD vt = 0, cnt0 = 0, sel0 = 0, gate = 0, cnt1 = 0, sel1 = 0, first1 = 0, hdr = 0;
    if (!g_listInstalled) return;                                         // said at the start: the list is not installed
    if (!RdDword(lc, &vt) || vt != LIST_LISTCTL_VTABLE || !RdDword(lc + LIST_ROWCOUNT_OFF, &cnt0) || !RdDword(lc + LIST_SELECTED_OFF, &sel0) || !RdDword(lc + 0x18, &hdr) || !RdDword(screen + LIST_HEADERGATE_OFF, &gate)
        || !Accessible(lc + LIST_ROWLIST_OFF, 0x20, TRUE) || !Accessible(screen + LIST_HEADERGATE_OFF, 4, TRUE)) {
        TraxProblem(SAID_E_REFILL, "the list refresh", "the screen's list control is not UG2's or could not be read", "The list follows the bar at the next screen open."); return;
    }
    if (gate != LIST_HEADERGATE_VALUE) { TraxProblem(SAID_E_REFILL, "the list refresh", "the screen's header count is not 4", "The list follows the bar at the next screen open."); return; }
    BuildView();                                                                                 // the view + the bound := N for the fill below
    ((void (__attribute__((thiscall)) *)(uintptr_t))UA_LIST_DELLIST)(lc + LIST_ROWLIST_OFF);          // every row unlinked + deleted (UG2's own helper and dtors)
    *(volatile DWORD *)(lc + LIST_ROWCOUNT_OFF) = 0; *(volatile DWORD *)(lc + LIST_FLAG_OFF) = 0; *(volatile DWORD *)(lc + LIST_SELECTED_OFF) = 0; *(volatile DWORD *)(lc + LIST_FIRSTVIS_OFF) = 0;
    *(volatile DWORD *)(screen + LIST_HEADERGATE_OFF) = 0;                                        // the fill's header part skipped
    ((void (__attribute__((thiscall)) *)(uintptr_t))UA_LIST_FILL)(screen);                            // the row loop over the view + the refresh + the tail
    *(volatile DWORD *)(screen + LIST_HEADERGATE_OFF) = gate;
    RdDword(lc + LIST_ROWCOUNT_OFF, &cnt1); RdDword(lc + LIST_SELECTED_OFF, &sel1); RdDword(lc + LIST_FIRSTVIS_OFF, &first1);
    if ((int)cnt1 != g_viewN || !sel1 || sel1 != first1) TraxProblem(SAID_E_REFILL, "the list refresh", "the rebuilt rows do not match the soundtrack's songs", "The list may show the wrong rows until the next screen open.");
}
// the pre-hook of the row cycle (the row stub -> here -> UG2's own body through the trampoline): mirrors the step UG2's body is about to make on the VIEW entry
void __cdecl List_RowStep(void *screen, unsigned hash)
{
    DWORD row = 0, idd = 0; uintptr_t sc = (uintptr_t)screen; int listSong[27]; char nm[NAME_LEN];
    if (!RdDword(sc + 0x70, &row) || !row || !RdDword(row + 0x18, &idd)) { TraxProblem(SAID_E_ROWSTEP, "a song setting step", "the highlighted row could not be read", "The step may not be kept."); return; }
    int id = (int)idd;
    if (!Accessible(UA_LIST, 27 * 8, FALSE)) { TraxProblem(SAID_E_ROWSTEP, "a song setting step", "UG2's song list could not be read", "The step may not be kept."); return; }
    for (int p = 0; p < 27; p++) listSong[p] = *(volatile int *)(UA_LIST + 8 * p);
    if (g_viewSt != g_set.soundtrack) TraxProblem(SAID_E_ROWSTEP, "a song setting step", "the rows shown belong to another soundtrack than the bar names", "The step was kept for the soundtrack shown.");
    List_RowPlan r = List_PlanRowStep(g_viewSt, id, hash, g_view, listSong);   // the VIEW's category decides which set the step writes (All Trax -> kind 5 = All Trax's own, even for a UG2 row)
    if (r.kind == 0) { if (r.viewIdx < 0) TraxProblem(SAID_E_ROWSTEP, "a song setting step", "the highlighted row is not one of the list's songs", "The step may not be kept."); return; }   // a foreign hash = no step (silent)
    int n = r.mwN;
    if (r.kind == 1) {
        uintptr_t p = UA_LIST + 8 * r.listPos + 4;
        if (!Accessible(p, 1, TRUE)) TraxProblem(SAID_E_ROWSTEP, "a song setting step", "UG2's song list could not be written", "The step may not be kept.");
        else { *(volatile unsigned char *)p = (unsigned char)r.newMode; if (*(volatile unsigned char *)p != (unsigned char)r.newMode) TraxProblem(SAID_E_ROWSTEP, "a song setting step", "the write to UG2's song list did not take", "The step may not be kept."); }
        Log("Song setting: %s, %s -> %s (in Underground 2).", SongName(id, nm), kModeShown[r.oldMode & 3], kModeShown[r.newMode & 3]);
        return;
    }
    if (r.kind == 5) {   // the view shows All Trax -> ALL TRAX's OWN set + its ALL line ONLY: 0x83ACB8 NOT written even for a UG2 row, no game array / game line touched
        if (id < 0 || id >= SONG_NSONG) { TraxProblem(SAID_E_ROWSTEP, "a song setting step", "the highlighted row is not one of the list's songs", "The step may not be kept."); return; }
        list_allMode[id] = (unsigned char)r.newMode; g_set.allMode[id] = (signed char)r.newMode;
        SettingsDirty();   // memory only; the file waits for the game's save
        Log("Song setting: %s, %s -> %s (in All Trax).", SongName(id, nm), kModeShown[r.oldMode & 3], kModeShown[r.newMode & 3]);
        return;
    }
    if (r.kind == 4) { list_cbMode[n] = (unsigned char)r.newMode; g_set.cbMode[n] = (signed char)r.newMode; }        // a Carbon row (nn = c + 1) -> the live Carbon mode + its 'CB nn' line
    else if (r.kind == 3) { list_u1Mode[n] = (unsigned char)r.newMode; g_set.u1Mode[n] = (signed char)r.newMode; }   // a UG1 row (nn = row + 1) -> the live UG1 mode + its file line
    else { list_mwMode[n] = (unsigned char)r.newMode; g_set.mwMode[n] = (signed char)r.newMode; }
    SettingsDirty();   // memory only; the file waits for the game's save
    Log("Song setting: %s, %s -> %s (in %s).", SongName(id, nm), kModeShown[r.oldMode & 3], kModeShown[r.newMode & 3], r.kind == 4 ? "Carbon" : r.kind == 3 ? "Underground" : "Most Wanted");
}
// the preview hook (the preview stub -> here): -> 0 = UG2's own path goes on with idx (the trampoline), 1 = handled here (ret 4)
int __cdecl List_Preview(void *mgr, int idx)
{
    List_PrevPlan p = List_PlanPreview(idx, g_st.last, g_st.lastCarrier); char nm[NAME_LEN];
    if (p.action == 0) {
        if (p.giveBack >= 0 && g_mapSlot >= 0 && MapReady()) ApplyCarrier(p.giveBack, 0);   // a carrier slot gets its own song back first (no map found yet: its wave plays as it is)
        g_st.last = idx;
        Log("Preview: %s.", SongName(idx, nm));
        return 0;
    }
    if (p.action == 2) return 0;   // MW 25 / 26 (out) or an id outside 0..108: UG2's own path plays nothing for a slot >= 27
    DWORD player = 0;
    if (!RdDword((uintptr_t)mgr + 0xA0, &player) || !player) { Log("Preview of %s FAILED: no music player.", SongName(idx, nm)); return 1; }
    if (!MapReady()) { Log("Preview of %s FAILED: the music map is NOT found.", SongName(idx, nm)); return 1; }
    int ci = p.carrier - SONG_CARRIER_A;
    if (!ApplyCarrier(p.carrier, p.plan.want[ci])) { Log("Preview of %s FAILED: the music map could not be changed.", SongName(idx, nm)); return 1; }
    for (int i = 0; i < 2; i++) if (SONG_CARRIER_A + i != p.carrier && p.plan.want[i] >= 0) ApplyCarrier(SONG_CARRIER_A + i, p.plan.want[i]);
    g_st.last = p.carrier; g_st.lastCarrier = p.carrier; g_lastSong = idx;
    ((void (__attribute__((thiscall)) *)(DWORD, int))UA_LIST_PLAYSLOT)(player, p.carrier);   // UG2's own: the old song stopped, the slot started, its pop-up = the carrier's rewritten row
    Log("Preview: %s.", SongName(idx, nm));
    return 1;
}
// the list's stubs. The row stub replaces the head of 0x53DEB0 (thiscall(screen, hash) ret 4): the row-step mirror (screen, hash) first (eax / ecx / edx saved), then the
// trampoline (the 5 head bytes 'sub esp,0x1c; push ebp; push esi' + jmp 0x53DEB5) = UG2's own body, which returns to the caller itself. The preview stub replaces the head of
// 0x480C10 (thiscall(mgr, idx) ret 4): the preview function (mgr, idx) -> 0 = the trampoline (the 6 head bytes 'mov ecx,[ecx+0xa0]' + jmp 0x480C16), 1 = ret 4 (handled).
extern void ListRowStub(void); extern void ListPrevStub(void);
__asm__(".text\n.globl _ListRowStub\n_ListRowStub:\n"
        "  pushl %ecx\n"
        "  pushl %edx\n"
        "  pushl %eax\n"
        "  pushl 16(%esp)\n"
        "  pushl %ecx\n"
        "  call _List_RowStep\n"
        "  addl $8, %esp\n"
        "  popl %eax\n"
        "  popl %edx\n"
        "  popl %ecx\n"
        "  jmp *_g_rowTramp\n");
__asm__(".text\n.globl _ListPrevStub\n_ListPrevStub:\n"
        "  pushl %ecx\n"
        "  pushl 8(%esp)\n"
        "  pushl %ecx\n"
        "  call _List_Preview\n"
        "  addl $8, %esp\n"
        "  popl %ecx\n"
        "  testl %eax, %eax\n"
        "  jne 1f\n"
        "  jmp *_g_prevTramp\n"
        "1:\n"
        "  ret $4\n");
// the 8 operands + the two search operands re-pointed, the two head hooks (tracklist.h kListSites), all or nothing after every site + continuation + callee + caller is
// byte-checked; only with the top bar installed (its ctor patch is where the view is refreshed before the fill). The bound is written by WriteBound (N per screen open).
static void InstallList(void)
{
    SiteFail f = { 0, "" }; BOOL ok = TRUE;
    if (!g_barInstalled) { Log("EA Trax list NOT installed: it needs the EA Trax top bar. The screen keeps UG2's 27 songs."); return; }
    for (int i = 0; i < LIST_NSITES; i++) ok &= CheckSite(kListSites[i].addr, kListSites[i].bytes, kListSites[i].len, &f);
    if (!ok) { Log("EA Trax list NOT installed: the game's code at 0x%06lX is not the retail exe's (found%s). The screen keeps UG2's 27 songs.", (unsigned long)f.addr, f.found); return; }
    unsigned char *tr = (unsigned char *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE), *tp = (unsigned char *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr || !tp) { Log("EA Trax list NOT installed: VirtualAlloc FAILED (Windows error %lu). The screen keeps UG2's 27 songs.", GetLastError()); return; }
    memcpy(tr, (const void *)UA_LIST_ROWFN, 5); tr[5] = 0xE9; *(DWORD *)(tr + 6) = (DWORD)(UA_LIST_ROWRESUME - ((uintptr_t)tr + 10)); g_rowTramp = (DWORD)(uintptr_t)tr;
    memcpy(tp, (const void *)UA_LIST_PREVFN, 6); tp[6] = 0xE9; *(DWORD *)(tp + 7) = (DWORD)(UA_LIST_PREVRESUME - ((uintptr_t)tp + 11)); g_prevTramp = (DWORD)(uintptr_t)tp;
    BuildView();                                                          // the view is valid before any operand points at it (the bound stays 0x1B until installed)
    uint32_t vb = (uint32_t)(uintptr_t)g_view, tb = (uint32_t)(uintptr_t)g_vtab; int w = 0, want = 0;
    for (int i = 0; i < LIST_NSITES; i++) {
        const List_Site *s = &kListSites[i]; int k = s->kind;
        if (k == 5 || k == 9) continue;                                   // the bound: WriteBound; 9 = checked only
        want++;
        if (k == 8) { unsigned char j[5] = { 0xE9, 0, 0, 0, 0 }; uintptr_t tgt = s->addr == UA_LIST_ROWFN ? (uintptr_t)ListRowStub : (uintptr_t)ListPrevStub; *(DWORD *)(j + 1) = (DWORD)(tgt - (s->addr + 5)); w += WriteCode(s->addr, j, 5, "EA Trax list"); }
        else { uint32_t v = List_PatchValue(k, vb, tb, 0); w += WriteCode(s->addr + s->patchOff, (const unsigned char *)&v, 4, "EA Trax list"); }
    }
    g_listInstalled = w == want;
    if (g_listInstalled) Log("EA Trax list: installed.");
    else Log("EA Trax list NOT fully installed: %d of %d writes FAILED (the lines above).", want - w, want);
    if (g_listInstalled) WriteBound();
}
// ---- THE GAME'S SAVE AND LOAD (savesync.h). The save hook runs inside UG2's save entry point (0x4D9C60 site 1 / 0x4D9B80 site 2) in place of its call to
// 0x43E030 (the stub jumps there after): the profile at 0x83A9E0 already holds the name being saved (save-as: 4e8f41 assigns it right before 4e8f52); the mod's file is
// written for it (dirty or not - the game writes its whole profile too). The load hook runs after UG2's profile load 0x5344D0 returned (the stub called it with the same
// three args; this = 0x83A9E0): the name is the loaded profile's now; the file is re-read whether the name changed or not (the unsaved steps dropped). Both on the game
// thread, both guarded (the name read, the file I/O).
void __cdecl Save_OnSave(int site)
{
    (void)site;
    SettingsSync();
    SettingsWrite();
}
void __cdecl Save_OnLoad(DWORD self, DWORD buf, DWORD size, DWORD flag)
{
    char live[32]; (void)self; (void)buf; (void)size; (void)flag;
    if (!ReadProfileName(live)) { if (!(g_said & SAID_PROFILE)) { g_said |= SAID_PROFILE; Log("The profile name could NOT be read - the song settings in memory stay."); } return; }
    memcpy(g_profile, live, 32); g_profileEver = TRUE;
    SettingsRead();
}
// the stubs. The two save stubs are reached by UG2's own 'call' at 4d9cb2 / 4d9bea (the stack = the return address + 0x43E030's three cdecl args): every register and the
// flags saved, the save hook (site), restored, then jmp 0x43E030 - it returns to UG2's own return address and the caller cleans the args. The load stub is reached by
// the 'call' at 5347ab / 534812 (thiscall: ecx = this, the stack = the return address + 3 args): ecx saved, the 3 args re-pushed, ecx restored, 0x5344D0 called (its 'ret
// 0xC' pops the copies), ecx popped; then every register (eax = the load's return value) and the flags saved, the load hook (this, a1, a2, a3), restored, ret 0xC (the
// original args popped as 0x5344D0 would have).
extern void SaveStub1(void); extern void SaveStub2(void); extern void LoadStub(void);
__asm__(".text\n.globl _SaveStub1\n_SaveStub1:\n"
        "  pushal\n"
        "  pushfl\n"
        "  pushl $1\n"
        "  call _Save_OnSave\n"
        "  addl $4, %esp\n"
        "  popfl\n"
        "  popal\n"
        "  jmp *_g_saveFn\n");
__asm__(".text\n.globl _SaveStub2\n_SaveStub2:\n"
        "  pushal\n"
        "  pushfl\n"
        "  pushl $2\n"
        "  call _Save_OnSave\n"
        "  addl $4, %esp\n"
        "  popfl\n"
        "  popal\n"
        "  jmp *_g_saveFn\n");
__asm__(".text\n.globl _LoadStub\n_LoadStub:\n"
        "  pushl %ecx\n"
        "  pushl 16(%esp)\n"
        "  pushl 16(%esp)\n"
        "  pushl 16(%esp)\n"
        "  movl 12(%esp), %ecx\n"
        "  call *_g_loadFn\n"
        "  popl %ecx\n"
        "  pushal\n"
        "  pushfl\n"
        "  pushl 48(%esp)\n"
        "  pushl 48(%esp)\n"
        "  pushl 48(%esp)\n"
        "  pushl %ecx\n"
        "  call _Save_OnLoad\n"
        "  addl $16, %esp\n"
        "  popfl\n"
        "  popal\n"
        "  ret $12\n");
// the four call operands re-pointed, all or nothing: each site must read 'E8' + the rel32 to its original target (savesync.h) first - one mismatch = nothing
// written (a step then stays in memory and is never written; the file is read only at a name change). UG2's save-function heads 0x4D9C60 / 0x4D9B80 are left untouched.
static void InstallSave(void)
{
    SiteFail f = { 0, "" }; BOOL ok = TRUE; unsigned char exp[SAVE_NSITES][5];
    for (int i = 0; i < SAVE_NSITES; i++) { Save_Expected(&kSaveSites[i], exp[i]); ok &= CheckSite(kSaveSites[i].addr, exp[i], 5, &f); }
    if (!ok) { Log("Saving with the profile NOT installed: the game's code at 0x%06lX is not the retail exe's (found%s). Song settings will NOT be saved.", (unsigned long)f.addr, f.found); return; }
    int w = 0;
    for (int i = 0; i < SAVE_NSITES; i++) {
        const Save_Site *s = &kSaveSites[i]; uintptr_t tgt = s->site == 1 ? (uintptr_t)SaveStub1 : s->site == 2 ? (uintptr_t)SaveStub2 : (uintptr_t)LoadStub;
        uint32_t rel = Save_Rel(s->addr, (uint32_t)tgt);
        w += WriteCode(s->addr + 1, (const unsigned char *)&rel, 4, "Saving with the profile");
    }
    g_saveInstalled = w == SAVE_NSITES;
    if (g_saveInstalled) Log("Saving with the profile: installed.");
    else Log("Saving with the profile NOT fully installed: %d of %d writes FAILED (the lines above). Song settings may NOT be saved.", SAVE_NSITES - w, SAVE_NSITES);
}
// ---- THE SAVE PROMPT (saveprompt.h). The game's profile object 0x83A9D0 keeps a serialized copy of itself at [this+0x16778] when a screen is entered (0x52A460)
// and compares itself with that copy when the screen is left (0x52A4C0 -> al = 1 = 'changed' -> the game's own save dialog). The mod does the same with its own state: a snapshot
// of the bar's soundtrack + every set's modes (MW / UG1 / Carbon / All Trax) taken when the game makes its copy, compared when the game compares, the answer ORed in. All on the
// game thread, reads guarded.
static BOOL Prompt_CopyField(DWORD self, DWORD *v) { *v = 0; return RdDword(self + PROMPT_COPY_OFF, v); }
void __cdecl Prompt_BeforeKeep(DWORD self) { DWORD c; g_promptHadCopy = Prompt_CopyField(self, &c) && c != 0; }
void __cdecl Prompt_AfterKeep(DWORD self)
{
    DWORD c; BOOL rd = Prompt_CopyField(self, &c); int has = rd && c != 0;
    Prompt_OnKeep(&g_prompt, g_promptHadCopy, has, g_set.soundtrack, g_set.mwMode, g_set.u1Mode, g_set.cbMode, g_set.allMode);   // the soundtrack + the MW / UG1 / Carbon modes + All Trax's lines
}
void __cdecl Prompt_BeforeCompare(DWORD self) { DWORD c; g_promptHadCopy = Prompt_CopyField(self, &c) && c != 0; }
int __cdecl Prompt_AfterCompare(DWORD self, int gameAl)
{
    int d = 0; (void)self;
    int r = Prompt_OnCompare(&g_prompt, g_promptHadCopy, gameAl & 0xFF, g_set.soundtrack, g_set.mwMode, g_set.u1Mode, g_set.cbMode, g_set.allMode, &d);   // the soundtrack + the MW / UG1 / Carbon modes + All Trax's lines
    if (r && !(gameAl & 0xFF)) Log("Leaving EA Trax: the song settings changed, so the game asks to save.");   // the mod's answer alone raised the prompt
    return r;
}
// the stubs. The keep stub replaces the head of 0x52A460 (thiscall(flag) ret 4): the pre-keep call (this) (ecx saved), the flag re-pushed, the trampoline (the 6 head bytes 'push ebp;
// mov ebp,esp; push esi; mov esi,ecx' + jmp 0x52A466 = UG2's own body, its ret 4 pops the copy), then the keep hook (this) with every register saved, ret 4. The compare stub replaces
// the head of 0x52A4C0 (thiscall() ret, al): the pre-compare call (this), the trampoline (the 6 head bytes 'sub esp,0xc; push esi; mov esi,ecx' + jmp 0x52A4C6), then the compare
// hook (this, al) -> eax = the answer the caller tests ('test al,al'); ebx / esi / edi / ebp untouched by the C code (cdecl).
extern void PromptKeepStub(void); extern void PromptCmpStub(void);
__asm__(".text\n.globl _PromptKeepStub\n_PromptKeepStub:\n"
        "  pushl %ecx\n"
        "  pushl %ecx\n"
        "  call _Prompt_BeforeKeep\n"
        "  addl $4, %esp\n"
        "  popl %ecx\n"
        "  pushl %ecx\n"
        "  pushl 8(%esp)\n"
        "  movl 4(%esp), %ecx\n"
        "  call *_g_keepTramp\n"
        "  popl %ecx\n"
        "  pushal\n"
        "  pushfl\n"
        "  pushl %ecx\n"
        "  call _Prompt_AfterKeep\n"
        "  addl $4, %esp\n"
        "  popfl\n"
        "  popal\n"
        "  ret $4\n");
__asm__(".text\n.globl _PromptCmpStub\n_PromptCmpStub:\n"
        "  pushl %ecx\n"
        "  pushl %ecx\n"
        "  call _Prompt_BeforeCompare\n"
        "  addl $4, %esp\n"
        "  popl %ecx\n"
        "  pushl %ecx\n"
        "  call *_g_cmpTramp\n"
        "  popl %ecx\n"
        "  pushl %ecx\n"
        "  pushl %eax\n"
        "  pushl %ecx\n"
        "  call _Prompt_AfterCompare\n"
        "  addl $8, %esp\n"
        "  popl %ecx\n"
        "  ret\n");
// the two head hooks, all or nothing: both heads (12 bytes each: the 6 stolen + 'mov eax,[esi+0x16778]') byte-checked first; one mismatch = nothing written (no prompt for the
// added songs; the save sync works either way). Independent of the top bar, the list and the save sync.
static void InstallPrompt(void)
{
    SiteFail f = { 0, "" }; BOOL ok = TRUE;
    ok &= CheckSite(UA_PROMPT_KEEP, kPromptKeepHead, 12, &f);
    ok &= CheckSite(UA_PROMPT_CMP, kPromptCmpHead, 12, &f);
    if (!ok) { Log("Save prompt NOT installed: the game's code at 0x%06lX is not the retail exe's (found%s). Leaving EA Trax will not ask to save the added songs.", (unsigned long)f.addr, f.found); return; }
    unsigned char *tk = (unsigned char *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE), *tc = (unsigned char *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tk || !tc) { Log("Save prompt NOT installed: VirtualAlloc FAILED (Windows error %lu). Leaving EA Trax will not ask to save the added songs.", GetLastError()); return; }
    memcpy(tk, (const void *)UA_PROMPT_KEEP, PROMPT_STOLEN); tk[PROMPT_STOLEN] = 0xE9; *(DWORD *)(tk + PROMPT_STOLEN + 1) = (DWORD)(UA_PROMPT_KEEP_RESUME - ((uintptr_t)tk + PROMPT_STOLEN + 5)); g_keepTramp = (DWORD)(uintptr_t)tk;
    memcpy(tc, (const void *)UA_PROMPT_CMP, PROMPT_STOLEN); tc[PROMPT_STOLEN] = 0xE9; *(DWORD *)(tc + PROMPT_STOLEN + 1) = (DWORD)(UA_PROMPT_CMP_RESUME - ((uintptr_t)tc + PROMPT_STOLEN + 5)); g_cmpTramp = (DWORD)(uintptr_t)tc;
    unsigned char jk[6] = { 0xE9, 0, 0, 0, 0, 0x90 }, jc[6] = { 0xE9, 0, 0, 0, 0, 0x90 };
    *(DWORD *)(jk + 1) = (DWORD)((uintptr_t)PromptKeepStub - (UA_PROMPT_KEEP + 5)); *(DWORD *)(jc + 1) = (DWORD)((uintptr_t)PromptCmpStub - (UA_PROMPT_CMP + 5));
    int w = WriteCode(UA_PROMPT_KEEP, jk, 6, "Save prompt") + WriteCode(UA_PROMPT_CMP, jc, 6, "Save prompt");
    g_promptInstalled = w == 2;
    if (g_promptInstalled) Log("Save prompt: installed.");
    else Log("Save prompt NOT fully installed: %d of 2 writes FAILED (the lines above).", 2 - w);
}
static void InstallLsHook(void)
{
    char found[64];
    if (!Accessible(UA_LS_SITE, 10, FALSE) || memcmp((void *)UA_LS_SITE, kLsBytes, 10)) { Hex(UA_LS_SITE, found, 10); Log("Long-song fix NOT installed: the game's code at 0x727CD8 is not the retail exe's (found%s). 9 Most Wanted songs will not end.", found); return; }
    DWORD old = 0;
    if (!VirtualProtect((void *)UA_LS_SITE, 5, PAGE_EXECUTE_READWRITE, &old)) { Log("Long-song fix NOT installed: a write to the game's code at 0x727CD8 FAILED (Windows error %lu). 9 Most Wanted songs will not end.", GetLastError()); return; }
    unsigned char jmp[5] = { 0xE9, 0, 0, 0, 0 }; *(DWORD *)(jmp + 1) = (DWORD)((uintptr_t)LsStub - (UA_LS_SITE + 5));
    memcpy((void *)UA_LS_SITE, jmp, 5); VirtualProtect((void *)UA_LS_SITE, 5, old, &old); FlushInstructionCache(GetCurrentProcess(), (void *)UA_LS_SITE, 5);
    g_lsHooked = TRUE;
    Log("Long-song fix: installed.");
}
static void InstallHook(void)
{
    char found[64];
    if (!Accessible(UA_PICKER, 6, FALSE) || memcmp((void *)UA_PICKER, kHead, 6)) { Hex(UA_PICKER, found, 6); Log("Song picker NOT installed: the game's code at 0x46B6F0 is not the retail exe's (found%s). UG2 picks every song; the added songs cannot play.", found); return; }
    g_tramp = (unsigned char *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_tramp) { Log("Song picker NOT installed: VirtualAlloc FAILED (Windows error %lu). UG2 picks every song; the added songs cannot play.", GetLastError()); return; }
    memcpy(g_tramp, kHead, 5); g_tramp[5] = 0xE9; *(DWORD *)(g_tramp + 6) = (DWORD)(UA_PICKER_RESUME - ((uintptr_t)g_tramp + 10));
    DWORD old = 0;
    if (!VirtualProtect((void *)UA_PICKER, 5, PAGE_EXECUTE_READWRITE, &old)) { Log("Song picker NOT installed: a write to the game's code at 0x46B6F0 FAILED (Windows error %lu). UG2 picks every song; the added songs cannot play.", GetLastError()); return; }
    unsigned char jmp[5] = { 0xE9, 0, 0, 0, 0 }; *(DWORD *)(jmp + 1) = (DWORD)((uintptr_t)PickStub - (UA_PICKER + 5));
    memcpy((void *)UA_PICKER, jmp, 5); VirtualProtect((void *)UA_PICKER, 5, old, &old); FlushInstructionCache(GetCurrentProcess(), (void *)UA_PICKER, 5);
    g_hooked = TRUE;
    Log("Song picker: installed.");
}
static void CheckRng(void)
{
    g_rngOk = Accessible(UA_TIMER, 7, FALSE) && !memcmp((void *)UA_TIMER, kHeadTimer, 7) && Accessible(UA_RANDOM, 6, FALSE) && !memcmp((void *)UA_RANDOM, kHeadRandom, 6);
    if (g_rngOk) { Log("Random draw: UG2's own, checked."); return; }
    char a[64], b[64]; Hex(UA_TIMER, a, 7); Hex(UA_RANDOM, b, 6);
    Log("Random draw: the game's code at 0x43BDF0 / 0x43C1C0 does NOT match (found%s /%s): UG2 picks every song.", a, b);
}
// ---- THE SHUFFLE BUTTON (shuffle.h). Two call operands re-pointed + one data word, all or nothing, byte-checked first, only with the top bar installed
// (the top bar's patches replace UG2's Random / Sequential bar, so nothing else in the game flips [0x83AA58] - this gives the toggle back as a button left of Back).
static int ShufRd32(void *ctx, uint32_t a, uint32_t *v) { DWORD d = 0; (void)ctx; if (!RdDword(a, &d)) return 0; *v = d; return 1; }
static int ShufRd8(void *ctx, uint32_t a, uint32_t *v) { DWORD d = 0; (void)ctx; if (!RdByte(a, &d)) return 0; *v = d; return 1; }
static BOOL Shuf_BarPkg(uintptr_t bar, char *pkg, int cap) { DWORD p = 0; pkg[0] = 0; return RdDword(bar + SHUF_BAR_PKG_OFF, &p) && p && ReadCStr(p, pkg, cap); }
static int Shuf_Print(DWORD obj, const char *text) { return ((int (__cdecl *)(DWORD, const char *, ...))UA_BAR_PRINTF_OBJ)(obj, "%s", text); }   // FEPrintf on the object, as SetLabel prints
// ---- THE DEFAULT BUTTON (defaultbutton.h). The second bit of the Shuffle button's flag word + the jukebox handler slot 0x79CB7C swapped, all or nothing, byte-checked first,
// only with the Shuffle button installed (the same re-pointed calls 54e8d7 / 55df22 carry the second slot's label and click: the label and click replacements hand them here by the
// hash / the bit).
// the second slot's label (from the label replacement on hash 0xEAA1D5DF = bit 0x100000's stock label; UG2's own label is set first, always): 'Default' on the jukebox only
static void Dflt_Label(DWORD obj, DWORD hash)
{
    DWORD bar = 0, kind = 0; char pkg[40];
    if (!RdDword(UA_SHUF_BAR, &bar) || !bar || !Shuf_BarPkg(bar, pkg, sizeof pkg)) { TraxProblem(SAID_E_DFLTLABEL, "the Default label", "the button bar could not be read", "The button shows UG2's own words."); return; }
    if (!RdDword(obj + 0x18, &kind)) kind = 0xFFFFFFFF;
    if (!Dflt_LabelWanted(hash, pkg, kind)) {
        if (Shuf_IsJukebox(pkg)) TraxProblem(SAID_E_DFLTLABEL, "the Default label", "the button's object is not a text object", "The button shows UG2's own words.");   // another screen: not this button (silent)
        return;
    }
    if (g_dfltDialogUp) g_dfltDialogUp = FALSE;   // a dialog mark left up (the screen closed under it, or another dialog replaced it) is cleared at an EA Trax open: the list takes input again
    int r = Shuf_Print(obj, DFLT_TEXT);
    if (r <= 0) TraxProblem(SAID_E_DFLTLABEL, "the Default label", "the game's print returned 0", "The button shows UG2's own words.");
}
// the second slot's click (from the click replacement on bit 0x100000; UG2's own bit-to-message map runs first, msg = its answer, 0xC539A4B0, goes on to the package as always): as
// UG2's own 'Default Settings' item (49b980-49b9cf) - the UI sound 0x50D150(pkg, 1, 0), then UG2's dialog 'GenericDialog_SMALL.fng' Yes / No with the mod's text through 0x558750,
// the answer messages = the mod's own (Yes / No; Esc = No), argument 1 = the package NAME pointer the bar holds at [bar+0x58] (= [screen+4], what UG2 passes for its own dialogs).
static DWORD Dflt_Click(DWORD bar, DWORD bit, DWORD msg)
{
    char pkg[40], file[40] = "?"; DWORD p = 0;
    if (!Shuf_BarPkg((uintptr_t)bar, pkg, sizeof pkg) || !RdDword(bar + SHUF_BAR_PKG_OFF, &p) || !p) { Log("Default FAILED: the button's bar could not be read."); return msg; }
    if (!Dflt_ClickWanted(bit, pkg)) return msg;   // another screen: not this button, UG2's message goes on as always
    if (!g_dfltInstalled) { Log("Default FAILED: the Default button is not installed."); return msg; }
    if (!ReadCStr(UA_DFLT_DIALOG_FILE, file, sizeof file) || strcmp(file, DFLT_DIALOG_FILE_TEXT)) { Log("Default FAILED: the dialog file is not UG2's own (0x78FA24 reads '%s').", file); return msg; }
    g_dfltScope = g_set.soundtrack;   // a dialog still marked up (no answer came) is replaced by this one
    Log("Default: asked to reset the %s songs.", StText(g_dfltScope));
    ((void (__cdecl *)(DWORD, DWORD, DWORD))UA_DFLT_UISOUND)(p, 1, 0);
    g_dfltDialogUp = TRUE;
    ((void (__cdecl *)(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const char *, ...))UA_DFLT_DIALOG_FMT)(p, UA_DFLT_DIALOG_FILE, DFLT_LABEL_YES, DFLT_LABEL_NO, DFLT_MSG_YES, DFLT_MSG_NO, DFLT_MSG_NO, 0, "%s", DFLT_DIALOG_TEXT);
    return msg;
}
// the reset after 'Yes': the scope = the ONE set of the category the bar shows (defaultbutton.h's scope tests) - Default resets only the category shown.
// Underground 2 -> UG2's list 0x83ACB8: the live list's mode bytes (the byte the row step writes) := UG2's defaults, then 0x479220 (the masks from the list, as 53e08d);
// Underground / Most Wanted / Carbon -> that game's live modes := its own defaults + its file lines := -1; All Trax -> ONLY All Trax's own 109 modes + its ALL lines
// (no write to 0x83ACB8, no 0x479220 - UG2's list does not change). A changed file line = an unsaved step (written at the game's next save; the save prompt's snapshot
// differs = UG2's own save prompt); then the rows on screen (RefillList). Nothing of the mod's beyond these: UG2's picker and save keep their own rules.
static void Dflt_Reset(DWORD screen)
{
    int st = g_set.soundtrack; char ub[28] = "-", ua[28] = "-", mb[27] = "-", ma[27] = "-", b1[27] = "-", a1[27] = "-", cb[31] = "-", ca[31] = "-", ab[SONG_NSONG + 1] = "-", aa[SONG_NSONG + 1] = "-";
    int nU = 0, nM = 0, nU1 = 0, nC = 0, nA = 0, lM = 0, lU = 0, lC = 0, lA = 0, wf = 0;
    if (Dflt_ScopeUg2(st)) {
        if (!Accessible(UA_LIST, 27 * 8, FALSE) || !Accessible(UA_LIST, 27 * 8, TRUE)) TraxProblem(SAID_E_RESET, "the Default reset", "UG2's song list could not be read or written", "UG2's songs kept their settings.");
        else {
            int listSong[27]; unsigned char lm[27], nm[27];
            for (int p = 0; p < 27; p++) { listSong[p] = *(volatile int *)(UA_LIST + 8 * p); lm[p] = nm[p] = *(volatile unsigned char *)(UA_LIST + 8 * p + 4); }
            nU = Dflt_ResetUg2(listSong, nm, ub, ua);
            for (int p = 0; p < 27; p++) if (nm[p] != lm[p]) { *(volatile unsigned char *)(UA_LIST + 8 * p + 4) = nm[p]; if (*(volatile unsigned char *)(UA_LIST + 8 * p + 4) != nm[p]) wf++; }
            if (wf) TraxProblem(SAID_E_RESET, "the Default reset", "a write to UG2's song list did not take", "Some of UG2's songs kept their settings.");
        }
    }
    if (Dflt_ScopeMw(st)) nM = Dflt_ResetAdded(list_mwMode, g_set.mwMode, kMwMode, 26, &lM, mb, ma);
    if (Dflt_ScopeU1(st)) nU1 = Dflt_ResetAdded(list_u1Mode, g_set.u1Mode, kU1Mode, 26, &lU, b1, a1);
    if (Dflt_ScopeCb(st)) nC = Dflt_ResetAdded(list_cbMode, g_set.cbMode, kCbMode, 30, &lC, cb, ca);
    if (Dflt_ScopeAll(st)) nA = Dflt_ResetAll(list_allMode, g_set.allMode, &lA, ab, aa);   // All Trax -> ONLY All Trax's own 109 modes + its ALL lines (0x83ACB8 and the four game sets untouched; no 0x479220: UG2's list does not change)
    int fileChanged = lM + lU + lC + lA + nM + nU1 + nC + nA;
    if (fileChanged) SettingsDirty();   // memory only; the file (fewer lines) waits for the game's save; the save prompt's snapshot differs -> UG2's save prompt
    Log("Default: Yes - the %s songs are back to their defaults (%d changed).", StText(st), nU + nM + nU1 + nC + nA);
    DWORD vt = 0;
    if (RdDword(screen, &vt) && vt == UA_DFLT_VTABLE) RefillList(screen);   // the view + the rows at once (RefillList's path), the bound := N
    else TraxProblem(SAID_E_RESET, "the Default reset", "the screen is not the EA Trax screen", "The rows follow at the next screen open.");
    if (Dflt_ScopeUg2(st)) {
        DWORD mgr = 0;
        if (RdDword(UA_BAR_SOUNDMGR, &mgr) && mgr) ((void (__attribute__((thiscall)) *)(DWORD))UA_SHUF_LISTSETUP)(mgr);   // 0x479220: the masks from the live list, as the row cycle's body does at 53e08d
    }
}
// in the jukebox vtable's handler slot 0x79CB7C in place of 0x56B1C0: thiscall(ecx = the screen; msg, a, b, c) ret 0x10. The mod's two answer messages after a Default dialog -> the
// reset / nothing. While a Default dialog is up, the jukebox's 11 INPUT messages (defaultbutton.h: accept x2, left / right, the list's two steps, the two triggers, PAD_BACK, a bar
// button, the mouse click) are DROPPED here - not passed on, the value 0x56B1C0 leaves for a message it ignores (the message itself) returned; every other message (the per-frame
// ones, the answers, anything 0x56B1C0 ignores) goes on to 0x56B1C0 unchanged and its return value is returned (so the hooks of the top bar and the list inside its callees keep running).
DWORD __attribute__((thiscall)) Dflt_Handler(DWORD screen, DWORD msg, DWORD a, DWORD b, DWORD c)
{
    int ans = Dflt_Answer(msg, g_dfltDialogUp);
    if (ans == 1) { g_dfltDialogUp = FALSE; Dflt_Reset(screen); }
    else if (ans == 2) { g_dfltDialogUp = FALSE; Log("Default: No - nothing changed."); }
    else if (Dflt_Gate(msg, g_dfltDialogUp)) return Dflt_GateReturn(msg);   // an input message while the dialog is up: dropped
    return ((DWORD (__attribute__((thiscall)) *)(DWORD, DWORD, DWORD, DWORD, DWORD))UA_DFLT_HANDLER_ORIG)(screen, msg, a, b, c);
}
// in place of 'call 0x50C900' at 54e8d7 inside the setup's walk (every bar setup on every screen, 80 callers): cdecl(obj = the slot's text object, hash = the slot's stock label).
// UG2's own label first, always; then only for hash 0xBA0E1020 (the 0x80000 slot) on the jukebox: the text by the byte. The caller adds 8 and reloads eax (esi / edi / ebx kept = C).
void __cdecl Shuf_Label(DWORD obj, DWORD hash)
{
    ((void (__cdecl *)(DWORD, DWORD))UA_SHUF_LABEL_ORIG)(obj, hash);
    if (hash == DFLT_HASH_HOOD) { Dflt_Label(obj, hash); return; }   // the second slot's label (bit 0x100000's stock hash) -> 'Default' on the jukebox only
    if (hash != SHUF_HASH_N2O) return;
    DWORD bar = 0, kind = 0, pm = 0; char pkg[40];
    if (!RdDword(UA_SHUF_BAR, &bar) || !bar || !Shuf_BarPkg(bar, pkg, sizeof pkg)) { TraxProblem(SAID_E_SHUFLABEL, "the Shuffle label", "the button bar could not be read", "The button shows UG2's own words."); return; }
    if (!RdDword(obj + 0x18, &kind)) kind = 0xFFFFFFFF;
    if (!Shuf_LabelWanted(hash, pkg, kind)) {
        if (Shuf_IsJukebox(pkg)) TraxProblem(SAID_E_SHUFLABEL, "the Shuffle label", "the button's object is not a text object", "The button shows UG2's own words.");   // another screen: not this button (silent)
        return;
    }
    if (!RdDword(UA_SHUF_PLAYMODE, &pm)) { TraxProblem(SAID_E_SHUFLABEL, "the Shuffle label", "UG2's shuffle byte could not be read", "The button shows UG2's own words."); return; }
    int r = Shuf_Print(obj, Shuf_Text(pm));
    if (r <= 0) TraxProblem(SAID_E_SHUFLABEL, "the Shuffle label", "the game's print returned 0", "The button shows UG2's own words.");
}
// in place of 'call 0x52D170' at 55df22 inside the bar click 0x55DE00: thiscall(ecx = the bar, bit = the clicked slot's bit) ret 4 -> eax = UG2's message for the bit (the
// caller sends it on to the package by name, 55df2e-55df84: 0xC539A4B0 for 0x80000 = ignored by the jukebox handler). UG2's own map first, always; then only for bit 0x80000
// on the jukebox: the flip as UG2's own trigger handler 0x5241E0 makes it, the trigger sound, 0x479220, the new text on the slot. edi (the bar) / esi / ebx kept = a thiscall C function.
DWORD __attribute__((thiscall)) Shuf_Click(DWORD bar, DWORD bit)
{
    DWORD msg = ((DWORD (__attribute__((thiscall)) *)(DWORD, DWORD))UA_SHUF_MSG_ORIG)(bar, bit);
    if (bit == DFLT_BIT) return Dflt_Click(bar, bit, msg);   // the second slot's click -> UG2's yes / no dialog on the jukebox only (UG2's message goes on either way)
    if (bit != SHUF_BIT) return msg;
    char pkg[40]; DWORD old = 0, nw = 0, rb = 0, mgr = 0, obj = 0, kind = 0; int steps = 0;
    if (!Shuf_BarPkg((uintptr_t)bar, pkg, sizeof pkg)) { TraxProblem(SAID_E_SHUFCLICK, "the Shuffle button", "the button bar could not be read", "The click did nothing."); return msg; }
    if (!Shuf_ClickWanted(bit, pkg)) return msg;   // another screen: not this button, UG2's message goes on as always
    if (!RdDword(UA_SHUF_PLAYMODE, &old) || !Accessible(UA_SHUF_PLAYMODE, 4, TRUE)) { TraxProblem(SAID_E_SHUFCLICK, "the Shuffle button", "UG2's shuffle byte could not be read or written", "The click did nothing."); return msg; }
    nw = Shuf_Flip(old); *(volatile DWORD *)UA_SHUF_PLAYMODE = nw; rb = *(volatile DWORD *)UA_SHUF_PLAYMODE;        // compare-first by construction (the new value = the other one), read back
    Log("Shuffle: %s -> %s.", (old & 0xFFu) ? "On" : "Off", (rb & 0xFFu) ? "On" : "Off");
    if (rb != nw) TraxProblem(SAID_E_SHUFCLICK, "the Shuffle button", "the write to UG2's shuffle byte did not take", "The setting did not change.");
    PlayUiSound(1);                                                                                              // the trigger sound as UG2's 0x5241E0 plays it (the byte [0x8381C6]; -1 = none)
    if (RdDword(UA_BAR_SOUNDMGR, &mgr) && mgr) ((void (__attribute__((thiscall)) *)(DWORD))UA_SHUF_LISTSETUP)(mgr);   // 524230-524236: [player+0x9C] = what both pickers read
    uint32_t node = Shuf_FindNode(bar, SHUF_BIT, ShufRd32, ShufRd8, NULL, &steps);
    if (!node || !RdDword(node + SHUF_NODE_TEXT, &obj) || !obj || !RdDword(obj + 0x18, &kind) || kind != 2) { TraxProblem(SAID_E_SHUFCLICK, "the Shuffle button", "its text object was not found", "The button's words change at the next screen open."); return msg; }
    int r = Shuf_Print(obj, Shuf_Text(rb));
    if (r <= 0) TraxProblem(SAID_E_SHUFCLICK, "the Shuffle button", "the game's print returned 0", "The button's words change at the next screen open.");
    return msg;
}
static void InstallShuffle(void)
{
    SiteFail f = { 0, "" }; BOOL ok = TRUE;
    if (!g_barInstalled) { Log("Shuffle button NOT installed: it needs the EA Trax top bar."); return; }
    ok &= CheckSite(UA_SHUF_FLAGWORD, kShufFlagBytes, 4, &f);
    ok &= CheckSite(UA_SHUF_LABELCALL, kShufLabelCall, 5, &f);
    ok &= CheckSite(UA_SHUF_CLICKCALL, kShufClickCall, 5, &f);
    ok &= CheckSite(UA_SHUF_MSG_ORIG, kShufMsgHead, 9, &f);
    ok &= CheckSite(UA_SHUF_LISTSETUP, kShufListSetupHead, 8, &f);
    ok &= CheckSite(UA_SHUF_LABEL_ORIG, kBarSetLabelHead, 12, &f);
    ok &= CheckSite(UA_BAR_PRINTF_OBJ, kBarPrintfHead, 12, &f);
    ok &= CheckSite(UA_BAR_SOUND, kBarSoundHead, 9, &f);
    if (!ok) { Log("Shuffle button NOT installed: the game's code at 0x%06lX is not the retail exe's (found%s).", (unsigned long)f.addr, f.found); return; }
    unsigned char nf[4], c1[5] = { 0xE8, 0, 0, 0, 0 }, c2[5] = { 0xE8, 0, 0, 0, 0 }; uint32_t v = SHUF_FLAGS_NEW; memcpy(nf, &v, 4);
    uint32_t r1 = Shuf_Rel32(UA_SHUF_LABELCALL, (uint32_t)(uintptr_t)Shuf_Label), r2 = Shuf_Rel32(UA_SHUF_CLICKCALL, (uint32_t)(uintptr_t)Shuf_Click); memcpy(c1 + 1, &r1, 4); memcpy(c2 + 1, &r2, 4);
    int w = 0;
    w += WriteCode(UA_SHUF_FLAGWORD, nf, 4, "Shuffle button"); w += WriteCode(UA_SHUF_LABELCALL, c1, 5, "Shuffle button"); w += WriteCode(UA_SHUF_CLICKCALL, c2, 5, "Shuffle button");
    g_shufInstalled = w == 3;
    if (g_shufInstalled) Log("Shuffle button: installed.");
    else Log("Shuffle button NOT fully installed: %d of 3 writes FAILED (the lines above).", 3 - w);
}
// the Default button: the flag word's second bit (compare-first on the Shuffle button's value) + the jukebox vtable's handler slot (compare-first 0x0056B1C0), byte-checked with
// the callees the click and the reset use, all or nothing, only with the Shuffle button in. A failed flag-word write puts the handler slot back to 0x56B1C0 (no handler swap
// without the button).
static void InstallDefault(void)
{
    SiteFail f = { 0, "" }; BOOL ok = TRUE; unsigned char b2[4]; uint32_t v2 = SHUF_FLAGS_NEW; memcpy(b2, &v2, 4);
    if (!g_shufInstalled) { Log("Default button NOT installed: it needs the Shuffle button."); return; }
    ok &= CheckSite(UA_SHUF_FLAGWORD, b2, 4, &f);
    ok &= CheckSite(UA_DFLT_VTABLE, kDfltVtableBytes, 16, &f);
    ok &= CheckSite(UA_DFLT_HANDLER_SLOT, kDfltHandlerSlotBytes, 4, &f);
    ok &= CheckSite(UA_DFLT_HANDLER_ORIG, kDfltHandlerHead, 12, &f);
    ok &= CheckSite(UA_DFLT_DIALOG_FMT, kDfltDialogHead, 12, &f);
    ok &= CheckSite(UA_DFLT_UISOUND, kDfltUiSoundHead, 11, &f);
    ok &= CheckSite(UA_DFLT_DIALOG_FILE, (const unsigned char *)DFLT_DIALOG_FILE_TEXT, 24, &f);
    ok &= CheckSite(UA_SHUF_LISTSETUP, kShufListSetupHead, 8, &f);
    if (!ok) { Log("Default button NOT installed: the game's code at 0x%06lX is not the retail exe's (found%s).", (unsigned long)f.addr, f.found); return; }
    unsigned char nf[4], hb[4]; uint32_t v = DFLT_FLAGS_NEW, h = (uint32_t)(uintptr_t)Dflt_Handler; memcpy(nf, &v, 4); memcpy(hb, &h, 4);
    int w = WriteCode(UA_DFLT_HANDLER_SLOT, hb, 4, "Default button");          // the handler first: a slot without a handler behind it must never show
    if (w == 1) w += WriteCode(UA_SHUF_FLAGWORD, nf, 4, "Default button");
    g_dfltInstalled = w == 2;
    if (w == 1) { WriteCode(UA_DFLT_HANDLER_SLOT, kDfltHandlerSlotBytes, 4, "Default button"); Log("Default button NOT installed: a write FAILED (the line above); the handler slot was put back."); return; }
    if (g_dfltInstalled) Log("Default button: installed.");
    else Log("Default button NOT installed: a write FAILED (the line above).");
}
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        GetModuleFileNameA(hModule, g_dir, sizeof g_dir); char *bs = strrchr(g_dir, '\\'); if (bs) *bs = 0;
        snprintf(g_logPath, sizeof g_logPath, "%s\\UG2Music.log", g_dir);
        InitializeCriticalSection(&g_logLock); LogStart(); g_logReady = TRUE; Song_Init(&g_st);
        SYSTEMTIME t; GetLocalTime(&t);
        Log("%s (UG2Music.asi) started on %04u-%02u-%02u.", MOD_VERSION, (unsigned)t.wYear, (unsigned)t.wMonth, (unsigned)t.wDay);
        if (g_logMoveError) Log("The previous log could NOT be moved to UG2Music.previous.log (Windows error %lu): this start's lines follow the old ones.", g_logMoveError);
        Log("Loaded from %s.", g_dir);
        LogGameFiles();         // the exe's size, the song archive's size
        SaveRows();             // UG2's song table read and checked before any write
        int albums = WriteAlbum19();        // after the rows were read, before any pick
        int artist = WriteArtist0();        // row 0's artist pointer (again at every pick)
        int titles = WriteTitlesUg2();      // every row's title pointer -> the text without quotes, once (after the rows were read and checked)
        albums += WriteAlbumsUg2();         // rows 0 / 1's album pointers (again at every pick + screen open: UG2's 0x459D90 re-writes them after this)
        Log("Song names: %d titles, %d albums and %d artist line set.", titles, albums, artist);
        CheckRng();
        InstallHook();
        InstallLsHook();
        SettingsInit();         // the folder, the profile name as it is at load, its file (or the defaults)
        InstallBar();           // the top bar: the two hooks + three patches, byte-checked, all or nothing
        LogWordsPart();         // the row text shift: its two lookups (checked with the bar's sites)
        InstallList();          // the list / modes / preview: 10 operands re-pointed + 2 head hooks, byte-checked, all or nothing, only with the top bar in
        InstallSave();          // the game's save / load: 4 call operands re-pointed, byte-checked, all or nothing (independent of the top bar / the list)
        InstallPrompt();        // the save prompt: 2 head hooks (the keep-a-copy 0x52A460 / the changed? 0x52A4C0), byte-checked, all or nothing
        InstallShuffle();       // the Shuffle button: 1 data word + 2 call operands re-pointed, byte-checked, all or nothing, only with the top bar in
        InstallDefault();       // the Default button: the flag word's second bit + the jukebox handler slot 0x79CB7C swapped, byte-checked, all or nothing, only with the Shuffle button in
        int parts = (g_hooked ? 1 : 0) + (g_lsHooked ? 1 : 0) + (g_barInstalled ? 1 : 0) + (g_barInstalled && g_wordsHeadsOk ? 1 : 0) + (g_listInstalled ? 1 : 0)
                  + (g_saveInstalled ? 1 : 0) + (g_promptInstalled ? 1 : 0) + (g_shufInstalled ? 1 : 0) + (g_dfltInstalled ? 1 : 0);
        if (parts == 9) Log("Ready: 9 of 9 parts installed.");
        else Log("Ready: %d of 9 parts installed - see the lines above.", parts);
    }
    return TRUE;
}
