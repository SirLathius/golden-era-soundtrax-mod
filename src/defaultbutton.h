// defaultbutton.h - the Default button on the EA Trax screen, its Yes / No dialog and the reset. The sites (UG2-EXE, retail speed2.exe f9dd86c0..), the bytes the
// asi checks before it writes, and the PURE rules the rig runs on the same code: the label / click gates for the second bit, the answer rule, the input gate under
// the dialog, the scope, the resets.
// HOW: the Shuffle button's flag word 0x7F80C8 gets a second new bit 0x100000 (0x00080401 -> 0x00180401): the FOURTH slot = UPPER LEFT, in UG2's own button look;
// its stock label = 'Open/Close Hood' (hash 0xEAA1D5DF, 52d46c-52d489), its message 0xC539A4B0 (52d237-52d24c) = the same one the Shuffle slot sends = one the
// jukebox handler 0x56B1C0 never compares. The mod prints 'Default' on that slot (the same re-pointed call 54e8d7, the second hash) and on its click (the same
// re-pointed call 55df22, the second bit) raises UG2's OWN yes / no dialog as UG2's own 'Default Settings' item does (49b980-49b9cf: 0x50D150(pkg, 1, 0) = the UI
// sound, then the dialog 'GenericDialog_SMALL.fng' with the labels Yes 0xF770 / No 0x061C) - through the entry point 0x558750 (the text as a format string, as
// UG2's 0x497FC0 uses it: 497fc4-497fed, 9 arguments + the va, cdecl 'add esp,0x28').
// Argument 1 = the package NAME ([screen+4] = [bar+0x58], the base ctor 0x551DE0 '[this+4] = name' and the setup 54e774): the dialog's answer comes back as a
// message to that package = the jukebox handler (UG2-EXE for the bar's own Quit dialog: [bar+4] -> 55dfbf; the delivery inside: from a decompile, not checked in
// the exe). The answer messages are the mod's own values (0 hits as dwords in speed2.exe): the mod swaps the jukebox vtable's handler slot 0x79CB7C (0x0056B1C0,
// the only such dword in the file, .rdata) for its own handler (thiscall ret 0x10 as 56b223), which acts on MSG_YES / MSG_NO after its own dialog and passes every
// other message on to 0x56B1C0 unchanged (so the soundtrack bar's and the rows' hooks below it keep running) - but for the input messages it holds while the
// dialog is up ((3) below).
// THE RESET: each EA Trax category keeps its own song settings; Default resets only the category the top bar shows. Underground 2 -> the live list 0x83ACB8 mode
// bytes := UG2's own defaults (6 Menu + 21 Racing by the song index, as fresh profiles hold them; the exe's writer of those defaults was not found) + 0x479220
// after (the masks from the list, as the row cycle's body does at 53e08d); Underground / Most Wanted / Carbon -> that game's live modes := kU1Mode / kMwMode /
// kCbMode and its file lines := -1 (= no line = the default); All Trax -> All Trax's own set only. The file is written at the next save (savesync.h); the save
// prompt's snapshot sees the difference = UG2's own save prompt (saveprompt.h). Then the rows on screen are rebuilt at once (RefillList). 'No' changes nothing.
// The click's sound + the dialog's own sounds = UG2's.
#ifndef DEFAULTBUTTON_H
#define DEFAULTBUTTON_H
#include <stdint.h>
#include <string.h>
// ---- (1) the sites (UG2-EXE unless said otherwise)
#define DFLT_BIT 0x00100000u              // bit 0x100000 = the 4th slot (upper left): > 0x400 (Back keeps slot 2), > 0x80000 (Shuffle keeps slot 3), not one of the setup's skipped bits
#define DFLT_FLAGS_NEW 0x00180401u        // Quit 0x1 + Back 0x400 + Shuffle 0x80000 + Default 0x100000 (compare-first on the Shuffle word 0x00080401)
#define DFLT_HASH_HOOD 0xEAA1D5DFu        // bit 0x100000's stock label 'Open/Close Hood' (52d46c-52d489) = what the setup hands the node right before the re-pointed call 54e8d7
#define DFLT_TEXT "Default"               // 7 characters < the slot's 16
#define UA_DFLT_VTABLE 0x0079CB74u         // the jukebox screen's vtable (553cbc 'mov [ebx],0x79cb74') = {0x554150, 0x40D080, 0x56B1C0 handler, 0x570A70} (.rdata, file 0x39CB74)
#define UA_DFLT_HANDLER_SLOT 0x0079CB7Cu   // slot 2 = the handler = dword 0056B1C0, 1 time in the file (file 0x39CB7C)
#define UA_DFLT_HANDLER_ORIG 0x0056B1C0u   // thiscall(ecx = screen; msg, a, b, c) ret 0x10 (56b1c0 'mov eax,[esp+4]', 56b223 'ret 0x10'); compares 13 messages, ignores the rest
#define UA_DFLT_DIALOG_FMT 0x00558750u     // cdecl(pkgName, dialogFile, label1, label2, msg1, msg2, msgEsc, 0, fmt, ...) -> the core 0x558680 (558750-55878c; the caller 497fed 'add esp,0x28')
#define UA_DFLT_UISOUND 0x0050D150u        // cdecl(pkgName, kind, set): the UI sound by table (50d158 'imul eax,eax,0x1b'; -1 = none) -> 0x458520; UG2's Default item plays (pkg, 1, 0) (49b997-49b99c)
#define UA_DFLT_DIALOG_FILE 0x0078FA24u    // .rdata 'GenericDialog_SMALL.fng' (49b9c1 = UG2's own 'Default Settings' prompt's file)
#define DFLT_DIALOG_FILE_TEXT "GenericDialog_SMALL.fng"
#define DFLT_LABEL_YES 0xF770u            // UG2's 'Yes' label hash (49b9bc, also 497fe2)
#define DFLT_LABEL_NO 0x061Cu             // UG2's 'No' label hash (49b9b7, also 497fdd)
#define DFLT_MSG_YES 0x7E16DEF1u          // the mod's own answer messages: 0 hits as a dword in speed2.exe and in the mod's own asi (UG2's own 0xD05FC3A3 / 0x34DC1BCF = 41 / 60 hits)
#define DFLT_MSG_NO 0x7E16DEF0u
#define DFLT_DIALOG_TEXT "Reset every song of this soundtrack^to its default setting?"   // '^' = UG2's own line break (English.bin: 374 texts)
static const unsigned char kDfltHandlerSlotBytes[4] = { 0xC0, 0xB1, 0x56, 0x00 };                                                              // 0x79CB7C = 0x0056B1C0
static const unsigned char kDfltVtableBytes[16] = { 0x50, 0x41, 0x55, 0x00, 0x80, 0xD0, 0x40, 0x00, 0xC0, 0xB1, 0x56, 0x00, 0x70, 0x0A, 0x57, 0x00 };   // 0x79CB74..0x79CB83
static const unsigned char kDfltHandlerHead[12] = { 0x8B, 0x44, 0x24, 0x04, 0x3D, 0x4B, 0x0A, 0x1C, 0x91, 0x56, 0x8B, 0xF1 };                  // 56b1c0 mov eax,[esp+4]; cmp eax,0x911c0a4b; push esi; mov esi,ecx
static const unsigned char kDfltDialogHead[12] = { 0x8B, 0x4C, 0x24, 0x24, 0x8B, 0x54, 0x24, 0x20, 0x8D, 0x44, 0x24, 0x28 };                   // 558750 mov ecx,[esp+0x24]; mov edx,[esp+0x20]; lea eax,[esp+0x28]
static const unsigned char kDfltUiSoundHead[11] = { 0x8B, 0x44, 0x24, 0x08, 0x8B, 0x4C, 0x24, 0x0C, 0x6B, 0xC0, 0x1B };                        // 50d150 mov eax,[esp+8]; mov ecx,[esp+0xc]; imul eax,eax,0x1b
// ---- (2) the pure rules
static inline int Dflt_LabelWanted(uint32_t hash, const char *pkg, uint32_t objKind) { return hash == DFLT_HASH_HOOD && Shuf_IsJukebox(pkg) && objKind == 2; }
static inline int Dflt_ClickWanted(uint32_t bit, const char *pkg) { return bit == DFLT_BIT && Shuf_IsJukebox(pkg); }
// the handler's decision on a message: 1 = Yes (the reset), 2 = No (nothing), 0 = not the mod's (pass on) - only while the mod's dialog is up
static inline int Dflt_Answer(uint32_t msg, int dialogUp) { if (!dialogUp) return 0; return msg == DFLT_MSG_YES ? 1 : msg == DFLT_MSG_NO ? 2 : 0; }
// ---- (3) THE GATE UNDER THE DIALOG. Seen in the mod's log in play: the press that answers the dialog ALSO reaches the jukebox handler 1-2 frames before the
// answer message does (5 of 5: 4 previews = accept 0x406415E3, 1 row step = left 0x9120409E). UG2's WAY (UG2-EXE, retail speed2.exe f9dd86c0..): a message goes
// to EVERY loaded package - the broadcast 0x55DC50 collects up to 32 packages with [pkg+0x18] != 0 (55dc7f-55dc93) and calls the base handler 0x55B5C0 for each
// (55dcb0-55dccf); 0x55B5C0 checks no dialog state (its one input filter 55bc4d-55bc61 = 0x50CF10(bar) = a time check since the bar's setup [bar+0x68]) and ends
// in the screen's own handler 55bc6b 'mov eax,[ebp]' 55bc75 'call [eax+0x8]' (= slot 2 = 0x56B1C0 for the jukebox = the mod's handler) and does NOT read eax after
// it (55bc78 pop edi .. 55bc7e ret 0x10); neither do its callers 55dc48 (0x55DC20 send by name, 55dc4d ret 8) and 55dcc7 (the broadcast loop). The dialog core's
// own mark 0x838974 (the dialog id: set 55822e in ShowDialog 0x558020, cleared 540aa4 in Dismiss 0x540A60) is read by 10 instructions, ALL dialog code (526c10
// 5409c0 540a60 540a75 540aa4 55807e 55815d 55822e 558270 55828c) - no screen reads it; UG2's own 'Default Settings' item 49b980-49b9d0 raises its dialog on
// 0x0C407210 / 0x406415E3 with no guard of its own; a second ShowDialog dismisses the first ('DIALOG :: Existing dialog (%d), will be dismissed..' 0x79d06c at
// 558094; 'Sending DismissedByOtherDialogMsg (0x%x) to parent package (%s)' 0x79d020 at 5580c1 -> 0x55DC20 (0x838960 = the parent's name, [0x838970])).
// => UG2 has NO mark a screen reads to hold its input under a dialog. So the mod's own gate: while g_dfltDialogUp (set at the raise, cleared at the answer =
// exactly the window the stray press lands in), the jukebox's INPUT messages are not passed on to 0x56B1C0. (The dialog core's mark 0x838974 is not read here.)
// THE LIST = 11 of the 13 messages 0x56B1C0 compares, each to its callee (UG2-EXE):
//   0x911C0A4B 56b1c4 -> 56b282 and 0x72619778 56b232 -> 56b282: 0x550F10 / 0x550E60(msg) by [screen+0x108] = the list object [screen+0x4C] stepped (550e7e 'cmp [ecx+0x24],eax' = the
//     highlighted row against the last) = the list's two directions                                                                                                 -> INPUT, gated
//   0x5073EF13 56b1d8 -> 56b321 and 0xD9FEEC59 56b31a -> 56b321: 0x5241E0(msg) = the trigger handler (the soundtrack bar's trigger hook inside)                     -> INPUT, gated
//   0x0C407210 56b1e5 -> 56b1fe and 0x406415E3 56b1f3 -> 56b1fe: 0x480C10([[screen+0x70]+0x18]) = the preview of the highlighted row (the mod's preview hook) + 0x50D150(.., 8, ..) -> INPUT, gated
//   0x911AB364 56b239 -> 56b244: 0x479220, 0x50D150(.., 0xa, ..), 0x537980(.., 0x78fa3c, 0), 0x480C30(-1) = PAD_BACK = the screen's close                           -> INPUT, gated
//   0xC519BFC0 56b2a1 -> 56b2da: 0x531630 / 0x5316B0(1) by [screen+0x108], then 0x458520 by the bar byte [[screen+0x30]+0x8380B8] = a bar button's message              -> INPUT, gated
//   0x9120409E 56b2aa -> 56b2c3 and 0xB5971BF1 56b2b8 -> 56b2c3: 0x53DEB0(msg) = the row cycle left / right (the mod's row-cycle hook)                              -> INPUT, gated
//   0x9803F6E2 56b2b1 -> 56b2cf: 0x565D90() = the soundtrack bar's mouse step (the mod's bar hook)                                                                  -> INPUT, gated
//   0x35F8620B 56b1ec -> 56b226: 0x4958D0([screen+0x4C]) = 0x51EE80([[list+0x2C]+0x2C], 0x78f228) = a text refresh (4958d0-4958e5), no input                        -> PASSED
//   0xC98356BA 56b313 -> 56b32d: [screen+0x78] -> 0x51EE80(.., 0x78f228) + 0x524170 = the per-frame refresh (seen x7 in the first frames of a screen open)          -> PASSED
//   every message 0x56B1C0 does not compare (FC946BFA / 32C72D8F per frame, the slots' 0xC539A4B0, ..) -> PASSED (= 0x56B1C0's own ignore); MSG_YES / MSG_NO -> the answer rule first.
// THE RETURN for a dropped message = what 0x56B1C0 leaves for a message it ignores: every 'jne' of its chain (56b1f8 56b23e 56b2bd 56b31f) lands at 56b349 'pop esi' 56b34a 'ret 0x10' with
// eax = the message (56b1c0 'mov eax,[esp+4]', untouched on those paths) -> the gate returns msg. (Nobody reads it, see above.)
// THE STALE MARK (a dialog whose answer never comes: the screen closed under it, or another dialog replaced it = DismissedByOtherDialogMsg is not the mod's): the
// next Default click resets it (the Default button lives on the BAR's handler 0x55DFA0 -> 0x55DE00 -> the mod's click hook - not gated), and so does the next EA
// Trax open (the label hook on the jukebox = the bar setup of a new screen). While stale, the list under no dialog would take no input until one of the two: logged as a stale gate.
#define DFLT_GATED_N 11
static const uint32_t kDfltGated[DFLT_GATED_N] = { 0x911C0A4Bu, 0x72619778u, 0x5073EF13u, 0xD9FEEC59u, 0x0C407210u, 0x406415E3u, 0x911AB364u, 0xC519BFC0u, 0x9120409Eu, 0xB5971BF1u, 0x9803F6E2u };
// 1 = drop (an input message while the mod's dialog is up), 0 = pass on. Checked AFTER the answer rule (MSG_YES / MSG_NO are not in the list anyway).
static inline int Dflt_Gate(uint32_t msg, int dialogUp) { if (!dialogUp) return 0; for (int i = 0; i < DFLT_GATED_N; i++) if (kDfltGated[i] == msg) return 1; return 0; }
static inline uint32_t Dflt_GateReturn(uint32_t msg) { return msg; }   // = eax at 56b34a for a message 0x56B1C0 ignores
// the scope: the soundtrack the bar shows -> the ONE set a reset touches (each category is reset on its own): Underground 2 -> UG2's list, Underground / Most Wanted /
// Carbon -> that game's own set, All Trax -> All Trax's OWN set only (the list and the game sets untouched), Off -> none
static inline int Dflt_ScopeUg2(int st) { return st == SET_ST_UG2; }
static inline int Dflt_ScopeMw(int st) { return st == SET_ST_MW; }
static inline int Dflt_ScopeU1(int st) { return st == SET_ST_UG1; }
static inline int Dflt_ScopeCb(int st) { return st == SET_ST_CARBON; }
static inline int Dflt_ScopeAll(int st) { return st == SET_ST_ALL; }
// UG2's own default per song index (UG2-DATA: three fresh profiles read '111111222222222222222222222' = songs 0..5 Menu, 6..26 Racing); -1 = not a UG2 song index
static inline int Dflt_Ug2Default(int song) { return (song >= 0 && song < 6) ? 1 : (song >= 6 && song < 27) ? 2 : -1; }
// the UG2 rows: listMode[p] := the default of listSong[p] (an out-of-range song index is left alone) -> the count changed; before / after = the 27 modes as digits
static inline int Dflt_ResetUg2(const int *listSong, unsigned char *listMode, char *before, char *after)
{
    int ch = 0;
    for (int p = 0; p < 27; p++) { int d = Dflt_Ug2Default(listSong[p]); before[p] = (char)('0' + (listMode[p] & 7)); if (d >= 0 && listMode[p] != (unsigned char)d) { listMode[p] = (unsigned char)d; ch++; } after[p] = (char)('0' + (listMode[p] & 7)); }
    before[27] = after[27] = 0; return ch;
}
// one added game's modes 1..n: live[k] := def[k] (the game's own default), set[k] := -1 (no file line = the default) -> the count of live modes that changed; *lines = the file lines dropped
static inline int Dflt_ResetAdded(unsigned char *live, signed char *set, const unsigned char *def, int n, int *lines, char *before, char *after)
{
    int ch = 0; *lines = 0;
    for (int k = 1; k <= n; k++) { before[k - 1] = (char)('0' + (live[k] & 7)); if (live[k] != def[k]) ch++; if (set[k] >= 0) (*lines)++; live[k] = def[k]; set[k] = -1; after[k - 1] = (char)('0' + (live[k] & 7)); }
    before[n] = after[n] = 0; return ch;
}
// All Trax's own 109 modes := each song's own game default, its ALL lines := -1 -> the count of live modes that changed; *lines = the ALL lines dropped; before / after =
// 109 digits by the id. UG2's list 0x83ACB8 and the four game sets are NOT touched (so no 0x479220: UG2's masks come from the list, which did not change).
static inline int Dflt_ResetAll(unsigned char *live, signed char *set, int *lines, char *before, char *after)
{
    int ch = 0; *lines = 0;
    for (int id = 0; id < SONG_NSONG; id++) { before[id] = (char)('0' + (live[id] & 7)); if (live[id] != Song_AllDefault(id)) ch++; if (set[id] >= 0) (*lines)++; live[id] = Song_AllDefault(id); set[id] = -1; after[id] = (char)('0' + (live[id] & 7)); }
    before[SONG_NSONG] = after[SONG_NSONG] = 0; return ch;
}
#endif
