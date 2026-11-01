// saveprompt.h - makes UG2's own 'save changes?' prompt cover the mod's settings. The pure parts (no Windows call, no game memory), so the offline rig runs
// the same code natively.
// HOW UG2 DECIDES THE PROMPT (UG2-EXE): the profile object 0x83A9D0 keeps a serialized COPY of itself at [this+0x16778]: 0x52A460 (thiscall(flag) ret 4;
// 10 callers = FE screen entries + the memcard flows) makes it when none is kept and (flag or [this+0xa108]) (52a466 'mov eax,[esi+0x16778]; test; jne ret',
// 52a470 / 52a477 the two conditions, 52a481 the size 0x5b9680 + 0xd254, 52a48e the alloc, 52a496 '[esi+0x16778] := it', 52a4b1 the serializer 0x52A280 with
// ecx = this+0x10 = 0x83A9E0). 0x52A4C0 (thiscall() ret, al; 7 callers 4af368 / 4b6020 / 4ed94a / 4edd7c / 552c88 / 55befb / 55e5a5, each 'test al,al' -> a
// dialog pushed, e.g. 55e5b9 'push 0x1141' after the hash 0x34dc1bec action) = no copy -> al 0 (52a4d4); else the profile serialized again (52a50c, ecx =
// this+0x10) and byte-compared with the copy (52a559 / 52a589 'repz cmps'; a difference -> 1), both buffers freed and [this+0x16778] := 0 (52a5b7).
// So the prompt = 'the profile differs from the copy kept at the screen's entry'. The mod's state (the bar's soundtrack, the added songs' modes, All Trax's own
// set) is not in the profile -> the save hooks alone never raise it. The mod MIRRORS the game: a head hook on 0x52A460 (after the original: when the game holds a
// copy, the mod snapshots its own state once) and a head hook on 0x52A4C0 (before: whether the game holds a copy; after: the game's answer OR 'the mod's state
// differs from its snapshot', only when the game held a copy; then the snapshot is dropped as the game drops its copy). So a step of the bar, of an MW / UG1 /
// Carbon row or of an All Trax row raises UG2's own save dialog the same way; an Underground 2 row step changes UG2's own bytes = UG2's own compare.
// A 'Yes' on the dialog = UG2's own save (savesync.h writes the file there); a 'No' = the game goes on with the changed state in memory (unsaved) - the mod the same.
#include <stdint.h>
#include <string.h>
#define UA_PROMPT_KEEP 0x0052A460u      // the keep-a-copy: prologue 55 8B EC 56 8B F1 (6, no relative operand) + 'mov eax,[esi+0x16778]'
#define UA_PROMPT_KEEP_RESUME 0x0052A466u
#define UA_PROMPT_CMP 0x0052A4C0u       // the changed?: prologue 83 EC 0C 56 8B F1 (6, no relative operand) + 'mov eax,[esi+0x16778]'
#define UA_PROMPT_CMP_RESUME 0x0052A4C6u
#define UA_PROMPT_PROFILE_OBJ 0x0083A9D0u
#define PROMPT_COPY_OFF 0x16778u        // [this+0x16778] = the kept copy (0 = none)
#define PROMPT_STOLEN 6
static const unsigned char kPromptKeepHead[12] = { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x78, 0x67, 0x01, 0x00 };
static const unsigned char kPromptCmpHead[12]  = { 0x83, 0xEC, 0x0C, 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x78, 0x67, 0x01, 0x00 };
// ---- the mod's state the snapshot holds (the bar's soundtrack + the 26 MW modes + the 26 UG1 modes + the 30 Carbon modes + All Trax's own 109 = 195 B)
typedef struct { signed char soundtrack; signed char mwMode[27]; signed char u1Mode[27]; signed char cbMode[31]; signed char allMode[109]; } Prompt_Snap;
typedef struct { Prompt_Snap snap; int valid; int keeps, keepsNoCopy, compares, comparesNoCopy, modSaysYes, gameSaysYes; } Prompt_State;
static void Prompt_Take(Prompt_Snap *s, int soundtrack, const signed char *mwMode, const signed char *u1Mode, const signed char *cbMode, const signed char *allMode) { s->soundtrack = (signed char)soundtrack; memcpy(s->mwMode, mwMode, 27); memcpy(s->u1Mode, u1Mode, 27); memcpy(s->cbMode, cbMode, 31); memcpy(s->allMode, allMode, 109); }
static int Prompt_Differs(const Prompt_Snap *s, int soundtrack, const signed char *mwMode, const signed char *u1Mode, const signed char *cbMode, const signed char *allMode) { return s->soundtrack != (signed char)soundtrack || memcmp(s->mwMode, mwMode, 27) != 0 || memcmp(s->u1Mode, u1Mode, 27) != 0 || memcmp(s->cbMode, cbMode, 31) != 0 || memcmp(s->allMode, allMode, 109) != 0; }
// around the game's keep-a-copy: hadBefore / hasAfter = [this+0x16778] != 0 before / after the call. A copy made in this call (none before, one after) -> snapshot now; a copy kept
// from before -> keep the old snapshot (the game kept its old copy too: 52a46e), unless none is held yet; no copy after -> nothing (the game made none)
static void Prompt_OnKeep(Prompt_State *st, int hadBefore, int hasAfter, int soundtrack, const signed char *mwMode, const signed char *u1Mode, const signed char *cbMode, const signed char *allMode)
{
    st->keeps++;
    if (!hasAfter) { st->keepsNoCopy++; st->valid = 0; return; }
    if (!hadBefore || !st->valid) { Prompt_Take(&st->snap, soundtrack, mwMode, u1Mode, cbMode, allMode); st->valid = 1; }
}
// around the game's changed?: hadCopy = [this+0x16778] != 0 BEFORE the call; gameAl = what the game returned. -> what the stub returns in al. The snapshot is dropped either way
// (the game drops its copy at 52a5b7; with no copy there was nothing to compare and the stale snapshot must not outlive the game's state)
static int Prompt_OnCompare(Prompt_State *st, int hadCopy, int gameAl, int soundtrack, const signed char *mwMode, const signed char *u1Mode, const signed char *cbMode, const signed char *allMode, int *modDiffers)
{
    st->compares++; *modDiffers = 0;
    if (!hadCopy) { st->comparesNoCopy++; st->valid = 0; return gameAl; }
    int d = st->valid && Prompt_Differs(&st->snap, soundtrack, mwMode, u1Mode, cbMode, allMode); *modDiffers = d;
    if (gameAl) st->gameSaysYes++;
    if (d) st->modSaysYes++;
    st->valid = 0;
    return (gameAl || d) ? 1 : 0;
}
